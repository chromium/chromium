// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/network/tray_network_state_model.h"
#include "ash/system/unified/quick_settings_view.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "ash/test/pixel/ash_pixel_differ.h"
#include "ash/test/pixel/ash_pixel_test_init_params.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ash/services/network_config/public/cpp/cros_network_config_test_helper.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/services/network_config/public/cpp/fake_cros_network_config.h"

namespace ash {
namespace {

using ::chromeos::network_config::FakeCrosNetworkConfig;
using ::chromeos::network_config::mojom::ConnectionStateType;
using ::chromeos::network_config::mojom::DeviceStateType;
using ::chromeos::network_config::mojom::NetworkType;
using network_config::CrosNetworkConfigTestHelper;

// Pixel tests for the quick settings network detailed view.
class NetworkDetailedNetworkViewPixelTest : public AshTestBase {
 public:
  NetworkDetailedNetworkViewPixelTest() {
    feature_list_.InitWithFeatures(
        {features::kQsRevamp, chromeos::features::kJelly}, {});
  }

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    cros_network_ = std::make_unique<FakeCrosNetworkConfig>();
    Shell::Get()
        ->system_tray_model()
        ->network_state_model()
        ->ConfigureRemoteForTesting(cros_network_->GetPendingRemote());
    base::RunLoop().RunUntilIdle();
  }

  absl::optional<pixel_test::InitParams> CreatePixelTestInitParams()
      const override {
    return pixel_test::InitParams();
  }

  FakeCrosNetworkConfig* cros_network() { return cros_network_.get(); }

 private:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<FakeCrosNetworkConfig> cros_network_;
};

TEST_F(NetworkDetailedNetworkViewPixelTest, Basics) {
  cros_network()->AddNetworkAndDevice(
      CrosNetworkConfigTestHelper::CreateStandaloneNetworkProperties(
          "My Ethernet", NetworkType::kEthernet,
          ConnectionStateType::kConnected));

  cros_network()->AddNetworkAndDevice(
      CrosNetworkConfigTestHelper::CreateStandaloneNetworkProperties(
          "My Cellular", NetworkType::kCellular,
          ConnectionStateType::kConnected));

  cros_network()->AddNetworkAndDevice(
      CrosNetworkConfigTestHelper::CreateStandaloneNetworkProperties(
          "My Wi-Fi", NetworkType::kWiFi, ConnectionStateType::kConnected));

  // Show the system tray bubble.
  UnifiedSystemTray* system_tray = GetPrimaryUnifiedSystemTray();
  system_tray->ShowBubble();
  ASSERT_TRUE(system_tray->bubble());

  // Show the detailed view.
  system_tray->bubble()
      ->unified_system_tray_controller()
      ->ShowNetworkDetailedView(/*force=*/true);
  views::View* detailed_view =
      system_tray->bubble()->quick_settings_view()->detailed_view();
  ASSERT_TRUE(detailed_view);

  // Compare pixels.
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "check_view",
      /*revision_number=*/0, detailed_view));
}

}  // namespace
}  // namespace ash
