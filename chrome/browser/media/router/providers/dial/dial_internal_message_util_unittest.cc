// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/dial/dial_internal_message_util.h"

#include "base/json/json_reader.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/media/router/discovery/dial/dial_app_discovery_service.h"
#include "chrome/browser/media/router/providers/dial/dial_activity_manager.h"
#include "chrome/browser/media/router/test/provider_test_helpers.h"
#include "components/media_router/common/test/test_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media_router {

class DialInternalMessageUtilTest : public ::testing::Test {
 public:
  DialInternalMessageUtilTest()
      : launch_info_("YouTube",
                     std::nullopt,
                     "152127444812943594",
                     GURL("http://172.17.32.151/app/YouTube")),
        util_("hash-token") {
    MediaSink sink{
        CreateDialSink("dial:<29a400068c051073801508058128105d>", "Lab Roku")};
    DialSinkExtraData extra_data;
    extra_data.ip_address = net::IPAddress(172, 17, 32, 151);
    sink_ = MediaSinkInternal(sink, extra_data);
  }

  void ExpectMessagesEqual(const std::string& expected_message,
                           const std::string& message) {
    auto expected_message_value = base::JSONReader::Read(expected_message);
    ASSERT_TRUE(expected_message_value);

    auto message_value = base::JSONReader::Read(message);
    ASSERT_TRUE(message_value);

    EXPECT_EQ(*expected_message_value, *message_value);
  }

 protected:
  DialLaunchInfo launch_info_;
  MediaSinkInternal sink_;
  DialInternalMessageUtil util_;
};

TEST_F(DialInternalMessageUtilTest, ParseClientConnectMessage) {
  constexpr char kClientConnectMessage[] = R"(
        {
          "type":"client_connect",
          "message":"15212681945883010",
          "sequenceNumber":-1,
          "timeoutMillis":0,
          "clientId":"15212681945883010"
        })";

  auto message = ParseDialInternalMessage(kClientConnectMessage);
  ASSERT_TRUE(message);
  EXPECT_EQ(DialInternalMessageType::kClientConnect, message->type);
  EXPECT_EQ(base::Value("15212681945883010"), message->body);
  EXPECT_EQ("15212681945883010", message->client_id);
  EXPECT_EQ(-1, message->sequence_number);
}

TEST_F(DialInternalMessageUtilTest, ParseCustomDialLaunchMessage) {
  constexpr char kCustomDialLaunchMessage[] = R"(
  {
    "type":"custom_dial_launch",
    "message": {
      "doLaunch":true,
      "launchParameter":"pairingCode=foo"
    },
    "sequenceNumber":12345,
    "timeoutMillis":3000,
    "clientId":"152127444812943594"
  })";

  auto message = ParseDialInternalMessage(kCustomDialLaunchMessage);
  ASSERT_TRUE(message);
  EXPECT_EQ(DialInternalMessageType::kCustomDialLaunch, message->type);
  EXPECT_EQ("152127444812943594", message->client_id);
  EXPECT_EQ(12345, message->sequence_number);

  CustomDialLaunchMessageBody body =
      CustomDialLaunchMessageBody::From(*message);
  EXPECT_TRUE(body.do_launch);
  EXPECT_EQ("pairingCode=foo", body.launch_parameter);
}

TEST_F(DialInternalMessageUtilTest, ParseV2StopSessionMessage) {
  constexpr char kV2StopSessionMessage[] = R"(
  {
    "type":"v2_message",
    "message": {
      "type":"STOP"
    },
    "sequenceNumber":-1,
    "timeoutMillis":0,
    "clientId":"152127444812943594"
  })";

  auto message = ParseDialInternalMessage(kV2StopSessionMessage);
  ASSERT_TRUE(message);
  EXPECT_EQ(DialInternalMessageType::kV2Message, message->type);
  EXPECT_EQ("152127444812943594", message->client_id);
  EXPECT_EQ(-1, message->sequence_number);

  EXPECT_TRUE(DialInternalMessageUtil::IsStopSessionMessage(*message));
}

TEST_F(DialInternalMessageUtilTest, CreateReceiverActionCastMessage) {
  constexpr char kReceiverActionCastMessage[] = R"(
    {
      "clientId":"152127444812943594",
      "message": {
        "action":"cast",
        "receiver": {
          "capabilities":[],
          "displayStatus":null,
          "friendlyName":"Lab Roku",
          "ipAddress":"172.17.32.151",
          "isActiveInput":null,
          "label":"vgK6BDL84IzefOLUvy2OcgFPhoo",
          "receiverType":"dial",
          "volume":null
        }
      },
      "sequenceNumber":-1,
      "timeoutMillis":0,
      "type":"receiver_action"
    })";

  auto message =
      util_.CreateReceiverActionCastMessage(launch_info_.client_id, sink_);
  ASSERT_TRUE(message->message);
  ExpectMessagesEqual(kReceiverActionCastMessage, message->message.value());
}

