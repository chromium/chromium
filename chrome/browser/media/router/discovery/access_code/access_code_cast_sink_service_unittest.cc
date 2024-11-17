// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/access_code/access_code_cast_sink_service.h"

#include <string>
#include <utility>
#include <vector>

#include "base/json/values_util.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/media/router/chrome_media_router_factory.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_cast_feature.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_cast_pref_updater.h"
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
#include "content/public/test/test_utils.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Eq;
using ::testing::Return;

namespace media_router {

using SinkSource = CastDeviceCountMetrics::SinkSource;
using MockAddSinkResultCallback = base::MockCallback<
    media_router::AccessCodeCastSinkService::AddSinkResultCallback>;
using DiscoveryDevice = chrome_browser_media::proto::DiscoveryDevice;

// This delay is needed because MediaNotificationService waits for
// `kExpirationDelay` before it calls the CastMediaSinkServiceImpl to
// disconnect/remove sinks. `kExpirationDelay` is not used here because
// sometimes there are two delayed tasks posted.
static constexpr base::TimeDelta kRemoveRouteDelay =
    AccessCodeCastSinkService::kExpirationDelay * 2;

static constexpr base::TimeDelta kNetworkChangeDelay =
    AccessCodeCastSinkService::kExpirationDelay +
    AccessCodeCastSinkService::kNetworkChangeBuffer;

class AccessCodeCastSinkServiceTest : public testing::Test {
 public:
  AccessCodeCastSinkServiceTest()
      : task_runner_(task_environment_.GetMainThreadTaskRunner()),
        mock_cast_socket_service_(
            std::make_unique<cast_channel::MockCastSocketService>(
                task_runner_)),
        message_handler_(mock_cast_socket_service_.get()),
        cast_media_sink_service_impl_(
            std::make_unique<MockCastMediaSinkServiceImpl>(
                mock_sink_discovered_cb_.Get(),
                mock_cast_socket_service_.get(),
                discovery_network_monitor_.get(),
                &dual_media_sink_service_)) {}

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
        std::make_unique<AccessCodeCastSinkService>(
            &profile_, router_.get(), cast_media_sink_service_impl_.get(),
            discovery_network_monitor_.get(), GetTestingPrefs(),
            std::make_unique<MockAccessCodeCastPrefUpdater>());
    access_code_cast_sink_service_->SetTaskRunnerForTesting(task_runner());
    task_environment_.RunUntilIdle();
  }

  void TearDown() override {
    access_code_cast_sink_service_->ShutdownForTesting();
    access_code_cast_sink_service_.reset();
    router_.reset();
    task_environment_.RunUntilIdle();
    fake_network_info_ = fake_ethernet_info_;
  }

  void SimulateRoutesUpdated(const std::vector<MediaRoute>& routes) {
    ON_CALL(*router_, GetCurrentRoutes()).WillByDefault(Return(routes));
    access_code_cast_sink_service_->OnObserverRoutesUpdatedForTesting(routes);
    task_environment_.RunUntilIdle();
  }

  void SetDeviceDurationPrefForTesting(const base::TimeDelta duration_time) {
    GetTestingPrefs()->SetUserPref(
        prefs::kAccessCodeCastDeviceDuration,
        base::Value(static_cast<int>(duration_time.InSeconds())));
  }

  bool IsDevicesDictEmpty() {
    base::test::TestFuture<base::Value::Dict> devices_dict;
    pref_updater()->GetDevicesDict(devices_dict.GetCallback());
    return devices_dict.Get().empty();
  }

  bool IsDeviceAddedTimeDictEmpty() {
    base::test::TestFuture<base::Value::Dict> devices_added_time_dict;
    pref_updater()->GetDeviceAddedTimeDict(
        devices_added_time_dict.GetCallback());
    return devices_added_time_dict.Get().empty();
  }

  MediaSinkInternal GetMediaSinkInternalFromPref(const MediaSink::Id& sink_id) {
    base::test::TestFuture<base::Value::Dict> devices_dict;
    pref_updater()->GetDevicesDict(devices_dict.GetCallback());
    auto* sink_dict = devices_dict.Get().FindDict(sink_id);
    EXPECT_TRUE(sink_dict);
    auto media_sink = ParseValueDictIntoMediaSinkInternal(*sink_dict);
    EXPECT_TRUE(media_sink.has_value());
    return media_sink.value();
  }

  void StoreSinkInPrefs(const MediaSinkInternal& sink) {
    base::RunLoop loop;
    access_code_cast_sink_service_->StoreSinkInPrefsForTesting(
        loop.QuitClosure(), &sink);
    loop.Run();
  }

  void SetExpirationTimerAndExpectTimerRunning(const MediaSinkInternal& sink) {
    access_code_cast_sink_service_->SetExpirationTimerForTesting(sink.id());
    task_environment_.RunUntilIdle();
    EXPECT_TRUE(GetExpirationTimer(sink.id())->IsRunning());
  }

  base::OneShotTimer* GetExpirationTimer(const MediaSink::Id& sink_id) {
    return current_session_expiration_timers().find(sink_id)->second.get();
  }

  void ChangeConnectionType(network::mojom::ConnectionType connection_type) {
    discovery_network_monitor_->OnConnectionChanged(connection_type);
    task_environment_.RunUntilIdle();
  }

  void OpenChannelIfNecessary(
      const MediaSinkInternal& sink,
      AccessCodeCastSinkService::AddSinkResultCallback add_sink_callback,
      bool has_sink) {
    access_code_cast_sink_service_->OpenChannelIfNecessaryForTesting(
        sink, std::move(add_sink_callback), has_sink);

    task_environment_.RunUntilIdle();
  }

  MockCastMediaSinkServiceImpl* mock_cast_media_sink_service_impl() {
    return cast_media_sink_service_impl_.get();
  }

  const scoped_refptr<base::SingleThreadTaskRunner>& task_runner() {
    return task_runner_;
  }

  sync_preferences::TestingPrefServiceSyncable* GetTestingPrefs() {
    return profile_.GetTestingPrefService();
  }

  AccessCodeCastPrefUpdater* pref_updater() {
    return access_code_cast_sink_service_->GetPrefUpdaterForTesting();
  }

  const std::map<MediaSink::Id, std::unique_ptr<base::OneShotTimer>>&
  current_session_expiration_timers() {
    return access_code_cast_sink_service_
        ->GetCurrentSessionExpirationTimersForTesting();
  }

