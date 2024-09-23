// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/cast/cast_internal_message_util.h"

#include "base/json/json_reader.h"
#include "base/test/gtest_util.h"
#include "build/build_config.h"
#include "chrome/browser/media/router/test/provider_test_helpers.h"
#include "components/media_router/common/providers/cast/channel/cast_test_util.h"
#include "components/media_router/common/test/test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::test::IsJson;
using base::test::ParseJson;
using base::test::ParseJsonDict;

namespace media_router {

namespace {

static constexpr char kReceiverIdToken[] = "token";

base::Value::Dict ReceiverStatus() {
  std::string receiver_status_str = R"({
      "applications": [{
        "appId": "ABCDEFGH",
        "displayName": "App display name",
        "namespaces": [
          {"name": "urn:x-cast:com.google.cast.media"},
          {"name": "urn:x-cast:com.google.foo"}
        ],
        "sessionId": "sessionId",
        "statusText":"App status",
        "transportId":"transportId"
      }]
  })";
  return ParseJsonDict(receiver_status_str);
}

// appId: native app ID
// universalAppId: web receiver app ID, which is appId in ReceiverStatus without
// universalAppId or appType
base::Value::Dict ReceiverStatusWithUniversalAppId() {
  std::string receiver_status_str = R"({
      "applications": [{
        "appId": "AD9AF8E0",
        "displayName": "App display name",
        "namespaces": [
          {"name": "urn:x-cast:com.google.cast.media"},
          {"name": "urn:x-cast:com.google.foo"}
        ],
        "sessionId": "sessionId",
        "statusText":"App status",
        "transportId":"transportId",
        "universalAppId": "ABCDEFGH",
        "appType": "ANDROID_TV"
      }]
  })";
  return ParseJsonDict(receiver_status_str);
}

void ExpectNoCastSession(const MediaSinkInternal& sink,
                         const std::string& receiver_status_str,
                         const std::string& reason) {
  auto session = CastSession::From(sink, ParseJsonDict(receiver_status_str));
  EXPECT_FALSE(session) << "Shouldn't have created session because of "
                        << reason;
}

void ExpectInvalidCastInternalMessage(const std::string& message_str,
                                      const std::string& invalid_reason) {
  EXPECT_FALSE(CastInternalMessage::From(ParseJsonDict(message_str)))
      << "message expected to be invlaid: " << invalid_reason;
}

class CastInternalMessageUtilDeathTest : public testing::Test {
 public:
  void SetUp() override { GTEST_FLAG_SET(death_test_style, "threadsafe"); }
};

}  // namespace

// TODO(crbug.com/1377730): This test sometimes times out on the Win bot.
#if BUILDFLAG(IS_WIN)
#define MAYBE_CastInternalMessageFromAppMessageString \
  DISABLED_CastInternalMessageFromAppMessageString
#else
#define MAYBE_CastInternalMessageFromAppMessageString \
  CastInternalMessageFromAppMessageString
#endif
TEST_F(CastInternalMessageUtilDeathTest,
       MAYBE_CastInternalMessageFromAppMessageString) {
  std::string message_str = R"({
    "type": "app_message",
    "clientId": "12345",
    "sequenceNumber": 999,
    "message": {
      "namespaceName": "urn:x-cast:com.google.foo",
      "sessionId": "sessionId",
      "message": { "foo": "bar" }
    }
  })";

  auto message = CastInternalMessage::From(ParseJsonDict(message_str));
  ASSERT_TRUE(message);
  EXPECT_EQ(CastInternalMessage::Type::kAppMessage, message->type());
  EXPECT_EQ("12345", message->client_id());
  EXPECT_EQ(999, message->sequence_number());
  EXPECT_EQ("urn:x-cast:com.google.foo", message->app_message_namespace());
  EXPECT_EQ("sessionId", message->session_id());
  base::Value::Dict message_body;
  message_body.Set("foo", base::Value("bar"));
  EXPECT_EQ(message_body, message->app_message_body());

  EXPECT_DCHECK_DEATH(message->v2_message_type());
  EXPECT_DCHECK_DEATH(message->v2_message_body());
}

