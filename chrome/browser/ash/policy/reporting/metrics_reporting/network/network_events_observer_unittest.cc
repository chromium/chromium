// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/network/network_events_observer.h"

#include <memory>
#include <string>
#include <utility>

#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "chromeos/ash/components/dbus/debug_daemon/debug_daemon_client.h"
#include "chromeos/ash/components/dbus/shill/shill_service_client.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/ash/components/network/managed_state.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/tether_constants.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "chromeos/services/network_health/public/mojom/network_health_types.mojom.h"
#include "components/reporting/proto/synced/metric_data.pb.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

using chromeos::network_health::mojom::NetworkState;
using testing::Eq;
using testing::StrEq;

namespace reporting {
namespace {

constexpr int kSignalStrength = 10;
constexpr int kGoodSignalStrengthRssi = -50;
constexpr int kLowSignalStrengthRssi = -75;
constexpr int kVeryLowSignalStrengthRssi = -85;

constexpr char kWifiConfig[] =
    R"({"GUID": "%s", "Type": "wifi", "State": "%s",
    "WiFi.SignalStrengthRssi": %d})";

// Guids.
constexpr char kWifiGuid[] = "wifi-guid";
constexpr char kWifiIdleGuid[] = "wifi-idle-guid";
constexpr char kCellularGuid[] = "cellular-guid";
constexpr char kVpnGuid[] = "vpn-guid";
constexpr char kTetherGuid[] = "tether-guid";
// Service paths.
constexpr char kWifiServicePath[] = "/service/wlan";
constexpr char kWifiIdleServicePath[] = "/service/wifi-idle";
constexpr char kCellularServicePath[] = "/service/cellular";
constexpr char kVpnServicePath[] = "/service/vpn";
constexpr char kTetherServicePath[] = "/service/tether";

class NetworkEventsObserverTestHelper {
 public:
  NetworkEventsObserverTestHelper() = default;

  NetworkEventsObserverTestHelper(const NetworkEventsObserverTestHelper&) =
      delete;
  NetworkEventsObserverTestHelper& operator=(
      const NetworkEventsObserverTestHelper&) = delete;

  ~NetworkEventsObserverTestHelper() = default;

  void SetUp() {
    ash::DebugDaemonClient::InitializeFake();

    // TODO(b/278643115) Remove LoginState dependency.
    ash::LoginState::Initialize();

    const AccountId account_id = AccountId::FromUserEmail("test@test");
    auto fake_user_manager = std::make_unique<user_manager::FakeUserManager>();
    fake_user_manager->AddUser(account_id);
    fake_user_manager->UserLoggedIn(account_id,
                                    network_handler_test_helper_.UserHash(),
                                    /*browser_restart=*/false,
                                    /*is_child=*/false);
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(fake_user_manager));

    ash::LoginState::Get()->SetLoggedInState(
        ash::LoginState::LOGGED_IN_ACTIVE,
        ash::LoginState::LOGGED_IN_USER_REGULAR);

    network_handler_test_helper_.AddDefaultProfiles();
    network_handler_test_helper_.ResetDevicesAndServices();
    network_handler_test_helper_.manager_test()->AddTechnology(
        ::ash::kTypeTether, true);

    auto* const service_client = network_handler_test_helper_.service_test();
    service_client->AddService(kWifiServicePath, kWifiGuid, "wifi-name",
                               shill::kTypeWifi, shill::kStateReady,
                               /*visible=*/true);
    service_client->AddService(kWifiIdleServicePath, kWifiIdleGuid,
                               "wifi-idle-name", shill::kTypeWifi,
                               shill::kStateIdle, /*visible=*/true);
    service_client->AddService(kVpnServicePath, kVpnGuid, "vpn-name",
                               shill::kTypeVPN, shill::kStateReady,
                               /*visible=*/true);
    service_client->AddService(kTetherServicePath, kTetherGuid, "tether-name",
                               ash::kTypeTether, shill::kStateReady,
                               /*visible=*/true);
    service_client->AddService(kCellularServicePath, kCellularGuid,
                               "cellular-network-name", shill::kTypeCellular,
                               shill::kStateAssociation, /*visible=*/true);
    service_client->SetServiceProperty(
        kCellularServicePath, shill::kIccidProperty, base::Value("test_iccid"));
    task_environment_.RunUntilIdle();
  }

  void TearDown() {
    scoped_user_manager_.reset();
    ash::LoginState::Shutdown();
    ash::DebugDaemonClient::Shutdown();
  }

  ash::NetworkHandlerTestHelper* network_handler_test_helper() {
    return &network_handler_test_helper_;
  }

 private:
  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;

  ash::NetworkHandlerTestHelper network_handler_test_helper_;
  ash::system::ScopedFakeStatisticsProvider statistics_provider_;
};

