// ESP32 BLE Terminal with Message Buffering
// Designed to handle character-by-character transmission

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// Standard BLE UART Service and Characteristic UUIDs
#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E" // UART service UUID
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E" // UART RX characteristic
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E" // UART TX characteristic

BLEServer *pServer = NULL;
BLECharacteristic *pTxCharacteristic = NULL;
BLECharacteristic *pRxCharacteristic = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;
String device_name = "ihson";  

// Message buffering variables
String messageBuffer = "";
unsigned long lastCharTime = 0;
const unsigned long MESSAGE_TIMEOUT = 100; // ms timeout to consider message complete

// Callback class for connection events
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
      Serial.println("Device connected");
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
      Serial.println("Device disconnected");
      // Clear message buffer on disconnect
      messageBuffer = "";
    }
};

// Callback class for receiving data
class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      // Get the raw data bytes
      uint8_t* rxData = pCharacteristic->getData();
      size_t len = pCharacteristic->getLength();
      
      if (len > 0) {
        // Reset the timeout timer
        lastCharTime = millis();
        
        // Add received character(s) to buffer
        for (int i = 0; i < len; i++) {
          char c = (char)rxData[i];
          
          // Check for end of message markers
          if (c == '\n' || c == '\r') {
            // Process complete message
            if (messageBuffer.length() > 0) {
              Serial.println(messageBuffer);
              messageBuffer = "";
            }
          } else {
            // Add character to buffer
            messageBuffer += c;
          }
        }
      }
    }
};

void setup() {
  // Initialize Serial communication
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("Type messages in the Serial Monitor to send to iPhone");

  // Initialize BLE with device name
  BLEDevice::init(device_name.c_str());

  // Create BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create a RX Characteristic (for ESP32 to receive data)
  pRxCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID_RX,
                      BLECharacteristic::PROPERTY_WRITE | 
                      BLECharacteristic::PROPERTY_READ);
  pRxCharacteristic->setCallbacks(new MyCallbacks());

  // Create a TX Characteristic (for ESP32 to send data)
  pTxCharacteristic = pService->createCharacteristic(
                        CHARACTERISTIC_UUID_TX,
                        BLECharacteristic::PROPERTY_NOTIFY |
                        BLECharacteristic::PROPERTY_READ);                      
  pTxCharacteristic->addDescriptor(new BLE2902());

  // Start the service
  pService->start();

  // Start advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);  // functions that help with iPhone connections issue
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();
  
  Serial.println("BLE UART service started");
  Serial.print("Device name: ");
  Serial.println(device_name);
  Serial.println("Waiting for iPhone connection...");
}

void loop() {
  // Check for message timeout (to process incomplete messages)
  if (messageBuffer.length() > 0 && millis() - lastCharTime > MESSAGE_TIMEOUT) {
    Serial.println(messageBuffer);
    messageBuffer = "";
  }

  // Check if data is available from Serial monitor
  if (Serial.available() && deviceConnected) {
    String message = "";
    
    // Read until newline or timeout
    while (Serial.available()) {
      char c = Serial.read();
      if (c != '\n' && c != '\r') {
        message += c;
      }
      delay(5);
    }
    
    if (message.length() > 0) {
      // Send the message to iPhone
      pTxCharacteristic->setValue(message.c_str());
      pTxCharacteristic->notify();
      
      // Echo the message locally
      Serial.println(message);
    }
  }

  // Handle connection state changes
  if (!deviceConnected && oldDeviceConnected) {
    delay(500); // Give the Bluetooth stack time to get ready
    pServer->startAdvertising(); // Restart advertising
    Serial.println("Restarting advertising...");
    oldDeviceConnected = deviceConnected;
  }
  
  // Handle new connection
  if (deviceConnected && !oldDeviceConnected) {
    // Send welcome message
    String welcomeMsg = "Hello";
    pTxCharacteristic->setValue(welcomeMsg.c_str());
    pTxCharacteristic->notify();
    Serial.println("Device connected - sent welcome message");
    oldDeviceConnected = deviceConnected;
  }
  
  delay(10);
}