TEST_F(CastInternalMessageUtilDeathTest,
       CastInternalMessageFromV2MessageString) {
  std::string message_str = R"({
    "type": "v2_message",
    "clientId": "12345",
    "sequenceNumber": 999,
    "message": {
      "type": "v2_message_type",
      "sessionId": "sessionId",
      "foo": "bar"
    }
  })";

  auto message = CastInternalMessage::From(ParseJsonDict(message_str));
  ASSERT_TRUE(message);
  EXPECT_EQ(CastInternalMessage::Type::kV2Message, message->type());
  EXPECT_EQ("12345", message->client_id());
  EXPECT_EQ(999, message->sequence_number());
  EXPECT_EQ("sessionId", message->session_id());
  EXPECT_EQ("v2_message_type", message->v2_message_type());
  auto v2_body = ParseJson(R"({
      "type": "v2_message_type",
      "sessionId": "sessionId",
      "foo": "bar"
    })");
  EXPECT_EQ(v2_body, message->v2_message_body());

  EXPECT_DCHECK_DEATH(message->app_message_namespace());
  EXPECT_DCHECK_DEATH(message->app_message_body());
}

// TODO(crbug.com/1378312): This test sometimes times out on the Win debug bot.
#if BUILDFLAG(IS_WIN) && !defined(NDEBUG) || BUILDFLAG(IS_LINUX)
#define MAYBE_CastInternalMessageFromClientConnectString \
  DISABLED_CastInternalMessageFromClientConnectString
#else
#define MAYBE_CastInternalMessageFromClientConnectString \
  CastInternalMessageFromClientConnectString
#endif
TEST_F(CastInternalMessageUtilDeathTest,
       MAYBE_CastInternalMessageFromClientConnectString) {
  std::string message_str = R"({
      "type": "client_connect",
      "clientId": "12345",
      "message": {}
    })";

  auto message = CastInternalMessage::From(ParseJsonDict(message_str));
  ASSERT_TRUE(message);
  EXPECT_EQ(CastInternalMessage::Type::kClientConnect, message->type());
  EXPECT_EQ("12345", message->client_id());
  EXPECT_FALSE(message->sequence_number());

  EXPECT_DCHECK_DEATH(message->session_id());
  EXPECT_DCHECK_DEATH(message->v2_message_type());
  EXPECT_DCHECK_DEATH(message->v2_message_body());
  EXPECT_DCHECK_DEATH(message->app_message_namespace());
  EXPECT_DCHECK_DEATH(message->app_message_body());
}

TEST(CastInternalMessageUtilTest, CastInternalMessageFromInvalidStrings) {
  std::string unknown_type = R"({
      "type": "some_unknown_type",
      "clientId": "12345",
      "message": {}
    })";
  ExpectInvalidCastInternalMessage(unknown_type, "unknown_type");

  std::string missing_client_id = R"({
      "type": "client_connect",
      "message": {}
    })";
  ExpectInvalidCastInternalMessage(missing_client_id, "missing client ID");

  std::string missing_message = R"({
      "type": "client_connect",
      "clientId": "12345"
    })";
  ExpectInvalidCastInternalMessage(missing_message, "missing message");

  std::string app_message_missing_namespace = R"({
    "type": "app_message",
    "clientId": "12345",
    "sequenceNumber": 999,
    "message": {
      "sessionId": "sessionId",
      "message": { "foo": "bar" }
    }
  })";
  ExpectInvalidCastInternalMessage(app_message_missing_namespace,
                                   "missing namespace");

  std::string app_message_missing_session_id = R"({
    "type": "app_message",
    "clientId": "12345",
    "sequenceNumber": 999,
    "message": {
      "namespaceName": "urn:x-cast:com.google.foo",
      "message": { "foo": "bar" }
    }
  })";
  ExpectInvalidCastInternalMessage(app_message_missing_session_id,
                                   "missing session ID");

  std::string app_message_missing_message = R"({
    "type": "app_message",
    "clientId": "12345",
    "sequenceNumber": 999,
    "message": {
      "namespaceName": "urn:x-cast:com.google.foo",
      "sessionId": "sessionId"
    }
  })";
  ExpectInvalidCastInternalMessage(app_message_missing_message,
                                   "missing app message");
}

