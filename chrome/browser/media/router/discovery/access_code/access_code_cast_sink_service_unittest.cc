// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/access_code/access_code_cast_sink_service.h"

#include <string>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/simple_test_clock.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/timer/mock_timer.h"
#include "chrome/browser/media/router/chrome_media_router_factory.h"
#include "chrome/browser/media/router/discovery/discovery_network_monitor.h"
#include "chrome/browser/media/router/discovery/mdns/cast_media_sink_service_impl.h"
#include "chrome/browser/media/router/discovery/mdns/cast_media_sink_service_test_helpers.h"
#include "chrome/browser/media/router/discovery/mdns/media_sink_util.h"
#include "chrome/browser/media/router/discovery/media_sink_discovery_metrics.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/media/router/test/provider_test_helpers.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/cast_channel/cast_socket.h"
#include "components/cast_channel/cast_socket_service.h"
#include "components/cast_channel/cast_test_util.h"
#include "components/media_router/browser/media_router_factory.h"
#include "components/media_router/browser/test/mock_media_router.h"
#include "components/media_router/common/discovery/media_sink_service_base.h"
#include "components/media_router/common/test/test_helper.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Return;

namespace media_router {

namespace {
MediaRoute CreateRouteForTesting(const MediaSinkInternal& sink) {
  std::string sink_id = sink.id();
  std::string route_id =
      "urn:x-org.chromium:media:route:1/" + sink_id + "/http://foo.com";
  return MediaRoute(route_id, MediaSource("access_code"), sink_id,
                    "access_sink", true);
}
}  // namespace

using SinkSource = CastDeviceCountMetrics::SinkSource;
using MockBoolCallback = base::MockCallback<base::OnceCallback<void(bool)>>;

class AccessCodeCastSinkServiceTest : public testing::Test {
 public:
  AccessCodeCastSinkServiceTest()
      : mock_time_task_runner_(new base::TestMockTimeTaskRunner()),
        mock_cast_socket_service_(
            std::make_unique<cast_channel::MockCastSocketService>(
                (mock_time_task_runner_))),
        message_handler_(mock_cast_socket_service_.get()),
        cast_media_sink_service_impl_(
            std::make_unique<MockCastMediaSinkServiceImpl>(
                OnSinksDiscoveredCallback(),
                mock_cast_socket_service_.get(),
                DiscoveryNetworkMonitor::GetInstance(),
                &dual_media_sink_service_)) {
    mock_cast_socket_service_->SetTaskRunnerForTest(mock_time_task_runner_);
  }
  AccessCodeCastSinkServiceTest(AccessCodeCastSinkServiceTest&) = delete;
  AccessCodeCastSinkServiceTest& operator=(AccessCodeCastSinkServiceTest&) =
      delete;

  void SetUp() override {
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
    profile_ = profile_manager()->CreateTestingProfile("foo_email");

    router_ = std::make_unique<NiceMock<media_router::MockMediaRouter>>();
    logger_ = std::make_unique<LoggerImpl>();
    ON_CALL(*router_, GetLogger()).WillByDefault(Return(logger_.get()));

    access_code_cast_sink_service_ =
        base::WrapUnique(new AccessCodeCastSinkService(
            profile_, router_.get(), cast_media_sink_service_impl_.get()));
    access_code_cast_sink_service_->SetTaskRunnerForTest(
        mock_time_task_runner_);
  }

  void TearDown() override {
    profile_manager_->DeleteAllTestingProfiles();
    profile_manager_.reset();
    access_code_cast_sink_service_.reset();
    router_.reset();
    task_environment_.RunUntilIdle();
    content::RunAllTasksUntilIdle();
  }

  MockCastMediaSinkServiceImpl* mock_cast_media_sink_service_impl() {
    return cast_media_sink_service_impl_.get();
  }

  base::TestMockTimeTaskRunner* mock_time_task_runner() {
    return mock_time_task_runner_.get();
  }
  TestingProfileManager* profile_manager() { return profile_manager_.get(); }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  std::unique_ptr<media_router::MockMediaRouter> router_;
  std::unique_ptr<LoggerImpl> logger_;

  raw_ptr<TestingProfile> profile_;
  scoped_refptr<base::TestMockTimeTaskRunner> mock_time_task_runner_;

  base::MockCallback<OnSinksDiscoveredCallback> mock_sink_discovered_cb_;

