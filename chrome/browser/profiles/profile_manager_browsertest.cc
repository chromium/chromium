// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/platform_apps/shortcut_manager.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_keep_alive_types.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/profiles/scoped_profile_keep_alive.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/browser/password_store_consumer.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_switches.h"
#include "base/path_service.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "testing/gtest/include/gtest/gtest.h"
#endif

namespace {

const ProfileManager::CreateCallback kOnProfileSwitchDoNothing;

// An observer that returns back to test code after a new profile is
// initialized.
void OnUnblockOnProfileCreation(base::RunLoop* run_loop,
                                Profile* profile,
                                Profile::CreateStatus status) {
  if (status == Profile::CREATE_STATUS_INITIALIZED)
    run_loop->Quit();
}

void ProfileCreationComplete(base::OnceClosure completion_callback,
                             Profile* profile,
                             Profile::CreateStatus status) {
  ASSERT_NE(status, Profile::CREATE_STATUS_LOCAL_FAIL);
  ASSERT_NE(status, Profile::CREATE_STATUS_REMOTE_FAIL);
  // No browser should have been created for this profile yet.
  EXPECT_EQ(chrome::GetBrowserCount(profile), 0U);
  EXPECT_EQ(chrome::GetTotalBrowserCount(), 1U);
  if (status == Profile::CREATE_STATUS_INITIALIZED)
    std::move(completion_callback).Run();
}

// An observer that returns back to test code after one or more profiles was
// deleted. It also creates ScopedKeepAlive and ScopedProfileKeepAlive objects
// to prevent browser shutdown started in case browser has become windowless.
class MultipleProfileDeletionObserver
    : public ProfileAttributesStorage::Observer {
 public:
  explicit MultipleProfileDeletionObserver(size_t expected_count)
      : expected_count_(expected_count),
        profiles_removed_count_(0),
        profiles_data_removed_count_(0) {
    EXPECT_GT(expected_count_, 0u);
    ProfileManager* profile_manager = g_browser_process->profile_manager();
    profile_manager->GetProfileAttributesStorage().AddObserver(this);

    base::RepeatingCallback<void(base::OnceClosure)> would_complete_callback =
        base::BindRepeating(&MultipleProfileDeletionObserver::
                                OnBrowsingDataRemoverWouldComplete,
                            base::Unretained(this));
    for (Profile* profile : profile_manager->GetLoadedProfiles()) {
      content::BrowserContext::GetBrowsingDataRemover(profile)
          ->SetWouldCompleteCallbackForTesting(would_complete_callback);
    }
  }

  ~MultipleProfileDeletionObserver() override {
    g_browser_process->profile_manager()->GetProfileAttributesStorage().
        RemoveObserver(this);
  }

  void Wait() {
    keep_alive_ = std::make_unique<ScopedKeepAlive>(
        KeepAliveOrigin::PROFILE_HELPER, KeepAliveRestartOption::DISABLED);
    loop_.Run();
  }

 private:
  void OnProfileWillBeRemoved(const base::FilePath& profile_path) override {
    profiles_removed_count_++;
    MaybeQuit();
  }

  // TODO(https://crbug.com/704601): remove this code when bug is fixed.
  void OnBrowsingDataRemoverWouldComplete(
      base::OnceClosure continue_to_completion) {
    std::move(continue_to_completion).Run();
    profiles_data_removed_count_++;
    MaybeQuit();
  }

  void MaybeQuit() {
    DLOG(INFO) << profiles_removed_count_ << " profiles removed, and "
               << profiles_data_removed_count_
               << " profile data removed of expected " << expected_count_;
    if (profiles_removed_count_ < expected_count_ ||
        profiles_data_removed_count_ < expected_count_)
      return;

    EXPECT_EQ(expected_count_, profiles_removed_count_);
    EXPECT_EQ(expected_count_, profiles_data_removed_count_);

    keep_alive_.reset();
    loop_.Quit();
  }

  base::RunLoop loop_;
  std::unique_ptr<ScopedKeepAlive> keep_alive_;
  size_t expected_count_;
  size_t profiles_removed_count_;
  size_t profiles_data_removed_count_;

  DISALLOW_COPY_AND_ASSIGN(MultipleProfileDeletionObserver);
};

void EphemeralProfileCreationComplete(base::OnceClosure completion_callback,
                                      Profile* profile,
                                      Profile::CreateStatus status) {
  if (status == Profile::CREATE_STATUS_INITIALIZED)
    profile->GetPrefs()->SetBoolean(prefs::kForceEphemeralProfiles, true);
  ProfileCreationComplete(std::move(completion_callback), profile, status);
}

class ProfileRemovalObserver : public ProfileAttributesStorage::Observer {
 public:
  ProfileRemovalObserver() {
    g_browser_process->profile_manager()->GetProfileAttributesStorage().
        AddObserver(this);
  }

