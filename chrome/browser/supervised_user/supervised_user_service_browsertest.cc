// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/supervised_user/supervision_mixin.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/safe_search_api/safe_search_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class SupervisedUserServiceBrowserTest
    : public MixinBasedInProcessBrowserTest,
      public ::testing::WithParamInterface<
          supervised_user::SupervisionMixin::SignInMode> {
 protected:
  static supervised_user::SupervisionMixin::SignInMode GetSignInMode() {
    return GetParam();
  }

  supervised_user::SupervisionMixin supervision_mixin_{
      mixin_host_,
      this,
      embedded_test_server(),
      {.sign_in_mode = GetSignInMode()}};

 private:
  base::test::ScopedFeatureList scoped_feature_list;
};

IN_PROC_BROWSER_TEST_P(SupervisedUserServiceBrowserTest, LocalPolicies) {
  Profile* profile = browser()->profile();
  PrefService* prefs = profile->GetPrefs();

  if (GetSignInMode() ==
      supervised_user::SupervisionMixin::SignInMode::kSupervised) {
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
#if !BUILDFLAG(IS_CHROMEOS_ASH)
        // Only for platforms that support signed-out browser.
        supervised_user::SupervisionMixin::SignInMode::kSignedOut,
#endif
        supervised_user::SupervisionMixin::SignInMode::kRegular,
        supervised_user::SupervisionMixin::SignInMode::kSupervised),
    ::testing::PrintToStringParamName());

}  // namespace
