// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/privacy_sandbox/tracking_protection_settings_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/tracking_protection_prefs.h"
#include "components/privacy_sandbox/tracking_protection_settings.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

class TrackingProtectionSettingsMetricsBrowserTest
    : public InProcessBrowserTest {
 protected:
  base::HistogramTester histogram_tester_;
};

IN_PROC_BROWSER_TEST_F(TrackingProtectionSettingsMetricsBrowserTest,
                       RecordsMetricsOnStartup) {
  histogram_tester_.ExpectUniqueSample("Settings.TrackingProtection.Enabled",
                                       false, 1);
}

class TrackingProtectionSettingsForEnterpriseBrowserTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    policy_provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);
  }

  void SetUpOnMainThread() override {
    enterprise_managed_ = GetParam();
    browser()->profile()->GetPrefs()->SetBoolean(
        prefs::kTrackingProtection3pcdEnabled, true);
    if (enterprise_managed_) {
      policy::PolicyMap policy;
      policy.Set(policy::key::kFirstPartySetsEnabled,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD, base::Value(false), nullptr);
      policy_provider_.UpdateChromePolicy(policy);
    }
  }

 protected:
  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;
  bool enterprise_managed_;
};

IN_PROC_BROWSER_TEST_P(TrackingProtectionSettingsForEnterpriseBrowserTest,
                       ReflectsEnterpriseStateOnStartup) {
  // Tracking protection 3pcd pref should be false iff relevant pref was
  // enterprise-managed at startup.
  EXPECT_EQ(browser()->profile()->GetPrefs()->GetBoolean(
                prefs::kTrackingProtection3pcdEnabled),
            !enterprise_managed_);
}

INSTANTIATE_TEST_SUITE_P(All,
                         TrackingProtectionSettingsForEnterpriseBrowserTest,
                         testing::Bool());
