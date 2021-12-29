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
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chromeos/dbus/cros_healthd/cros_healthd_client.h"
#include "chromeos/dbus/cros_healthd/fake_cros_healthd_client.h"
#include "chromeos/dbus/shill/shill_ipconfig_client.h"
#include "chromeos/dbus/shill/shill_service_client.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_handler_test_helper.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/tether_constants.h"
#include "chromeos/services/cros_healthd/public/cpp/service_connection.h"
#include "components/reporting/metrics/fake_sampler.h"
#include "components/reporting/proto/synced/metric_data.pb.h"
#include "components/reporting/util/test_support_callbacks.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace reporting {
namespace {

// Wifi constants.
constexpr char kAccessPointAddress[] = "access_point";
constexpr bool kEncryptionOn = true;
constexpr bool kPowerManagementOn = true;
constexpr int64_t kTxBitRateMbps = 8;
constexpr int64_t kRxBitRateMbps = 4;
constexpr int64_t kTxPowerDbm = 2;
constexpr int64_t kLinkQuality = 1;

struct FakeNetworkData {
  std::string guid;
  std::string connection_state;
  std::string type;
  int signal_strength;
  std::string device_path;
  std::string ip_address;
  std::string gateway;
  bool is_portal;
  bool is_visible;
  bool is_configured;
};

TelemetryData NetworkTelemetrySamplerTestHelper(
    const std::vector<FakeNetworkData>& networks_data) {
  MetricData metric_data;
  auto* latency_data = metric_data.mutable_telemetry_data()
                           ->mutable_networks_telemetry()
                           ->mutable_https_latency_data();
  latency_data->set_verdict(RoutineVerdict::PROBLEM);
  latency_data->set_problem(HttpsLatencyProblem::VERY_HIGH_LATENCY);
  latency_data->set_latency_ms(3000);
  auto https_latency_sampler = std::make_unique<test::FakeSampler>();
  https_latency_sampler->SetMetricData(metric_data);

  chromeos::NetworkHandlerTestHelper network_handler_test_helper;
  const std::string profile_path = "/profile/path";
  network_handler_test_helper.profile_test()->AddProfile(profile_path,
                                                         "user_hash");
  chromeos::ShillServiceClient::TestInterface* service_client =
      network_handler_test_helper.service_test();
  chromeos::ShillIPConfigClient::TestInterface* ip_config_client =
      network_handler_test_helper.ip_config_test();
  base::RunLoop().RunUntilIdle();
  service_client->ClearServices();

  network_handler_test_helper.manager_test()->AddTechnology(
      ::chromeos::kTypeTether, true);
  for (const auto& network_data : networks_data) {
    const std::string service_path =
        base::StrCat({"service_path", network_data.guid});
    service_client->AddService(service_path, network_data.guid, "name",
                               network_data.type, network_data.connection_state,
                               network_data.is_visible);
    service_client->SetServiceProperty(
        service_path, shill::kSignalStrengthProperty,
        base::Value(network_data.signal_strength));
    ash::NetworkHandler::Get()
        ->network_state_handler()
        ->SetNetworkChromePortalDetected(service_path, network_data.is_portal);
    service_client->SetServiceProperty(service_path, shill::kDeviceProperty,
                                       base::Value(network_data.device_path));
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
      service_client->SetServiceProperty(service_path, shill::kProfileProperty,
                                         base::Value(profile_path));
    }
  }
  base::RunLoop().RunUntilIdle();

  TelemetryData result;
  NetworkTelemetrySampler network_telemetry_sampler(
      https_latency_sampler.get());
  test::TestEvent<MetricData> metric_collect_event;
  network_telemetry_sampler.Collect(metric_collect_event.cb());
  result = metric_collect_event.result().telemetry_data();

  EXPECT_EQ(result.networks_telemetry().https_latency_data().verdict(),
            latency_data->verdict());
  EXPECT_EQ(result.networks_telemetry().https_latency_data().problem(),
            latency_data->problem());
  EXPECT_EQ(result.networks_telemetry().https_latency_data().latency_ms(),
            latency_data->latency_ms());
  return result;
}

