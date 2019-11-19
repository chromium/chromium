// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "build/build_config.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/autofill/core/common/password_form.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/browser/password_store_consumer.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/test/test_utils.h"

#if defined(OS_CHROMEOS)
#include "base/path_service.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chromeos/constants/chromeos_switches.h"
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

void ProfileCreationComplete(Profile* profile, Profile::CreateStatus status) {
  ASSERT_NE(status, Profile::CREATE_STATUS_LOCAL_FAIL);
  ASSERT_NE(status, Profile::CREATE_STATUS_REMOTE_FAIL);
  // No browser should have been created for this profile yet.
  EXPECT_EQ(chrome::GetBrowserCount(profile), 0U);
  EXPECT_EQ(chrome::GetTotalBrowserCount(), 1U);
  if (status == Profile::CREATE_STATUS_INITIALIZED)
    base::RunLoop::QuitCurrentWhenIdleDeprecated();
}

// An observer that returns back to test code after one or more profiles was
// deleted. It also create ScopedKeepAlive object to prevent browser shutdown
// started in case browser has become windowless.
class MultipleProfileDeletionObserver
    : public ProfileAttributesStorage::Observer {
 public:
  explicit MultipleProfileDeletionObserver(size_t expected_count)
      : expected_count_(expected_count),
        profiles_created_count_(0),
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
    profiles_created_count_++;
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
    DLOG(INFO) << profiles_created_count_
               << " profiles removed, and "
               << profiles_data_removed_count_
               << " profile data removed of expected "
               << expected_count_;
    if (profiles_created_count_ < expected_count_ ||
        profiles_data_removed_count_ < expected_count_)
      return;

    EXPECT_EQ(expected_count_, profiles_created_count_);
    EXPECT_EQ(expected_count_, profiles_data_removed_count_);

    keep_alive_.reset();
    loop_.Quit();
  }

  base::RunLoop loop_;
  std::unique_ptr<ScopedKeepAlive> keep_alive_;
  size_t expected_count_;
  size_t profiles_created_count_;
  size_t profiles_data_removed_count_;

  DISALLOW_COPY_AND_ASSIGN(MultipleProfileDeletionObserver);
};

void EphemeralProfileCreationComplete(Profile* profile,
                                      Profile::CreateStatus status) {
  if (status == Profile::CREATE_STATUS_INITIALIZED)
    profile->GetPrefs()->SetBoolean(prefs::kForceEphemeralProfiles, true);
  ProfileCreationComplete(profile, status);
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
      std::vector<std::unique_ptr<autofill::PasswordForm>> results) override {
    password_entries_.swap(results);
    run_loop_.Quit();
  }

  void Wait() {
    run_loop_.Run();
  }

  const std::vector<std::unique_ptr<autofill::PasswordForm>>& GetPasswords()
      const {
    return password_entries_;
  }

 private:
  base::RunLoop run_loop_;
  std::vector<std::unique_ptr<autofill::PasswordForm>> password_entries_;
};

base::FilePath GetFirstNonSigninNonLockScreenAppProfile(
    ProfileAttributesStorage* storage) {
  std::vector<ProfileAttributesEntry*> entries =
      storage->GetAllProfilesAttributesSortedByName();
#if defined(OS_CHROMEOS)
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

// TODO(jeremy): crbug.com/103355 - These tests should be enabled on all
// platforms.
class ProfileManagerBrowserTest : public InProcessBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
#if defined(OS_CHROMEOS)
    command_line->AppendSwitch(
        chromeos::switches::kIgnoreUserProfileMappingForTests);
#endif
  }
};

// CrOS multi-profiles implementation is too different for these tests.
#if !defined(OS_CHROMEOS)