  void ExpectOpenChannels(std::vector<MediaSinkInternal> cast_sinks,
                          int num_times) {
    // Only check whether sink_id matches. Since `cast_channel_id_` is not
    // stored in the pref service.
    for (auto sink : cast_sinks) {
      EXPECT_CALL(
          *mock_cast_media_sink_service_impl(),
          OpenChannel(testing::Property(&MediaSinkInternal::id, sink.id()), _,
                      SinkSource::kAccessCode, _, _))
          .Times(num_times);
    }
  }

  void ExpectHasSink(std::vector<MediaSinkInternal> cast_sinks, int num_times) {
    for (auto sink : cast_sinks) {
      EXPECT_CALL(*mock_cast_media_sink_service_impl(), HasSink(sink.id()))
          .Times(num_times);
    }
  }

  void AddSinkToMediaRouter(const MediaSinkInternal& sink) {
    base::RunLoop loop;
    EXPECT_CALL(*mock_cast_media_sink_service_impl(), OpenChannel)
        .WillOnce([&] { loop.Quit(); });
    access_code_cast_sink_service_->AddSinkToMediaRouter(sink,
                                                         base::DoNothing());
    loop.Run();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::TimeSource::MOCK_TIME};
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
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  base::MockCallback<OnSinksDiscoveredCallback> mock_sink_discovered_cb_;

  TestMediaSinkService dual_media_sink_service_;
  std::unique_ptr<cast_channel::MockCastSocketService>
      mock_cast_socket_service_;
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
  SetDeviceDurationPrefForTesting(base::Seconds(10));

  // Add a non-access code cast sink to media router and route list.
  MediaSinkInternal cast_sink1 = CreateCastSink(1);
  MediaRoute media_route_cast = CreateRouteForTesting(cast_sink1.id());
  std::vector<MediaRoute> route_list = {media_route_cast};

  // Expect that the removed_route_id_ member variable has not changes since no
  // route was removed.
  SimulateRoutesUpdated(route_list);
  EXPECT_TRUE(
      access_code_cast_sink_service_->GetObserverRemovedRouteIdForTesting()
          .empty());

  // Add a cast sink discovered by access code to the list of routes.
  MediaSinkInternal access_code_sink2 = CreateCastSink(2);
  access_code_sink2.cast_data().discovery_type =
      CastDiscoveryType::kAccessCodeManualEntry;
  MediaRoute media_route_access = CreateRouteForTesting(access_code_sink2.id());

  route_list.push_back(media_route_access);

  mock_cast_media_sink_service_impl()->AddSinkForTest(access_code_sink2);
  StoreSinkInPrefs(access_code_sink2);
  SetExpirationTimerAndExpectTimerRunning(access_code_sink2);

  // Expect that the removed_route_id_ member variable has not changes since no
  // route was removed.
  SimulateRoutesUpdated(route_list);
  EXPECT_TRUE(
      access_code_cast_sink_service_->GetObserverRemovedRouteIdForTesting()
          .empty());

  // Remove the non-access code sink from the list of routes.
  std::erase(route_list, media_route_cast);
  SimulateRoutesUpdated(route_list);
  EXPECT_EQ(
      access_code_cast_sink_service_->GetObserverRemovedRouteIdForTesting(),
      media_route_cast.media_route_id());

  // Expect cast sink is NOT removed from the media router since it
  // is not an access code sink.
  EXPECT_CALL(*mock_cast_media_sink_service_impl(),
              DisconnectAndRemoveSink(cast_sink1))
      .Times(0);
  access_code_cast_sink_service_->HandleMediaRouteRemovedByAccessCodeForTesting(
      &cast_sink1);
  task_environment_.FastForwardBy(kRemoveRouteDelay);

  // Remove the access code sink from the list of routes.
  std::erase(route_list, media_route_access);

  SimulateRoutesUpdated(route_list);
  EXPECT_EQ(
      access_code_cast_sink_service_->GetObserverRemovedRouteIdForTesting(),
      media_route_access.media_route_id());

  // Expire the access code cast sink while the route is still active.
  task_environment_.AdvanceClock(base::Seconds(100));

  EXPECT_CALL(*mock_cast_media_sink_service_impl(),
              DisconnectAndRemoveSink(access_code_sink2));
  access_code_cast_sink_service_->HandleMediaRouteRemovedByAccessCodeForTesting(
      &access_code_sink2);
  task_environment_.FastForwardBy(kRemoveRouteDelay);
}

TEST_F(AccessCodeCastSinkServiceTest, AddExistingSinkToMediaRouter) {
  // Ensure that the call to OpenChannel is NOT made since the cast sink already
  // exists in the media router.
  MockAddSinkResultCallback mock_callback;
  MediaSinkInternal cast_sink1 = CreateCastSink(1);
  auto cast_data = cast_sink1.cast_data();
  cast_data.discovery_type = CastDiscoveryType::kAccessCodeManualEntry;
  cast_sink1.set_cast_data(cast_data);
  mock_cast_media_sink_service_impl()->AddSinkForTest(cast_sink1);
  EXPECT_CALL(*mock_cast_media_sink_service_impl(),
              OpenChannel(cast_sink1, _, SinkSource::kAccessCode, _, _))
      .Times(0);
  EXPECT_CALL(mock_callback, Run(AddSinkResultCode::OK, Eq(cast_sink1.id())));
  OpenChannelIfNecessary(cast_sink1, mock_callback.Get(), true);
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
  OpenChannelIfNecessary(cast_sink1, mock_callback.Get(), false);
}

TEST_F(AccessCodeCastSinkServiceTest, DiscoveryDeviceMissingWithOk) {
  // Test to ensure that the add_sink_callback returns an EMPTY_RESPONSE if the
  // the device is missing.
  MockAddSinkResultCallback mock_callback;
  EXPECT_CALL(mock_callback,
              Run(AddSinkResultCode::EMPTY_RESPONSE, Eq(std::nullopt)));
  access_code_cast_sink_service_->OnAccessCodeValidatedForTesting(
      mock_callback.Get(), std::nullopt, AddSinkResultCode::OK);
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
  access_code_cast_sink_service_->OnAccessCodeValidatedForTesting(
      mock_callback.Get(), discovery_device_proto, AddSinkResultCode::OK);

  // Channel successfully opens.
  access_code_cast_sink_service_->OnChannelOpenedResultForTesting(
      mock_callback.Get(), cast_sink1, true);
  task_environment_.RunUntilIdle();
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
              Run(AddSinkResultCode::SINK_CREATION_ERROR, Eq(std::nullopt)));
  access_code_cast_sink_service_->OnAccessCodeValidatedForTesting(
      mock_callback.Get(), discovery_device_proto, AddSinkResultCode::OK);
}