class NetworkEventsObserverSignalStrengthTest : public ::testing::Test {
 protected:
  void SetUp() override { network_events_observer_test_helper_.SetUp(); }

  void TearDown() override { network_events_observer_test_helper_.TearDown(); }

  ash::NetworkHandlerTestHelper* network_handler_test_helper() {
    return network_events_observer_test_helper_.network_handler_test_helper();
  }

  void SetFeatureEnabled(bool enabled) {
    scoped_feature_list_.InitWithFeatureState(kEnableWifiSignalEventsReporting,
                                              enabled);
  }

 private:
  NetworkEventsObserverTestHelper network_events_observer_test_helper_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(NetworkEventsObserverSignalStrengthTest, InitiallyLowSignal) {
  SetFeatureEnabled(true);

  const std::string service_config_low_signal = base::StringPrintf(
      kWifiConfig, kWifiGuid, shill::kStateReady, kLowSignalStrengthRssi);
  std::string service_path = network_handler_test_helper()->ConfigureService(
      service_config_low_signal);
  ASSERT_THAT(service_path, Eq(kWifiServicePath));

  NetworkEventsObserver network_events_observer;
  MetricData result_metric_data;
  base::test::TestFuture<MetricData> test_future;

  network_events_observer.SetOnEventObservedCallback(
      test_future.GetRepeatingCallback());
  network_events_observer.SetReportingEnabled(/*is_enabled=*/true);
  result_metric_data = test_future.Take();

  ASSERT_TRUE(result_metric_data.has_event_data());
  EXPECT_THAT(result_metric_data.event_data().type(),
              Eq(MetricEventType::WIFI_SIGNAL_STRENGTH_LOW));
  ASSERT_TRUE(result_metric_data.has_telemetry_data());
  ASSERT_TRUE(result_metric_data.telemetry_data().has_networks_telemetry());
  ASSERT_TRUE(result_metric_data.telemetry_data()
                  .networks_telemetry()
                  .has_signal_strength_event_data());
  EXPECT_THAT(result_metric_data.telemetry_data()
                  .networks_telemetry()
                  .signal_strength_event_data()
                  .guid(),
              Eq(kWifiGuid));
  EXPECT_THAT(result_metric_data.telemetry_data()
                  .networks_telemetry()
                  .signal_strength_event_data()
                  .signal_strength_dbm(),
              Eq(kLowSignalStrengthRssi));

  std::string service_config_very_low_signal = base::StringPrintf(
      kWifiConfig, kWifiGuid, shill::kStateReady, kVeryLowSignalStrengthRssi);
  service_path = network_handler_test_helper()->ConfigureService(
      service_config_very_low_signal);
  ASSERT_THAT(service_path, Eq(kWifiServicePath));

  network_events_observer.OnSignalStrengthChanged(
      kWifiGuid,
      ::chromeos::network_health::mojom::UInt32Value::New(kSignalStrength));
  base::RunLoop().RunUntilIdle();

  // Low signal strength event already reported.
  ASSERT_FALSE(test_future.IsReady());

  std::string service_config_good_signal = base::StringPrintf(
      kWifiConfig, kWifiGuid, shill::kStateReady, kGoodSignalStrengthRssi);
  service_path = network_handler_test_helper()->ConfigureService(
      service_config_good_signal);
  ASSERT_THAT(service_path, Eq(kWifiServicePath));

  network_events_observer.OnSignalStrengthChanged(
      kWifiGuid,
      ::chromeos::network_health::mojom::UInt32Value::New(kSignalStrength));
  result_metric_data = test_future.Take();

  ASSERT_TRUE(result_metric_data.has_event_data());
  EXPECT_THAT(result_metric_data.event_data().type(),
              Eq(MetricEventType::WIFI_SIGNAL_STRENGTH_RECOVERED));
  ASSERT_TRUE(result_metric_data.has_telemetry_data());
  ASSERT_TRUE(result_metric_data.telemetry_data().has_networks_telemetry());
  ASSERT_TRUE(result_metric_data.telemetry_data()
                  .networks_telemetry()
                  .has_signal_strength_event_data());
  EXPECT_THAT(result_metric_data.telemetry_data()
                  .networks_telemetry()
                  .signal_strength_event_data()
                  .guid(),
              Eq(kWifiGuid));
  EXPECT_THAT(result_metric_data.telemetry_data()
                  .networks_telemetry()
                  .signal_strength_event_data()
                  .signal_strength_dbm(),
              Eq(kGoodSignalStrengthRssi));
}

TEST_F(NetworkEventsObserverSignalStrengthTest, WifiNotConnected) {
  SetFeatureEnabled(true);

  network_handler_test_helper()->ResetDevicesAndServices();
  auto* const service_client = network_handler_test_helper()->service_test();
  service_client->AddService(kWifiIdleServicePath, kWifiIdleGuid,
                             "wifi-idle-name", shill::kTypeWifi,
                             shill::kStateIdle, /*visible=*/true);
  base::RunLoop().RunUntilIdle();

  std::string idle_service_config = base::StringPrintf(
      kWifiConfig, kWifiIdleGuid, shill::kStateIdle, kLowSignalStrengthRssi);
  std::string idle_service_path =
      network_handler_test_helper()->ConfigureService(idle_service_config);
  ASSERT_THAT(idle_service_path, Eq(kWifiIdleServicePath));

  NetworkEventsObserver network_events_observer;
  base::test::TestFuture<MetricData> test_future;

  network_events_observer.SetOnEventObservedCallback(
      test_future.GetRepeatingCallback());
  network_events_observer.SetReportingEnabled(/*is_enabled=*/true);
  base::RunLoop().RunUntilIdle();

  network_events_observer.OnSignalStrengthChanged(
      kWifiIdleGuid,
      ::chromeos::network_health::mojom::UInt32Value::New(kSignalStrength));
  base::RunLoop().RunUntilIdle();

  ASSERT_FALSE(test_future.IsReady());
}

TEST_F(NetworkEventsObserverSignalStrengthTest, WifiConnecting) {
  SetFeatureEnabled(true);

  network_handler_test_helper()->ResetDevicesAndServices();
  auto* const service_client = network_handler_test_helper()->service_test();
  service_client->AddService(kWifiServicePath, kWifiGuid, "wifi-name",
                             shill::kTypeWifi, shill::kStateAssociation,
                             /*visible=*/true);
  base::RunLoop().RunUntilIdle();

  const std::string service_config_low_signal = base::StringPrintf(
      kWifiConfig, kWifiGuid, shill::kStateAssociation, kLowSignalStrengthRssi);
  std::string service_path = network_handler_test_helper()->ConfigureService(
      service_config_low_signal);
  ASSERT_THAT(service_path, Eq(kWifiServicePath));

  NetworkEventsObserver network_events_observer;
  base::test::TestFuture<MetricData> test_future;

  network_events_observer.SetOnEventObservedCallback(
      test_future.GetRepeatingCallback());
  network_events_observer.SetReportingEnabled(/*is_enabled=*/true);
  base::RunLoop().RunUntilIdle();

  network_events_observer.OnSignalStrengthChanged(
      kWifiGuid,
      ::chromeos::network_health::mojom::UInt32Value::New(kSignalStrength));
  base::RunLoop().RunUntilIdle();

  ASSERT_FALSE(test_future.IsReady());
}

TEST_F(NetworkEventsObserverSignalStrengthTest, Cellular) {
  SetFeatureEnabled(true);

  std::string service_config_good_signal = base::StringPrintf(
      kWifiConfig, kWifiGuid, shill::kStateReady, kGoodSignalStrengthRssi);
  std::string service_path = network_handler_test_helper()->ConfigureService(
      service_config_good_signal);
  ASSERT_THAT(service_path, Eq(kWifiServicePath));

  NetworkEventsObserver network_events_observer;
  base::test::TestFuture<MetricData> test_future;

  network_events_observer.SetOnEventObservedCallback(
      test_future.GetRepeatingCallback());
  network_events_observer.SetReportingEnabled(/*is_enabled=*/true);
  base::RunLoop().RunUntilIdle();

  network_events_observer.OnSignalStrengthChanged(
      kCellularGuid,
      ::chromeos::network_health::mojom::UInt32Value::New(kSignalStrength));
  base::RunLoop().RunUntilIdle();

  ASSERT_FALSE(test_future.IsReady());
}

TEST_F(NetworkEventsObserverSignalStrengthTest, InvalidGuid) {
  SetFeatureEnabled(true);

  NetworkEventsObserver network_events_observer;
  bool event_reported = false;
  auto cb =
      base::BindLambdaForTesting([&](MetricData) { event_reported = true; });

  network_events_observer.SetOnEventObservedCallback(std::move(cb));
  network_events_observer.SetReportingEnabled(/*is_enabled=*/true);
  network_events_observer.OnSignalStrengthChanged(
      "invalid_guid",
      ::chromeos::network_health::mojom::UInt32Value::New(kSignalStrength));
  base::RunLoop().RunUntilIdle();

  ASSERT_FALSE(event_reported);
}

TEST_F(NetworkEventsObserverSignalStrengthTest, FeatureDisabled) {
  SetFeatureEnabled(false);

  const std::string service_config_low_signal = base::StringPrintf(
      kWifiConfig, kWifiGuid, shill::kStateReady, kLowSignalStrengthRssi);
  std::string service_path = network_handler_test_helper()->ConfigureService(
      service_config_low_signal);
  ASSERT_THAT(service_path, Eq(kWifiServicePath));

  NetworkEventsObserver network_events_observer;
  bool event_reported = false;
  auto cb =
      base::BindLambdaForTesting([&](MetricData) { event_reported = true; });

  network_events_observer.SetOnEventObservedCallback(std::move(cb));
  network_events_observer.SetReportingEnabled(/*is_enabled=*/true);
  network_events_observer.OnSignalStrengthChanged(
      kWifiGuid,
      ::chromeos::network_health::mojom::UInt32Value::New(kSignalStrength));
  base::RunLoop().RunUntilIdle();

  ASSERT_FALSE(event_reported);
}

struct NetworkConnectionStateTestCase {
  std::string test_name;
  std::string service_path;
  std::string guid;
  std::string input_state;
  std::string other_state = shill::kStateIdle;
  MetricEventType expected_event_type;
  NetworkConnectionState expected_state;
  NetworkConnectionState other_expected_state =
      NetworkConnectionState::NOT_CONNECTED;
};

class NetworkEventsObserverConnectionStateTest
    : public ::testing::TestWithParam<NetworkConnectionStateTestCase> {
 protected:
  void SetUp() override { network_events_observer_test_helper_.SetUp(); }

  void TearDown() override { network_events_observer_test_helper_.TearDown(); }

  ash::ShillServiceClient::TestInterface* service_client() {
    return network_events_observer_test_helper_.network_handler_test_helper()
        ->service_test();
  }

  NetworkEventsObserverTestHelper network_events_observer_test_helper_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(NetworkEventsObserverConnectionStateTest, PhysicalFeatureDisabled) {
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/{kEnableVpnConnectionStateEventsReporting},
      /*disabled_features=*/{kEnableNetworkConnectionStateEventsReporting});

  NetworkEventsObserver network_events_observer;
  network_events_observer.SetReportingEnabled(true);
  base::test::TestFuture<MetricData> test_future;
  network_events_observer.SetOnEventObservedCallback(
      test_future.GetRepeatingCallback());

  service_client()->SetServiceProperty(kWifiServicePath, shill::kStateProperty,
                                       base::Value(shill::kStateOnline));
  base::RunLoop().RunUntilIdle();

  ASSERT_FALSE(test_future.IsReady());

  service_client()->SetServiceProperty(kVpnServicePath, shill::kStateProperty,
                                       base::Value(shill::kStateOnline));

  MetricData metric_data = test_future.Take();
  EXPECT_THAT(metric_data.event_data().type(),
              Eq(MetricEventType::VPN_CONNECTION_STATE_CHANGE));
  EXPECT_THAT(metric_data.telemetry_data()
                  .networks_telemetry()
                  .network_connection_change_event_data()
                  .connection_state(),
              Eq(NetworkConnectionState::ONLINE));
  EXPECT_FALSE(test_future.IsReady());
}

TEST_F(NetworkEventsObserverConnectionStateTest, VpnFeatureDisabled) {
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/
      {kEnableNetworkConnectionStateEventsReporting},
      /*disabled_features=*/
      {kEnableVpnConnectionStateEventsReporting});

