// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/platform_experience/prefs.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/platform_experience/features.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace platform_experience::prefs {
namespace {

class PlatformExperiencePrefsTest : public testing::Test {
 protected:
  PlatformExperiencePrefsTest() = default;
  ~PlatformExperiencePrefsTest() override = default;

  void SetUp() override {
    local_state_ = std::make_unique<ScopedTestingLocalState>(
        TestingBrowserProcess::GetGlobal());
  }

  PrefService& local_state() { return *local_state_->Get(); }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<ScopedTestingLocalState> local_state_;
};

// Test to ensure that the preference is registered correctly with its default
// value.
TEST_F(PlatformExperiencePrefsTest, RegisterPrefs) {
  EXPECT_NE(nullptr,
            local_state().FindPreference(kDisablePEHNotificationsPrefName));
  EXPECT_FALSE(local_state().GetBoolean(kDisablePEHNotificationsPrefName));
  EXPECT_FALSE(
      local_state().GetBoolean(kShouldUsePEHNotificationTextIndexPrefName));
  EXPECT_EQ(0, local_state().GetInteger(kPEHNotificationTextIndexPrefName));
}

// Test when the kDisableNotifications feature is disabled.
TEST_F(PlatformExperiencePrefsTest, SetPrefOverrides_FeatureDisabled) {
  // Ensure the feature is disabled (default state or explicitly disable).
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({},
                                       {features::kDisablePEHNotifications});

  SetPrefOverrides(local_state());
  EXPECT_FALSE(local_state().GetBoolean(kDisablePEHNotificationsPrefName));
}

// Test when the kDisableNotifications feature is enabled.
TEST_F(PlatformExperiencePrefsTest, SetPrefOverrides_FeatureEnabled) {
  // Enable the feature.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({features::kDisablePEHNotifications},
                                       {});

  SetPrefOverrides(local_state());
  EXPECT_TRUE(local_state().GetBoolean(kDisablePEHNotificationsPrefName));
}

// Tests that low-engagement feature prefs are not set when the
// "ShouldUseSpecificPEHNotificationText" flag is disabled.
TEST_F(PlatformExperiencePrefsTest,
       SetPrefOverrides_LowEngagementFeaturesDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeaturesAndParameters(
      {{features::kShouldUseSpecificPEHNotificationText,
        {{features::kUseNotificationTextIndex.name, "42"}}}},
      {});

  SetPrefOverrides(local_state());
  EXPECT_FALSE(
      local_state().GetBoolean(kShouldUsePEHNotificationTextIndexPrefName));
}

// Tests that PEH notification text is set when low-engagement PEH features
// are enabled.
TEST_F(PlatformExperiencePrefsTest, SetPrefOverrides_PEHNotificationText) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeaturesAndParameters(
      {{features::kLoadLowEngagementPEHFeaturesToPrefs, {}},
       {features::kShouldUseSpecificPEHNotificationText,
        {{features::kUseNotificationTextIndex.name, "42"}}}},
      {});

  SetPrefOverrides(local_state());
  EXPECT_TRUE(
      local_state().GetBoolean(kShouldUsePEHNotificationTextIndexPrefName));
  EXPECT_EQ(42, local_state().GetInteger(kPEHNotificationTextIndexPrefName));
}

}  // namespace
}  // namespace platform_experience::prefs