TEST(CastInternalMessageUtilTest, CastSessionFromReceiverStatusNoStatusText) {
  MediaSinkInternal sink = CreateCastSink(1);
  std::string receiver_status_str = R"({
      "applications": [{
        "appId": "ABCDEFGH",
        "displayName": "App display name",
        "namespaces": [
          {"name": "urn:x-cast:com.google.cast.media"},
          {"name": "urn:x-cast:com.google.foo"}
        ],
        "sessionId": "sessionId",
        "transportId":"transportId"
      }]
  })";
  auto session = CastSession::From(sink, ParseJsonDict(receiver_status_str));
  ASSERT_TRUE(session);
  EXPECT_EQ("sessionId", session->session_id());
  EXPECT_EQ("ABCDEFGH", session->app_id());
  EXPECT_EQ("transportId", session->destination_id());
  base::flat_set<std::string> message_namespaces = {
      "urn:x-cast:com.google.cast.media", "urn:x-cast:com.google.foo"};
  EXPECT_EQ(message_namespaces, session->message_namespaces());
  EXPECT_EQ("App display name", session->GetRouteDescription());
}

TEST(CastInternalMessageUtilTest, CastSessionFromInvalidReceiverStatuses) {
  MediaSinkInternal sink = CreateCastSink(1);
  std::string missing_app_id = R"({
      "applications": [{
        "displayName": "App display name",
        "namespaces": [
          {"name": "urn:x-cast:com.google.cast.media"},
          {"name": "urn:x-cast:com.google.foo"}
        ],
        "sessionId": "sessionId",
        "statusText":"App status",
        "transportId":"transportId"
      }]
  })";
  ExpectNoCastSession(sink, missing_app_id, "missing app id");

  std::string missing_display_name = R"({
      "applications": [{
        "appId": "ABCDEFGH",
        "namespaces": [
          {"name": "urn:x-cast:com.google.cast.media"},
          {"name": "urn:x-cast:com.google.foo"}
        ],
        "sessionId": "sessionId",
        "statusText":"App status",
        "transportId":"transportId"
      }]
  })";
  ExpectNoCastSession(sink, missing_display_name, "missing display name");

  std::string missing_namespaces = R"({
      "applications": [{
        "appId": "ABCDEFGH",
        "displayName": "App display name",
        "namespaces": [],
        "sessionId": "sessionId",
        "statusText":"App status",
        "transportId":"transportId"
      }]
  })";
  ExpectNoCastSession(sink, missing_namespaces, "missing namespaces");

  std::string missing_session_id = R"({
      "applications": [{
        "appId": "ABCDEFGH",
        "displayName": "App display name",
        "namespaces": [
          {"name": "urn:x-cast:com.google.cast.media"},
          {"name": "urn:x-cast:com.google.foo"}
        ],
        "statusText":"App status",
        "transportId":"transportId"
      }]
  })";
  ExpectNoCastSession(sink, missing_session_id, "missing session id");

  std::string missing_transport_id = R"({
      "applications": [{
        "appId": "ABCDEFGH",
        "displayName": "App display name",
        "namespaces": [
          {"name": "urn:x-cast:com.google.cast.media"},
          {"name": "urn:x-cast:com.google.foo"}
        ],
        "sessionId": "sessionId",
        "statusText":"App status"
      }]
  })";
  ExpectNoCastSession(sink, missing_transport_id, "missing transport id");
}