  ~ProfileRemovalObserver() override {
    g_browser_process->profile_manager()->GetProfileAttributesStorage().
        RemoveObserver(this);
  }

  std::string last_used_profile_name() { return last_used_profile_name_; }

  // ProfileAttributesStorage::Observer overrides:
  void OnProfileWillBeRemoved(const base::FilePath& profile_path) override {
    last_used_profile_name_ = g_browser_process->local_state()->GetString(
        prefs::kProfileLastUsed);
  }

 private:
  std::string last_used_profile_name_;

  DISALLOW_COPY_AND_ASSIGN(ProfileRemovalObserver);
};

// The class serves to retrieve passwords from PasswordStore asynchronously. It
// used by ProfileManagerBrowserTest.DeletePasswords on some platforms.
class PasswordStoreConsumerVerifier
    : public password_manager::PasswordStoreConsumer {
 public:
  void OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<password_manager::PasswordForm>> results)
      override {
    password_entries_.swap(results);
    run_loop_.Quit();
  }

  void Wait() {
    run_loop_.Run();
  }

  const std::vector<std::unique_ptr<password_manager::PasswordForm>>&
  GetPasswords() const {
    return password_entries_;
  }

 private:
  base::RunLoop run_loop_;
  std::vector<std::unique_ptr<password_manager::PasswordForm>>
      password_entries_;
};

base::FilePath GetFirstNonSigninNonLockScreenAppProfile(
    ProfileAttributesStorage* storage) {
  std::vector<ProfileAttributesEntry*> entries =
      storage->GetAllProfilesAttributesSortedByName();
#if BUILDFLAG(IS_CHROMEOS_ASH)
  const base::FilePath signin_path =
      chromeos::ProfileHelper::GetSigninProfileDir();
  const base::FilePath lock_screen_apps_path =
      chromeos::ProfileHelper::GetLockScreenAppProfilePath();

  for (ProfileAttributesEntry* entry : entries) {
    base::FilePath profile_path = entry->GetPath();
    if (profile_path != signin_path && profile_path != lock_screen_apps_path) {
      return profile_path;
    }
  }
  return base::FilePath();
#else
  return entries.front()->GetPath();
#endif
}

}  // namespace

