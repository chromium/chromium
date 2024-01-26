// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/cast/cast_media_route_provider.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/test_simple_task_runner.h"
#include "chrome/browser/media/router/providers/cast/cast_activity_manager.h"
#include "chrome/browser/media/router/providers/cast/cast_session_tracker.h"
#include "chrome/browser/media/router/providers/cast/mock_mirroring_activity.h"
#include "chrome/browser/media/router/providers/cast/mock_mirroring_service_host.h"
#include "chrome/browser/media/router/test/mock_mojo_media_router.h"
#include "chrome/browser/media/router/test/provider_test_helpers.h"
#include "components/media_router/common/media_route.h"
#include "components/media_router/common/providers/cast/channel/cast_test_util.h"
#include "components/media_router/common/test/test_helper.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "media/media_buildflags.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using testing::Mock;
using ::testing::NiceMock;
using testing::WithArg;

namespace media_router {

namespace {
static constexpr char kAppId[] = "ABCDEFGH";

constexpr char kCastSource[] =
    "cast:ABCDEFGH?clientId=theClientId&appParams={\"credentialsType\":"
    "\"mobile\"}";
static constexpr char kPresentationId[] = "presentationId";
static constexpr char kOrigin[] = "https://www.youtube.com";
static constexpr int kFrameTreeNodeId = 1;
static constexpr base::TimeDelta kRouteTimeout = base::Seconds(30);

base::Value::Dict MakeReceiverStatus() {
  return base::test::ParseJsonDict(R"({
        "applications": [{
          "appId": "ABCDEFGH",
          "displayName": "theDisplayName",
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

MediaSinkInternal CreateCastSinkWithModelName(const std::string model_name) {
  MediaSinkInternal cast_sink = CreateCastSink(1);
  auto cast_data = cast_sink.cast_data();
  cast_data.model_name = model_name;
  cast_sink.set_cast_data(cast_data);
  return cast_sink;
}
}  // namespace

class CastMediaRouteProviderTest : public testing::Test {
 public:
  CastMediaRouteProviderTest()
      : socket_service_(content::GetUIThreadTaskRunner({})),
        message_handler_(&socket_service_) {}
  CastMediaRouteProviderTest(CastMediaRouteProviderTest&) = delete;
  CastMediaRouteProviderTest& operator=(CastMediaRouteProviderTest&) = delete;
  ~CastMediaRouteProviderTest() override = default;

  void SetUp() override {
    mojo::PendingRemote<mojom::MediaRouter> router_remote;
    router_receiver_ = std::make_unique<mojo::Receiver<mojom::MediaRouter>>(
        &mock_router_, router_remote.InitWithNewPipeAndPassReceiver());

    session_tracker_ = std::unique_ptr<CastSessionTracker>(
        new CastSessionTracker(&media_sink_service_, &message_handler_,
                               socket_service_.task_runner()));
    CastSessionTracker::SetInstanceForTest(session_tracker_.get());

    provider_ = std::make_unique<CastMediaRouteProvider>(
        provider_remote_.BindNewPipeAndPassReceiver(), std::move(router_remote),
        &media_sink_service_, &app_discovery_service_, &message_handler_,
        "hash-token", base::SequencedTaskRunner::GetCurrentDefault());

    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    provider_.reset();
    CastSessionTracker::SetInstanceForTest(nullptr);
    session_tracker_.reset();
  }

  void ExpectCreateRouteSuccessAndSetRoute(
      const std::optional<MediaRoute>& route,
      mojom::RoutePresentationConnectionPtr presentation_connections,
      const std::optional<std::string>& error,
      mojom::RouteRequestResultCode result) {
    EXPECT_TRUE(route);
    EXPECT_TRUE(presentation_connections);
    EXPECT_FALSE(error);
    EXPECT_EQ(mojom::RouteRequestResultCode::OK, result);
    route_ = std::make_unique<MediaRoute>(*route);
  }

  void ExpectCreateRouteFailure(
      mojom::RouteRequestResultCode expected_result,
      const std::optional<MediaRoute>& route,
      mojom::RoutePresentationConnectionPtr presentation_connections,
      const std::optional<std::string>& error,
      mojom::RouteRequestResultCode result) {
    EXPECT_FALSE(route);
    EXPECT_FALSE(presentation_connections);
    EXPECT_TRUE(error);
    EXPECT_EQ(expected_result, result);
  }

  void ExpectTerminateRouteSuccess(const std::optional<std::string>& error,
                                   mojom::RouteRequestResultCode result) {
    EXPECT_FALSE(error);
    EXPECT_EQ(mojom::RouteRequestResultCode::OK, result);
    route_.reset();
  }

  void SendLaunchSessionResponseSuccess() {
    cast_channel::LaunchSessionResponse response;
    response.result = cast_channel::LaunchSessionResponse::Result::kOk;
    response.receiver_status = MakeReceiverStatus();
    std::move(launch_session_callback_).Run(std::move(response), nullptr);
    base::RunLoop().RunUntilIdle();
  }

  void SendLaunchSessionResponseFailure() {
    cast_channel::LaunchSessionResponse response;
    response.result = cast_channel::LaunchSessionResponse::Result::kError;
    std::move(launch_session_callback_).Run(std::move(response), nullptr);
    base::RunLoop().RunUntilIdle();
  }

  void UpdateSinkQueryAndExpectSinkReceived(
      const std::vector<MediaSinkInternal>& expected_received_sinks,
      const MediaSource::Id& source_id,
      const std::vector<MediaSinkInternal>& discovered_sinks) {
    base::RunLoop run_loop;
    EXPECT_CALL(mock_router_, OnSinksReceived(mojom::MediaRouteProviderId::CAST,
                                              _, expected_received_sinks, _))
        .WillOnce(base::test::RunOnceClosure(run_loop.QuitClosure()));
    provider_->OnSinkQueryUpdated(source_id, discovered_sinks);
    run_loop.Run();
    Mock::VerifyAndClearExpectations(&mock_router_);
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;

  mojo::Remote<mojom::MediaRouteProvider> provider_remote_;
  NiceMock<MockMojoMediaRouter> mock_router_;
  std::unique_ptr<mojo::Receiver<mojom::MediaRouter>> router_receiver_;

  cast_channel::MockCastSocketService socket_service_;
  NiceMock<cast_channel::MockCastMessageHandler> message_handler_;

  std::unique_ptr<CastSessionTracker> session_tracker_;
  TestMediaSinkService media_sink_service_;
  MockCastAppDiscoveryService app_discovery_service_;
  std::unique_ptr<CastMediaRouteProvider> provider_;

  cast_channel::LaunchSessionCallback launch_session_callback_;

  url::Origin origin_ = url::Origin::Create(GURL(kOrigin));
  std::unique_ptr<MediaRoute> route_;
};

TEST_F(CastMediaRouteProviderTest, StartObservingMediaSinks) {
  MediaSource::Id non_cast_source("not-a-cast-source:foo");
  EXPECT_CALL(app_discovery_service_, DoStartObservingMediaSinks(_)).Times(0);
  provider_->StartObservingMediaSinks(non_cast_source);

  EXPECT_CALL(app_discovery_service_, DoStartObservingMediaSinks(_));
  provider_->StartObservingMediaSinks(kCastSource);
  EXPECT_FALSE(app_discovery_service_.callbacks().empty());

  provider_->StopObservingMediaSinks(kCastSource);
  EXPECT_TRUE(app_discovery_service_.callbacks().empty());
}

TEST_F(CastMediaRouteProviderTest, CreateRouteFailsInvalidSink) {
  // Sink does not exist.
  provider_->CreateRoute(
      kCastSource, "sinkId", kPresentationId, origin_, kFrameTreeNodeId,
      kRouteTimeout,
      base::BindOnce(&CastMediaRouteProviderTest::ExpectCreateRouteFailure,
                     base::Unretained(this),
                     mojom::RouteRequestResultCode::SINK_NOT_FOUND));
}

TEST_F(CastMediaRouteProviderTest, CreateRouteFailsInvalidSource) {
  MediaSinkInternal sink = CreateCastSink(1);
  media_sink_service_.AddOrUpdateSink(sink);

  provider_->CreateRoute(
      "invalidSource", sink.sink().id(), kPresentationId, origin_,
      kFrameTreeNodeId, kRouteTimeout,
      base::BindOnce(&CastMediaRouteProviderTest::ExpectCreateRouteFailure,
                     base::Unretained(this),
                     mojom::RouteRequestResultCode::NO_SUPPORTED_PROVIDER));
}

TEST_F(CastMediaRouteProviderTest, CreateRoute) {
  MediaSinkInternal sink = CreateCastSink(1);
  media_sink_service_.AddOrUpdateSink(sink);
  auto quit_closure = task_environment_.QuitClosure();

  std::vector<std::string> default_supported_app_types = {"WEB"};
  EXPECT_CALL(
      message_handler_,
      LaunchSession(sink.cast_data().cast_channel_id, kAppId,
                    kDefaultLaunchTimeout, default_supported_app_types, _, _))
      .WillOnce(WithArg<5>([&, this](auto callback) {
        launch_session_callback_ = std::move(callback);
        quit_closure.Run();
      }));
  provider_->CreateRoute(
      kCastSource, sink.sink().id(), kPresentationId, origin_, kFrameTreeNodeId,
      kRouteTimeout,
      base::BindOnce(
          &CastMediaRouteProviderTest::ExpectCreateRouteSuccessAndSetRoute,
          base::Unretained(this)));
  task_environment_.RunUntilQuit();
  SendLaunchSessionResponseSuccess();
  ASSERT_TRUE(route_);
}

TEST_F(CastMediaRouteProviderTest, TerminateRoute) {
  MediaSinkInternal sink = CreateCastSink(1);
  media_sink_service_.AddOrUpdateSink(sink);
  auto quit_closure = task_environment_.QuitClosure();

  EXPECT_CALL(message_handler_, LaunchSession)
      .WillOnce(WithArg<5>([&, this](auto callback) {
        launch_session_callback_ = std::move(callback);
        quit_closure.Run();
      }));
  provider_->CreateRoute(
      kCastSource, sink.sink().id(), kPresentationId, origin_, kFrameTreeNodeId,
      kRouteTimeout,
      base::BindOnce(
          &CastMediaRouteProviderTest::ExpectCreateRouteSuccessAndSetRoute,
          base::Unretained(this)));
  task_environment_.RunUntilQuit();
  SendLaunchSessionResponseSuccess();

  ASSERT_TRUE(route_);
  EXPECT_CALL(message_handler_, StopSession)
      .WillOnce(WithArg<3>([](auto callback) {
        std::move(callback).Run(cast_channel::Result::kOk);
      }));
  provider_->TerminateRoute(
      route_->media_route_id(),
      base::BindOnce(&CastMediaRouteProviderTest::ExpectTerminateRouteSuccess,
                     base::Unretained(this)));
  ASSERT_FALSE(route_);
}

TEST_F(CastMediaRouteProviderTest, GetState) {
  MediaSinkInternal sink = CreateCastSink(1);
  media_sink_service_.AddOrUpdateSink(sink);
  session_tracker_->HandleReceiverStatusMessage(sink,
                                                base::test::ParseJsonDict(R"({
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
  })"));

  provider_->GetState(base::BindOnce([](mojom::ProviderStatePtr state) {
    ASSERT_TRUE(state);
    ASSERT_TRUE(state->is_cast_provider_state());
    const mojom::CastProviderState& cast_state =
        *(state->get_cast_provider_state());
    ASSERT_EQ(cast_state.session_state.size(), 1UL);
    const mojom::CastSessionState& session_state =
        *(cast_state.session_state[0]);
    EXPECT_EQ(session_state.sink_id, "cast:id1");
    EXPECT_EQ(session_state.app_id, "ABCDEFGH");
    EXPECT_EQ(session_state.session_id, "theSessionId");
    EXPECT_EQ(session_state.route_description, "App status");
  }));
}

// MediaRemotingWithoutFullscreen is enabled on Win/Mac/Linux.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
TEST_F(CastMediaRouteProviderTest, GetRemotePlaybackCompatibleSinks) {
  MediaSinkInternal cc = CreateCastSinkWithModelName("Chromecast");
  MediaSinkInternal cc_ultra = CreateCastSinkWithModelName("Chromecast Ultra");
  MediaSinkInternal nest = CreateCastSinkWithModelName("Nest");
  std::vector<MediaSinkInternal> all_sinks{cc, cc_ultra, nest};

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
  UpdateSinkQueryAndExpectSinkReceived(
      {cc, cc_ultra},
      "remote-playback:media-session?tab_id=1&video_codec=h264&audio_codec=aac",
      all_sinks);

  UpdateSinkQueryAndExpectSinkReceived(
      {cc_ultra},
      "remote-playback:media-session?tab_id=1&video_codec=hevc&audio_codec=aac",
      all_sinks);
#else

  UpdateSinkQueryAndExpectSinkReceived(
      {},
      "remote-playback:media-session?tab_id=1&video_codec=h264&audio_codec=aac",
      all_sinks);

  UpdateSinkQueryAndExpectSinkReceived(
      {cc, cc_ultra},
      "remote-playback:media-session?tab_id=1&video_codec=vp8&audio_codec=opus",
      all_sinks);

#endif

  UpdateSinkQueryAndExpectSinkReceived(
      {},
      "remote-playback:media-session?tab_id=1&video_codec=unknown&audio_codec="
      "unknown",
      all_sinks);

  UpdateSinkQueryAndExpectSinkReceived({},
                                       "remote-playback:media-session?tab_id=1&"
                                       "video_codec=vp8&audio_codec=unknown",
                                       all_sinks);

  UpdateSinkQueryAndExpectSinkReceived(
      std::vector<MediaSinkInternal>{cc, cc_ultra},
      "remote-playback:media-session?tab_id=1&video_codec=vp8", all_sinks);
}
#else
// MediaRemotingWithoutFullscreen is disabled on other desktop platforms. The
// Cast MRP should not update sinks for RemotePlayback MediaSource.
TEST_F(CastMediaRouteProviderTest, GetRemotePlaybackCompatibleSinks) {
  EXPECT_CALL(mock_router_, OnSinksReceived(_, _, _, _)).Times(0);
  provider_->OnSinkQueryUpdated(
      "remote-playback:media-session?tab_id=1&video_codec=vp8&audio_codec=aac",
      {CreateCastSinkWithModelName("Chromecast")});
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(&mock_router_);
}
#endif
}  // namespace media_router
