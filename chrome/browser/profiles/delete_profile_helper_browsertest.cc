// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/delete_profile_helper.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/callback_helpers.h"
#include "base/json/values_util.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_waiter.h"
#include "chrome/browser/profiles/nuke_profile_directory_utils.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/keep_alive_registry/keep_alive_registry.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_launcher.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

void WaitForPrefToBeEmpty(PrefService* prefs, const std::string& pref_name) {
  if (prefs->GetList(pref_name).empty()) {
    return;
  }

  base::RunLoop run_loop;
  PrefChangeRegistrar registrar;
  registrar.Init(prefs);
  registrar.Add(pref_name, base::BindLambdaForTesting([&]() {
                  if (prefs->GetList(pref_name).empty()) {
                    run_loop.Quit();
                  }
                }));
  run_loop.Run();
}

}  // namespace

class DeleteProfileHelperBrowserTest : public InProcessBrowserTest {
 public:
  DeleteProfileHelperBrowserTest() = default;
  ~DeleteProfileHelperBrowserTest() override = default;

  base::FilePath GetTestProfilePath() {
    return g_browser_process->profile_manager()->user_data_dir().AppendASCII(
        "TestProfile");
  }
};

IN_PROC_BROWSER_TEST_F(DeleteProfileHelperBrowserTest, KeepAlive) {
  // Create an additional profile.
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  base::FilePath profile_path_to_delete =
      profile_manager->GenerateNextProfileDirectoryPath();
  Profile& profile_to_delete = profiles::testing::CreateProfileSync(
      profile_manager, profile_path_to_delete);
  EXPECT_TRUE(profile_manager->GetProfileAttributesStorage()
                  .GetProfileAttributesWithPath(profile_path_to_delete));
  // Set the profile as last-used, so that the callback of
  // `MaybeScheduleProfileForDeletion()` is called.
  Browser::Create(Browser::CreateParams(&profile_to_delete, true));
  profiles::SetLastUsedProfile(profile_path_to_delete.BaseName());
  // Schedule profile deletion.
  ProfileKeepAliveAddedWaiter keep_alive_added_waiter(
      &profile_to_delete, ProfileKeepAliveOrigin::kProfileDeletionProcess);
  base::RunLoop loop;
  profile_manager->GetDeleteProfileHelper().MaybeScheduleProfileForDeletion(
      profile_path_to_delete,
      base::BindLambdaForTesting([&loop, &profile_path_to_delete,
                                  profile_manager,
                                  &profile_to_delete](Profile* profile) {
        // `profile` is the new active profile.
        EXPECT_NE(&profile_to_delete, profile);
        // There is an active `ScopedKeepAlive`.
        EXPECT_TRUE(KeepAliveRegistry::GetInstance()->IsOriginRegistered(
            KeepAliveOrigin::PROFILE_MANAGER));
        // The profile has been deleted.
        EXPECT_FALSE(profile_manager->GetProfileAttributesStorage()
                         .GetProfileAttributesWithPath(profile_path_to_delete));
        loop.Quit();
      }),
      ProfileMetrics::DELETE_PROFILE_PRIMARY_ACCOUNT_NOT_ALLOWED);
  // Check that kProfileDeletionProcess was added.
  keep_alive_added_waiter.Wait();
  loop.Run();
  // The `ScopedKeepAlive` has been released.
  EXPECT_FALSE(KeepAliveRegistry::GetInstance()->IsOriginRegistered(
      KeepAliveOrigin::PROFILE_MANAGER));
}

// Tests that a profile marked for deletion is correctly cleaned up on startup
// via the ephemeral mechanism.
IN_PROC_BROWSER_TEST_F(DeleteProfileHelperBrowserTest,
                       PRE_CleanUpEphemeralProfilesTest) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  base::FilePath profile_path = GetTestProfilePath();
  profiles::testing::CreateProfileSync(profile_manager, profile_path);

  MarkProfileDirectoryForDeletion(profile_path);

  ProfileAttributesEntry* entry =
      profile_manager->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile_path);
  ASSERT_NE(entry, nullptr);
  EXPECT_TRUE(entry->IsEphemeral());
  EXPECT_TRUE(entry->IsOmitted());

  const base::ListValue& deleted_profiles =
      g_browser_process->local_state()->GetList(prefs::kProfilesDeleted);
  ASSERT_EQ(deleted_profiles.size(), 1u);
  const base::Value& value = deleted_profiles[0];
  EXPECT_EQ(base::ValueToFilePath(value), profile_path.BaseName());
}

IN_PROC_BROWSER_TEST_F(DeleteProfileHelperBrowserTest,
                       CleanUpEphemeralProfilesTest) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();

  // Cleanup happens during startup in PreProfileInit, but it might be
  // asynchronous.
  WaitForPrefToBeEmpty(g_browser_process->local_state(),
                       prefs::kProfilesDeleted);

  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_FALSE(base::PathExists(GetTestProfilePath()));
  }

  for (auto* entry : profile_manager->GetProfileAttributesStorage()
                         .GetAllProfilesAttributes()) {
    EXPECT_FALSE(entry->IsEphemeral());
  }
}

// Tests that a profile marked for deletion but removed from storage is
// correctly cleaned up on startup via the kProfilesDeleted preference.
IN_PROC_BROWSER_TEST_F(DeleteProfileHelperBrowserTest,
                       PRE_CleanUpDeletedProfilesTest) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  base::FilePath profile_path = GetTestProfilePath();
  profiles::testing::CreateProfileSync(profile_manager, profile_path);

  MarkProfileDirectoryForDeletion(profile_path);
  profile_manager->GetProfileAttributesStorage().RemoveProfile(profile_path);

  EXPECT_EQ(profile_manager->GetProfileAttributesStorage()
                .GetProfileAttributesWithPath(profile_path),
            nullptr);

  const base::ListValue& deleted_profiles =
      g_browser_process->local_state()->GetList(prefs::kProfilesDeleted);
  ASSERT_EQ(deleted_profiles.size(), 1u);
  const base::Value& value = deleted_profiles[0];
  EXPECT_EQ(base::ValueToFilePath(value), profile_path.BaseName());
}

IN_PROC_BROWSER_TEST_F(DeleteProfileHelperBrowserTest,
                       CleanUpDeletedProfilesTest) {
  WaitForPrefToBeEmpty(g_browser_process->local_state(),
                       prefs::kProfilesDeleted);

  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_FALSE(base::PathExists(GetTestProfilePath()));
  }
}
