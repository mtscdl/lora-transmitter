git cl#include <SPI.h>
#include <LoRa.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Update.h>
#include <EEPROM.h>

const int EEPROM_SIZE = 96;

const char* firmware_url = "https://github.com/mtscdl/lora-transmitter/releases/latest/download/firmware.bin";
const char* version_url  = "https://github.com/mtscdl/lora-transmitter/releases/latest/download/version.txt";

const unsigned long otaCheckInterval = 10000; // 10s entre verificações OTA

SPIClass SPI_LoRa(FSPI);

#define SCK     7
#define MISO    9
#define MOSI    11
#define SS      12
#define RST     3
#define DIO0    5

int counter = 0;
unsigned long lastOtaCheck = 0;

char ssid[32];
char password[64];

// Versão atual do firmware
const char* currentVersion = "v1.0";

void readWiFiCredentials() {
  EEPROM.begin(EEPROM_SIZE);

  int lenSsid = EEPROM.read(0);
  for (int i = 0; i < lenSsid; i++) {
    ssid[i] = EEPROM.read(1 + i);
  }
  ssid[lenSsid] = '\0';

  int lenPass = EEPROM.read(32);
  for (int i = 0; i < lenPass; i++) {
    password[i] = EEPROM.read(33 + i);
  }
  password[lenPass] = '\0';

  EEPROM.end();
}

void connectWiFi() {
  WiFi.begin(ssid, password);
  Serial.print("Conectando ao Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi conectado!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

bool checkVersionUpdate() {
  HTTPClient http;
  WiFiClient client;

  Serial.println("[OTA] Checando versão no servidor...");

  http.begin(client, version_url);
  int httpCode = http.GET();

  if (httpCode == 200) {
    String newVersion = http.getString();
    newVersion.trim(); // remove espaços e quebras de linha

    Serial.print("[OTA] Versão atual: ");
    Serial.println(currentVersion);
    Serial.print("[OTA] Versão no servidor: ");
    Serial.println(newVersion);

    http.end();

    if (newVersion != String(currentVersion)) {
      Serial.println("[OTA] Nova versão detectada!");
      return true;
    } else {
      Serial.println("[OTA] Firmware já está atualizado.");
      return false;
    }
  } else {
    Serial.printf("[OTA] Erro ao buscar versão: %d\n", httpCode);
    http.end();
    return false;
  }
}

void checkForUpdates() {
  if (!checkVersionUpdate()) {
    return;
  }

  Serial.println("[OTA] Iniciando download da nova versão...");

  WiFiClient client;
  HTTPClient http;

  http.begin(client, firmware_url);
  int httpCode = http.GET();

  if (httpCode == 200) {
    int contentLength = http.getSize();
    bool canBegin = Update.begin(contentLength);

    if (canBegin) {
      Serial.println("[OTA] Atualizando firmware...");
      WiFiClient* stream = http.getStreamPtr();
      size_t written = Update.writeStream(*stream);

      if (Update.end() && Update.isFinished()) {
        Serial.println("[OTA] Atualização concluída. Reiniciando...");
        delay(1000);
        ESP.restart();
      } else {
        Serial.printf("[OTA] Erro durante atualização: %s\n", Update.errorString());
      }
    } else {
      Serial.println("[OTA] Falha ao iniciar atualização");
    }
  } else {
    Serial.printf("[OTA] Erro HTTP no download do firmware: %d\n", httpCode);
  }

  http.end();
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  readWiFiCredentials();
  connectWiFi();

  SPI_LoRa.begin(SCK, MISO, MOSI, SS);
  LoRa.setSPI(SPI_LoRa);
  LoRa.setPins(SS, RST, DIO0);

  if (!LoRa.begin(433E6)) {
    Serial.println("Falha ao iniciar LoRa");
    while (1);
  }

  LoRa.setTxPower(10);
  LoRa.setSpreadingFactor(7);
  LoRa.setSignalBandwidth(125E3);
  LoRa.setCodingRate4(5);

  Serial.println("LoRa inicializado.");
}

void loop() {
  if (millis() - lastOtaCheck >= otaCheckInterval) {
    lastOtaCheck = millis();
    checkForUpdates();
  }

  if (LoRa.beginPacket()) {
    LoRa.print(counter++);
    LoRa.endPacket();
    Serial.print(currentVersion);
    Serial.println(" | Pacote enviado via LoRa");
  } else {
    Serial.println("Falha ao iniciar pacote LoRa");
  }

  delay(2000);
}