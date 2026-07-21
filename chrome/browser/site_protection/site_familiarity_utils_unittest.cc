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

struct JsOptimizerTestParam {
  std::string test_name;
  std::vector<base::test::FeatureRef> enabled_features;
  std::vector<base::test::FeatureRef> disabled_features;
  safe_browsing::SafeBrowsingState sb_state;
  content_settings::JavascriptOptimizerSetting expected_default_setting;
};

class SiteFamiliarityUtilsJsOptimizerParameterizedTest
    : public SiteFamiliarityUtilsJsOptimizerTest,
      public ::testing::WithParamInterface<JsOptimizerTestParam> {
 public:
  SiteFamiliarityUtilsJsOptimizerParameterizedTest()
      : SiteFamiliarityUtilsJsOptimizerTest(GetParam().enabled_features,
                                            GetParam().disabled_features) {}

  void SetUp() override {
    SiteFamiliarityUtilsJsOptimizerTest::SetUp();
    safe_browsing::SetSafeBrowsingState(profile()->GetPrefs(),
                                        GetParam().sb_state);
  }
};

TEST_P(SiteFamiliarityUtilsJsOptimizerParameterizedTest, DefaultBehavior) {
  ExpectJsOptimizerSetting(GetParam().expected_default_setting);
}

TEST_P(SiteFamiliarityUtilsJsOptimizerParameterizedTest, ManualOverride) {
  SetJsOptimizerSetting(content_settings::JavascriptOptimizerSetting::kAllowed);
  ExpectJsOptimizerSetting(
      content_settings::JavascriptOptimizerSetting::kAllowed);
}

TEST_P(SiteFamiliarityUtilsJsOptimizerParameterizedTest,
       BlockedUserRemainsBlocked) {
  SetJsOptimizerSetting(content_settings::JavascriptOptimizerSetting::kBlocked);
  ExpectJsOptimizerSetting(
      content_settings::JavascriptOptimizerSetting::kBlocked);
}

TEST_P(SiteFamiliarityUtilsJsOptimizerParameterizedTest,
       ReturnsAllowedIfSafeBrowsingDisabled) {
  safe_browsing::SetSafeBrowsingState(
      profile()->GetPrefs(),
      safe_browsing::SafeBrowsingState::NO_SAFE_BROWSING);
  ExpectJsOptimizerSetting(
      content_settings::JavascriptOptimizerSetting::kAllowed);
}

TEST_P(SiteFamiliarityUtilsJsOptimizerParameterizedTest,
       ReturnsAllowedIfProcessSelectionFlagDisabled) {
  base::test::ScopedFeatureList local_features;
  local_features.InitAndDisableFeature(
      ::features::kProcessSelectionDeferringConditions);
  ExpectJsOptimizerSetting(
      content_settings::JavascriptOptimizerSetting::kAllowed);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    SiteFamiliarityUtilsJsOptimizerParameterizedTest,
    ::testing::Values(
        // Scenario 1: General Migration Feature Enabled -> Blocked for all SB
        // users
        JsOptimizerTestParam{
            .test_name = "MigrationEnabled",
            .enabled_features =
                {safe_browsing::kMigrateToBlockV8OptimizerOnUnfamiliarSites},
            .sb_state = safe_browsing::SafeBrowsingState::STANDARD_PROTECTION,
            .expected_default_setting = content_settings::
                JavascriptOptimizerSetting::kBlockedForUnfamiliarSites,
        },
        // Scenario 2: ESB Feature Enabled + ESB Active -> Blocked
        JsOptimizerTestParam{
            .test_name = "EsbFeatureEnabled_EsbUser",
            .enabled_features =
                {safe_browsing::
                     kEnableBlockV8OptimizerOnUnfamiliarSitesForEsbClients},
            .sb_state = safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION,
            .expected_default_setting = content_settings::
                JavascriptOptimizerSetting::kBlockedForUnfamiliarSites,
        },
        // Scenario 3: ESB Feature Enabled + Standard SB -> Allowed
        JsOptimizerTestParam{
            .test_name = "EsbFeatureEnabled_StandardUser",
            .enabled_features =
                {safe_browsing::
                     kEnableBlockV8OptimizerOnUnfamiliarSitesForEsbClients},
            .sb_state = safe_browsing::SafeBrowsingState::STANDARD_PROTECTION,
            .expected_default_setting =
                content_settings::JavascriptOptimizerSetting::kAllowed,
        },
        // Scenario 4: Both Features Disabled -> Allowed
        JsOptimizerTestParam{
            .test_name = "BothFeaturesDisabled",
            .disabled_features =
                {safe_browsing::kMigrateToBlockV8OptimizerOnUnfamiliarSites,
                 safe_browsing::
                     kEnableBlockV8OptimizerOnUnfamiliarSitesForEsbClients},
            .sb_state = safe_browsing::SafeBrowsingState::STANDARD_PROTECTION,
            .expected_default_setting =
                content_settings::JavascriptOptimizerSetting::kAllowed,
        }),
    [](const ::testing::TestParamInfo<JsOptimizerTestParam>& info) {
      return info.param.test_name;
    });

