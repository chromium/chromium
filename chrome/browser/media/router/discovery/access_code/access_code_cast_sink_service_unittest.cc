// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/access_code/access_code_cast_sink_service.h"

#include <string>
#include <utility>
#include <vector>

#include "base/json/values_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/timer/mock_timer.h"
#include "chrome/browser/media/router/chrome_media_router_factory.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_cast_feature.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_cast_pref_updater_impl.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_media_sink_util.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_test_util.h"
#include "chrome/browser/media/router/discovery/discovery_network_monitor.h"
#include "chrome/browser/media/router/discovery/mdns/cast_media_sink_service_impl.h"
#include "chrome/browser/media/router/discovery/mdns/cast_media_sink_service_test_helpers.h"
#include "chrome/browser/media/router/discovery/mdns/media_sink_util.h"
#include "chrome/browser/media/router/discovery/media_sink_discovery_metrics.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/media/router/test/provider_test_helpers.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/media_router/browser/media_router_factory.h"
#include "components/media_router/browser/test/mock_media_router.h"
#include "components/media_router/common/discovery/media_sink_service_base.h"
#include "components/media_router/common/providers/cast/channel/cast_socket.h"
#include "components/media_router/common/providers/cast/channel/cast_socket_service.h"
#include "components/media_router/common/providers/cast/channel/cast_test_util.h"
#include "components/media_router/common/test/test_helper.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_utils.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Eq;
using ::testing::Return;

namespace media_router {

using SinkSource = CastDeviceCountMetrics::SinkSource;
using MockBoolCallback = base::MockCallback<base::OnceCallback<void(bool)>>;
using MockAddSinkResultCallback = base::MockCallback<
    media_router::AccessCodeCastSinkService::AddSinkResultCallback>;
using DiscoveryDevice = chrome_browser_media::proto::DiscoveryDevice;

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
                mock_sink_discovered_cb_.Get(),
                mock_cast_socket_service_.get(),
                discovery_network_monitor_.get(),
                &dual_media_sink_service_)) {
    mock_cast_socket_service_->SetTaskRunnerForTest(mock_time_task_runner_);
  }
  AccessCodeCastSinkServiceTest(AccessCodeCastSinkServiceTest&) = delete;
  AccessCodeCastSinkServiceTest& operator=(AccessCodeCastSinkServiceTest&) =
      delete;

  void SetUp() override {
    content::SetNetworkConnectionTrackerForTesting(
        network::TestNetworkConnectionTracker::GetInstance());
    network::TestNetworkConnectionTracker::GetInstance()->SetConnectionType(
        network::mojom::ConnectionType::CONNECTION_WIFI);

    feature_list_.InitWithFeatures({features::kAccessCodeCastRememberDevices},
                                   {});
    GetTestingPrefs()->SetManagedPref(::prefs::kEnableMediaRouter,
                                      std::make_unique<base::Value>(true));
    GetTestingPrefs()->SetManagedPref(prefs::kAccessCodeCastEnabled,
                                      std::make_unique<base::Value>(true));

    router_ = std::make_unique<NiceMock<media_router::MockMediaRouter>>();
    logger_ = std::make_unique<LoggerImpl>();
    ON_CALL(*router_, GetLogger()).WillByDefault(Return(logger_.get()));

    access_code_cast_sink_service_ =
        base::WrapUnique(new AccessCodeCastSinkService(
            &profile_, router_.get(), cast_media_sink_service_impl_.get(),
            discovery_network_monitor_.get(), GetTestingPrefs()));
    access_code_cast_sink_service_->SetTaskRunnerForTest(
        mock_time_task_runner_);
    mock_time_task_runner()->FastForwardUntilNoTasksRemain();
    task_environment_.RunUntilIdle();
    content::RunAllTasksUntilIdle();
  }

  void TearDown() override {
    access_code_cast_sink_service_->Shutdown();
    access_code_cast_sink_service_.reset();
    router_.reset();
    mock_time_task_runner()->ClearPendingTasks();
    task_environment_.RunUntilIdle();
    content::RunAllTasksUntilIdle();
    fake_network_info_ = fake_ethernet_info_;
  }

  void FastForwardUiAndIoTasks() {
    mock_time_task_runner()->FastForwardUntilNoTasksRemain();
    task_environment_.RunUntilIdle();
  }

  void SetDeviceDurationPrefForTest(const base::TimeDelta duration_time) {
    GetTestingPrefs()->SetUserPref(
        prefs::kAccessCodeCastDeviceDuration,
        base::Value(static_cast<int>(duration_time.InSeconds())));
  }

  MockCastMediaSinkServiceImpl* mock_cast_media_sink_service_impl() {
    return cast_media_sink_service_impl_.get();
  }

  base::TestMockTimeTaskRunner* mock_time_task_runner() {
    return mock_time_task_runner_.get();
  }

  sync_preferences::TestingPrefServiceSyncable* GetTestingPrefs() {
    return profile_.GetTestingPrefService();
  }

  void ChangeConnectionType(network::mojom::ConnectionType connection_type) {
    discovery_network_monitor_->OnConnectionChanged(connection_type);
  }

  void ExpectOpenChannels(std::vector<MediaSinkInternal> cast_sinks,
                          int num_times) {
    for (auto sink : cast_sinks) {
      EXPECT_CALL(*mock_cast_media_sink_service_impl(),
                  OpenChannel(sink, _, SinkSource::kAccessCode, _, _))
          .Times(num_times);
    }
  }

  void ExpectHasSink(std::vector<MediaSinkInternal> cast_sinks, int num_times) {
    for (auto sink : cast_sinks) {
      EXPECT_CALL(*mock_cast_media_sink_service_impl(), HasSink(sink.id()))
          .Times(num_times);
    }
  }

 protected:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<media_router::MockMediaRouter> router_;
  std::unique_ptr<LoggerImpl> logger_;

  base::test::ScopedFeatureList feature_list_;

  static std::vector<DiscoveryNetworkInfo> fake_network_info_;

  static const std::vector<DiscoveryNetworkInfo> fake_ethernet_info_;
  static const std::vector<DiscoveryNetworkInfo> fake_wifi_info_;
  static const std::vector<DiscoveryNetworkInfo> fake_unknown_info_;

  static std::vector<DiscoveryNetworkInfo> FakeGetNetworkInfo() {
    return fake_network_info_;
  }

  std::unique_ptr<DiscoveryNetworkMonitor> discovery_network_monitor_ =
      DiscoveryNetworkMonitor::CreateInstanceForTest(&FakeGetNetworkInfo);

  TestingProfile profile_;
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

// static
const std::vector<DiscoveryNetworkInfo>
    AccessCodeCastSinkServiceTest::fake_ethernet_info_ = {
        DiscoveryNetworkInfo{std::string("enp0s2"), std::string("ethernet1")}};
// static
const std::vector<DiscoveryNetworkInfo>
    AccessCodeCastSinkServiceTest::fake_wifi_info_ = {
        DiscoveryNetworkInfo{std::string("wlp3s0"), std::string("wifi1")},
        DiscoveryNetworkInfo{std::string("wlp3s1"), std::string("wifi2")}};
// static
const std::vector<DiscoveryNetworkInfo>
    AccessCodeCastSinkServiceTest::fake_unknown_info_ = {
        DiscoveryNetworkInfo{std::string("enp0s2"), std::string()}};

// static
std::vector<DiscoveryNetworkInfo>
    AccessCodeCastSinkServiceTest::fake_network_info_ =
        AccessCodeCastSinkServiceTest::fake_ethernet_info_;

