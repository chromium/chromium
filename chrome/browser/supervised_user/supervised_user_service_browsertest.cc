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
#include "chrome/browser/supervised_user/supervised_user_settings_service.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/net/safe_search_util.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
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
  logged_in_user_mixin_.LogInUser();
  Profile* profile = browser()->profile();
  PrefService* prefs = profile->GetPrefs();
  EXPECT_TRUE(prefs->GetBoolean(prefs::kForceGoogleSafeSearch));
  EXPECT_EQ(prefs->GetInteger(prefs::kForceYouTubeRestrict),
            safe_search_util::YOUTUBE_RESTRICT_MODERATE);
  EXPECT_FALSE(
      prefs->IsUserModifiablePreference(prefs::kForceGoogleSafeSearch));
  EXPECT_FALSE(prefs->IsUserModifiablePreference(prefs::kForceYouTubeRestrict));
}

IN_PROC_BROWSER_TEST_F(SupervisedUserServiceTestSupervised, ProfileName) {
  logged_in_user_mixin_.LogInUser();
  Profile* profile = browser()->profile();
  PrefService* prefs = profile->GetPrefs();
  std::string original_name = prefs->GetString(prefs::kProfileName);

  SupervisedUserSettingsService* settings =
      SupervisedUserSettingsServiceFactory::GetForKey(profile->GetProfileKey());

  // Change the name. Both the profile pref and the entry in
  // ProfileAttributesStorage should be updated.
  std::string name = "Supervised User Test Name";
  settings->SetLocalSetting(supervised_users::kUserName, base::Value(name));
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
  settings->SetLocalSetting(supervised_users::kUserName, base::Value(new_name));
  EXPECT_EQ(new_name, prefs->GetString(prefs::kProfileName));
  EXPECT_EQ(new_name, base::UTF16ToUTF8(entry->GetName()));

  // Remove the setting.
  settings->RemoveLocalSetting(supervised_users::kUserName);
  EXPECT_EQ(original_name, prefs->GetString(prefs::kProfileName));
  EXPECT_EQ(original_name, base::UTF16ToUTF8(entry->GetName()));
}

// Disabled due to excessive flakiness (crbug/1251785).
IN_PROC_BROWSER_TEST_F(SupervisedUserServiceTestSupervised,
                       DISABLED_DenylistLoaded) {
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(
      SupervisedUserService::GetDenylistSourceHistogramForTesting(), 0);

  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedPathOverride path_override(chrome::DIR_USER_DATA,
                                         fake_user_data_dir_.GetPath());
  base::FilePath denylist_path =
      SupervisedUserService::GetDenylistPathForTesting();
  EXPECT_FALSE(base::PathExists(denylist_path));

  base::RunLoop run_loop;
  content::URLLoaderInterceptor interceptor(base::BindLambdaForTesting(
      [&](content::URLLoaderInterceptor::RequestParams* params) {
        std::string headers =
            "HTTP/1.1 200 OK\n"
            "Content-Type: text/html\n";
        content::URLLoaderInterceptor::WriteResponse(headers, "",
                                                     params->client.get());
        run_loop.Quit();
        return true;
      }));

  logged_in_user_mixin_.LogInUser();
  run_loop.Run();

  EXPECT_TRUE(base::PathExists(denylist_path));
  histogram_tester.ExpectUniqueSample(
      SupervisedUserService::GetDenylistSourceHistogramForTesting(),
      SupervisedUserService::DenylistSource::kDenylist, 1);
}

// Disabled due to excessive flakiness (crbug/1251785).
IN_PROC_BROWSER_TEST_F(SupervisedUserServiceTestSupervised,
                       DISABLED_ExistingDenylistLoaded) {
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(
      SupervisedUserService::GetDenylistSourceHistogramForTesting(), 0);

  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedPathOverride path_override(chrome::DIR_USER_DATA,
                                         fake_user_data_dir_.GetPath());

  base::FilePath denylist_path =
      SupervisedUserService::GetDenylistPathForTesting();
  base::WriteFile(denylist_path, "");
  EXPECT_TRUE(base::PathExists(denylist_path));

  logged_in_user_mixin_.LogInUser();

  histogram_tester.ExpectUniqueSample(
      SupervisedUserService::GetDenylistSourceHistogramForTesting(),
      SupervisedUserService::DenylistSource::kDenylist, 1);
}

// Disabled due to excessive flakiness (crbug/1251785).
IN_PROC_BROWSER_TEST_F(SupervisedUserServiceTestSupervised,
                       DISABLED_NoDenylistLoaded) {
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(
      SupervisedUserService::GetDenylistSourceHistogramForTesting(), 0);

  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedPathOverride path_override(chrome::DIR_USER_DATA,
                                         fake_user_data_dir_.GetPath());

  base::RunLoop run_loop;
  content::URLLoaderInterceptor interceptor(base::BindLambdaForTesting(
      [&](content::URLLoaderInterceptor::RequestParams* params) {
        LOG(ERROR) << params->url_request.url.path();
        std::string headers = "HTTP/1.1 500 Internal Server Error\n\n";
        content::URLLoaderInterceptor::WriteResponse(headers, "",
                                                     params->client.get());
        run_loop.Quit();
        return true;
      }));

  logged_in_user_mixin_.LogInUser();
  run_loop.Run();

  base::FilePath denylist_path =
      SupervisedUserService::GetDenylistPathForTesting();
  EXPECT_FALSE(base::PathExists(denylist_path));
  histogram_tester.ExpectUniqueSample(
      SupervisedUserService::GetDenylistSourceHistogramForTesting(),
      SupervisedUserService::DenylistSource::kNoSource, 1);
}