TEST_F(AccessCodeCastSinkServiceTest, NonOKResultCode) {
  // Check to see that any result code that isn't OK will return that error.
  MockAddSinkResultCallback mock_callback;

  EXPECT_CALL(mock_callback,
              Run(AddSinkResultCode::AUTH_ERROR, Eq(std::nullopt)));
  access_code_cast_sink_service_->OnAccessCodeValidatedForTesting(
      mock_callback.Get(), std::nullopt, AddSinkResultCode::AUTH_ERROR);
}

TEST_F(AccessCodeCastSinkServiceTest, OnChannelOpenedSuccess) {
  // Validate callback calls for OnChannelOpened for success.
  MockAddSinkResultCallback mock_callback;
  MediaSinkInternal cast_sink1 = CreateCastSink(1);

  EXPECT_CALL(mock_callback, Run(AddSinkResultCode::OK, Eq("cast:id1")));
  access_code_cast_sink_service_->OnChannelOpenedResultForTesting(
      mock_callback.Get(), cast_sink1, true);
}

TEST_F(AccessCodeCastSinkServiceTest, OnChannelOpenedFailure) {
  // Validate callback calls for OnChannelOpened for failure.
  MockAddSinkResultCallback mock_callback;
  MediaSinkInternal cast_sink1 = CreateCastSink(1);

  EXPECT_CALL(mock_callback,
              Run(AddSinkResultCode::CHANNEL_OPEN_ERROR, Eq(std::nullopt)));
  access_code_cast_sink_service_->OnChannelOpenedResultForTesting(
      mock_callback.Get(), cast_sink1, false);
}

TEST_F(AccessCodeCastSinkServiceTest, SinkDoesntExistForPrefs) {
  // Ensure that the StoreSinkInPrefs() function returns if no sink exists in
  // the media router and no tasks are posted.
  base::MockCallback<base::OnceClosure> mock_callback;
  EXPECT_CALL(mock_callback, Run()).Times(0);
  access_code_cast_sink_service_->StoreSinkInPrefsForTesting(
      mock_callback.Get(), nullptr);
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(IsDeviceAddedTimeDictEmpty());
  EXPECT_TRUE(IsDevicesDictEmpty());
}

TEST_F(AccessCodeCastSinkServiceTest, TestFetchAndAddStoredDevices) {
  // Test that ensures OpenChannels is called after valid sinks are fetched from
  // the internal pref service.

  // Initialize histogram tester so we can ensure metrics are being collected.
  base::HistogramTester histogram_tester;
  std::vector<MediaSinkInternal> cast_sinks_ethernet = {
      CreateCastSink(1), CreateCastSink(2), CreateCastSink(3)};
  for (auto sink : cast_sinks_ethernet) {
    StoreSinkInPrefs(sink);
  }
  ExpectOpenChannels(cast_sinks_ethernet, 1);
  ExpectHasSink(cast_sinks_ethernet, 1);

  access_code_cast_sink_service_->InitAllStoredDevicesForTesting();
  task_environment_.RunUntilIdle();

  // Test to ensure that the count of remembered devices was properly logged.
  histogram_tester.ExpectBucketCount(
      "AccessCodeCast.Discovery.RememberedDevicesCount", 3, 1);
}

TEST_F(AccessCodeCastSinkServiceTest, TestChangeNetworksExpiration) {
  // Test that ensures sinks are stored on a network and then (attempted) to be
  // reopened when we connect back to any other network(including the original
  // one).
  SetDeviceDurationPrefForTesting(base::Seconds(100));
  std::vector<MediaSinkInternal> cast_sinks_ethernet = {
      CreateCastSink(1), CreateCastSink(2), CreateCastSink(3)};
  for (const auto& sink : cast_sinks_ethernet) {
    mock_cast_media_sink_service_impl()->AddSinkForTest(sink);
    StoreSinkInPrefs(sink);
  }

  // Overall this unit test should call OpenChannel for each cast sink twice.
  // This is on init stored devices and then connecting to a new network will
  // trigger a call to add every stored cast device back again.
  ExpectOpenChannels(cast_sinks_ethernet, 2);
  ExpectHasSink(cast_sinks_ethernet, 2);

  access_code_cast_sink_service_->InitAllStoredDevicesForTesting();
  task_environment_.RunUntilIdle();

  // 3 expiration timers should be set.
  EXPECT_EQ(current_session_expiration_timers().size(), 3u);

  // Don't expire the devices yet, but let some time pass.
  task_environment_.AdvanceClock(base::Seconds(75));

  EXPECT_FALSE(IsDeviceAddedTimeDictEmpty());
  EXPECT_FALSE(IsDevicesDictEmpty());

  // When the network changes, the sinks on that network should be removed. The
  // sinks should also be expired after the network changes and the expiration
  // is fully completed.
  for (const auto& sink : cast_sinks_ethernet) {
    EXPECT_CALL(*mock_cast_media_sink_service_impl(),
                DisconnectAndRemoveSink(sink))
        .Times(2);
  }

  // Connect to a new network with different sinks.
  fake_network_info_ = fake_wifi_info_;
  ChangeConnectionType(network::mojom::ConnectionType::CONNECTION_WIFI);

  task_environment_.FastForwardBy(kNetworkChangeDelay);

  // 3 expiration timers should be set still.
  EXPECT_EQ(3u, current_session_expiration_timers().size());

  task_environment_.FastForwardBy(base::Seconds(50));
  task_environment_.RunUntilIdle();

  // Now all the expiration timers should be completed and the devices should be
  // removed.
  EXPECT_TRUE(IsDeviceAddedTimeDictEmpty());
  EXPECT_TRUE(IsDevicesDictEmpty());
}

