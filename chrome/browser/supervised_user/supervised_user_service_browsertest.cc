// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <memory>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_path_override.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/ash/login/test/logged_in_user_mixin.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/safe_search_api/safe_search_util.h"
#include "components/supervised_user/core/browser/supervised_user_settings_service.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "google_apis/gaia/google_service_auth_error.h"

class SupervisedUserServiceTestSupervised
    : public MixinBasedInProcessBrowserTest {
 protected:
  void SetUp() override {
    ASSERT_TRUE(fake_user_data_dir_.CreateUniqueTempDir());
    MixinBasedInProcessBrowserTest::SetUp();
  }

  ash::LoggedInUserMixin logged_in_user_mixin_{
      &mixin_host_, ash::LoggedInUserMixin::LogInType::kChild,
      embedded_test_server(), this};
  base::ScopedTempDir fake_user_data_dir_;
};

// unsupervised tests
using SupervisedUserServiceTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(SupervisedUserServiceTest, LocalPolicies) {
  Profile* profile = browser()->profile();
  PrefService* prefs = profile->GetPrefs();
  EXPECT_FALSE(prefs->GetBoolean(policy::policy_prefs::kForceGoogleSafeSearch));
  EXPECT_EQ(prefs->GetInteger(policy::policy_prefs::kForceYouTubeRestrict),
            safe_search_api::YOUTUBE_RESTRICT_OFF);
  EXPECT_TRUE(prefs->IsUserModifiablePreference(
      policy::policy_prefs::kForceGoogleSafeSearch));
  EXPECT_TRUE(prefs->IsUserModifiablePreference(
      policy::policy_prefs::kForceYouTubeRestrict));
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
  logged_in_user_mixin_.LogInUser();
  Profile* profile = browser()->profile();
  PrefService* prefs = profile->GetPrefs();
  EXPECT_TRUE(prefs->GetBoolean(policy::policy_prefs::kForceGoogleSafeSearch));
  EXPECT_EQ(prefs->GetInteger(policy::policy_prefs::kForceYouTubeRestrict),
            safe_search_api::YOUTUBE_RESTRICT_MODERATE);
  EXPECT_FALSE(prefs->IsUserModifiablePreference(
      policy::policy_prefs::kForceGoogleSafeSearch));
  EXPECT_FALSE(prefs->IsUserModifiablePreference(
      policy::policy_prefs::kForceYouTubeRestrict));
}
