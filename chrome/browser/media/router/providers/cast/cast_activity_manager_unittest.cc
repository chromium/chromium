// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/cast/cast_activity_manager.h"

#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/media/router/providers/cast/cast_session_client.h"
#include "chrome/browser/media/router/providers/cast/mirroring_activity.h"
#include "chrome/browser/media/router/providers/cast/mock_app_activity.h"
#include "chrome/browser/media/router/providers/cast/mock_mirroring_activity.h"
#include "chrome/browser/media/router/providers/cast/test_util.h"
#include "chrome/browser/media/router/test/mock_mojo_media_router.h"
#include "chrome/browser/media/router/test/provider_test_helpers.h"
#include "components/media_router/common/media_source.h"
#include "components/media_router/common/providers/cast/channel/cast_message_util.h"
#include "components/media_router/common/providers/cast/channel/cast_test_util.h"
#include "components/media_router/common/test/mock_logger.h"
#include "components/media_router/common/test/test_helper.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/openscreen/src/cast/common/public/cast_streaming_app_ids.h"

using base::test::IsJson;
using base::test::ParseJsonDict;
using testing::_;
using testing::AnyNumber;
using testing::ByRef;
using testing::ElementsAre;
using testing::Invoke;
using testing::IsEmpty;
using testing::NiceMock;
using testing::Not;
using testing::Return;
using testing::WithArg;

