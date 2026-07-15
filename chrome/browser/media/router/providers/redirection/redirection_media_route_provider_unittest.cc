// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/redirection/redirection_media_route_provider.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/browser/media/router/providers/redirection/redirection_service_host.h"
#include "chrome/browser/media/router/test/mock_mojo_media_router.h"
#include "components/media_router/common/media_route.h"
#include "components/media_router/common/media_source.h"
#include "content/public/test/browser_task_environment.h"
#include "media/base/audio_codecs.h"
#include "media/base/video_codecs.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

using testing::_;
using testing::IsEmpty;
using testing::NiceMock;

namespace media_router {

namespace {

constexpr char kRedirectionSinkId[] = "redirection";
constexpr char kPresentationId[] = "presentationId";
constexpr mojom::MediaRouteProviderId kProviderId =
    mojom::MediaRouteProviderId::REDIRECTION;

// A RedirectionServiceHost that counts Start() calls without launching a
// utility process. The test holds a WeakPtr to observe destruction.
class FakeRedirectionServiceHost : public RedirectionServiceHost {
 public:
  FakeRedirectionServiceHost() = default;
  ~FakeRedirectionServiceHost() override = default;

  void Start() override { ++start_count_; }

  int get_start_count() const { return start_count_; }

  base::WeakPtr<FakeRedirectionServiceHost> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  int start_count_ = 0;
  base::WeakPtrFactory<FakeRedirectionServiceHost> weak_factory_{this};
};

// A provider that bypasses the OS session check and injects a fake host so
// tests never launch a utility process.
class TestRedirectionMediaRouteProvider : public RedirectionMediaRouteProvider {
 public:
  using RedirectionMediaRouteProvider::RedirectionMediaRouteProvider;

  void set_redirection_available(bool available) { available_ = available; }

  base::WeakPtr<FakeRedirectionServiceHost> host() { return host_; }

 protected:
  bool IsRedirectionAvailable() const override { return available_; }

  std::unique_ptr<RedirectionServiceHost> CreateServiceHost() override {
    auto host = std::make_unique<FakeRedirectionServiceHost>();
    host_ = host->GetWeakPtr();
    return host;
  }

 private:
  bool available_ = true;
  base::WeakPtr<FakeRedirectionServiceHost> host_;
};

}  // namespace

class RedirectionMediaRouteProviderTest : public testing::Test {
 public:
  RedirectionMediaRouteProviderTest() = default;
  ~RedirectionMediaRouteProviderTest() override = default;

  void SetUp() override {
    mojo::PendingRemote<mojom::MediaRouter> router_remote;
    router_receiver_.Bind(router_remote.InitWithNewPipeAndPassReceiver());
    provider_ = std::make_unique<TestRedirectionMediaRouteProvider>(
        provider_remote_.BindNewPipeAndPassReceiver(),
        std::move(router_remote));
  }

  void TearDown() override { provider_.reset(); }

 protected:
  // Source that backs the Cast UI's tab-mirroring query.
  static std::string TabMirroringSource() {
    return MediaSource::ForTab(/*tab_id=*/1).id();
  }

  // Source produced by the page-script Remote Playback path.
  static std::string RemotePlaybackSource() {
    return MediaSource::ForRemotePlayback(
               /*tab_id=*/1, media::VideoCodec::kH264, media::AudioCodec::kAAC)
        .id();
  }

  mojom::RouteRequestResultCode CreateRoute(const std::string& source,
                                            std::optional<MediaRoute>* route) {
    mojom::RouteRequestResultCode result_code =
        mojom::RouteRequestResultCode::UNKNOWN_ERROR;
    provider_->CreateRoute(
        source, kRedirectionSinkId, kPresentationId, url::Origin(),
        /*frame_tree_node_id=*/1, base::Seconds(30),
        base::BindLambdaForTesting(
            [&](const std::optional<MediaRoute>& created_route,
                mojom::RoutePresentationConnectionPtr,
                const std::optional<std::string>&,
                mojom::RouteRequestResultCode code) {
              *route = created_route;
              result_code = code;
            }));
    return result_code;
  }