  NetworkEventsObserver network_events_observer;
  network_events_observer.SetReportingEnabled(true);
  base::test::TestFuture<MetricData> test_future;
  network_events_observer.SetOnEventObservedCallback(
      test_future.GetRepeatingCallback());

  service_client()->SetServiceProperty(kVpnServicePath, shill::kStateProperty,
                                       base::Value(shill::kStateIdle));
  base::RunLoop().RunUntilIdle();

  ASSERT_FALSE(test_future.IsReady());

  service_client()->SetServiceProperty(kWifiServicePath, shill::kStateProperty,
                                       base::Value(shill::kStateIdle));

  MetricData metric_data = test_future.Take();
  EXPECT_THAT(metric_data.event_data().type(),
              Eq(MetricEventType::NETWORK_STATE_CHANGE));
  EXPECT_THAT(metric_data.telemetry_data()
                  .networks_telemetry()
                  .network_connection_change_event_data()
                  .connection_state(),
              Eq(NetworkConnectionState::NOT_CONNECTED));
  EXPECT_FALSE(test_future.IsReady());
}

TEST_F(NetworkEventsObserverConnectionStateTest, NewVpnConnection) {
  static constexpr char kNewVpnServicePath1[] = "new-vpn-path1";
  static constexpr char kNewVpnGuid1[] = "new-vpn-guid1";
  static constexpr char kNewVpnServicePath2[] = "new-vpn-path2";
  static constexpr char kNewVpnGuid2[] = "new-vpn-guid2";

  scoped_feature_list_.InitAndEnableFeature(
      kEnableVpnConnectionStateEventsReporting);

  NetworkEventsObserver network_events_observer;
  network_events_observer.SetReportingEnabled(true);
  base::test::TestFuture<MetricData> test_future;
  network_events_observer.SetOnEventObservedCallback(
      test_future.GetRepeatingCallback());

  service_client()->AddService(kNewVpnServicePath1, kNewVpnGuid1, "new-name1",
                               shill::kTypeVPN, shill::kStateIdle,
                               /*visible=*/true);
  base::RunLoop().RunUntilIdle();

  // New connection added in disconnected state, nothing is reported.
  ASSERT_FALSE(test_future.IsReady());

  // Change new connection state.
  service_client()->SetServiceProperty(kNewVpnServicePath1,
                                       shill::kStateProperty,
                                       base::Value(shill::kStateReady));

  MetricData metric_data1 = test_future.Take();
  const auto& connection_event_data1 =
      metric_data1.telemetry_data()
          .networks_telemetry()
          .network_connection_change_event_data();
  EXPECT_THAT(metric_data1.event_data().type(),
              Eq(MetricEventType::VPN_CONNECTION_STATE_CHANGE));
  EXPECT_THAT(connection_event_data1.guid(), StrEq(kNewVpnGuid1));
  EXPECT_THAT(connection_event_data1.connection_state(),
              Eq(NetworkConnectionState::CONNECTED));

  service_client()->AddService(kNewVpnServicePath2, kNewVpnGuid2, "new-name2",
                               shill::kTypeVPN, shill::kStateAssociation,
                               /*visible=*/true);

  // New connection added in connecting state should be reported.
  MetricData metric_data2 = test_future.Take();
  const auto& connection_event_data2 =
      metric_data2.telemetry_data()
          .networks_telemetry()
          .network_connection_change_event_data();
  EXPECT_THAT(metric_data2.event_data().type(),
              Eq(MetricEventType::VPN_CONNECTION_STATE_CHANGE));
  EXPECT_THAT(connection_event_data2.guid(), StrEq(kNewVpnGuid2));
  EXPECT_THAT(connection_event_data2.connection_state(),
              Eq(NetworkConnectionState::CONNECTING));
}

