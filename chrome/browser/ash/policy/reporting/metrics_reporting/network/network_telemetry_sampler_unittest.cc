// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/network/network_telemetry_sampler.h"

#include <string>
#include <utility>
#include <vector>

#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/metric_reporting_manager.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/tether_constants.h"
#include "chromeos/ash/services/cros_healthd/public/cpp/fake_cros_healthd.h"
#include "chromeos/dbus/shill/shill_ipconfig_client.h"
#include "chromeos/dbus/shill/shill_service_client.h"
#include "chromeos/login/login_state/login_state.h"
#include "components/reporting/metrics/fake_sampler.h"
#include "components/reporting/proto/synced/metric_data.pb.h"
#include "components/reporting/util/test_support_callbacks.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace reporting {
namespace {

// Wifi constants.
constexpr char kInterfaceName[] = "wlan_main";
constexpr char kAccessPointAddress[] = "00:00:5e:00:53:af";
constexpr bool kEncryptionOn = true;
constexpr bool kPowerManagementOn = true;
constexpr int64_t kTxBitRateMbps = 8;
constexpr int64_t kRxBitRateMbps = 4;
constexpr int64_t kTxPowerDbm = 2;
constexpr int64_t kLinkQuality = 1;
constexpr int64_t kSignalStrength = 70;

// Https latency constants.
constexpr RoutineVerdict kVerdict = RoutineVerdict::PROBLEM;
constexpr HttpsLatencyProblem kLatencyProblem =
    HttpsLatencyProblem::VERY_HIGH_LATENCY;
constexpr int64_t kLatencyMs = 3000;

struct FakeNetworkData {
  std::string guid;
  std::string connection_state;
  std::string type;
  int signal_strength;
  std::string device_name;
  std::string ip_address;
  std::string gateway;
  bool is_portal;
  bool is_visible;
  bool is_configured;
};

void SetWifiInterfaceData() {
  auto telemetry_info = ::chromeos::cros_healthd::mojom::TelemetryInfo::New();
  std::vector<::chromeos::cros_healthd::mojom::NetworkInterfaceInfoPtr>
      network_interfaces;

  auto wireless_link_info =
      ::chromeos::cros_healthd::mojom::WirelessLinkInfo::New(
          kAccessPointAddress, kTxBitRateMbps, kRxBitRateMbps, kTxPowerDbm,
          kEncryptionOn, kLinkQuality, -50);
  auto wireless_interface_info =
      ::chromeos::cros_healthd::mojom::WirelessInterfaceInfo::New(
          kInterfaceName, kPowerManagementOn, std::move(wireless_link_info));
  network_interfaces.push_back(
      ::chromeos::cros_healthd::mojom::NetworkInterfaceInfo::
          NewWirelessInterfaceInfo(std::move(wireless_interface_info)));
  auto network_interface_result =
      ::chromeos::cros_healthd::mojom::NetworkInterfaceResult::
          NewNetworkInterfaceInfo(std::move(network_interfaces));

  telemetry_info->network_interface_result =
      std::move(network_interface_result);
  ::ash::cros_healthd::FakeCrosHealthd::Get()
      ->SetProbeTelemetryInfoResponseForTesting(telemetry_info);
}

std::string DevicePath(const std::string& interface_name) {
  return base::StrCat({"device/", interface_name});
}

class NetworkTelemetrySamplerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ::chromeos::LoginState::Initialize();
    ::chromeos::LoginState::Get()->SetLoggedInStateAndPrimaryUser(
        ::chromeos::LoginState::LOGGED_IN_ACTIVE,
        ::chromeos::LoginState::LOGGED_IN_USER_REGULAR,
        network_handler_test_helper_.UserHash());

    network_handler_test_helper_.AddDefaultProfiles();
    network_handler_test_helper_.ResetDevicesAndServices();

    ::ash::cros_healthd::FakeCrosHealthd::Initialize();
    SetWifiInterfaceData();