  content::BrowserTaskEnvironment task_environment_;
  mojo::Remote<mojom::MediaRouteProvider> provider_remote_;
  NiceMock<MockMojoMediaRouter> router_;
  mojo::Receiver<mojom::MediaRouter> router_receiver_{&router_};
  std::unique_ptr<TestRedirectionMediaRouteProvider> provider_;
};

TEST_F(RedirectionMediaRouteProviderTest, ExposesSinkForTabMirroringSource) {
  const std::string source = TabMirroringSource();
  base::RunLoop run_loop;
  EXPECT_CALL(router_, OnSinksReceived(kProviderId, source, _, IsEmpty()))
      .WillOnce([&run_loop](mojom::MediaRouteProviderId, const std::string&,
                            const std::vector<MediaSinkInternal>& sinks,
                            const std::vector<url::Origin>&) {
        ASSERT_EQ(sinks.size(), 1u);
        EXPECT_EQ(sinks[0].sink().id(), kRedirectionSinkId);
        EXPECT_EQ(sinks[0].sink().provider_id(), kProviderId);
        run_loop.Quit();
      });
  provider_->StartObservingMediaSinks(source);
  run_loop.Run();
}

TEST_F(RedirectionMediaRouteProviderTest, HidesSinkFromRemotePlaybackSource) {
  // A page-script Remote Playback query does not queue a sink notification.
  EXPECT_CALL(router_, OnSinksReceived(_, _, _, _)).Times(0);
  provider_->StartObservingMediaSinks(RemotePlaybackSource());
}

TEST_F(RedirectionMediaRouteProviderTest, HidesSinkWhenUnavailable) {
  provider_->set_redirection_available(false);
  EXPECT_CALL(router_, OnSinksReceived(_, _, _, _)).Times(0);
  provider_->StartObservingMediaSinks(TabMirroringSource());
}

TEST_F(RedirectionMediaRouteProviderTest, CreateRouteSucceedsAndStartsHost) {
  std::optional<MediaRoute> route;
  EXPECT_EQ(CreateRoute(TabMirroringSource(), &route),
            mojom::RouteRequestResultCode::OK);

  ASSERT_TRUE(route.has_value());
  EXPECT_TRUE(route->is_local());
  EXPECT_EQ(route->media_sink_id(), kRedirectionSinkId);
  EXPECT_EQ(route->presentation_id(), kPresentationId);
  ASSERT_TRUE(provider_->host());
  EXPECT_EQ(provider_->host()->get_start_count(), 1);
}

TEST_F(RedirectionMediaRouteProviderTest, CreateRouteFailsWhenUnavailable) {
  provider_->set_redirection_available(false);
  std::optional<MediaRoute> route;
  EXPECT_EQ(CreateRoute(TabMirroringSource(), &route),
            mojom::RouteRequestResultCode::SINK_NOT_FOUND);

  EXPECT_FALSE(route.has_value());
  EXPECT_FALSE(provider_->host());
}

TEST_F(RedirectionMediaRouteProviderTest, TerminateRouteDestroysHost) {
  std::optional<MediaRoute> route;
  ASSERT_EQ(CreateRoute(TabMirroringSource(), &route),
            mojom::RouteRequestResultCode::OK);
  ASSERT_TRUE(route.has_value());
  ASSERT_TRUE(provider_->host());

  // The route list is cleared when the active route is terminated.
  base::RunLoop run_loop;
  EXPECT_CALL(router_, OnRoutesUpdated(kProviderId, IsEmpty()))
      .WillOnce(
          [&run_loop](mojom::MediaRouteProviderId,
                      const std::vector<MediaRoute>&) { run_loop.Quit(); });

  mojom::RouteRequestResultCode result_code =
      mojom::RouteRequestResultCode::UNKNOWN_ERROR;
  provider_->TerminateRoute(
      route->media_route_id(),
      base::BindLambdaForTesting(
          [&](const std::optional<std::string>&,
              mojom::RouteRequestResultCode code) { result_code = code; }));

  EXPECT_EQ(result_code, mojom::RouteRequestResultCode::OK);
  // Terminating the route destroys the host, tearing down the session.
  EXPECT_FALSE(provider_->host());
  run_loop.Run();
}

TEST_F(RedirectionMediaRouteProviderTest, TerminateUnknownRouteFails) {
  mojom::RouteRequestResultCode result_code = mojom::RouteRequestResultCode::OK;
  provider_->TerminateRoute(
      "urn:x-org.chromium:media:route:unknown/redirection/source",
      base::BindLambdaForTesting(
          [&](const std::optional<std::string>&,
              mojom::RouteRequestResultCode code) { result_code = code; }));

  EXPECT_EQ(result_code, mojom::RouteRequestResultCode::ROUTE_NOT_FOUND);
}

}  // namespace media_router
