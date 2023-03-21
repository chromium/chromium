// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/network/network_events_observer.h"

#include <memory>
#include <string>
#include <utility>

#include "base/run_loop.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/repeating_test_future.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/dbus/debug_daemon/debug_daemon_client.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "chromeos/ash/components/network/tether_constants.h"
#include "chromeos/services/network_health/public/mojom/network_health_types.mojom.h"
#include "components/reporting/proto/synced/metric_data.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

using chromeos::network_health::mojom::NetworkState;
using testing::Eq;

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

    ash::LoginState::Initialize();
    ash::LoginState::Get()->SetLoggedInStateAndPrimaryUser(
        ash::LoginState::LOGGED_IN_ACTIVE,
        ash::LoginState::LOGGED_IN_USER_REGULAR,
        network_handler_test_helper_.UserHash());

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
                               shill::kStateReady, /*visible=*/true);
    service_client->SetServiceProperty(
        kCellularServicePath, shill::kIccidProperty, base::Value("test_iccid"));
    task_environment_.RunUntilIdle();
  }

  void TearDown() {
    ash::LoginState::Shutdown();
    ash::DebugDaemonClient::Shutdown();
  }

  ash::NetworkHandlerTestHelper* network_handler_test_helper() {
    return &network_handler_test_helper_;
  }

 private:
  base::test::TaskEnvironment task_environment_;

  ash::NetworkHandlerTestHelper network_handler_test_helper_;
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
  base::test::RepeatingTestFuture<MetricData> test_future;

  network_events_observer.SetOnEventObservedCallback(test_future.GetCallback());
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
  ASSERT_TRUE(test_future.IsEmpty());

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
  base::test::RepeatingTestFuture<MetricData> test_future;

  network_events_observer.SetOnEventObservedCallback(test_future.GetCallback());
  network_events_observer.SetReportingEnabled(/*is_enabled=*/true);
  base::RunLoop().RunUntilIdle();

  network_events_observer.OnSignalStrengthChanged(
      kWifiIdleGuid,
      ::chromeos::network_health::mojom::UInt32Value::New(kSignalStrength));
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(test_future.IsEmpty());
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
  base::test::RepeatingTestFuture<MetricData> test_future;

  network_events_observer.SetOnEventObservedCallback(test_future.GetCallback());
  network_events_observer.SetReportingEnabled(/*is_enabled=*/true);
  base::RunLoop().RunUntilIdle();

  network_events_observer.OnSignalStrengthChanged(
      kWifiGuid,
      ::chromeos::network_health::mojom::UInt32Value::New(kSignalStrength));
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(test_future.IsEmpty());
}

TEST_F(NetworkEventsObserverSignalStrengthTest, Cellular) {
  SetFeatureEnabled(true);

  std::string service_config_good_signal = base::StringPrintf(
      kWifiConfig, kWifiGuid, shill::kStateReady, kGoodSignalStrengthRssi);
  std::string service_path = network_handler_test_helper()->ConfigureService(
      service_config_good_signal);
  ASSERT_THAT(service_path, Eq(kWifiServicePath));

  NetworkEventsObserver network_events_observer;
  base::test::RepeatingTestFuture<MetricData> test_future;

  network_events_observer.SetOnEventObservedCallback(test_future.GetCallback());
  network_events_observer.SetReportingEnabled(/*is_enabled=*/true);
  base::RunLoop().RunUntilIdle();

  network_events_observer.OnSignalStrengthChanged(
      kCellularGuid,
      ::chromeos::network_health::mojom::UInt32Value::New(kSignalStrength));
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(test_future.IsEmpty());
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
  NetworkState input_state;
  NetworkConnectionState expected_state;
};

