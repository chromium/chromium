// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/apps/boca_web_app_info.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/webui/boca_ui/url_constants.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/browser_context_helper/fake_browser_context_helper_delegate.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"
#include "ui/menus/simple_menu_model.h"

namespace {

constexpr char kProfile[] = "Default";
constexpr char kAffiliationId1[] = "affiliation-id-1";

class BocaSystemAppDelegateTest : public testing::Test {
 protected:
  BocaSystemAppDelegateTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile(kProfile);
    delegate_ = std::make_unique<BocaSystemAppDelegate>(profile_);
  }
  const BocaSystemAppDelegate* delegate() const { return delegate_.get(); }
  base::test::ScopedFeatureList* scoped_feature_list() {
    return &scoped_feature_list_;
  }
  TestingProfile* profile() { return profile_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
  std::unique_ptr<BocaSystemAppDelegate> delegate_;
  base::test::ScopedFeatureList scoped_feature_list_;
  raw_ptr<TestingProfile> profile_;
};

TEST_F(BocaSystemAppDelegateTest, AppShowFromSearchAndShelfByDefault) {
  EXPECT_TRUE(delegate()->ShouldShowInSearchAndShelf());
  EXPECT_FALSE(delegate()->IsAppEnabled());
}

TEST_F(BocaSystemAppDelegateTest, AppShowFromLauncherByDefault) {
  EXPECT_TRUE(delegate()->ShouldShowInLauncher());
  EXPECT_FALSE(delegate()->IsAppEnabled());
}

TEST_F(BocaSystemAppDelegateTest, AppDisabledByKillSwitch) {
  scoped_feature_list()->InitAndDisableFeature(ash::features::kBocaUber);
  EXPECT_FALSE(delegate()->IsAppEnabled());
}

TEST_F(BocaSystemAppDelegateTest, AppEnabledWhenFeatureEnabled) {
  scoped_feature_list()->InitAndEnableFeature(ash::features::kBoca);
  EXPECT_TRUE(delegate()->IsAppEnabled());
}

TEST_F(BocaSystemAppDelegateTest, AppDisabledForUnAffliatedUser) {
  EXPECT_FALSE(delegate()->IsAppEnabled());
}

TEST_F(BocaSystemAppDelegateTest, AppDisabledFromPref) {
  profile()->GetProfilePolicyConnector()->SetUserAffiliationIdsForTesting(
      {kAffiliationId1});
  g_browser_process->browser_policy_connector()
      ->SetDeviceAffiliatedIdsForTesting({kAffiliationId1});
  profile()->GetTestingPrefService()->SetUserPref(
      ash::prefs::kClassManagementToolsAvailabilitySetting,
      base::Value("disabled"));
  EXPECT_FALSE(delegate()->IsAppEnabled());
}

TEST_F(BocaSystemAppDelegateTest, BocaDisabledByDefaultFromPref) {
  profile()->GetProfilePolicyConnector()->SetUserAffiliationIdsForTesting(
      {kAffiliationId1});
  g_browser_process->browser_policy_connector()
      ->SetDeviceAffiliatedIdsForTesting({kAffiliationId1});
  profile()->GetTestingPrefService()->SetUserPref(
      ash::prefs::kClassManagementToolsAvailabilitySetting, base::Value(""));
  EXPECT_FALSE(delegate()->IsAppEnabled());
}

TEST_F(BocaSystemAppDelegateTest, AppEnabledFromPref) {
  profile()->GetProfilePolicyConnector()->SetUserAffiliationIdsForTesting(
      {kAffiliationId1});
  g_browser_process->browser_policy_connector()
      ->SetDeviceAffiliatedIdsForTesting({kAffiliationId1});
  profile()->GetTestingPrefService()->SetUserPref(
      ash::prefs::kClassManagementToolsAvailabilitySetting,
      base::Value("teacher"));
  EXPECT_TRUE(delegate()->IsAppEnabled());
}

TEST_F(BocaSystemAppDelegateTest, OverrideURLScopeChecks) {
  profile()->GetProfilePolicyConnector()->SetUserAffiliationIdsForTesting(
      {kAffiliationId1});
  g_browser_process->browser_policy_connector()
      ->SetDeviceAffiliatedIdsForTesting({kAffiliationId1});
  EXPECT_TRUE(delegate()->IsUrlInSystemAppScope(GURL()));
}

class BocaSystemAppProviderDelegateTest : public BocaSystemAppDelegateTest {
 public:
  BocaSystemAppProviderDelegateTest() {
    scoped_feature_list()->InitWithFeatures(
        /*enabled_features=*/{ash::features::kBoca},
        /*disabled_features=*/{ash::features::kBocaConsumer});
  }
};

TEST_F(BocaSystemAppProviderDelegateTest, MissingTabStrip) {
  EXPECT_FALSE(delegate()->ShouldHaveTabStrip());
}

TEST_F(BocaSystemAppProviderDelegateTest, DoNotOverrideURLScopeChecks) {
  EXPECT_FALSE(delegate()->IsUrlInSystemAppScope(GURL()));
}

TEST_F(BocaSystemAppProviderDelegateTest, AllowResize) {
  EXPECT_TRUE(delegate()->ShouldAllowResize());
}

TEST_F(BocaSystemAppProviderDelegateTest, AllowMaximize) {
  EXPECT_TRUE(delegate()->ShouldAllowMaximize());
}

TEST_F(BocaSystemAppProviderDelegateTest, HasMinimalSize) {
  EXPECT_EQ(gfx::Size(400, 400), delegate()->GetMinimumWindowSize());
}

TEST_F(BocaSystemAppProviderDelegateTest, UsesDefaultTabMenuModel) {
  EXPECT_FALSE(delegate()->HasCustomTabMenuModel());
}

class BocaSystemAppConsumerDelegateTest : public BocaSystemAppDelegateTest {
 public:
  BocaSystemAppConsumerDelegateTest() {
    scoped_feature_list()->InitWithFeatures(
        /*enabled_features=*/{ash::features::kBoca,
                              ash::features::kBocaConsumer},
        /*disabled_features=*/{});
  }
};

TEST_F(BocaSystemAppConsumerDelegateTest, ShouldHaveTabStrip) {
  EXPECT_TRUE(delegate()->ShouldHaveTabStrip());
}

TEST_F(BocaSystemAppConsumerDelegateTest, OverrideURLScopeChecks) {
  EXPECT_TRUE(delegate()->IsUrlInSystemAppScope(GURL()));
}

TEST_F(BocaSystemAppConsumerDelegateTest, DisallowResize) {
  EXPECT_FALSE(delegate()->ShouldAllowResize());
}

TEST_F(BocaSystemAppConsumerDelegateTest, DisallowMaximize) {
  EXPECT_FALSE(delegate()->ShouldAllowMaximize());
}

TEST_F(BocaSystemAppConsumerDelegateTest, PinHomeTab) {
  EXPECT_TRUE(delegate()->ShouldPinTab(
      GURL(ash::boca::kChromeBocaAppUntrustedIndexURL)));
}

TEST_F(BocaSystemAppConsumerDelegateTest, HideNewTabButton) {
  EXPECT_TRUE(delegate()->ShouldHideNewTabButton());
}

TEST_F(BocaSystemAppConsumerDelegateTest, HasMinimalSize) {
  EXPECT_EQ(gfx::Size(500, 500), delegate()->GetMinimumWindowSize());
}

TEST_F(BocaSystemAppConsumerDelegateTest, UsesCustomTabMenuModel) {
  ASSERT_TRUE(delegate()->HasCustomTabMenuModel());

  const std::unique_ptr<ui::SimpleMenuModel> tab_menu =
      delegate()->GetTabMenuModel(nullptr);
  ASSERT_EQ(2u, tab_menu->GetItemCount());
  EXPECT_EQ(TabStripModel::CommandReload, tab_menu->GetCommandIdAt(0));
  EXPECT_EQ(TabStripModel::CommandGoBack, tab_menu->GetCommandIdAt(1));
}

}  // namespace
