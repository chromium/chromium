// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/cast/cast_media_route_provider.h"

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/media/router/providers/cast/cast_session_tracker.h"
#include "chrome/browser/media/router/test/mock_mojo_media_router.h"
#include "chrome/browser/media/router/test/test_helper.h"
#include "chrome/common/media_router/test/test_helper.h"
#include "components/cast_channel/cast_test_util.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace media_router {

namespace {
static constexpr char kCastSource[] = "cast:ABCDEFGH?clientId=123";
static constexpr char kPresentationId[] = "presentationId";
static constexpr char kOrigin[] = "https://www.youtube.com";
static constexpr int kTabId = 1;
static constexpr base::TimeDelta kRouteTimeout =
    base::TimeDelta::FromSeconds(30);
}  // namespace

class CastMediaRouteProviderTest : public testing::Test {
 public:
  CastMediaRouteProviderTest()
      : socket_service_(
            base::CreateSingleThreadTaskRunner({content::BrowserThread::UI})),
        message_handler_(&socket_service_) {}
  ~CastMediaRouteProviderTest() override = default;

  void SetUp() override {
    mojo::PendingRemote<mojom::MediaRouter> router_remote;
    router_receiver_ = std::make_unique<mojo::Receiver<mojom::MediaRouter>>(
        &mock_router_, router_remote.InitWithNewPipeAndPassReceiver());

    session_tracker_ = std::unique_ptr<CastSessionTracker>(
        new CastSessionTracker(&media_sink_service_, &message_handler_,
                               socket_service_.task_runner()));
    CastSessionTracker::SetInstanceForTest(session_tracker_.get());

    EXPECT_CALL(mock_router_, OnSinkAvailabilityUpdated(_, _));
    provider_ = std::make_unique<CastMediaRouteProvider>(
        provider_remote_.BindNewPipeAndPassReceiver(), std::move(router_remote),
        &media_sink_service_, &app_discovery_service_, &message_handler_,
        "hash-token", base::SequencedTaskRunnerHandle::Get());

    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    provider_.reset();
    CastSessionTracker::SetInstanceForTest(nullptr);
    session_tracker_.reset();
  }

  void ExpectCreateRouteSuccessAndSetRoute(
      const base::Optional<MediaRoute>& route,
      mojom::RoutePresentationConnectionPtr presentation_connections,
      const base::Optional<std::string>& error,
      RouteRequestResult::ResultCode result) {
    EXPECT_TRUE(route);
    EXPECT_TRUE(presentation_connections);
    EXPECT_FALSE(error);
    EXPECT_EQ(RouteRequestResult::ResultCode::OK, result);
    route_ = std::make_unique<MediaRoute>(*route);
  }

  void ExpectCreateRouteFailure(
      RouteRequestResult::ResultCode expected_result,
      const base::Optional<MediaRoute>& route,
      mojom::RoutePresentationConnectionPtr presentation_connections,
      const base::Optional<std::string>& error,
      RouteRequestResult::ResultCode result) {
    EXPECT_FALSE(route);
    EXPECT_FALSE(presentation_connections);
    EXPECT_TRUE(error);
    EXPECT_EQ(expected_result, result);
  }

