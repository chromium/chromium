// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/supervised_user_service.h"

#include "base/functional/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/supervised_user/supervision_mixin.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/safe_search_api/safe_search_util.h"
#include "components/supervised_user/core/browser/supervised_user_preferences.h"
#include "components/supervised_user/test_support/kids_management_api_server_mock.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace supervised_user {
namespace {

class SupervisedUserServiceBrowserTest
    : public MixinBasedInProcessBrowserTest,
      public ::testing::WithParamInterface<SupervisionMixin::SignInMode> {
 protected:
  static SupervisionMixin::SignInMode GetSignInMode() { return GetParam(); }

  KidsManagementApiServerMock& kids_management_api_mock() {
    return supervision_mixin_.api_mock_setup_mixin().api_mock();
  }

  SupervisionMixin supervision_mixin_{mixin_host_,
                                      this,
                                      embedded_test_server(),
                                      {.sign_in_mode = GetSignInMode()}};

 private:
  base::test::ScopedFeatureList scoped_feature_list;
};

IN_PROC_BROWSER_TEST_P(SupervisedUserServiceBrowserTest, LocalPolicies) {
  Profile* profile = browser()->profile();
  PrefService* prefs = profile->GetPrefs();

  if (GetSignInMode() == SupervisionMixin::SignInMode::kSupervised) {
    EXPECT_FALSE(
        prefs->GetBoolean(policy::policy_prefs::kForceGoogleSafeSearch));
    EXPECT_TRUE(prefs->IsUserModifiablePreference(
        policy::policy_prefs::kForceGoogleSafeSearch));

    EXPECT_EQ(prefs->GetInteger(policy::policy_prefs::kForceYouTubeRestrict),
              safe_search_api::YOUTUBE_RESTRICT_OFF);
    EXPECT_TRUE(prefs->IsUserModifiablePreference(
        policy::policy_prefs::kForceYouTubeRestrict));
  } else {
    EXPECT_FALSE(
        prefs->GetBoolean(policy::policy_prefs::kForceGoogleSafeSearch));
    EXPECT_EQ(prefs->GetInteger(policy::policy_prefs::kForceYouTubeRestrict),
              safe_search_api::YOUTUBE_RESTRICT_OFF);
    EXPECT_TRUE(prefs->IsUserModifiablePreference(
        policy::policy_prefs::kForceGoogleSafeSearch));
    EXPECT_TRUE(prefs->IsUserModifiablePreference(
        policy::policy_prefs::kForceYouTubeRestrict));
  }
}

IN_PROC_BROWSER_TEST_P(SupervisedUserServiceBrowserTest, ProfileName) {
  Profile* profile = browser()->profile();
  PrefService* prefs = profile->GetPrefs();
  EXPECT_TRUE(prefs->IsUserModifiablePreference(prefs::kProfileName));

  std::string original_name = prefs->GetString(prefs::kProfileName);
  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile->GetPath());
  ASSERT_NE(entry, nullptr);
  EXPECT_EQ(original_name, base::UTF16ToUTF8(entry->GetName()));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    SupervisedUserServiceBrowserTest,
    testing::Values(
#if !BUILDFLAG(IS_CHROMEOS)
        // Only for platforms that support signed-out browser.
        SupervisionMixin::SignInMode::kSignedOut,
#endif
        SupervisionMixin::SignInMode::kRegular,
        SupervisionMixin::SignInMode::kSupervised),
    ::testing::PrintToStringParamName());

// Suite for supervised user features activated for regular users.
class SupervisedUserServiceForRegularUsersBrowserTest
    : public MixinBasedInProcessBrowserTest,
      public ::testing::WithParamInterface<SupervisionMixin::SignInMode> {
 protected:
  static SupervisionMixin::SignInMode GetSignInMode() { return GetParam(); }
  SupervisionMixin supervision_mixin_{mixin_host_,
                                      this,
                                      embedded_test_server(),
                                      {.sign_in_mode = GetSignInMode()}};
};