TEST_F(AccessCodeCastSinkServiceTest, TestChangeNetworksNoExpiration) {
  // Test that ensures sinks are stored on a network and then (attempted) to be
  // reopened when we connect back to any other network(including the original
  // one). Don't include expiration in this test however
  SetDeviceDurationPrefForTesting(base::Seconds(10000));

  std::vector<MediaSinkInternal> cast_sinks = {
      CreateCastSink(1), CreateCastSink(2), CreateCastSink(3)};
  for (const auto& sink : cast_sinks) {
    mock_cast_media_sink_service_impl()->AddSinkForTest(sink);
    StoreSinkInPrefs(sink);
  }
  // Overall this unit test should call OpenChannel for each cast sink twice.
  // This is on init stored devices and then connecting to a new network will
  // trigger a call to add every stored cast device back again.
  ExpectOpenChannels(cast_sinks, 2);
  ExpectHasSink(cast_sinks, 2);

  access_code_cast_sink_service_->InitAllStoredDevicesForTesting();
  task_environment_.RunUntilIdle();

  // 3 expiration timers should be set.
  EXPECT_EQ(current_session_expiration_timers().size(), 3u);

  // Don't expire the devices yet, but let some time pass.
  task_environment_.AdvanceClock(base::Seconds(75));

  EXPECT_FALSE(IsDeviceAddedTimeDictEmpty());
  EXPECT_FALSE(IsDevicesDictEmpty());

  // When the network changes, the sinks on that network should be removed.
  for (const auto& sink : cast_sinks) {
    EXPECT_CALL(*mock_cast_media_sink_service_impl(),
                DisconnectAndRemoveSink(sink));
  }
  // Connect to a new network with different sinks.
  fake_network_info_ = fake_wifi_info_;
  ChangeConnectionType(network::mojom::ConnectionType::CONNECTION_WIFI);
  task_environment_.FastForwardBy(kNetworkChangeDelay);
  task_environment_.AdvanceClock(base::Seconds(75));

  // 3 expiration timers should be set still.
  EXPECT_EQ(3u, current_session_expiration_timers().size());

  task_environment_.AdvanceClock(base::Seconds(50));

  // The expiration should still not be triggered yet.
  EXPECT_FALSE(IsDeviceAddedTimeDictEmpty());
  EXPECT_FALSE(IsDevicesDictEmpty());
}

TEST_F(AccessCodeCastSinkServiceTest,
       TestAddInvalidDevicesNoMediaSinkInternal) {
  // Test that to check that if a sink is not stored in the devies dict, no cal
  // will be made to add devies to the router or to start expiration timers.
  auto cast_sink1 = CreateCastSink(1);
  StoreSinkInPrefs(cast_sink1);
  std::vector<MediaSinkInternal> cast_sinks = {cast_sink1};

  // Remove the cast sink from the devices dict -- now the cast sink is
  // incompletely stored since it only exists in 2/3 of the prefs.
  EXPECT_CALL(*mock_cast_media_sink_service_impl(),
              OpenChannels(cast_sinks, SinkSource::kAccessCode))
      .Times(0);

  pref_updater()->RemoveSinkIdFromDevicesDict(cast_sink1.id(),
                                              base::DoNothing());
  EXPECT_FALSE(IsDeviceAddedTimeDictEmpty());
  EXPECT_TRUE(IsDevicesDictEmpty());

  // Expect that the sink id is removed from all instance in the pref service
  // when we try to init connections with a corrupted device entry.
  access_code_cast_sink_service_->InitAllStoredDevicesForTesting();
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(current_session_expiration_timers().empty());
}

TEST_F(AccessCodeCastSinkServiceTest, TestCalculateDurationTillExpiration) {
  // Test that checks all variations of calculated duration.
  SetDeviceDurationPrefForTesting(base::Seconds(100));

  const MediaSinkInternal cast_sink1 = CreateCastSink(1);
  // Since there are no stored sinks, this should return 0 seconds.
  {
    base::test::TestFuture<base::TimeDelta> duration;
    access_code_cast_sink_service_->CalculateDurationTillExpirationForTesting(
        cast_sink1.id(), duration.GetCallback());
    EXPECT_EQ(base::Seconds(0), duration.Get());
  }

  StoreSinkInPrefs(cast_sink1);

  // No mock time has passed since the start of the test, so the returned
  // duration should be the same as the set pref.
  {
    base::test::TestFuture<base::TimeDelta> duration;
    access_code_cast_sink_service_->CalculateDurationTillExpirationForTesting(
        cast_sink1.id(), duration.GetCallback());
    EXPECT_EQ(base::Seconds(100), duration.Get());
  }

  task_environment_.AdvanceClock(base::Seconds(50));

  // 50s mock time has passed since the start of the test, so the returned
  // duration should now be modified.
  {
    base::test::TestFuture<base::TimeDelta> duration;
    access_code_cast_sink_service_->CalculateDurationTillExpirationForTesting(
        cast_sink1.id(), duration.GetCallback());
    EXPECT_EQ(base::Seconds(50), duration.Get());
  }

  task_environment_.AdvanceClock(base::Seconds(1000));

  // More mock time has passed then the pref, it should return 0 instead of a
  // negative number.
  {
    base::test::TestFuture<base::TimeDelta> duration;
    access_code_cast_sink_service_->CalculateDurationTillExpirationForTesting(
        cast_sink1.id(), duration.GetCallback());
    EXPECT_EQ(base::Seconds(0), duration.Get());
  }
}

TEST_F(AccessCodeCastSinkServiceTest, TestSetExpirationTimer) {
  // Test to see that setting the expiration timer overwrites any timers that
  // are currently running.
  SetDeviceDurationPrefForTesting(base::Seconds(100));
  const MediaSinkInternal cast_sink1 = CreateCastSink(1);
  StoreSinkInPrefs(cast_sink1);
  SetExpirationTimerAndExpectTimerRunning(cast_sink1);

  // The expiration should not have triggered yet.
  task_environment_.AdvanceClock(base::Seconds(75));

  EXPECT_FALSE(IsDeviceAddedTimeDictEmpty());
  EXPECT_FALSE(IsDevicesDictEmpty());

  // Add the device again, the expiration timer should be reset and 150 seconds
  // passed will not reset the cast device.
  StoreSinkInPrefs(cast_sink1);
  SetExpirationTimerAndExpectTimerRunning(cast_sink1);

  task_environment_.AdvanceClock(base::Seconds(75));

  EXPECT_FALSE(IsDeviceAddedTimeDictEmpty());
  EXPECT_FALSE(IsDevicesDictEmpty());
}

