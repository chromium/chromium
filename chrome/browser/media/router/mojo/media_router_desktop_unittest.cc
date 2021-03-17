// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/mojo/media_router_desktop.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <utility>

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/media/router/event_page_request_manager.h"
#include "chrome/browser/media/router/event_page_request_manager_factory.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/media/router/test/media_router_mojo_test.h"
#include "chrome/browser/media/router/test/provider_test_helpers.h"
#include "components/media_router/common/media_source.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Invoke;
using testing::Mock;
using testing::Return;

namespace media_router {

namespace {

const char kOrigin[] = "http://origin/";
const char kRouteId[] = "routeId";
const char kSource[] = "source1";

class NullMessageObserver : public RouteMessageObserver {
 public:
  NullMessageObserver(MediaRouter* router, const MediaRoute::Id& route_id)
      : RouteMessageObserver(router, route_id) {}
  ~NullMessageObserver() final {}

  void OnMessagesReceived(const std::vector<mojom::RouteMessagePtr>) final {}
};

}  // namespace

class MediaRouterDesktopTest : public MediaRouterMojoTest {
 public:
  MediaRouterDesktopTest() {}
  ~MediaRouterDesktopTest() override {}

  DualMediaSinkService* media_sink_service() {
    return media_sink_service_.get();
  }

  MockCastMediaSinkService* cast_media_sink_service() {
    return cast_media_sink_service_;
  }

 protected:
  std::unique_ptr<MediaRouterMojoImpl> CreateMediaRouter() override {
    std::unique_ptr<MockCastMediaSinkService> cast_media_sink_service;
    // We disable the DIAL and Cast MRPs because initializing the MRPs requires
    // initialization of objects they depend on, which is outside the scope of
    // this unit test. MRP initialization is covered by Media Router browser
    // tests.
    feature_list_.InitWithFeatures(
        {}, /* disabled_features */ {kDialMediaRouteProvider,
                                     kCastMediaRouteProvider});
    cast_media_sink_service = std::make_unique<MockCastMediaSinkService>();
    cast_media_sink_service_ = cast_media_sink_service.get();
    media_sink_service_ =
        std::unique_ptr<DualMediaSinkService>(new DualMediaSinkService(
            std::move(cast_media_sink_service),
            std::make_unique<MockDialMediaSinkService>(),
            std::make_unique<MockCastAppDiscoveryService>()));
    return std::unique_ptr<MediaRouterDesktop>(
        new MediaRouterDesktop(profile(), media_sink_service_.get()));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<DualMediaSinkService> media_sink_service_;

  // Owned by |media_sink_service_|.
  MockCastMediaSinkService* cast_media_sink_service_;

  DISALLOW_COPY_AND_ASSIGN(MediaRouterDesktopTest);
};

#if defined(OS_WIN)
TEST_F(MediaRouterDesktopTest, EnableMdnsAfterEachRegister) {
  EXPECT_CALL(mock_extension_provider_, EnableMdnsDiscovery()).Times(0);
  EXPECT_CALL(*cast_media_sink_service(), StartMdnsDiscovery()).Times(0);
  RegisterExtensionProvider();
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(mock_extension_provider_, EnableMdnsDiscovery()).Times(0);
  EXPECT_CALL(*cast_media_sink_service(), StartMdnsDiscovery());
  router()->OnUserGesture();
  base::RunLoop().RunUntilIdle();

  // EnableMdnsDiscovery() is called on this RegisterExtensionProvider() because
  // we've already seen an mdns-enabling event.
  EXPECT_CALL(mock_extension_provider_, EnableMdnsDiscovery()).Times(0);
  EXPECT_CALL(*cast_media_sink_service(), StartMdnsDiscovery());
  RegisterExtensionProvider();
  base::RunLoop().RunUntilIdle();
}
#endif

TEST_F(MediaRouterDesktopTest, OnUserGesture) {
  EXPECT_CALL(mock_extension_provider_,
              UpdateMediaSinks(MediaSource::ForUnchosenDesktop().id()));
  router()->OnUserGesture();
  base::RunLoop().RunUntilIdle();
}

TEST_F(MediaRouterDesktopTest, SyncStateToMediaRouteProvider) {
  MediaSource media_source(kSource);
  std::unique_ptr<MockMediaSinksObserver> sinks_observer;
  std::unique_ptr<MockMediaRoutesObserver> routes_observer;
  std::unique_ptr<NullMessageObserver> messages_observer;
  ProvideTestRoute(MediaRouteProviderId::EXTENSION, kRouteId);

  router()->OnSinkAvailabilityUpdated(
      MediaRouteProviderId::EXTENSION,
      mojom::MediaRouter::SinkAvailability::PER_SOURCE);
  EXPECT_CALL(mock_extension_provider_,
              StartObservingMediaSinks(media_source.id()));
  sinks_observer = std::make_unique<MockMediaSinksObserver>(
      router(), media_source, url::Origin::Create(GURL(kOrigin)));
  EXPECT_TRUE(sinks_observer->Init());

  EXPECT_CALL(mock_extension_provider_,
              StartObservingMediaRoutes(media_source.id()));
  routes_observer =
      std::make_unique<MockMediaRoutesObserver>(router(), media_source.id());

  EXPECT_CALL(mock_extension_provider_,
              StartListeningForRouteMessages(kRouteId));
  messages_observer = std::make_unique<NullMessageObserver>(router(), kRouteId);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(Mock::VerifyAndClearExpectations(&mock_extension_provider_));
}

// Tests that auto-join and Cast SDK join requests are routed to the extension
// MediaRouteProvider.
TEST_F(MediaRouterDesktopTest, SendCastJoinRequestsToExtension) {
  TestJoinRoute(kAutoJoinPresentationId);
  TestJoinRoute(kCastPresentationIdPrefix + std::string("123"));
}

TEST_F(MediaRouterDesktopTest, ExtensionMrpRecoversFromConnectionError) {
  MediaRouterDesktop* media_router_desktop =
      static_cast<MediaRouterDesktop*>(router());
  auto* extension_mrp_proxy =
      media_router_desktop->extension_provider_proxy_.get();
  // |media_router_desktop| detects connection error and reconnects with
  // |extension_mrp_proxy|.
  for (int i = 0; i < MediaRouterDesktop::kMaxMediaRouteProviderErrorCount;
       i++) {
    ignore_result(extension_mrp_proxy->receiver_.Unbind());
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(extension_mrp_proxy->receiver_.is_bound());
  }
  ignore_result(extension_mrp_proxy->receiver_.Unbind());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(extension_mrp_proxy->receiver_.is_bound());
}

}  // namespace media_router
