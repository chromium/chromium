// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/privacy_sandbox_settings_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "components/privacy_sandbox/privacy_sandbox_settings.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

class PrivacySandboxSettingsBrowserPolicyTest : public InProcessBrowserTest {
 public:
  PrivacySandboxSettingsBrowserPolicyTest() {
    policy_provider()->SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        policy_provider());

    policy::PolicyMap third_party_cookies_blocked_policy;
    third_party_cookies_blocked_policy.Set(
        policy::key::kBlockThirdPartyCookies, policy::POLICY_LEVEL_MANDATORY,
        policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
        base::Value(true),
        /*external_data_fetcher=*/nullptr);
    policy_provider()->UpdateChromePolicy(third_party_cookies_blocked_policy);
  }

  privacy_sandbox::PrivacySandboxSettings* privacy_sandbox_settings() {
    return PrivacySandboxSettingsFactory::GetForProfile(browser()->profile());
  }

  policy::MockConfigurationPolicyProvider* policy_provider() {
    return &policy_provider_;
  }

 protected:
  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;
};

// Reconciliation should not run while 3P or all cookies are disabled by
// policy, but should run if the policy is changed or removed.
IN_PROC_BROWSER_TEST_F(PrivacySandboxSettingsBrowserPolicyTest,
                       DelayedReconciliationCookieSettingsManaged) {
  privacy_sandbox_settings();

  // Policies set in the test constructor should have prevented reconciliation
  // from running immediately.
  EXPECT_FALSE(browser()->profile()->GetPrefs()->GetBoolean(
      prefs::kPrivacySandboxPreferencesReconciled));

  // Check that applying a different policy which also results in 3P cookies
  // being blocked does not result in reconciliation running.
  policy::PolicyMap all_cookies_blocked_policy;
  all_cookies_blocked_policy.Set(
      policy::key::kDefaultCookiesSetting, policy::POLICY_LEVEL_MANDATORY,
      policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
      base::Value(CONTENT_SETTING_BLOCK),
      /*external_data_fetcher=*/nullptr);
  policy_provider()->UpdateChromePolicy(all_cookies_blocked_policy);
  EXPECT_FALSE(browser()->profile()->GetPrefs()->GetBoolean(
      prefs::kPrivacySandboxPreferencesReconciled));

  // Apply policy which allows third party cookies and ensure that
  // reconciliation runs.
  policy::PolicyMap third_party_cookies_allowed_policy;
  third_party_cookies_allowed_policy.Set(
      policy::key::kBlockThirdPartyCookies, policy::POLICY_LEVEL_MANDATORY,
      policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
      base::Value(false),
      /*external_data_fetcher=*/nullptr);
  policy_provider()->UpdateChromePolicy(third_party_cookies_allowed_policy);
  EXPECT_TRUE(browser()->profile()->GetPrefs()->GetBoolean(
      prefs::kPrivacySandboxPreferencesReconciled));
}