    MetricData metric_data;
    auto* latency_data = metric_data.mutable_telemetry_data()
                             ->mutable_networks_telemetry()
                             ->mutable_https_latency_data();
    latency_data->set_verdict(kVerdict);
    latency_data->set_problem(kLatencyProblem);
    latency_data->set_latency_ms(kLatencyMs);
    https_latency_sampler_ = std::make_unique<test::FakeSampler>();
    https_latency_sampler_->SetMetricData(metric_data);
  }

  void TearDown() override {
    ::chromeos::LoginState::Shutdown();
    ash::cros_healthd::FakeCrosHealthd::Shutdown();
  }

  void SetNetworkData(const std::vector<FakeNetworkData>& networks_data) {
    auto* const service_client = network_handler_test_helper_.service_test();
    auto* const device_client = network_handler_test_helper_.device_test();
    auto* const ip_config_client =
        network_handler_test_helper_.ip_config_test();
    network_handler_test_helper_.manager_test()->AddTechnology(
        ::chromeos::kTypeTether, true);

    for (const auto& network_data : networks_data) {
      const std::string device_path = DevicePath(network_data.device_name);
      device_client->AddDevice(device_path, network_data.type,
                               network_data.device_name);
      device_client->SetDeviceProperty(device_path, shill::kInterfaceProperty,
                                       base::Value(network_data.device_name),
                                       /*notify_changed=*/true);
      base::RunLoop().RunUntilIdle();
      const std::string service_path =
          base::StrCat({"service_path", network_data.guid});
      const std::string network_name =
          base::StrCat({"network_name", network_data.guid});
      service_client->AddService(
          service_path, network_data.guid, network_name, network_data.type,
          network_data.connection_state, network_data.is_visible);
      service_client->SetServiceProperty(
          service_path, shill::kSignalStrengthProperty,
          base::Value(network_data.signal_strength));
      ash::NetworkHandler::Get()
          ->network_state_handler()
          ->SetNetworkChromePortalDetected(service_path,
                                           network_data.is_portal);
      service_client->SetServiceProperty(service_path, shill::kDeviceProperty,
                                         base::Value(device_path));
      base::DictionaryValue ip_config_properties;
      ip_config_properties.SetKey(shill::kAddressProperty,
                                  base::Value(network_data.ip_address));
      ip_config_properties.SetKey(shill::kGatewayProperty,
                                  base::Value(network_data.gateway));
      const std::string kIPConfigPath =
          base::StrCat({"test_ip_config", network_data.guid});
      ip_config_client->AddIPConfig(kIPConfigPath, ip_config_properties);
      service_client->SetServiceProperty(service_path, shill::kIPConfigProperty,
                                         base::Value(kIPConfigPath));
      if (network_data.type == shill::kTypeCellular) {
        service_client->SetServiceProperty(service_path, shill::kIccidProperty,
                                           base::Value("test_iccid"));
      }
      if (network_data.is_configured) {
        service_client->SetServiceProperty(
            service_path, shill::kProfileProperty,
            base::Value(network_handler_test_helper_.ProfilePathUser()));
      }
    }
    base::RunLoop().RunUntilIdle();
  }

  std::unique_ptr<test::FakeSampler> https_latency_sampler_;

  base::test::SingleThreadTaskEnvironment task_environment_;

  ::ash::NetworkHandlerTestHelper network_handler_test_helper_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(NetworkTelemetrySamplerTest, CellularConnected) {
  const std::vector<FakeNetworkData> networks_data = {
      {"guid1", shill::kStateReady, shill::kTypeCellular,
       0 /* signal_strength */, "cellular0", "192.168.86.25" /* ip_address */,
       "192.168.86.1" /* gateway */, false /* is_portal */,
       true /* is_visible */, true /* is_configured */}};

  SetNetworkData(networks_data);
  NetworkTelemetrySampler network_telemetry_sampler(
      https_latency_sampler_.get());
  test::TestEvent<absl::optional<MetricData>> metric_collect_event;
  network_telemetry_sampler.MaybeCollect(metric_collect_event.cb());
  const absl::optional<MetricData> optional_result =
      metric_collect_event.result();

  ASSERT_TRUE(optional_result.has_value());
  ASSERT_TRUE(optional_result->has_telemetry_data());
  const TelemetryData& result = optional_result->telemetry_data();
  ASSERT_TRUE(result.has_networks_telemetry());

  // No online networks, no latency data should be collected.
  EXPECT_FALSE(result.networks_telemetry().has_https_latency_data());

  ASSERT_THAT(result.networks_telemetry().network_telemetry(),
              ::testing::SizeIs(networks_data.size()));
  EXPECT_EQ(result.networks_telemetry().network_telemetry(0).guid(),
            networks_data[0].guid);
  EXPECT_EQ(result.networks_telemetry().network_telemetry(0).connection_state(),
            NetworkConnectionState::CONNECTED);
  EXPECT_FALSE(
      result.networks_telemetry().network_telemetry(0).has_signal_strength());
  EXPECT_EQ(result.networks_telemetry().network_telemetry(0).device_path(),
            DevicePath(networks_data[0].device_name));
  EXPECT_EQ(result.networks_telemetry().network_telemetry(0).ip_address(),
            networks_data[0].ip_address);
  EXPECT_EQ(result.networks_telemetry().network_telemetry(0).gateway(),
            networks_data[0].gateway);
  EXPECT_EQ(result.networks_telemetry().network_telemetry(0).type(),
            NetworkType::CELLULAR);
  // Make sure wireless interface info wasn't added.
  EXPECT_FALSE(
      result.networks_telemetry().network_telemetry(0).has_tx_bit_rate_mbps());
  EXPECT_FALSE(
      result.networks_telemetry().network_telemetry(0).has_rx_bit_rate_mbps());
  EXPECT_FALSE(
      result.networks_telemetry().network_telemetry(0).has_tx_power_dbm());
  EXPECT_FALSE(
      result.networks_telemetry().network_telemetry(0).has_encryption_on());
  EXPECT_FALSE(
      result.networks_telemetry().network_telemetry(0).has_link_quality());
  EXPECT_FALSE(result.networks_telemetry()
                   .network_telemetry(0)
                   .has_power_management_enabled());
}

