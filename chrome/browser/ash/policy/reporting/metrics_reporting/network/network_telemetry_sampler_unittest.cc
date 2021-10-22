// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/network/network_telemetry_sampler.h"

#include "base/test/bind.h"
#include "base/values.h"
#include "chromeos/dbus/shill/shill_service_client.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_handler_test_helper.h"
#include "chromeos/network/network_state_handler.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

#include "base/logging.h"

namespace reporting {

class FakeHttpsLatencySampler : public Sampler {
 public:
  ~FakeHttpsLatencySampler() override = default;

  explicit FakeHttpsLatencySampler(MetricData metric_data)
      : metric_data_(std::move(metric_data)) {}

  void Collect(MetricCallback callback) override {
    std::move(callback).Run(metric_data_);
  }

 private:
  const MetricData metric_data_;
};

struct FakeNetworkData {
  std::string guid;
  std::string connection_state;
  std::string type;
  int signal_strength;
  bool is_portal;
  bool is_visible;
  bool is_configured;
};

TelemetryData NetworkTelemetrySamplerTestHelper(
    const std::vector<FakeNetworkData>& networks_data) {
  content::BrowserTaskEnvironment task_environment_;

  MetricData metric_data;
  auto* latency_data = metric_data.mutable_telemetry_data()
                           ->mutable_networks_telemetry()
                           ->mutable_https_latency_data();
  latency_data->set_verdict(RoutineVerdict::PROBLEM);
  latency_data->set_problem(HttpsLatencyProblem::VERY_HIGH_LATENCY);
  latency_data->set_latency_ms(3000);
  auto https_latency_sampler =
      std::make_unique<FakeHttpsLatencySampler>(metric_data);

  chromeos::NetworkHandlerTestHelper network_handler_test_helper;
  const std::string profile_path = "/profile/path";
  network_handler_test_helper.profile_test()->AddProfile(profile_path,
                                                         "user_hash");
  chromeos::ShillServiceClient::TestInterface* service_client =
      network_handler_test_helper.service_test();
  base::RunLoop().RunUntilIdle();
  service_client->ClearServices();

  for (const auto& network_data : networks_data) {
    const std::string service_path = "service_path" + network_data.guid;
    service_client->AddService(service_path, network_data.guid, "name",
                               network_data.type, network_data.connection_state,
                               network_data.is_visible);
    service_client->SetServiceProperty(
        service_path, shill::kSignalStrengthProperty,
        base::Value(network_data.signal_strength));
    ash::NetworkHandler::Get()
        ->network_state_handler()
        ->SetNetworkChromePortalDetected(service_path, network_data.is_portal);
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

TEST(NetworkTelemetrySamplerTest, WifiConnecting) {
  const std::vector<FakeNetworkData> networks_data = {
      {"guid1", shill::kStateConfiguration, shill::kTypeWifi, 10,
       false /* is_portal */, true /* is_visible */, true /* is_configured */}};

  TelemetryData result = NetworkTelemetrySamplerTestHelper(networks_data);

  ASSERT_EQ(result.networks_telemetry().network_telemetry_size(),
            networks_data.size());
  EXPECT_EQ(result.networks_telemetry().network_telemetry(0).guid(),
            networks_data[0].guid);
  EXPECT_EQ(result.networks_telemetry().network_telemetry(0).connection_state(),
            NetworkConnectionState::CONNECTING);
  EXPECT_EQ(result.networks_telemetry().network_telemetry(0).signal_strength(),
            networks_data[0].signal_strength);
}

TEST(NetworkTelemetrySamplerTest, WifiInvisibleNotConnected) {
  const std::vector<FakeNetworkData> networks_data = {
      {"guid1", shill::kStateOffline, shill::kTypeWifi, 10,
       false /* is_portal */, false /* is_visible */,
       true /* is_configured */}};

  TelemetryData result = NetworkTelemetrySamplerTestHelper(networks_data);

  ASSERT_EQ(result.networks_telemetry().network_telemetry_size(),
            networks_data.size());
  EXPECT_EQ(result.networks_telemetry().network_telemetry(0).guid(),
            networks_data[0].guid);
  EXPECT_EQ(result.networks_telemetry().network_telemetry(0).connection_state(),
            NetworkConnectionState::NOT_CONNECTED);
  EXPECT_EQ(result.networks_telemetry().network_telemetry(0).signal_strength(),
            networks_data[0].signal_strength);
}

TEST(NetworkTelemetrySamplerTest, WifiPortal) {
  const std::vector<FakeNetworkData> networks_data = {
      {"guid1", shill::kStateRedirectFound, shill::kTypeWifi, 10,
       true /* is_portal */, true /* is_visible */, true /* is_configured */}};

  TelemetryData result = NetworkTelemetrySamplerTestHelper(networks_data);

  ASSERT_EQ(result.networks_telemetry().network_telemetry_size(),
            networks_data.size());
  EXPECT_EQ(result.networks_telemetry().network_telemetry(0).guid(),
            networks_data[0].guid);
  EXPECT_EQ(result.networks_telemetry().network_telemetry(0).connection_state(),
            NetworkConnectionState::PORTAL);
  EXPECT_EQ(result.networks_telemetry().network_telemetry(0).signal_strength(),
            networks_data[0].signal_strength);
}

TEST(NetworkTelemetrySamplerTest, MixTypesAndConfigurations) {
  const std::vector<FakeNetworkData> networks_data = {
      {"guid1", shill::kStateReady, shill::kTypeWifi, 10, false /* is_portal */,
       true /* is_visible */, false /* is_configured */},
      {"guid2", shill::kStateReady, shill::kTypeEthernet, -10,
       false /* is_portal */, true /* is_visible */, true /* is_configured */},
      {"guid3", shill::kStateOnline, shill::kTypeWifi, 50,
       false /* is_portal */, true /* is_visible */, true /* is_configured */}};

  TelemetryData result = NetworkTelemetrySamplerTestHelper(networks_data);

  // Not configured network is not included
  ASSERT_EQ(result.networks_telemetry().network_telemetry_size(),
            networks_data.size() - 1);

  // Ethernet
  EXPECT_EQ(result.networks_telemetry().network_telemetry(0).guid(),
            networks_data[1].guid);
  EXPECT_EQ(result.networks_telemetry().network_telemetry(0).connection_state(),
            NetworkConnectionState::CONNECTED);
  EXPECT_FALSE(
      result.networks_telemetry().network_telemetry(0).has_signal_strength());

  // Wifi
  EXPECT_EQ(result.networks_telemetry().network_telemetry(1).guid(),
            networks_data[2].guid);
  EXPECT_EQ(result.networks_telemetry().network_telemetry(1).connection_state(),
            NetworkConnectionState::ONLINE);
  EXPECT_EQ(result.networks_telemetry().network_telemetry(1).signal_strength(),
            networks_data[2].signal_strength);
}

}  // namespace reporting
