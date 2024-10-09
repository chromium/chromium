// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/util/managed_browser_utils.h"

#include <vector>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/pref_names.h"
#include "components/content_settings/core/common/pref_names.h"
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
class EnterpriseBadgingTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<std::tuple<bool, bool, bool, bool>> {
 public:
  void SetUp() override {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;
    if (avatar_feature_enabled()) {
      enabled_features.emplace_back(
          features::kEnterpriseProfileBadgingForAvatar);
    } else {
      disabled_features.emplace_back(
          features::kEnterpriseProfileBadgingForAvatar);
    }
    if (profile_menu_feature_enabled()) {
      enabled_features.emplace_back(features::kEnterpriseProfileBadgingForMenu);
    } else {
      disabled_features.emplace_back(
          features::kEnterpriseProfileBadgingForMenu);
    }
    if (policies_feature_enabled()) {
      enabled_features.emplace_back(
          features::kEnterpriseProfileBadgingPolicies);
    } else {
      disabled_features.emplace_back(
          features::kEnterpriseProfileBadgingPolicies);
    }
    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    SetUserAcceptedAccountManagement(browser()->profile(), managed_profile());
    InProcessBrowserTest::SetUpOnMainThread();
  }
  bool avatar_feature_enabled() { return std::get<0>(GetParam()); }
  bool profile_menu_feature_enabled() { return std::get<1>(GetParam()); }
  bool policies_feature_enabled() { return std::get<2>(GetParam()); }
  bool managed_profile() { return std::get<3>(GetParam()); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(EnterpriseBadgingTest, CanShowEnterpriseBadging) {
  Profile* profile = browser()->profile();
  // When no custom policy is set, the visibility of each of the the avatar
  // badging and profile menu badging depends on whether the profile is managed
  // and if each feature controlling the default behaviour is enabled.
  EXPECT_EQ(CanShowEnterpriseBadgingForAvatar(profile),
            avatar_feature_enabled() && managed_profile());
  EXPECT_EQ(CanShowEnterpriseBadgingForMenu(profile),
            profile_menu_feature_enabled() && managed_profile());

  profile->GetPrefs()->SetString(prefs::kEnterpriseCustomLabelForProfile,
                                 "some_label");
  EXPECT_EQ(CanShowEnterpriseBadgingForAvatar(profile),
            (avatar_feature_enabled() || policies_feature_enabled()) &&
                managed_profile());

  profile->GetPrefs()->SetString(prefs::kEnterpriseLogoUrlForProfile,
                                 "some_url");
  EXPECT_EQ(CanShowEnterpriseBadgingForMenu(profile),
            ((profile_menu_feature_enabled() || policies_feature_enabled()) &&
             managed_profile()));
}

INSTANTIATE_TEST_SUITE_P(,
                         EnterpriseBadgingTest,
                         testing::Combine(testing::Bool(),
                                          testing::Bool(),
                                          testing::Bool(),
                                          testing::Bool()));
#endif  // !BUILDFLAG(IS_CHROMEOS)
}  // namespace enterprise_util

