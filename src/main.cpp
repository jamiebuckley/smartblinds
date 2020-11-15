
#include <ESP8266WiFi.h>
#include <Wire.h>
#include <AzureIoTHub.h>
#include "AzureIoTProtocol_MQTT.h"
#include "iothubtransportmqtt.h"
#include "Stepper.h"


#define sample_init esp8266_sample_init
#define is_esp_board
void esp8266_sample_init(const char* ssid, const char* password);

const int stepsPerRevolution = 200;
const char* connection_string = "HostName=smart-blinds-hub.azure-devices.net;DeviceId=test-device;SharedAccessKey=zWN/F9JK6oZAeN1894BpV1EXZQnDBavJRjF7hOmzLHk=;";

IOTHUB_DEVICE_CLIENT_LL_HANDLE device_ll_handle;
int receiveContext = 0;

const char* close_blinds_message = "close blinds";
const char* open_blinds_message = "open blinds";

static bool g_continueRunning = true;

int mode = LOW;

Stepper myStepper(stepsPerRevolution, 14, 12, 13, 4);
int stepCount = 0;  // number of steps the motor has taken

static IOTHUBMESSAGE_DISPOSITION_RESULT receive_message_callback(IOTHUB_MESSAGE_HANDLE message, void* userContextCallback)
{
    int* counter = (int*)userContextCallback;
    const unsigned char* buffer;
    size_t size;
    const char* messageId;

    // Message properties
    if ((messageId = IoTHubMessage_GetMessageId(message)) == NULL)
    {
        messageId = "<null>";
    }

    // Message content
    if (IoTHubMessage_GetByteArray(message, (const unsigned char**)&buffer, &size) != IOTHUB_MESSAGE_OK)
    {
        Serial.println("Unable to retrieve the message data");
    }
    else
    {
        Serial.println("Received message");
        LogInfo("Received Message [%d]\r\n Message ID: %s\r\n Data: <<<%.*s>>> & Size=%d\r\n", *counter, messageId, (int)size, buffer, (int)size);
        Serial.print((char*)buffer);
        // If we receive the word 'quit' then we stop running
        size_t open_blinds_length = strlen(open_blinds_message) * sizeof(char);
        size_t close_blinds_length = strlen(close_blinds_message) * sizeof(char);
        if (size >= close_blinds_length && memcmp(buffer, close_blinds_message, close_blinds_length - 1) == 0)
        {
            Serial.println("Close blinds");
            mode = HIGH;
        }
        else if (size >= open_blinds_length && memcmp(buffer, open_blinds_message, close_blinds_length - 1) == 0)
        {
            Serial.println("Open blinds");
            mode = LOW;
        }
    }

    /* Some device specific action code goes here... */
    (*counter)++;
    return IOTHUBMESSAGE_ACCEPTED;
}

static void send_confirm_callback(IOTHUB_CLIENT_CONFIRMATION_RESULT result, void* userContextCallback)
{
    (void)userContextCallback;
    Serial.println("Confirmation callback receieved");
}

static void connection_status_callback(IOTHUB_CLIENT_CONNECTION_STATUS result, IOTHUB_CLIENT_CONNECTION_STATUS_REASON reason, void* user_context)
{
    (void)reason;
    (void)user_context;
    // This sample DOES NOT take into consideration network outages.
    if (result == IOTHUB_CLIENT_CONNECTION_AUTHENTICATED)
    {
        Serial.println("The device client is connected to iothub");
    }
    else
    {
        Serial.println("The device client has been disconnected");
        Serial.println(MU_ENUM_TO_STRING(IOTHUB_CLIENT_CONNECTION_STATUS, result));
        Serial.println(MU_ENUM_TO_STRING(IOTHUB_CLIENT_CONNECTION_STATUS_REASON, reason));
    }
}

void setup() {
    pinMode(LED_BUILTIN, OUTPUT);
    sample_init(WIFI_SSID, WIFI_PASS);


    IoTHub_Init();
    device_ll_handle = IoTHubDeviceClient_LL_CreateFromConnectionString(connection_string, MQTT_Protocol);

    Serial.println("Creating MQTT handle");
    if (device_ll_handle == NULL)
    {
        Serial.println("Error AZ002: Failure creating Iothub device. Hint: Check you connection string");
        return;
    }

    int diag_off = 0;
    IoTHubDeviceClient_LL_SetOption(device_ll_handle, OPTION_DIAGNOSTIC_SAMPLING_PERCENTAGE, &diag_off);

    bool traceOn = true;
    IoTHubDeviceClient_LL_SetOption(device_ll_handle, OPTION_LOG_TRACE, &traceOn);

    IoTHubDeviceClient_LL_SetOption(device_ll_handle, OPTION_TRUSTED_CERT, certificates);

    bool urlEncodeOn = true;
    IoTHubDeviceClient_LL_SetOption(device_ll_handle, OPTION_AUTO_URL_ENCODE_DECODE, &urlEncodeOn);
    /* Setting Message call back, so we can receive Commands. */
    if (IoTHubClient_LL_SetMessageCallback(device_ll_handle, receive_message_callback, &receiveContext) != IOTHUB_CLIENT_OK)
    {
        LogInfo("ERROR: IoTHubClient_LL_SetMessageCallback..........FAILED!\r\n");
    }

    (void)IoTHubDeviceClient_LL_SetConnectionStatusCallback(device_ll_handle, connection_status_callback, NULL);

    Serial.println("Sending message");
    IOTHUB_MESSAGE_HANDLE message_handle = IoTHubMessage_CreateFromString("test_message");
    int result = IoTHubDeviceClient_LL_SendEventAsync(device_ll_handle, message_handle, send_confirm_callback, NULL);
    IoTHubMessage_Destroy(message_handle);

    Serial.println("Doing work");
}

void loop() {
    IoTHubDeviceClient_LL_DoWork(device_ll_handle);
    delay(1000);
    Serial.println(mode);
    digitalWrite(LED_BUILTIN, mode);

    delay(10);
    myStepper.setSpeed(60);
    delay(10);
    myStepper.step(stepsPerRevolution);
    delay(2000);
}