TEST(CastInternalMessageUtilTest, CreateReceiverActionCastMessage) {
  std::string client_id = "clientId";
  MediaSinkInternal sink = CreateCastSink(1);

  auto message =
      CreateReceiverActionCastMessage(client_id, sink, kReceiverIdToken);
  EXPECT_THAT(message, IsPresentationConnectionMessage(R"({
     "clientId": "clientId",
     "message": {
        "action": "cast",
        "receiver": {
           "capabilities": [ "video_out", "audio_out" ],
           "displayStatus": null,
           "friendlyName": "friendly name 1",
           "isActiveInput": null,
           "label": "Pr2CqqG4_NOPcVV6oKbocxyAik0",
           "receiverType": "cast",
           "volume": null
        }
     },
     "timeoutMillis": 0,
     "type": "receiver_action"
    })"));
}

TEST(CastInternalMessageUtilTest, CreateReceiverActionStopMessage) {
  std::string client_id = "clientId";
  MediaSinkInternal sink = CreateCastSink(1);

  auto message =
      CreateReceiverActionStopMessage(client_id, sink, kReceiverIdToken);
  EXPECT_THAT(message, IsPresentationConnectionMessage(R"({
     "clientId": "clientId",
     "message": {
        "action": "stop",
        "receiver": {
           "capabilities": [ "video_out", "audio_out" ],
           "displayStatus": null,
           "friendlyName": "friendly name 1",
           "isActiveInput": null,
           "label": "Pr2CqqG4_NOPcVV6oKbocxyAik0",
           "receiverType": "cast",
           "volume": null
        }
     },
     "timeoutMillis": 0,
     "type": "receiver_action"
    })"));
}

TEST(CastInternalMessageUtilTest, CreateNewSessionMessage) {
  MediaSinkInternal sink = CreateCastSink(1);
  std::string client_id = "clientId";
  auto session = CastSession::From(sink, ReceiverStatus());
  ASSERT_TRUE(session);

  auto message =
      CreateNewSessionMessage(*session, client_id, sink, kReceiverIdToken);
  EXPECT_THAT(message, IsPresentationConnectionMessage(R"({
   "clientId": "clientId",
   "message": {
      "appId": "ABCDEFGH",
      "appImages": [  ],
      "displayName": "App display name",
      "namespaces": [ {
         "name": "urn:x-cast:com.google.cast.media"
      }, {
         "name": "urn:x-cast:com.google.foo"
      } ],
      "receiver": {
         "capabilities": [ "video_out", "audio_out" ],
         "displayStatus": null,
         "friendlyName": "friendly name 1",
         "isActiveInput": null,
         "label": "Pr2CqqG4_NOPcVV6oKbocxyAik0",
         "receiverType": "cast",
         "volume": null
      },
      "senderApps": [  ],
      "sessionId": "sessionId",
      "statusText": "App status",
      "transportId": "transportId"
   },
   "timeoutMillis": 0,
   "type": "new_session"
  })"));
}

TEST(CastInternalMessageUtilTest, CreateNewSessionMessageWithUniversalAppId) {
  MediaSinkInternal sink = CreateCastSink(1);
  std::string client_id = "clientId";
  auto session = CastSession::From(sink, ReceiverStatusWithUniversalAppId());
  ASSERT_TRUE(session);

  auto message =
      CreateNewSessionMessage(*session, client_id, sink, kReceiverIdToken);
  EXPECT_THAT(message, IsPresentationConnectionMessage(R"({
   "clientId": "clientId",
   "message": {
      "appId": "AD9AF8E0",
      "appImages": [  ],
      "displayName": "App display name",
      "namespaces": [ {
         "name": "urn:x-cast:com.google.cast.media"
      }, {
         "name": "urn:x-cast:com.google.foo"
      } ],
      "receiver": {
         "capabilities": [ "video_out", "audio_out" ],
         "displayStatus": null,
         "friendlyName": "friendly name 1",
         "isActiveInput": null,
         "label": "Pr2CqqG4_NOPcVV6oKbocxyAik0",
         "receiverType": "cast",
         "volume": null
      },
      "senderApps": [  ],
      "sessionId": "sessionId",
      "statusText": "App status",
      "transportId": "transportId",
      "universalAppId": "ABCDEFGH",
      "appType": "ANDROID_TV"
   },
   "timeoutMillis": 0,
   "type": "new_session"
  })"));
}

