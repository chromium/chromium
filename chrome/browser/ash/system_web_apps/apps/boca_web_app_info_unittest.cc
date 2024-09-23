// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/apps/boca_web_app_info.h"

#include "ash/constants/ash_features.h"
#include "ash/webui/boca_ui/url_constants.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/models/simple_menu_model.h"

namespace {

class BocaSystemAppDelegateTest : public ::testing::Test {
 protected:
  BocaSystemAppDelegateTest() : delegate_(/*profile=*/nullptr) {}

  const BocaSystemAppDelegate* delegate() const { return &delegate_; }
  base::test::ScopedFeatureList* scoped_feature_list() {
    return &scoped_feature_list_;
  }

 private:
  const BocaSystemAppDelegate delegate_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(BocaSystemAppDelegateTest, AppDisabledByDefault) {
  EXPECT_FALSE(delegate()->IsAppEnabled());
}

TEST_F(BocaSystemAppDelegateTest, AppDisabledWhenFeatureDisabled) {
  scoped_feature_list()->InitAndDisableFeature(ash::features::kBoca);
  EXPECT_FALSE(delegate()->IsAppEnabled());
}

TEST_F(BocaSystemAppDelegateTest, AppEnabledWhenFeatureEnabled) {
  scoped_feature_list()->InitAndEnableFeature(ash::features::kBoca);
  EXPECT_TRUE(delegate()->IsAppEnabled());
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

TEST_F(BocaSystemAppConsumerDelegateTest, UsesCustomTabMenuModel) {
  ASSERT_TRUE(delegate()->HasCustomTabMenuModel());

  const std::unique_ptr<ui::SimpleMenuModel> tab_menu =
      delegate()->GetTabMenuModel(nullptr);
  ASSERT_EQ(2u, tab_menu->GetItemCount());
  EXPECT_EQ(TabStripModel::CommandReload, tab_menu->GetCommandIdAt(0));
  EXPECT_EQ(TabStripModel::CommandGoBack, tab_menu->GetCommandIdAt(1));
}

}  // namespace