IN_PROC_BROWSER_TEST_P(SupervisedUserServiceForRegularUsersBrowserTest,
                       ForceGoogleSafeSearchCanBeOverriden) {
  Profile* profile = browser()->profile();
  PrefService* prefs = profile->GetPrefs();

  content::TestNavigationObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents());
  GURL search_url("https://google.com/search?q=test");

  // 1. Default behavior.
  EXPECT_FALSE(prefs->GetBoolean(policy::policy_prefs::kForceGoogleSafeSearch));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), search_url));
  {
    // Safe search is not on. Called url is left unchanged.
    const GURL& navigated_url = observer.last_navigation_url();
    EXPECT_EQ(navigated_url, search_url);
  }

  // 2. Enable safe search.
  SetGoogleSafeSearch(*prefs, GoogleSafeSearchStateStatus::kEnforced);
  ASSERT_TRUE(prefs->GetBoolean(policy::policy_prefs::kForceGoogleSafeSearch));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), search_url));

  {
    // Safe search is enforced. Note extra query params `safe` and `ssui`.
    const GURL& navigated_url = observer.last_navigation_url();
    EXPECT_EQ(navigated_url.spec(),
              "https://google.com/search?q=test&safe=active&ssui=on");
  }

  // 3. Disable safe search back to original state.
  SetGoogleSafeSearch(*prefs, GoogleSafeSearchStateStatus::kDisabled);
  ASSERT_FALSE(prefs->GetBoolean(policy::policy_prefs::kForceGoogleSafeSearch));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), search_url));

  {
    // Safe search is not on. Called url is left unchanged.
    const GURL& navigated_url = observer.last_navigation_url();
    EXPECT_EQ(navigated_url, search_url);
  }
}

IN_PROC_BROWSER_TEST_P(SupervisedUserServiceForRegularUsersBrowserTest,
                       UrlFilterIsOffByDefault) {
  EXPECT_TRUE(SupervisedUserServiceFactory::GetForProfile(browser()->profile())
                  ->GetURLFilter()
                  ->GetWebFilterType() == WebFilterType::kDisabled);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    SupervisedUserServiceForRegularUsersBrowserTest,
    testing::Values(
#if !BUILDFLAG(IS_CHROMEOS)
        // Only for platforms that support signed-out browser.
        SupervisionMixin::SignInMode::kSignedOut,
#endif
        SupervisionMixin::SignInMode::kRegular),
    ::testing::PrintToStringParamName());

// Suite for supervised user features behavior for supervised users.
class SupervisedUserServiceForSupervisedUsersBrowserTest
    : public MixinBasedInProcessBrowserTest {
 protected:
  SupervisionMixin supervision_mixin_{
      mixin_host_,
      this,
      embedded_test_server(),
      {.sign_in_mode = SupervisionMixin::SignInMode::kSupervised}};
};

IN_PROC_BROWSER_TEST_F(SupervisedUserServiceForSupervisedUsersBrowserTest,
                       UrlFilterIsOnByDefault) {
  EXPECT_NE(WebFilterType::kDisabled,
            SupervisedUserServiceFactory::GetForProfile(browser()->profile())
                ->GetURLFilter()
                ->GetWebFilterType());
}

IN_PROC_BROWSER_TEST_F(SupervisedUserServiceForSupervisedUsersBrowserTest,
                       FilterIsNeutralized) {
  Profile* profile = browser()->profile();
  PrefService* pref_service = profile->GetPrefs();

  supervised_user_test_util::SetWebFilterType(
      profile, supervised_user::WebFilterType::kTryToBlockMatureSites);
  supervised_user_test_util::SetManualFilterForHost(profile, "example1.com",
                                                    /*allowlist=*/true);
  supervised_user_test_util::SetManualFilterForHost(profile, "example2.com",
                                                    /*allowlist=*/false);
  supervised_user_test_util::SetManualFilterForUrl(profile, "example3.com",
                                                   /*allowlist=*/true);
  supervised_user_test_util::SetManualFilterForUrl(profile, "example4.com",
                                                   /*allowlist=*/false);

  EXPECT_FALSE(pref_service->FindPreference(prefs::kSupervisedUserManualHosts)
                   ->IsDefaultValue());
  EXPECT_FALSE(pref_service->FindPreference(prefs::kSupervisedUserManualURLs)
                   ->IsDefaultValue());
  EXPECT_FALSE(pref_service->FindPreference(prefs::kSupervisedUserSafeSites)
                   ->IsDefaultValue());
  EXPECT_FALSE(
      pref_service
          ->FindPreference(prefs::kDefaultSupervisedUserFilteringBehavior)
          ->IsDefaultValue());

  DisableParentalControls(*pref_service);
  EXPECT_EQ(WebFilterType::kDisabled,
            SupervisedUserServiceFactory::GetForProfile(browser()->profile())
                ->GetURLFilter()
                ->GetWebFilterType());

  EXPECT_TRUE(pref_service->FindPreference(prefs::kSupervisedUserManualHosts)
                  ->IsDefaultValue());
  EXPECT_TRUE(pref_service->FindPreference(prefs::kSupervisedUserManualURLs)
                  ->IsDefaultValue());
  EXPECT_TRUE(pref_service->FindPreference(prefs::kSupervisedUserSafeSites)
                  ->IsDefaultValue());
  EXPECT_TRUE(
      pref_service
          ->FindPreference(prefs::kDefaultSupervisedUserFilteringBehavior)
          ->IsDefaultValue());
}

}  // namespace
}  // namespace supervised_user