chromeos::cros_healthd::mojom::TelemetryInfoPtr CreateWifiResult(
    const std::string& interface_name,
    bool power_management_enabled,
    const std::string& access_point_address,
    int64_t tx_bit_rate_mbps,
    int64_t rx_bit_rate_mbps,
    int64_t tx_power_dbm,
    bool encryption_on,
    int64_t link_quality,
    int signal_level_dbm) {
  auto telemetry_info = chromeos::cros_healthd::mojom::TelemetryInfo::New();
  std::vector<chromeos::cros_healthd::mojom::NetworkInterfaceInfoPtr>
      network_interfaces;

  auto wireless_link_info =
      chromeos::cros_healthd::mojom::WirelessLinkInfo::New(
          access_point_address, tx_bit_rate_mbps, rx_bit_rate_mbps,
          tx_power_dbm, encryption_on, link_quality, signal_level_dbm);
  auto wireless_interface_info =
      chromeos::cros_healthd::mojom::WirelessInterfaceInfo::New(
          interface_name, power_management_enabled,
          std::move(wireless_link_info));
  network_interfaces.push_back(
      chromeos::cros_healthd::mojom::NetworkInterfaceInfo::
          NewWirelessInterfaceInfo(std::move(wireless_interface_info)));
  auto network_interface_result =
      chromeos::cros_healthd::mojom::NetworkInterfaceResult::
          NewNetworkInterfaceInfo(std::move(network_interfaces));

  telemetry_info->network_interface_result =
      std::move(network_interface_result);
  return telemetry_info;
}

class NetworkTelemetrySamplerTest : public testing::Test {
 public:
  NetworkTelemetrySamplerTest() {
    chromeos::CrosHealthdClient::InitializeFake();
  }

