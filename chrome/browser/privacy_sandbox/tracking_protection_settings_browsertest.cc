// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/privacy_sandbox/tracking_protection_settings.h"

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/privacy_sandbox/tracking_protection_settings_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/privacy_sandbox/tracking_protection_prefs.h"
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
  histogram_tester_.ExpectUniqueSample("Settings.IpProtection.Enabled", false,
                                       1);
  histogram_tester_.ExpectUniqueSample(
      "Settings.FingerprintingProtection.Enabled", false, 1);
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

class TrackingProtectionSettingsIppInitializationBrowserTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  TrackingProtectionSettingsIppInitializationBrowserTest() {
    if (GetParam()) {
      feature_list_.InitAndEnableFeature(
          privacy_sandbox::kIpProtectionDogfoodDefaultOn);
    } else {
      feature_list_.InitAndDisableFeature(
          privacy_sandbox::kIpProtectionDogfoodDefaultOn);
    }
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(TrackingProtectionSettingsIppInitializationBrowserTest,
                       DogfoodDefaultOnFeatureInitializesPrefToEnabled) {
  EXPECT_EQ(
      browser()->profile()->GetPrefs()->GetBoolean(prefs::kIpProtectionEnabled),
      GetParam());
  EXPECT_EQ(browser()->profile()->GetPrefs()->GetBoolean(
                prefs::kIpProtectionInitializedByDogfood),
            GetParam());
}

INSTANTIATE_TEST_SUITE_P(All,
                         TrackingProtectionSettingsIppInitializationBrowserTest,
                         testing::Bool());

class TrackingProtectionSettingsExceptionsMigrationBrowserTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  TrackingProtectionSettingsExceptionsMigrationBrowserTest() {
    if (migrate_to_tp_) {
      feature_list_.InitAndEnableFeature(
          privacy_sandbox::kTrackingProtectionContentSettingFor3pcb);
    } else {
      feature_list_.InitAndDisableFeature(
          privacy_sandbox::kTrackingProtectionContentSettingFor3pcb);
    }
  }

  bool HasException(ContentSettingsType type, GURL site) {
    content_settings::SettingInfo setting_info;
    HostContentSettingsMapFactory::GetForProfile(browser()->profile())
        ->GetContentSetting(GURL(), site, type, &setting_info);
    return !setting_info.secondary_pattern.MatchesAllHosts();
  }

 protected:
  const GURL k3pcUrl = GURL("http://www.cookies.com");
  const GURL kTpUrl = GURL("http://www.tracking-protection.com");
  const GURL kUserBypass3pcUrl = GURL("http://www.ub-cookies.com");
  const GURL kUserBypassTpUrl = GURL("http://www.ub-tracking-protection.com");

  bool migrate_to_tp_ = std::get<0>(GetParam());
  bool was_already_migrated_ = std::get<1>(GetParam());
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(TrackingProtectionSettingsExceptionsMigrationBrowserTest,
                       PRE_MigratesExceptionsBasedOnPrefAndFeature) {
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kUserBypass3pcExceptionsMigrated, was_already_migrated_);

  auto* map =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  // Reset any content settings that may have been added in the previous run.
  for (auto& url : {kUserBypass3pcUrl, kUserBypassTpUrl, k3pcUrl, kTpUrl}) {
    for (const auto& type : {ContentSettingsType::COOKIES,
                             ContentSettingsType::TRACKING_PROTECTION}) {
      map->SetContentSettingCustomScope(ContentSettingsPattern::Wildcard(),
                                        ContentSettingsPattern::FromURL(url),
                                        type, CONTENT_SETTING_DEFAULT);
    }
  }
  // Add UB content settings exceptions.
  content_settings::ContentSettingConstraints constraints;
  constraints.set_lifetime(base::Days(30));
  map->SetContentSettingCustomScope(
      ContentSettingsPattern::Wildcard(),
      ContentSettingsPattern::FromURL(kUserBypass3pcUrl),
      ContentSettingsType::COOKIES, CONTENT_SETTING_ALLOW, constraints);
  map->SetContentSettingCustomScope(
      ContentSettingsPattern::Wildcard(),
      ContentSettingsPattern::FromURL(kUserBypassTpUrl),
      ContentSettingsType::TRACKING_PROTECTION, CONTENT_SETTING_ALLOW,
      constraints);
  // Add permanent content settings exceptions.
  map->SetContentSettingCustomScope(ContentSettingsPattern::Wildcard(),
                                    ContentSettingsPattern::FromURL(k3pcUrl),
                                    ContentSettingsType::COOKIES,
                                    CONTENT_SETTING_ALLOW);
  map->SetContentSettingCustomScope(ContentSettingsPattern::Wildcard(),
                                    ContentSettingsPattern::FromURL(kTpUrl),
                                    ContentSettingsType::TRACKING_PROTECTION,
                                    CONTENT_SETTING_ALLOW);
}

IN_PROC_BROWSER_TEST_P(TrackingProtectionSettingsExceptionsMigrationBrowserTest,
                       MigratesExceptionsBasedOnPrefAndFeature) {
  EXPECT_EQ(browser()->profile()->GetPrefs()->GetBoolean(
                prefs::kUserBypass3pcExceptionsMigrated),
            migrate_to_tp_);
  // UB Tracking Protection exceptions
  EXPECT_EQ(
      HasException(ContentSettingsType::TRACKING_PROTECTION, kUserBypassTpUrl),
      migrate_to_tp_ || !was_already_migrated_);
  EXPECT_EQ(
      HasException(ContentSettingsType::TRACKING_PROTECTION, kUserBypass3pcUrl),
      migrate_to_tp_ && !was_already_migrated_);
  // UB 3PC exceptions
  EXPECT_EQ(HasException(ContentSettingsType::COOKIES, kUserBypass3pcUrl),
            !migrate_to_tp_ || was_already_migrated_);
  EXPECT_EQ(HasException(ContentSettingsType::COOKIES, kUserBypassTpUrl),
            !migrate_to_tp_ && was_already_migrated_);
  // Permanent exceptions
  EXPECT_TRUE(HasException(ContentSettingsType::TRACKING_PROTECTION, kTpUrl));
  EXPECT_FALSE(HasException(ContentSettingsType::TRACKING_PROTECTION, k3pcUrl));
  EXPECT_FALSE(HasException(ContentSettingsType::COOKIES, kTpUrl));
  EXPECT_TRUE(HasException(ContentSettingsType::COOKIES, k3pcUrl));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    TrackingProtectionSettingsExceptionsMigrationBrowserTest,
    testing::Combine(testing::Bool(), testing::Bool()));