TEST_F(NetworkEventsObserverConnectionStateTest, TetherConnection) {
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/
      {kEnableNetworkConnectionStateEventsReporting,
       kEnableVpnConnectionStateEventsReporting},
      /*disabled_features=*/
      {});

  NetworkEventsObserver network_events_observer;
  network_events_observer.SetReportingEnabled(true);
  base::test::TestFuture<MetricData> test_future;
  network_events_observer.SetOnEventObservedCallback(
      test_future.GetRepeatingCallback());

  service_client()->SetServiceProperty(kTetherServicePath,
                                       shill::kStateProperty,
                                       base::Value(shill::kStateOnline));
  base::RunLoop().RunUntilIdle();

  ASSERT_FALSE(test_future.IsReady());
}

TEST_F(NetworkEventsObserverConnectionStateTest, WifiPortal) {
  static constexpr char kNewWifiServicePath[] = "new-wifi-path";
  static constexpr char kNewWifiGuid[] = "new-wifi-guid";

  scoped_feature_list_.InitAndEnableFeature(
      kEnableNetworkConnectionStateEventsReporting);

  NetworkEventsObserver network_events_observer;
  network_events_observer.SetReportingEnabled(true);
  base::test::TestFuture<MetricData> test_future;
  network_events_observer.SetOnEventObservedCallback(
      test_future.GetRepeatingCallback());

  service_client()->AddService(kNewWifiServicePath, kNewWifiGuid, "new-name",
                               shill::kTypeWifi, shill::kStateRedirectFound,
                               /*visible=*/true);

  MetricData metric_data = test_future.Take();
  const auto& connection_event_data =
      metric_data.telemetry_data()
          .networks_telemetry()
          .network_connection_change_event_data();
  EXPECT_THAT(metric_data.event_data().type(),
              Eq(MetricEventType::NETWORK_STATE_CHANGE));
  EXPECT_THAT(connection_event_data.guid(), Eq((kNewWifiGuid)));
  EXPECT_THAT(connection_event_data.connection_state(),
              Eq(NetworkConnectionState::PORTAL));
}

