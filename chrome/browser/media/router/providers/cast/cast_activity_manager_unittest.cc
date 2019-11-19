// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/cast/cast_activity_manager.h"

#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "base/test/bind_test_util.h"
#include "base/test/mock_callback.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "chrome/browser/media/router/providers/cast/cast_session_client.h"
#include "chrome/browser/media/router/providers/cast/mock_activity_record.h"
#include "chrome/browser/media/router/providers/cast/test_util.h"
#include "chrome/browser/media/router/providers/common/buffered_message_sender.h"
#include "chrome/browser/media/router/test/mock_mojo_media_router.h"
#include "chrome/browser/media/router/test/test_helper.h"
#include "chrome/common/media_router/test/test_helper.h"
#include "components/cast_channel/cast_test_util.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::test::IsJson;
using base::test::ParseJson;
using testing::_;
using testing::AnyNumber;
using testing::ByRef;
using testing::ElementsAre;
using testing::IsEmpty;
using testing::Not;
using testing::Return;
using testing::WithArg;

namespace media_router {

namespace {
constexpr int kChannelId = 42;
constexpr char kOrigin[] = "https://google.com";
constexpr int kTabId = 1;
constexpr char kAppId1[] = "ABCDEFGH";
constexpr char kAppId2[] = "BBBBBBBB";

std::string MakeSourceId(const std::string& app_id = kAppId1) {
  return "cast:" + app_id + "?clientId=theClientId";
}

base::Value MakeReceiverStatus(const std::string& app_id,
                               bool update_display_name = false) {
  return ParseJson(R"({
        "applications": [{
          "appId": ")" +
                   app_id +
                   R"(",
          "displayName": "theDisplayName)" +
                   std::string(update_display_name ? "1" : "2") + R"(",
          "namespaces": [
            {"name": "urn:x-cast:com.google.cast.media"},
            {"name": "urn:x-cast:com.google.foo"},
          ],
          "sessionId": "theSessionId",
          "statusText": "theAppStatus",
          "transportId": "theTransportId",
        }],
      })");
}

using MockActivityRecordCallback =
    base::RepeatingCallback<void(MockActivityRecord*)>;

}  // namespace