TEST_F(NetworkTelemetrySamplerTest, NoNetworkData) {
  SetNetworkData({});

  NetworkTelemetrySampler network_telemetry_sampler(
      https_latency_sampler_.get());
  test::TestEvent<absl::optional<MetricData>> metric_collect_event;
  network_telemetry_sampler.MaybeCollect(metric_collect_event.cb());
  const absl::optional<MetricData> result = metric_collect_event.result();

  ASSERT_FALSE(result.has_value());
}

TEST_F(NetworkTelemetrySamplerTest, CellularNotConnected) {
  // Signal strength should be ignored for non wifi networks even if it is set.
  const std::vector<FakeNetworkData> networks_data = {
      {"guid1", shill::kStateIdle, shill::kTypeCellular, kSignalStrength,
       "cellular0", "" /* ip_address */, "" /* gateway */, true /* is_portal */,
       true /* is_visible */, true /* is_configured */}};

  SetNetworkData(networks_data);
  NetworkTelemetrySampler network_telemetry_sampler(
      https_latency_sampler_.get());
  test::TestEvent<absl::optional<MetricData>> metric_collect_event;
  network_telemetry_sampler.MaybeCollect(metric_collect_event.cb());
  const absl::optional<MetricData> result = metric_collect_event.result();

  ASSERT_FALSE(result.has_value());
}