TEST_F(AccessCodeCastSinkServiceTest,
       AccessCodeCastDeviceRemovedAfterRouteEndsExpirationEnabled) {
  // Test to see that an AccessCode cast sink will be removed after the session
  // is ended.
  mock_time_task_runner()->FastForwardUntilNoTasksRemain();
  SetDeviceDurationPrefForTest(base::Seconds(10));

  // Add a non-access code cast sink to media router and route list.
  MediaSinkInternal cast_sink1 = CreateCastSink(1);
  MediaRoute media_route_cast = CreateRouteForTesting(cast_sink1.id());
  std::vector<MediaRoute> route_list = {media_route_cast};

  // Expect that the removed_route_id_ member variable has not changes since no
  // route was removed.
  access_code_cast_sink_service_->media_routes_observer_->OnRoutesUpdated(
      route_list);
  EXPECT_TRUE(access_code_cast_sink_service_->media_routes_observer_
                  ->removed_route_id_.empty());

  // Add a cast sink discovered by access code to the list of routes.
  MediaSinkInternal access_code_sink2 = CreateCastSink(2);
  access_code_sink2.cast_data().discovery_type =
      CastDiscoveryType::kAccessCodeManualEntry;
  MediaRoute media_route_access = CreateRouteForTesting(access_code_sink2.id());

  route_list.push_back(media_route_access);

  mock_cast_media_sink_service_impl()->AddSinkForTest(access_code_sink2);
  access_code_cast_sink_service_->SetExpirationTimer(&access_code_sink2);
  access_code_cast_sink_service_->StoreSinkInPrefs(&access_code_sink2);
  mock_time_task_runner()->FastForwardUntilNoTasksRemain();

  // Expect that the removed_route_id_ member variable has not changes since no
  // route was removed.
  access_code_cast_sink_service_->media_routes_observer_->OnRoutesUpdated(
      route_list);
  EXPECT_TRUE(access_code_cast_sink_service_->media_routes_observer_
                  ->removed_route_id_.empty());

  // Remove the non-access code sink from the list of routes.
  route_list.erase(
      std::remove(route_list.begin(), route_list.end(), media_route_cast),
      route_list.end());
  access_code_cast_sink_service_->media_routes_observer_->OnRoutesUpdated(
      route_list);
  EXPECT_EQ(
      access_code_cast_sink_service_->media_routes_observer_->removed_route_id_,
      media_route_cast.media_route_id());
  mock_time_task_runner()->FastForwardUntilNoTasksRemain();

  // Expect cast sink is NOT removed from the media router since it
  // is not an access code sink.
  access_code_cast_sink_service_->HandleMediaRouteRemovedByAccessCode(
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
  EXPECT_EQ(
      access_code_cast_sink_service_->media_routes_observer_->removed_route_id_,
      media_route_access.media_route_id());
  mock_time_task_runner()->FastForwardUntilNoTasksRemain();

  // Expire the access code cast sink while the route is still active.
  task_environment_.AdvanceClock(base::Seconds(100));

  access_code_cast_sink_service_->HandleMediaRouteRemovedByAccessCode(
      &access_code_sink2);

  EXPECT_CALL(*mock_cast_media_sink_service_impl(),
              DisconnectAndRemoveSink(access_code_sink2));

  mock_time_task_runner()->FastForwardUntilNoTasksRemain();
}

TEST_F(AccessCodeCastSinkServiceTest, AddExistingSinkToMediaRouter) {
  // Ensure that the call to OpenChannel is NOT made since the cast sink already
  // exists in the media router.
  MockAddSinkResultCallback mock_callback;
  MediaSinkInternal cast_sink1 = CreateCastSink(1);
  auto cast_data = cast_sink1.cast_data();
  cast_data.discovery_type = CastDiscoveryType::kAccessCodeManualEntry;
  cast_sink1.set_cast_data(cast_data);

  EXPECT_CALL(*mock_cast_media_sink_service_impl(),
              OpenChannel(cast_sink1, _, SinkSource::kAccessCode, _, _))
      .Times(0);
  EXPECT_CALL(mock_callback, Run(AddSinkResultCode::OK, Eq(cast_sink1.id())));
  access_code_cast_sink_service_->OpenChannelIfNecessary(
      cast_sink1, mock_callback.Get(), true);
  mock_time_task_runner()->FastForwardUntilNoTasksRemain();
}

TEST_F(AccessCodeCastSinkServiceTest, AddNewSinkToMediaRouter) {
  // Make sure that the sink is added to the media router if it does not already
  // exist.
  MockAddSinkResultCallback mock_callback;
  MediaSinkInternal cast_sink1 = CreateCastSink(1);
  auto cast_data = cast_sink1.cast_data();
  cast_data.discovery_type = CastDiscoveryType::kAccessCodeManualEntry;
  cast_sink1.set_cast_data(cast_data);

  EXPECT_CALL(*mock_cast_media_sink_service_impl(),
              OpenChannel(cast_sink1, _, SinkSource::kAccessCode, _, _));
  EXPECT_CALL(mock_callback, Run(_, _)).Times(0);
  access_code_cast_sink_service_->OpenChannelIfNecessary(
      cast_sink1, mock_callback.Get(), false);
  mock_time_task_runner()->FastForwardUntilNoTasksRemain();
}

TEST_F(AccessCodeCastSinkServiceTest, DiscoveryDeviceMissingWithOk) {
  // Test to ensure that the add_sink_callback returns an EMPTY_RESPONSE if the
  // the device is missing.
  MockAddSinkResultCallback mock_callback;
  EXPECT_CALL(mock_callback,
              Run(AddSinkResultCode::EMPTY_RESPONSE, Eq(absl::nullopt)));
  access_code_cast_sink_service_->OnAccessCodeValidated(
      mock_callback.Get(), absl::nullopt, AddSinkResultCode::OK);
}

TEST_F(AccessCodeCastSinkServiceTest, ValidDiscoveryDeviceAndCode) {
  // If discovery device is present, formatted correctly, and code is OK, no
  // callback should be run during OnAccessCodeValidated. Instead when the
  // channel opens successfully the callback should run with OK.
  MockAddSinkResultCallback mock_callback;
  MediaSinkInternal cast_sink1 = CreateCastSink(1);

  DiscoveryDevice discovery_device_proto =
      media_router::BuildDiscoveryDeviceProto();
  discovery_device_proto.set_id("id1");

  EXPECT_CALL(mock_callback, Run(AddSinkResultCode::OK, _));
  EXPECT_CALL(*mock_cast_media_sink_service_impl(), HasSink(_));
  EXPECT_CALL(*mock_cast_media_sink_service_impl(),
              OpenChannel(_, _, SinkSource::kAccessCode, _, _));
  access_code_cast_sink_service_->OnAccessCodeValidated(
      mock_callback.Get(), discovery_device_proto, AddSinkResultCode::OK);

  // Channel successfully opens.
  access_code_cast_sink_service_->OnChannelOpenedResult(mock_callback.Get(),
                                                        "123456", true);
  mock_time_task_runner()->FastForwardUntilNoTasksRemain();
  FastForwardUiAndIoTasks();
  mock_time_task_runner()->FastForwardUntilNoTasksRemain();
}

TEST_F(AccessCodeCastSinkServiceTest, InvalidDiscoveryDevice) {
  // If discovery device is present, but formatted incorrectly, and code is OK,
  // then callback should be SINK_CREATION_ERROR.
  MockAddSinkResultCallback mock_callback;

  // Create discovery_device with an invalid port
  DiscoveryDevice discovery_device_proto =
      media_router::BuildDiscoveryDeviceProto("foo_display_name", "1234",
                                              "```````23489:1238:1239");

  EXPECT_CALL(mock_callback,
              Run(AddSinkResultCode::SINK_CREATION_ERROR, Eq(absl::nullopt)));
  access_code_cast_sink_service_->OnAccessCodeValidated(
      mock_callback.Get(), discovery_device_proto, AddSinkResultCode::OK);
}

TEST_F(AccessCodeCastSinkServiceTest, NonOKResultCode) {
  // Check to see that any result code that isn't OK will return that error.
  MockAddSinkResultCallback mock_callback;

  EXPECT_CALL(mock_callback,
              Run(AddSinkResultCode::AUTH_ERROR, Eq(absl::nullopt)));
  access_code_cast_sink_service_->OnAccessCodeValidated(
      mock_callback.Get(), absl::nullopt, AddSinkResultCode::AUTH_ERROR);
}

TEST_F(AccessCodeCastSinkServiceTest, OnChannelOpenedSuccess) {
  // Validate callback calls for OnChannelOpened for success.
  MockAddSinkResultCallback mock_callback;

  EXPECT_CALL(mock_callback, Run(AddSinkResultCode::OK, Eq("123456")));
  access_code_cast_sink_service_->OnChannelOpenedResult(mock_callback.Get(),
                                                        "123456", true);
}

TEST_F(AccessCodeCastSinkServiceTest, OnChannelOpenedFailure) {
  // Validate callback calls for OnChannelOpened for failure.
  MockAddSinkResultCallback mock_callback;
  EXPECT_CALL(mock_callback,
              Run(AddSinkResultCode::CHANNEL_OPEN_ERROR, Eq(absl::nullopt)));
  access_code_cast_sink_service_->OnChannelOpenedResult(mock_callback.Get(),
                                                        "123456", false);
}

TEST_F(AccessCodeCastSinkServiceTest, SinkDoesntExistForPrefs) {
  // Ensure that the StoreSinkInPrefs() function returns if no sink exists in
  // the media router and no tasks are posted.
  access_code_cast_sink_service_->StoreSinkInPrefs(nullptr);
  mock_time_task_runner()->FastForwardUntilNoTasksRemain();

  EXPECT_TRUE(
      access_code_cast_sink_service_->pref_updater_->GetDeviceAddedTimeDict()
          .empty());
  EXPECT_TRUE(
      access_code_cast_sink_service_->pref_updater_->GetDevicesDict().empty());
}

TEST_F(AccessCodeCastSinkServiceTest, TestFetchAndAddStoredDevices) {
  // Test that ensures OpenChannels is called after valid sinks are fetched from
  // the internal pref service.
  const MediaSinkInternal cast_sink1 = CreateCastSink(1);
  const MediaSinkInternal cast_sink2 = CreateCastSink(2);
  const MediaSinkInternal cast_sink3 = CreateCastSink(3);

  access_code_cast_sink_service_->StoreSinkInPrefs(&cast_sink1);
  access_code_cast_sink_service_->StoreSinkInPrefs(&cast_sink2);
  access_code_cast_sink_service_->StoreSinkInPrefs(&cast_sink3);

  // Initialize histogram tester so we can ensure metrics are being collected.
  base::HistogramTester histogram_tester;

  FastForwardUiAndIoTasks();

  std::vector<MediaSinkInternal> cast_sinks_ethernet;
  cast_sinks_ethernet.push_back(
      access_code_cast_sink_service_->ValidateDeviceFromSinkId(cast_sink1.id())
          .value());
  cast_sinks_ethernet.push_back(
      access_code_cast_sink_service_->ValidateDeviceFromSinkId(cast_sink2.id())
          .value());
  cast_sinks_ethernet.push_back(
      access_code_cast_sink_service_->ValidateDeviceFromSinkId(cast_sink3.id())
          .value());

  ExpectOpenChannels(cast_sinks_ethernet, 1);
  ExpectHasSink(cast_sinks_ethernet, 1);

  FastForwardUiAndIoTasks();

  access_code_cast_sink_service_->InitAllStoredDevices();

  // Test to ensure that the count of remembered devices was properly logged.
  histogram_tester.ExpectBucketCount(
      "AccessCodeCast.Discovery.RememberedDevicesCount", 3, 1);

  // GetNetworkId() is run on the IO thread, so we must run RunUntilIdle and
  // RunAllTasks for that task to finish before we can continue with the
  // mock_task_runner.
  FastForwardUiAndIoTasks();
  content::RunAllTasksUntilIdle();
  mock_time_task_runner()->FastForwardUntilNoTasksRemain();
  FastForwardUiAndIoTasks();
  mock_time_task_runner()->FastForwardUntilNoTasksRemain();
}

TEST_F(AccessCodeCastSinkServiceTest, TestChangeNetworksExpiration) {
  // Test that ensures sinks are stored on a network and then (attempted) to be
  // reopened when we connect back to any other network(including the original
  // one).

  mock_time_task_runner()->ClearPendingTasks();
  task_environment_.RunUntilIdle();
  content::RunAllTasksUntilIdle();
  SetDeviceDurationPrefForTest(base::Seconds(100));
  FastForwardUiAndIoTasks();

  const MediaSinkInternal cast_sink1 = CreateCastSink(1);
  const MediaSinkInternal cast_sink2 = CreateCastSink(2);
  const MediaSinkInternal cast_sink3 = CreateCastSink(3);

  mock_cast_media_sink_service_impl()->AddSinkForTest(cast_sink1);
  mock_cast_media_sink_service_impl()->AddSinkForTest(cast_sink2);
  mock_cast_media_sink_service_impl()->AddSinkForTest(cast_sink3);

  access_code_cast_sink_service_->StoreSinkInPrefs(&cast_sink1);
  access_code_cast_sink_service_->StoreSinkInPrefs(&cast_sink2);
  access_code_cast_sink_service_->StoreSinkInPrefs(&cast_sink3);

  FastForwardUiAndIoTasks();

  std::vector<MediaSinkInternal> cast_sinks_ethernet;
  cast_sinks_ethernet.push_back(
      access_code_cast_sink_service_->ValidateDeviceFromSinkId(cast_sink1.id())
          .value());
  cast_sinks_ethernet.push_back(
      access_code_cast_sink_service_->ValidateDeviceFromSinkId(cast_sink2.id())
          .value());
  cast_sinks_ethernet.push_back(
      access_code_cast_sink_service_->ValidateDeviceFromSinkId(cast_sink3.id())
          .value());

  // Overall this unit test should call OpenChannel for each cast sink twice.
  // This is on init stored devices and then connecting to a new network will
  // trigger a call to add every stored cast device back again.
  ExpectOpenChannels(cast_sinks_ethernet, 2);
  ExpectHasSink(cast_sinks_ethernet, 2);

  FastForwardUiAndIoTasks();

  access_code_cast_sink_service_->InitAllStoredDevices();

  FastForwardUiAndIoTasks();

  // 3 expiration timers should be set.
  EXPECT_EQ(
      access_code_cast_sink_service_->current_session_expiration_timers_.size(),
      3u);

  // Don't expire the devices yet, but let some time pass.
  task_environment_.AdvanceClock(base::Seconds(75));

  EXPECT_FALSE(
      access_code_cast_sink_service_->pref_updater_->GetDeviceAddedTimeDict()
          .empty());
  EXPECT_FALSE(
      access_code_cast_sink_service_->pref_updater_->GetDevicesDict().empty());

  // When the network changes, the sinks on that network should be removed. The
  // sinks should also be expired after the network changes and the expiration
  // is fully completed.
  EXPECT_CALL(*mock_cast_media_sink_service_impl(),
              DisconnectAndRemoveSink(cast_sink1))
      .Times(2);
  EXPECT_CALL(*mock_cast_media_sink_service_impl(),
              DisconnectAndRemoveSink(cast_sink2))
      .Times(2);
  EXPECT_CALL(*mock_cast_media_sink_service_impl(),
              DisconnectAndRemoveSink(cast_sink3))
      .Times(2);

  // Connect to a new network with different sinks.
  fake_network_info_ = fake_wifi_info_;
  ChangeConnectionType(network::mojom::ConnectionType::CONNECTION_WIFI);
  content::RunAllTasksUntilIdle();
  mock_time_task_runner()->FastForwardUntilNoTasksRemain();

  // 3 expiration timers should be set still.
  EXPECT_EQ(3u, access_code_cast_sink_service_
                    ->current_session_expiration_timers_.size());

  content::RunAllTasksUntilIdle();
  mock_time_task_runner_->FastForwardUntilNoTasksRemain();
  task_environment_.FastForwardBy(base::Seconds(50));
  mock_time_task_runner_->FastForwardUntilNoTasksRemain();
  content::RunAllTasksUntilIdle();
  FastForwardUiAndIoTasks();

  // Now all the expiration timers should be completed and the devices should be
  // removed.
  EXPECT_TRUE(
      access_code_cast_sink_service_->pref_updater_->GetDeviceAddedTimeDict()
          .empty());
  EXPECT_FALSE(
      access_code_cast_sink_service_->pref_updater_->GetDevicesDict().size());
}

TEST_F(AccessCodeCastSinkServiceTest, TestChangeNetworksNoExpiration) {
  // Test that ensures sinks are stored on a network and then (attempted) to be
  // reopened when we connect back to any other network(including the original
  // one). Don't include expiration in this test however

  mock_time_task_runner()->ClearPendingTasks();
  task_environment_.RunUntilIdle();
  content::RunAllTasksUntilIdle();
  SetDeviceDurationPrefForTest(base::Seconds(10000));
  FastForwardUiAndIoTasks();

  const MediaSinkInternal cast_sink1 = CreateCastSink(1);
  const MediaSinkInternal cast_sink2 = CreateCastSink(2);
  const MediaSinkInternal cast_sink3 = CreateCastSink(3);

  mock_cast_media_sink_service_impl()->AddSinkForTest(cast_sink1);
  mock_cast_media_sink_service_impl()->AddSinkForTest(cast_sink2);
  mock_cast_media_sink_service_impl()->AddSinkForTest(cast_sink3);

  access_code_cast_sink_service_->StoreSinkInPrefs(&cast_sink1);
  access_code_cast_sink_service_->StoreSinkInPrefs(&cast_sink2);
  access_code_cast_sink_service_->StoreSinkInPrefs(&cast_sink3);

  FastForwardUiAndIoTasks();

  std::vector<MediaSinkInternal> cast_sinks_ethernet;
  cast_sinks_ethernet.push_back(
      access_code_cast_sink_service_->ValidateDeviceFromSinkId(cast_sink1.id())
          .value());
  cast_sinks_ethernet.push_back(
      access_code_cast_sink_service_->ValidateDeviceFromSinkId(cast_sink2.id())
          .value());
  cast_sinks_ethernet.push_back(
      access_code_cast_sink_service_->ValidateDeviceFromSinkId(cast_sink3.id())
          .value());

  // Overall this unit test should call OpenChannel for each cast sink twice.
  // This is on init stored devices and then connecting to a new network will
  // trigger a call to add every stored cast device back again.
  ExpectOpenChannels(cast_sinks_ethernet, 2);
  ExpectHasSink(cast_sinks_ethernet, 2);

  FastForwardUiAndIoTasks();

  access_code_cast_sink_service_->InitAllStoredDevices();

  FastForwardUiAndIoTasks();

  // 3 expiration timers should be set.
  EXPECT_EQ(
      access_code_cast_sink_service_->current_session_expiration_timers_.size(),
      3u);

  // Don't expire the devices yet, but let some time pass.
  task_environment_.AdvanceClock(base::Seconds(75));

  EXPECT_FALSE(
      access_code_cast_sink_service_->pref_updater_->GetDeviceAddedTimeDict()
          .empty());
  EXPECT_FALSE(
      access_code_cast_sink_service_->pref_updater_->GetDevicesDict().empty());

  content::RunAllTasksUntilIdle();
  mock_time_task_runner()->FastForwardUntilNoTasksRemain();

  // When the network changes, the sinks on that network should be removed.
  EXPECT_CALL(*mock_cast_media_sink_service_impl(),
              DisconnectAndRemoveSink(cast_sink1));
  EXPECT_CALL(*mock_cast_media_sink_service_impl(),
              DisconnectAndRemoveSink(cast_sink2));
  EXPECT_CALL(*mock_cast_media_sink_service_impl(),
              DisconnectAndRemoveSink(cast_sink3));

  // Connect to a new network with different sinks.
  fake_network_info_ = fake_wifi_info_;
  ChangeConnectionType(network::mojom::ConnectionType::CONNECTION_WIFI);
  content::RunAllTasksUntilIdle();
  mock_time_task_runner_->FastForwardUntilNoTasksRemain();

  task_environment_.AdvanceClock(base::Seconds(75));

  // 3 expiration timers should be set still.
  EXPECT_EQ(3u, access_code_cast_sink_service_
                    ->current_session_expiration_timers_.size());

  content::RunAllTasksUntilIdle();
  mock_time_task_runner_->FastForwardUntilNoTasksRemain();
  task_environment_.FastForwardBy(base::Seconds(50));
  mock_time_task_runner_->FastForwardUntilNoTasksRemain();

  // The expiration should still not be triggered yet
  EXPECT_FALSE(
      access_code_cast_sink_service_->pref_updater_->GetDeviceAddedTimeDict()
          .empty());
  EXPECT_TRUE(
      access_code_cast_sink_service_->pref_updater_->GetDevicesDict().size());
}

TEST_F(AccessCodeCastSinkServiceTest,
       TestAddInvalidDevicesNoMediaSinkInternal) {
  // Test that to check that if a sink is not stored in the devies dict, no cal
  // wil be made to add devies to the router or to start expiration timers.
  const MediaSinkInternal cast_sink1 = CreateCastSink(1);
  access_code_cast_sink_service_->StoreSinkInPrefs(&cast_sink1);

  FastForwardUiAndIoTasks();

  std::vector<MediaSinkInternal> cast_sinks;
  cast_sinks.push_back(
      access_code_cast_sink_service_->ValidateDeviceFromSinkId(cast_sink1.id())
          .value());

  FastForwardUiAndIoTasks();

  // Remove the cast sink from the devices dict -- now the cast sink is
  // incompletely stored since it only exists in 2/3 of the prefs.
  access_code_cast_sink_service_->pref_updater_->RemoveSinkIdFromDevicesDict(
      cast_sink1.id());

  mock_time_task_runner()->FastForwardUntilNoTasksRemain();

  EXPECT_CALL(*mock_cast_media_sink_service_impl(),
              OpenChannels(cast_sinks, SinkSource::kAccessCode))
      .Times(0);

  EXPECT_FALSE(
      access_code_cast_sink_service_->pref_updater_->GetDeviceAddedTimeDict()
          .empty());
  EXPECT_TRUE(
      access_code_cast_sink_service_->pref_updater_->GetDevicesDict().empty());

  // Expect that the sink id is removed from all instance in the pref service
  // when we try to init connections with a corrupted device entry.
  access_code_cast_sink_service_->InitAllStoredDevices();

  FastForwardUiAndIoTasks();
  mock_time_task_runner()->FastForwardUntilNoTasksRemain();

  EXPECT_TRUE(access_code_cast_sink_service_->current_session_expiration_timers_
                  .empty());

  FastForwardUiAndIoTasks();
  content::RunAllTasksUntilIdle();
}

TEST_F(AccessCodeCastSinkServiceTest, TestCalculateDurationTillExpiration) {
  // Test that checks all variations of calculated duration.
  mock_time_task_runner()->FastForwardUntilNoTasksRemain();

  const MediaSinkInternal cast_sink1 = CreateCastSink(1);

  // Since there are no stored sinks, this should return 0 seconds.
  EXPECT_EQ(access_code_cast_sink_service_->CalculateDurationTillExpiration(
                cast_sink1.id()),
            base::Seconds(0));

  access_code_cast_sink_service_->StoreSinkInPrefs(&cast_sink1);
  FastForwardUiAndIoTasks();
  mock_time_task_runner()->FastForwardUntilNoTasksRemain();

  SetDeviceDurationPrefForTest(base::Seconds(100));

  // No mock time has passed since the start of the test, so the returned
  // duration should be the same as the set pref.
  EXPECT_EQ(access_code_cast_sink_service_->CalculateDurationTillExpiration(
                cast_sink1.id()),
            base::Seconds(100));

  task_environment_.FastForwardBy(base::Seconds(50));

  // 50 s mock time has passed since the start of the test, so the returned
  // duration should now be modified.
  EXPECT_EQ(access_code_cast_sink_service_->CalculateDurationTillExpiration(
                cast_sink1.id()),
            base::Seconds(50));

  task_environment_.FastForwardBy(base::Seconds(1000));

  // More mock time has passed then the pref, it should return 0 instead of a
  // negative number.
  EXPECT_EQ(access_code_cast_sink_service_->CalculateDurationTillExpiration(
                cast_sink1.id()),
            base::Seconds(0));
}

TEST_F(AccessCodeCastSinkServiceTest, TestSetExpirationTimer) {
  // Test to see that setting the expiration timer overwrites any timers that
  // are currently running.
  const MediaSinkInternal cast_sink1 = CreateCastSink(1);

  access_code_cast_sink_service_->StoreSinkInPrefs(&cast_sink1);
  FastForwardUiAndIoTasks();
  content::RunAllTasksUntilIdle();
  FastForwardUiAndIoTasks();

  SetDeviceDurationPrefForTest(base::Seconds(100));

  access_code_cast_sink_service_->SetExpirationTimer(&cast_sink1);
  EXPECT_TRUE(access_code_cast_sink_service_
                  ->current_session_expiration_timers_[cast_sink1.id()]
                  ->IsRunning());

  // The expiration should not have triggered yet.
  task_environment_.FastForwardBy(base::Seconds(75));

  EXPECT_FALSE(
      access_code_cast_sink_service_->pref_updater_->GetDeviceAddedTimeDict()
          .empty());
  EXPECT_FALSE(
      access_code_cast_sink_service_->pref_updater_->GetDevicesDict().empty());

  // Add the device again, the expiration timer should be reset and 150 seconds
  // passed will not reset the cast device.
  access_code_cast_sink_service_->StoreSinkInPrefs(&cast_sink1);
  FastForwardUiAndIoTasks();

  access_code_cast_sink_service_->SetExpirationTimer(&cast_sink1);
  EXPECT_TRUE(access_code_cast_sink_service_
                  ->current_session_expiration_timers_[cast_sink1.id()]
                  ->IsRunning());

  task_environment_.FastForwardBy(base::Seconds(75));

  EXPECT_FALSE(
      access_code_cast_sink_service_->pref_updater_->GetDeviceAddedTimeDict()
          .empty());
  EXPECT_FALSE(
      access_code_cast_sink_service_->pref_updater_->GetDevicesDict().empty());
  mock_time_task_runner()->FastForwardUntilNoTasksRemain();
}

TEST_F(AccessCodeCastSinkServiceTest, TestResetExpirationTimersNetworkChange) {
  // Test to check all expiration timers are restarted after network is changed.
  SetDeviceDurationPrefForTest(base::Seconds(10000));
  FastForwardUiAndIoTasks();

  const MediaSinkInternal cast_sink1 = CreateCastSink(1);
  const MediaSinkInternal cast_sink2 = CreateCastSink(2);
  const MediaSinkInternal cast_sink3 = CreateCastSink(3);

  access_code_cast_sink_service_->StoreSinkInPrefs(&cast_sink1);
  access_code_cast_sink_service_->StoreSinkInPrefs(&cast_sink2);
  access_code_cast_sink_service_->StoreSinkInPrefs(&cast_sink3);
  FastForwardUiAndIoTasks();
  content::RunAllTasksUntilIdle();
  FastForwardUiAndIoTasks();

  access_code_cast_sink_service_->SetExpirationTimer(&cast_sink1);
  access_code_cast_sink_service_->SetExpirationTimer(&cast_sink2);
  access_code_cast_sink_service_->SetExpirationTimer(&cast_sink3);

  EXPECT_TRUE(access_code_cast_sink_service_
                  ->current_session_expiration_timers_[cast_sink1.id()]
                  ->IsRunning());
  EXPECT_TRUE(access_code_cast_sink_service_
                  ->current_session_expiration_timers_[cast_sink2.id()]
                  ->IsRunning());
  EXPECT_TRUE(access_code_cast_sink_service_
                  ->current_session_expiration_timers_[cast_sink3.id()]
                  ->IsRunning());

  fake_network_info_ = fake_wifi_info_;
  ChangeConnectionType(network::mojom::ConnectionType::CONNECTION_WIFI);

  content::RunAllTasksUntilIdle();
  mock_time_task_runner()->FastForwardBy(base::Seconds(100));

  // The timers should be restarted and they should still be running since
  // expiration has not occurred yet.
  EXPECT_TRUE(access_code_cast_sink_service_
                  ->current_session_expiration_timers_[cast_sink1.id()]
                  ->IsRunning());
  EXPECT_TRUE(access_code_cast_sink_service_
                  ->current_session_expiration_timers_[cast_sink2.id()]
                  ->IsRunning());
  EXPECT_TRUE(access_code_cast_sink_service_
                  ->current_session_expiration_timers_[cast_sink3.id()]
                  ->IsRunning());

  FastForwardUiAndIoTasks();
  content::RunAllTasksUntilIdle();
}

TEST_F(AccessCodeCastSinkServiceTest, TestResetExpirationTimersShutdown) {
  // Test to check all expiration timers are cleared after the service is
  // shutdown.
  SetDeviceDurationPrefForTest(base::Seconds(10000));
  FastForwardUiAndIoTasks();

  const MediaSinkInternal cast_sink1 = CreateCastSink(1);
  const MediaSinkInternal cast_sink2 = CreateCastSink(2);
  const MediaSinkInternal cast_sink3 = CreateCastSink(3);

  access_code_cast_sink_service_->StoreSinkInPrefs(&cast_sink1);
  access_code_cast_sink_service_->StoreSinkInPrefs(&cast_sink2);
  access_code_cast_sink_service_->StoreSinkInPrefs(&cast_sink3);
  FastForwardUiAndIoTasks();
  content::RunAllTasksUntilIdle();
  FastForwardUiAndIoTasks();

  access_code_cast_sink_service_->SetExpirationTimer(&cast_sink1);
  access_code_cast_sink_service_->SetExpirationTimer(&cast_sink2);
  access_code_cast_sink_service_->SetExpirationTimer(&cast_sink3);

  EXPECT_TRUE(access_code_cast_sink_service_
                  ->current_session_expiration_timers_[cast_sink1.id()]
                  ->IsRunning());
  EXPECT_TRUE(access_code_cast_sink_service_
                  ->current_session_expiration_timers_[cast_sink2.id()]
                  ->IsRunning());
  EXPECT_TRUE(access_code_cast_sink_service_
                  ->current_session_expiration_timers_[cast_sink3.id()]
                  ->IsRunning());

  access_code_cast_sink_service_->Shutdown();
  EXPECT_TRUE(access_code_cast_sink_service_->current_session_expiration_timers_
                  .empty());
  FastForwardUiAndIoTasks();
  content::RunAllTasksUntilIdle();
}

TEST_F(AccessCodeCastSinkServiceTest, TestChangeEnabledPref) {
  // Test to ensure that all existing sinks are removed, all timers are reset,
  // and all prefs related to access code casting are removed.
  SetDeviceDurationPrefForTest(base::Seconds(10000));
  FastForwardUiAndIoTasks();

  const MediaSinkInternal cast_sink1 = CreateCastSink(1);
  access_code_cast_sink_service_->StoreSinkInPrefs(&cast_sink1);
  mock_cast_media_sink_service_impl()->AddSinkForTest(cast_sink1);

  FastForwardUiAndIoTasks();
  access_code_cast_sink_service_->SetExpirationTimer(&cast_sink1);
  EXPECT_TRUE(access_code_cast_sink_service_
                  ->current_session_expiration_timers_[cast_sink1.id()]
                  ->IsRunning());
  EXPECT_FALSE(
      access_code_cast_sink_service_->pref_updater_->GetDeviceAddedTimeDict()
          .empty());
  EXPECT_FALSE(
      access_code_cast_sink_service_->pref_updater_->GetDevicesDict().empty());

  EXPECT_CALL(*mock_cast_media_sink_service_impl(),
              DisconnectAndRemoveSink(cast_sink1));

  GetTestingPrefs()->SetManagedPref(prefs::kAccessCodeCastEnabled,
                                    base::Value(false));

  EXPECT_TRUE(access_code_cast_sink_service_->current_session_expiration_timers_
                  .empty());
  EXPECT_TRUE(
      access_code_cast_sink_service_->pref_updater_->GetDeviceAddedTimeDict()
          .empty());
  EXPECT_TRUE(
      access_code_cast_sink_service_->pref_updater_->GetDevicesDict().empty());
  FastForwardUiAndIoTasks();
  content::RunAllTasksUntilIdle();
  mock_time_task_runner_->FastForwardUntilNoTasksRemain();
}

TEST_F(AccessCodeCastSinkServiceTest, TestChangeDurationPref) {
  // Test to ensure timers are reset whenever the duration pref changes.
  SetDeviceDurationPrefForTest(base::Seconds(10000));
  FastForwardUiAndIoTasks();

  const MediaSinkInternal cast_sink1 = CreateCastSink(1);
  access_code_cast_sink_service_->StoreSinkInPrefs(&cast_sink1);

  FastForwardUiAndIoTasks();
  access_code_cast_sink_service_->SetExpirationTimer(&cast_sink1);
  EXPECT_TRUE(access_code_cast_sink_service_
                  ->current_session_expiration_timers_[cast_sink1.id()]
                  ->IsRunning());
  EXPECT_EQ(
      base::Seconds(10000) + AccessCodeCastSinkService::kExpirationTimerDelay,
      access_code_cast_sink_service_
          ->current_session_expiration_timers_[cast_sink1.id()]
          ->GetCurrentDelay());

  GetTestingPrefs()->SetUserPref(prefs::kAccessCodeCastDeviceDuration,
                                 base::Value(100));

  EXPECT_TRUE(access_code_cast_sink_service_
                  ->current_session_expiration_timers_[cast_sink1.id()]
                  ->IsRunning());
  EXPECT_EQ(
      base::Seconds(100) + AccessCodeCastSinkService::kExpirationTimerDelay,
      access_code_cast_sink_service_
          ->current_session_expiration_timers_[cast_sink1.id()]
          ->GetCurrentDelay());
}

TEST_F(AccessCodeCastSinkServiceTest, TestChangeNetworkWithRouteActive) {
  // This test ensures that a call to remove a media sink will NOT be made if
  // there is currently an active route.
  SetDeviceDurationPrefForTest(base::Seconds(10000));
  const MediaSinkInternal cast_sink1 = CreateCastSink(1);

  mock_cast_media_sink_service_impl()->AddSinkForTest(cast_sink1);
  access_code_cast_sink_service_->StoreSinkInPrefs(&cast_sink1);
  MediaRoute media_route_cast = CreateRouteForTesting(cast_sink1.id());
  std::vector<MediaRoute> route_list = {media_route_cast};

  EXPECT_CALL(*mock_cast_media_sink_service_impl(), HasSink(cast_sink1.id()));
  // Simulate that this cast sink has an open route.
  access_code_cast_sink_service_->media_routes_observer_->OnRoutesUpdated(
      route_list);
  ON_CALL(*router_, GetCurrentRoutes())
      .WillByDefault(Return(std::vector<MediaRoute>{media_route_cast}));

  // Since the route has not changed, no call to remove the sink should have
  // been made after the network changed.
  EXPECT_CALL(*mock_cast_media_sink_service_impl(),
              DisconnectAndRemoveSink(cast_sink1))
      .Times(0);

  FastForwardUiAndIoTasks();
  content::RunAllTasksUntilIdle();

  fake_network_info_ = fake_wifi_info_;
  ChangeConnectionType(network::mojom::ConnectionType::CONNECTION_WIFI);

  content::RunAllTasksUntilIdle();
  mock_time_task_runner()->FastForwardBy(base::Seconds(100));

  // Simulate that the route has ended.
  ON_CALL(*router_, GetCurrentRoutes())
      .WillByDefault(Return(std::vector<MediaRoute>{}));
  access_code_cast_sink_service_->media_routes_observer_->OnRoutesUpdated({});

  // The sink should NOT now be removed from the media router since it was not
  // expired.
  EXPECT_CALL(*mock_cast_media_sink_service_impl(),
              DisconnectAndRemoveSink(cast_sink1))
      .Times(0);
  FastForwardUiAndIoTasks();
  content::RunAllTasksUntilIdle();

  // The sink did not expire in this situation so it should still exist in the
  // pref service.
  EXPECT_FALSE(
      access_code_cast_sink_service_->pref_updater_->GetDeviceAddedTimeDict()
          .empty());
  EXPECT_FALSE(
      access_code_cast_sink_service_->pref_updater_->GetDevicesDict().empty());
}

TEST_F(AccessCodeCastSinkServiceTest,
       TestChangeNetworkWithRouteActiveExpiration) {
  // This test ensures that a call to remove a media sink will NOT be made if
  // there is currently an active route, this time when the sink has expired
  // before the network has changed.
  SetDeviceDurationPrefForTest(base::Seconds(100));
  MediaSinkInternal cast_sink1 = CreateCastSink(1);

  auto cast_data = cast_sink1.cast_data();
  cast_data.discovery_type = CastDiscoveryType::kAccessCodeRememberedDevice;
  cast_sink1.set_cast_data(cast_data);

  mock_cast_media_sink_service_impl()->AddSinkForTest(cast_sink1);
  access_code_cast_sink_service_->StoreSinkInPrefs(&cast_sink1);
  access_code_cast_sink_service_->SetExpirationTimer(&cast_sink1);
  MediaRoute media_route_cast = CreateRouteForTesting(cast_sink1.id());
  std::vector<MediaRoute> route_list = {media_route_cast};

  EXPECT_CALL(*mock_cast_media_sink_service_impl(), HasSink(cast_sink1.id()));
  // Simulate that this cast sink has an open route.
  access_code_cast_sink_service_->media_routes_observer_->OnRoutesUpdated(
      route_list);
  ON_CALL(*router_, GetCurrentRoutes())
      .WillByDefault(Return(std::vector<MediaRoute>{media_route_cast}));

  // Since the route has not changed, no call to remove the sink should have
  // been made after the network changed.
  EXPECT_CALL(*mock_cast_media_sink_service_impl(),
              DisconnectAndRemoveSink(cast_sink1))
      .Times(0);

  // Expire the sink
  mock_time_task_runner()->FastForwardBy(base::Seconds(300));
  task_environment_.FastForwardBy(base::Seconds(300));

  FastForwardUiAndIoTasks();
  content::RunAllTasksUntilIdle();

  fake_network_info_ = fake_wifi_info_;
  ChangeConnectionType(network::mojom::ConnectionType::CONNECTION_WIFI);

  content::RunAllTasksUntilIdle();
  mock_time_task_runner()->FastForwardBy(base::Seconds(300));

  // Simulate that the route has ended.
  ON_CALL(*router_, GetCurrentRoutes())
      .WillByDefault(Return(std::vector<MediaRoute>{}));
  access_code_cast_sink_service_->media_routes_observer_->OnRoutesUpdated({});

  // The sink should now be removed from the media router.
  EXPECT_CALL(*mock_cast_media_sink_service_impl(),
              DisconnectAndRemoveSink(cast_sink1));
  FastForwardUiAndIoTasks();
  content::RunAllTasksUntilIdle();
  FastForwardUiAndIoTasks();

  // The sink did expire in this situation so it should not exist in the pref
  // service.
  EXPECT_TRUE(
      access_code_cast_sink_service_->pref_updater_->GetDeviceAddedTimeDict()
          .empty());
  EXPECT_TRUE(
      access_code_cast_sink_service_->pref_updater_->GetDevicesDict().empty());
}

TEST_F(AccessCodeCastSinkServiceTest, DiscoverSinkWithNoMediaRouter) {
  // Make sure that a nullptr media router will cause an error and immediately
  // returned.
  MockAddSinkResultCallback mock_callback;

  // Shut down the access code cast sink service causing the media router to
  // become nullptr.
  access_code_cast_sink_service_->Shutdown();
  EXPECT_CALL(mock_callback, Run(AddSinkResultCode::INTERNAL_MEDIA_ROUTER_ERROR,
                                 Eq(absl::nullopt)));

  access_code_cast_sink_service_->DiscoverSink("", mock_callback.Get());

  mock_time_task_runner()->FastForwardUntilNoTasksRemain();
}

TEST_F(AccessCodeCastSinkServiceTest,
       TestCheckMediaSinkForExpirationNoExpiration) {
  // Demonstrates that checking a media sink for expiration will return if it
  // hasn't expired yet.

  const MediaSinkInternal cast_sink1 = CreateCastSink(1);
  SetDeviceDurationPrefForTest(base::Seconds(100));

  access_code_cast_sink_service_->SetExpirationTimer(&cast_sink1);
  access_code_cast_sink_service_->StoreSinkInPrefs(&cast_sink1);
  mock_time_task_runner()->FastForwardUntilNoTasksRemain();

  EXPECT_TRUE(access_code_cast_sink_service_
                  ->current_session_expiration_timers_[cast_sink1.id()]
                  ->IsRunning());
  // Expect that CheckMediaSinkForExpiration() will simply return since the
  // timer has not expired yet.
  access_code_cast_sink_service_->CheckMediaSinkForExpiration(cast_sink1.id());
  EXPECT_TRUE(access_code_cast_sink_service_
                  ->current_session_expiration_timers_[cast_sink1.id()]
                  ->IsRunning());

  mock_time_task_runner()->FastForwardUntilNoTasksRemain();
}

TEST_F(AccessCodeCastSinkServiceTest,
       TestCheckMediaSinkForExpirationBeforeDelay) {
  // Demonstrates that checking a media sink for expiration will fire it before
  // OnExpiration is called because of the kExpirationDelay.

  const MediaSinkInternal cast_sink1 = CreateCastSink(1);
  SetDeviceDurationPrefForTest(base::Seconds(0));

  mock_cast_media_sink_service_impl()->AddSinkForTest(cast_sink1);
  access_code_cast_sink_service_->SetExpirationTimer(&cast_sink1);
  access_code_cast_sink_service_->StoreSinkInPrefs(&cast_sink1);
  mock_time_task_runner()->FastForwardUntilNoTasksRemain();

  EXPECT_TRUE(access_code_cast_sink_service_
                  ->current_session_expiration_timers_[cast_sink1.id()]
                  ->IsRunning());

  // There is a brief delay that is added to any expiration timer. We want to
  // check the time between the delay and zero seconds (instant expiration) to
  // ensure that we can manually trigger this expiration and override the delay.
  task_environment_.AdvanceClock(
      AccessCodeCastSinkService::kExpirationTimerDelay - base::Seconds(10));
  EXPECT_TRUE(access_code_cast_sink_service_
                  ->current_session_expiration_timers_[cast_sink1.id()]
                  ->IsRunning());

  access_code_cast_sink_service_->CheckMediaSinkForExpiration(cast_sink1.id());
  EXPECT_TRUE(access_code_cast_sink_service_->current_session_expiration_timers_
                  .empty());

  // The sink should now be removed from the media router.
  EXPECT_CALL(*mock_cast_media_sink_service_impl(),
              DisconnectAndRemoveSink(cast_sink1));
  FastForwardUiAndIoTasks();
  content::RunAllTasksUntilIdle();
  FastForwardUiAndIoTasks();

  // The sink did expire in this situation so it should not exist in the pref
  // service.
  EXPECT_TRUE(
      access_code_cast_sink_service_->pref_updater_->GetDeviceAddedTimeDict()
          .empty());
  EXPECT_TRUE(
      access_code_cast_sink_service_->pref_updater_->GetDevicesDict().empty());
}

TEST_F(AccessCodeCastSinkServiceTest,
       TestCheckMediaSinkForExpirationAfterDelay) {
  // Demonstrates that if an expiration timer is no longer running, then we will
  // attempt to expire the sink since this means the sink has expired but the
  // route was still active.

  MediaSinkInternal cast_sink1 = CreateCastSink(1);
  cast_sink1.cast_data().discovery_type =
      CastDiscoveryType::kAccessCodeManualEntry;
  SetDeviceDurationPrefForTest(base::Seconds(0));

  mock_cast_media_sink_service_impl()->AddSinkForTest(cast_sink1);
  access_code_cast_sink_service_->SetExpirationTimer(&cast_sink1);
  access_code_cast_sink_service_->StoreSinkInPrefs(&cast_sink1);

  MediaRoute media_route_cast = CreateRouteForTesting(cast_sink1.id());
  access_code_cast_sink_service_->media_routes_observer_->OnRoutesUpdated(
      {media_route_cast});

  FastForwardUiAndIoTasks();
  content::RunAllTasksUntilIdle();

  EXPECT_TRUE(access_code_cast_sink_service_
                  ->current_session_expiration_timers_[cast_sink1.id()]
                  ->IsRunning());

  media_route_cast.set_local(true);
  ON_CALL(*router_, GetCurrentRoutes())
      .WillByDefault(Return(std::vector<MediaRoute>{media_route_cast}));

  // Expire the sink while there is still an active local route.
  task_environment_.AdvanceClock(
      AccessCodeCastSinkService::kExpirationTimerDelay + base::Seconds(100));
  content::RunAllTasksUntilIdle();
  FastForwardUiAndIoTasks();
  mock_time_task_runner()->FastForwardUntilNoTasksRemain();
  EXPECT_FALSE(access_code_cast_sink_service_
                   ->current_session_expiration_timers_[cast_sink1.id()]
                   ->IsRunning());

  // The sink should NOT be removed from the media router since there is still
  // an open local route.
  EXPECT_CALL(*mock_cast_media_sink_service_impl(),
              DisconnectAndRemoveSink(cast_sink1))
      .Times(0);
  FastForwardUiAndIoTasks();
  content::RunAllTasksUntilIdle();
  FastForwardUiAndIoTasks();

  // The sink did NOT expire in this situation so it should exist in the
  // pref service.
  EXPECT_FALSE(
      access_code_cast_sink_service_->pref_updater_->GetDeviceAddedTimeDict()
          .empty());
  EXPECT_FALSE(
      access_code_cast_sink_service_->pref_updater_->GetDevicesDict().empty());

  // Finally remove the route from the media router.
  ON_CALL(*router_, GetCurrentRoutes())
      .WillByDefault(Return(std::vector<MediaRoute>{}));
  access_code_cast_sink_service_->media_routes_observer_->OnRoutesUpdated({});

  // The sink should now be removed from the media router.
  EXPECT_CALL(*mock_cast_media_sink_service_impl(),
              DisconnectAndRemoveSink(cast_sink1));
  task_environment_.RunUntilIdle();
  FastForwardUiAndIoTasks();
  content::RunAllTasksUntilIdle();
  FastForwardUiAndIoTasks();

  // The sink did expire in this situation so it should not exist in the pref
  // service.
  EXPECT_TRUE(
      access_code_cast_sink_service_->pref_updater_->GetDeviceAddedTimeDict()
          .empty());
  EXPECT_TRUE(
      access_code_cast_sink_service_->pref_updater_->GetDevicesDict().empty());
}

TEST_F(AccessCodeCastSinkServiceTest, TestOfflineDiscoverSink) {
  // Test to ensure that discover sink triggers a network error callback if we
  // are offline.
  MockAddSinkResultCallback mock_callback;

  network::TestNetworkConnectionTracker::GetInstance()->SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_NONE);
  EXPECT_CALL(mock_callback,
              Run(AddSinkResultCode::SERVICE_NOT_PRESENT, Eq(absl::nullopt)));

  access_code_cast_sink_service_->DiscoverSink("", mock_callback.Get());

  mock_time_task_runner()->FastForwardUntilNoTasksRemain();
}