TEST_F(DialInternalMessageUtilTest, CreateReceiverActionStopMessage) {
  constexpr char kReceiverActionStopMessage[] = R"(
    {
      "clientId":"152127444812943594",
      "message": {
        "action":"stop",
        "receiver": {
          "capabilities":[],
          "displayStatus":null,
          "friendlyName":"Lab Roku",
          "ipAddress":"172.17.32.151",
          "isActiveInput":null,
          "label":"vgK6BDL84IzefOLUvy2OcgFPhoo",
          "receiverType":"dial",
          "volume":null
        }
      },
      "sequenceNumber":-1,
      "timeoutMillis":0,
      "type":"receiver_action"
    })";

  auto message =
      util_.CreateReceiverActionStopMessage(launch_info_.client_id, sink_);
  ASSERT_TRUE(message->message);
  ExpectMessagesEqual(kReceiverActionStopMessage, message->message.value());
}

TEST_F(DialInternalMessageUtilTest, CreateNewSessionMessage) {
  constexpr char kNewSessionMessage[] = R"(
  {
    "clientId":"152127444812943594",
    "message": {
      "appId":"",
      "appImages":[],
      "displayName":"YouTube",
      "media":[],
      "namespaces":[],
      "receiver": {
        "capabilities":[],
        "displayStatus":null,
        "friendlyName":"Lab Roku",
        "ipAddress":"172.17.32.151",
        "isActiveInput":null,
        "label":"vgK6BDL84IzefOLUvy2OcgFPhoo",
        "receiverType":"dial",
        "volume":null
      },
      "senderApps":[],
      "sessionId":"1",
      "status":"connected",
      "statusText":"",
      "transportId":""
    },
    "sequenceNumber":-1,
    "timeoutMillis":0,
    "type":"new_session"
  })";

  auto message = util_.CreateNewSessionMessage(launch_info_.app_name,
                                               launch_info_.client_id, sink_);
  ASSERT_TRUE(message->message);
  ExpectMessagesEqual(kNewSessionMessage, message->message.value());
}

TEST_F(DialInternalMessageUtilTest, CreateCustomDialLaunchMessage) {
  constexpr char kCustomDialLaunchMessage[] = R"(
  {
    "clientId":"152127444812943594",
    "message": {
      "appState":"stopped",
      "receiver": {
        "capabilities":[],
        "displayStatus":null,
        "friendlyName":"Lab Roku",
        "ipAddress":"172.17.32.151",
        "isActiveInput":null,
        "label":"vgK6BDL84IzefOLUvy2OcgFPhoo",
        "receiverType":"dial",
        "volume":null
      },
      "extraData": {}
    },
    "sequenceNumber":%d,
    "timeoutMillis":0,
    "type":"custom_dial_launch"
  })";

  ParsedDialAppInfo app_info =
      CreateParsedDialAppInfo("YouTube", DialAppState::kStopped);
  auto message_and_seq_num = util_.CreateCustomDialLaunchMessage(
      launch_info_.client_id, sink_, app_info);
  const auto& message = message_and_seq_num.first;
  int seq_num = message_and_seq_num.second;
  ASSERT_TRUE(message->message);
  ExpectMessagesEqual(base::StringPrintf(kCustomDialLaunchMessage, seq_num),
                      message->message.value());
}

TEST_F(DialInternalMessageUtilTest, CreateDialAppInfoParsingErrorMessage) {
  constexpr char kClientId[] = "152127444812943594";
  constexpr char kErrorMessage[] = R"(
  {
    "clientId": "%s",
    "type": "error",
    "message": {
      "code": "parsing_error",
      "description": "XML parsing error"
    },
    "sequenceNumber": %d,
    "timeoutMillis": 0
  })";
  const int kSequenceNumber = 42;

  auto message = util_.CreateDialAppInfoErrorMessage(
      DialAppInfoResultCode::kParsingError, kClientId, kSequenceNumber,
      "XML parsing error");
  ExpectMessagesEqual(
      base::StringPrintf(kErrorMessage, kClientId, kSequenceNumber),
      message->message.value());
}

TEST_F(DialInternalMessageUtilTest, CreateDialAppInfoHttpErrorMessage) {
  constexpr char kClientId[] = "152127444812943594";
  constexpr char kErrorMessage[] = R"(
  {
    "clientId": "%s",
    "type": "error",
    "message": {
      "code": "http_error",
      "description": "",
      "details": {
        "http_error_code": 404
      }
    },
    "sequenceNumber": %d,
    "timeoutMillis": 0
  })";
  const int kSequenceNumber = 42;

  auto message = util_.CreateDialAppInfoErrorMessage(
      DialAppInfoResultCode::kHttpError, kClientId, kSequenceNumber, "", 404);
  ExpectMessagesEqual(
      base::StringPrintf(kErrorMessage, kClientId, kSequenceNumber),
      message->message.value());
}

}  // namespace media_router
