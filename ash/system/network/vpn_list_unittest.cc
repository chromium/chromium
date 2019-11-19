// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/vpn_list.h"

#include <algorithm>
#include <vector>

#include "ash/system/network/tray_network_state_model.h"
#include "ash/test/ash_test_base.h"
#include "base/macros.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

using chromeos::network_config::mojom::VpnProvider;
using chromeos::network_config::mojom::VpnProviderPtr;
using chromeos::network_config::mojom::VpnType;

namespace ash {

namespace {

class TestVpnListObserver : public VpnList::Observer {
 public:
  TestVpnListObserver() = default;
  ~TestVpnListObserver() override = default;

  // VpnList::Observer:
  void OnVpnProvidersChanged() override { change_count_++; }

  int change_count_ = 0;
};

std::vector<VpnProviderPtr> CopyProviders(
    const std::vector<VpnProviderPtr>& providers) {
  std::vector<VpnProviderPtr> result;
  for (const VpnProviderPtr& provider : providers)
    result.push_back(provider.Clone());
  return result;
}

}  // namespace

class VpnListTest : public AshTestBase {
 public:
  VpnListTest() = default;
  ~VpnListTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    network_state_model_ = std::make_unique<TrayNetworkStateModel>();
  }

  void TearDown() override {
    network_state_model_.reset();
    AshTestBase::TearDown();
  }

  VpnList& GetVpnList() { return *network_state_model_->vpn_list(); }

 private:
  std::unique_ptr<TrayNetworkStateModel> network_state_model_;
  std::unique_ptr<VpnList> vpn_list_;

  DISALLOW_COPY_AND_ASSIGN(VpnListTest);
};

TEST_F(VpnListTest, BuiltInProvider) {
  VpnList& vpn_list = GetVpnList();

  // The VPN list should only contain the built-in provider.
  ASSERT_EQ(1u, vpn_list.extension_vpn_providers().size());
  const VpnProviderPtr& provider = vpn_list.extension_vpn_providers()[0];
  EXPECT_EQ(provider->type, VpnType::kOpenVPN);
  EXPECT_TRUE(provider->app_id.empty());
}

TEST_F(VpnListTest, ThirdPartyProviders) {
  VpnList& vpn_list = GetVpnList();
  // The VpnList model doesn't sort by launch time or otherwise do anything
  // with the value, so we use the same value for all instances and di a single
  // verification that it gets set.
  base::Time launch_time = base::Time::Now();

  // The VPN list should only contain the built-in provider.
  EXPECT_EQ(1u, vpn_list.extension_vpn_providers().size());

  // Add some third party providers.
  VpnProviderPtr extension_provider1 =
      VpnProvider::New(VpnType::kExtension, "extension_id1", "name1",
                       "extension_id1", launch_time);
  VpnProviderPtr extension_provider2 =
      VpnProvider::New(VpnType::kExtension, "extension_id2", "name2",
                       "extension_id2", launch_time);
  VpnProviderPtr arc_provider1 =
      VpnProvider::New(VpnType::kArc, "package.name.foo1", "ArcVPNMonster1",
                       "arc_app_id1", launch_time);

  std::vector<VpnProviderPtr> third_party_providers;
  third_party_providers.push_back(extension_provider1->Clone());
  third_party_providers.push_back(extension_provider2->Clone());
  third_party_providers.push_back(arc_provider1->Clone());
  vpn_list.SetVpnProvidersForTest(CopyProviders(third_party_providers));

  // Extension list contains the builtin provider and extension-backed
  // providers.
  {
    const std::vector<VpnProviderPtr>& extension_providers =
        vpn_list.extension_vpn_providers();
    ASSERT_EQ(3u, extension_providers.size());
    EXPECT_EQ(VpnType::kOpenVPN, extension_providers[0]->type);
    EXPECT_TRUE(extension_providers[1]->Equals(*extension_provider1));
    EXPECT_TRUE(extension_providers[2]->Equals(*extension_provider2));
  }

  // Arc list contains the Arc providers.
  {
    const std::vector<VpnProviderPtr>& arc_providers =
        vpn_list.arc_vpn_providers();
    EXPECT_EQ(1u, arc_providers.size());
    EXPECT_TRUE(arc_providers[0]->Equals(*arc_provider1));
    EXPECT_EQ(launch_time, arc_providers[0]->last_launch_time);
  }

  // A second Arc VPN gets installed.
  VpnProviderPtr arc_provider2 =
      VpnProvider::New(VpnType::kArc, "package.name.foo2", "ArcVPNMonster2",
                       "arc_app_id2", launch_time);
  third_party_providers.push_back(arc_provider2->Clone());
  vpn_list.SetVpnProvidersForTest(CopyProviders(third_party_providers));
  {
    const std::vector<VpnProviderPtr>& arc_providers =
        vpn_list.arc_vpn_providers();
    EXPECT_EQ(2u, arc_providers.size());
    EXPECT_TRUE(arc_providers[0]->Equals(*arc_provider1));
    EXPECT_TRUE(arc_providers[1]->Equals(*arc_provider2));
  }

  // The first Arc VPN gets uninstalled.
  auto iter = std::find_if(
      third_party_providers.begin(), third_party_providers.end(),
      [](const auto& p) { return p->provider_id == "package.name.foo1"; });
  ASSERT_NE(iter, third_party_providers.end());
  third_party_providers.erase(iter);
  vpn_list.SetVpnProvidersForTest(CopyProviders(third_party_providers));
  {
    const std::vector<VpnProviderPtr>& arc_providers =
        vpn_list.arc_vpn_providers();
    EXPECT_EQ(1u, arc_providers.size());
    ASSERT_TRUE(arc_providers[0]);
    ASSERT_TRUE(arc_provider2);
    EXPECT_TRUE(arc_providers[0]->Equals(*arc_provider2));
  }

  // package.name.foo2 changes due to update or system language change.
  arc_provider2->provider_name = "ArcVPNMonster2Rename";
  arc_provider2->app_id = "arc_app_id2_rename";
  third_party_providers[2] = arc_provider2->Clone();
  vpn_list.SetVpnProvidersForTest(CopyProviders(third_party_providers));
  {
    const std::vector<VpnProviderPtr>& arc_providers =
        vpn_list.arc_vpn_providers();
    EXPECT_TRUE(arc_providers[0]->Equals(*arc_provider2));
  }
}

TEST_F(VpnListTest, Observers) {
  VpnList& vpn_list = GetVpnList();

  // Observers are not notified when they are added.
  TestVpnListObserver observer;
  vpn_list.AddObserver(&observer);
  EXPECT_EQ(0, observer.change_count_);

  // Add a third party (extension-backed) provider.
  std::vector<VpnProviderPtr> third_party_providers;
  VpnProviderPtr third_party1 = VpnProvider::New();
  third_party1->type = VpnType::kExtension;
  third_party1->provider_name = "name1";
  third_party1->provider_id = "extension_id1";
  third_party_providers.push_back(std::move(third_party1));
  vpn_list.SetVpnProvidersForTest(std::move(third_party_providers));

  // Observer was notified.
  EXPECT_EQ(1, observer.change_count_);

  vpn_list.RemoveObserver(&observer);
}

}  // namespace ash
