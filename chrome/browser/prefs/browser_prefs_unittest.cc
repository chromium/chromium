// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/browser_prefs.h"

#include <cstddef>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/chrome_browser_main_extra_parts_profiles.h"
#include "chrome/browser/profiles/pref_service_builder_utils.h"
#include "components/sync/base/pref_names.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/common/pref_names.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "components/prefs/testing_pref_service.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace {

class BrowserPrefsTest : public testing::Test {
 protected:
  BrowserPrefsTest() {
    // Invoking `RegisterUserProfilePrefs()` isn't sufficient as some migration
    // code relies on prefs registered by the keyed service factories, via
    // BrowserContextKeyedService::RegisterProfilePrefs().
    ChromeBrowserMainExtraPartsProfiles::
        EnsureBrowserContextKeyedServiceFactoriesBuilt();
    RegisterProfilePrefs(/*is_signin_profile=*/false,
                         g_browser_process->GetApplicationLocale(),
                         prefs_.registry());
  }

  // The task environment is needed because some keyed services CHECK for things
  // like content::BrowserThread::CurrentlyOn(content::BrowserThread::UI).
  content::BrowserTaskEnvironment task_environment_;
  sync_preferences::TestingPrefServiceSyncable prefs_;
};

#if BUILDFLAG(IS_CHROMEOS)
constexpr char kTestDohTemplate[] = "https://doh.example/dns-query{?dns}";
constexpr char kTestDohSalt[] = "testsalt";

class BrowserPrefsLocalStateTest : public testing::Test {
 protected:
  BrowserPrefsLocalStateTest() { RegisterLocalState(local_state_.registry()); }

  content::BrowserTaskEnvironment task_environment_;
  TestingPrefServiceSimple local_state_;
};

TEST_F(BrowserPrefsLocalStateTest, ClearsUserSetSecureDnsPrefs) {
  local_state_.SetString(prefs::kDnsOverHttpsMode, "secure");
  local_state_.SetString(prefs::kDnsOverHttpsTemplates, kTestDohTemplate);
  local_state_.SetString(ash::prefs::kDnsOverHttpsSalt, kTestDohSalt);
  local_state_.SetString(ash::prefs::kDnsOverHttpsTemplatesWithIdentifiers,
                         kTestDohTemplate);

  MigrateObsoleteLocalStatePrefs(&local_state_);

  EXPECT_FALSE(local_state_.GetUserPrefValue(prefs::kDnsOverHttpsMode));
  EXPECT_FALSE(local_state_.GetUserPrefValue(prefs::kDnsOverHttpsTemplates));
  EXPECT_FALSE(local_state_.GetUserPrefValue(ash::prefs::kDnsOverHttpsSalt));
  EXPECT_FALSE(local_state_.GetUserPrefValue(
      ash::prefs::kDnsOverHttpsTemplatesWithIdentifiers));
}

TEST_F(BrowserPrefsLocalStateTest, KeepsPolicySecureDnsPrefs) {
  local_state_.SetManagedPref(prefs::kDnsOverHttpsMode, base::Value("secure"));
  local_state_.SetManagedPref(prefs::kDnsOverHttpsTemplates,
                              base::Value(kTestDohTemplate));
  local_state_.SetManagedPref(ash::prefs::kDnsOverHttpsSalt,
                              base::Value(kTestDohSalt));
  local_state_.SetManagedPref(ash::prefs::kDnsOverHttpsTemplatesWithIdentifiers,
                              base::Value(kTestDohTemplate));

  MigrateObsoleteLocalStatePrefs(&local_state_);

  EXPECT_EQ(local_state_.GetString(prefs::kDnsOverHttpsMode), "secure");
  EXPECT_EQ(local_state_.GetString(prefs::kDnsOverHttpsTemplates),
            kTestDohTemplate);
  EXPECT_EQ(local_state_.GetString(ash::prefs::kDnsOverHttpsSalt),
            kTestDohSalt);
  EXPECT_EQ(
      local_state_.GetString(ash::prefs::kDnsOverHttpsTemplatesWithIdentifiers),
      kTestDohTemplate);
}

TEST_F(BrowserPrefsLocalStateTest, KillSwitchOffKeepsUserSetSecureDnsPrefs) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      ash::features::kKeepUserSecureDnsOutOfLocalState);
  local_state_.SetString(prefs::kDnsOverHttpsTemplates, kTestDohTemplate);
  local_state_.SetString(ash::prefs::kDnsOverHttpsSalt, kTestDohSalt);
  local_state_.SetString(ash::prefs::kDnsOverHttpsTemplatesWithIdentifiers,
                         kTestDohTemplate);

  MigrateObsoleteLocalStatePrefs(&local_state_);

  EXPECT_EQ(local_state_.GetString(prefs::kDnsOverHttpsTemplates),
            kTestDohTemplate);
  EXPECT_EQ(local_state_.GetString(ash::prefs::kDnsOverHttpsSalt),
            kTestDohSalt);
  EXPECT_EQ(
      local_state_.GetString(ash::prefs::kDnsOverHttpsTemplatesWithIdentifiers),
      kTestDohTemplate);
}
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace

