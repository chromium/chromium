// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/mojo/media_router_debugger_impl.h"

#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/media/router/mojo/media_router_mojo_impl.h"
#include "chrome/browser/media/router/test/provider_test_helpers.h"
#include "chrome/test/base/testing_profile.h"
#include "components/media_router/browser/media_router_debugger.h"
#include "components/media_router/browser/test/mock_media_router.h"
#include "components/media_router/common/issue.h"
#include "components/media_router/common/media_route.h"
#include "components/media_router/common/media_source.h"
#include "content/public/test/browser_task_environment.h"
#include "media/base/media_switches.h"
#include "media/cast/constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace media_router {

namespace {

const char kDescription[] = "description";
const char kSource[] = "source1";
const char kMirroringSource[] = "urn:x-org.chromium.media:source:tab:*";
const char kRouteId[] = "routeId";
const char kSinkId[] = "sink";

// Creates a media route whose ID is |kRouteId|.
MediaRoute CreateMediaRoute() {
  MediaRoute route(kRouteId, MediaSource(kSource), kSinkId, kDescription, true);
  route.set_controller_type(RouteControllerType::kGeneric);
  return route;
}

MediaRoute CreateTabMirroringMediaRoute() {
  MediaRoute route(kRouteId, MediaSource(kMirroringSource), kSinkId,
                   kDescription, true);
  route.set_controller_type(RouteControllerType::kMirroring);
  return route;
}

class StubMediaRouterMojoImpl : public MediaRouterMojoImpl {
 public:
  explicit StubMediaRouterMojoImpl(content::BrowserContext* context)
      : MediaRouterMojoImpl(context) {}
  ~StubMediaRouterMojoImpl() override = default;

  // media_router::MediaRouter:
  base::Value::Dict GetState() const override {
    NOTIMPLEMENTED();
    return base::Value::Dict();
  }

  void GetProviderState(
      mojom::MediaRouteProviderId provider_id,
      mojom::MediaRouteProvider::GetStateCallback callback) const override {
    NOTIMPLEMENTED();
  }
};

class MockMirroringStatsObserver
    : public MediaRouterDebugger::MirroringStatsObserver {
 public:
  MOCK_METHOD(void, OnMirroringStatsUpdated, (const base::Value::Dict&));
};

}  // namespace

class MediaRouterDebuggerImplTest : public ::testing::Test {
 public:
  MediaRouterDebuggerImplTest()
      : media_router_(std::make_unique<StubMediaRouterMojoImpl>(profile())),
        debugger_(std::make_unique<MediaRouterDebuggerImpl>(*router())) {}
  MediaRouterDebuggerImplTest(const MediaRouterDebuggerImplTest&) = delete;
  ~MediaRouterDebuggerImplTest() override = default;
  MediaRouterDebuggerImplTest& operator=(const MediaRouterDebuggerImplTest&) =
      delete;

 protected:
  void SetUp() override { debugger_->AddObserver(observer_); }
  void TearDown() override { debugger_->RemoveObserver(observer_); }

  void UpdateRoutes(const std::vector<MediaRoute>& routes) {
    debugger()->OnRoutesUpdated(routes);
  }

  Profile* profile() { return &profile_; }
  MediaRouterMojoImpl* router() { return media_router_.get(); }
  MediaRouterDebuggerImpl* debugger() { return debugger_.get(); }

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingProfile profile_;
  std::unique_ptr<MediaRouterMojoImpl> media_router_;
  std::unique_ptr<MediaRouterDebuggerImpl> debugger_;

  testing::NiceMock<MockMirroringStatsObserver> observer_;
};

TEST_F(MediaRouterDebuggerImplTest, ReportsNotEnabled) {
  const std::vector<MediaRoute> routes{CreateMediaRoute()};
  EXPECT_CALL(observer_, OnMirroringStatsUpdated(_)).Times(0);
  UpdateRoutes(routes);
}

TEST_F(MediaRouterDebuggerImplTest, NonMirroringRoutes) {
  debugger()->EnableRtcpReports();

  const std::vector<MediaRoute> routes{CreateMediaRoute()};
  EXPECT_CALL(observer_, OnMirroringStatsUpdated(_)).Times(0);
  UpdateRoutes(routes);
}

TEST_F(MediaRouterDebuggerImplTest, FetchMirroringStats) {
  debugger()->EnableRtcpReports();

  const std::vector<MediaRoute> routes{CreateTabMirroringMediaRoute()};
  EXPECT_CALL(observer_, OnMirroringStatsUpdated(_)).Times(1);

  // Add a mirroring route and fast forward enough to trigger one loop of
  // mirroing stats fetch.
  UpdateRoutes(routes);
  task_environment_.FastForwardBy(base::Seconds(5) +
                                  media::cast::kRtcpReportInterval);

  EXPECT_CALL(observer_, OnMirroringStatsUpdated(_)).Times(0);
  // Remove the route after one loop has occurred to verify that fetching stops.
  UpdateRoutes({});
  task_environment_.RunUntilIdle();
}

}  // namespace media_router