  ~NetworkTelemetrySamplerTest() override {
    chromeos::CrosHealthdClient::Shutdown();
    chromeos::cros_healthd::ServiceConnection::GetInstance()->FlushForTesting();
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
};

TEST_F(NetworkTelemetrySamplerTest, CellularConnecting) {
  const std::vector<FakeNetworkData> networks_data = {
      {"guid1", shill::kStateConfiguration, shill::kTypeCellular,
       0 /* signal_strength */, "device/path", "192.168.86.25" /* ip_address */,
       "192.168.86.1" /* gateway */, false /* is_portal */,
       true /* is_visible */, true /* is_configured */}};

  TelemetryData result = NetworkTelemetrySamplerTestHelper(networks_data);

  ASSERT_EQ(result.networks_telemetry().network_telemetry_size(),
            networks_data.size());
  EXPECT_EQ(result.networks_telemetry().network_telemetry(0).guid(),
            networks_data[0].guid);
  EXPECT_EQ(result.networks_telemetry().network_telemetry(0).connection_state(),
            NetworkConnectionState::CONNECTING);
  EXPECT_FALSE(
      result.networks_telemetry().network_telemetry(0).has_signal_strength());
  EXPECT_EQ(result.networks_telemetry().network_telemetry(0).device_path(),
            networks_data[0].device_path);
  EXPECT_EQ(result.networks_telemetry().network_telemetry(0).ip_address(),
            networks_data[0].ip_address);
  EXPECT_EQ(result.networks_telemetry().network_telemetry(0).gateway(),
            networks_data[0].gateway);
  EXPECT_EQ(result.networks_telemetry().network_telemetry(0).type(),
            NetworkType::CELLULAR);
}

TEST_F(NetworkTelemetrySamplerTest, VpnInvisibleNotConnected) {
  const std::vector<FakeNetworkData> networks_data = {
      {"guid1", shill::kStateOffline, shill::kTypeVPN, 0 /* signal_strength */,
       "device/path", "192.168.86.25" /* ip_address */,
       "192.168.86.1" /* gateway */, false /* is_portal */,
       false /* is_visible */, true /* is_configured */}};

  TelemetryData result = NetworkTelemetrySamplerTestHelper(networks_data);

  ASSERT_EQ(result.networks_telemetry().network_telemetry_size(),
            networks_data.size());
  EXPECT_EQ(result.networks_telemetry().network_telemetry(0).guid(),
            networks_data[0].guid);
  EXPECT_EQ(result.networks_telemetry().network_telemetry(0).connection_state(),
            NetworkConnectionState::NOT_CONNECTED);
  EXPECT_FALSE(
      result.networks_telemetry().network_telemetry(0).has_signal_strength());
  EXPECT_EQ(result.networks_telemetry().network_telemetry(0).device_path(),
            networks_data[0].device_path);
  EXPECT_EQ(result.networks_telemetry().network_telemetry(0).ip_address(),
            networks_data[0].ip_address);
  EXPECT_EQ(result.networks_telemetry().network_telemetry(0).gateway(),
            networks_data[0].gateway);
  EXPECT_EQ(result.networks_telemetry().network_telemetry(0).type(),
            NetworkType::VPN);
}

TEST_F(NetworkTelemetrySamplerTest, EthernetPortal) {
  const std::vector<FakeNetworkData> networks_data = {
      {"guid1", shill::kStateRedirectFound, shill::kTypeEthernet,
       0 /* signal_strength */, "device/path", "192.168.86.25" /* ip_address */,
       "192.168.86.1" /* gateway */, true /* is_portal */,
       true /* is_visible */, true /* is_configured */}};

  TelemetryData result = NetworkTelemetrySamplerTestHelper(networks_data);

  ASSERT_EQ(result.networks_telemetry().network_telemetry_size(),
            networks_data.size());
  EXPECT_EQ(result.networks_telemetry().network_telemetry(0).guid(),
            networks_data[0].guid);
  EXPECT_EQ(result.networks_telemetry().network_telemetry(0).connection_state(),
            NetworkConnectionState::PORTAL);
  EXPECT_FALSE(
      result.networks_telemetry().network_telemetry(0).has_signal_strength());
  EXPECT_EQ(result.networks_telemetry().network_telemetry(0).device_path(),
            networks_data[0].device_path);
  EXPECT_EQ(result.networks_telemetry().network_telemetry(0).ip_address(),
            networks_data[0].ip_address);
  EXPECT_EQ(result.networks_telemetry().network_telemetry(0).gateway(),
            networks_data[0].gateway);
  EXPECT_EQ(result.networks_telemetry().network_telemetry(0).type(),
            NetworkType::ETHERNET);
}

TEST_F(NetworkTelemetrySamplerTest, MixTypesAndConfigurations) {
  const std::vector<FakeNetworkData> networks_data = {
      {"guid1", shill::kStateReady, shill::kTypeWifi, 10 /* signal_strength */,
       "device/path1", "192.168.86.25" /* ip_address */,
       "192.168.86.1" /* gateway */, false /* is_portal */,
       true /* is_visible */, false /* is_configured */},
      {"guid2", shill::kStateOnline, shill::kTypeWifi, 50 /* signal_strength */,
       "device/path3", "192.168.86.26" /* ip_address */,
       "192.168.86.2" /* gateway */, false /* is_portal */,
       true /* is_visible */, true /* is_configured */},
      {"guid3", shill::kStateReady, ::chromeos::kTypeTether,
       0 /* signal_strength */, "device/path2",
       "192.168.86.27" /* ip_address */, "192.168.86.3" /* gateway */,
       false /* is_portal */, true /* is_visible */, true /* is_configured */}};

  auto telemetry_info = CreateWifiResult(
      "path3", kPowerManagementOn, kAccessPointAddress, kTxBitRateMbps,
      kRxBitRateMbps, kTxPowerDbm, kEncryptionOn, kLinkQuality,
      /*signal_level=*/0);
  chromeos::cros_healthd::FakeCrosHealthdClient::Get()
      ->SetProbeTelemetryInfoResponseForTesting(telemetry_info);
  TelemetryData result = NetworkTelemetrySamplerTestHelper(networks_data);

  // Not configured network is not included
  ASSERT_EQ(result.networks_telemetry().network_telemetry_size(),
            networks_data.size() - 1);

  // Wifi
  EXPECT_EQ(result.networks_telemetry().network_telemetry(0).guid(),
            networks_data[1].guid);
  EXPECT_EQ(result.networks_telemetry().network_telemetry(0).connection_state(),
            NetworkConnectionState::ONLINE);
  EXPECT_EQ(result.networks_telemetry().network_telemetry(0).signal_strength(),
            networks_data[1].signal_strength);
  EXPECT_EQ(result.networks_telemetry().network_telemetry(0).device_path(),
            networks_data[1].device_path);
  EXPECT_EQ(result.networks_telemetry().network_telemetry(0).ip_address(),
            networks_data[1].ip_address);
  EXPECT_EQ(result.networks_telemetry().network_telemetry(0).gateway(),
            networks_data[1].gateway);
  EXPECT_EQ(result.networks_telemetry().network_telemetry(0).type(),
            NetworkType::WIFI);
  EXPECT_EQ(
      result.networks_telemetry().network_telemetry(0).access_point_address(),
      kAccessPointAddress);
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
            networks_data[2].device_path);
  EXPECT_EQ(result.networks_telemetry().network_telemetry(1).ip_address(),
            networks_data[2].ip_address);
  EXPECT_EQ(result.networks_telemetry().network_telemetry(1).gateway(),
            networks_data[2].gateway);
  EXPECT_EQ(result.networks_telemetry().network_telemetry(1).type(),
            NetworkType::TETHER);
  // Make sure wireless info wasn't added to tether.
  EXPECT_FALSE(result.networks_telemetry()
                   .network_telemetry(1)
                   .has_access_point_address());
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
                   .power_management_enabled());
}
}  // namespace
}  // namespace reporting
