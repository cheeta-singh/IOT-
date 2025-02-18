#include <ModbusMaster.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// WiFi Configuration
#define WIFI_SSID "Nothing2a" // Replace with your WiFi SSID
#define WIFI_PASSWORD "ayush_1980" // Replace with your WiFi password

// Firebase Configuration
#define FIREBASE_HOST "https://agrimitra-be6eb-default-rtdb.firebaseio.com"
#define FIREBASE_AUTH "2QICl4mN1C3X36cU6vyJ30avsnlQcCGBXJ3tf1u8"

// RS485 Configuration
const int RE_PIN = 8; // Receive Enable pin
const int DE_PIN = 7; // Driver Enable pin
ModbusMaster node;

// RS485 Transmission Control
void preTransmission() {
  digitalWrite(RE_PIN, HIGH);
  digitalWrite(DE_PIN, HIGH);
}

void postTransmission() {
  digitalWrite(RE_PIN, LOW);
  digitalWrite(DE_PIN, LOW);
}

void setup() {
  Serial.begin(115200); // Debugging serial
  Serial2.begin(9600, SERIAL_8N1, 16, 17); // RX = GPIO 16, TX = GPIO 17 for RS485 communication

  // Configure RS485 control pins
  pinMode(RE_PIN, OUTPUT);
  pinMode(DE_PIN, OUTPUT);
  digitalWrite(RE_PIN, LOW);
  digitalWrite(DE_PIN, LOW);

  // Initialize Modbus
  node.begin(2, Serial2); // Slave ID = 2
  node.preTransmission(preTransmission);
  node.postTransmission(postTransmission);

  // Connect to WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi");

  Serial.println("Setup completed.");
}

void loop() {
  uint8_t result;

  // Read data from NPK sensor
  result = node.readHoldingRegisters(0x0000, 7); // Read 7 registers starting at address 0x0000
  if (result == node.ku8MBSuccess) {
    // Parse sensor data
    float temperature = node.getResponseBuffer(0) / 10.0;
    float moisture = node.getResponseBuffer(1) / 10.0;
    int ec = node.getResponseBuffer(2);
    float pH = node.getResponseBuffer(3) / 100.0;
    int nitrogen = node.getResponseBuffer(4) * 10;
    int phosphorus = node.getResponseBuffer(5) * 10;
    int potassium = node.getResponseBuffer(6) * 10;

    // Log data to Serial Monitor
    Serial.println("Sensor Data:");
    Serial.printf("Temp: %.1f°C, Moisture: %.1f%%, EC: %d uS/cm, pH: %.2f\n", temperature, moisture, ec, pH);
    Serial.printf("N: %d mg/kg, P: %d mg/kg, K: %d mg/kg\n", nitrogen, phosphorus, potassium);

    // Create JSON payload
    String payload;
    StaticJsonDocument<200> json;
    json["Temperature"] = temperature;
    json["Moisture"] = moisture;
    json["EC"] = ec;
    json["pH"] = pH;
    json["Nitrogen"] = nitrogen;
    json["Phosphorus"] = phosphorus;
    json["Potassium"] = potassium;
    serializeJson(json, payload);

    // Send data to Firebase
    if (sendToFirebase(payload)) {
      Serial.println("Data uploaded successfully to Firebase.");
    } else {
      Serial.println("Failed to upload data to Firebase.");
    }
  } else {
    printModbusError(result);
  }
  delay(10000); // Delay 10 seconds before the next read
}

bool sendToFirebase(String payload) {
  HTTPClient http;
  String url = String(FIREBASE_HOST) + "/npk_data.json?auth=" + FIREBASE_AUTH;

  http.begin(url);
  http.addHeader("Content-Type", "application/json");

  int httpResponseCode = http.POST(payload);
  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.println("Response: " + response);
    http.end();
    return true;
  } else {
    Serial.println("Error sending data: " + String(httpResponseCode));
    http.end();
    return false;
  }
}

// Print out Modbus error
void printModbusError(uint8_t errNum) {
  switch (errNum) {
    case node.ku8MBSuccess:
      Serial.println(F("Success"));
      break;
    case node.ku8MBIllegalFunction:
      Serial.println(F("Illegal Function Exception"));
      break;
    case node.ku8MBIllegalDataAddress:
      Serial.println(F("Illegal Data Address Exception"));
      break;
    case node.ku8MBIllegalDataValue:
      Serial.println(F("Illegal Data Value Exception"));
      break;
    case node.ku8MBSlaveDeviceFailure:
      Serial.println(F("Slave Device Failure"));
      break;
    case node.ku8MBResponseTimedOut:
      Serial.println(F("Response Timed Out"));
      break;
    case node.ku8MBInvalidCRC:
      Serial.println(F("Invalid CRC"));
      break;
    default:
      Serial.println(F("Unknown Error"));
      break;
  }
}