TEST_P(NetworkEventsObserverConnectionStateTest, Default) {
  const NetworkConnectionStateTestCase& test_case = GetParam();
  static constexpr char kNewWifiServicePath[] = "new-wifi-path";
  static constexpr char kNewWifiGuid[] = "new-wifi-guid";

  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/{kEnableNetworkConnectionStateEventsReporting,
                            kEnableVpnConnectionStateEventsReporting},
      /*disabled_features=*/{});

  service_client()->AddService(kNewWifiServicePath, kNewWifiGuid, "new-name",
                               shill::kTypeWifi, test_case.other_state,
                               /*visible=*/true);
  base::RunLoop().RunUntilIdle();

  NetworkEventsObserver network_events_observer;
  network_events_observer.SetReportingEnabled(true);
  base::test::TestFuture<MetricData> test_future;
  network_events_observer.SetOnEventObservedCallback(
      test_future.GetRepeatingCallback());

  service_client()->SetServiceProperty(test_case.service_path,
                                       shill::kStateProperty,
                                       base::Value(test_case.input_state));
  {
    MetricData metric_data = test_future.Take();
    const auto& connection_event_data =
        metric_data.telemetry_data()
            .networks_telemetry()
            .network_connection_change_event_data();
    EXPECT_THAT(metric_data.event_data().type(),
                Eq(test_case.expected_event_type));
    EXPECT_THAT(connection_event_data.guid(), Eq(test_case.guid));
    EXPECT_THAT(connection_event_data.connection_state(),
                Eq(test_case.expected_state));
  }

  // Same event for different service should be reported.
  service_client()->SetServiceProperty(kNewWifiServicePath,
                                       shill::kStateProperty,
                                       base::Value(test_case.input_state));
  {
    MetricData metric_data = test_future.Take();
    const auto& connection_event_data =
        metric_data.telemetry_data()
            .networks_telemetry()
            .network_connection_change_event_data();
    EXPECT_THAT(metric_data.event_data().type(),
                MetricEventType::NETWORK_STATE_CHANGE);
    EXPECT_THAT(connection_event_data.guid(), Eq(kNewWifiGuid));
    EXPECT_THAT(connection_event_data.connection_state(),
                Eq(test_case.expected_state));
  }

  // Duplicate events should not be reported
  service_client()->SetServiceProperty(test_case.service_path,
                                       shill::kStateProperty,
                                       base::Value(test_case.input_state));
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(test_future.IsReady());

  // Different event for same network should be reported.
  service_client()->SetServiceProperty(test_case.service_path,
                                       shill::kStateProperty,
                                       base::Value(test_case.other_state));
  {
    MetricData metric_data = test_future.Take();
    const auto& connection_event_data =
        metric_data.telemetry_data()
            .networks_telemetry()
            .network_connection_change_event_data();
    EXPECT_THAT(metric_data.event_data().type(),
                Eq(test_case.expected_event_type));
    EXPECT_THAT(connection_event_data.guid(), Eq(test_case.guid));
    EXPECT_THAT(connection_event_data.connection_state(),
                Eq(test_case.other_expected_state));
  }

  // Reporting disabled, no events should be reported.
  network_events_observer.SetReportingEnabled(/*is_enabled=*/false);
  service_client()->SetServiceProperty(test_case.service_path,
                                       shill::kStateProperty,
                                       base::Value(test_case.input_state));
  base::RunLoop().RunUntilIdle();

  ASSERT_FALSE(test_future.IsReady());

  // Same last network reported event should be reported if reporting state
  // changed from disabled to enabled.
  network_events_observer.SetReportingEnabled(/*is_enabled=*/true);
  service_client()->SetServiceProperty(test_case.service_path,
                                       shill::kStateProperty,
                                       base::Value(test_case.other_state));

  EXPECT_TRUE(test_future.Wait());
}