  TestMediaSinkService dual_media_sink_service_;
  std::unique_ptr<cast_channel::MockCastSocketService>
      mock_cast_socket_service_;
  raw_ptr<base::MockOneShotTimer> mock_timer_;
  testing::NiceMock<cast_channel::MockCastMessageHandler> message_handler_;
  std::unique_ptr<MockCastMediaSinkServiceImpl> cast_media_sink_service_impl_;
  std::unique_ptr<AccessCodeCastSinkService> access_code_cast_sink_service_;
};

TEST_F(AccessCodeCastSinkServiceTest,
       AccessCodeCastDeviceRemovedAfterRouteEnds) {
  // Test to see that an AccessCode cast sink will be removed after the session
  // is ended.
  mock_time_task_runner()->FastForwardUntilNoTasksRemain();

  // Add a non-access code cast sink to media router and route list.
  MediaSinkInternal cast_sink1 = CreateCastSink(1);
  MediaRoute media_route_cast = CreateRouteForTesting(cast_sink1);
  std::vector<MediaRoute> route_list = {media_route_cast};

  // Expect that no Task is posted since no routes were removed.
  access_code_cast_sink_service_->media_routes_observer_->OnRoutesUpdated(
      route_list);
  EXPECT_EQ(0u, mock_time_task_runner()->GetPendingTaskCount());

  // Add a cast sink discovered by access code to the list of routes.
  MediaSinkInternal access_code_sink2 = CreateCastSink(2);
  access_code_sink2.cast_data().discovered_by_access_code = true;
  MediaRoute media_route_access = CreateRouteForTesting(access_code_sink2);

  route_list.push_back(media_route_access);

  // Expect that no Task is posted since no routes were removed.
  access_code_cast_sink_service_->media_routes_observer_->OnRoutesUpdated(
      route_list);
  EXPECT_EQ(0u, mock_time_task_runner()->GetPendingTaskCount());

  // Remove the non-access code sink from the list of routes.
  route_list.erase(
      std::remove(route_list.begin(), route_list.end(), media_route_cast),
      route_list.end());
  access_code_cast_sink_service_->media_routes_observer_->OnRoutesUpdated(
      route_list);
  EXPECT_EQ(1u, mock_time_task_runner()->GetPendingTaskCount());
  mock_time_task_runner()->FastForwardUntilNoTasksRemain();

  // Expect cast sink is NOT removed from the media router since it
  // is not an access code sink.
  access_code_cast_sink_service_->HandleMediaRouteDiscoveredByAccessCode(
      &cast_sink1);
  EXPECT_CALL(*mock_cast_media_sink_service_impl(),
              DisconnectAndRemoveSink(cast_sink1))
      .Times(0);

  // Remove the access code sink from the list of routes.
  route_list.erase(
      std::remove(route_list.begin(), route_list.end(), media_route_access),
      route_list.end());

  access_code_cast_sink_service_->media_routes_observer_->OnRoutesUpdated(
      route_list);
  EXPECT_EQ(1u, mock_time_task_runner()->GetPendingTaskCount());
  mock_time_task_runner()->FastForwardUntilNoTasksRemain();

  access_code_cast_sink_service_->HandleMediaRouteDiscoveredByAccessCode(
      &access_code_sink2);
  // Expect that there is a pending attempt to examine the sink to see if it
  // should be expired.
  EXPECT_EQ(1u, mock_time_task_runner()->GetPendingTaskCount());

  EXPECT_CALL(*mock_cast_media_sink_service_impl(),
              DisconnectAndRemoveSink(access_code_sink2));
  mock_time_task_runner()->FastForwardUntilNoTasksRemain();
}

TEST_F(AccessCodeCastSinkServiceTest, AddExistingSinkToMediaRouter) {
  // Ensure that the call to OpenChannel is NOT made since the cast sink already
  // exists in the media router.
  MockBoolCallback mock_callback;
  MediaSinkInternal cast_sink1 = CreateCastSink(1);

  EXPECT_CALL(*mock_cast_media_sink_service_impl(),
              OpenChannel(cast_sink1, _, SinkSource::kAccessCode, _, _))
      .Times(0);
  EXPECT_CALL(mock_callback, Run(true));
  access_code_cast_sink_service_->OpenChannelIfNecessary(
      cast_sink1, mock_callback.Get(), true);
  mock_time_task_runner()->FastForwardUntilNoTasksRemain();
}

TEST_F(AccessCodeCastSinkServiceTest, AddExistingSinkToMediaRouterWithRoute) {
  // When an existing sink has an existing route, ensure that that route is
  // terminated before the caller is alerted to the successful discovery.
  MediaSinkInternal cast_sink1 = CreateCastSink(1);
  cast_sink1.cast_data().discovered_by_access_code = true;

  MediaRoute media_route_cast = CreateRouteForTesting(cast_sink1);

  access_code_cast_sink_service_->media_routes_observer_->OnRoutesUpdated(
      {media_route_cast});
  mock_time_task_runner()->FastForwardUntilNoTasksRemain();

  EXPECT_CALL(*router_, GetCurrentRoutes())
      .WillOnce(Return(std::vector<MediaRoute>{media_route_cast}));
  EXPECT_CALL(*router_, TerminateRoute(media_route_cast.media_route_id()));
  EXPECT_CALL(*mock_cast_media_sink_service_impl(),
              OpenChannel(cast_sink1, _, SinkSource::kAccessCode, _, _))
      .Times(0);

  MockBoolCallback mock_callback;
  EXPECT_CALL(mock_callback, Run(true));

  access_code_cast_sink_service_->OpenChannelIfNecessary(
      cast_sink1, mock_callback.Get(), true);

  access_code_cast_sink_service_->media_routes_observer_->OnRoutesUpdated({});
  // Since a route has been removed, there should be a pending task to examine
  // whether the route's sink is an access code sink.
  EXPECT_EQ(1u, mock_time_task_runner()->GetPendingTaskCount());

  access_code_cast_sink_service_->HandleMediaRouteDiscoveredByAccessCode(
      &cast_sink1);
  mock_time_task_runner()->FastForwardUntilNoTasksRemain();
}

TEST_F(AccessCodeCastSinkServiceTest, AddNewSinkToMediaRouter) {
  // Make sure that the sink is added to the media router if it does not already
  // exist.
  MockBoolCallback mock_callback;
  MediaSinkInternal cast_sink1 = CreateCastSink(1);

  EXPECT_CALL(*mock_cast_media_sink_service_impl(),
              OpenChannel(cast_sink1, _, SinkSource::kAccessCode, _, _));
  EXPECT_CALL(mock_callback, Run(true)).Times(0);
  access_code_cast_sink_service_->OpenChannelIfNecessary(
      cast_sink1, mock_callback.Get(), false);
  mock_time_task_runner()->FastForwardUntilNoTasksRemain();
}

}  // namespace media_router
