// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/vpn_list_view.h"

#include "ash/constants/ash_features.h"
#include "ash/system/network/tray_network_state_model.h"
#include "ash/system/tray/fake_detailed_view_delegate.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "chromeos/services/network_config/public/mojom/network_types.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

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

constexpr char kVpnProviderId[] = "provider_id";
constexpr char kVpnProviderName[] = "provider_name";

// Tests are parameterized by QsRevamp.
class VPNListViewTest : public AshTestBase,
                        public testing::WithParamInterface<bool> {
 public:
  VPNListViewTest() {
    if (IsQsRevampEnabled()) {
      feature_list_.InitWithFeatures(
          {features::kQsRevamp, features::kQsRevampWip}, {});
    } else {
      feature_list_.InitWithFeatures(
          {}, {features::kQsRevamp, features::kQsRevampWip});
    }
  }

  bool IsQsRevampEnabled() { return GetParam(); }

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    delegate_ = std::make_unique<FakeDetailedViewDelegate>();
    vpn_list_view_ =
        std::make_unique<VPNListView>(delegate_.get(), LoginStatus::USER);
    vpn_list_view_->Init();
    vpn_list_view_->OnGetNetworkStateList({});
  }

  void TearDown() override {
    vpn_list_view_.reset();
    delegate_.reset();
    AshTestBase::TearDown();
  }

  void AddVpnProviderAndNetwork() {
    std::vector<VpnProviderPtr> providers;
    VpnProviderPtr provider = VpnProvider::New();
    provider->type = VpnType::kExtension;
    provider->provider_name = kVpnProviderName;
    provider->provider_id = kVpnProviderId;
    providers.push_back(std::move(provider));
    vpn_list_view_->model()->vpn_list()->SetVpnProvidersForTest(
        std::move(providers));

    NetworkStatePropertiesPtr network = NetworkStateProperties::New();
    network->guid = "vpn_id";
    network->name = "vpn_name";
    network->type = NetworkType::kVPN;
    network->connection_state = ConnectionStateType::kNotConnected;
    VPNStatePropertiesPtr vpn = VPNStateProperties::New();
    vpn->type = VpnType::kExtension;
    vpn->provider_name = kVpnProviderName;
    vpn->provider_id = kVpnProviderId;
    network->type_state = NetworkTypeStateProperties::NewVpn(std::move(vpn));
    std::vector<NetworkStatePropertiesPtr> networks;
    networks.push_back(std::move(network));
    vpn_list_view_->OnGetNetworkStateList(std::move(networks));
  }

  size_t GetProviderViewCount() {
    return vpn_list_view_->provider_view_map_.size();
  }

  size_t GetNetworkViewCount() {
    return vpn_list_view_->network_view_guid_map_.size();
  }

  std::vector<const views::View*> GetProviderViews() {
    std::vector<const views::View*> views;
    for (const auto& it : vpn_list_view_->provider_view_map_)
      views.push_back(it.first);
    return views;
  }

  std::vector<const views::View*> GetNetworkViews() {
    std::vector<const views::View*> views;
    for (const auto& it : vpn_list_view_->network_view_guid_map_)
      views.push_back(it.first);
    return views;
  }

  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<FakeDetailedViewDelegate> delegate_;
  std::unique_ptr<VPNListView> vpn_list_view_;
};

INSTANTIATE_TEST_SUITE_P(QsRevamp, VPNListViewTest, testing::Bool());

TEST_P(VPNListViewTest, Basics) {
  // By default there is 1 provider (for built-in OpenVPN) and no networks.
  EXPECT_EQ(GetProviderViewCount(), 1u);
  EXPECT_EQ(GetNetworkViewCount(), 0u);

  AddVpnProviderAndNetwork();

  EXPECT_EQ(GetProviderViewCount(), 2u);
  EXPECT_EQ(GetNetworkViewCount(), 1u);
}

TEST_P(VPNListViewTest, ParentContainerConfiguration) {
  AddVpnProviderAndNetwork();
  for (const views::View* view : GetProviderViews()) {
    const views::View* parent = view->parent();
    if (IsQsRevampEnabled())
      EXPECT_STREQ(parent->GetClassName(), "RoundedContainer");
    else
      EXPECT_STREQ(parent->GetClassName(), "ScrollContentsView");
  }
  for (const views::View* view : GetNetworkViews()) {
    const views::View* parent = view->parent();
    if (IsQsRevampEnabled())
      EXPECT_STREQ(parent->GetClassName(), "RoundedContainer");
    else
      EXPECT_STREQ(parent->GetClassName(), "ScrollContentsView");
  }
}

}  // namespace ash
