// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>
#include <vector>

#include "ash/system/network/tray_network_state_model.h"
#include "ash/system/network/vpn_detailed_view.h"
#include "ash/system/unified/quick_settings_view.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/pixel/ash_pixel_differ.h"
#include "ash/test/pixel/ash_pixel_test_init_params.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "chromeos/services/network_config/public/mojom/network_types.mojom.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"

namespace ash {

using chromeos::network_config::mojom::ConnectionStateType;
using chromeos::network_config::mojom::NetworkStateProperties;
using chromeos::network_config::mojom::NetworkStatePropertiesPtr;
using chromeos::network_config::mojom::NetworkType;
using chromeos::network_config::mojom::NetworkTypeStateProperties;
using chromeos::network_config::mojom::VpnProvider;
using chromeos::network_config::mojom::VpnProviderPtr;
using chromeos::network_config::mojom::VPNStateProperties;
using chromeos::network_config::mojom::VPNStatePropertiesPtr;
using chromeos::network_config::mojom::VpnType;

constexpr char kArcProviderAppId[] = "arc_provider_app_id";
constexpr char kArcProviderId[] = "arc_provider_id";
constexpr char kArcProviderName[] = "arc_provider_name";
constexpr char kExtensionProviderAppId[] = "extension_provider_app_id";
constexpr char kExtensionProviderId[] = "extension_provider_id";
constexpr char kExtensionProviderName[] = "extension_provider_name";

// Pixel test for the VPN list that is shown in the quick settings VPN sub-page.
class VpnDetailedViewPixelTest : public AshTestBase {
 public:
  VpnDetailedViewPixelTest() = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    // Show the detailed view.
    UnifiedSystemTray* system_tray = GetPrimaryUnifiedSystemTray();
    system_tray->ShowBubble();
    ASSERT_TRUE(system_tray->bubble());
    system_tray->bubble()
        ->unified_system_tray_controller()
        ->ShowVPNDetailedView();

    TrayDetailedView* detailed_view =
        system_tray->bubble()
            ->quick_settings_view()
            ->GetDetailedViewForTest<TrayDetailedView>();
    ASSERT_TRUE(detailed_view);
    ASSERT_TRUE(views::IsViewClass<VpnDetailedView>(detailed_view));
    vpn_detailed_view_ = static_cast<VpnDetailedView*>(detailed_view);
  }

  std::optional<pixel_test::InitParams> CreatePixelTestInitParams()
      const override {
    return pixel_test::InitParams();
  }

  void AddBuiltInProvider() {
    // Updating the list of third-party providers adds the built-in provider,
    // even if that list is empty.
    vpn_detailed_view_->model()->vpn_list()->SetVpnProvidersForTest({});
    vpn_detailed_view_->OnGetNetworkStateList({});
  }

  void AddMultipleProvidersAndNetwork() {
    std::vector<VpnProviderPtr> providers;
    // Add an extension provider.
    VpnProviderPtr provider = VpnProvider::New();
    provider->type = VpnType::kExtension;
    provider->provider_name = kExtensionProviderName;
    provider->provider_id = kExtensionProviderId;
    provider->app_id = kExtensionProviderAppId;
    providers.push_back(std::move(provider));
    // Add an ARC provider.
    provider = VpnProvider::New();
    provider->type = VpnType::kArc;
    provider->provider_name = kArcProviderName;
    provider->provider_id = kArcProviderId;
    provider->app_id = kArcProviderAppId;
    providers.push_back(std::move(provider));
    vpn_detailed_view_->model()->vpn_list()->SetVpnProvidersForTest(
        std::move(providers));

    // Add a network just for the extension provider.
    NetworkStatePropertiesPtr network = NetworkStateProperties::New();
    network->guid = "vpn_id";
    network->name = "vpn_name";
    network->type = NetworkType::kVPN;
    network->connection_state = ConnectionStateType::kNotConnected;
    VPNStatePropertiesPtr vpn = VPNStateProperties::New();
    vpn->type = VpnType::kExtension;
    vpn->provider_name = kExtensionProviderName;
    vpn->provider_id = kExtensionProviderId;
    network->type_state = NetworkTypeStateProperties::NewVpn(std::move(vpn));
    std::vector<NetworkStatePropertiesPtr> networks;
    networks.push_back(std::move(network));
    vpn_detailed_view_->OnGetNetworkStateList(std::move(networks));
  }

  raw_ptr<VpnDetailedView, DanglingUntriaged> vpn_detailed_view_ = nullptr;
};

TEST_F(VpnDetailedViewPixelTest, OnlyBuiltInVpn) {
  AddBuiltInProvider();

  // Compare pixels.
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "check_view",
      /*revision_number=*/10, vpn_detailed_view_));
}

TEST_F(VpnDetailedViewPixelTest, MultipleVpns) {
  AddMultipleProvidersAndNetwork();

  // Compare pixels.
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "check_view",
      /*revision_number=*/10, vpn_detailed_view_));
}

}  // namespace ash