TEST_F(NetworkTelemetrySamplerTest, WifiNotConnected_NoSignalStrength) {
  const std::vector<FakeNetworkData> networks_data = {
      {"guid1", shill::kStateIdle, shill::kTypeWifi, 0 /* signal_strength */,
       kInterfaceName, "" /* ip_address */, "" /* gateway */,
       true /* is_portal */, false /* is_visible */, true /* is_configured */}};

  SetNetworkData(networks_data);
  NetworkTelemetrySampler network_telemetry_sampler(
      https_latency_sampler_.get());
  test::TestEvent<absl::optional<MetricData>> metric_collect_event;
  network_telemetry_sampler.MaybeCollect(metric_collect_event.cb());
  const absl::optional<MetricData> result = metric_collect_event.result();

  ASSERT_FALSE(result.has_value());
}

TEST_F(NetworkTelemetrySamplerTest, EthernetPortal) {
  const std::vector<FakeNetworkData> networks_data = {
      {"guid1", shill::kStateRedirectFound, shill::kTypeEthernet,
       0 /* signal_strength */, "eth0", "192.168.86.25" /* ip_address */,
       "192.168.86.1" /* gateway */, true /* is_portal */,
       true /* is_visible */, true /* is_configured */}};

  SetNetworkData(networks_data);
  NetworkTelemetrySampler network_telemetry_sampler(
      https_latency_sampler_.get());
  test::TestEvent<absl::optional<MetricData>> metric_collect_event;
  network_telemetry_sampler.MaybeCollect(metric_collect_event.cb());
  const absl::optional<MetricData> optional_result =
      metric_collect_event.result();

  ASSERT_TRUE(optional_result.has_value());
  ASSERT_TRUE(optional_result->has_telemetry_data());
  const TelemetryData& result = optional_result->telemetry_data();
  ASSERT_TRUE(result.has_networks_telemetry());

  // No online networks, no latency data should be collected.
  EXPECT_FALSE(result.networks_telemetry().has_https_latency_data());

  ASSERT_THAT(result.networks_telemetry().network_telemetry(),
              ::testing::SizeIs(networks_data.size()));
  EXPECT_EQ(result.networks_telemetry().network_telemetry(0).guid(),
            networks_data[0].guid);
  EXPECT_EQ(result.networks_telemetry().network_telemetry(0).connection_state(),
            NetworkConnectionState::PORTAL);
  EXPECT_FALSE(
      result.networks_telemetry().network_telemetry(0).has_signal_strength());
  EXPECT_EQ(result.networks_telemetry().network_telemetry(0).device_path(),
            DevicePath(networks_data[0].device_name));
  EXPECT_EQ(result.networks_telemetry().network_telemetry(0).ip_address(),
            networks_data[0].ip_address);
  EXPECT_EQ(result.networks_telemetry().network_telemetry(0).gateway(),
            networks_data[0].gateway);
  EXPECT_EQ(result.networks_telemetry().network_telemetry(0).type(),
            NetworkType::ETHERNET);

  // Make sure wireless interface info wasn't added.
  EXPECT_FALSE(
      result.networks_telemetry().network_telemetry(0).has_tx_bit_rate_mbps());
  EXPECT_FALSE(
      result.networks_telemetry().network_telemetry(0).has_rx_bit_rate_mbps());
  EXPECT_FALSE(
      result.networks_telemetry().network_telemetry(0).has_tx_power_dbm());
  EXPECT_FALSE(
      result.networks_telemetry().network_telemetry(0).has_encryption_on());
  EXPECT_FALSE(
      result.networks_telemetry().network_telemetry(0).has_link_quality());
  EXPECT_FALSE(result.networks_telemetry()
                   .network_telemetry(0)
                   .has_power_management_enabled());
}