// Test parameters are a boolean indicating whether the client connection should
// be closed by a leave_session message, and the URL used to create the test
// session.
class CastActivityManagerTest : public testing::Test,
                                public CastActivityRecordFactoryForTest {
 public:
  CastActivityManagerTest()
      : socket_service_(
            base::CreateSingleThreadTaskRunner({content::BrowserThread::UI})),
        message_handler_(&socket_service_) {
    media_sink_service_.AddOrUpdateSink(sink_);
    socket_.set_id(kChannelId);
  }

  ~CastActivityManagerTest() override = default;

  void SetUp() override {
    CastActivityManager::SetActitivyRecordFactoryForTest(this);

    router_receiver_ = std::make_unique<mojo::Receiver<mojom::MediaRouter>>(
        &mock_router_, router_remote_.BindNewPipeAndPassReceiver());

    session_tracker_.reset(
        new CastSessionTracker(&media_sink_service_, &message_handler_,
                               socket_service_.task_runner()));
    manager_ = std::make_unique<CastActivityManager>(
        &media_sink_service_, session_tracker_.get(), &message_handler_,
        router_remote_.get(),
        "theHashToken");

    RunUntilIdle();

    // Make sure we get route updates.
    manager_->AddRouteQuery(route_query_);
  }

  void TearDown() override {
    // This is a no-op for many tests, but it serves as a good sanity check in
    // any case.
    RunUntilIdle();

    manager_.reset();
    CastActivityManager::SetActitivyRecordFactoryForTest(nullptr);
  }

  std::unique_ptr<ActivityRecord> MakeCastActivityRecord(
      const MediaRoute& route,
      const std::string& app_id) override {
    auto activity = std::make_unique<MockActivityRecord>(route, app_id);
    auto* activity_ptr = activity.get();
    std::string route_id = route.media_route_id();
    ON_CALL(*activity, SendStopSessionMessageToReceiver)
        .WillByDefault(WithArg<2>([this, route_id](auto callback) {
          result_callback_ = manager_->MakeResultCallbackForRoute(
              route_id, std::move(callback));
        }));
    ON_CALL(*activity, SetOrUpdateSession)
        .WillByDefault(WithArg<0>([activity_ptr](const auto& session) {
          activity_ptr->set_session_id(session.session_id());
        }));
    activities_.push_back(activity_ptr);
    activity_record_callback_.Run(activity_ptr);
    return std::move(activity);
  }

  // Run any pending events and verify expectations associated with them.  This
  // method is sometimes called when there are clearly no pending events simply
  // to check expectations for code executed synchronously.
  void RunUntilIdle() {
    task_environment_.RunUntilIdle();
    testing::Mock::VerifyAndClearExpectations(&message_handler_);
    testing::Mock::VerifyAndClearExpectations(&mock_router_);
  }

  void ExpectLaunchSessionSuccess(
      const base::Optional<MediaRoute>& route,
      mojom::RoutePresentationConnectionPtr presentation_connections,
      const base::Optional<std::string>&,
      media_router::RouteRequestResult::ResultCode) {
    ASSERT_TRUE(route);
    route_ = std::make_unique<MediaRoute>(*route);
  }

  void CallLaunchSession(const std::string& source_id = MakeSourceId(kAppId1)) {
    // MediaRouter is notified of new route.
    ExpectSingleRouteUpdate();

    // A launch session request is sent to the sink.
    EXPECT_CALL(message_handler_,
                LaunchSession(kChannelId, "ABCDEFGH", kDefaultLaunchTimeout, _))
        .WillOnce(WithArg<3>([this](auto callback) {
          launch_session_callback_ = std::move(callback);
        }));

    auto source = CastMediaSource::FromMediaSourceId(source_id);
    ASSERT_TRUE(source);

    activity_record_callback_ =
        base::BindLambdaForTesting([this](MockActivityRecord* activity) {
          // TODO(jrw): Check parameters.
          EXPECT_CALL(*activity, AddClient);
          EXPECT_CALL(*activity, SendMessageToClient).RetiresOnSaturation();
          activity_record_callback_ = base::DoNothing();
        });

    // Callback will be invoked synchronously.
    manager_->LaunchSession(
        *source, sink_, "presentationId", origin_, kTabId,
        /*incognito*/ false,
        base::BindOnce(&CastActivityManagerTest::ExpectLaunchSessionSuccess,
                       base::Unretained(this)));

    RunUntilIdle();
  }

  cast_channel::LaunchSessionResponse GetSuccessLaunchResponse() {
    cast_channel::LaunchSessionResponse response;
    response.result = cast_channel::LaunchSessionResponse::Result::kOk;
    response.receiver_status = MakeReceiverStatus(kAppId1);
    return response;
  }

  void LaunchSession(const std::string& source_id = MakeSourceId(kAppId1)) {
    CallLaunchSession(source_id);

    // 3 things will happen:
    // (1) SDK client receives new_session message.
    // (2) Virtual connection is created.
    // (3) Route list will be updated.

    // TODO(jrw): Check more params.
    EXPECT_CALL(*activities_[0], SendMessageToClient("theClientId", _));
    EXPECT_CALL(*activities_[0], SetOrUpdateSession(_, sink_, _));

    EXPECT_CALL(message_handler_,
                EnsureConnection(kChannelId, "theClientId", "theTransportId"));

    auto response = GetSuccessLaunchResponse();
    session_tracker_->SetSessionForTest(
        route_->media_sink_id(),
        CastSession::From(sink_, *response.receiver_status));
    std::move(launch_session_callback_).Run(std::move(response));
    ExpectSingleRouteUpdate();
    RunUntilIdle();
  }

  void TerminateSession(bool expect_success) {
    EXPECT_CALL(*activities_[0], SendStopSessionMessageToReceiver);
    if (expect_success) {
      ExpectEmptyRouteUpdate();
    } else {
      ExpectNoRouteUpdate();
    }
    manager_->TerminateSession(route_->media_route_id(),
                               MakeTerminateRouteCallback(expect_success));
    std::move(result_callback_)
        .Run(expect_success ? cast_channel::Result::kOk
                            : cast_channel::Result::kFailed);
  }

  void TerminateNoSession() {
    // Stop session message not sent because session has not launched yet.
    EXPECT_CALL(*activities_[0], SendStopSessionMessageToReceiver).Times(0);
    ExpectNoRouteUpdate();
    manager_->TerminateSession(route_->media_route_id(),
                               MakeTerminateRouteCallback(true));
  }

  mojom::MediaRouteProvider::TerminateRouteCallback MakeTerminateRouteCallback(
      bool expect_success) {
    return base::BindLambdaForTesting(
        [expect_success](const base::Optional<std::string>& error_text,
                         RouteRequestResult::ResultCode result_code) {
          if (expect_success) {
            EXPECT_FALSE(error_text.has_value());
            EXPECT_EQ(RouteRequestResult::OK, result_code);
          } else {
            EXPECT_TRUE(error_text.has_value());
            EXPECT_NE(RouteRequestResult::OK, result_code);
          }
        });
  }

  // Expect a call to OnRoutesUpdated() with a single route, which will
  // optionally be saved in the variable pointed to by |route_ptr|.
  void ExpectSingleRouteUpdate() {
    updated_route_ = base::nullopt;
    EXPECT_CALL(mock_router_,
                OnRoutesUpdated(MediaRouteProviderId::CAST, ElementsAre(_),
                                route_query_, IsEmpty()))
        .WillOnce(WithArg<1>(
            [this](const auto& routes) { updated_route_ = routes[0]; }));
  }

  // Expect a call to OnRoutesUpdated() with no routes.
  void ExpectEmptyRouteUpdate() {
    updated_route_ = base::nullopt;
    EXPECT_CALL(mock_router_,
                OnRoutesUpdated(MediaRouteProviderId::CAST, IsEmpty(),
                                route_query_, IsEmpty()))
        .Times(1);
  }

  // Expect that OnRoutesUpdated() will not be called.
  void ExpectNoRouteUpdate() {
    updated_route_ = base::nullopt;
    EXPECT_CALL(mock_router_, OnRoutesUpdated).Times(0);
  }

  std::unique_ptr<CastSession> MakeSession(const std::string& app_id,
                                           bool update_display_name = false) {
    return CastSession::From(sink_,
                             MakeReceiverStatus(app_id, update_display_name));
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  MockMojoMediaRouter mock_router_;
  mojo::Remote<mojom::MediaRouter> router_remote_;
  std::unique_ptr<mojo::Receiver<mojom::MediaRouter>> router_receiver_;
  cast_channel::MockCastSocketService socket_service_;
  cast_channel::MockCastSocket socket_;
  cast_channel::MockCastMessageHandler message_handler_;
  MediaSinkInternal sink_ = CreateCastSink(kChannelId);
  std::unique_ptr<MediaRoute> route_;  // TODO(jrw): Is this needed?
  cast_channel::LaunchSessionCallback launch_session_callback_;
  TestMediaSinkService media_sink_service_;
  MockCastAppDiscoveryService app_discovery_service_;
  std::unique_ptr<CastActivityManager> manager_;
  std::unique_ptr<CastSessionTracker> session_tracker_;
  std::vector<MockActivityRecord*> activities_;
  MockActivityRecordCallback activity_record_callback_ = base::DoNothing();
  const url::Origin origin_ = url::Origin::Create(GURL(kOrigin));
  const MediaSource::Id route_query_ = "theRouteQuery";
  base::Optional<MediaRoute> updated_route_;
  cast_channel::ResultCallback result_callback_;
};

TEST_F(CastActivityManagerTest, LaunchSession) {
  LaunchSession();
}

TEST_F(CastActivityManagerTest, LaunchSessionFails) {
  // 3 things will happen:
  // (1) Route is removed
  // (2) Issue will be sent.
  // (3) The PresentationConnection associated with the route will be closed
  //     with error.

  CallLaunchSession();

  EXPECT_CALL(
      *activities_[0],
      ClosePresentationConnections(
          blink::mojom::PresentationConnectionCloseReason::CONNECTION_ERROR));

  cast_channel::LaunchSessionResponse response;
  response.result = cast_channel::LaunchSessionResponse::Result::kError;
  std::move(launch_session_callback_).Run(std::move(response));

  EXPECT_CALL(mock_router_, OnIssue);
  ExpectEmptyRouteUpdate();
  RunUntilIdle();
}

TEST_F(CastActivityManagerTest, LaunchSessionTerminatesExistingSessionOnSink) {
  LaunchSession();

  EXPECT_CALL(*activities_[0], SendStopSessionMessageToReceiver);

  {
    testing::InSequence dummy;

    // Existing route is terminated before new route is created.
    // MediaRouter is notified of terminated route.
    ExpectEmptyRouteUpdate();

    // After existing route is terminated, new route is created.
    // MediaRouter is notified of new route.
    ExpectSingleRouteUpdate();
  }

  // Launch a new session on the same sink.
  auto source = CastMediaSource::FromMediaSourceId(MakeSourceId(kAppId2));
  manager_->LaunchSession(
      // TODO(jrw): Verify that presentation ID is used correctly.
      *source, sink_, "presentationId2", origin_, kTabId, /*incognito*/
      false,
      base::BindOnce(&CastActivityManagerTest::ExpectLaunchSessionSuccess,
                     base::Unretained(this)));

  EXPECT_CALL(message_handler_,
              LaunchSession(kChannelId, "BBBBBBBB", kDefaultLaunchTimeout, _));
  std::move(result_callback_).Run(cast_channel::Result::kOk);
}

TEST_F(CastActivityManagerTest, AddRemoveNonLocalActivity) {
  auto session = MakeSession(kAppId1);
  ExpectSingleRouteUpdate();
  manager_->OnSessionAddedOrUpdated(sink_, *session);
  RunUntilIdle();
  ASSERT_TRUE(updated_route_);
  EXPECT_FALSE(updated_route_->is_local());

  ExpectEmptyRouteUpdate();
  manager_->OnSessionRemoved(sink_);
}

TEST_F(CastActivityManagerTest, UpdateNewlyCreatedSession) {
  LaunchSession();

  EXPECT_CALL(*activities_[0], SetOrUpdateSession(_, sink_, _));
  auto session = MakeSession(kAppId1);
  ExpectSingleRouteUpdate();
  manager_->OnSessionAddedOrUpdated(sink_, *session);
  RunUntilIdle();
  ASSERT_TRUE(updated_route_);
  EXPECT_TRUE(updated_route_->is_local());
}

TEST_F(CastActivityManagerTest, OnSessionAddedOrUpdated) {
  LaunchSession();
  auto session = MakeSession(kAppId1);
  ExpectSingleRouteUpdate();
  EXPECT_CALL(*activities_[0], SetOrUpdateSession(_, _, "theHashToken"));
  manager_->OnSessionAddedOrUpdated(sink_, *session);
}

TEST_F(CastActivityManagerTest, TerminateSession) {
  LaunchSession();
  TerminateSession(true);
}

TEST_F(CastActivityManagerTest, TerminateSessionFails) {
  LaunchSession();
  TerminateSession(false);
}

TEST_F(CastActivityManagerTest, TerminateSessionBeforeLaunchResponse) {
  CallLaunchSession();
  TerminateNoSession();
  ExpectEmptyRouteUpdate();
  std::move(launch_session_callback_).Run(GetSuccessLaunchResponse());
}

TEST_F(CastActivityManagerTest, AppMessageFromReceiver) {
  LaunchSession();

  // Destination ID matches client ID.
  cast_channel::CastMessage message = cast_channel::CreateCastMessage(
      "urn:x-cast:com.google.foo", base::Value(base::Value::Type::DICTIONARY),
      "sourceId", "theClientId");

  EXPECT_CALL(*activities_[0], OnAppMessage(IsCastChannelMessage(message)));
  manager_->OnAppMessage(kChannelId, message);
}

TEST_F(CastActivityManagerTest, OnMediaStatusUpdated) {
  LaunchSession();

  const char status[] = R"({"foo": "bar"})";
  base::Optional<int> request_id(345);

  EXPECT_CALL(*activities_[0],
              SendMediaStatusToClients(IsJson(status), request_id));
  manager_->OnMediaStatusUpdated(sink_, ParseJson(status), request_id);
}

}  // namespace media_router