// Delete single profile and make sure a new one is created.
IN_PROC_BROWSER_TEST_F(ProfileManagerBrowserTest, DeleteSingletonProfile) {
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
IN_PROC_BROWSER_TEST_F(ProfileManagerBrowserTest, DeleteInactiveProfile) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ProfileAttributesStorage& storage =
      profile_manager->GetProfileAttributesStorage();
  base::FilePath current_profile_path = browser()->profile()->GetPath();

  // Create an additional profile.
  base::FilePath new_path = profile_manager->GenerateNextProfileDirectoryPath();
  base::RunLoop run_loop;
  profile_manager->CreateProfileAsync(
      new_path, base::Bind(&OnUnblockOnProfileCreation, &run_loop),
      base::string16(), std::string());
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

// Delete current profile in a multi profile setup and make sure an existing one
// is loaded.
IN_PROC_BROWSER_TEST_F(ProfileManagerBrowserTest, DeleteCurrentProfile) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ProfileAttributesStorage& storage =
      profile_manager->GetProfileAttributesStorage();

  // Create an additional profile.
  base::FilePath new_path = profile_manager->GenerateNextProfileDirectoryPath();
  base::RunLoop run_loop;
  profile_manager->CreateProfileAsync(
      new_path, base::Bind(&OnUnblockOnProfileCreation, &run_loop),
      base::string16(), std::string());
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

// Delete all profiles in a multi profile setup and make sure a new one is
// created.
IN_PROC_BROWSER_TEST_F(ProfileManagerBrowserTest, DeleteAllProfiles) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ProfileAttributesStorage& storage =
      profile_manager->GetProfileAttributesStorage();

  // Create an additional profile.
  base::FilePath new_path = profile_manager->GenerateNextProfileDirectoryPath();
  base::RunLoop run_loop;
  profile_manager->CreateProfileAsync(
      new_path, base::Bind(&OnUnblockOnProfileCreation, &run_loop),
      base::string16(), std::string());

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
#endif  // !defined(OS_CHROMEOS)

#if defined(OS_CHROMEOS)

class ProfileManagerCrOSBrowserTest : public ProfileManagerBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Use a user hash other than the default chrome::kTestUserProfileDir
    // so that the prefix case is tested.
    command_line->AppendSwitchASCII(chromeos::switches::kLoginProfile,
                                    "test-user-hash");
  }
};

IN_PROC_BROWSER_TEST_F(ProfileManagerCrOSBrowserTest, GetLastUsedProfile) {
  // Make sure that last used profile is correct.
  Profile* last_used_profile = ProfileManager::GetLastUsedProfile();
  EXPECT_TRUE(last_used_profile != NULL);

  base::FilePath profile_path;
  base::PathService::Get(chrome::DIR_USER_DATA, &profile_path);

  profile_path = profile_path.AppendASCII(
      std::string(chrome::kProfileDirPrefix) + "test-user-hash");
  EXPECT_EQ(profile_path.value(), last_used_profile->GetPath().value());
}

#endif  // OS_CHROMEOS

// Times out (http://crbug.com/159002)
IN_PROC_BROWSER_TEST_F(ProfileManagerBrowserTest,
                       DISABLED_CreateProfileWithCallback) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();

  ASSERT_EQ(profile_manager->GetNumberOfProfiles(), 1U);
  EXPECT_EQ(chrome::GetTotalBrowserCount(), 1U);

  // Create a profile, make sure callback is invoked before any callbacks are
  // invoked (so they can do things like sign in the profile, etc).
  ProfileManager::CreateMultiProfileAsync(base::string16(),  // name
                                          std::string(),     // icon url
                                          base::Bind(ProfileCreationComplete));
  // Wait for profile to finish loading.
  content::RunMessageLoop();
  EXPECT_EQ(profile_manager->GetNumberOfProfiles(), 2U);
  EXPECT_EQ(chrome::GetTotalBrowserCount(), 2U);

  // Now close all browser windows.
  std::vector<Profile*> profiles = profile_manager->GetLoadedProfiles();
  for (std::vector<Profile*>::const_iterator it = profiles.begin();
       it != profiles.end(); ++it) {
    BrowserList::CloseAllBrowsersWithProfile(*it);
  }
}

IN_PROC_BROWSER_TEST_F(ProfileManagerBrowserTest, SwitchToProfile) {
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
      path_profile2, base::Bind(&OnUnblockOnProfileCreation, &run_loop),
      base::string16(), std::string());

  // Run the message loop to allow profile creation to take place; the loop is
  // terminated by OnUnblockOnProfileCreation when the profile is created.
  run_loop.Run();

  BrowserList* browser_list = BrowserList::GetInstance();
  ASSERT_EQ(initial_profile_count + 1U, storage.GetNumberOfProfiles());
  EXPECT_EQ(1U, browser_list->size());

  // Open a browser window for the first profile.
  profiles::SwitchToProfile(path_profile1, false, kOnProfileSwitchDoNothing,
                            ProfileMetrics::SWITCH_PROFILE_ICON);
  EXPECT_EQ(1U, chrome::GetTotalBrowserCount());
  EXPECT_EQ(1U, browser_list->size());
  EXPECT_EQ(path_profile1, browser_list->get(0)->profile()->GetPath());

  // Open a browser window for the second profile.
  profiles::SwitchToProfile(path_profile2, false, kOnProfileSwitchDoNothing,
                            ProfileMetrics::SWITCH_PROFILE_ICON);
  EXPECT_EQ(2U, chrome::GetTotalBrowserCount());
  EXPECT_EQ(2U, browser_list->size());
  EXPECT_EQ(path_profile2, browser_list->get(1)->profile()->GetPath());

  // Switch to the first profile without opening a new window.
  profiles::SwitchToProfile(path_profile1, false, kOnProfileSwitchDoNothing,
                            ProfileMetrics::SWITCH_PROFILE_ICON);
  EXPECT_EQ(2U, chrome::GetTotalBrowserCount());
  EXPECT_EQ(2U, browser_list->size());

  EXPECT_EQ(path_profile1, browser_list->get(0)->profile()->GetPath());
  EXPECT_EQ(path_profile2, browser_list->get(1)->profile()->GetPath());
}