// This file contains tests for the ProfileManager that require a heavyweight
// InProcessBrowserTest.  These include tests involving profile deletion.
class ProfileManagerBrowserTest : public InProcessBrowserTest,
                                  public testing::WithParamInterface<bool> {
 protected:
  ProfileManagerBrowserTest() {
    bool enable_destroy_profile = GetParam();
    if (enable_destroy_profile) {
      feature_list_.InitAndEnableFeature(
          features::kDestroyProfileOnBrowserClose);
    } else {
      feature_list_.InitAndDisableFeature(
          features::kDestroyProfileOnBrowserClose);
    }
  }

  void SetUp() override {
    // Shortcut deletion delays tests shutdown on Win-7 and results in time out.
    // See crbug.com/1073451.
#if defined(OS_WIN)
    AppShortcutManager::SuppressShortcutsForTesting();
#endif
    InProcessBrowserTest::SetUp();
  }
  void SetUpCommandLine(base::CommandLine* command_line) override {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    command_line->AppendSwitch(
        chromeos::switches::kIgnoreUserProfileMappingForTests);
#endif
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// CrOS multi-profiles implementation is too different for these tests.
#if !BUILDFLAG(IS_CHROMEOS_ASH)

IN_PROC_BROWSER_TEST_P(ProfileManagerBrowserTest, DeleteSingletonProfile) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ProfileAttributesStorage& storage =
      profile_manager->GetProfileAttributesStorage();
  ProfileRemovalObserver observer;

  // We should start out with 1 profile.
  ASSERT_EQ(1u, storage.GetNumberOfProfiles());

  // Delete singleton profile.
  base::FilePath singleton_profile_path =
      storage.GetAllProfilesAttributes().front()->GetPath();
  EXPECT_FALSE(singleton_profile_path.empty());
  MultipleProfileDeletionObserver profile_deletion_observer(1u);
  profile_manager->ScheduleProfileForDeletion(singleton_profile_path,
                                              base::DoNothing());

  // Run the message loop until the profile is actually deleted (as indicated
  // by the callback above being called).
  profile_deletion_observer.Wait();

  // Make sure a new profile was created automatically.
  EXPECT_EQ(1u, storage.GetNumberOfProfiles());
  base::FilePath new_profile_path =
      storage.GetAllProfilesAttributes().front()->GetPath();
  EXPECT_NE(new_profile_path.value(), singleton_profile_path.value());

  // Make sure that last used profile preference is set correctly.
  Profile* last_used = ProfileManager::GetLastUsedProfile();
  EXPECT_EQ(new_profile_path.value(), last_used->GetPath().value());

  // Make sure the last used profile was set correctly before the notification
  // was sent.
  std::string last_used_profile_name =
      last_used->GetPath().BaseName().MaybeAsASCII();
  EXPECT_EQ(last_used_profile_name, observer.last_used_profile_name());
}

// Delete inactive profile in a multi profile setup and make sure current
// browser is not affected.
IN_PROC_BROWSER_TEST_P(ProfileManagerBrowserTest, DeleteInactiveProfile) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ProfileAttributesStorage& storage =
      profile_manager->GetProfileAttributesStorage();
  base::FilePath current_profile_path = browser()->profile()->GetPath();

  // Create an additional profile.
  base::FilePath new_path = profile_manager->GenerateNextProfileDirectoryPath();
  base::RunLoop run_loop;
  profile_manager->CreateProfileAsync(
      new_path, base::BindRepeating(&OnUnblockOnProfileCreation, &run_loop));
  run_loop.Run();

  ASSERT_EQ(2u, storage.GetNumberOfProfiles());

  // Delete inactive profile.
  MultipleProfileDeletionObserver profile_deletion_observer(1u);
  profile_manager->ScheduleProfileForDeletion(new_path, base::DoNothing());
  profile_deletion_observer.Wait();

  // Make sure there only preexisted profile left.
  EXPECT_EQ(1u, storage.GetNumberOfProfiles());
  EXPECT_EQ(current_profile_path,
            storage.GetAllProfilesAttributes().front()->GetPath());

  // Make sure that last used profile preference is set correctly.
  Profile* last_used = ProfileManager::GetLastUsedProfile();
  EXPECT_EQ(current_profile_path, last_used->GetPath());
}

IN_PROC_BROWSER_TEST_P(ProfileManagerBrowserTest, DeleteCurrentProfile) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ProfileAttributesStorage& storage =
      profile_manager->GetProfileAttributesStorage();

  // Create an additional profile.
  base::FilePath new_path = profile_manager->GenerateNextProfileDirectoryPath();
  base::RunLoop run_loop;
  profile_manager->CreateProfileAsync(
      new_path, base::BindRepeating(&OnUnblockOnProfileCreation, &run_loop));
  run_loop.Run();

  ASSERT_EQ(2u, storage.GetNumberOfProfiles());

  // Delete current profile.
  MultipleProfileDeletionObserver profile_deletion_observer(1u);
  profile_manager->ScheduleProfileForDeletion(browser()->profile()->GetPath(),
                                              base::DoNothing());
  profile_deletion_observer.Wait();

  // Make sure a profile created earlier become the only profile.
  EXPECT_EQ(1u, storage.GetNumberOfProfiles());
  EXPECT_EQ(new_path, storage.GetAllProfilesAttributes().front()->GetPath());

  // Make sure that last used profile preference is set correctly.
  Profile* last_used = ProfileManager::GetLastUsedProfile();
  EXPECT_EQ(new_path, last_used->GetPath());
}

IN_PROC_BROWSER_TEST_P(ProfileManagerBrowserTest, DeleteAllProfiles) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ProfileAttributesStorage& storage =
      profile_manager->GetProfileAttributesStorage();

  // Create an additional profile.
  base::FilePath new_path = profile_manager->GenerateNextProfileDirectoryPath();
  base::RunLoop run_loop;
  profile_manager->CreateProfileAsync(
      new_path, base::BindRepeating(&OnUnblockOnProfileCreation, &run_loop));

  // Run the message loop to allow profile creation to take place; the loop is
  // terminated by OnUnblockOnProfileCreation when the profile is created.
  run_loop.Run();

  ASSERT_EQ(2u, storage.GetNumberOfProfiles());

  // Delete all profiles.
  MultipleProfileDeletionObserver profile_deletion_observer(2u);
  std::vector<ProfileAttributesEntry*> entries =
      storage.GetAllProfilesAttributes();
  std::vector<base::FilePath> old_profile_paths;
  for (ProfileAttributesEntry* entry : entries) {
    base::FilePath profile_path = entry->GetPath();
    EXPECT_FALSE(profile_path.empty());
    profile_manager->ScheduleProfileForDeletion(profile_path,
                                                base::DoNothing());
    old_profile_paths.push_back(profile_path);
  }
  profile_deletion_observer.Wait();

  // Make sure a new profile was created automatically.
  EXPECT_EQ(1u, storage.GetNumberOfProfiles());
  base::FilePath new_profile_path =
      storage.GetAllProfilesAttributes().front()->GetPath();
  for (const base::FilePath& old_profile_path : old_profile_paths)
    EXPECT_NE(old_profile_path, new_profile_path);

  // Make sure that last used profile preference is set correctly.
  Profile* last_used = ProfileManager::GetLastUsedProfile();
  EXPECT_EQ(new_profile_path, last_used->GetPath());
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

