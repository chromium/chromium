// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/apps/os_settings_web_app_info.h"

#include <vector>

#include "ash/constants/ash_features.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"

// Base class for tests of `OSSettingsSystemAppDelegate` parameterized by:
// * whether theme changes should be animated for the Settings app
class OSSettingsSystemAppDelegateTest
    : public testing::Test,
      public testing::WithParamInterface<bool> {
 public:
  OSSettingsSystemAppDelegateTest() : delegate_(/*profile=*/nullptr) {
    std::vector<base::test::FeatureRef> enabled;
    std::vector<base::test::FeatureRef> disabled;

    // Whether theme changes should be animated for the Settings app given test
    // parameterization.
    (GetParam() ? enabled : disabled)
        .push_back(ash::features::kSettingsAppThemeChangeAnimation);

    scoped_feature_list_.InitWithFeatures(enabled, disabled);
  }

  // Returns the `delegate_` under test.
  OSSettingsSystemAppDelegate* delegate() { return &delegate_; }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  OSSettingsSystemAppDelegate delegate_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    OSSettingsSystemAppDelegateTest,
    /*settings_app_theme_change_animation_enabled=*/testing::Bool());

// Verifies that `OSSettingsSystemAppDelegate::ShouldAnimateThemeChanges()`
// returns true iff the necessary feature flags are enabled.
TEST_P(OSSettingsSystemAppDelegateTest, ShouldAnimateThemeChanges) {
  EXPECT_EQ(delegate()->ShouldAnimateThemeChanges(), GetParam());
}
