#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <Adafruit_NeoPixel.h>

// NEOPIXEL SETUP
// Use tools->Pin Numbering GPIO(legacy) and connect DIN to A5
#define PIN            12
#define NUMPIXELS      8
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

uint8_t *pixelBuffer = NULL;
uint8_t width = 0;
uint8_t height = 0;
uint8_t stride;
uint8_t componentsValue;
bool is400Hz;
uint8_t components = 3;     // only 3 and 4 are valid values

#define NEOPIXEL_VERSION_STRING "Neopixel v2.0"

BLECharacteristic *pCharacteristic;
bool deviceConnected = false;
std::string txValue;

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/

#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E" // UART service UUID
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"


class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
    }
};

class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string rxValue = pCharacteristic->getValue();

      if (rxValue.length() > 0) {
        Serial.println("*********");
        Serial.print("Received Value: ");
        for (int i = 0; i < rxValue.length(); i++){
          Serial.print(rxValue[i]);
        }
        Serial.println();
        
        
        switch (char(rxValue[0])) {
          case 'V':
              Serial.println(F("Command: Version check"));
              txValue = NEOPIXEL_VERSION_STRING;
              break; 
          case 'S': {   // Setup dimensions, components, stride...
              Serial.println(F("Command: Setup"));
              width = rxValue[1];
              height = rxValue[2];
              stride = rxValue[3];
              componentsValue = rxValue[4];
              is400Hz = rxValue[5];

              neoPixelType pixelType;
              pixelType = componentsValue + (is400Hz ? NEO_KHZ400 : NEO_KHZ800);

              components = (componentsValue == NEO_RGB || componentsValue == NEO_RBG || componentsValue == NEO_GRB || componentsValue == NEO_GBR || componentsValue == NEO_BRG || componentsValue == NEO_BGR) ? 3:4;
              
              Serial.printf("\tsize: %dx%d\n", width, height);
              Serial.printf("\tstride: %d\n", stride);
              Serial.printf("\tpixelType %d\n", pixelType);
              Serial.printf("\tcomponents: %d\n", components);

              uint32_t size = width*height;
              pixels.updateLength(size);
              pixels.updateType(pixelType);
              pixels.setPin(PIN);

              // Done
              txValue = "OK";
              break;
          }

          case 'C': {   // Clear with color
              Serial.println(F("Command: ClearColor"));
              uint8_t r = (uint8_t)rxValue[1];
              uint8_t g = (uint8_t)rxValue[2];
              uint8_t b = (uint8_t)rxValue[3];
              

              Serial.printf("R: %d | G: %d | B: %d \n\r",r,g,b);
              for(int i=0;i<width*height;i++){
                pixels.setPixelColor(i,r,g,b);
              }
              pixels.show();
              
              // Done
              txValue = "OK";
              break;
          }

          case 'B': {   // Set Brightness
              pixels.setBrightness(rxValue[1]);
              pixels.show();
              txValue = "OK";
              break;
          }
                
          case 'P': {   // Set Pixel
              Serial.println(F("Command: SetPixel"));

              // Read position
              uint8_t x = rxValue[1];
              uint8_t y = rxValue[2];

              // Read colors
              uint32_t pixelOffset = y*width+x;
              pixels.setPixelColor(pixelOffset,rxValue[3],rxValue[4],rxValue[5]);
              pixels.show();
              txValue = "OK";
              break;
          }

        }
    
        // if(command == "!C"){
        //   uint8_t r = (uint8_t)rxValue[2];
        //   uint8_t g = (uint8_t)rxValue[3];
        //   uint8_t b = (uint8_t)rxValue[4];

        //   Serial.printf("R: %d | G: %d | B: %d \n\r",r,g,b);

        //   for(int i=0;i<NUMPIXELS;i++){
        //     pixels.setPixelColor(i, pixels.Color(r,g,b));
        //   }
        //   pixels.show();


        // Serial.println(command);
        Serial.println("*********");
      }
    }
};


void setup() {
  Serial.begin(115200);

  // SETUP NeoPixel
  pixels.begin();

  // Create the BLE Device
  BLEDevice::init("NeoPixel Controller BLE");

  // Create the BLE Server
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create a BLE Characteristic
  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID_TX,
                      BLECharacteristic::PROPERTY_NOTIFY
                    );

  pCharacteristic->addDescriptor(new BLE2902());

  BLECharacteristic *pCharacteristic = pService->createCharacteristic(
                                         CHARACTERISTIC_UUID_RX,
                                         BLECharacteristic::PROPERTY_WRITE
                                       );

  pCharacteristic->setCallbacks(new MyCallbacks());

  // Start the service
  pService->start();

  // Start advertising
  pServer->getAdvertising()->start();
  Serial.println("Waiting a client connection to notify...");
}

void loop() {

  if (deviceConnected) {
    pCharacteristic->setValue(txValue);
    pCharacteristic->notify();
    txValue.clear();
  }
  delay(1000);
}