INSTANTIATE_TEST_SUITE_P(
    NetworkEventsObserverConnectionStateTest,
    NetworkEventsObserverConnectionStateTest,
    ::testing::ValuesIn<NetworkConnectionStateTestCase>(
        {{.test_name = "WifiOnline",
          .service_path = kWifiServicePath,
          .guid = kWifiGuid,
          .input_state = shill::kStateOnline,
          .expected_event_type = MetricEventType::NETWORK_STATE_CHANGE,
          .expected_state = NetworkConnectionState::ONLINE},
         {.test_name = "CellularConnected",
          .service_path = kCellularServicePath,
          .guid = kCellularGuid,
          .input_state = shill::kStateReady,
          .expected_event_type = MetricEventType::NETWORK_STATE_CHANGE,
          .expected_state = NetworkConnectionState::CONNECTED},
         {.test_name = "VpnConnecting",
          .service_path = kVpnServicePath,
          .guid = kVpnGuid,
          .input_state = shill::kStateAssociation,
          .expected_event_type = MetricEventType::VPN_CONNECTION_STATE_CHANGE,
          .expected_state = NetworkConnectionState::CONNECTING},
         {.test_name = "VpnNotConnected",
          .service_path = kVpnServicePath,
          .guid = kVpnGuid,
          .input_state = shill::kStateIdle,
          .other_state = shill::kStateConfiguration,
          .expected_event_type = MetricEventType::VPN_CONNECTION_STATE_CHANGE,
          .expected_state = NetworkConnectionState::NOT_CONNECTED,
          .other_expected_state = NetworkConnectionState::CONNECTING}}),
    [](const testing::TestParamInfo<
        NetworkEventsObserverConnectionStateTest::ParamType>& info) {
      return info.param.test_name;
    });
}  // namespace
}  // namespace reporting
