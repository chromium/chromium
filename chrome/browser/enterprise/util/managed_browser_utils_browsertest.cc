// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/util/managed_browser_utils.h"

#include <vector>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/enterprise/signals/user_permission_service_factory.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/pref_names.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/device_signals/core/browser/user_permission_service.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/policy/core/common/management/scoped_management_service_override_for_testing.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"

namespace enterprise_util {

namespace {

class ManagedBrowserUtilsBrowserTest
    : public policy::PolicyTest,
      public testing::WithParamInterface<bool> {
 public:
  ManagedBrowserUtilsBrowserTest() = default;
  ~ManagedBrowserUtilsBrowserTest() override = default;

  bool managed_policy() { return GetParam(); }

  base::Value policy_value() {
    constexpr char kAutoSelectCertificateValue[] = R"({
      "pattern": "https://foo.com",
      "filter": {
        "ISSUER": {
          "O": "Chrome",
          "OU": "Chrome Org Unit",
          "CN": "Chrome Common Name"
        }
      }
    })";
    base::Value::List list;
    list.Append(kAutoSelectCertificateValue);
    return base::Value(std::move(list));
  }
};

INSTANTIATE_TEST_SUITE_P(, ManagedBrowserUtilsBrowserTest, testing::Bool());

}  // namespace

IN_PROC_BROWSER_TEST_P(ManagedBrowserUtilsBrowserTest, LocalState) {
  EXPECT_FALSE(
      IsMachinePolicyPref(prefs::kManagedAutoSelectCertificateForUrls));

  policy::PolicyMap policies;
  policies.Set(policy::key::kAutoSelectCertificateForUrls,
               managed_policy() ? policy::POLICY_LEVEL_MANDATORY
                                : policy::POLICY_LEVEL_RECOMMENDED,
               policy::POLICY_SCOPE_MACHINE, policy::POLICY_SOURCE_CLOUD,
               policy_value(), nullptr);
  UpdateProviderPolicy(policies);

  EXPECT_EQ(managed_policy(),
            IsMachinePolicyPref(prefs::kManagedAutoSelectCertificateForUrls));
}

#if !BUILDFLAG(IS_CHROMEOS)
class EnterpriseProfileBadgingTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  void SetUp() override {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;
    if (profile_menu_feature_enabled()) {
      enabled_features.emplace_back(features::kEnterpriseProfileBadgingForMenu);
    } else {
      disabled_features.emplace_back(
          features::kEnterpriseProfileBadgingForMenu);
    }
    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    SetUserAcceptedAccountManagement(browser()->profile(), managed_profile());
    if (managed_profile()) {
      scoped_browser_management_ =
          std::make_unique<policy::ScopedManagementServiceOverrideForTesting>(
              policy::ManagementServiceFactory::GetForProfile(
                  browser()->profile()),
              policy::EnterpriseManagementAuthority::CLOUD);
    }
    InProcessBrowserTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override { scoped_browser_management_.reset(); }

  bool profile_menu_feature_enabled() { return std::get<0>(GetParam()); }
  bool managed_profile() { return std::get<1>(GetParam()); }

 private:
  std::unique_ptr<policy::ScopedManagementServiceOverrideForTesting>
      scoped_browser_management_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(EnterpriseProfileBadgingTest, CanShowEnterpriseBadging) {
  Profile* profile = browser()->profile();
  // When no custom policy is set, the visibility of each of the the avatar
  // badging and profile menu badging depends on whether the profile is managed
  // and if each feature controlling the default behaviour is enabled.
  EXPECT_EQ(CanShowEnterpriseBadgingForAvatar(profile), managed_profile());
  EXPECT_EQ(CanShowEnterpriseBadgingForMenu(profile),
            profile_menu_feature_enabled() && managed_profile());

  profile->GetPrefs()->SetString(prefs::kEnterpriseCustomLabelForProfile,
                                 "some_label");
  EXPECT_EQ(CanShowEnterpriseBadgingForAvatar(profile),
                managed_profile());

  profile->GetPrefs()->SetString(prefs::kEnterpriseLogoUrlForProfile,
                                 "some_url");
  EXPECT_EQ(CanShowEnterpriseBadgingForMenu(profile), managed_profile());
}

IN_PROC_BROWSER_TEST_P(EnterpriseProfileBadgingTest,
                       CanNotShowEnterpriseBadgingForPrimaryOTRProfile) {
  Browser* incognito_browser = Browser::Create(Browser::CreateParams(
      browser()->profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true),
      true));
  // Profile badging should always return false in incognito.
  EXPECT_FALSE(CanShowEnterpriseBadgingForAvatar(incognito_browser->profile()));
  EXPECT_FALSE(CanShowEnterpriseBadgingForMenu(incognito_browser->profile()));
}

