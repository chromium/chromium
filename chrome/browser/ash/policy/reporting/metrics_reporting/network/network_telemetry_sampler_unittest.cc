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
#include "chromeos/dbus/shill/shill_ipconfig_client.h"
#include "chromeos/dbus/shill/shill_service_client.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_handler_test_helper.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/tether_constants.h"
#include "components/reporting/metrics/fake_sampler.h"
#include "components/reporting/proto/synced/metric_data.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace reporting {
namespace {

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
  base::test::SingleThreadTaskEnvironment task_environment;

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
  network_telemetry_sampler.Collect(
      base::BindLambdaForTesting([&result](MetricData metric_data) {
        result = std::move(metric_data.telemetry_data());
      }));

  EXPECT_EQ(result.networks_telemetry().https_latency_data().verdict(),
            latency_data->verdict());
  EXPECT_EQ(result.networks_telemetry().https_latency_data().problem(),
            latency_data->problem());
  EXPECT_EQ(result.networks_telemetry().https_latency_data().latency_ms(),
            latency_data->latency_ms());
  return result;
}

TEST(NetworkTelemetrySamplerTest, CellularConnecting) {
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

TEST(NetworkTelemetrySamplerTest, VpnInvisibleNotConnected) {
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

TEST(NetworkTelemetrySamplerTest, EthernetPortal) {
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

TEST(NetworkTelemetrySamplerTest, MixTypesAndConfigurations) {
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
}
}  // namespace
}  // namespace reporting