TEST_F(AccessCodeCastSinkServiceTest, TestResetExpirationTimersNetworkChange) {
  // Test to check all expiration timers are restarted after network is changed.
  SetDeviceDurationPrefForTesting(base::Seconds(10000));
  std::vector<MediaSinkInternal> cast_sinks = {
      CreateCastSink(1), CreateCastSink(2), CreateCastSink(3)};
  for (const auto& sink : cast_sinks) {
    StoreSinkInPrefs(sink);
    SetExpirationTimerAndExpectTimerRunning(sink);
  }
  fake_network_info_ = fake_wifi_info_;
  ChangeConnectionType(network::mojom::ConnectionType::CONNECTION_WIFI);
  task_environment_.FastForwardBy(kNetworkChangeDelay);

  task_environment_.AdvanceClock(base::Seconds(100));

  // The timers should be restarted and they should still be running since
  // expiration has not occurred yet.
  for (const auto& sink : cast_sinks) {
    EXPECT_TRUE(GetExpirationTimer(sink.id())->IsRunning());
  }
}

TEST_F(AccessCodeCastSinkServiceTest, TestResetExpirationTimersShutdown) {
  // Test to check all expiration timers are cleared after the service is
  // shutdown.
  SetDeviceDurationPrefForTesting(base::Seconds(10000));
  std::vector<MediaSinkInternal> cast_sinks_ethernet = {
      CreateCastSink(1), CreateCastSink(2), CreateCastSink(3)};

  for (const auto& sink : cast_sinks_ethernet) {
    StoreSinkInPrefs(sink);
    SetExpirationTimerAndExpectTimerRunning(sink);
  }

  access_code_cast_sink_service_->ShutdownForTesting();
  EXPECT_TRUE(current_session_expiration_timers().empty());
}

TEST_F(AccessCodeCastSinkServiceTest, TestChangeEnabledPref) {
  // Test to ensure that all existing sinks are removed, all timers are reset,
  // and all prefs related to access code casting are removed.
  SetDeviceDurationPrefForTesting(base::Seconds(10000));

  const MediaSinkInternal cast_sink = CreateCastSink(1);
  StoreSinkInPrefs(cast_sink);
  mock_cast_media_sink_service_impl()->AddSinkForTest(cast_sink);
  AddSinkToMediaRouter(cast_sink);
  SetExpirationTimerAndExpectTimerRunning(cast_sink);

  EXPECT_FALSE(IsDeviceAddedTimeDictEmpty());
  EXPECT_FALSE(IsDevicesDictEmpty());

  EXPECT_CALL(*mock_cast_media_sink_service_impl(),
              DisconnectAndRemoveSink(cast_sink));
  GetTestingPrefs()->SetManagedPref(prefs::kAccessCodeCastEnabled,
                                    base::Value(false));
  task_environment_.FastForwardBy(kRemoveRouteDelay);

  EXPECT_TRUE(current_session_expiration_timers().empty());
  EXPECT_TRUE(IsDeviceAddedTimeDictEmpty());
  EXPECT_TRUE(IsDevicesDictEmpty());
}

TEST_F(AccessCodeCastSinkServiceTest, TestChangeDurationPref) {
  // Test to ensure timers are reset whenever the duration pref changes.
  SetDeviceDurationPrefForTesting(base::Seconds(10000));

  const MediaSinkInternal cast_sink = CreateCastSink(1);
  StoreSinkInPrefs(cast_sink);
  SetExpirationTimerAndExpectTimerRunning(cast_sink);
  EXPECT_EQ(
      base::Seconds(10000) + AccessCodeCastSinkService::kExpirationTimerDelay,
      GetExpirationTimer(cast_sink.id())->GetCurrentDelay());

  SetDeviceDurationPrefForTesting(base::Seconds(100));
  EXPECT_TRUE(GetExpirationTimer(cast_sink.id())->IsRunning());
  EXPECT_EQ(
      base::Seconds(100) + AccessCodeCastSinkService::kExpirationTimerDelay,
      GetExpirationTimer(cast_sink.id())->GetCurrentDelay());
}

TEST_F(AccessCodeCastSinkServiceTest, TestChangeNetworkWithRouteActive) {
  // This test ensures that a call to remove a media sink will NOT be made if
  // there is currently an active route.
  SetDeviceDurationPrefForTesting(base::Seconds(10000));
  const MediaSinkInternal cast_sink1 = CreateCastSink(1);

  mock_cast_media_sink_service_impl()->AddSinkForTest(cast_sink1);
  StoreSinkInPrefs(cast_sink1);
  MediaRoute media_route_cast = CreateRouteForTesting(cast_sink1.id());
  std::vector<MediaRoute> route_list = {media_route_cast};

  EXPECT_CALL(*mock_cast_media_sink_service_impl(), HasSink(cast_sink1.id()));
  SimulateRoutesUpdated(route_list);

  // Since the route has not changed, no call to remove the sink should have
  // been made after the network changed.
  EXPECT_CALL(*mock_cast_media_sink_service_impl(),
              DisconnectAndRemoveSink(cast_sink1))
      .Times(0);

  fake_network_info_ = fake_wifi_info_;
  ChangeConnectionType(network::mojom::ConnectionType::CONNECTION_WIFI);
  task_environment_.FastForwardBy(kNetworkChangeDelay);

  // The sink should NOT now be removed from the media router since it was not
  // expired.
  EXPECT_CALL(*mock_cast_media_sink_service_impl(),
              DisconnectAndRemoveSink(cast_sink1))
      .Times(0);
  SimulateRoutesUpdated({});

  // The sink did not expire in this situation so it should still exist in the
  // pref service.
  EXPECT_FALSE(IsDeviceAddedTimeDictEmpty());
  EXPECT_FALSE(IsDevicesDictEmpty());
}

