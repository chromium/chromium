// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/boca_ui/provider/network_info_provider.h"

#include <memory>
#include <string>

#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/services/network_config/public/cpp/cros_network_config_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::chromeos::network_config::mojom::ConnectionStateType;

namespace ash::boca {
namespace {

constexpr char kWifiName[] = "wifi";
constexpr int kSignalStrength = 100;

class NetworkInfoProviderTest : public ::testing::Test {
 protected:
  void SetUp() override {
    cros_network_config_helper_.network_state_helper()
        .ResetDevicesAndServices();
    wifi_device_path_ =
        cros_network_config_helper_.network_state_helper().ConfigureWiFi(
            shill::kStateIdle);
    ToggleWifiOnline();
    SetWifiName(kWifiName);
  }

  void ToggleWifiOnline() {
    cros_network_config_helper_.network_state_helper().SetServiceProperty(
        wifi_device_path_, shill::kStateProperty,
        base::Value(shill::kStateOnline));
  }

  void ToggleWifiOffline() {
    cros_network_config_helper_.network_state_helper().SetServiceProperty(
        wifi_device_path_, shill::kStateProperty,
        base::Value(shill::kStateDisconnecting));
  }

  void SetWifiName(const std::string& name) {
    cros_network_config_helper_.network_state_helper().SetServiceProperty(
        wifi_device_path_, shill::kNameProperty, base::Value(name));
  }

  void SetWifiSignalStrength(int signal_strength) {
    cros_network_config_helper_.network_state_helper().SetServiceProperty(
        wifi_device_path_, shill::kSignalStrengthProperty,
        base::Value(signal_strength));
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  std::string wifi_device_path_;
  network_config::CrosNetworkConfigTestHelper cros_network_config_helper_;
};

TEST_F(NetworkInfoProviderTest, FetchActiveNetworksWhenActiveNetworksChanged) {
  std::vector<mojom::NetworkInfoPtr> networks;
  NetworkInfoProvider network_info_provider(base::BindLambdaForTesting(
      [&](std::vector<mojom::NetworkInfoPtr> network_info) {
        networks = std::move(network_info);
      }));

  SetWifiSignalStrength(kSignalStrength);
  ASSERT_EQ(networks.size(), 1u);
  mojom::NetworkInfoPtr network = std::move(networks[0]);
  EXPECT_EQ(network->state, ConnectionStateType::kOnline);
  EXPECT_EQ(network->type, mojom::NetworkType::kWiFi);
  EXPECT_EQ(network->name, kWifiName);
  EXPECT_EQ(network->signal_strength, kSignalStrength);

  ToggleWifiOffline();
  EXPECT_TRUE(networks.empty());
}

}  // namespace
}  // namespace ash::boca
