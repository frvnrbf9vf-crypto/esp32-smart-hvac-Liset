#include <DHT.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

// ================= PIN DEFINITIONS =================
#define DHT_PIN       4
#define DHT_TYPE      DHT22
#define LED_PIN       18
#define BUTTON_PIN    15
#define BUZZER_PIN    19
#define POT_PIN       32

// ================= OBJECTS =================
DHT dht(DHT_PIN, DHT_TYPE);

// ================= SYSTEM STATE =================
typedef struct {
  float temperature;
  float voltage;
  bool systemOn;
  bool fault;
} SystemState;

SystemState state;
SemaphoreHandle_t stateMutex;

// ================= INTERRUPT FLAG =================
volatile bool buttonPressed = false;

// ================= INTERRUPT HANDLER =================
void IRAM_ATTR onButtonPress() {
  buttonPressed = true;
}

// ================= SENSOR TASK =================
void sensorTask(void *pvParameters) {
  float filteredVoltage = 0;

  while (true) {
    float temp = dht.readTemperature();
    int rawADC = analogRead(POT_PIN);

    // Convert ADC to voltage
    float voltage = (rawADC / 4095.0f) * 3.3f;

    // Simple smoothing filter
    filteredVoltage = (filteredVoltage * 0.7f) + (voltage * 0.3f);

    xSemaphoreTake(stateMutex, portMAX_DELAY);
    state.temperature = temp;
    state.voltage = filteredVoltage;
    xSemaphoreGive(stateMutex);

    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

// ================= CONTROL TASK =================
void controlTask(void *pvParameters) {

  static unsigned long lastPress = 0;  // debounce timer

  while (true) {
    xSemaphoreTake(stateMutex, portMAX_DELAY);

    // 🔘 Button handling (debounced)
    if (buttonPressed && millis() - lastPress > 200) {
      state.systemOn = !state.systemOn;
      buttonPressed = false;
      lastPress = millis();
    }

    // 🔥 Safety: Overheat shutdown
    if (state.temperature > 50.0f) {
      state.systemOn = false;
      state.fault = true;
    }

    // 💡 HVAC control (LED)
    if (state.systemOn && state.temperature > 25.0f) {
      digitalWrite(LED_PIN, HIGH);
    } else {
      digitalWrite(LED_PIN, LOW);
    }

    // 🔊 Alarm (Buzzer)
    if (state.fault) {
       tone(BUZZER_PIN, 1000);  // 1000 Hz sound
    } else {
      noTone(BUZZER_PIN);
    }

    xSemaphoreGive(stateMutex);

    vTaskDelay(pdMS_TO_TICKS(500));
  }
}

// ================= LOGGING TASK =================
void loggingTask(void *pvParameters) {
  while (true) {
    xSemaphoreTake(stateMutex, portMAX_DELAY);

    Serial.println("------ SYSTEM STATUS ------");
    Serial.print("Temperature: ");
    Serial.println(state.temperature);

    Serial.print("Voltage: ");
    Serial.println(state.voltage);

    Serial.print("System ON: ");
    Serial.println(state.systemOn ? "YES" : "NO");

    Serial.print("Fault: ");
    Serial.println(state.fault ? "YES" : "NO");

  Serial.println(digitalRead(BUTTON_PIN));
  
    xSemaphoreGive(stateMutex);

    vTaskDelay(pdMS_TO_TICKS(2000));
  }
}

// ================= SETUP =================
void setup() {
  Serial.begin(115200);

  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);

  dht.begin();

  // Create mutex
  stateMutex = xSemaphoreCreateMutex();

  // Attach interrupt
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), onButtonPress, FALLING);

  // Initial state
  state.systemOn = true;
  state.fault = false;

  // Create RTOS tasks
  xTaskCreate(sensorTask, "SensorTask", 2048, NULL, 1, NULL);
  xTaskCreate(controlTask, "ControlTask", 2048, NULL, 2, NULL);
  xTaskCreate(loggingTask, "LoggingTask", 2048, NULL, 1, NULL);
}

// ================= LOOP =================
void loop() {
  // Not used (RTOS handles everything)
}