TEST_F(AccessCodeCastSinkServiceTest, RefreshStoredDeviceInfo) {
  // If a device is already in the list, then its info changes. To store a
  // device, we take its info from the entry in the media router, so checking
  // the value of the stored device pref suffices
  SetDeviceDurationPrefForTest(base::Seconds(100));

  // Create fake sinks for test. Importantly, new_sink_1 has the same id as
  // existing_sink_1, but they have different names.
  MediaSinkInternal existing_sink_1 = CreateCastSink(1);
  MediaSinkInternal existing_sink_2 = CreateCastSink(2);
  MediaSinkInternal new_sink_1 = CreateCastSink(1);
  const std::string fake_new_sink_name = "Fake new sink";
  new_sink_1.sink().set_name(fake_new_sink_name);
  EXPECT_EQ(existing_sink_1.id(), new_sink_1.id());
  EXPECT_NE(existing_sink_1.sink().name(), new_sink_1.sink().name());

  existing_sink_1.cast_data().discovery_type =
      CastDiscoveryType::kAccessCodeManualEntry;
  existing_sink_2.cast_data().discovery_type =
      CastDiscoveryType::kAccessCodeManualEntry;
  new_sink_1.cast_data().discovery_type =
      CastDiscoveryType::kAccessCodeManualEntry;

  // Add existing sinks to the mock media router and stored prefs. Since the
  // CastMediaSinkService is mocked out, just manually set everything instead.
  mock_cast_media_sink_service_impl()->AddSinkForTest(existing_sink_1);
  mock_cast_media_sink_service_impl()->AddSinkForTest(existing_sink_2);

  access_code_cast_sink_service_->StoreSinkInPrefs(&existing_sink_1);
  access_code_cast_sink_service_->StoreSinkInPrefs(&existing_sink_2);

  mock_time_task_runner()->FastForwardUntilNoTasksRemain();

  // Try to add new_sink_1, which has an id that matches an existing sink.
  MockAddSinkResultCallback mock_callback;
  access_code_cast_sink_service_->OpenChannelIfNecessary(
      new_sink_1, mock_callback.Get(), /*has_sink=*/true);
  mock_time_task_runner()->FastForwardUntilNoTasksRemain();

  // Now we expect that the name for the sink with the id of existing_sink_1
  // should have changed.
  auto* sink_1_dict =
      access_code_cast_sink_service_->pref_updater_->GetDevicesDict().Find(
          existing_sink_1.id());
  auto* sink_2_dict =
      access_code_cast_sink_service_->pref_updater_->GetDevicesDict().Find(
          existing_sink_2.id());
  EXPECT_EQ(*sink_1_dict, CreateValueDictFromMediaSinkInternal(new_sink_1));
  EXPECT_EQ(*sink_2_dict,
            CreateValueDictFromMediaSinkInternal(existing_sink_2));
}

