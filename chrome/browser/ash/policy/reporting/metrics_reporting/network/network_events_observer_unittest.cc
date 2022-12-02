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
#include "base/test/task_environment.h"
#include "chromeos/ash/components/dbus/debug_daemon/debug_daemon_client.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
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
// Service paths.
constexpr char kWifiServicePath[] = "/service/wlan";
constexpr char kWifiIdleServicePath[] = "/service/wifi-idle";
constexpr char kCellularServicePath[] = "/service/cellular";

struct NetworkConnectionStateTestCase {
  std::string test_name;
  NetworkState input_state;
  NetworkConnectionState expected_state;
};

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

class NetworkEventsObserverTest
    : public ::testing::TestWithParam<NetworkConnectionStateTestCase> {
 public:
  NetworkEventsObserverTest() = default;

  NetworkEventsObserverTest(const NetworkEventsObserverTest&) = delete;
  NetworkEventsObserverTest& operator=(const NetworkEventsObserverTest&) =
      delete;

  ~NetworkEventsObserverTest() override = default;

  void SetUp() override {
    ash::DebugDaemonClient::InitializeFake();

    ash::LoginState::Initialize();
    ash::LoginState::Get()->SetLoggedInStateAndPrimaryUser(
        ash::LoginState::LOGGED_IN_ACTIVE,
        ash::LoginState::LOGGED_IN_USER_REGULAR,
        network_handler_test_helper_.UserHash());

    network_handler_test_helper_.AddDefaultProfiles();
    network_handler_test_helper_.ResetDevicesAndServices();
    auto* const service_client = network_handler_test_helper_.service_test();

    service_client->AddService(kWifiServicePath, kWifiGuid, "wifi-name",
                               shill::kTypeWifi, shill::kStateReady,
                               /*visible=*/true);

    service_client->AddService(kWifiIdleServicePath, kWifiIdleGuid,
                               "wifi-idle-name", shill::kTypeWifi,
                               shill::kStateIdle, /*visible=*/true);

    service_client->AddService(kCellularServicePath, kCellularGuid,
                               "cellular-network-name", shill::kTypeCellular,
                               shill::kStateReady, /*visible=*/true);
    service_client->SetServiceProperty(
        kCellularServicePath, shill::kIccidProperty, base::Value("test_iccid"));
    task_environment_.RunUntilIdle();
  }

  void TearDown() override {
    ash::LoginState::Shutdown();
    ash::DebugDaemonClient::Shutdown();
  }

  base::test::TaskEnvironment task_environment_;

  ash::NetworkHandlerTestHelper network_handler_test_helper_;
};

TEST_F(NetworkEventsObserverTest, WifiSignalStrength_InitiallyLowSignal) {
  const std::string service_config_low_signal = base::StringPrintf(
      kWifiConfig, kWifiGuid, shill::kStateReady, kLowSignalStrengthRssi);
  std::string service_path =
      network_handler_test_helper_.ConfigureService(service_config_low_signal);
  ASSERT_THAT(service_path, Eq(kWifiServicePath));

  NetworkEventsObserver network_events_observer;
  MetricData result_metric_data;
  bool event_reported = false;
  std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
  auto cb = base::BindLambdaForTesting([&](MetricData metric_data) {
    event_reported = true;
    result_metric_data = std::move(metric_data);
    run_loop->Quit();
  });

  network_events_observer.SetOnEventObservedCallback(std::move(cb));
  network_events_observer.SetReportingEnabled(/*is_enabled=*/true);
  run_loop->Run();

  ASSERT_TRUE(event_reported);
  ASSERT_TRUE(result_metric_data.has_event_data());
  EXPECT_THAT(result_metric_data.event_data().type(),
              Eq(MetricEventType::NETWORK_SIGNAL_STRENGTH_LOW));

  std::string service_config_very_low_signal = base::StringPrintf(
      kWifiConfig, kWifiGuid, shill::kStateReady, kVeryLowSignalStrengthRssi);
  service_path = network_handler_test_helper_.ConfigureService(
      service_config_very_low_signal);
  ASSERT_THAT(service_path, Eq(kWifiServicePath));
  event_reported = false;

  network_events_observer.OnSignalStrengthChanged(
      kWifiGuid,
      ::chromeos::network_health::mojom::UInt32Value::New(kSignalStrength));
  base::RunLoop().RunUntilIdle();

  // Low signal strength event already reported.
  ASSERT_FALSE(event_reported);

  std::string service_config_good_signal = base::StringPrintf(
      kWifiConfig, kWifiGuid, shill::kStateReady, kGoodSignalStrengthRssi);
  service_path =
      network_handler_test_helper_.ConfigureService(service_config_good_signal);
  ASSERT_THAT(service_path, Eq(kWifiServicePath));

  run_loop = std::make_unique<base::RunLoop>();
  network_events_observer.OnSignalStrengthChanged(
      kWifiGuid,
      ::chromeos::network_health::mojom::UInt32Value::New(kSignalStrength));
  run_loop->Run();

  ASSERT_TRUE(event_reported);
  ASSERT_TRUE(result_metric_data.has_event_data());
  EXPECT_THAT(result_metric_data.event_data().type(),
              Eq(MetricEventType::NETWORK_SIGNAL_STRENGTH_RECOVERED));
}