TEST_F(AccessCodeCastSinkServiceTest,
       TestChangeNetworkWithRouteActiveExpiration) {
  // This test ensures that a call to remove a media sink will NOT be made if
  // there is currently an active route, this time when the sink has expired
  // before the network has changed.
  SetDeviceDurationPrefForTesting(base::Seconds(100));
  MediaSinkInternal cast_sink1 = CreateCastSink(1);

  auto cast_data = cast_sink1.cast_data();
  cast_data.discovery_type = CastDiscoveryType::kAccessCodeRememberedDevice;
  cast_sink1.set_cast_data(cast_data);

  mock_cast_media_sink_service_impl()->AddSinkForTest(cast_sink1);
  StoreSinkInPrefs(cast_sink1);
  SetExpirationTimerAndExpectTimerRunning(cast_sink1);
  MediaRoute media_route_cast = CreateRouteForTesting(cast_sink1.id());
  std::vector<MediaRoute> route_list = {media_route_cast};

  EXPECT_CALL(*mock_cast_media_sink_service_impl(), HasSink(cast_sink1.id()));
  SimulateRoutesUpdated(route_list);

  // Since the route has not changed, no call to remove the sink should have
  // been made after the network changed.
  EXPECT_CALL(*mock_cast_media_sink_service_impl(),
              DisconnectAndRemoveSink(cast_sink1))
      .Times(0);

  // Expire the sink
  task_environment_.AdvanceClock(base::Seconds(300));

  fake_network_info_ = fake_wifi_info_;
  ChangeConnectionType(network::mojom::ConnectionType::CONNECTION_WIFI);
  task_environment_.FastForwardBy(kNetworkChangeDelay);

  // The sink should now be removed from the media router.
  EXPECT_CALL(*mock_cast_media_sink_service_impl(),
              DisconnectAndRemoveSink(cast_sink1));
  SimulateRoutesUpdated({});
  task_environment_.FastForwardBy(kRemoveRouteDelay);

  // The sink did expire in this situation so it should not exist in the pref
  // service.
  EXPECT_TRUE(IsDeviceAddedTimeDictEmpty());
  EXPECT_TRUE(IsDevicesDictEmpty());
}

TEST_F(AccessCodeCastSinkServiceTest, DiscoverSinkWithNoMediaRouter) {
  // Make sure that a nullptr media router will cause an error and immediately
  // returned.
  MockAddSinkResultCallback mock_callback;

  // Shut down the access code cast sink service causing the media router to
  // become nullptr.
  access_code_cast_sink_service_->ShutdownForTesting();

  EXPECT_CALL(mock_callback, Run(AddSinkResultCode::INTERNAL_MEDIA_ROUTER_ERROR,
                                 Eq(std::nullopt)));
  access_code_cast_sink_service_->DiscoverSink("", mock_callback.Get());
}

TEST_F(AccessCodeCastSinkServiceTest,
       TestCheckMediaSinkForExpirationNoExpiration) {
  // Demonstrates that checking a media sink for expiration will return if it
  // hasn't expired yet.

  const MediaSinkInternal cast_sink = CreateCastSink(1);
  SetDeviceDurationPrefForTesting(base::Seconds(100));

  StoreSinkInPrefs(cast_sink);
  SetExpirationTimerAndExpectTimerRunning(cast_sink);

  // Expect that CheckMediaSinkForExpiration() will simply return since the
  // timer has not expired yet.
  access_code_cast_sink_service_->CheckMediaSinkForExpiration(cast_sink.id());
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(GetExpirationTimer(cast_sink.id())->IsRunning());
}

TEST_F(AccessCodeCastSinkServiceTest,
       TestCheckMediaSinkForExpirationBeforeDelay) {
  // Demonstrates that checking a media sink for expiration will fire it before
  // OnExpiration is called because of the kExpirationDelay.

  const MediaSinkInternal cast_sink = CreateCastSink(1);
  SetDeviceDurationPrefForTesting(base::Seconds(0));

  mock_cast_media_sink_service_impl()->AddSinkForTest(cast_sink);
  StoreSinkInPrefs(cast_sink);
  SetExpirationTimerAndExpectTimerRunning(cast_sink);

  // There is a brief delay that is added to any expiration timer. We want to
  // check the time between the delay and zero seconds (instant expiration) to
  // ensure that we can manually trigger this expiration and override the delay.
  task_environment_.AdvanceClock(
      AccessCodeCastSinkService::kExpirationTimerDelay - base::Seconds(10));
  EXPECT_TRUE(GetExpirationTimer(cast_sink.id())->IsRunning());

  access_code_cast_sink_service_->CheckMediaSinkForExpiration(cast_sink.id());
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(current_session_expiration_timers().empty());

  // The sink should now be removed from the media router.
  EXPECT_CALL(*mock_cast_media_sink_service_impl(),
              DisconnectAndRemoveSink(cast_sink));
  task_environment_.FastForwardBy(kRemoveRouteDelay);

  // The sink did expire in this situation so it should not exist in the pref
  // service.
  EXPECT_TRUE(IsDeviceAddedTimeDictEmpty());
  EXPECT_TRUE(IsDevicesDictEmpty());
}

TEST_F(AccessCodeCastSinkServiceTest,
       TestCheckMediaSinkForExpirationAfterDelay) {
  // Demonstrates that if an expiration timer is no longer running, then we will
  // attempt to expire the sink since this means the sink has expired but the
  // route was still active.

  MediaSinkInternal cast_sink = CreateCastSink(1);
  cast_sink.cast_data().discovery_type =
      CastDiscoveryType::kAccessCodeManualEntry;
  SetDeviceDurationPrefForTesting(base::Seconds(0));

  mock_cast_media_sink_service_impl()->AddSinkForTest(cast_sink);
  StoreSinkInPrefs(cast_sink);

  SetExpirationTimerAndExpectTimerRunning(cast_sink);
  MediaRoute media_route_cast = CreateRouteForTesting(cast_sink.id());
  media_route_cast.set_local(true);
  SimulateRoutesUpdated({media_route_cast});
  EXPECT_TRUE(GetExpirationTimer(cast_sink.id())->IsRunning());

  // Expire the sink while there is still an active local route.
  task_environment_.FastForwardBy(
      AccessCodeCastSinkService::kExpirationTimerDelay + base::Seconds(100));
  EXPECT_FALSE(GetExpirationTimer(cast_sink.id())->IsRunning());

  // The sink should NOT be removed from the media router since there is still
  // an open local route.
  EXPECT_CALL(*mock_cast_media_sink_service_impl(),
              DisconnectAndRemoveSink(cast_sink))
      .Times(0);

  // The sink did NOT expire in this situation so it should exist in the
  // pref service.
  EXPECT_FALSE(IsDeviceAddedTimeDictEmpty());
  EXPECT_FALSE(IsDevicesDictEmpty());

  // Finally remove the route from the media router. The sink should now be
  // removed from the media router.
  EXPECT_CALL(*mock_cast_media_sink_service_impl(),
              DisconnectAndRemoveSink(cast_sink));
  SimulateRoutesUpdated({});
  task_environment_.FastForwardBy(kRemoveRouteDelay);

  // The sink did expire in this situation so it should not exist in the pref
  // service.
  EXPECT_TRUE(IsDeviceAddedTimeDictEmpty());
  EXPECT_TRUE(IsDevicesDictEmpty());
}