TEST_F(AccessCodeCastSinkServiceTest, RefreshExistingDeviceName) {
  SetDeviceDurationPrefForTest(base::Seconds(100));

  // Create fake sinks for test. Importantly, new_sink_1 has the same id as
  // existing_sink_1, but they have different names.
  MediaSinkInternal existing_sink_1 = CreateCastSink(1);
  MediaSinkInternal new_sink_1 = CreateCastSink(1);
  const std::string fake_new_sink_name = "Fake new sink";
  new_sink_1.sink().set_name(fake_new_sink_name);
  EXPECT_EQ(existing_sink_1.id(), new_sink_1.id());
  EXPECT_NE(existing_sink_1.sink().name(), new_sink_1.sink().name());

  existing_sink_1.cast_data().discovery_type =
      CastDiscoveryType::kAccessCodeManualEntry;
  new_sink_1.cast_data().discovery_type =
      CastDiscoveryType::kAccessCodeManualEntry;

  // Add existing sinks to the mock media router and stored prefs. Since the
  // CastMediaSinkService is mocked out, just manually set everything instead.
  mock_cast_media_sink_service_impl()->AddSinkForTest(existing_sink_1);

  mock_time_task_runner()->FastForwardUntilNoTasksRemain();

  // Try to add new_sink_1, which has an id that matches an existing sink.
  MockAddSinkResultCallback mock_callback;
  access_code_cast_sink_service_->OpenChannelIfNecessary(
      new_sink_1, mock_callback.Get(), /*has_sink=*/true);
  FastForwardUiAndIoTasks();
  mock_time_task_runner()->FastForwardUntilNoTasksRemain();

  EXPECT_EQ(mock_cast_media_sink_service_impl()
                ->GetSinkById(existing_sink_1.id())
                ->sink()
                .name(),
            new_sink_1.sink().name());
}