#if BUILDFLAG(IS_ANDROID)
TEST_F(BrowserPrefsTest, DeletesObsoleteLoginDataFilesWithoutBlocking) {
  base::ScopedTempDir profile_dir;
  ASSERT_TRUE(profile_dir.CreateUniqueTempDir());
  const base::FilePath login_data_files[] = {
      profile_dir.GetPath().Append(FILE_PATH_LITERAL("Login Data")),
      profile_dir.GetPath().Append(FILE_PATH_LITERAL("Login Data For Account")),
      profile_dir.GetPath().Append(FILE_PATH_LITERAL("Login Data-journal")),
      profile_dir.GetPath().Append(
          FILE_PATH_LITERAL("Login Data For Account-journal")),
  };
  for (const base::FilePath& file : login_data_files) {
    ASSERT_TRUE(base::WriteFile(file, ""));
  }

  {
    // Profiles can be loaded after blocking is disallowed on the UI thread.
    base::ScopedDisallowBlocking disallow_blocking;
    MigrateObsoleteProfilePrefs(&prefs_, profile_dir.GetPath());
  }
  // The files are deleted on the thread pool, which doesn't wake up the main
  // thread, so wait for the thread pool directly.
  base::ThreadPoolInstance::Get()->FlushForTesting();

  for (const base::FilePath& file : login_data_files) {
    EXPECT_FALSE(base::PathExists(file)) << file;
  }
}
#else
TEST_F(BrowserPrefsTest, MigrateLegacyProfileAvatarPrefs) {
  base::ScopedTempDir profile_dir;
  ASSERT_TRUE(profile_dir.CreateUniqueTempDir());

  // Legacy avatar without GAIA picture: migrated to placeholder + default.
  prefs_.SetInteger(prefs::kProfileAvatarIndex, 5);
  prefs_.SetBoolean(prefs::kProfileUsingDefaultAvatar, false);
  prefs_.SetBoolean(prefs::kProfileUsingGAIAAvatar, false);
  MigrateObsoleteProfilePrefs(&prefs_, profile_dir.GetPath());
  EXPECT_EQ(prefs_.GetInteger(prefs::kProfileAvatarIndex),
            static_cast<int>(profiles::GetPlaceholderAvatarIndex()));
  EXPECT_TRUE(prefs_.GetBoolean(prefs::kProfileUsingDefaultAvatar));
  EXPECT_FALSE(prefs_.GetBoolean(prefs::kProfileUsingGAIAAvatar));

  // Legacy avatar with GAIA picture: migrated to placeholder, keeping
  // kProfileUsingDefaultAvatar false.
  prefs_.SetInteger(prefs::kProfileAvatarIndex, 0);
  prefs_.SetBoolean(prefs::kProfileUsingDefaultAvatar, false);
  prefs_.SetBoolean(prefs::kProfileUsingGAIAAvatar, true);
  MigrateObsoleteProfilePrefs(&prefs_, profile_dir.GetPath());
  EXPECT_EQ(prefs_.GetInteger(prefs::kProfileAvatarIndex),
            static_cast<int>(profiles::GetPlaceholderAvatarIndex()));
  EXPECT_FALSE(prefs_.GetBoolean(prefs::kProfileUsingDefaultAvatar));
  EXPECT_TRUE(prefs_.GetBoolean(prefs::kProfileUsingGAIAAvatar));

  // Valid avatar (modern on desktop, placeholder on ChromeOS) is not modified.
  const int valid_index =
      static_cast<int>(profiles::GetModernAvatarIconStartIndex());
  prefs_.SetInteger(prefs::kProfileAvatarIndex, valid_index);
  prefs_.SetBoolean(prefs::kProfileUsingDefaultAvatar, false);
  prefs_.SetBoolean(prefs::kProfileUsingGAIAAvatar, false);
  MigrateObsoleteProfilePrefs(&prefs_, profile_dir.GetPath());
  EXPECT_EQ(prefs_.GetInteger(prefs::kProfileAvatarIndex), valid_index);
  EXPECT_FALSE(prefs_.GetBoolean(prefs::kProfileUsingDefaultAvatar));

  // Unknown future index is not modified.
  prefs_.SetInteger(prefs::kProfileAvatarIndex, 999);
  prefs_.SetBoolean(prefs::kProfileUsingDefaultAvatar, false);
  MigrateObsoleteProfilePrefs(&prefs_, profile_dir.GetPath());
  EXPECT_EQ(prefs_.GetInteger(prefs::kProfileAvatarIndex), 999);
  EXPECT_FALSE(prefs_.GetBoolean(prefs::kProfileUsingDefaultAvatar));

  // Unset pref (default -1) stays unset.
  prefs_.ClearPref(prefs::kProfileAvatarIndex);
  prefs_.ClearPref(prefs::kProfileUsingDefaultAvatar);
  prefs_.ClearPref(prefs::kProfileUsingGAIAAvatar);
  MigrateObsoleteProfilePrefs(&prefs_, profile_dir.GetPath());
  EXPECT_FALSE(prefs_.HasPrefPath(prefs::kProfileAvatarIndex));
  EXPECT_FALSE(prefs_.HasPrefPath(prefs::kProfileUsingDefaultAvatar));
}
#endif  // BUILDFLAG(IS_ANDROID)