TEST_F(AccessCodeCastSinkServiceTest, TestOfflineDiscoverSink) {
  // Test to ensure that discover sink triggers a network error callback if we
  // are offline.
  MockAddSinkResultCallback mock_callback;

  network::TestNetworkConnectionTracker::GetInstance()->SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_NONE);
  EXPECT_CALL(mock_callback,
              Run(AddSinkResultCode::SERVICE_NOT_PRESENT, Eq(std::nullopt)));

  access_code_cast_sink_service_->DiscoverSink("", mock_callback.Get());
  task_environment_.RunUntilIdle();
}

TEST_F(AccessCodeCastSinkServiceTest, RefreshStoredDeviceInfo) {
  // If a device is already in the list, then its info changes. To store a
  // device, we take its info from the entry in the media router, so checking
  // the value of the stored device pref suffices
  SetDeviceDurationPrefForTesting(base::Seconds(100));

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

  StoreSinkInPrefs(existing_sink_1);
  StoreSinkInPrefs(existing_sink_2);

  // Try to add new_sink_1, which has an id that matches an existing sink.
  MockAddSinkResultCallback mock_callback;
  OpenChannelIfNecessary(new_sink_1, mock_callback.Get(), /*has_sink=*/true);

  // Now we expect that the name for the sink with the id of existing_sink_1
  // should have changed.
  base::test::TestFuture<base::Value::Dict> devices_dict;
  pref_updater()->GetDevicesDict(devices_dict.GetCallback());
  auto* sink_1_dict = devices_dict.Get().FindDict(existing_sink_1.id());
  auto* sink_2_dict = devices_dict.Get().FindDict(existing_sink_2.id());
  EXPECT_TRUE(sink_1_dict);
  EXPECT_TRUE(sink_2_dict);
  EXPECT_EQ(*sink_1_dict, CreateValueDictFromMediaSinkInternal(new_sink_1));
  EXPECT_EQ(*sink_2_dict,
            CreateValueDictFromMediaSinkInternal(existing_sink_2));
}

TEST_F(AccessCodeCastSinkServiceTest, RefreshExistingDeviceName) {
  SetDeviceDurationPrefForTesting(base::Seconds(100));

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

  // Try to add new_sink_1, which has an id that matches an existing sink.
  MockAddSinkResultCallback mock_callback;
  OpenChannelIfNecessary(new_sink_1, mock_callback.Get(), /*has_sink=*/true);

  EXPECT_EQ(mock_cast_media_sink_service_impl()
                ->GetSinkById(existing_sink_1.id())
                ->sink()
                .name(),
            new_sink_1.sink().name());
}

TEST_F(AccessCodeCastSinkServiceTest, RefreshStoredDeviceTimer) {
  // If a device is already stored, then its timer is reset.
  SetDeviceDurationPrefForTesting(base::Seconds(100));

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

  StoreSinkInPrefs(cast_sink1);
  StoreSinkInPrefs(cast_sink2);

  SetExpirationTimerAndExpectTimerRunning(cast_sink1);
  SetExpirationTimerAndExpectTimerRunning(cast_sink2);

  // Forward the time and trigger the timer to be refreshed for one of the
  // devices.
  task_environment_.FastForwardBy(base::Seconds(50));

  MockAddSinkResultCallback mock_callback;
  OpenChannelIfNecessary(cast_sink1, mock_callback.Get(), /*has_sink=*/true);

  // Check that both devices still have an expiration timer as expected.
  EXPECT_EQ(current_session_expiration_timers().size(), 2u);

  // Forward the time again, so an un-refreshed device would expire.
  task_environment_.FastForwardBy(base::Seconds(75));

  // Check that the proper device has expired by checking if its expiration
  // timer is still running.
  EXPECT_TRUE(GetExpirationTimer(cast_sink1.id())->IsRunning());
  EXPECT_FALSE(GetExpirationTimer(cast_sink2.id())->IsRunning());

  // Forward the time again, so all devices expire.
  task_environment_.FastForwardBy(base::Seconds(50));

  // Check that the proper device has expired by checking if its expiration
  // timer is still running.
  EXPECT_FALSE(GetExpirationTimer(cast_sink1.id())->IsRunning());
  EXPECT_FALSE(GetExpirationTimer(cast_sink2.id())->IsRunning());
}