TEST_F(AccessCodeCastSinkServiceTest, RefreshStoredDeviceTimer) {
  // If a device is already stored, then its timer is reset.
  SetDeviceDurationPrefForTest(base::Seconds(100));

  // Init fake stored devices for test.
  MediaSinkInternal cast_sink1 = CreateCastSink(1);
  auto cast_data1 = cast_sink1.cast_data();
  cast_data1.discovery_type = CastDiscoveryType::kAccessCodeManualEntry;
  cast_sink1.set_cast_data(cast_data1);

  MediaSinkInternal cast_sink2 = CreateCastSink(2);
  auto cast_data2 = cast_sink2.cast_data();
  cast_data2.discovery_type = CastDiscoveryType::kAccessCodeManualEntry;
  cast_sink2.set_cast_data(cast_data2);

  mock_cast_media_sink_service_impl()->AddSinkForTest(cast_sink1);
  mock_cast_media_sink_service_impl()->AddSinkForTest(cast_sink2);

  access_code_cast_sink_service_->StoreSinkInPrefs(&cast_sink1);
  access_code_cast_sink_service_->StoreSinkInPrefs(&cast_sink2);

  access_code_cast_sink_service_->SetExpirationTimer(&cast_sink1);
  access_code_cast_sink_service_->SetExpirationTimer(&cast_sink2);

  // Forward the time and trigger the timer to be refreshed for one of the
  // devices.
  mock_time_task_runner()->FastForwardBy(base::Seconds(50));
  task_environment_.FastForwardBy(base::Seconds(50));

  MockAddSinkResultCallback mock_callback;
  access_code_cast_sink_service_->OpenChannelIfNecessary(
      cast_sink1, mock_callback.Get(), /*has_sink=*/true);
  mock_time_task_runner()->FastForwardUntilNoTasksRemain();

  // Check that both devices still have an expiration timer as expected.
  EXPECT_EQ(
      access_code_cast_sink_service_->current_session_expiration_timers_.size(),
      2u);

  // Forward the time again, so an un-refreshed device would expire.
  mock_time_task_runner()->FastForwardBy(base::Seconds(75));
  task_environment_.FastForwardBy(base::Seconds(75));
  mock_time_task_runner()->FastForwardUntilNoTasksRemain();

  // Check that the proper device has expired by checking if its expiration
  // timer is still running.
  EXPECT_TRUE(access_code_cast_sink_service_
                  ->current_session_expiration_timers_[cast_sink1.id()]
                  ->IsRunning());
  EXPECT_FALSE(access_code_cast_sink_service_
                   ->current_session_expiration_timers_[cast_sink2.id()]
                   ->IsRunning());

  // Forward the time again, so all devices expire.
  mock_time_task_runner()->FastForwardBy(base::Seconds(50));
  task_environment_.FastForwardBy(base::Seconds(50));
  mock_time_task_runner()->FastForwardUntilNoTasksRemain();

  // Check that the proper device has expired by checking if its expiration
  // timer is still running.
  EXPECT_FALSE(access_code_cast_sink_service_
                   ->current_session_expiration_timers_[cast_sink1.id()]
                   ->IsRunning());
  EXPECT_FALSE(access_code_cast_sink_service_
                   ->current_session_expiration_timers_[cast_sink2.id()]
                   ->IsRunning());
}

