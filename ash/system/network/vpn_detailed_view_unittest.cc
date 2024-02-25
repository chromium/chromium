// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/vpn_detailed_view.h"

#include "ash/public/cpp/test/test_system_tray_client.h"
#include "ash/system/network/tray_network_state_model.h"
#include "ash/system/tray/fake_detailed_view_delegate.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "chromeos/services/network_config/public/mojom/network_types.mojom.h"
#include "components/onc/onc_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/widget/widget.h"

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

namespace ash {

constexpr char kArcProviderAppId[] = "arc_provider_app_id";
constexpr char kArcProviderId[] = "arc_provider_id";
constexpr char kArcProviderName[] = "arc_provider_name";
constexpr char kExtensionProviderAppId[] = "extension_provider_app_id";
constexpr char kExtensionProviderId[] = "extension_provider_id";
constexpr char kExtensionProviderName[] = "extension_provider_name";

class VpnDetailedViewTest : public AshTestBase {
 public:
  VpnDetailedViewTest() = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    // Create a widget so that tests can click on views.
    widget_ = CreateFramelessTestWidget();
    widget_->SetFullscreen(true);
    delegate_ = std::make_unique<FakeDetailedViewDelegate>();
    vpn_detailed_view_ = widget_->SetContentsView(
        std::make_unique<VpnDetailedView>(delegate_.get(), LoginStatus::USER));
    vpn_detailed_view_->Init();
    vpn_detailed_view_->OnGetNetworkStateList({});
  }

  void TearDown() override {
    widget_.reset();
    vpn_detailed_view_ = nullptr;
    delegate_.reset();
    AshTestBase::TearDown();
  }

  void AddVpnProvidersAndNetwork() {
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

  size_t GetProviderViewCount() {
    return vpn_detailed_view_->provider_view_map_.size();
  }

  size_t GetNetworkViewCount() {
    return vpn_detailed_view_->network_view_guid_map_.size();
  }

  std::vector<const views::View*> GetProviderViews() {
    std::vector<const views::View*> views;
    for (const auto& it : vpn_detailed_view_->provider_view_map_) {
      views.push_back(it.first);
    }
    return views;
  }

  std::vector<const views::View*> GetNetworkViews() {
    std::vector<const views::View*> views;
    for (const auto& it : vpn_detailed_view_->network_view_guid_map_) {
      views.push_back(it.first);
    }
    return views;
  }

  const views::View* GetBuiltInProviderView() {
    for (const auto& it : vpn_detailed_view_->provider_view_map_) {
      if (it.second->type == VpnType::kOpenVPN) {
        return it.first;
      }
    }
    return nullptr;
  }

  const views::View* GetExtensionProviderView() {
    for (const auto& it : vpn_detailed_view_->provider_view_map_) {
      if (it.second->type == VpnType::kExtension) {
        return it.first;
      }
    }
    return nullptr;
  }

  const views::View* GetArcProviderView() {
    for (const auto& it : vpn_detailed_view_->provider_view_map_) {
      if (it.second->type == VpnType::kArc) {
        return it.first;
      }
    }
    return nullptr;
  }

  std::unique_ptr<views::Widget> widget_;
  std::unique_ptr<FakeDetailedViewDelegate> delegate_;
  raw_ptr<VpnDetailedView, DanglingUntriaged> vpn_detailed_view_ = nullptr;
};

TEST_F(VpnDetailedViewTest, Basics) {
  // By default there is 1 provider (for built-in OpenVPN) and no networks.
  EXPECT_EQ(GetProviderViewCount(), 1u);
  EXPECT_EQ(GetNetworkViewCount(), 0u);

  AddVpnProvidersAndNetwork();

  EXPECT_EQ(GetProviderViewCount(), 3u);
  EXPECT_EQ(GetNetworkViewCount(), 1u);
}

TEST_F(VpnDetailedViewTest, ParentContainerConfiguration) {
  AddVpnProvidersAndNetwork();
  for (const views::View* view : GetProviderViews()) {
    const views::View* parent = view->parent();
    EXPECT_STREQ(parent->GetClassName(), "RoundedContainer");
  }
  for (const views::View* view : GetNetworkViews()) {
    const views::View* parent = view->parent();
    EXPECT_STREQ(parent->GetClassName(), "RoundedContainer");
  }
}

TEST_F(VpnDetailedViewTest, ClickOnBuiltInProviderRowToAddNetwork) {
  AddVpnProvidersAndNetwork();

  const views::View* built_in_provider = GetBuiltInProviderView();
  ASSERT_TRUE(built_in_provider);
  EXPECT_TRUE(built_in_provider->GetEnabled());

  const views::View* extension_provider = GetExtensionProviderView();
  ASSERT_TRUE(extension_provider);
  EXPECT_TRUE(extension_provider->GetEnabled());

  const views::View* arc_provider = GetArcProviderView();
  ASSERT_TRUE(arc_provider);
  EXPECT_TRUE(arc_provider->GetEnabled());

  // Clicking on the built-in provider row creates a built-in VPN network.
  LeftClickOn(built_in_provider);
  TestSystemTrayClient* client = GetSystemTrayClient();
  EXPECT_EQ(client->show_network_create_count(), 1);
  EXPECT_EQ(client->last_network_type(), ::onc::network_type::kVPN);

  // Clicking on the extension provider row creates a third-party VPN network.
  LeftClickOn(extension_provider);
  EXPECT_EQ(client->show_third_party_vpn_create_count(), 1);
  EXPECT_EQ(client->last_third_party_vpn_extension_id(),
            kExtensionProviderAppId);

  // Clicking on the ARC provider row creates an ARC VPN network.
  LeftClickOn(arc_provider);
  EXPECT_EQ(client->show_arc_vpn_create_count(), 1);
  EXPECT_EQ(client->last_arc_vpn_app_id(), kArcProviderAppId);
}

}  // namespace ash
