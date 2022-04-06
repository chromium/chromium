// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/network/network_events_observer.h"

#include <string>
#include <utility>

#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "chromeos/dbus/cros_healthd/cros_healthd_client.h"
#include "chromeos/dbus/cros_healthd/fake_cros_healthd_client.h"
#include "chromeos/network/network_handler_test_helper.h"
#include "chromeos/services/cros_healthd/public/cpp/service_connection.h"
#include "components/reporting/proto/synced/metric_data.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace reporting {
namespace {

constexpr int kSignalStrength = 10;
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
  ::chromeos::network_health::mojom::NetworkState input_state;
  NetworkConnectionState expected_state;
};

class NetworkEventsObserverTest
    : public ::testing::TestWithParam<NetworkConnectionStateTestCase> {
 public:
  NetworkEventsObserverTest() = default;

  NetworkEventsObserverTest(const NetworkEventsObserverTest&) = delete;
  NetworkEventsObserverTest& operator=(const NetworkEventsObserverTest&) =
      delete;

  ~NetworkEventsObserverTest() override = default;

  void SetUp() override {
    ::chromeos::CrosHealthdClient::InitializeFake();
    auto* const service_client = network_handler_test_helper_.service_test();
    auto* const device_client = network_handler_test_helper_.device_test();

    task_environment_.RunUntilIdle();
    service_client->ClearServices();
    device_client->ClearDevices();
    task_environment_.RunUntilIdle();

    service_client->AddService(kWifiServicePath, kWifiGuid, "wifi-name",
                               shill::kTypeWifi, shill::kStateReady, true);

    service_client->AddService(kWifiIdleServicePath, kWifiIdleGuid,
                               "wifi-idle-name", shill::kTypeWifi,
                               shill::kStateIdle, true);

    service_client->AddService(kCellularServicePath, kCellularGuid,
                               "cellular-network-name", shill::kTypeCellular,
                               shill::kStateReady, true);
    service_client->SetServiceProperty(
        kCellularServicePath, shill::kIccidProperty, base::Value("test_iccid"));
    task_environment_.RunUntilIdle();
  }

  void TearDown() override { ::chromeos::CrosHealthdClient::Shutdown(); }

 private:
  base::test::TaskEnvironment task_environment_;

  ::ash::NetworkHandlerTestHelper network_handler_test_helper_;
};

TEST_F(NetworkEventsObserverTest, WifiSignalStrength) {
  NetworkEventsObserver network_events_observer;
  MetricData result_metric_data;
  bool event_reported = false;
  auto cb = base::BindLambdaForTesting([&](MetricData metric_data) {
    event_reported = true;
    result_metric_data = std::move(metric_data);
  });

  network_events_observer.SetOnEventObservedCallback(std::move(cb));
  network_events_observer.OnSignalStrengthChanged(
      kWifiGuid,
      ::chromeos::network_health::mojom::UInt32Value::New(kSignalStrength));

  ASSERT_TRUE(event_reported);
  ASSERT_TRUE(result_metric_data.has_event_data());
  EXPECT_EQ(result_metric_data.event_data().type(),
            MetricEventType::NETWORK_SIGNAL_STRENGTH_CHANGE);
  ASSERT_TRUE(result_metric_data.has_telemetry_data());
  ASSERT_TRUE(result_metric_data.telemetry_data().has_networks_telemetry());
  ASSERT_EQ(result_metric_data.telemetry_data()
                .networks_telemetry()
                .network_telemetry_size(),
            1);
  EXPECT_EQ(result_metric_data.telemetry_data()
                .networks_telemetry()
                .network_telemetry(0)
                .guid(),
            kWifiGuid);
  EXPECT_EQ(result_metric_data.telemetry_data()
                .networks_telemetry()
                .network_telemetry(0)
                .signal_strength(),
            kSignalStrength);
}

TEST_F(NetworkEventsObserverTest, WifiSignalStrength_NotConnected) {
  NetworkEventsObserver network_events_observer;
  bool event_reported = false;
  auto cb =
      base::BindLambdaForTesting([&](MetricData) { event_reported = true; });

  network_events_observer.SetOnEventObservedCallback(std::move(cb));
  network_events_observer.OnSignalStrengthChanged(
      kWifiIdleGuid,
      ::chromeos::network_health::mojom::UInt32Value::New(kSignalStrength));

  ASSERT_FALSE(event_reported);
}

TEST_F(NetworkEventsObserverTest, CellularSignalStrength) {
  NetworkEventsObserver network_events_observer;
  bool event_reported = false;
  auto cb =
      base::BindLambdaForTesting([&](MetricData) { event_reported = true; });

  network_events_observer.SetOnEventObservedCallback(std::move(cb));
  network_events_observer.OnSignalStrengthChanged(
      kCellularGuid,
      ::chromeos::network_health::mojom::UInt32Value::New(kSignalStrength));

  ASSERT_FALSE(event_reported);
}

TEST_P(NetworkEventsObserverTest, ConnectionState) {
  const NetworkConnectionStateTestCase& test_case = GetParam();

  NetworkEventsObserver network_events_observer;
  MetricData result_metric_data;
  auto cb = base::BindLambdaForTesting([&](MetricData metric_data) {
    result_metric_data = std::move(metric_data);
  });

  network_events_observer.SetOnEventObservedCallback(std::move(cb));
  network_events_observer.OnConnectionStateChanged(kWifiGuid,
                                                   test_case.input_state);

  ASSERT_TRUE(result_metric_data.has_event_data());
  EXPECT_EQ(result_metric_data.event_data().type(),
            MetricEventType::NETWORK_CONNECTION_STATE_CHANGE);
  ASSERT_TRUE(result_metric_data.has_telemetry_data());
  ASSERT_TRUE(result_metric_data.telemetry_data().has_networks_telemetry());
  ASSERT_EQ(result_metric_data.telemetry_data()
                .networks_telemetry()
                .network_telemetry_size(),
            1);
  EXPECT_EQ(result_metric_data.telemetry_data()
                .networks_telemetry()
                .network_telemetry(0)
                .guid(),
            kWifiGuid);
  EXPECT_EQ(result_metric_data.telemetry_data()
                .networks_telemetry()
                .network_telemetry(0)
                .connection_state(),
            test_case.expected_state);
}

INSTANTIATE_TEST_SUITE_P(
    NetworkEventsObserverConnectionStateTest,
    NetworkEventsObserverTest,
    ::testing::ValuesIn<NetworkConnectionStateTestCase>(
        {{"Online", ::chromeos::network_health::mojom::NetworkState::kOnline,
          NetworkConnectionState::ONLINE},
         {"Connected",
          ::chromeos::network_health::mojom::NetworkState::kConnected,
          NetworkConnectionState::CONNECTED},
         {"Portal", ::chromeos::network_health::mojom::NetworkState::kPortal,
          NetworkConnectionState::PORTAL},
         {"Connecting",
          ::chromeos::network_health::mojom::NetworkState::kConnecting,
          NetworkConnectionState::CONNECTING},
         {"NotConnected",
          ::chromeos::network_health::mojom::NetworkState::kNotConnected,
          NetworkConnectionState::NOT_CONNECTED}}),
    [](const testing::TestParamInfo<NetworkEventsObserverTest::ParamType>&
           info) { return info.param.test_name; });
}  // namespace
}  // namespace reporting