TEST_F(AccessCodeCastSinkServiceTest, HandleMediaRouteAdded) {
  // Initialize histogram tester so we can ensure metrics are collected.
  base::HistogramTester histogram_tester;

  // Set duration pref since it will be recorded in metrics.
  SetDeviceDurationPrefForTest(base::Seconds(10));

  // Create fake sinks for the test.

  // cast_sink1 is not an access code sink.
  MediaSinkInternal cast_sink1 = CreateCastSink(1);
  auto cast_data1 = cast_sink1.cast_data();
  cast_data1.discovery_type = CastDiscoveryType::kMdns;
  cast_sink1.set_cast_data(cast_data1);

  const MediaRoute::Id fake_route_1 =
      "urn:x-org.chromium:media:route:1/" + cast_sink1.id() + "/http://foo.com";

  // cast_sink2 is a new access code sink.
  MediaSinkInternal cast_sink2 = CreateCastSink(2);
  auto cast_data2 = cast_sink2.cast_data();
  cast_data2.discovery_type = CastDiscoveryType::kAccessCodeManualEntry;
  cast_sink2.set_cast_data(cast_data2);

  const MediaRoute::Id fake_route_2 =
      "urn:x-org.chromium:media:route:2/" + cast_sink2.id() + "/http://foo.com";

  // cast_sink3 is a saved access code sink.
  MediaSinkInternal cast_sink3 = CreateCastSink(3);
  auto cast_data3 = cast_sink3.cast_data();
  cast_data3.discovery_type = CastDiscoveryType::kAccessCodeRememberedDevice;
  cast_sink3.set_cast_data(cast_data3);

  const MediaRoute::Id fake_route_3 =
      "urn:x-org.chromium:media:route:3/" + cast_sink3.id() + "/http://foo.com";

  // The histogram should start with nothing logged.
  histogram_tester.ExpectTotalCount(
      "AccessCodeCast.Discovery.DeviceDurationOnRoute", 0);

  access_code_cast_sink_service_->HandleMediaRouteAdded(
      fake_route_1, true, MediaSource::ForAnyTab(), &cast_sink1);

  // The histogram should not be logged to after a non access code route starts.
  histogram_tester.ExpectTotalCount(
      "AccessCodeCast.Discovery.DeviceDurationOnRoute", 0);

  access_code_cast_sink_service_->HandleMediaRouteAdded(
      fake_route_2, true, MediaSource::ForAnyTab(), &cast_sink2);

  // The histogram should log when a route starts to a new access code device.
  histogram_tester.ExpectBucketCount(
      "AccessCodeCast.Discovery.DeviceDurationOnRoute", 10, 1);

  access_code_cast_sink_service_->HandleMediaRouteAdded(
      fake_route_3, true, MediaSource::ForAnyTab(), &cast_sink3);

  // The histogram should log when a route starts to a saved access code device.
  histogram_tester.ExpectBucketCount(
      "AccessCodeCast.Discovery.DeviceDurationOnRoute", 10, 2);

  // Ensure various pref values are can be logged.
  SetDeviceDurationPrefForTest(base::Seconds(100));
  access_code_cast_sink_service_->HandleMediaRouteAdded(
      fake_route_2, true, MediaSource::ForAnyTab(), &cast_sink2);
  histogram_tester.ExpectBucketCount(
      "AccessCodeCast.Discovery.DeviceDurationOnRoute", 100, 1);

  SetDeviceDurationPrefForTest(base::Seconds(1000));
  access_code_cast_sink_service_->HandleMediaRouteAdded(
      fake_route_2, true, MediaSource::ForAnyTab(), &cast_sink2);
  histogram_tester.ExpectBucketCount(
      "AccessCodeCast.Discovery.DeviceDurationOnRoute", 1000, 1);

  // Histogram logs should not have been lost.
  histogram_tester.ExpectTotalCount(
      "AccessCodeCast.Discovery.DeviceDurationOnRoute", 4);
}