TEST_F(SiteFamiliarityUtilsJsOptimizerTest, IsV8OptimizerBlockingDryRun) {
  // 1. Migration disabled, dry run disabled (default).
  EXPECT_FALSE(IsV8OptimizerBlockingDryRun(profile()));

  // 2. Migration enabled, dry run disabled (default).
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(
        safe_browsing::kMigrateToBlockV8OptimizerOnUnfamiliarSites);
    EXPECT_FALSE(IsV8OptimizerBlockingDryRun(profile()));
  }

  // 3. Migration enabled, dry run enabled.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeatureWithParameters(
        safe_browsing::kMigrateToBlockV8OptimizerOnUnfamiliarSites,
        {{safe_browsing::kMigrateToBlockV8OptimizerOnUnfamiliarSitesDryRun.name,
          "true"}});

    // Default user (no pref) -> should be dry run.
    EXPECT_TRUE(IsV8OptimizerBlockingDryRun(profile()));

    // User opted in via pref -> should NOT be dry run.
    SetJsOptimizerSetting(content_settings::JavascriptOptimizerSetting::
                              kBlockedForUnfamiliarSites);
    EXPECT_FALSE(IsV8OptimizerBlockingDryRun(profile()));

    // User opted out via pref -> should NOT be dry run.
    SetJsOptimizerSetting(
        content_settings::JavascriptOptimizerSetting::kAllowed);
    EXPECT_FALSE(IsV8OptimizerBlockingDryRun(profile()));

    // Clear pref for next tests
    profile()->GetPrefs()->ClearPref(
        prefs::kJavascriptOptimizerBlockedForUnfamiliarSites);
  }

  // 4. ESB feature enabled, dry run disabled (default).
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(
        safe_browsing::kEnableBlockV8OptimizerOnUnfamiliarSitesForEsbClients);
    safe_browsing::SetSafeBrowsingState(
        profile()->GetPrefs(),
        safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION);
    EXPECT_FALSE(IsV8OptimizerBlockingDryRun(profile()));
  }

  // 5. ESB feature enabled, dry run enabled.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeatureWithParameters(
        safe_browsing::kEnableBlockV8OptimizerOnUnfamiliarSitesForEsbClients,
        {{safe_browsing::kEsbDryRun.name, "true"}});
    safe_browsing::SetSafeBrowsingState(
        profile()->GetPrefs(),
        safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION);

    // Default user (no pref) -> should be dry run.
    EXPECT_TRUE(IsV8OptimizerBlockingDryRun(profile()));

    // User opted in via pref -> should NOT be dry run.
    SetJsOptimizerSetting(content_settings::JavascriptOptimizerSetting::
                              kBlockedForUnfamiliarSites);
    EXPECT_FALSE(IsV8OptimizerBlockingDryRun(profile()));

    // User opted out via pref -> should NOT be dry run.
    SetJsOptimizerSetting(
        content_settings::JavascriptOptimizerSetting::kAllowed);
    EXPECT_FALSE(IsV8OptimizerBlockingDryRun(profile()));
  }
}

}  // namespace site_protection