// Flakes on Windows: http://crbug.com/314905
#if defined(OS_WIN)
#define MAYBE_EphemeralProfile DISABLED_EphemeralProfile
#else
#define MAYBE_EphemeralProfile EphemeralProfile
#endif
IN_PROC_BROWSER_TEST_F(ProfileManagerBrowserTest, MAYBE_EphemeralProfile) {
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
  profile_manager->CreateProfileAsync(
      path_profile2, base::Bind(&EphemeralProfileCreationComplete),
      base::string16(), std::string());

  // Spin to allow profile creation to take place.
  content::RunMessageLoop();

  BrowserList* browser_list = BrowserList::GetInstance();
  ASSERT_EQ(initial_profile_count + 1U, storage.GetNumberOfProfiles());
  EXPECT_EQ(1U, browser_list->size());

  // Open a browser window for the second profile.
  profiles::SwitchToProfile(path_profile2, false, kOnProfileSwitchDoNothing,
                            ProfileMetrics::SWITCH_PROFILE_ICON);
  EXPECT_EQ(2U, chrome::GetTotalBrowserCount());
  EXPECT_EQ(2U, browser_list->size());
  EXPECT_EQ(path_profile2, browser_list->get(1)->profile()->GetPath());

  // Create a second window for the ephemeral profile.
  profiles::SwitchToProfile(path_profile2, true, kOnProfileSwitchDoNothing,
                            ProfileMetrics::SWITCH_PROFILE_ICON);
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
  CloseBrowserSynchronously(browser_list->get(1));
  EXPECT_EQ(1U, browser_list->size());
  EXPECT_EQ(initial_profile_count, storage.GetNumberOfProfiles());
}

// The test makes sense on those platforms where the keychain exists.
#if !defined(OS_WIN) && !defined(OS_CHROMEOS)

// Suddenly started failing on Linux, see http://crbug.com/660488.
#if defined(OS_LINUX)
#define MAYBE_DeletePasswords DISABLED_DeletePasswords
#else
#define MAYBE_DeletePasswords DeletePasswords
#endif

IN_PROC_BROWSER_TEST_F(ProfileManagerBrowserTest, MAYBE_DeletePasswords) {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  ASSERT_TRUE(profile);

  autofill::PasswordForm form;
  form.scheme = autofill::PasswordForm::Scheme::kHtml;
  form.origin = GURL("http://accounts.google.com/LoginAuth");
  form.signon_realm = "http://accounts.google.com/";
  form.username_value = base::ASCIIToUTF16("my_username");
  form.password_value = base::ASCIIToUTF16("my_password");
  form.preferred = true;
  form.blacklisted_by_user = false;

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
#endif  // !defined(OS_WIN) && !defined(OS_CHROMEOS)

// Tests Profile::HasOffTheRecordProfile, Profile::IsValidProfile and the
// profile counts in ProfileManager with respect to the creation and destruction
// of incognito profiles.
IN_PROC_BROWSER_TEST_F(ProfileManagerBrowserTest, IncognitoProfile) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ASSERT_TRUE(profile_manager);

  Profile* profile = ProfileManager::GetActiveUserProfile();
  ASSERT_TRUE(profile);
  EXPECT_FALSE(profile->HasOffTheRecordProfile());

  size_t initial_profile_count = profile_manager->GetNumberOfProfiles();

  // Create an incognito profile.
  Profile* incognito_profile = profile->GetOffTheRecordProfile();

  EXPECT_TRUE(profile->HasOffTheRecordProfile());
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
  incognito_profile->GetOriginalProfile()->DestroyOffTheRecordProfile();

  EXPECT_FALSE(profile->HasOffTheRecordProfile());
  EXPECT_FALSE(profile_manager->IsValidProfile(incognito_profile));
  EXPECT_EQ(initial_profile_count, profile_manager->GetNumberOfProfiles());
  // After destroying the incognito profile incognito preferences should be
  // cleared so the default save path should be taken from the main profile.
  EXPECT_FALSE(profile->GetOffTheRecordPrefs()
                   ->GetFilePath(prefs::kSaveFileDefaultDirectory)
                   .empty());
}