TEST_F(NetworkTelemetrySamplerTest, EmptyLatencyData) {
  const std::vector<FakeNetworkData> networks_data = {
      {"guid1", shill::kStateOnline, shill::kTypeEthernet,
       0 /* signal_strength */, "eth0", "192.168.86.25" /* ip_address */,
       "192.168.86.1" /* gateway */, true /* is_portal */,
       true /* is_visible */, true /* is_configured */}};

  SetNetworkData(networks_data);
  https_latency_sampler_->SetMetricData(absl::nullopt);

  NetworkTelemetrySampler network_telemetry_sampler(
      https_latency_sampler_.get());
  test::TestEvent<absl::optional<MetricData>> metric_collect_event;
  network_telemetry_sampler.MaybeCollect(metric_collect_event.cb());
  const absl::optional<MetricData> optional_result =
      metric_collect_event.result();

  ASSERT_TRUE(optional_result.has_value());
  ASSERT_TRUE(optional_result->has_telemetry_data());
  const TelemetryData& result = optional_result->telemetry_data();
  ASSERT_TRUE(result.has_networks_telemetry());

  // Latency data is absent but network data is still collected.
  EXPECT_FALSE(result.networks_telemetry().has_https_latency_data());

  ASSERT_THAT(result.networks_telemetry().network_telemetry(),
              ::testing::SizeIs(networks_data.size()));
  EXPECT_EQ(result.networks_telemetry().network_telemetry(0).guid(),
            networks_data[0].guid);
  EXPECT_EQ(result.networks_telemetry().network_telemetry(0).connection_state(),
            NetworkConnectionState::ONLINE);
  EXPECT_FALSE(
      result.networks_telemetry().network_telemetry(0).has_signal_strength());
  EXPECT_EQ(result.networks_telemetry().network_telemetry(0).device_path(),
            DevicePath(networks_data[0].device_name));
  EXPECT_EQ(result.networks_telemetry().network_telemetry(0).ip_address(),
            networks_data[0].ip_address);
  EXPECT_EQ(result.networks_telemetry().network_telemetry(0).gateway(),
            networks_data[0].gateway);
  EXPECT_EQ(result.networks_telemetry().network_telemetry(0).type(),
            NetworkType::ETHERNET);

  // Make sure wireless interface info wasn't added.
  EXPECT_FALSE(
      result.networks_telemetry().network_telemetry(0).has_tx_bit_rate_mbps());
  EXPECT_FALSE(
      result.networks_telemetry().network_telemetry(0).has_rx_bit_rate_mbps());
  EXPECT_FALSE(
      result.networks_telemetry().network_telemetry(0).has_tx_power_dbm());
  EXPECT_FALSE(
      result.networks_telemetry().network_telemetry(0).has_encryption_on());
  EXPECT_FALSE(
      result.networks_telemetry().network_telemetry(0).has_link_quality());
  EXPECT_FALSE(result.networks_telemetry()
                   .network_telemetry(0)
                   .has_power_management_enabled());
}

