#include "Server.h"
#include "GlobalVariables.h"
#include <Arduino.h>
#include <SPIFFS.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

#define hostName       "SelfBalancingRobot"
#define localSsid      "V1RU$"       // My Local WiFi SSID
#define localPassword  "@mikochu123"  // My Local WiFi password

#define esp32SSID		"Self Balancing Robot"
#define esp32Password	"@mikochu123"

AsyncWebServer  server(80); // define web server port 80
AsyncWebSocket ws("/ws");

AsyncEventSource events("/events");

void onWsEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len) {
	if (type == WS_EVT_CONNECT) {
		Serial.printf("ws[%s][%u] connect\n", server->url(), client->id());
		client->printf("Hello Client %u :)", client->id());
		client->ping();
	}
	else if (type == WS_EVT_DISCONNECT) {
		Serial.printf("ws[%s][%u] disconnect: %u\n", server->url(), client->id());
	}
	else if (type == WS_EVT_ERROR) {
		Serial.printf("ws[%s][%u] error(%u): %s\n", server->url(), client->id(), *((uint16_t*)arg), (char*)data);
	}
	else if (type == WS_EVT_PONG) {
		Serial.printf("ws[%s][%u] pong[%u]: %s\n", server->url(), client->id(), len, (len) ? (char*)data : "");
	}
	else if (type == WS_EVT_DATA) {
		AwsFrameInfo * info = (AwsFrameInfo*)arg;
		String msg = "";
		if (info->final && info->index == 0 && info->len == len) {
			//the whole message is in a single frame and we got all of it's data
			//Serial.printf("ws[%s][%u] %s-message[%llu]: ", server->url(), client->id(), (info->opcode == WS_TEXT) ? "text" : "binary", info->len);

			if (info->opcode == WS_TEXT) {
				for (size_t i = 0; i < info->len; i++) {
					msg += (char)data[i];
				}
			}
			else {
				char buff[3];
				for (size_t i = 0; i < info->len; i++) {
					//sprintf(buff, "%02x ", (uint8_t)data[i]);
					msg += buff;
				}
			}
			if (info->opcode == WS_TEXT) {
				//Serial.printf("%s\n", msg.c_str());
				if (msg == "YPR") {
					client->binary((uint8_t*)&allData.YPR, 12);
				}
				else if (msg == "GYRO") {
					client->binary((uint8_t*)&allData.imuData.gyro, 12);
				}
				else if (msg == "ACC") {
					client->binary((uint8_t*)&allData.imuData.accel, 12);
				}
				else if (msg == "MOT") {
					allData.motor[0] = M1Counter;
					allData.motor[1] = M2Counter;
					client->binary((uint8_t*)&allData.motor, 8);
				}
				else if (msg == "ALL") {
					allData.motor[0] = M1Counter;
					allData.motor[1] = M2Counter;
					client->binary((uint8_t*)&allData, 44);
				}
				else {
					client->text("I got your text message");
				}
			}
			else
				client->binary("I got your binary message");
		}
		else {
			//message is comprised of multiple frames or the frame is split into multiple packets
			if (info->index == 0) {
				if (info->num == 0)
					Serial.printf("ws[%s][%u] %s-message start\n", server->url(), client->id(), (info->message_opcode == WS_TEXT) ? "text" : "binary");
				Serial.printf("ws[%s][%u] frame[%u] start[%llu]\n", server->url(), client->id(), info->num, info->len);
			}

			Serial.printf("ws[%s][%u] frame[%u] %s[%llu - %llu]: ", server->url(), client->id(), info->num, (info->message_opcode == WS_TEXT) ? "text" : "binary", info->index, info->index + len);

			if (info->opcode == WS_TEXT) {
				for (size_t i = 0; i < len; i++) {
					msg += (char)data[i];
				}
			}
			else {
				char buff[3];
				for (size_t i = 0; i < len; i++) {
					sprintf(buff, "%02x ", (uint8_t)data[i]);
					msg += buff;
				}
			}
			Serial.printf("%s\n", msg.c_str());

			if ((info->index + len) == info->len) {
				Serial.printf("ws[%s][%u] frame[%u] end[%llu]\n", server->url(), client->id(), info->num, info->len);
				if (info->final) {
					Serial.printf("ws[%s][%u] %s-message end\n", server->url(), client->id(), (info->message_opcode == WS_TEXT) ? "text" : "binary");
					if (info->message_opcode == WS_TEXT)
						client->text("I got your text message 2");
					else
						client->binary("I got your binary message 2");
				}
			}
		}
	}

}
void registerServer() {
	server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html").setCacheControl("max-age=600");
	// API
	server.on("/api/info", HTTP_GET, [](AsyncWebServerRequest *request) {
		DynamicJsonDocument doc(1024);
		String jsonValue;
		doc["ChipRevision"] = String(ESP.getChipRevision());
		doc["CpuFreqMHz"] = String(ESP.getCpuFreqMHz());
		doc["SketchMD5"] = String(ESP.getSketchMD5());
		doc["SketchSize"] = String(ESP.getSketchSize());
		doc["CycleCount"] = String(ESP.getCycleCount());
		doc["SdkVersion"] = String(ESP.getSdkVersion());
		serializeJson(doc, jsonValue);
		request->send(200, "application/json", jsonValue);
	});
	server.on("/api/ypr", HTTP_GET, [](AsyncWebServerRequest *request) {
		DynamicJsonDocument doc(1024);
		String jsonValue;
		doc["yaw"] = allData.YPR[0];
		doc["pitch"] = allData.YPR[1];
		doc["roll"] = allData.YPR[2];
		serializeJson(doc, jsonValue);
		request->send(200, "application/json", jsonValue);
	});
	server.on("/api/gyro", HTTP_GET, [](AsyncWebServerRequest *request) {
		DynamicJsonDocument doc(1024);
		String jsonValue;
		doc["x"] = allData.imuData.gyro[0];
		doc["y"] = allData.imuData.gyro[1];
		doc["z"] = allData.imuData.gyro[2];
		serializeJson(doc, jsonValue);
		request->send(200, "application/json", jsonValue);
	});
	server.on("/api/gyroOffset", HTTP_GET, [](AsyncWebServerRequest *request) {
		DynamicJsonDocument doc(1024);
		String jsonValue;
		serializeJson(doc, jsonValue);
		request->send(200, "application/json", jsonValue);
	});
	server.on("/api/accMinMax", HTTP_GET, [](AsyncWebServerRequest *request) {
		DynamicJsonDocument doc(1024);
		String jsonValue;
		doc["xMin"] = accelMinMax[0];
		doc["yMin"] = accelMinMax[1];
		doc["zMin"] = accelMinMax[2];
		doc["xMax"] = accelMinMax[3];
		doc["yMax"] = accelMinMax[4];
		doc["zMax"] = accelMinMax[5];
		serializeJson(doc, jsonValue);
		request->send(200, "application/json", jsonValue);
	});
	server.on("/api/motor", HTTP_GET, [](AsyncWebServerRequest *request) {
		DynamicJsonDocument doc(1024);
		String jsonValue;
		doc["M1"] = M1Counter;
		doc["M2"] = M2Counter;
		serializeJson(doc, jsonValue);
		request->send(200, "application/json", jsonValue);
	});
	server.on("/api/acc", HTTP_GET, [](AsyncWebServerRequest *request) {
		DynamicJsonDocument doc(1024);
		String jsonValue;
		doc["x"] = allData.imuData.accel[0];
		doc["y"] = allData.imuData.accel[1];
		doc["z"] = allData.imuData.accel[2];
		serializeJson(doc, jsonValue);
		request->send(200, "application/json", jsonValue);
	});
	server.on("/api/cal", HTTP_GET, [](AsyncWebServerRequest *request) {
		DynamicJsonDocument doc(1024);
		String jsonValue;
		JsonObject  jsonGyro = doc.createNestedObject("gyro");
		jsonGyro["x"] = gyroOffset[0];
		jsonGyro["y"] = gyroOffset[1];
		jsonGyro["z"] = gyroOffset[2];

		JsonObject  jsonAcc = doc.createNestedObject("acc");
		jsonAcc["xMin"] = accelMinMax[0];
		jsonAcc["yMin"] = accelMinMax[1];
		jsonAcc["zMin"] = accelMinMax[2];
		jsonAcc["xMax"] = accelMinMax[3];
		jsonAcc["yMax"] = accelMinMax[4];
		jsonAcc["zMax"] = accelMinMax[5];

		JsonObject jsonPID = doc.createNestedObject("pid");
		JsonObject jsonYawPID = jsonPID.createNestedObject("yaw");
		jsonYawPID["P"] = stabilizerPID.yaw[0];
		jsonYawPID["I"] = stabilizerPID.yaw[1];
		jsonYawPID["D"] = stabilizerPID.yaw[2];
		JsonObject jsonPitchPID = jsonPID.createNestedObject("pitch");
		jsonPitchPID["P"] = stabilizerPID.pitch[0];
		jsonPitchPID["I"] = stabilizerPID.pitch[1];
		jsonPitchPID["D"] = stabilizerPID.pitch[2];
		JsonObject jsonRollPID = jsonPID.createNestedObject("roll");
		jsonRollPID["P"] = stabilizerPID.roll[0];
		jsonRollPID["I"] = stabilizerPID.roll[1];
		jsonRollPID["D"] = stabilizerPID.roll[2];
		
		serializeJson(doc, jsonValue);
		request->send(200, "application/json", jsonValue);
	});
}

void initServer() {
	Serial.begin(115200);
	if (!SPIFFS.begin()) {
		//Serial.println("An Error has occurred while mounting SPIFFS");
		return;
	}
	WiFi.mode(WIFI_AP_STA);
	WiFi.softAPdisconnect(true);
	WiFi.setHostname(hostName);
	WiFi.begin(localSsid, localPassword);

	// Wait for connection
	while (WiFi.status() != WL_CONNECTED) {
	}

	WiFi.softAPsetHostname(hostName);
	WiFi.softAPdisconnect(false);
	WiFi.softAP(esp32SSID, esp32Password, 1, 0, 4);
	MDNS.addService("http", "tcp", 80);
	registerServer();

	ws.onEvent(onWsEvent);
	server.addHandler(&ws);

	events.onConnect([](AsyncEventSourceClient *client) {
		client->send("hello!", NULL, millis(), 1000);
	});
	server.begin();

}
