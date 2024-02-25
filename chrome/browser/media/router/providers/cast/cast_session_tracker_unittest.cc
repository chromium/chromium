// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/cast/cast_session_tracker.h"

#include "base/memory/raw_ptr.h"
#include "base/test/values_test_util.h"
#include "chrome/browser/media/router/test/provider_test_helpers.h"
#include "components/media_router/common/providers/cast/channel/cast_message_util.h"
#include "components/media_router/common/providers/cast/channel/cast_test_util.h"
#include "components/media_router/common/test/test_helper.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::test::IsJson;
using base::test::ParseJsonDict;
using cast_channel::kMediaNamespace;
using cast_channel::kReceiverNamespace;
using testing::_;
using testing::ByRef;
using testing::Eq;
using testing::NiceMock;

namespace media_router {

namespace {

constexpr char kDestinationId[] = "destination_id";
constexpr char kSourceId[] = "source_id";

constexpr char kReceiverStatus[] = R"({
    "status": {
        "applications": [{
          "appId": "ABCDEFGH",
          "displayName": "App display name",
          "namespaces": [
            {"name": "urn:x-cast:com.google.cast.media"},
            {"name": "urn:x-cast:com.google.foo"}
          ],
          "sessionId": "theSessionId",
          "statusText":"App status",
          "transportId":"theTransportId"
        }]
  }
})";

// Receiver status for the backdrop (idle) app.
constexpr char kIdleReceiverStatus[] = R"({
    "status": {
        "applications": [{
          "appId": "E8C28D3C",
          "displayName": "Backdrop",
          "namespaces": [
            {"name": "urn:x-cast:com.google.cast.media"},
            {"name": "urn:x-cast:com.google.foo"}
          ],
          "sessionId": "theSessionId",
          "statusText":"App status",
          "transportId":"theTransportId"
        }]
  }
})";

}  // namespace

class MockCastSessionObserver : public CastSessionTracker::Observer {
 public:
  MockCastSessionObserver() = default;
  ~MockCastSessionObserver() override = default;

  MOCK_METHOD(void,
              OnSessionAddedOrUpdated,
              (const MediaSinkInternal& sink, const CastSession& session));
  MOCK_METHOD(void, OnSessionRemoved, (const MediaSinkInternal& sink));
  MOCK_METHOD(void,
              OnMediaStatusUpdated,
              (const MediaSinkInternal& sink,
               const base::Value::Dict& media_status,
               std::optional<int> request_id));
};

class CastSessionTrackerTest : public testing::Test {
 public:
  CastSessionTrackerTest()
      : socket_service_(content::GetUIThreadTaskRunner({})),
        message_handler_(&socket_service_),
        session_tracker_(&media_sink_service_,
                         &message_handler_,
                         socket_service_.task_runner()) {
    base::RunLoop().RunUntilIdle();
  }

  void SetUp() override { session_tracker_.AddObserver(&observer_); }

  void TearDown() override { session_tracker_.RemoveObserver(&observer_); }

