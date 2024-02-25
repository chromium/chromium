// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/vpn_feature_pod_controller.h"

#include <vector>

#include "ash/shell.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/network/tray_network_state_model.h"
#include "ash/system/unified/feature_tile.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/test/ash_test_base.h"
#include "chromeos/ash/services/network_config/public/cpp/cros_network_config_test_helper.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"

namespace ash {

namespace {

using ::chromeos::network_config::mojom::ConnectionStateType;
using ::chromeos::network_config::mojom::NetworkStatePropertiesPtr;
using ::chromeos::network_config::mojom::NetworkType;

TrayNetworkStateModel* GetNetworkStateModel() {
  return Shell::Get()->system_tray_model()->network_state_model();
}

}  // namespace

class VPNFeaturePodControllerTest : public AshTestBase {
 public:
  VPNFeaturePodControllerTest() = default;

  // AshTestBase:
  void TearDown() override {
    feature_tile_.reset();
    feature_pod_controller_.reset();
    AshTestBase::TearDown();
  }

  void CreateTile() {
    UnifiedSystemTray* system_tray = GetPrimaryUnifiedSystemTray();
    system_tray->ShowBubble();

    feature_pod_controller_ = std::make_unique<VPNFeaturePodController>(
        system_tray->bubble()->unified_system_tray_controller());
    feature_tile_ = feature_pod_controller_->CreateTile();
  }

  // Simulates having a VPN configured.
  void SimulateVpn() {
    std::vector<NetworkStatePropertiesPtr> networks;
    networks.push_back(
        network_config_test_helper_.CreateStandaloneNetworkProperties(
            "vpn_id", NetworkType::kVPN, ConnectionStateType::kConnected,
            /*signal_strength=*/100));
    GetNetworkStateModel()->OnGetVirtualNetworks(std::move(networks));
  }

  network_config::CrosNetworkConfigTestHelper network_config_test_helper_;
  std::unique_ptr<VPNFeaturePodController> feature_pod_controller_;
  std::unique_ptr<FeatureTile> feature_tile_;
};

TEST_F(VPNFeaturePodControllerTest, TileNotVisibleWithNoVPNs) {
  ASSERT_FALSE(GetNetworkStateModel()->has_vpn());
  CreateTile();
  EXPECT_FALSE(feature_tile_->GetVisible());
}

TEST_F(VPNFeaturePodControllerTest, Basics) {
  SimulateVpn();
  ASSERT_TRUE(GetNetworkStateModel()->has_vpn());
  CreateTile();
  EXPECT_TRUE(feature_tile_->GetVisible());
  EXPECT_EQ(feature_tile_->GetTooltipText(), u"Show VPN settings");
  ASSERT_TRUE(feature_tile_->drill_in_arrow());
  EXPECT_TRUE(feature_tile_->drill_in_arrow()->GetVisible());
}

}  // namespace ash
