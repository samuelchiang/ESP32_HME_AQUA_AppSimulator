#include <Arduino.h>

#include <BLEClient.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <NTPClient.h>
#include <WiFi.h>
#include <WiFiUdp.h>

#define WIFI_SSID "xxxxx"               // 請填入您的 WiFi 名稱
#define WIFI_PASSWORD "xxxxx"           // 請填入您的 WiFi 密碼
#define GRASSY_CORE_NAME "GrassyCore C" // Grassy Core 模擬設備的名稱
#define SERVICE_UUID "F000C0E0-0451-4000-B000-000000000000" // 服務 UUID
#define CHARACTERISTIC_UUID                                                    \
  "F000C0E1-0451-4000-B000-000000000000" // 特征值 UUID

BLEClient *pClient;
BLERemoteCharacteristic *pRemoteCharacteristic;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 0, 60000); // 設定 NTP 客戶端

// 發送設置時間指令的函數
void sendSetTimeCommand(uint8_t hour, uint8_t minute, uint8_t second) {
  if (pRemoteCharacteristic == nullptr)
    return;

  // 構建模擬的時間數據（例如：80 31 06 00 00 hour 00 00 minute 00 00 second 00
  // 00）
  uint8_t commandData[] = {0x80, 0x31,   0x06, 0x00, 0x00,   hour, 0x00,
                           0x00, minute, 0x00, 0x00, second, 0x00, 0x00};

  // 發送指令
  pRemoteCharacteristic->writeValue(commandData, sizeof(commandData));
  Serial.print("Sent command: ");
  for (int i = 0; i < sizeof(commandData); i++) {
    Serial.printf("%02X ", commandData[i]);
  }
  Serial.println();
}

// 發送固定指令的函數
void sendFixedCommand() {
  if (pRemoteCharacteristic == nullptr)
    return;

  // 固定指令數據（例如：80 31 01 00 00 3B 00 00 01 00 00 6E 01 00）
  uint8_t commandData[] = {0x80, 0x31, 0x01, 0x00, 0x00, 0x3B, 0x00,
                           0x00, 0x01, 0x00, 0x00, 0x6E, 0x01, 0x00};

  // 發送指令
  pRemoteCharacteristic->writeValue(commandData, sizeof(commandData));
  Serial.print("Sent fixed command: ");
  for (int i = 0; i < sizeof(commandData); i++) {
    Serial.printf("%02X ", commandData[i]);
  }
  Serial.println();
}

// 掃描並連接到 Grassy Core 模擬設備
class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    if (advertisedDevice.getName() == GRASSY_CORE_NAME) {
      Serial.println("Found Grassy Core device, connecting...");
      advertisedDevice.getScan()->stop();
      pClient->connect(&advertisedDevice);
      Serial.println("Connected to Grassy Core device");

      BLERemoteService *pRemoteService = pClient->getService(SERVICE_UUID);
      if (pRemoteService != nullptr) {
        pRemoteCharacteristic =
            pRemoteService->getCharacteristic(CHARACTERISTIC_UUID);
        if (pRemoteCharacteristic->canWrite()) {
          // 更新時間
          timeClient.update();
          unsigned long epochTime = timeClient.getEpochTime();
          struct tm *ptm = gmtime((time_t *)&epochTime);
          uint8_t hour = (ptm->tm_hour + 8) % 24; // 調整為 +8 時區
          uint8_t minute = ptm->tm_min;
          uint8_t second = ptm->tm_sec;

          // 發送設置時間指令
          sendSetTimeCommand(hour, minute, second); // 使用從 NTP 取得的時間
          delay(50);                                // 等待 50 毫秒

          // 發送固定指令
          sendFixedCommand();
        }
      } else {
        Serial.println("Failed to find the service on Grassy Core");
      }
    }
  }
};

void setup() {
  Serial.begin(115200);

  // 連接 WiFi
  Serial.printf("Connecting to %s ", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" connected");

  // 啟動 NTP 客戶端並更新時間
  timeClient.begin();
  timeClient.update();

  // 斷開 WiFi 連接
  WiFi.disconnect(true);
  Serial.println("WiFi disconnected");

  unsigned long currentTime = timeClient.getEpochTime();
  struct tm *ptm = gmtime((time_t *)&currentTime);
  ptm->tm_hour = (ptm->tm_hour + 8) % 24; // 調整為 +8 時區
  Serial.printf("Current Time after disconnect (+8 timezone): %02d:%02d:%02d",
                ptm->tm_hour, ptm->tm_min, ptm->tm_sec);

  // 初始化 BLE
  BLEDevice::init("");
  pClient = BLEDevice::createClient();
  BLEScan *pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true);
  pBLEScan->start(5, false); // 開始掃描 5 秒
}

void loop() {
  // 主程式循環中無需進行其他操作，因為指令在連接後已自動發送
}