TEST_F(NetworkTelemetrySamplerTest, MixTypesAndConfigurations) {
  const std::vector<FakeNetworkData> networks_data = {
      {"guid1", shill::kStateReady, shill::kTypeWifi, 10 /* signal_strength */,
       "wlan0", "192.168.86.25" /* ip_address */, "192.168.86.1" /* gateway */,
       false /* is_portal */, true /* is_visible */, false /* is_configured */},
      {"guid2", shill::kStateOnline, shill::kTypeWifi, 50 /* signal_strength */,
       kInterfaceName, "192.168.86.26" /* ip_address */,
       "192.168.86.2" /* gateway */, false /* is_portal */,
       true /* is_visible */, true /* is_configured */},
      {"guid3", shill::kStateReady, ::chromeos::kTypeTether,
       0 /* signal_strength */, "tether1", "192.168.86.27" /* ip_address */,
       "192.168.86.3" /* gateway */, false /* is_portal */,
       true /* is_visible */, true /* is_configured */}};

  SetNetworkData(networks_data);

  network_handler_test_helper_.ConfigureService(
      R"({"GUID": "guid1", "Type": "wifi", "State": "ready",
            "WiFi.SignalStrengthRssi": -70})");
  network_handler_test_helper_.ConfigureService(
      R"({"GUID": "guid2", "Type": "wifi", "State": "online",
            "WiFi.SignalStrengthRssi": -60})");

  NetworkTelemetrySampler network_telemetry_sampler(
      https_latency_sampler_.get());
  test::TestEvent<absl::optional<MetricData>> metric_collect_event;
  network_telemetry_sampler.MaybeCollect(metric_collect_event.cb());
  const absl::optional<MetricData> optional_result =
      metric_collect_event.result();

  ASSERT_TRUE(optional_result.has_value());
  ASSERT_TRUE(optional_result->has_telemetry_data());
  const TelemetryData& result = optional_result->telemetry_data();
  ASSERT_TRUE(result.has_networks_telemetry());

  // An online network exists, latency data should be collected.
  ASSERT_TRUE(result.networks_telemetry().has_https_latency_data());
  EXPECT_EQ(result.networks_telemetry().https_latency_data().verdict(),
            kVerdict);
  EXPECT_EQ(result.networks_telemetry().https_latency_data().problem(),
            kLatencyProblem);
  EXPECT_EQ(result.networks_telemetry().https_latency_data().latency_ms(),
            kLatencyMs);

  // Not configured network is not included
  ASSERT_THAT(result.networks_telemetry().network_telemetry(),
              ::testing::SizeIs(networks_data.size() - 1));

  // Wifi
  EXPECT_EQ(result.networks_telemetry().network_telemetry(0).guid(),
            networks_data[1].guid);
  EXPECT_EQ(result.networks_telemetry().network_telemetry(0).connection_state(),
            NetworkConnectionState::ONLINE);
  EXPECT_EQ(result.networks_telemetry().network_telemetry(0).signal_strength(),
            networks_data[1].signal_strength);
  EXPECT_EQ(result.networks_telemetry().network_telemetry(0).device_path(),
            DevicePath(networks_data[1].device_name));
  EXPECT_EQ(result.networks_telemetry().network_telemetry(0).ip_address(),
            networks_data[1].ip_address);
  EXPECT_EQ(result.networks_telemetry().network_telemetry(0).gateway(),
            networks_data[1].gateway);
  EXPECT_EQ(result.networks_telemetry().network_telemetry(0).type(),
            NetworkType::WIFI);
  EXPECT_EQ(
      result.networks_telemetry().network_telemetry(0).signal_strength_dbm(),
      -60);

  EXPECT_EQ(result.networks_telemetry().network_telemetry(0).tx_bit_rate_mbps(),
            kTxBitRateMbps);
  EXPECT_EQ(result.networks_telemetry().network_telemetry(0).rx_bit_rate_mbps(),
            kRxBitRateMbps);
  EXPECT_EQ(result.networks_telemetry().network_telemetry(0).tx_power_dbm(),
            kTxPowerDbm);
  EXPECT_EQ(result.networks_telemetry().network_telemetry(0).encryption_on(),
            kEncryptionOn);
  EXPECT_EQ(result.networks_telemetry().network_telemetry(0).link_quality(),
            kLinkQuality);
  EXPECT_EQ(result.networks_telemetry()
                .network_telemetry(0)
                .power_management_enabled(),
            kPowerManagementOn);

  // TETHER
  EXPECT_EQ(result.networks_telemetry().network_telemetry(1).guid(),
            networks_data[2].guid);
  EXPECT_EQ(result.networks_telemetry().network_telemetry(1).connection_state(),
            NetworkConnectionState::CONNECTED);
  EXPECT_FALSE(
      result.networks_telemetry().network_telemetry(1).has_signal_strength());
  EXPECT_EQ(result.networks_telemetry().network_telemetry(1).device_path(),
            DevicePath(networks_data[2].device_name));
  EXPECT_EQ(result.networks_telemetry().network_telemetry(1).ip_address(),
            networks_data[2].ip_address);
  EXPECT_EQ(result.networks_telemetry().network_telemetry(1).gateway(),
            networks_data[2].gateway);
  EXPECT_EQ(result.networks_telemetry().network_telemetry(1).type(),
            NetworkType::TETHER);

  // Make sure wireless info wasn't added to tether.
  EXPECT_FALSE(
      result.networks_telemetry().network_telemetry(1).has_tx_bit_rate_mbps());
  EXPECT_FALSE(
      result.networks_telemetry().network_telemetry(1).has_rx_bit_rate_mbps());
  EXPECT_FALSE(
      result.networks_telemetry().network_telemetry(1).has_tx_power_dbm());
  EXPECT_FALSE(
      result.networks_telemetry().network_telemetry(1).has_encryption_on());
  EXPECT_FALSE(
      result.networks_telemetry().network_telemetry(1).has_link_quality());
  EXPECT_FALSE(result.networks_telemetry()
                   .network_telemetry(1)
                   .has_power_management_enabled());
}

