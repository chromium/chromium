// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/site_protection/site_familiarity_utils.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/content_settings/generated_javascript_optimizer_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace site_protection {

class SiteFamiliarityUtilsJsOptimizerTest : public testing::Test {
 public:
  explicit SiteFamiliarityUtilsJsOptimizerTest(
      const std::vector<base::test::FeatureRef>& enabled_features = {},
      const std::vector<base::test::FeatureRef>& disabled_features = {}) {
    feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

  void SetUp() override {
    testing::Test::SetUp();
    profile_ = std::make_unique<TestingProfile>();
  }

  TestingProfile* profile() { return profile_.get(); }

  void SetJsOptimizerSetting(
      content_settings::JavascriptOptimizerSetting setting) {
    content_settings::GeneratedJavascriptOptimizerPref pref(profile());
    base::Value value(static_cast<int>(setting));
    pref.SetPref(&value);
  }

  void ExpectJsOptimizerSetting(
      content_settings::JavascriptOptimizerSetting expected_setting) {
    EXPECT_EQ(ComputeDefaultJavascriptOptimizerSetting(profile()),
              expected_setting);
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<TestingProfile> profile_;
};

class SiteFamiliarityUtilsJsOptimizerMigrationEnabledTest
    : public SiteFamiliarityUtilsJsOptimizerTest {
 public:
  SiteFamiliarityUtilsJsOptimizerMigrationEnabledTest()
      : SiteFamiliarityUtilsJsOptimizerTest(
            {safe_browsing::kMigrateToBlockV8OptimizerOnUnfamiliarSites}) {}
};

TEST_F(SiteFamiliarityUtilsJsOptimizerMigrationEnabledTest,
       DefaultUserReturnsBlockedForUnfamiliarSites) {
  ExpectJsOptimizerSetting(
      content_settings::JavascriptOptimizerSetting::kBlockedForUnfamiliarSites);
}

TEST_F(SiteFamiliarityUtilsJsOptimizerMigrationEnabledTest,
       ManualOverrideIgnoresMigration) {
  SetJsOptimizerSetting(content_settings::JavascriptOptimizerSetting::kAllowed);
  ExpectJsOptimizerSetting(
      content_settings::JavascriptOptimizerSetting::kAllowed);
}

TEST_F(SiteFamiliarityUtilsJsOptimizerMigrationEnabledTest,
       BlockedUserRemainsBlocked) {
  SetJsOptimizerSetting(content_settings::JavascriptOptimizerSetting::kBlocked);
  ExpectJsOptimizerSetting(
      content_settings::JavascriptOptimizerSetting::kBlocked);
}

TEST_F(SiteFamiliarityUtilsJsOptimizerMigrationEnabledTest,
       ReturnsAllowedIfSafeBrowsingDisabled) {
  safe_browsing::SetSafeBrowsingState(
      profile()->GetPrefs(),
      safe_browsing::SafeBrowsingState::NO_SAFE_BROWSING);
  ExpectJsOptimizerSetting(
      content_settings::JavascriptOptimizerSetting::kAllowed);
}


TEST_F(SiteFamiliarityUtilsJsOptimizerMigrationEnabledTest,
       ReturnsAllowedIfProcessSelectionFlagDisabled) {
  base::test::ScopedFeatureList local_features;
  local_features.InitAndDisableFeature(
      ::features::kProcessSelectionDeferringConditions);
  ExpectJsOptimizerSetting(
      content_settings::JavascriptOptimizerSetting::kAllowed);
}

TEST_F(SiteFamiliarityUtilsJsOptimizerTest, MigrationFeatureToggle) {
  {
    base::test::ScopedFeatureList migration_feature_list;
    migration_feature_list.InitAndEnableFeature(
        safe_browsing::kMigrateToBlockV8OptimizerOnUnfamiliarSites);
    ExpectJsOptimizerSetting(content_settings::JavascriptOptimizerSetting::
                                 kBlockedForUnfamiliarSites);
  }

  ExpectJsOptimizerSetting(
      content_settings::JavascriptOptimizerSetting::kAllowed);
}

class SiteFamiliarityUtilsJsOptimizerMigrationDisabledTest
    : public SiteFamiliarityUtilsJsOptimizerTest {
 public:
  SiteFamiliarityUtilsJsOptimizerMigrationDisabledTest()
      : SiteFamiliarityUtilsJsOptimizerTest(
            {},
            {safe_browsing::kMigrateToBlockV8OptimizerOnUnfamiliarSites}) {}
};

TEST_F(SiteFamiliarityUtilsJsOptimizerMigrationDisabledTest,
       DefaultUserReturnsAllowedIfFlagDisabled) {
  ExpectJsOptimizerSetting(
      content_settings::JavascriptOptimizerSetting::kAllowed);
}

}  // namespace site_protection