TEST_F(AccessCodeCastSinkServiceTest, HandleMediaRouteAdded) {
  // Initialize histogram tester so we can ensure metrics are collected.
  base::HistogramTester histogram_tester;

  // Set duration pref since it will be recorded in metrics.
  SetDeviceDurationPrefForTesting(base::Seconds(10));

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

  access_code_cast_sink_service_->HandleMediaRouteAddedForTesting(
      fake_route_1, true, MediaSource::ForAnyTab(), &cast_sink1);

  // The histogram should not be logged to after a non access code route starts.
  histogram_tester.ExpectTotalCount(
      "AccessCodeCast.Discovery.DeviceDurationOnRoute", 0);

  access_code_cast_sink_service_->HandleMediaRouteAddedForTesting(
      fake_route_2, true, MediaSource::ForAnyTab(), &cast_sink2);

  // The histogram should log when a route starts to a new access code device.
  histogram_tester.ExpectBucketCount(
      "AccessCodeCast.Discovery.DeviceDurationOnRoute", 10, 1);

  access_code_cast_sink_service_->HandleMediaRouteAddedForTesting(
      fake_route_3, true, MediaSource::ForAnyTab(), &cast_sink3);

  // The histogram should log when a route starts to a saved access code device.
  histogram_tester.ExpectBucketCount(
      "AccessCodeCast.Discovery.DeviceDurationOnRoute", 10, 2);

  // Ensure various pref values are can be logged.
  SetDeviceDurationPrefForTesting(base::Seconds(100));
  access_code_cast_sink_service_->HandleMediaRouteAddedForTesting(
      fake_route_2, true, MediaSource::ForAnyTab(), &cast_sink2);
  histogram_tester.ExpectBucketCount(
      "AccessCodeCast.Discovery.DeviceDurationOnRoute", 100, 1);

  SetDeviceDurationPrefForTesting(base::Seconds(1000));
  access_code_cast_sink_service_->HandleMediaRouteAddedForTesting(
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
  StoreSinkInPrefs(cast_sink1);
  SetExpirationTimerAndExpectTimerRunning(cast_sink1);
  MediaRoute media_route_cast = CreateRouteForTesting(cast_sink1.id());
  std::vector<MediaRoute> route_list = {media_route_cast};

  // Simulate that this cast sink has an open route.
  SimulateRoutesUpdated(route_list);

  // The histogram should start with nothing logged.
  histogram_tester.ExpectTotalCount("AccessCodeCast.Session.RouteDuration", 0);
  task_environment_.AdvanceClock(base::Seconds(30));

  // Simulate that all routes have ended.
  SimulateRoutesUpdated({});

  // The histogram should have recorded since a local route that ended.
  histogram_tester.ExpectTotalCount("AccessCodeCast.Session.RouteDuration", 1);

  // Now add a non-local route
  std::string route_id =
      "urn:x-org.chromium:media:route:2/" + cast_sink1.id() + "/http://foo.com";
  MediaRoute remote_route = MediaRoute(route_id, MediaSource("access_code"),
                                       cast_sink1.id(), "access_sink",
                                       /*is_local=*/false);
  SimulateRoutesUpdated({remote_route});
  task_environment_.AdvanceClock(base::Seconds(30));

  // Now simulate ending the route
  SimulateRoutesUpdated({});

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
  SimulateRoutesUpdated(route_list);

  // The cast sink was not added by an access code so no histogram should be
  // recorded.
  histogram_tester.ExpectTotalCount("AccessCodeCast.Session.RouteDuration", 0);
}

TEST_F(AccessCodeCastSinkServiceTest, RestartExpirationTimerDoesntResetTimer) {
  // Test to check that expiration timers are not reset when they are re-added
  // to the media router.
  SetDeviceDurationPrefForTesting(base::Seconds(1000));

  const MediaSinkInternal cast_sink1 = CreateCastSink(1);

  StoreSinkInPrefs(cast_sink1);
  SetExpirationTimerAndExpectTimerRunning(cast_sink1);

  // Advance the expiration timer by 500 seconds before restarting the service.
  task_environment_.AdvanceClock(base::Seconds(500));
  {
    base::test::TestFuture<base::TimeDelta> duration;
    access_code_cast_sink_service_->CalculateDurationTillExpirationForTesting(
        cast_sink1.id(), duration.GetCallback());
    EXPECT_EQ(base::Seconds(500), duration.Get());
  }

  MockAccessCodeCastPrefUpdater* mock_pref_updater =
      static_cast<MockAccessCodeCastPrefUpdater*>(pref_updater());
  base::Value::Dict devices_dict_copy =
      mock_pref_updater->devices_dict().Clone();
  base::Value::Dict device_added_time_dict_copy =
      mock_pref_updater->device_added_time_dict().Clone();

  // Shutdown the access code cast sink service.
  access_code_cast_sink_service_->ShutdownForTesting();
  access_code_cast_sink_service_.reset();

  auto new_pref_updater = std::make_unique<MockAccessCodeCastPrefUpdater>();
  new_pref_updater->set_devices_dict(std::move(devices_dict_copy));
  new_pref_updater->set_device_added_time_dict(
      std::move(device_added_time_dict_copy));

  access_code_cast_sink_service_ =
      base::WrapUnique(new AccessCodeCastSinkService(
          &profile_, router_.get(), cast_media_sink_service_impl_.get(),
          discovery_network_monitor_.get(), GetTestingPrefs(),
          std::move(new_pref_updater)));
  access_code_cast_sink_service_->SetTaskRunnerForTesting(task_runner());

  // On service recreation the duration should be the same and NOT be reset.
  {
    base::test::TestFuture<base::TimeDelta> duration;
    access_code_cast_sink_service_->CalculateDurationTillExpirationForTesting(
        cast_sink1.id(), duration.GetCallback());
    EXPECT_EQ(base::Seconds(500), duration.Get());
  }
}

TEST_F(AccessCodeCastSinkServiceTest, AddRouteCallsHandleMediaRoute) {
  // Test that adding a route will properly call HandleMediaRouteAdded and
  // metrics.
  SetDeviceDurationPrefForTesting(base::Seconds(10));

  // Initialize histogram tester so we can ensure metrics are collected.
  base::HistogramTester histogram_tester;

  MediaSinkInternal access_code_sink = CreateCastSink(1);
  access_code_sink.cast_data().discovery_type =
      CastDiscoveryType::kAccessCodeManualEntry;
  mock_cast_media_sink_service_impl()->AddSinkForTest(access_code_sink);

  MediaRoute media_route_cast = CreateRouteForTesting(access_code_sink.id());
  std::vector<MediaRoute> route_list = {media_route_cast};

  // Simulate that this cast sink has an open route.
  SimulateRoutesUpdated(route_list);

  // The cast sink was added by an access code so the histogram should be
  // recorded.
  histogram_tester.ExpectBucketCount(
      "AccessCodeCast.Discovery.DeviceDurationOnRoute", 10, 1);
}

TEST_F(AccessCodeCastSinkServiceTest, InitializePrefUpdater) {
  auto cast_sink = CreateCastSink(1);
  base::Value::Dict devices_dict;
  devices_dict.Set(cast_sink.id(),
                   CreateValueDictFromMediaSinkInternal(cast_sink));
  GetTestingPrefs()->SetDict(prefs::kAccessCodeCastDevices,
                             std::move(devices_dict));
  ExpectOpenChannels({cast_sink}, 1);
  ExpectHasSink({cast_sink}, 1);

  // InitializePrefUpdater() should instnatiate `pref_updater_` as
  // AccessCodeCastPrefUpdaterImpl.
  access_code_cast_sink_service_->ResetPrefUpdaterForTesting();
  access_code_cast_sink_service_->InitializePrefUpdaterForTesting();
  task_environment_.RunUntilIdle();
}

}  // namespace media_router