IN_PROC_BROWSER_TEST_P(ProfileManagerBrowserTest, ProfileFromProfileKey) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  Profile* profile1 = browser()->profile();

  // Create an additional profile.
  base::FilePath new_path = profile_manager->GenerateNextProfileDirectoryPath();
  base::RunLoop run_loop;
  profile_manager->CreateProfileAsync(
      new_path, base::BindRepeating(&OnUnblockOnProfileCreation, &run_loop));

  // Run the message loop to allow profile creation to take place; the loop is
  // terminated by OnUnblockOnProfileCreation when the profile is created.
  run_loop.Run();

  Profile* profile2 = profile_manager->GetProfile(new_path);

  EXPECT_NE(profile1, profile2);
  EXPECT_NE(profile1->GetProfileKey(), profile2->GetProfileKey());
  EXPECT_EQ(profile1, profile_manager->GetProfileFromProfileKey(
                          profile1->GetProfileKey()));
  EXPECT_EQ(profile2, profile_manager->GetProfileFromProfileKey(
                          profile2->GetProfileKey()));

  // Create off-the-record profiles.
  Profile* otr_1a = profile1->GetPrimaryOTRProfile();
  Profile* otr_1b =
      profile1->GetOffTheRecordProfile(Profile::OTRProfileID("profile::otr1"));
  Profile* otr_1c =
      profile1->GetOffTheRecordProfile(Profile::OTRProfileID("profile::otr2"));
  Profile* otr_2a = profile2->GetPrimaryOTRProfile();
  Profile* otr_2b =
      profile2->GetOffTheRecordProfile(Profile::OTRProfileID("profile::otr1"));

  EXPECT_EQ(otr_1a,
            profile_manager->GetProfileFromProfileKey(otr_1a->GetProfileKey()));
  EXPECT_EQ(otr_1b,
            profile_manager->GetProfileFromProfileKey(otr_1b->GetProfileKey()));
  EXPECT_EQ(otr_1c,
            profile_manager->GetProfileFromProfileKey(otr_1c->GetProfileKey()));
  EXPECT_EQ(otr_2a,
            profile_manager->GetProfileFromProfileKey(otr_2a->GetProfileKey()));
  EXPECT_EQ(otr_2b,
            profile_manager->GetProfileFromProfileKey(otr_2b->GetProfileKey()));
}

#if BUILDFLAG(IS_CHROMEOS_ASH)

class ProfileManagerCrOSBrowserTest : public ProfileManagerBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Use a user hash other than the default chrome::kTestUserProfileDir
    // so that the prefix case is tested.
    command_line->AppendSwitchASCII(chromeos::switches::kLoginProfile,
                                    "test-user-hash");
  }
};

IN_PROC_BROWSER_TEST_P(ProfileManagerCrOSBrowserTest, GetLastUsedProfile) {
  // Make sure that last used profile is correct.
  Profile* last_used_profile = ProfileManager::GetLastUsedProfile();
  EXPECT_TRUE(last_used_profile != NULL);

  base::FilePath profile_path;
  base::PathService::Get(chrome::DIR_USER_DATA, &profile_path);

  profile_path = profile_path.AppendASCII(
      std::string(chrome::kProfileDirPrefix) + "test-user-hash");
  EXPECT_EQ(profile_path.value(), last_used_profile->GetPath().value());
}

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Times out (http://crbug.com/159002)
IN_PROC_BROWSER_TEST_P(ProfileManagerBrowserTest,
                       DISABLED_CreateProfileWithCallback) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();

  ASSERT_EQ(profile_manager->GetNumberOfProfiles(), 1U);
  EXPECT_EQ(chrome::GetTotalBrowserCount(), 1U);

  // Create a profile, make sure callback is invoked before any callbacks are
  // invoked (so they can do things like sign in the profile, etc).
  base::RunLoop run_loop;
  ProfileManager::CreateMultiProfileAsync(
      u"New Profile",
      /*icon_index=*/0,
      base::BindRepeating(&ProfileCreationComplete,
                          run_loop.QuitWhenIdleClosure()));
  run_loop.Run();
  EXPECT_EQ(profile_manager->GetNumberOfProfiles(), 2U);
  EXPECT_EQ(chrome::GetTotalBrowserCount(), 2U);

  // Now close all browser windows.
  std::vector<Profile*> profiles = profile_manager->GetLoadedProfiles();
  for (std::vector<Profile*>::const_iterator it = profiles.begin();
       it != profiles.end(); ++it) {
    BrowserList::CloseAllBrowsersWithProfile(*it);
  }
}