class NetworkEventsObserverConnectionStateTest
    : public ::testing::TestWithParam<NetworkConnectionStateTestCase> {
 protected:
  void SetUp() override { network_events_observer_test_helper_.SetUp(); }

  void TearDown() override { network_events_observer_test_helper_.TearDown(); }

  void VerifyConnectionState(const MetricData& result_metric_data,
                             base::StringPiece guid,
                             NetworkConnectionState expected_connection_state) {
    ASSERT_TRUE(result_metric_data.has_event_data());
    EXPECT_THAT(result_metric_data.event_data().type(),
                Eq(MetricEventType::NETWORK_STATE_CHANGE));
    ASSERT_TRUE(result_metric_data.telemetry_data()
                    .networks_telemetry()
                    .has_network_connection_change_event_data());
    const auto& connection_change_event_data =
        result_metric_data.telemetry_data()
            .networks_telemetry()
            .network_connection_change_event_data();
    EXPECT_THAT(connection_change_event_data.guid(), Eq(guid));
    EXPECT_THAT(connection_change_event_data.connection_state(),
                Eq(expected_connection_state));
  }

  void SetFeatureEnabled(bool enabled) {
    scoped_feature_list_.InitWithFeatureState(
        kEnableNetworkConnectionStateEventsReporting, enabled);
  }

 private:
  NetworkEventsObserverTestHelper network_events_observer_test_helper_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(NetworkEventsObserverConnectionStateTest, FeatureDisabled) {
  SetFeatureEnabled(false);

  bool event_reported = false;

  NetworkEventsObserver network_events_observer;
  MetricData result_metric_data;
  auto cb =
      base::BindLambdaForTesting([&](MetricData) { event_reported = true; });

  network_events_observer.SetOnEventObservedCallback(std::move(cb));
  network_events_observer.OnConnectionStateChanged(kWifiGuid,
                                                   NetworkState::kNotConnected);

  EXPECT_FALSE(event_reported);
}

TEST_F(NetworkEventsObserverConnectionStateTest, VirtualConnection) {
  SetFeatureEnabled(true);

  bool event_reported = false;

  NetworkEventsObserver network_events_observer;
  MetricData result_metric_data;
  auto cb =
      base::BindLambdaForTesting([&](MetricData) { event_reported = true; });

  network_events_observer.SetOnEventObservedCallback(std::move(cb));
  network_events_observer.OnConnectionStateChanged(kVpnGuid,
                                                   NetworkState::kNotConnected);
  network_events_observer.OnConnectionStateChanged(kTetherGuid,
                                                   NetworkState::kConnected);

  EXPECT_FALSE(event_reported);
}

TEST_F(NetworkEventsObserverConnectionStateTest, MultipleEvents) {
  SetFeatureEnabled(true);

  bool event_reported = false;

  NetworkEventsObserver network_events_observer;
  MetricData result_metric_data;
  auto cb = base::BindLambdaForTesting([&](MetricData metric_data) {
    event_reported = true;
    result_metric_data = std::move(metric_data);
  });

  network_events_observer.SetOnEventObservedCallback(std::move(cb));
  network_events_observer.OnConnectionStateChanged(kWifiIdleGuid,
                                                   NetworkState::kNotConnected);

  ASSERT_TRUE(event_reported);
  VerifyConnectionState(result_metric_data, kWifiIdleGuid, NOT_CONNECTED);

  // Duplicate events should not be reported.
  event_reported = false;
  network_events_observer.OnConnectionStateChanged(kWifiIdleGuid,
                                                   NetworkState::kNotConnected);

  ASSERT_FALSE(event_reported);

  // Same event with different guid should be reported.
  event_reported = false;
  network_events_observer.OnConnectionStateChanged(kWifiGuid,
                                                   NetworkState::kNotConnected);

  ASSERT_TRUE(event_reported);
  VerifyConnectionState(result_metric_data, kWifiGuid, NOT_CONNECTED);

  // Duplicate events should not be reported even if another connection event
  // was observed in between.
  event_reported = false;
  network_events_observer.OnConnectionStateChanged(kWifiIdleGuid,
                                                   NetworkState::kNotConnected);

  ASSERT_FALSE(event_reported);

  // Different event with same guid should be reported.
  event_reported = false;
  network_events_observer.OnConnectionStateChanged(kWifiGuid,
                                                   NetworkState::kConnecting);

  ASSERT_TRUE(event_reported);
  VerifyConnectionState(result_metric_data, kWifiGuid, CONNECTING);

  // Same event with same guid should be reported if reporting state changed
  // from disabled to enabled.
  network_events_observer.SetReportingEnabled(/*is_enabled=*/false);
  network_events_observer.SetReportingEnabled(/*is_enabled=*/true);
  event_reported = false;
  network_events_observer.OnConnectionStateChanged(kWifiGuid,
                                                   NetworkState::kConnecting);

  ASSERT_TRUE(event_reported);
  VerifyConnectionState(result_metric_data, kWifiGuid, CONNECTING);
}

TEST_F(NetworkEventsObserverConnectionStateTest, InvalidGuid) {
  SetFeatureEnabled(true);

  NetworkEventsObserver network_events_observer;
  bool event_reported = false;
  auto cb =
      base::BindLambdaForTesting([&](MetricData) { event_reported = true; });

  network_events_observer.SetOnEventObservedCallback(std::move(cb));
  network_events_observer.SetReportingEnabled(/*is_enabled=*/true);
  network_events_observer.OnConnectionStateChanged("invalid_guid",
                                                   NetworkState::kOnline);
  base::RunLoop().RunUntilIdle();

  ASSERT_FALSE(event_reported);
}

TEST_P(NetworkEventsObserverConnectionStateTest, Default) {
  SetFeatureEnabled(true);

  const NetworkConnectionStateTestCase& test_case = GetParam();
  bool event_reported = false;

  NetworkEventsObserver network_events_observer;
  MetricData result_metric_data;
  auto cb = base::BindLambdaForTesting([&](MetricData metric_data) {
    event_reported = true;
    result_metric_data = std::move(metric_data);
  });

  network_events_observer.SetOnEventObservedCallback(std::move(cb));
  network_events_observer.OnConnectionStateChanged(kWifiGuid,
                                                   test_case.input_state);

  ASSERT_TRUE(event_reported);
  ASSERT_TRUE(result_metric_data.has_event_data());
  EXPECT_THAT(result_metric_data.event_data().type(),
              Eq(MetricEventType::NETWORK_STATE_CHANGE));
  ASSERT_TRUE(result_metric_data.has_telemetry_data());
  ASSERT_TRUE(result_metric_data.telemetry_data().has_networks_telemetry());
  ASSERT_TRUE(result_metric_data.telemetry_data()
                  .networks_telemetry()
                  .has_network_connection_change_event_data());
  EXPECT_THAT(result_metric_data.telemetry_data()
                  .networks_telemetry()
                  .network_connection_change_event_data()
                  .guid(),
              Eq(kWifiGuid));
  EXPECT_THAT(result_metric_data.telemetry_data()
                  .networks_telemetry()
                  .network_connection_change_event_data()
                  .connection_state(),
              Eq(test_case.expected_state));

  // Duplicate events should not be reported
  event_reported = false;
  network_events_observer.OnConnectionStateChanged(kWifiGuid,
                                                   test_case.input_state);

  ASSERT_FALSE(event_reported);
}

INSTANTIATE_TEST_SUITE_P(
    NetworkEventsObserverConnectionStateTest,
    NetworkEventsObserverConnectionStateTest,
    ::testing::ValuesIn<NetworkConnectionStateTestCase>(
        {{"Online", NetworkState::kOnline, NetworkConnectionState::ONLINE},
         {"Connected", NetworkState::kConnected,
          NetworkConnectionState::CONNECTED},
         {"Portal", NetworkState::kPortal, NetworkConnectionState::PORTAL},
         {"Connecting", NetworkState::kConnecting,
          NetworkConnectionState::CONNECTING},
         {"NotConnected", NetworkState::kNotConnected,
          NetworkConnectionState::NOT_CONNECTED}}),
    [](const testing::TestParamInfo<
        NetworkEventsObserverConnectionStateTest::ParamType>& info) {
      return info.param.test_name;
    });
}  // namespace
}  // namespace reporting
