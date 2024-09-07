// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/ntp_custom_background_enabled_policy_handler.h"

#include "base/run_loop.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_syncable_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

// Tests whether setting the |policy::key::kNTPCustomBackgroundEnabled| policy
// to false clears the |prefs::kNtpCustomBackgroundDict| dictionary pref.
class NtpCustomBackgroundEnabledPolicyHandlerTest
    : public InProcessBrowserTest {
 public:
  void SetUp() override {
    policy_provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);
    InProcessBrowserTest::SetUp();
  }

 protected:
  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;
};

IN_PROC_BROWSER_TEST_F(NtpCustomBackgroundEnabledPolicyHandlerTest, Override) {
  PrefService* prefs = browser()->profile()->GetPrefs();

  // Check initial states.
  EXPECT_FALSE(prefs
                   ->GetDict(GetThemePrefNameInMigration(
                       ThemePrefInMigration::kNtpCustomBackgroundDict))
                   .empty());
  EXPECT_FALSE(prefs->IsManagedPreference(GetThemePrefNameInMigration(
      ThemePrefInMigration::kNtpCustomBackgroundDict)));

  // Check if updated policy is reflected.
  policy::PolicyMap policies;
  policies.Set(policy::key::kNTPCustomBackgroundEnabled,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_CLOUD, base::Value(false), nullptr);
  policy_provider_.UpdateChromePolicy(policies);

  EXPECT_TRUE(prefs
                  ->GetDict(GetThemePrefNameInMigration(
                      ThemePrefInMigration::kNtpCustomBackgroundDict))
                  .empty());
  EXPECT_TRUE(prefs->IsManagedPreference(GetThemePrefNameInMigration(
      ThemePrefInMigration::kNtpCustomBackgroundDict)));

  // Flip the value, and check again.
  policies.Set(policy::key::kNTPCustomBackgroundEnabled,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_CLOUD, base::Value(true), nullptr);
  policy_provider_.UpdateChromePolicy(policies);

  EXPECT_FALSE(prefs
                   ->GetDict(GetThemePrefNameInMigration(
                       ThemePrefInMigration::kNtpCustomBackgroundDict))
                   .empty());
  EXPECT_FALSE(prefs->IsManagedPreference(GetThemePrefNameInMigration(
      ThemePrefInMigration::kNtpCustomBackgroundDict)));

  policy_provider_.UpdateChromePolicy(policy::PolicyMap());

  EXPECT_FALSE(prefs
                   ->GetDict(GetThemePrefNameInMigration(
                       ThemePrefInMigration::kNtpCustomBackgroundDict))
                   .empty());
  EXPECT_FALSE(prefs->IsManagedPreference(GetThemePrefNameInMigration(
      ThemePrefInMigration::kNtpCustomBackgroundDict)));
}