IN_PROC_BROWSER_TEST_P(ProfileManagerBrowserTest, SwitchToProfile) {
  // If multiprofile mode is not enabled, you can't switch between profiles.
  if (!profiles::IsMultipleProfilesEnabled())
    return;

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ProfileAttributesStorage& storage =
      profile_manager->GetProfileAttributesStorage();
  size_t initial_profile_count = profile_manager->GetNumberOfProfiles();
  base::FilePath path_profile1 =
      GetFirstNonSigninNonLockScreenAppProfile(&storage);

  ASSERT_NE(0U, initial_profile_count);
  EXPECT_EQ(1U, chrome::GetTotalBrowserCount());

  // Create an additional profile.
  base::FilePath path_profile2 =
      profile_manager->GenerateNextProfileDirectoryPath();
  base::RunLoop run_loop;
  profile_manager->CreateProfileAsync(
      path_profile2,
      base::BindRepeating(&OnUnblockOnProfileCreation, &run_loop));

  // Run the message loop to allow profile creation to take place; the loop is
  // terminated by OnUnblockOnProfileCreation when the profile is created.
  run_loop.Run();

  BrowserList* browser_list = BrowserList::GetInstance();
  ASSERT_EQ(initial_profile_count + 1U, storage.GetNumberOfProfiles());
  EXPECT_EQ(1U, browser_list->size());

  // Open a browser window for the first profile.
  profiles::SwitchToProfile(path_profile1, false, kOnProfileSwitchDoNothing);
  EXPECT_EQ(1U, chrome::GetTotalBrowserCount());
  EXPECT_EQ(1U, browser_list->size());
  EXPECT_EQ(path_profile1, browser_list->get(0)->profile()->GetPath());

  // Open a browser window for the second profile.
  profiles::SwitchToProfile(path_profile2, false, kOnProfileSwitchDoNothing);
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(2U, chrome::GetTotalBrowserCount());
  EXPECT_EQ(2U, browser_list->size());
  EXPECT_EQ(path_profile2, browser_list->get(1)->profile()->GetPath());

  // Switch to the first profile without opening a new window.
  profiles::SwitchToProfile(path_profile1, false, kOnProfileSwitchDoNothing);
  EXPECT_EQ(2U, chrome::GetTotalBrowserCount());
  EXPECT_EQ(2U, browser_list->size());

  EXPECT_EQ(path_profile1, browser_list->get(0)->profile()->GetPath());
  EXPECT_EQ(path_profile2, browser_list->get(1)->profile()->GetPath());
}

// Prepares the setup for AddMultipleProfiles test, creates multiple browser
// windows with multiple browser windows.
IN_PROC_BROWSER_TEST_P(ProfileManagerBrowserTest, PRE_AddMultipleProfiles) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();

  ProfileAttributesStorage& storage =
      profile_manager->GetProfileAttributesStorage();
  size_t initial_profile_count = profile_manager->GetNumberOfProfiles();

  base::FilePath path_profile1 =
      GetFirstNonSigninNonLockScreenAppProfile(&storage);
  ASSERT_NE(0U, initial_profile_count);
  EXPECT_EQ(1U, chrome::GetTotalBrowserCount());
  // Create an additional profile.
  base::FilePath path_profile2 =
      profile_manager->GenerateNextProfileDirectoryPath();
  base::RunLoop run_loop;
  profile_manager->CreateProfileAsync(
      path_profile2,
      base::BindRepeating(&OnUnblockOnProfileCreation, &run_loop));
  // Run the message loop to allow profile creation to take place; the loop is
  // terminated by OnUnblockOnProfileCreation when the profile is created.
  run_loop.Run();
  BrowserList* browser_list = BrowserList::GetInstance();
  ASSERT_EQ(initial_profile_count + 1U, storage.GetNumberOfProfiles());
  EXPECT_EQ(1U, browser_list->size());

  // Open a browser window for the first profile.
  profiles::SwitchToProfile(path_profile1, false, kOnProfileSwitchDoNothing);
  EXPECT_EQ(1U, chrome::GetTotalBrowserCount());
  ASSERT_EQ(1U, browser_list->size());
  EXPECT_EQ(path_profile1, browser_list->get(0)->profile()->GetPath());
  // Open a browser window for the second profile.
  profiles::SwitchToProfile(path_profile2, false, kOnProfileSwitchDoNothing);
  EXPECT_EQ(2U, chrome::GetTotalBrowserCount());
  ASSERT_EQ(2U, browser_list->size());
  EXPECT_EQ(path_profile2, browser_list->get(1)->profile()->GetPath());
}

