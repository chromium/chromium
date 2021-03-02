// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/callback.h"
#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/ash/login/test/logged_in_user_mixin.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/supervised_user/supervised_user_constants.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/net/safe_search_util.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "google_apis/gaia/google_service_auth_error.h"

class SupervisedUserServiceTestSupervised
    : public MixinBasedInProcessBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
    logged_in_user_mixin_.LogInUser();
  }

 private:
  chromeos::LoggedInUserMixin logged_in_user_mixin_{
      &mixin_host_, chromeos::LoggedInUserMixin::LogInType::kChild,
      embedded_test_server(), this};
};

// unsupervised tests
using SupervisedUserServiceTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(SupervisedUserServiceTest, LocalPolicies) {
  Profile* profile = browser()->profile();
  PrefService* prefs = profile->GetPrefs();
  EXPECT_FALSE(prefs->GetBoolean(prefs::kForceGoogleSafeSearch));
  EXPECT_EQ(prefs->GetInteger(prefs::kForceYouTubeRestrict),
            safe_search_util::YOUTUBE_RESTRICT_OFF);
  EXPECT_TRUE(prefs->IsUserModifiablePreference(prefs::kForceGoogleSafeSearch));
  EXPECT_TRUE(prefs->IsUserModifiablePreference(prefs::kForceYouTubeRestrict));
}

IN_PROC_BROWSER_TEST_F(SupervisedUserServiceTest, ProfileName) {
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

IN_PROC_BROWSER_TEST_F(SupervisedUserServiceTestSupervised, LocalPolicies) {
  Profile* profile = browser()->profile();
  PrefService* prefs = profile->GetPrefs();
  EXPECT_FALSE(prefs->GetBoolean(prefs::kForceGoogleSafeSearch));
  EXPECT_EQ(prefs->GetInteger(prefs::kForceYouTubeRestrict),
            safe_search_util::YOUTUBE_RESTRICT_OFF);
  EXPECT_FALSE(
      prefs->IsUserModifiablePreference(prefs::kForceGoogleSafeSearch));
  EXPECT_FALSE(prefs->IsUserModifiablePreference(prefs::kForceYouTubeRestrict));
}

IN_PROC_BROWSER_TEST_F(SupervisedUserServiceTestSupervised, ProfileName) {
  Profile* profile = browser()->profile();
  PrefService* prefs = profile->GetPrefs();
  std::string original_name = prefs->GetString(prefs::kProfileName);

  SupervisedUserSettingsService* settings =
      SupervisedUserSettingsServiceFactory::GetForKey(profile->GetProfileKey());

  // Change the name. Both the profile pref and the entry in
  // ProfileAttributesStorage should be updated.
  std::string name = "Supervised User Test Name";
  settings->SetLocalSetting(
      supervised_users::kUserName,
      std::unique_ptr<base::Value>(new base::Value(name)));
  EXPECT_FALSE(prefs->IsUserModifiablePreference(prefs::kProfileName));
  EXPECT_EQ(name, prefs->GetString(prefs::kProfileName));

  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile->GetPath());
  ASSERT_NE(entry, nullptr);
  EXPECT_EQ(name, base::UTF16ToUTF8(entry->GetName()));

  // Change the name once more.
  std::string new_name = "New Supervised User Test Name";
  settings->SetLocalSetting(
      supervised_users::kUserName,
      std::unique_ptr<base::Value>(new base::Value(new_name)));
  EXPECT_EQ(new_name, prefs->GetString(prefs::kProfileName));
  EXPECT_EQ(new_name, base::UTF16ToUTF8(entry->GetName()));

  // Remove the setting.
  settings->SetLocalSetting(supervised_users::kUserName,
                            std::unique_ptr<base::Value>());
  EXPECT_EQ(original_name, prefs->GetString(prefs::kProfileName));
  EXPECT_EQ(original_name, base::UTF16ToUTF8(entry->GetName()));
}