namespace media_router {

namespace {
constexpr int kChannelId = 42;
constexpr int kChannelId2 = 43;
constexpr char kClientId[] = "theClientId";
constexpr char kOrigin[] = "https://google.com";
constexpr content::FrameTreeNodeId kFrameTreeNodeId =
    content::FrameTreeNodeId(123);
constexpr content::FrameTreeNodeId kFrameTreeNodeId2 =
    content::FrameTreeNodeId(234);
constexpr char kAppId1[] = "ABCDEFGH";
constexpr char kAppId2[] = "BBBBBBBB";
constexpr char kAppParams[] = R"(
{
  "requiredFeatures" : ["STREAM_TRANSFER"],
  "launchCheckerParams" : {
    "credentialsData" : {
      "credentialsType" : "mobile",
      "credentials" : "99843n2idsguyhga"
    }
  }
}
)";
constexpr char kPresentationId[] = "presentationId";
constexpr char kPresentationId2[] = "presentationId2";
constexpr char histogram[] =
    "AccessCodeCast.Session.SavedDeviceRouteCreationDuration";

std::string MakeSourceId(const std::string& app_id = kAppId1,
                         const std::string& app_params = "",
                         const std::string& client_id = kClientId) {
  return base::StrCat(
      {"cast:", app_id, "?clientId=", client_id, "&appParams=", app_params});
}

base::Value::Dict MakeReceiverStatus(const std::string& app_id,
                                     bool update_display_name = false) {
  return ParseJsonDict(R"({
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

class MockLaunchSessionCallback {
 public:
  MOCK_METHOD(void,
              Run,
              (const std::optional<MediaRoute>& route,
               mojom::RoutePresentationConnectionPtr presentation_connections,
               const std::optional<std::string>& error_message,
               media_router::mojom::RouteRequestResultCode result_code));
};

using MockAppActivityCallback = base::RepeatingCallback<void(MockAppActivity*)>;
using MockMirroringActivityCallback =
    base::RepeatingCallback<void(MockMirroringActivity*)>;
}  // namespace

// Test parameters are a boolean indicating whether the client connection should
// be closed by a leave_session message, and the URL used to create the test
// session.
class CastActivityManagerTest : public testing::Test,
                                public CastActivityFactoryForTest {
 public:
  CastActivityManagerTest()
      : socket_service_(content::GetUIThreadTaskRunner({})),
        message_handler_(&socket_service_),
        cast_streaming_app_id_(
            openscreen::cast::GetCastStreamingAudioVideoAppId()) {
    media_sink_service_.AddOrUpdateSink(sink_);
    socket_.set_id(kChannelId);
  }

  ~CastActivityManagerTest() override = default;

  void SetUp() override {
    CastActivityManager::SetActitityFactoryForTest(this);

    router_receiver_ = std::make_unique<mojo::Receiver<mojom::MediaRouter>>(
        &mock_router_, router_remote_.BindNewPipeAndPassReceiver());

    session_tracker_.reset(
        new CastSessionTracker(&media_sink_service_, &message_handler_,
                               socket_service_.task_runner()));
    manager_ = std::make_unique<CastActivityManager>(
        &media_sink_service_, session_tracker_.get(), &message_handler_,
        router_remote_.get(), &logger_, "theHashToken");

    ON_CALL(message_handler_, StopSession)
        .WillByDefault(WithArg<3>([this](auto callback) {
          std::move(callback).Run(stop_session_callback_arg_);
        }));

    RunUntilIdle();
  }

  void TearDown() override {
    // This is a no-op for many tests, but it serves as a good sanity check in
    // any case.
    RunUntilIdle();

    manager_.reset();
    CastActivityManager::SetActitityFactoryForTest(nullptr);
  }

  // from CastActivityFactoryForTest
  std::unique_ptr<AppActivity> MakeAppActivity(
      const MediaRoute& route,
      const std::string& app_id) override {
    auto activity = std::make_unique<NiceMock<MockAppActivity>>(route, app_id);
    app_activity_ = activity.get();
    app_activity_callback_.Run(activity.get());
    return activity;
  }

  // from CastActivityFactoryForTest
  std::unique_ptr<MirroringActivity> MakeMirroringActivity(
      const MediaRoute& route,
      const std::string& app_id,
      MirroringActivity::OnStopCallback on_stop,
      OnSourceChangedCallback on_source_changed) override {
    auto activity = std::make_unique<NiceMock<MockMirroringActivity>>(
        route, app_id, std::move(on_stop), std::move(on_source_changed));
    mirroring_activity_ = activity.get();
    mirroring_activity_callback_.Run(activity.get());
    return activity;
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
      const std::optional<MediaRoute>& route,
      mojom::RoutePresentationConnectionPtr presentation_connections,
      const std::optional<std::string>&,
      media_router::mojom::RouteRequestResultCode) {
    ASSERT_TRUE(route);
    route_ = std::make_unique<MediaRoute>(*route);
    presentation_connections_ = std::move(presentation_connections);
  }

  void ExpectLaunchSessionFailure(
      const std::optional<MediaRoute>& route,
      mojom::RoutePresentationConnectionPtr presentation_connections,
      const std::optional<std::string>& error_message,
      media_router::mojom::RouteRequestResultCode result_code) {
    ASSERT_FALSE(route);
    LaunchSessionFailed();
  }

  MOCK_METHOD(void, LaunchSessionFailed, ());

  void CallLaunchSessionCommon(
      const std::string& app_id,
      const std::string& app_params,
      const std::string& client_id,
      mojom::MediaRouteProvider::CreateRouteCallback callback) {
    ExpectSingleRouteUpdate();
    // A launch session request is sent to the sink.
    const std::optional<base::Value> json = base::JSONReader::Read(app_params);
    EXPECT_CALL(message_handler_,
                LaunchSession(kChannelId, app_id, kDefaultLaunchTimeout,
                              testing::ElementsAre("WEB"),
                              testing::Eq(testing::ByRef(json)), _))
        .WillOnce(WithArg<5>([this](auto callback) {
          launch_session_callback_ = std::move(callback);
        }));

    auto source = CastMediaSource::FromMediaSourceId(
        MakeSourceId(app_id, app_params, client_id));
    ASSERT_TRUE(source);

    // Callback needs to be invoked by running |launch_session_callback_|.
    manager_->LaunchSession(*source, sink_, kPresentationId, origin_,
                            kFrameTreeNodeId, std::move(callback));

    RunUntilIdle();
  }

  void CallLaunchSessionSuccess(const std::string& app_id = kAppId1,
                                const std::string& app_params = "",
                                const std::string& client_id = kClientId) {
    app_activity_callback_ =
        base::BindLambdaForTesting([this](MockAppActivity* activity) {
          // TODO(crbug.com/1291744): Check parameters.
          EXPECT_CALL(*activity, AddClient);
          EXPECT_CALL(*activity, SendMessageToClient).Times(2);
          EXPECT_CALL(*activity, OnSessionSet).WillOnce([this]() {
            EXPECT_EQ(sink_, app_activity_->sink());
          });
          app_activity_callback_ = base::DoNothing();
        });

    CallLaunchSessionCommon(
        app_id, app_params, client_id,
        base::BindOnce(&CastActivityManagerTest::ExpectLaunchSessionSuccess,
                       base::Unretained(this)));
  }

  void CallLaunchSessionFailure(const std::string& app_id = kAppId1,
                                const std::string& app_params = "",
                                const std::string& client_id = kClientId) {
    CallLaunchSessionCommon(
        app_id, app_params, client_id,
        base::BindOnce(&CastActivityManagerTest::ExpectLaunchSessionFailure,
                       base::Unretained(this)));
  }

  void ReceiveLaunchSuccessResponseFromReceiver(
      const std::string& app_id = kAppId1) {
    cast_channel::LaunchSessionResponse response;
    response.result = cast_channel::LaunchSessionResponse::Result::kOk;
    response.receiver_status = MakeReceiverStatus(app_id);

    SetSessionForTest(sink_.id(),
                      CastSession::From(sink_, *response.receiver_status));

    std::move(launch_session_callback_).Run(std::move(response), nullptr);
    RunUntilIdle();
  }

  void LaunchAppSession(const std::string& app_id = kAppId1,
                        const std::string& app_params = "",
                        const std::string& client_id = kClientId) {
    CallLaunchSessionSuccess(app_id, app_params, client_id);

    // 3 things will happen after a launch session response is received:
    // (1) SDK client receives "receiver_action" and "new_session" message.
    // (2) Virtual connection is created.
    // (3) Route list will be updated.

    EXPECT_CALL(message_handler_,
                EnsureConnection(kChannelId, client_id, "theTransportId",
                                 cast_channel::VirtualConnectionType::kStrong));
    EXPECT_CALL(message_handler_,
                SendMediaRequest(kChannelId,
                                 // NOTE: MEDIA_GET_STATUS is translated to
                                 // GET_STATUS inside SendMediaRequest.
                                 IsJson(R"({"type": "MEDIA_GET_STATUS"})"),
                                 client_id, "theTransportId"));

    // MediaRouter is notified of new route.
    ExpectSingleRouteUpdate();

    ReceiveLaunchSuccessResponseFromReceiver(app_id);
  }

  void ExpectAppActivityStoppedTimes(int times) {
    EXPECT_CALL(*app_activity_, SendStopSessionMessageToClients).Times(times);

    if (times == 0) {
      EXPECT_CALL(message_handler_, StopSession).Times(0);
    } else {
      // TODO(crbug.com/1291744): Check other parameters
      EXPECT_CALL(message_handler_, StopSession(kChannelId, _, _, _))
          .Times(times);
    }
  }

  void LaunchNonSdkMirroringSession() {
    mirroring_activity_callback_ =
        base::BindLambdaForTesting([this](MockMirroringActivity* activity) {
          EXPECT_CALL(*activity, OnSessionSet).WillOnce([this]() {
            EXPECT_EQ(sink_, mirroring_activity_->sink());
          });
          mirroring_activity_callback_ = base::DoNothing();
        });

    CallLaunchSessionSuccess(cast_streaming_app_id_, /* app_params */ "",
                             /* client_id */ "");

    ResolveMirroringSessionLaunch();
  }

  void LaunchCastSdkMirroringSession() {
    CallLaunchSessionSuccess(cast_streaming_app_id_, kAppParams, kClientId);
    mirroring_activity_callback_ =
        base::BindLambdaForTesting([this](MockMirroringActivity* activity) {
          EXPECT_CALL(*activity, OnSessionSet).WillOnce([this]() {
            EXPECT_EQ(sink_, mirroring_activity_->sink());
          });
          mirroring_activity_callback_ = base::DoNothing();
        });
    // We expect EnsureConnection() to be called for both the sender client and
    // |message_handler_.source_id()|. The latter is captured in
    // ResolveMirroringSessionLaunch().
    //
    // CallLaunchSessionSuccess() calls VerifyAndClearExpectations() on
    // |message_handler_|, so this EXPECT_CALL() must come after that.
    EXPECT_CALL(message_handler_,
                EnsureConnection(kChannelId, kClientId, "theTransportId",
                                 cast_channel::VirtualConnectionType::kStrong));
    ResolveMirroringSessionLaunch();
  }

  void ResolveMirroringSessionLaunch() {
    // MediaRouter is notified of new route.
    ExpectSingleRouteUpdate();
    EXPECT_CALL(message_handler_,
                EnsureConnection(kChannelId, message_handler_.source_id(),
                                 "theTransportId",
                                 cast_channel::VirtualConnectionType::kStrong));

    ReceiveLaunchSuccessResponseFromReceiver();
    DCHECK(mirroring_activity_);
  }

  void AddRemoteMirroringSession() {
    auto session =
        CastSession::From(sink2_, MakeReceiverStatus(cast_streaming_app_id_));
    manager_->OnSessionAddedOrUpdated(sink2_, *session);
    SetSessionForTest(sink2_.id(), std::move(session));
    DCHECK(mirroring_activity_);
    DCHECK(!mirroring_activity_->route().is_local());
  }

  void ExpectMirroringActivityStoppedTimes(int times) {
    DCHECK(mirroring_activity_);
    EXPECT_CALL(message_handler_, StopSession).Times(times);
    EXPECT_CALL(*mirroring_activity_, SendStopSessionMessageToClients)
        .Times(times);
  }

  void TerminateSession(bool expect_success, const MediaRoute::Id& route_id) {
    stop_session_callback_arg_ = expect_success ? cast_channel::Result::kOk
                                                : cast_channel::Result::kFailed;
    manager_->TerminateSession(route_id,
                               MakeTerminateRouteCallback(expect_success));
  }

  void TerminateSession(bool expect_success) {
    TerminateSession(expect_success, route_->media_route_id());
  }

  mojom::MediaRouteProvider::TerminateRouteCallback MakeTerminateRouteCallback(
      bool expect_success) {
    return base::BindLambdaForTesting(
        [expect_success](const std::optional<std::string>& error_text,
                         mojom::RouteRequestResultCode result_code) {
          if (expect_success) {
            EXPECT_FALSE(error_text.has_value());
            EXPECT_EQ(mojom::RouteRequestResultCode::OK, result_code);
          } else {
            EXPECT_TRUE(error_text.has_value());
            EXPECT_NE(mojom::RouteRequestResultCode::OK, result_code);
          }
        });
  }

  // Expect a call to OnRoutesUpdated() with a single route, which will be saved
  // in |updated_route_|.
  void ExpectSingleRouteUpdate() {
    updated_route_ = std::nullopt;
    EXPECT_CALL(mock_router_, OnRoutesUpdated(mojom::MediaRouteProviderId::CAST,
                                              ElementsAre(_)))
        .WillOnce(WithArg<1>(
            [this](const auto& routes) { updated_route_ = routes[0]; }));
  }

  // Expect a call to OnRoutesUpdated() with no routes.
  void ExpectEmptyRouteUpdate() {
    updated_route_ = std::nullopt;
    EXPECT_CALL(mock_router_,
                OnRoutesUpdated(mojom::MediaRouteProviderId::CAST, IsEmpty()))
        .Times(1);
  }

  // Expect that OnRoutesUpdated() will not be called.
  void ExpectNoRouteUpdate() {
    updated_route_ = std::nullopt;
    EXPECT_CALL(mock_router_, OnRoutesUpdated).Times(0);
  }

  std::unique_ptr<CastSession> MakeSession(const std::string& app_id,
                                           bool update_display_name = false) {
    return CastSession::From(sink_,
                             MakeReceiverStatus(app_id, update_display_name));
  }

  // Needed because CastSessionTracker::SetSessionForTest is private.
  void SetSessionForTest(const MediaSink::Id& sink_id,
                         std::unique_ptr<CastSession> session) {
    session_tracker_->SetSessionForTest(sink_id, std::move(session));
  }

  // This method starts out with `route_id_to_move` associated with
  // `frame_id_before`, and `route_id_to_terminate` with `frame_id_after`. We
  // move `route_id_to_move` to `frame_id_after`, and that results in the
  // termination of `route_id_to_terminate`.
  void UpdateRouteSourceTabInRoutesMap(
      content::FrameTreeNodeId frame_id_before,
      const MediaRoute::Id& route_id_to_move,
      content::FrameTreeNodeId frame_id_after,
      const MediaRoute::Id& route_id_to_terminate) {
    EXPECT_EQ(2u, manager_->GetRoutes().size());

    // `route_id_to_move` is connected to `sink_`.
    EXPECT_TRUE(manager_->GetRoute(route_id_to_move));
    EXPECT_EQ(manager_->GetSinkForMirroringActivity(frame_id_before), sink_);

    // `route_id_to_terminate` is connected to `sink2_`.
    EXPECT_TRUE(manager_->GetRoute(route_id_to_terminate));
    EXPECT_EQ(manager_->GetSinkForMirroringActivity(frame_id_after), sink2_);

    ExpectMirroringActivityStoppedTimes(1);
    ExpectSingleRouteUpdate();
    manager_->OnSourceChanged(route_id_to_move, frame_id_before,
                              frame_id_after);
    router_remote_.FlushForTesting();
    testing::Mock::VerifyAndClearExpectations(&mock_router_);
    // The route with id `route_id_to_terminate` should be terminated.
    // `frame_id_before` should have no route connected to it.
    // `frame_id_after` should be connected to `route_id_to_move`.

    // Verify that `route_id_to_terminate` was terminated.
    EXPECT_EQ(1u, manager_->GetRoutes().size());
    EXPECT_FALSE(manager_->GetRoute(route_id_to_terminate));
    EXPECT_TRUE(manager_->GetRoute(route_id_to_move));

    // Verify that the source tab has been updated.
    EXPECT_FALSE(manager_->GetSinkForMirroringActivity(frame_id_before));
    EXPECT_EQ(manager_->GetSinkForMirroringActivity(frame_id_after), sink_);

    ExpectNoRouteUpdate();
    manager_->OnSourceChanged(route_id_to_move, frame_id_before,
                              frame_id_after);
    // Nothing is expected to happen as there is no route exist for the given
    // `frame_id_before`.
    EXPECT_EQ(1u, manager_->GetRoutes().size());
    EXPECT_TRUE(manager_->GetRoute(route_id_to_move));
    EXPECT_FALSE(manager_->GetSinkForMirroringActivity(frame_id_before));
    EXPECT_EQ(manager_->GetSinkForMirroringActivity(frame_id_after), sink_);
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  NiceMock<MockMojoMediaRouter> mock_router_;
  mojo::Remote<mojom::MediaRouter> router_remote_;
  std::unique_ptr<mojo::Receiver<mojom::MediaRouter>> router_receiver_;
  cast_channel::MockCastSocketService socket_service_;
  cast_channel::MockCastSocket socket_;
  NiceMock<cast_channel::MockCastMessageHandler> message_handler_;
  MediaSinkInternal sink_ = CreateCastSink(kChannelId);
  MediaSinkInternal sink2_ = CreateCastSink(kChannelId2);
  std::unique_ptr<MediaRoute> route_;
  cast_channel::LaunchSessionCallback launch_session_callback_;
  TestMediaSinkService media_sink_service_;
  MockCastAppDiscoveryService app_discovery_service_;
  std::unique_ptr<CastActivityManager> manager_;
  std::unique_ptr<CastSessionTracker> session_tracker_;
  raw_ptr<MockAppActivity, DanglingUntriaged> app_activity_ = nullptr;
  raw_ptr<MockMirroringActivity, DanglingUntriaged> mirroring_activity_ =
      nullptr;
  MockAppActivityCallback app_activity_callback_ = base::DoNothing();
  MockMirroringActivityCallback mirroring_activity_callback_ =
      base::DoNothing();
  const url::Origin origin_ = url::Origin::Create(GURL(kOrigin));
  const MediaSource::Id route_query_ = "theRouteQuery";
  std::optional<MediaRoute> updated_route_;
  cast_channel::Result stop_session_callback_arg_ = cast_channel::Result::kOk;
  NiceMock<MockLogger> logger_;
  mojom::RoutePresentationConnectionPtr presentation_connections_;
  const std::string cast_streaming_app_id_;
};

TEST_F(CastActivityManagerTest, LaunchAppSession) {
  LaunchAppSession();
  EXPECT_EQ(RouteControllerType::kGeneric, route_->controller_type());
}

TEST_F(CastActivityManagerTest, LaunchAppSessionWithAppParams) {
  LaunchAppSession(kAppId1, kAppParams);
  EXPECT_EQ(RouteControllerType::kGeneric, route_->controller_type());
}

TEST_F(CastActivityManagerTest, LaunchSessionSuccessWhenUserAllowed) {
  CallLaunchSessionSuccess();

  cast_channel::LaunchSessionCallbackWrapper out_callback;

  cast_channel::LaunchSessionResponse response;
  response.result =
      cast_channel::LaunchSessionResponse::Result::kPendingUserAuth;
  std::move(launch_session_callback_).Run(std::move(response), &out_callback);

  EXPECT_FALSE(out_callback.callback.is_null());

  cast_channel::LaunchSessionResponse response2;
  response2.result = cast_channel::LaunchSessionResponse::Result::kUserAllowed;
  std::move(out_callback.callback).Run(std::move(response2), &out_callback);

  EXPECT_FALSE(out_callback.callback.is_null());

  cast_channel::LaunchSessionResponse response3;
  response3.result = cast_channel::LaunchSessionResponse::Result::kOk;
  response3.receiver_status = MakeReceiverStatus(kAppId1);

  std::move(out_callback.callback).Run(std::move(response3), &out_callback);

  EXPECT_TRUE(out_callback.callback.is_null());

  RunUntilIdle();

  EXPECT_EQ(RouteControllerType::kGeneric, route_->controller_type());
}

TEST_F(CastActivityManagerTest, LaunchMirroringSession) {
  LaunchNonSdkMirroringSession();
  EXPECT_EQ(RouteControllerType::kMirroring, route_->controller_type());
}

TEST_F(CastActivityManagerTest, LaunchMirroringSessionViaCastSdk) {
  LaunchCastSdkMirroringSession();
  EXPECT_EQ(RouteControllerType::kMirroring, route_->controller_type());
}

TEST_F(CastActivityManagerTest, LaunchSiteInitiatedMirroringSession) {
  // For a session initiated by a website with the mirroring source we should be
  // establishing a presentation connection, even if the client ID isn't set.
  CallLaunchSessionSuccess(cast_streaming_app_id_, /*app_params*/ "",
                           /*client_id*/ "");
  ReceiveLaunchSuccessResponseFromReceiver(cast_streaming_app_id_);
  EXPECT_FALSE(presentation_connections_.is_null());
  EXPECT_EQ(RouteControllerType::kMirroring, route_->controller_type());
}

TEST_F(CastActivityManagerTest, MirroringSessionStopped) {
  LaunchNonSdkMirroringSession();
  ExpectMirroringActivityStoppedTimes(1);
  ExpectEmptyRouteUpdate();
  mirroring_activity_->DidStop();
}

TEST_F(CastActivityManagerTest, LaunchSessionFails) {
  // 3 things will happen:
  // (1) Route is removed
  // (2) Issue will be sent.
  // (3) The PresentationConnection associated with the route will be closed
  //     with error.

  // A launch session request is sent to the sink.
  CallLaunchSessionFailure();

  EXPECT_CALL(
      *app_activity_,
      ClosePresentationConnections(
          blink::mojom::PresentationConnectionCloseReason::CONNECTION_ERROR));
  ExpectEmptyRouteUpdate();
  EXPECT_CALL(mock_router_, OnIssue);

  cast_channel::LaunchSessionResponse response;
  response.result = cast_channel::LaunchSessionResponse::Result::kError;
  std::move(launch_session_callback_).Run(std::move(response), nullptr);
  RunUntilIdle();
}

TEST_F(CastActivityManagerTest, LaunchSessionFailsWhenUserNotAllowed) {
  CallLaunchSessionFailure();

  cast_channel::LaunchSessionCallbackWrapper out_callback;
  cast_channel::LaunchSessionResponse response;
  response.result =
      cast_channel::LaunchSessionResponse::Result::kPendingUserAuth;
  std::move(launch_session_callback_).Run(std::move(response), &out_callback);

  EXPECT_FALSE(out_callback.callback.is_null());

  EXPECT_CALL(
      *app_activity_,
      ClosePresentationConnections(
          blink::mojom::PresentationConnectionCloseReason::CONNECTION_ERROR));
  ExpectEmptyRouteUpdate();

  cast_channel::LaunchSessionResponse response2;
  response2.result =
      cast_channel::LaunchSessionResponse::Result::kUserNotAllowed;
  std::move(out_callback.callback).Run(std::move(response2), &out_callback);
  EXPECT_TRUE(out_callback.callback.is_null());
}

TEST_F(CastActivityManagerTest,
       LaunchSessionFailsWhenNotificationsAreDisabledOnReceiver) {
  CallLaunchSessionFailure();

  EXPECT_CALL(
      *app_activity_,
      ClosePresentationConnections(
          blink::mojom::PresentationConnectionCloseReason::CONNECTION_ERROR));
  ExpectEmptyRouteUpdate();

  cast_channel::LaunchSessionResponse response;
  response.result =
      cast_channel::LaunchSessionResponse::Result::kNotificationDisabled;
  std::move(launch_session_callback_).Run(std::move(response), nullptr);
  RunUntilIdle();
}

TEST_F(CastActivityManagerTest, LaunchSessionFailsWhenSessionIsRemoved) {
  CallLaunchSessionFailure();
  manager_->OnSessionRemoved(sink_);

  // The launch session callback should still be called even if the session was
  // removed before receiving a response from the receiver.
  EXPECT_CALL(*this, LaunchSessionFailed());
  cast_channel::LaunchSessionResponse response;
  response.result = cast_channel::LaunchSessionResponse::Result::kError;
  std::move(launch_session_callback_).Run(std::move(response), nullptr);
}

TEST_F(CastActivityManagerTest, LaunchAppSessionFailsWithAppParams) {
  EXPECT_CALL(message_handler_, LaunchSession).Times(0);
  auto source =
      CastMediaSource::FromMediaSourceId(MakeSourceId(kAppId1, "invalidjson"));
  ASSERT_TRUE(source);

  // Callback will be invoked synchronously.
  manager_->LaunchSession(
      *source, sink_, kPresentationId, origin_, kFrameTreeNodeId,
      base::BindOnce(&CastActivityManagerTest::ExpectLaunchSessionFailure,
                     base::Unretained(this)));

  RunUntilIdle();
}

TEST_F(CastActivityManagerTest, LaunchSessionTerminatesExistingSessionFromTab) {
  LaunchAppSession();
  ExpectAppActivityStoppedTimes(1);

  // Launch a new session from the same tab on a different sink.
  auto source = CastMediaSource::FromMediaSourceId(MakeSourceId(kAppId2));
  // Use LaunchSessionParsed() instead of LaunchSession() here because
  // LaunchSessionParsed() is called asynchronously and will fail the test.
  manager_->LaunchSessionParsed(
      *source, sink2_, kPresentationId2, origin_, kFrameTreeNodeId,
      base::BindOnce(&CastActivityManagerTest::ExpectLaunchSessionSuccess,
                     base::Unretained(this)),
      data_decoder::DataDecoder::ValueOrError());
}

TEST_F(CastActivityManagerTest, LaunchSessionTerminatesPendingLaunchFromTab) {
  CallLaunchSessionFailure();
  // Stop session message not sent because session has not launched yet.
  ExpectAppActivityStoppedTimes(0);

  // Launch a new session from the same tab on a different sink.
  auto source = CastMediaSource::FromMediaSourceId(MakeSourceId(kAppId2));
  // Use LaunchSessionParsed() instead of LaunchSession() here because
  // LaunchSessionParsed() is called asynchronously and will fail the test.
  manager_->LaunchSessionParsed(
      *source, sink2_, kPresentationId2, origin_, kFrameTreeNodeId,
      base::BindOnce(&CastActivityManagerTest::ExpectLaunchSessionSuccess,
                     base::Unretained(this)),
      data_decoder::DataDecoder::ValueOrError());
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
  LaunchAppSession();

  EXPECT_CALL(*app_activity_, OnSessionUpdated).WillOnce([this]() {
    EXPECT_EQ(sink_, app_activity_->sink());
  });
  auto session = MakeSession(kAppId1);
  ExpectSingleRouteUpdate();
  manager_->OnSessionAddedOrUpdated(sink_, *session);
  RunUntilIdle();
  ASSERT_TRUE(updated_route_);
  EXPECT_TRUE(updated_route_->is_local());
}

// This test is essentially the same as UpdateNewlyCreatedSession, but it uses
// mirroring, which at one point was handled differently enough that this test
// would have failed.
TEST_F(CastActivityManagerTest, UpdateNewlyCreatedMirroringSession) {
  LaunchCastSdkMirroringSession();

  ASSERT_TRUE(mirroring_activity_);
  auto session = MakeSession(cast_streaming_app_id_);
  ExpectSingleRouteUpdate();
  manager_->OnSessionAddedOrUpdated(sink_, *session);
  RunUntilIdle();
  ASSERT_TRUE(updated_route_);
  EXPECT_TRUE(updated_route_->is_local());
}

TEST_F(CastActivityManagerTest, OnSessionAddedOrUpdated) {
  LaunchAppSession();
  auto session = MakeSession(kAppId1);
  ExpectSingleRouteUpdate();
  EXPECT_CALL(*app_activity_, OnSessionUpdated(_, "theHashToken"));
  manager_->OnSessionAddedOrUpdated(sink_, *session);
}

// TODO(takumif): Add a test case to terminate a session and launch another.
TEST_F(CastActivityManagerTest, TerminateSession) {
  LaunchAppSession();
  ExpectAppActivityStoppedTimes(1);
  ExpectEmptyRouteUpdate();
  TerminateSession(true);
}

TEST_F(CastActivityManagerTest, TerminateSessionFails) {
  LaunchAppSession();
  ExpectAppActivityStoppedTimes(1);
  ExpectNoRouteUpdate();
  TerminateSession(false);
}

TEST_F(CastActivityManagerTest, DestructorClosesLocalMirroringSession) {
  LaunchNonSdkMirroringSession();
  ExpectMirroringActivityStoppedTimes(1);
  ExpectEmptyRouteUpdate();
  manager_.reset();
}

TEST_F(CastActivityManagerTest, DestructorIgnoresNonlocalMirroringSession) {
  AddRemoteMirroringSession();
  ExpectMirroringActivityStoppedTimes(0);
  manager_.reset();
}

TEST_F(CastActivityManagerTest, DestructorIgnoresAppSession) {
  LaunchAppSession();
  ExpectAppActivityStoppedTimes(0);
  manager_.reset();
}

TEST_F(CastActivityManagerTest, TerminateSessionBeforeLaunchResponse) {
  CallLaunchSessionFailure();
  // Stop session message not sent because session has not launched yet.
  ExpectAppActivityStoppedTimes(0);
  ExpectNoRouteUpdate();
  TerminateSession(true,
                   MediaRoute::GetMediaRouteId(
                       kPresentationId, sink_.id(),
                       MediaSource(MakeSourceId(kAppId1, "", kClientId))));
  ExpectEmptyRouteUpdate();
  ReceiveLaunchSuccessResponseFromReceiver();
}

TEST_F(CastActivityManagerTest, AppMessageFromReceiver) {
  LaunchAppSession();

  // Destination ID matches client ID.
  openscreen::cast::proto::CastMessage message =
      cast_channel::CreateCastMessage("urn:x-cast:com.google.foo",
                                      base::Value(base::Value::Dict()),
                                      "sourceId", "theClientId");

  EXPECT_CALL(*app_activity_, OnAppMessage(IsCastChannelMessage(message)));
  manager_->OnAppMessage(kChannelId, message);
}

TEST_F(CastActivityManagerTest, OnMediaStatusUpdated) {
  LaunchAppSession();

  const char status[] = R"({"foo": "bar"})";
  std::optional<int> request_id(345);

  EXPECT_CALL(*app_activity_,
              SendMediaStatusToClients(IsJson(status), request_id));
  manager_->OnMediaStatusUpdated(sink_, ParseJsonDict(status), request_id);
}

TEST_F(CastActivityManagerTest, OnSourceChanged) {
  LaunchNonSdkMirroringSession();

  MediaRoute::Id route_id = MediaRoute::GetMediaRouteId(
      kPresentationId, sink_.id(),
      MediaSource(MakeSourceId(cast_streaming_app_id_, /* app_params */ "",
                               /* client_id */ "")));

  // Launch a 2nd session from a different tab on a different sink.
  media_sink_service_.AddOrUpdateSink(sink2_);
  EXPECT_CALL(message_handler_,
              LaunchSession(kChannelId2, cast_streaming_app_id_, _, _, _, _))
      .WillOnce(WithArg<5>([this](auto callback) {
        launch_session_callback_ = std::move(callback);
      }));

  auto source2 =
      CastMediaSource::FromMediaSourceId(MakeSourceId(cast_streaming_app_id_));
  manager_->LaunchSession(
      *source2, sink2_, kPresentationId2, origin_, kFrameTreeNodeId2,
      base::BindOnce(&CastActivityManagerTest::ExpectLaunchSessionSuccess,
                     base::Unretained(this)));

  RunUntilIdle();
  ReceiveLaunchSuccessResponseFromReceiver(cast_streaming_app_id_);

  MediaRoute::Id route_id2 = MediaRoute::GetMediaRouteId(
      kPresentationId2, sink2_.id(), MediaSource(source2->source_id()));

  UpdateRouteSourceTabInRoutesMap(kFrameTreeNodeId, route_id, kFrameTreeNodeId2,
                                  route_id2);
}

TEST_F(CastActivityManagerTest, StartSessionAndRemoveExistingSessionOnSink) {
  LaunchAppSession();

  // Launch another route on the same sink and store it in `route_`.
  EXPECT_CALL(message_handler_, LaunchSession(kChannelId, kAppId2, _, _, _, _))
      .WillOnce(WithArg<5>([this](auto callback) {
        launch_session_callback_ = std::move(callback);
      }));
  auto source = CastMediaSource::FromMediaSourceId(MakeSourceId(kAppId2));
  manager_->LaunchSessionParsed(
      *source, sink_, kPresentationId2, origin_, kFrameTreeNodeId2,
      base::BindOnce(&CastActivityManagerTest::ExpectLaunchSessionSuccess,
                     base::Unretained(this)),
      data_decoder::DataDecoder::ValueOrError());
  RunUntilIdle();
  ReceiveLaunchSuccessResponseFromReceiver(kAppId2);

  // Removing a session from the sink removes the first route, leaving us with
  // `route_`.
  ExpectSingleRouteUpdate();
  manager_->OnSessionRemoved(sink_);
  RunUntilIdle();
  EXPECT_EQ(updated_route_->media_route_id(), route_->media_route_id());
}

TEST_F(CastActivityManagerTest, FindMirroringActivityByRouteIdNonMirroring) {
  LaunchAppSession();
  EXPECT_FALSE(
      manager_->FindMirroringActivityByRouteId(route_->media_route_id()));
}

TEST_F(CastActivityManagerTest, FindMirroringActivityByRouteId) {
  LaunchCastSdkMirroringSession();
  EXPECT_TRUE(
      manager_->FindMirroringActivityByRouteId(route_->media_route_id()));
}

TEST_F(CastActivityManagerTest, LaunchAccessCodeCastSavedDeviceSuccess) {
  // Set the sink member variable to a saved device.
  sink_.cast_data().discovery_type =
      CastDiscoveryType::kAccessCodeRememberedDevice;

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(histogram, 0);

  // A successful saved device mirroring request should trigger 1 histogram
  // count.
  LaunchNonSdkMirroringSession();
  histogram_tester.ExpectTotalCount(histogram, 1);
}

TEST_F(CastActivityManagerTest, LaunchAccessCodeCastSavedDeviceFailure) {
  // Set the sink member variable to a saved device.
  sink_.cast_data().discovery_type =
      CastDiscoveryType::kAccessCodeRememberedDevice;

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(histogram, 0);

  // A failed saved device mirroring request should trigger 0 histogram count.
  CallLaunchSessionFailure();
  histogram_tester.ExpectTotalCount(histogram, 0);
}

TEST_F(CastActivityManagerTest, LaunchAccessCodeCastInstantDeviceSuccess) {
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(histogram, 0);

  // Set the sink member variable to a instant device.
  sink_.cast_data().discovery_type = CastDiscoveryType::kAccessCodeManualEntry;

  // A successful instant device mirroring request should not trigger any
  // histogram count.
  LaunchNonSdkMirroringSession();
  histogram_tester.ExpectTotalCount(histogram, 0);
}

TEST_F(CastActivityManagerTest, LaunchMdnsInstantDeviceSuccess) {
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(histogram, 0);

  // Set the sink member variable to Mdns.
  sink_.cast_data().discovery_type = CastDiscoveryType::kMdns;

  // A successful instant device mirroring request should not trigger any
  // histogram count.
  LaunchNonSdkMirroringSession();
  histogram_tester.ExpectTotalCount(histogram, 0);
}

}  // namespace media_router
