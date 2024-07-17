// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/apps/boca_web_app_info.h"

#include "ash/constants/ash_features.h"
#include "ash/webui/boca_ui/url_constants.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class BocaSystemAppDelegateTest : public ::testing::Test {
 protected:
  BocaSystemAppDelegateTest() : delegate_(/*profile=*/nullptr) {}

  const BocaSystemAppDelegate delegate_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(BocaSystemAppDelegateTest, AppDisabledByDefault) {
  EXPECT_FALSE(delegate_.IsAppEnabled());
}

TEST_F(BocaSystemAppDelegateTest, AppDisabledWhenFeatureDisabled) {
  scoped_feature_list_.InitAndDisableFeature(ash::features::kBoca);
  EXPECT_FALSE(delegate_.IsAppEnabled());
}

TEST_F(BocaSystemAppDelegateTest, AppEnabledWhenFeatureEnabled) {
  scoped_feature_list_.InitAndEnableFeature(ash::features::kBoca);
  EXPECT_TRUE(delegate_.IsAppEnabled());
}

TEST_F(BocaSystemAppDelegateTest, MissingTabStripForProviderUsers) {
  EXPECT_FALSE(delegate_.ShouldHaveTabStrip());
}

TEST_F(BocaSystemAppDelegateTest, AvailableTabStripForConsumerUsers) {
  scoped_feature_list_.InitAndEnableFeature(ash::features::kBocaConsumer);
  EXPECT_TRUE(delegate_.ShouldHaveTabStrip());
}

TEST_F(BocaSystemAppDelegateTest, DoNotOverrideURLScopeChecksForProviderUsers) {
  EXPECT_FALSE(delegate_.IsUrlInSystemAppScope(GURL()));
}

TEST_F(BocaSystemAppDelegateTest, OverrideURLScopeChecksForConsumerUsers) {
  scoped_feature_list_.InitAndEnableFeature(ash::features::kBocaConsumer);
  EXPECT_TRUE(delegate_.IsUrlInSystemAppScope(GURL()));
}

TEST_F(BocaSystemAppDelegateTest, AllowResizeForProviderUsers) {
  EXPECT_TRUE(delegate_.ShouldAllowResize());
}

TEST_F(BocaSystemAppDelegateTest, DisallowResizeForConsumerUsers) {
  scoped_feature_list_.InitAndEnableFeature(ash::features::kBocaConsumer);
  EXPECT_FALSE(delegate_.ShouldAllowResize());
}

TEST_F(BocaSystemAppDelegateTest, AllowMaximizeForProviderUsers) {
  EXPECT_TRUE(delegate_.ShouldAllowMaximize());
}

TEST_F(BocaSystemAppDelegateTest, DisallowMaximizeForConsumerUsers) {
  scoped_feature_list_.InitAndEnableFeature(ash::features::kBocaConsumer);
  EXPECT_FALSE(delegate_.ShouldAllowMaximize());
}

TEST_F(BocaSystemAppDelegateTest, PinHomeTabForConsumerUsers) {
  scoped_feature_list_.InitAndEnableFeature(ash::features::kBocaConsumer);
  EXPECT_TRUE(
      delegate_.ShouldPinTab(GURL(ash::kChromeBocaAppUntrustedIndexURL)));
}

}  // namespace