IN_PROC_BROWSER_TEST_P(ProfileManagerBrowserTest, AddMultipleProfiles) {
  // Verifies that the browser doesn't crash when it is restarted.
}

// Flakes on Windows: http://crbug.com/314905
#if defined(OS_WIN)
#define MAYBE_EphemeralProfile DISABLED_EphemeralProfile
#else
#define MAYBE_EphemeralProfile EphemeralProfile
#endif
IN_PROC_BROWSER_TEST_P(ProfileManagerBrowserTest, MAYBE_EphemeralProfile) {
  // If multiprofile mode is not enabled, you can't switch between profiles.
  if (!profiles::IsMultipleProfilesEnabled())
    return;

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ProfileAttributesStorage& storage =
      profile_manager->GetProfileAttributesStorage();
  size_t initial_profile_count = profile_manager->GetNumberOfProfiles();
  base::FilePath path_profile1 =
      GetFirstNonSigninNonLockScreenAppProfile(&storage);

  ASSERT_NE(0U, initial_profile_count);
  EXPECT_EQ(1U, chrome::GetTotalBrowserCount());

  // Create an ephemeral profile.
  base::FilePath path_profile2 =
      profile_manager->GenerateNextProfileDirectoryPath();
  base::RunLoop run_loop;
  profile_manager->CreateProfileAsync(
      path_profile2, base::BindRepeating(&EphemeralProfileCreationComplete,
                                         run_loop.QuitWhenIdleClosure()));
  run_loop.Run();

  BrowserList* browser_list = BrowserList::GetInstance();
  ASSERT_EQ(initial_profile_count + 1U, storage.GetNumberOfProfiles());
  EXPECT_EQ(1U, browser_list->size());

  // Open a browser window for the second profile.
  profiles::SwitchToProfile(path_profile2, false, kOnProfileSwitchDoNothing);
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(2U, chrome::GetTotalBrowserCount());
  EXPECT_EQ(2U, browser_list->size());
  EXPECT_EQ(path_profile2, browser_list->get(1)->profile()->GetPath());

  // Create a second window for the ephemeral profile.
  profiles::SwitchToProfile(path_profile2, true, kOnProfileSwitchDoNothing);
  EXPECT_EQ(3U, chrome::GetTotalBrowserCount());
  EXPECT_EQ(3U, browser_list->size());

  EXPECT_EQ(path_profile1, browser_list->get(0)->profile()->GetPath());
  EXPECT_EQ(path_profile2, browser_list->get(1)->profile()->GetPath());
  EXPECT_EQ(path_profile2, browser_list->get(2)->profile()->GetPath());

  // Closing the first window of the ephemeral profile should not delete it.
  CloseBrowserSynchronously(browser_list->get(2));
  EXPECT_EQ(2U, browser_list->size());
  EXPECT_EQ(initial_profile_count + 1U, storage.GetNumberOfProfiles());

  // The second should though.
  MultipleProfileDeletionObserver observer(1u);
  CloseBrowserSynchronously(browser_list->get(1));
  observer.Wait();

  EXPECT_EQ(1U, browser_list->size());
  EXPECT_EQ(initial_profile_count, storage.GetNumberOfProfiles());

  // TODO(crbug.com/1191455): Once RemoveProfile()/NukeProfileFromDisk() aren't
  // flaky anymore, EXPECT_FALSE(PathExists(path_profile2)).
}

// The test makes sense on those platforms where the keychain exists.
#if !defined(OS_WIN) && !BUILDFLAG(IS_CHROMEOS_ASH)