IN_PROC_BROWSER_TEST_P(EnterpriseProfileBadgingTest,
                       CanNotShowEnterpriseBadgingForNonPrimaryOTRProfile) {
  browser()->profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  Profile* secondary_incognito = browser()->profile()->GetOffTheRecordProfile(
      Profile::OTRProfileID::CreateUnique("Test:NonPrimaryOTRProfile"),
      /*create_if_needed=*/true);
  // Profile badging should always return false in incognito.
  EXPECT_FALSE(CanShowEnterpriseBadgingForAvatar(secondary_incognito));
  EXPECT_FALSE(CanShowEnterpriseBadgingForMenu(secondary_incognito));
}

INSTANTIATE_TEST_SUITE_P(,
                         EnterpriseProfileBadgingTest,
                         testing::Combine(testing::Bool(),
                                          testing::Bool()));

class EnterpriseBrowserBadgingTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<std::tuple<bool, bool, bool>> {
 public:
  void SetUp() override {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;
    if (footer_management_notice_feature_enabled()) {
      enabled_features.emplace_back(features::kEnterpriseBadgingForNtpFooter);
    } else {
      disabled_features.emplace_back(features::kEnterpriseBadgingForNtpFooter);
    }
    if (policies_feature_enabled()) {
      enabled_features.emplace_back(features::kNTPFooterBadgingPolicies);
    } else {
      disabled_features.emplace_back(features::kNTPFooterBadgingPolicies);
    }
    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    if (managed_browser()) {
      scoped_browser_management_ =
          std::make_unique<policy::ScopedManagementServiceOverrideForTesting>(
              policy::ManagementServiceFactory::GetForProfile(
                  browser()->profile()),
              policy::EnterpriseManagementAuthority::CLOUD_DOMAIN);
    } else {
      scoped_browser_management_ =
          std::make_unique<policy::ScopedManagementServiceOverrideForTesting>(
              policy::ManagementServiceFactory::GetForProfile(
                  browser()->profile()),
              policy::EnterpriseManagementAuthority::NONE);
    }
    InProcessBrowserTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override { scoped_browser_management_.reset(); }

  bool footer_management_notice_feature_enabled() {
    return std::get<0>(GetParam());
  }
  bool policies_feature_enabled() { return std::get<1>(GetParam()); }
  bool managed_browser() { return std::get<2>(GetParam()); }

 private:
  std::unique_ptr<policy::ScopedManagementServiceOverrideForTesting>
      scoped_browser_management_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(EnterpriseBrowserBadgingTest,
                       CanShowEnterpriseBadgingForNTPFooter) {
  Profile* profile = browser()->profile();
  // When no custom policy is set, the visibility of the management notice in
  // the NTP footer depends on whether the browser is managed and
  // if the feature controlling the default behaviour is enabled.
  EXPECT_EQ(CanShowEnterpriseBadgingForNTPFooter(profile),
            footer_management_notice_feature_enabled() && managed_browser());

  g_browser_process->local_state()->SetString(
      prefs::kEnterpriseCustomLabelForBrowser, "some_label");
  EXPECT_EQ(CanShowEnterpriseBadgingForNTPFooter(profile),
            (footer_management_notice_feature_enabled() ||
             policies_feature_enabled()) &&
                managed_browser());

  g_browser_process->local_state()->SetString(
      prefs::kEnterpriseCustomLabelForBrowser, "");
  g_browser_process->local_state()->SetString(
      prefs::kEnterpriseLogoUrlForBrowser, "some_url");
  EXPECT_EQ(CanShowEnterpriseBadgingForNTPFooter(profile),
            ((footer_management_notice_feature_enabled() ||
              policies_feature_enabled()) &&
             managed_browser()));
}

IN_PROC_BROWSER_TEST_P(EnterpriseBrowserBadgingTest,
                       GetManagementNoticeStateForNTPFooter) {
  Profile* profile = browser()->profile();

  if (!managed_browser()) {
    EXPECT_EQ(GetManagementNoticeStateForNTPFooter(profile),
              BrowserManagementNoticeState::kNotApplicable);
    return;
  }

  // Default state: no policy or user setting is specified yet.
  BrowserManagementNoticeState expected_state;
  expected_state = footer_management_notice_feature_enabled()
                       ? BrowserManagementNoticeState::kEnabled
                       : BrowserManagementNoticeState::kNotApplicable;
  EXPECT_EQ(GetManagementNoticeStateForNTPFooter(profile), expected_state);

  // Notice is disabled by policy.
  g_browser_process->local_state()->SetBoolean(
      prefs::kNTPFooterManagementNoticeEnabled, false);
  EXPECT_EQ(GetManagementNoticeStateForNTPFooter(profile),
            BrowserManagementNoticeState::kNotApplicable);
  g_browser_process->local_state()->SetBoolean(
      prefs::kNTPFooterManagementNoticeEnabled, true);

  // Footer is disabled by user pref.
  profile->GetPrefs()->SetBoolean(prefs::kNtpFooterVisible, false);
  expected_state = footer_management_notice_feature_enabled()
                       ? BrowserManagementNoticeState::kDisabled
                       : BrowserManagementNoticeState::kNotApplicable;
  EXPECT_EQ(GetManagementNoticeStateForNTPFooter(profile), expected_state);
  profile->GetPrefs()->SetBoolean(prefs::kNtpFooterVisible, true);

  // Footer has a custom label.
  g_browser_process->local_state()->SetString(
      prefs::kEnterpriseCustomLabelForBrowser, "some_label");
  if (policies_feature_enabled()) {
    expected_state = BrowserManagementNoticeState::kEnabledByPolicy;
  } else {
    expected_state = footer_management_notice_feature_enabled()
                         ? BrowserManagementNoticeState::kEnabled
                         : BrowserManagementNoticeState::kNotApplicable;
  }
  EXPECT_EQ(GetManagementNoticeStateForNTPFooter(profile), expected_state);

  // Footer with custom policy and footer hidden by user pref.
  profile->GetPrefs()->SetBoolean(prefs::kNtpFooterVisible, false);
  if (policies_feature_enabled()) {
    expected_state = BrowserManagementNoticeState::kEnabledByPolicy;
  } else {
    expected_state = footer_management_notice_feature_enabled()
                         ? BrowserManagementNoticeState::kDisabled
                         : BrowserManagementNoticeState::kNotApplicable;
  }
  EXPECT_EQ(GetManagementNoticeStateForNTPFooter(profile), expected_state);
  profile->GetPrefs()->SetBoolean(prefs::kNtpFooterVisible, true);
  g_browser_process->local_state()->SetString(
      prefs::kEnterpriseCustomLabelForBrowser, "");

  // Footer with custom policy and footer enabled by user pref.
  g_browser_process->local_state()->SetString(
      prefs::kEnterpriseLogoUrlForBrowser, "some_url");
  if (policies_feature_enabled()) {
    expected_state = BrowserManagementNoticeState::kEnabledByPolicy;
  } else {
    expected_state = footer_management_notice_feature_enabled()
                         ? BrowserManagementNoticeState::kEnabled
                         : BrowserManagementNoticeState::kNotApplicable;
  }
  EXPECT_EQ(GetManagementNoticeStateForNTPFooter(profile), expected_state);
}

INSTANTIATE_TEST_SUITE_P(,
                         EnterpriseBrowserBadgingTest,
                         testing::Combine(testing::Bool(),
                                          testing::Bool(),
                                          testing::Bool()));
#endif  // !BUILDFLAG(IS_CHROMEOS)

using ManagedBrowserUtilsDeviceSignalsBrowserTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(ManagedBrowserUtilsDeviceSignalsBrowserTest,
                       UserAcceptedAccountManagementSharesDeviceSignals) {
  Profile* profile = browser()->profile();
  auto* user_permission_service =
      enterprise_signals::UserPermissionServiceFactory::GetForProfile(profile);

  ASSERT_FALSE(user_permission_service->HasUserConsented());
  ASSERT_FALSE(UserAcceptedAccountManagement(profile));

  // User has not consented to anything.
  SetUserAcceptedAccountManagement(profile, false);
  ASSERT_FALSE(UserAcceptedAccountManagement(profile));
  ASSERT_FALSE(user_permission_service->HasUserConsented());

  // User has consented to sharing signals for the lifetime of the profile.
  SetUserAcceptedAccountManagement(profile, true);
  ASSERT_TRUE(UserAcceptedAccountManagement(profile));
  ASSERT_EQ(user_permission_service->HasUserConsented(), true);
}
}  // namespace enterprise_util
