// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/system/network/network_feature_pod_controller.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/pixel/ash_pixel_differ.h"
#include "ash/test/pixel/ash_pixel_test_init_params.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ash/components/network/network_state_test_helper.h"
#include "chromeos/ash/services/network_config/public/cpp/cros_network_config_test_helper.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

constexpr char kStubCellularDevice[] = "/device/stub_cellular_device";

// The GUIDs used for the different network types.
constexpr char kNetworkGuidCellular[] = "cellular_guid";
constexpr char kNetworkGuidWifi[] = "wifi_guid";
constexpr char kNetworkGuidEthernet[] = "ethernet_guid";

// The templates used to configure services for different network types.
constexpr char kServicePatternCellular[] = R"({
    "GUID": "%s", "Type": "cellular", "State": "online", "Strength": 100,
    "Device": "%s", "Cellular.NetworkTechnology": "LTE"})";

constexpr char kServicePatternEthernet[] = R"({
    "GUID": "%s", "Type": "ethernet", "State": "online"})";

constexpr char kServicePatternWiFi[] = R"({
    "GUID": "%s", "Type": "wifi", "State": "online", "Strength": 100})";
}  // namespace

// Pixel test for the quick settings network feature tile view.
class NetworkFeatureTilePixelTest : public AshTestBase {
 public:
  NetworkFeatureTilePixelTest() {
    feature_list_.InitWithFeatures(
        {features::kQsRevamp, chromeos::features::kJelly}, {});
  }

  void SetUp() override {
    AshTestBase::SetUp();

    GetPrimaryUnifiedSystemTray()->ShowBubble();

    network_feature_pod_controller_ =
        std::make_unique<NetworkFeaturePodController>(tray_controller());

    auto feature_tile = network_feature_pod_controller_->CreateTile();

    widget_ = CreateFramelessTestWidget();
    widget_->SetFullscreen(true);

    feature_tile_ = widget_->SetContentsView(std::move(feature_tile));

    // Add the non-default cellular and ethernet devices to Shill.
    network_state_helper()->manager_test()->AddTechnology(shill::kTypeCellular,
                                                          /*enabled=*/true);
    network_state_helper()->AddDevice(kStubCellularDevice, shill::kTypeCellular,
                                      "stub_cellular_device");
    network_state_helper()->AddDevice("/device/stub_eth_device",
                                      shill::kTypeEthernet, "stub_eth_device");

    network_state_handler()->SetTetherTechnologyState(
        NetworkStateHandler::TechnologyState::TECHNOLOGY_ENABLED);
  }

  void TearDown() override {
    widget_.reset();
    network_feature_pod_controller_.reset();

    AshTestBase::TearDown();
  }

  // AshTestBase:
  absl::optional<pixel_test::InitParams> CreatePixelTestInitParams()
      const override {
    return pixel_test::InitParams();
  }

  FeatureTile* feature_tile() { return feature_tile_; }

  NetworkStateTestHelper* network_state_helper() {
    return &network_config_helper_.network_state_helper();
  }

  UnifiedSystemTrayController* tray_controller() {
    return GetPrimaryUnifiedSystemTray()
        ->bubble()
        ->unified_system_tray_controller();
  }

  NetworkStateHandler* network_state_handler() {
    return network_state_helper()->network_state_handler();
  }

  void SetupEthernet() {
    ASSERT_TRUE(ethernet_path_.empty());
    ethernet_path_ = ConfigureService(
        base::StringPrintf(kServicePatternEthernet, kNetworkGuidEthernet));
    base::RunLoop().RunUntilIdle();
  }

  void SetupWiFi() {
    ASSERT_TRUE(wifi_path_.empty());
    wifi_path_ = ConfigureService(
        base::StringPrintf(kServicePatternWiFi, kNetworkGuidWifi));
    base::RunLoop().RunUntilIdle();
  }

  void SetupCellular() {
    ASSERT_TRUE(cellular_path_.empty());
    cellular_path_ = ConfigureService(base::StringPrintf(
        kServicePatternCellular, kNetworkGuidCellular, kStubCellularDevice));
    base::RunLoop().RunUntilIdle();
  }

 private:
  std::string ConfigureService(const std::string& shill_json_string) {
    return network_state_helper()->ConfigureService(shill_json_string);
  }

  base::test::ScopedFeatureList feature_list_;
  std::string cellular_path_;
  std::string ethernet_path_;
  std::string wifi_path_;
  std::unique_ptr<views::Widget> widget_;
  std::unique_ptr<NetworkFeaturePodController> network_feature_pod_controller_;
  network_config::CrosNetworkConfigTestHelper network_config_helper_;
  // Owned by `widget_`.
  raw_ptr<FeatureTile, ExperimentalAsh> feature_tile_ = nullptr;
};

TEST_F(NetworkFeatureTilePixelTest, NoNetworks) {
  auto* tile_view = feature_tile();
  ASSERT_TRUE(tile_view);
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "check_tile_view",
      /*revision_number=*/2, tile_view));
}

TEST_F(NetworkFeatureTilePixelTest, Ethernet) {
  ASSERT_TRUE(network_state_handler()->IsTechnologyEnabled(
      NetworkTypePattern::Ethernet()));

  SetupEthernet();
  auto* tile_view = feature_tile();
  ASSERT_TRUE(tile_view);
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "check_tile_view",
      /*revision_number=*/1, tile_view));
}

TEST_F(NetworkFeatureTilePixelTest, Wifi) {
  ASSERT_TRUE(
      network_state_handler()->IsTechnologyEnabled(NetworkTypePattern::WiFi()));

  SetupWiFi();
  auto* tile_view = feature_tile();
  ASSERT_TRUE(tile_view);
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "check_tile_view",
      /*revision_number=*/1, tile_view));
}

TEST_F(NetworkFeatureTilePixelTest, Cellular) {
  ASSERT_TRUE(network_state_handler()->IsTechnologyEnabled(
      NetworkTypePattern::Cellular()));

  SetupCellular();
  auto* tile_view = feature_tile();
  ASSERT_TRUE(tile_view);
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "check_tile_view",
      /*revision_number=*/1, tile_view));
}

}  // namespace ash