// Suddenly started failing on Linux, see http://crbug.com/660488.
// TODO(crbug.com/1052397): Revisit once build flag switch of lacros-chrome is
// complete.
#if defined(OS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
#define MAYBE_DeletePasswords DISABLED_DeletePasswords
#else
#define MAYBE_DeletePasswords DeletePasswords
#endif

IN_PROC_BROWSER_TEST_P(ProfileManagerBrowserTest, MAYBE_DeletePasswords) {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  ASSERT_TRUE(profile);

  password_manager::PasswordForm form;
  form.scheme = password_manager::PasswordForm::Scheme::kHtml;
  form.url = GURL("http://accounts.google.com/LoginAuth");
  form.signon_realm = "http://accounts.google.com/";
  form.username_value = u"my_username";
  form.password_value = u"my_password";
  form.blocked_by_user = false;

  scoped_refptr<password_manager::PasswordStore> password_store =
      PasswordStoreFactory::GetForProfile(
          profile, ServiceAccessType::EXPLICIT_ACCESS).get();
  ASSERT_TRUE(password_store.get());

  password_store->AddLogin(form);
  PasswordStoreConsumerVerifier verify_add;
  password_store->GetAutofillableLogins(&verify_add);
  verify_add.Wait();
  EXPECT_EQ(1u, verify_add.GetPasswords().size());

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  base::RunLoop run_loop;
  profile_manager->ScheduleProfileForDeletion(
      profile->GetPath(),
      base::BindLambdaForTesting([&run_loop](Profile*) { run_loop.Quit(); }));
  run_loop.Run();

  PasswordStoreConsumerVerifier verify_delete;
  password_store->GetAutofillableLogins(&verify_delete);
  verify_delete.Wait();
  EXPECT_EQ(0u, verify_delete.GetPasswords().size());
}
#endif  // !defined(OS_WIN) && !BUILDFLAG(IS_CHROMEOS_ASH)

// Tests Profile::HasOffTheRecordProfile, Profile::IsValidProfile and the
// profile counts in ProfileManager with respect to the creation and destruction
// of incognito profiles.
IN_PROC_BROWSER_TEST_P(ProfileManagerBrowserTest, IncognitoProfile) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ASSERT_TRUE(profile_manager);

  Profile* profile = ProfileManager::GetActiveUserProfile();
  ASSERT_TRUE(profile);
  EXPECT_FALSE(profile->HasPrimaryOTRProfile());

  size_t initial_profile_count = profile_manager->GetNumberOfProfiles();

  // Create an incognito profile.
  Profile* incognito_profile = profile->GetPrimaryOTRProfile();

  EXPECT_TRUE(profile->HasPrimaryOTRProfile());
  ASSERT_TRUE(profile_manager->IsValidProfile(incognito_profile));
  EXPECT_EQ(initial_profile_count, profile_manager->GetNumberOfProfiles());

  // Check that a default save path is not empty, since it's taken from the
  // main profile preferences, set it to empty and verify that it becomes
  // empty.
  EXPECT_FALSE(profile->GetOffTheRecordPrefs()
                   ->GetFilePath(prefs::kSaveFileDefaultDirectory)
                   .empty());
  profile->GetOffTheRecordPrefs()->SetFilePath(prefs::kSaveFileDefaultDirectory,
                                               base::FilePath());
  EXPECT_TRUE(profile->GetOffTheRecordPrefs()
                  ->GetFilePath(prefs::kSaveFileDefaultDirectory)
                  .empty());

  // Delete the incognito profile.
  incognito_profile->GetOriginalProfile()->DestroyOffTheRecordProfile(
      incognito_profile);

  EXPECT_FALSE(profile->HasPrimaryOTRProfile());
  EXPECT_FALSE(profile_manager->IsValidProfile(incognito_profile));
  EXPECT_EQ(initial_profile_count, profile_manager->GetNumberOfProfiles());
  // After destroying the incognito profile incognito preferences should be
  // cleared so the default save path should be taken from the main profile.
  EXPECT_FALSE(profile->GetOffTheRecordPrefs()
                   ->GetFilePath(prefs::kSaveFileDefaultDirectory)
                   .empty());
}

#if !defined(OS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)