TEST_F(AccessCodeCastSinkServiceTest, RecordRouteDuration) {
  // Initialize histogram tester so we can ensure metrics are collected.
  base::HistogramTester histogram_tester;

  MediaSinkInternal cast_sink1 = CreateCastSink(1);

  auto cast_data = cast_sink1.cast_data();
  cast_data.discovery_type = CastDiscoveryType::kAccessCodeRememberedDevice;
  cast_sink1.set_cast_data(cast_data);

  mock_cast_media_sink_service_impl()->AddSinkForTest(cast_sink1);
  access_code_cast_sink_service_->StoreSinkInPrefs(&cast_sink1);
  access_code_cast_sink_service_->SetExpirationTimer(&cast_sink1);
  MediaRoute media_route_cast = CreateRouteForTesting(cast_sink1.id());
  std::vector<MediaRoute> route_list = {media_route_cast};

  // Simulate that this cast sink has an open route.
  access_code_cast_sink_service_->media_routes_observer_->OnRoutesUpdated(
      route_list);
  FastForwardUiAndIoTasks();

  // The histogram should start with nothing logged.
  histogram_tester.ExpectTotalCount("AccessCodeCast.Session.RouteDuration", 0);
  mock_time_task_runner()->FastForwardBy(base::Seconds(30));
  FastForwardUiAndIoTasks();

  // Simulate that all routes have ended.
  access_code_cast_sink_service_->media_routes_observer_->OnRoutesUpdated({});
  FastForwardUiAndIoTasks();

  // The histogram should have recorded since a local route that ended.
  histogram_tester.ExpectTotalCount("AccessCodeCast.Session.RouteDuration", 1);

  // Now add a non-local route
  std::string route_id =
      "urn:x-org.chromium:media:route:2/" + cast_sink1.id() + "/http://foo.com";
  MediaRoute remote_route = MediaRoute(route_id, MediaSource("access_code"),
                                       cast_sink1.id(), "access_sink",
                                       /*is_local=*/false);
  access_code_cast_sink_service_->media_routes_observer_->OnRoutesUpdated(
      {remote_route});
  mock_time_task_runner()->FastForwardBy(base::Seconds(30));
  FastForwardUiAndIoTasks();

  // Now simulate ending the route
  access_code_cast_sink_service_->media_routes_observer_->OnRoutesUpdated({});
  FastForwardUiAndIoTasks();

  // Expect the count not to change, since the route wasn't local.
  histogram_tester.ExpectTotalCount("AccessCodeCast.Session.RouteDuration", 1);
}

TEST_F(AccessCodeCastSinkServiceTest, RecordRouteDurationNonAccessCodeDevice) {
  // Initialize histogram tester so we can ensure metrics are collected.
  base::HistogramTester histogram_tester;

  MediaSinkInternal mock_non_access_code_sink = CreateCastSink(1);
  MediaRoute media_route_cast =
      CreateRouteForTesting(mock_non_access_code_sink.id());
  std::vector<MediaRoute> route_list = {media_route_cast};

  // Simulate that this cast sink has an open route.
  access_code_cast_sink_service_->media_routes_observer_->OnRoutesUpdated(
      route_list);
  FastForwardUiAndIoTasks();

  // The cast sink was not added by an access code so no histogram should be
  // recorded.
  histogram_tester.ExpectTotalCount("AccessCodeCast.Session.RouteDuration", 0);
}

}  // namespace media_router