TEST_F(NetworkTelemetrySamplerTest, WifiNotConnected) {
  const std::vector<FakeNetworkData> networks_data = {
      {"guid1", shill::kStateIdle, shill::kTypeWifi, kSignalStrength,
       kInterfaceName, "" /* ip_address */, "" /* gateway */,
       false /* is_portal */, true /* is_visible */, true /* is_configured */}};

  SetNetworkData(networks_data);
  network_handler_test_helper_.ConfigureService(
      R"({"GUID": "guid1", "Type": "wifi", "State": "idle",
            "WiFi.SignalStrengthRssi": -70})");
  NetworkTelemetrySampler network_telemetry_sampler(
      https_latency_sampler_.get());
  test::TestEvent<absl::optional<MetricData>> metric_collect_event;
  network_telemetry_sampler.MaybeCollect(metric_collect_event.cb());
  const absl::optional<MetricData> optional_result =
      metric_collect_event.result();

  ASSERT_TRUE(optional_result.has_value());
  ASSERT_TRUE(optional_result->has_telemetry_data());
  const TelemetryData& result = optional_result->telemetry_data();
  ASSERT_TRUE(result.has_networks_telemetry());

  // No online networks, no latency data should be collected.
  EXPECT_FALSE(result.networks_telemetry().has_https_latency_data());

  ASSERT_THAT(result.networks_telemetry().network_telemetry(),
              ::testing::SizeIs(networks_data.size()));
  EXPECT_EQ(result.networks_telemetry().network_telemetry(0).guid(),
            networks_data[0].guid);
  EXPECT_EQ(result.networks_telemetry().network_telemetry(0).connection_state(),
            NetworkConnectionState::NOT_CONNECTED);
  EXPECT_EQ(result.networks_telemetry().network_telemetry(0).signal_strength(),
            kSignalStrength);
  EXPECT_EQ(result.networks_telemetry().network_telemetry(0).device_path(),
            DevicePath(networks_data[0].device_name));
  EXPECT_FALSE(
      result.networks_telemetry().network_telemetry(0).has_ip_address());
  EXPECT_FALSE(result.networks_telemetry().network_telemetry(0).has_gateway());
  EXPECT_EQ(result.networks_telemetry().network_telemetry(0).type(),
            NetworkType::WIFI);
  EXPECT_EQ(
      result.networks_telemetry().network_telemetry(0).signal_strength_dbm(),
      -70);

  // Make sure wireless link info wasn't added since the network is not
  // connected.
  EXPECT_FALSE(
      result.networks_telemetry().network_telemetry(0).has_tx_bit_rate_mbps());
  EXPECT_FALSE(
      result.networks_telemetry().network_telemetry(0).has_rx_bit_rate_mbps());
  EXPECT_FALSE(
      result.networks_telemetry().network_telemetry(0).has_tx_power_dbm());
  EXPECT_FALSE(
      result.networks_telemetry().network_telemetry(0).has_encryption_on());
  EXPECT_FALSE(
      result.networks_telemetry().network_telemetry(0).has_link_quality());

  // Power management can still be added with non connected networks.
  ASSERT_TRUE(result.networks_telemetry()
                  .network_telemetry(0)
                  .has_power_management_enabled());
  EXPECT_EQ(result.networks_telemetry()
                .network_telemetry(0)
                .power_management_enabled(),
            kPowerManagementOn);
}
}  // namespace
}  // namespace reporting