class ProfileManagerEphemeralGuestProfileBrowserTest
    : public ProfileManagerBrowserTest {
 public:
  ProfileManagerEphemeralGuestProfileBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kEnableEphemeralGuestProfilesOnDesktop);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(ProfileManagerEphemeralGuestProfileBrowserTest,
                       PRE_CleanUpEphemeralProfilesOnStartup) {
  // If multiprofile mode is not enabled, you can't switch between profiles.
  if (!profiles::IsMultipleProfilesEnabled())
    return;

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ProfileAttributesStorage& storage =
      profile_manager->GetProfileAttributesStorage();
  EXPECT_EQ(1u, storage.GetNumberOfProfiles(true));

  // Create an ephemeral profile.
  base::FilePath ephemeral_profile_path =
      profile_manager->GenerateNextProfileDirectoryPath();
  base::RunLoop run_loop;
  profile_manager->CreateProfileAsync(
      ephemeral_profile_path,
      base::BindRepeating(&EphemeralProfileCreationComplete,
                          run_loop.QuitWhenIdleClosure()));
  run_loop.Run();

  // Create an ephemeral guest profile.
  base::FilePath guest_path = profile_manager->GetGuestProfilePath();
  base::RunLoop run_loop2;
  profile_manager->CreateProfileAsync(
      guest_path, base::BindRepeating(&ProfileCreationComplete,
                                      run_loop2.QuitWhenIdleClosure()));
  run_loop2.Run();
  Profile* guest = profile_manager->GetProfileByPath(guest_path);
  EXPECT_TRUE(guest->IsEphemeralGuestProfile());

  BrowserList* browser_list = BrowserList::GetInstance();
  ASSERT_EQ(3U, storage.GetNumberOfProfiles(true));
  EXPECT_EQ(1U, browser_list->size());

  // Do not open browser windows for ephemeral profiles so that they don't
  // get deleted in this session.
}

IN_PROC_BROWSER_TEST_P(ProfileManagerEphemeralGuestProfileBrowserTest,
                       CleanUpEphemeralProfilesOnStartup) {
  // If multiprofile mode is not enabled, you can't switch between profiles.
  if (!profiles::IsMultipleProfilesEnabled())
    return;

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ProfileAttributesStorage& storage =
      profile_manager->GetProfileAttributesStorage();
  // Check that ephemeral profiles got deleted.
  EXPECT_EQ(1u, storage.GetNumberOfProfiles(true));
}

class EphemeralGuestProfilePolicyTest
    : public policy::PolicyTest,
      public ::testing::WithParamInterface<bool> {
 public:
  EphemeralGuestProfilePolicyTest() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kEnableEphemeralGuestProfilesOnDesktop);
  }

 protected:
  void SetUp() override {
    // Shortcut deletion delays tests shutdown on Win-7 and results in time out.
    // See crbug.com/1073451.
#if defined(OS_WIN)
    AppShortcutManager::SuppressShortcutsForTesting();
#endif
    InProcessBrowserTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// TODO(https://crbug.com/1125474): Remove this comment!
// If this test times out on Windows7 (or flaky on Windows 10), please disable
// it and assign the bug to rhalavati@.
IN_PROC_BROWSER_TEST_P(EphemeralGuestProfilePolicyTest,
                       TestsForceEphemeralProfilesPolicy) {
  policy::PolicyMap policies;
  SetPolicy(&policies, policy::key::kForceEphemeralProfiles,
            base::Value(GetParam()));
  UpdateProviderPolicy(policies);

  Profile* guest = CreateGuestBrowser()->profile();
  EXPECT_TRUE(guest->IsEphemeralGuestProfile());

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ProfileAttributesEntry* entry =
      profile_manager->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(guest->GetPath());
  ASSERT_NE(entry, nullptr);
  EXPECT_TRUE(entry->IsGuest());
  EXPECT_TRUE(entry->IsEphemeral());
  EXPECT_TRUE(entry->IsOmitted());
}

INSTANTIATE_TEST_SUITE_P(AllGuestProfileTypes,
                         EphemeralGuestProfilePolicyTest,
                         /*policy_is_enforced=*/testing::Bool());

INSTANTIATE_TEST_SUITE_P(DestroyProfileOnBrowserClose,
                         ProfileManagerEphemeralGuestProfileBrowserTest,
                         testing::Bool());

#endif  // !defined(OS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_ASH)
INSTANTIATE_TEST_SUITE_P(DestroyProfileOnBrowserClose,
                         ProfileManagerBrowserTest,
                         testing::Values(false));

INSTANTIATE_TEST_SUITE_P(DestroyProfileOnBrowserClose,
                         ProfileManagerCrOSBrowserTest,
                         testing::Bool());
#else
INSTANTIATE_TEST_SUITE_P(DestroyProfileOnBrowserClose,
                         ProfileManagerBrowserTest,
                         testing::Bool());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