TEST(CastInternalMessageUtilTest, CreateUpdateSessionMessage) {
  MediaSinkInternal sink = CreateCastSink(1);
  std::string client_id = "clientId";
  auto session = CastSession::From(sink, ReceiverStatus());
  ASSERT_TRUE(session);

  auto message =
      CreateUpdateSessionMessage(*session, client_id, sink, kReceiverIdToken);
  EXPECT_THAT(message, IsPresentationConnectionMessage(R"({
   "clientId": "clientId",
   "message": {
      "appId": "ABCDEFGH",
      "appImages": [  ],
      "displayName": "App display name",
      "namespaces": [ {
         "name": "urn:x-cast:com.google.cast.media"
      }, {
         "name": "urn:x-cast:com.google.foo"
      } ],
      "receiver": {
         "capabilities": [ "video_out", "audio_out" ],
         "displayStatus": null,
         "friendlyName": "friendly name 1",
         "isActiveInput": null,
         "label": "Pr2CqqG4_NOPcVV6oKbocxyAik0",
         "receiverType": "cast",
         "volume": null
      },
      "senderApps": [  ],
      "sessionId": "sessionId",
      "statusText": "App status",
      "transportId": "transportId"
   },
   "timeoutMillis": 0,
   "type": "update_session"
  })"));
}

TEST(CastInternalMessageUtilTest, CreateAppMessageAck) {
  std::string client_id = "clientId";
  int sequence_number = 12345;

  auto message = CreateAppMessageAck(client_id, sequence_number);
  EXPECT_THAT(message, IsPresentationConnectionMessage(R"({
   "clientId": "clientId",
   "message": null,
   "sequenceNumber": 12345,
   "timeoutMillis": 0,
   "type": "app_message"
  })"));
}

TEST(CastInternalMessageUtilTest, CreateAppMessage) {
  std::string session_id = "sessionId";
  std::string client_id = "clientId";
  base::Value::Dict message_body;
  message_body.Set("foo", base::Value("bar"));
  openscreen::cast::proto::CastMessage cast_message =
      cast_channel::CreateCastMessage("urn:x-cast:com.google.foo",
                                      base::Value(std::move(message_body)),
                                      "sourceId", "transportId");

  auto message = CreateAppMessage(session_id, client_id, cast_message);
  EXPECT_THAT(message, IsPresentationConnectionMessage(R"({
   "clientId": "clientId",
   "message": {
      "message": "{\"foo\":\"bar\"}",
      "namespaceName": "urn:x-cast:com.google.foo",
      "sessionId": "sessionId"
   },
   "timeoutMillis": 0,
   "type": "app_message"
  })"));
}

TEST(CastInternalMessageUtilTest, CreateV2Message) {
  base::Value::Dict message_body;
  message_body.Set("foo", base::Value("bar"));

  auto message = CreateV2Message("client_id", message_body, 12345);
  EXPECT_THAT(message, IsPresentationConnectionMessage(R"({
   "clientId": "client_id",
   "message": {"foo": "bar"},
   "sequenceNumber": 12345,
   "timeoutMillis": 0,
   "type": "v2_message"
  })"));
}

TEST(CastInternalMessageUtilTest, SupportedMediaCommandsToListValue) {
  EXPECT_THAT(SupportedMediaCommandsToListValue(0), IsJson("[]"));
  EXPECT_THAT(SupportedMediaCommandsToListValue(1), IsJson("[\"pause\"]"));
  EXPECT_THAT(SupportedMediaCommandsToListValue(2), IsJson("[\"seek\"]"));
  EXPECT_THAT(SupportedMediaCommandsToListValue(4),
              IsJson("[\"stream_volume\"]"));
  EXPECT_THAT(SupportedMediaCommandsToListValue(8),
              IsJson("[\"stream_mute\"]"));
  EXPECT_THAT(
      SupportedMediaCommandsToListValue(15),
      IsJson("[\"pause\", \"seek\", \"stream_volume\", \"stream_mute\"]"));
}

}  // namespace media_router