  void AddSinkAndSendReceiverStatusResponse() {
    EXPECT_CALL(message_handler_,
                RequestReceiverStatus(sink_.cast_data().cast_channel_id));
    media_sink_service_.AddOrUpdateSink(sink_);

    EXPECT_CALL(observer_, OnSessionAddedOrUpdated(sink_, _));
    session_tracker_.OnInternalMessage(
        sink_.cast_data().cast_channel_id,
        cast_channel::InternalMessage(
            cast_channel::CastMessageType::kReceiverStatus, kSourceId,
            kDestinationId, kReceiverNamespace,
            ParseJsonDict(kReceiverStatus)));

    session_ = session_tracker_.GetSessions().begin()->second.get();
    ASSERT_TRUE(session_);
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;

  cast_channel::MockCastSocketService socket_service_;
  NiceMock<cast_channel::MockCastMessageHandler> message_handler_;

  TestMediaSinkService media_sink_service_;
  CastSessionTracker session_tracker_;

  NiceMock<MockCastSessionObserver> observer_;

  MediaSinkInternal sink_ = CreateCastSink(1);
  raw_ptr<CastSession, DanglingUntriaged> session_;
};

TEST_F(CastSessionTrackerTest, QueryReceiverOnSinkAdded) {
  AddSinkAndSendReceiverStatusResponse();

  // Receiver status is sent again when sinks is updated.
  sink_.cast_data().cast_channel_id = 2;
  EXPECT_CALL(message_handler_,
              RequestReceiverStatus(sink_.cast_data().cast_channel_id));
  media_sink_service_.AddOrUpdateSink(sink_);
}

TEST_F(CastSessionTrackerTest, RemoveSessionOnSinkRemoved) {
  AddSinkAndSendReceiverStatusResponse();

  EXPECT_CALL(observer_, OnSessionRemoved(sink_));
  media_sink_service_.RemoveSink(sink_);
}

TEST_F(CastSessionTrackerTest, RemoveSession) {
  AddSinkAndSendReceiverStatusResponse();

  EXPECT_CALL(observer_, OnSessionRemoved(sink_));
  session_tracker_.OnInternalMessage(
      sink_.cast_data().cast_channel_id,
      cast_channel::InternalMessage(
          cast_channel::CastMessageType::kReceiverStatus, kSourceId,
          kDestinationId, kReceiverNamespace,
          ParseJsonDict(kIdleReceiverStatus)));
}

TEST_F(CastSessionTrackerTest, GetSessions) {
  EXPECT_TRUE(session_tracker_.GetSessions().empty());

  AddSinkAndSendReceiverStatusResponse();

  const auto& sessions = session_tracker_.GetSessions();
  EXPECT_EQ(1u, sessions.size());
  auto it = sessions.find(sink_.sink().id());
  ASSERT_TRUE(it != sessions.end());
  EXPECT_EQ("theSessionId", it->second->session_id());

  EXPECT_TRUE(session_tracker_.GetSessionById("theSessionId"));
}

TEST_F(CastSessionTrackerTest, HandleMediaStatusMessageBasic) {
  AddSinkAndSendReceiverStatusResponse();

  // Expect that:
  // - The session ID is copied into the output message and all values in in the
  //   'status' list.
  //
  // - A request ID is not required.
  //
  // - A 'supportedMediaCommands' field whose value is zero in the 'status'
  //   objects is converted to an empty list.
  //
  EXPECT_CALL(observer_, OnMediaStatusUpdated(sink_, IsJson(R"({
    "sessionId": "theSessionId",
    "status": [{
        "playerState": "anything but IDLE",
        "sessionId": "theSessionId",
        "supportedMediaCommands": [],
      },
      {
        "playerState": "IDLE",
        "sessionId": "theSessionId"
      }
      ]})"),
                                              std::optional<int>()));

  // This should call session_tracker_.HandleMediaStatusMessage(...).
  session_tracker_.OnInternalMessage(
      sink_.cast_data().cast_channel_id,
      cast_channel::InternalMessage(cast_channel::CastMessageType::kMediaStatus,
                                    kSourceId, kDestinationId, kMediaNamespace,
                                    ParseJsonDict(R"({
    "status": [{
        "playerState": "anything but IDLE",
        "supportedMediaCommands": 0,
      }, {
        "playerState": "IDLE",
      },
      "not a dict; should be removed"
    ],
  })")));

  // Check that the stored media value is the same as the 'status' field in the
  // outgoing message.
  EXPECT_THAT(*session_->value().Find("media"), IsJson(R"([{
    "playerState": "anything but IDLE",
    "sessionId": "theSessionId",
    "supportedMediaCommands": [],
  },
  {
   "playerState": "IDLE",
   "sessionId": "theSessionId"
  }])"));
}

TEST_F(CastSessionTrackerTest, HandleMediaStatusMessageFancy) {
  AddSinkAndSendReceiverStatusResponse();

  // Expect that:
  //
  // - The session ID is copied into the output message and all values in in the
  //   'status' list.
  //
  // - The request ID is copied into the output message and passed as a separate
  //   parameters to OnMediaStatusUpdated().
  //
  // - A nonzero numeric 'supportedMediaCommands' field in the 'status' objects
  //   is converted to a non-empty list.
  //
  // - Extra fields are preserved in the message and the status objects.
  //
  EXPECT_CALL(observer_, OnMediaStatusUpdated(sink_, IsJson(R"({
    "requestId": 12345,
    "sessionId": "theSessionId",
    "status": [{
        "playerState": "anything but IDLE",
        "sessionId": "theSessionId",
        "supportedMediaCommands": ["pause"],
        "xyzzy": "xyzzyValue1",
      },
      {
        "playerState": "IDLE",
        "sessionId": "theSessionId"
      }],
    "xyzzy": "xyzzyValue2",
  })"),
                                              std::make_optional(12345)));

  // This should call session_tracker_.HandleMediaStatusMessage(...).
  session_tracker_.OnInternalMessage(
      sink_.cast_data().cast_channel_id,
      cast_channel::InternalMessage(cast_channel::CastMessageType::kMediaStatus,
                                    kSourceId, kDestinationId, kMediaNamespace,
                                    ParseJsonDict(R"({
    "requestId": 12345,
    "status": [{
        "playerState": "anything but IDLE",
        "supportedMediaCommands": 1,
        "xyzzy": "xyzzyValue1",
      }, {
        "playerState": "IDLE",
      },
    ],
    "xyzzy": "xyzzyValue2",
  })")));

  // Check that the stored media value is the same as the 'status' field in the
  // outgoing message.
  EXPECT_THAT(*session_->value().Find("media"), IsJson(R"([{
    "playerState": "anything but IDLE",
    "sessionId": "theSessionId",
    "supportedMediaCommands": ["pause"],
    "xyzzy": "xyzzyValue1",
  },
  {
    "playerState": "IDLE",
    "sessionId": "theSessionId"
  }])"));
}