TEST_F(NetworkEventsObserverTest, WifiSignalStrength_NotConnected) {
  network_handler_test_helper_.ResetDevicesAndServices();
  auto* const service_client = network_handler_test_helper_.service_test();
  service_client->AddService(kWifiIdleServicePath, kWifiIdleGuid,
                             "wifi-idle-name", shill::kTypeWifi,
                             shill::kStateIdle, /*visible=*/true);
  base::RunLoop().RunUntilIdle();

  std::string idle_service_config = base::StringPrintf(
      kWifiConfig, kWifiIdleGuid, shill::kStateIdle, kLowSignalStrengthRssi);
  std::string idle_service_path =
      network_handler_test_helper_.ConfigureService(idle_service_config);
  ASSERT_THAT(idle_service_path, Eq(kWifiIdleServicePath));

  NetworkEventsObserver network_events_observer;
  bool event_reported = false;
  auto cb =
      base::BindLambdaForTesting([&](MetricData) { event_reported = true; });

  network_events_observer.SetOnEventObservedCallback(std::move(cb));
  network_events_observer.SetReportingEnabled(/*is_enabled=*/true);
  base::RunLoop().RunUntilIdle();

  network_events_observer.OnSignalStrengthChanged(
      kWifiIdleGuid,
      ::chromeos::network_health::mojom::UInt32Value::New(kSignalStrength));
  base::RunLoop().RunUntilIdle();

  ASSERT_FALSE(event_reported);
}

TEST_F(NetworkEventsObserverTest, WifiSignalStrength_Connecting) {
  network_handler_test_helper_.ResetDevicesAndServices();
  auto* const service_client = network_handler_test_helper_.service_test();
  service_client->AddService(kWifiServicePath, kWifiGuid, "wifi-name",
                             shill::kTypeWifi, shill::kStateAssociation,
                             /*visible=*/true);
  base::RunLoop().RunUntilIdle();

  const std::string service_config_low_signal = base::StringPrintf(
      kWifiConfig, kWifiGuid, shill::kStateAssociation, kLowSignalStrengthRssi);
  std::string service_path =
      network_handler_test_helper_.ConfigureService(service_config_low_signal);
  ASSERT_THAT(service_path, Eq(kWifiServicePath));

  NetworkEventsObserver network_events_observer;
  bool event_reported = false;
  auto cb =
      base::BindLambdaForTesting([&](MetricData) { event_reported = true; });

  network_events_observer.SetOnEventObservedCallback(std::move(cb));
  network_events_observer.SetReportingEnabled(/*is_enabled=*/true);
  base::RunLoop().RunUntilIdle();

  network_events_observer.OnSignalStrengthChanged(
      kWifiGuid,
      ::chromeos::network_health::mojom::UInt32Value::New(kSignalStrength));
  base::RunLoop().RunUntilIdle();

  ASSERT_FALSE(event_reported);
}

TEST_F(NetworkEventsObserverTest, CellularSignalStrength) {
  std::string service_config_good_signal = base::StringPrintf(
      kWifiConfig, kWifiGuid, shill::kStateReady, kGoodSignalStrengthRssi);
  std::string service_path =
      network_handler_test_helper_.ConfigureService(service_config_good_signal);
  ASSERT_THAT(service_path, Eq(kWifiServicePath));

  NetworkEventsObserver network_events_observer;
  bool event_reported = false;
  auto cb =
      base::BindLambdaForTesting([&](MetricData) { event_reported = true; });

  network_events_observer.SetOnEventObservedCallback(std::move(cb));
  network_events_observer.SetReportingEnabled(/*is_enabled=*/true);
  base::RunLoop().RunUntilIdle();

  network_events_observer.OnSignalStrengthChanged(
      kCellularGuid,
      ::chromeos::network_health::mojom::UInt32Value::New(kSignalStrength));
  base::RunLoop().RunUntilIdle();

  ASSERT_FALSE(event_reported);
}

TEST_F(NetworkEventsObserverTest, SignalStrengthInvalidGuid) {
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

TEST_F(NetworkEventsObserverTest, ConnectionState_MultipleEvents) {
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

TEST_P(NetworkEventsObserverTest, ConnectionState) {
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
    NetworkEventsObserverTest,
    ::testing::ValuesIn<NetworkConnectionStateTestCase>(
        {{"Online", NetworkState::kOnline, NetworkConnectionState::ONLINE},
         {"Connected", NetworkState::kConnected,
          NetworkConnectionState::CONNECTED},
         {"Portal", NetworkState::kPortal, NetworkConnectionState::PORTAL},
         {"Connecting", NetworkState::kConnecting,
          NetworkConnectionState::CONNECTING},
         {"NotConnected", NetworkState::kNotConnected,
          NetworkConnectionState::NOT_CONNECTED}}),
    [](const testing::TestParamInfo<NetworkEventsObserverTest::ParamType>&
           info) { return info.param.test_name; });
}  // namespace
}  // namespace reporting