  void ExpectTerminateRouteSuccess(const base::Optional<std::string>& error,
                                   RouteRequestResult::ResultCode result) {
    EXPECT_FALSE(error);
    EXPECT_EQ(RouteRequestResult::ResultCode::OK, result);
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;

  mojo::Remote<mojom::MediaRouteProvider> provider_remote_;
  MockMojoMediaRouter mock_router_;
  std::unique_ptr<mojo::Receiver<mojom::MediaRouter>> router_receiver_;

  cast_channel::MockCastSocketService socket_service_;
  cast_channel::MockCastMessageHandler message_handler_;

  std::unique_ptr<CastSessionTracker> session_tracker_;
  TestMediaSinkService media_sink_service_;
  MockCastAppDiscoveryService app_discovery_service_;
  std::unique_ptr<CastMediaRouteProvider> provider_;

  url::Origin origin_ = url::Origin::Create(GURL(kOrigin));
  std::unique_ptr<MediaRoute> route_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CastMediaRouteProviderTest);
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

TEST_F(CastMediaRouteProviderTest, BroadcastRequest) {
  media_sink_service_.AddOrUpdateSink(CreateCastSink(1));
  media_sink_service_.AddOrUpdateSink(CreateCastSink(2));
  MediaSource::Id source_id(
      "cast:ABCDEFAB?capabilities=video_out,audio_out"
      "&clientId=123"
      "&broadcastNamespace=namespace"
      "&broadcastMessage=message");

  std::vector<std::string> app_ids = {"ABCDEFAB"};
  cast_channel::BroadcastRequest request("namespace", "message");
  EXPECT_CALL(message_handler_, SendBroadcastMessage(1, app_ids, request));
  EXPECT_CALL(message_handler_, SendBroadcastMessage(2, app_ids, request));
  EXPECT_CALL(app_discovery_service_, DoStartObservingMediaSinks(_)).Times(0);
  provider_->StartObservingMediaSinks(source_id);
  EXPECT_TRUE(app_discovery_service_.callbacks().empty());
}

TEST_F(CastMediaRouteProviderTest, CreateRouteFailsInvalidSink) {
  // Sink does not exist.
  provider_->CreateRoute(
      kCastSource, "sinkId", kPresentationId, origin_, kTabId, kRouteTimeout,
      /* incognito */ false,
      base::BindOnce(&CastMediaRouteProviderTest::ExpectCreateRouteFailure,
                     base::Unretained(this),
                     RouteRequestResult::ResultCode::SINK_NOT_FOUND));
}

TEST_F(CastMediaRouteProviderTest, CreateRouteFailsInvalidSource) {
  MediaSinkInternal sink = CreateCastSink(1);
  media_sink_service_.AddOrUpdateSink(sink);

  provider_->CreateRoute(
      "invalidSource", sink.sink().id(), kPresentationId, origin_, kTabId,
      kRouteTimeout, /* incognito */ false,
      base::BindOnce(&CastMediaRouteProviderTest::ExpectCreateRouteFailure,
                     base::Unretained(this),
                     RouteRequestResult::ResultCode::NO_SUPPORTED_PROVIDER));
}

TEST_F(CastMediaRouteProviderTest, CreateRoute) {
  MediaSinkInternal sink = CreateCastSink(1);
  media_sink_service_.AddOrUpdateSink(sink);

  EXPECT_CALL(message_handler_, LaunchSession(_, _, _, _));
  provider_->CreateRoute(
      kCastSource, sink.sink().id(), kPresentationId, origin_, kTabId,
      kRouteTimeout, /* incognito */ false,
      base::BindOnce(
          &CastMediaRouteProviderTest::ExpectCreateRouteSuccessAndSetRoute,
          base::Unretained(this)));
}

TEST_F(CastMediaRouteProviderTest, TerminateRoute) {
  MediaSinkInternal sink = CreateCastSink(1);
  media_sink_service_.AddOrUpdateSink(sink);

  EXPECT_CALL(message_handler_, LaunchSession(_, _, _, _));
  provider_->CreateRoute(
      kCastSource, sink.sink().id(), kPresentationId, origin_, kTabId,
      kRouteTimeout, /* incognito */ false,
      base::BindOnce(
          &CastMediaRouteProviderTest::ExpectCreateRouteSuccessAndSetRoute,
          base::Unretained(this)));

  ASSERT_TRUE(route_);
  provider_->TerminateRoute(
      route_->media_route_id(),
      base::BindOnce(&CastMediaRouteProviderTest::ExpectTerminateRouteSuccess,
                     base::Unretained(this)));
}

}  // namespace media_router