TEST_F(CastSessionTrackerTest, CopySavedMediaFieldsToMediaList) {
  AddSinkAndSendReceiverStatusResponse();

  // Add media status information to the session with mediaSessionId = 345.
  EXPECT_CALL(observer_, OnMediaStatusUpdated(sink_, _, _));
  session_tracker_.OnInternalMessage(
      sink_.cast_data().cast_channel_id,
      cast_channel::InternalMessage(cast_channel::CastMessageType::kMediaStatus,
                                    kSourceId, kDestinationId, kMediaNamespace,
                                    ParseJsonDict(R"({
    "status": [{
        "media": "theMedia",
        "mediaSessionId": 345,
        "playerState": "anything but IDLE",
        "supportedMediaCommands": 0,
        "xyzzy": "xyzzy1",
      },
    ],
  })")));

  // Check that the stored media value is what we expected.
  ASSERT_THAT(*session_->value().Find("media"), IsJson(R"([{
    "mediaSessionId": 345,
    "media": "theMedia",
    "playerState": "anything but IDLE",
    "sessionId": "theSessionId",
    "supportedMediaCommands": [],
    "xyzzy": "xyzzy1",
  }])"));

  // Not strictly needed, but makes this test easier to debug.
  testing::Mock::VerifyAndClear(&observer_);

  // Expect the outgoing status message to have a 'media' field filled in from
  // the previously stored value.
  EXPECT_CALL(observer_, OnMediaStatusUpdated(sink_, IsJson(R"({
    "sessionId": "theSessionId",
    "status": [{
        "media": "theMedia",
        "mediaSessionId": 345,
        "playerState": "anything but IDLE",
        "sessionId": "theSessionId",
        "supportedMediaCommands": [],
        "xyzzy": "xyzzy2",
      },
    ],
  })"),
                                              _));

  // Receive a message referring to the previously stored mediaSessionId with a
  // missing 'media' field.  This tests the logic in the
  // CopySavedMediaFieldsToMediaList() method.
  session_tracker_.OnInternalMessage(
      sink_.cast_data().cast_channel_id,
      cast_channel::InternalMessage(cast_channel::CastMessageType::kMediaStatus,
                                    kSourceId, kDestinationId, kMediaNamespace,
                                    ParseJsonDict(R"({
    "status": [{
        "mediaSessionId": 345,
        "playerState": "anything but IDLE",
        "supportedMediaCommands": 0,
        "xyzzy": "xyzzy2",
      },
    ],
  })")));

  // Check that the stored media value is the same as the 'status' field in the
  // outgoing message.
  EXPECT_THAT(*session_->value().Find("media"), IsJson(R"([{
    "media": "theMedia",
    "mediaSessionId": 345,
    "playerState": "anything but IDLE",
    "sessionId": "theSessionId",
    "supportedMediaCommands": [],
    "xyzzy": "xyzzy2",
  }])"));
}

TEST_F(CastSessionTrackerTest, DoNotCopySavedMediaFieldsWhenFieldPresent) {
  AddSinkAndSendReceiverStatusResponse();

  // Add media status information to the session with mediaSessionId = 345.
  session_tracker_.OnInternalMessage(
      sink_.cast_data().cast_channel_id,
      cast_channel::InternalMessage(cast_channel::CastMessageType::kMediaStatus,
                                    kSourceId, kDestinationId, kMediaNamespace,
                                    ParseJsonDict(R"({
    "status": [{
        "media": "oldMedia",
        "mediaSessionId": 345,
        "playerState": "anything but IDLE",
        "supportedMediaCommands": 0,
        "xyzzy": "xyzzy1",
      },
    ],
  })")));

  // Receive a message referring to the previously stored mediaSessionId with a
  // new 'media' value.
  session_tracker_.OnInternalMessage(
      sink_.cast_data().cast_channel_id,
      cast_channel::InternalMessage(cast_channel::CastMessageType::kMediaStatus,
                                    kSourceId, kDestinationId, kMediaNamespace,
                                    ParseJsonDict(R"({
    "status": [{
        "media": "newMedia",
        "mediaSessionId": 345,
        "playerState": "anything but IDLE",
        "supportedMediaCommands": 0,
        "xyzzy": "xyzzy2",
      },
    ],
  })")));

  // Check that 'media' field has the new value rather than the cached value.
  EXPECT_THAT(*session_->value().Find("media"), IsJson(R"([{
    "media": "newMedia",
    "mediaSessionId": 345,
    "playerState": "anything but IDLE",
    "sessionId": "theSessionId",
    "supportedMediaCommands": [],
    "xyzzy": "xyzzy2",
  }])"));
}

}  // namespace media_router
