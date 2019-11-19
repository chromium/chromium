// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/dial/dial_internal_message_util.h"

#include "base/json/json_reader.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/media/router/providers/dial/dial_activity_manager.h"
#include "chrome/browser/media/router/test/test_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media_router {

class DialInternalMessageUtilTest : public ::testing::Test {
 public:
  DialInternalMessageUtilTest()
      : launch_info_("YouTube",
                     base::nullopt,
                     "152127444812943594",
                     GURL("http://172.17.32.151/app/YouTube")),
        util_("hash-token") {
    MediaSink sink("dial:<29a400068c051073801508058128105d>", "Lab Roku",
                   SinkIconType::GENERIC);
    DialSinkExtraData extra_data;
    extra_data.ip_address = net::IPAddress(172, 17, 32, 151);
    sink_ = MediaSinkInternal(sink, extra_data);
  }

  void ExpectMessagesEqual(const std::string& expected_message,
                           const std::string& message) {
    auto expected_message_value =
        base::JSONReader::ReadDeprecated(expected_message);
    ASSERT_TRUE(expected_message_value);

    auto message_value = base::JSONReader::ReadDeprecated(message);
    ASSERT_TRUE(message_value);

    EXPECT_EQ(*expected_message_value, *message_value);
  }

 protected:
  DialLaunchInfo launch_info_;
  MediaSinkInternal sink_;
  DialInternalMessageUtil util_;
};

TEST_F(DialInternalMessageUtilTest, ParseClientConnectMessage) {
  const char kClientConnectMessage[] = R"(
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
  const char kCustomDialLaunchMessage[] = R"(
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
  const char kV2StopSessionMessage[] = R"(
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
  const char kReceiverActionCastMessage[] = R"(
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

  auto message = util_.CreateReceiverActionCastMessage(launch_info_, sink_);
  ASSERT_TRUE(message->message);
  ExpectMessagesEqual(kReceiverActionCastMessage, message->message.value());
}

TEST_F(DialInternalMessageUtilTest, CreateReceiverActionStopMessage) {
  const char kReceiverActionStopMessage[] = R"(
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

  auto message = util_.CreateReceiverActionStopMessage(launch_info_, sink_);
  ASSERT_TRUE(message->message);
  ExpectMessagesEqual(kReceiverActionStopMessage, message->message.value());
}

TEST_F(DialInternalMessageUtilTest, CreateNewSessionMessage) {
  const char kNewSessionMessage[] = R"(
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

  auto message = util_.CreateNewSessionMessage(launch_info_, sink_);
  ASSERT_TRUE(message->message);
  ExpectMessagesEqual(kNewSessionMessage, message->message.value());
}

TEST_F(DialInternalMessageUtilTest, CreateCustomDialLaunchMessage) {
  const char kCustomDialLaunchMessage[] = R"(
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
      }
    },
    "sequenceNumber":%d,
    "timeoutMillis":0,
    "type":"custom_dial_launch"
  })";

  ParsedDialAppInfo app_info =
      CreateParsedDialAppInfo("YouTube", DialAppState::kStopped);
  auto message_and_seq_num =
      util_.CreateCustomDialLaunchMessage(launch_info_, sink_, app_info);
  const auto& message = message_and_seq_num.first;
  int seq_num = message_and_seq_num.second;
  ASSERT_TRUE(message->message);
  ExpectMessagesEqual(base::StringPrintf(kCustomDialLaunchMessage, seq_num),
                      message->message.value());
}

}  // namespace media_router
