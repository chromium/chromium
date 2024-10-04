// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <string>

#include "base/command_line.h"
#include "base/containers/to_vector.h"
#include "base/files/file_path_watcher.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "base/test/test_timeouts.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/platform_apps/shortcut_manager.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/password_manager/profile_password_store_factory.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/profile_deletion_observer.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store/password_store_consumer.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/supervised_user/core/common/pref_names.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/constants/ash_switches.h"
#include "base/path_service.h"
#include "chrome/common/chrome_paths.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_types.h"
#include "testing/gtest/include/gtest/gtest.h"
#endif

namespace {

void ProfileCreationComplete(base::OnceClosure completion_callback,
                             Profile* profile) {
  ASSERT_TRUE(profile);
  // No browser should have been created for this profile yet.
  EXPECT_EQ(chrome::GetBrowserCount(profile), 0U);
  EXPECT_EQ(chrome::GetTotalBrowserCount(), 1U);
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
      profile->GetBrowsingDataRemover()->SetWouldCompleteCallbackForTesting(
          would_complete_callback);
    }
  }

  MultipleProfileDeletionObserver(const MultipleProfileDeletionObserver&) =
      delete;
  MultipleProfileDeletionObserver& operator=(
      const MultipleProfileDeletionObserver&) = delete;

  ~MultipleProfileDeletionObserver() override {
    g_browser_process->profile_manager()->GetProfileAttributesStorage().
        RemoveObserver(this);
  }

  void Wait() {
    keep_alive_ = std::make_unique<ScopedKeepAlive>(
        KeepAliveOrigin::PROFILE_MANAGER, KeepAliveRestartOption::DISABLED);
    loop_.Run();
  }

 private:
  void OnProfileWillBeRemoved(const base::FilePath& profile_path) override {
    profiles_removed_count_++;
    MaybeQuit();
  }

  // TODO(crbug.com/41309128): remove this code when bug is fixed.
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
};

void EphemeralProfileCreationComplete(base::OnceClosure completion_callback,
                                      Profile* profile) {
  if (profile)
    profile->GetPrefs()->SetBoolean(prefs::kForceEphemeralProfiles, true);
  ProfileCreationComplete(std::move(completion_callback), profile);
}

class ProfileRemovalObserver : public ProfileAttributesStorage::Observer {
 public:
  ProfileRemovalObserver() {
    g_browser_process->profile_manager()->GetProfileAttributesStorage().
        AddObserver(this);
  }

  ProfileRemovalObserver(const ProfileRemovalObserver&) = delete;
  ProfileRemovalObserver& operator=(const ProfileRemovalObserver&) = delete;

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

  base::WeakPtr<password_manager::PasswordStoreConsumer> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::RunLoop run_loop_;
  std::vector<std::unique_ptr<password_manager::PasswordForm>>
      password_entries_;
  base::WeakPtrFactory<PasswordStoreConsumerVerifier> weak_ptr_factory_{this};
};

base::FilePath GetFirstNonSigninNonLockScreenAppProfile(
    ProfileAttributesStorage* storage) {
  std::vector<ProfileAttributesEntry*> entries =
      storage->GetAllProfilesAttributesSortedByNameWithCheck();
#if BUILDFLAG(IS_CHROMEOS)
  for (ProfileAttributesEntry* entry : entries) {
    base::FilePath profile_path = entry->GetPath();
    std::string base_name = profile_path.BaseName().value();
    if (base_name != ash::kSigninBrowserContextBaseName &&
        base_name != ash::kLockScreenAppBrowserContextBaseName) {
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

class ProfileManagerBrowserTestBase : public InProcessBrowserTest {
 protected:
  void SetUp() override {
    // Shortcut deletion delays tests shutdown on Win-7 and results in time out.
    // See crbug.com/1073451.
#if BUILDFLAG(IS_WIN)
    AppShortcutManager::SuppressShortcutsForTesting();
#endif
    InProcessBrowserTest::SetUp();
  }
  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
#if BUILDFLAG(IS_CHROMEOS)
    command_line->AppendSwitch(
        ash::switches::kIgnoreUserProfileMappingForTests);
#endif
  }
};

class ProfileManagerBrowserTest : public ProfileManagerBrowserTestBase,
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

 private:
  base::test::ScopedFeatureList feature_list_;
};

// CrOS multi-profiles implementation is too different for these tests.
#if !BUILDFLAG(IS_CHROMEOS)

// TODO(crbug.com/40818380): Test failed on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_DeleteSingletonProfile DISABLED_DeleteSingletonProfile
#else
#define MAYBE_DeleteSingletonProfile DeleteSingletonProfile
#endif

IN_PROC_BROWSER_TEST_P(ProfileManagerBrowserTest,
                       MAYBE_DeleteSingletonProfile) {
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
  profile_manager->GetDeleteProfileHelper().MaybeScheduleProfileForDeletion(
      singleton_profile_path, base::DoNothing(),
      ProfileMetrics::DELETE_PROFILE_USER_MANAGER);

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
  std::string last_used_profile_name = last_used->GetBaseName().MaybeAsASCII();
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
  profiles::testing::CreateProfileSync(profile_manager, new_path);

  ASSERT_EQ(2u, storage.GetNumberOfProfiles());

  // Delete inactive profile.
  MultipleProfileDeletionObserver profile_deletion_observer(1u);
  profile_manager->GetDeleteProfileHelper().MaybeScheduleProfileForDeletion(
      new_path, base::DoNothing(), ProfileMetrics::DELETE_PROFILE_USER_MANAGER);
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
  base::FilePath new_profile_path =
      profile_manager->GenerateNextProfileDirectoryPath();
  [[maybe_unused]] Profile& new_profile =
      profiles::testing::CreateProfileSync(profile_manager, new_profile_path);

  base::FilePath current_profile_path = browser()->profile()->GetPath();
  base::FilePath new_last_used_path = new_profile_path;

  ASSERT_EQ(2u, storage.GetNumberOfProfiles());

  // Delete current profile.
  MultipleProfileDeletionObserver profile_deletion_observer(1u);
  profile_manager->GetDeleteProfileHelper().MaybeScheduleProfileForDeletion(
      current_profile_path, base::DoNothing(),
      ProfileMetrics::DELETE_PROFILE_USER_MANAGER);
  profile_deletion_observer.Wait();

  // Make sure a profile created earlier become the only profile.
  EXPECT_EQ(1u, storage.GetNumberOfProfiles());
  EXPECT_EQ(new_last_used_path,
            storage.GetAllProfilesAttributes().front()->GetPath());

  // Make sure that last used profile preference is set correctly.
  Profile* last_used = ProfileManager::GetLastUsedProfile();
  EXPECT_EQ(new_last_used_path, last_used->GetPath());
}

// Test is flaky. https://crbug.com/1206184
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
#define MAYBE_DeleteAllProfiles DISABLED_DeleteAllProfiles
#else
#define MAYBE_DeleteAllProfiles DeleteAllProfiles
#endif
IN_PROC_BROWSER_TEST_P(ProfileManagerBrowserTest, MAYBE_DeleteAllProfiles) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ProfileAttributesStorage& storage =
      profile_manager->GetProfileAttributesStorage();

  // Create  additional profiles.
  for (size_t i = 0; i < 2; i++) {
    profiles::testing::CreateProfileSync(
        profile_manager, profile_manager->GenerateNextProfileDirectoryPath());
  }
  ASSERT_EQ(3u, storage.GetNumberOfProfiles());

  size_t profiles_to_be_deleted = 3U;

  // Delete all profiles.
  MultipleProfileDeletionObserver profile_deletion_observer(
      profiles_to_be_deleted);
  std::vector<ProfileAttributesEntry*> entries =
      storage.GetAllProfilesAttributes();
  std::vector<base::FilePath> old_profile_paths;
  for (ProfileAttributesEntry* entry : entries) {
    base::FilePath profile_path = entry->GetPath();
    EXPECT_FALSE(profile_path.empty());
    profile_manager->GetDeleteProfileHelper().MaybeScheduleProfileForDeletion(
        profile_path, base::DoNothing(),
        ProfileMetrics::DELETE_PROFILE_USER_MANAGER);
    old_profile_paths.push_back(profile_path);
  }
  profile_deletion_observer.Wait();

  EXPECT_EQ(1u, storage.GetNumberOfProfiles());
  // Make sure a new profile was created automatically.
  base::FilePath new_profile_path =
      storage.GetAllProfilesAttributes().front()->GetPath();
  for (const base::FilePath& old_profile_path : old_profile_paths)
    EXPECT_NE(old_profile_path, new_profile_path);

  // Make sure that last used profile preference is set correctly.
  Profile* last_used = ProfileManager::GetLastUsedProfile();
  EXPECT_EQ(new_profile_path, last_used->GetPath());
}

#endif  // !BUILDFLAG(IS_CHROMEOS)

IN_PROC_BROWSER_TEST_P(ProfileManagerBrowserTest, ProfileFromProfileKey) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  Profile* profile1 = browser()->profile();

  // Create an additional profile.
  base::FilePath new_path = profile_manager->GenerateNextProfileDirectoryPath();
  profiles::testing::CreateProfileSync(profile_manager, new_path);

  Profile* profile2 = profile_manager->GetProfile(new_path);

  EXPECT_NE(profile1, profile2);
  EXPECT_NE(profile1->GetProfileKey(), profile2->GetProfileKey());
  EXPECT_EQ(profile1, profile_manager->GetProfileFromProfileKey(
                          profile1->GetProfileKey()));
  EXPECT_EQ(profile2, profile_manager->GetProfileFromProfileKey(
                          profile2->GetProfileKey()));

  // Create off-the-record profiles.
  auto otr_profile_id1 = Profile::OTRProfileID::CreateUniqueForTesting();
  auto otr_profile_id2 = Profile::OTRProfileID::CreateUniqueForTesting();

  Profile* otr_1a = profile1->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  Profile* otr_1b = profile1->GetOffTheRecordProfile(otr_profile_id1,
                                                     /*create_if_needed=*/true);
  Profile* otr_1c = profile1->GetOffTheRecordProfile(otr_profile_id2,
                                                     /*create_if_needed=*/true);
  Profile* otr_2a = profile2->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  Profile* otr_2b = profile2->GetOffTheRecordProfile(otr_profile_id1,
                                                     /*create_if_needed=*/true);

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

#if BUILDFLAG(IS_CHROMEOS)

class ProfileManagerCrOSBrowserTest : public ProfileManagerBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Use a user hash other than the default
    // ash::BrowserContextHelper::kTestUserBrowserContextDirName so that
    // the prefix case is tested.
    command_line->AppendSwitchASCII(ash::switches::kLoginProfile,
                                    "test-user-hash");
  }
};

IN_PROC_BROWSER_TEST_P(ProfileManagerCrOSBrowserTest, GetLastUsedProfile) {
  // Make sure that last used profile is correct.
  Profile* last_used_profile = ProfileManager::GetLastUsedProfile();
  EXPECT_TRUE(last_used_profile != nullptr);

  base::FilePath profile_path;
  base::PathService::Get(chrome::DIR_USER_DATA, &profile_path);

  profile_path = profile_path.Append(
      ash::BrowserContextHelper::GetUserBrowserContextDirName(
          "test-user-hash"));
  EXPECT_EQ(profile_path.value(), last_used_profile->GetPath().value());
}

#endif  // BUILDFLAG(IS_CHROMEOS)

// ChromeOS doesn't support multiple profiles.
#if !BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_P(ProfileManagerBrowserTest, CreateProfileWithCallback) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();

  ASSERT_EQ(profile_manager->GetNumberOfProfiles(), 1U);

  // Create a profile, make sure callback is invoked before any callbacks are
  // invoked (so they can do things like sign in the profile, etc).
  base::RunLoop run_loop;
  ProfileManager::CreateMultiProfileAsync(
      u"New Profile",
      /*icon_index=*/0, /*is_hidden=*/false,
      base::BindOnce(&ProfileCreationComplete, run_loop.QuitWhenIdleClosure()));
  run_loop.Run();
  EXPECT_EQ(profile_manager->GetNumberOfProfiles(), 2U);
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

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
  profiles::testing::CreateProfileSync(profile_manager, path_profile2);

  BrowserList* browser_list = BrowserList::GetInstance();
  ASSERT_EQ(initial_profile_count + 1U, storage.GetNumberOfProfiles());
  EXPECT_EQ(1U, browser_list->size());

  // Open a browser window for the first profile.
  profiles::SwitchToProfile(path_profile1, false);
  EXPECT_EQ(1U, chrome::GetTotalBrowserCount());
  EXPECT_EQ(1U, browser_list->size());
  EXPECT_EQ(path_profile1, browser_list->get(0)->profile()->GetPath());

  // Open a browser window for the second profile.
  profiles::SwitchToProfile(path_profile2, false);
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(2U, chrome::GetTotalBrowserCount());
  EXPECT_EQ(2U, browser_list->size());
  EXPECT_EQ(path_profile2, browser_list->get(1)->profile()->GetPath());

  // Switch to the first profile without opening a new window.
  profiles::SwitchToProfile(path_profile1, false);
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
  base::FilePath path_profile2 =
      profile_manager->GenerateNextProfileDirectoryPath();
  // Create an additional profile.
  profiles::testing::CreateProfileSync(profile_manager, path_profile2);

  BrowserList* browser_list = BrowserList::GetInstance();
  ASSERT_EQ(initial_profile_count + 1U, storage.GetNumberOfProfiles());
  EXPECT_EQ(1U, browser_list->size());

  // Open a browser window for the first profile.
  base::test::TestFuture<Browser*> browser1_future;
  profiles::SwitchToProfile(path_profile1, false,
                            browser1_future.GetCallback());
  EXPECT_TRUE(browser1_future.Wait());
  EXPECT_EQ(1U, chrome::GetTotalBrowserCount());
  ASSERT_EQ(1U, browser_list->size());
  EXPECT_EQ(path_profile1, browser1_future.Get()->profile()->GetPath());
  // Open a browser window for the second profile.
  base::test::TestFuture<Browser*> browser2_future;
  profiles::SwitchToProfile(path_profile2, false,
                            browser2_future.GetCallback());
  EXPECT_TRUE(browser2_future.Wait());
  EXPECT_EQ(2U, chrome::GetTotalBrowserCount());
  ASSERT_EQ(2U, browser_list->size());
  EXPECT_EQ(path_profile2, browser2_future.Get()->profile()->GetPath());
}

IN_PROC_BROWSER_TEST_P(ProfileManagerBrowserTest, AddMultipleProfiles) {
  // Verifies that the browser doesn't crash when it is restarted.
}

// Regression test for https://crbug.com/1472849
IN_PROC_BROWSER_TEST_F(ProfileManagerBrowserTestBase,
                       ConcurrentCreationAsyncAndSync) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  base::FilePath profile_path =
      profile_manager->GenerateNextProfileDirectoryPath();
  // Initiate asynchronous creation.
  profile_manager->CreateProfileAsync(profile_path, base::DoNothing());
  // The profile is being created, but creation is not complete.
  EXPECT_EQ(nullptr, profile_manager->GetProfileByPath(profile_path));
  // Request synchronous creation of the same profile, this should not crash.
  Profile* profile = profile_manager->GetProfile(profile_path);
  // The profile has been loaded.
  EXPECT_EQ(profile, profile_manager->GetProfileByPath(profile_path));
  EXPECT_EQ(profile->GetPath(), profile_path);
}

IN_PROC_BROWSER_TEST_P(ProfileManagerBrowserTest, EphemeralProfile) {
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
  {
    base::RunLoop run_loop;
    profile_manager->CreateProfileAsync(
        path_profile2, base::BindOnce(&EphemeralProfileCreationComplete,
                                      run_loop.QuitWhenIdleClosure()));

    run_loop.Run();
  }

  BrowserList* browser_list = BrowserList::GetInstance();
  ASSERT_EQ(initial_profile_count + 1U, storage.GetNumberOfProfiles());
  EXPECT_EQ(1U, browser_list->size());

  // Open a browser window for the second profile.
  profiles::SwitchToProfile(path_profile2, false);
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(2U, chrome::GetTotalBrowserCount());
  EXPECT_EQ(2U, browser_list->size());
  EXPECT_EQ(path_profile2, browser_list->get(1)->profile()->GetPath());

  // Create a second window for the ephemeral profile.
  profiles::SwitchToProfile(path_profile2, true);
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
  ProfileDeletionObserver observer;
  CloseBrowserSynchronously(browser_list->get(1));
  observer.Wait();

  EXPECT_EQ(1U, browser_list->size());
  EXPECT_EQ(initial_profile_count, storage.GetNumberOfProfiles());

// The following check is flaky on Windows.
// TODO(crbug.com/40756611): re-enable this check when the profile
// directory deletion works more reliably on Windows.
#if !BUILDFLAG(IS_WIN)
  if (base::FeatureList::IsEnabled(features::kDestroyProfileOnBrowserClose)) {
    // Check that NukeProfileFromDisk() works correctly.
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::FilePathWatcher watcher;
    base::RunLoop run_loop;
    ASSERT_TRUE(watcher.Watch(
        path_profile2, base::FilePathWatcher::Type::kNonRecursive,
        base::BindLambdaForTesting([&run_loop, &path_profile2](
                                       const base::FilePath& path, bool error) {
          if (path != path_profile2)
            return;
          EXPECT_FALSE(error);
          if (!base::PathExists(path))
            run_loop.Quit();
        })));
    run_loop.Run();
    EXPECT_FALSE(base::PathExists(path_profile2));
  }
#endif  // !BUILDFLAG(IS_WIN)
}

// The test makes sense on those platforms where the keychain exists.
#if !BUILDFLAG(IS_WIN) && !BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_P(ProfileManagerBrowserTest, DeletePasswords) {
  Profile* profile = browser()->profile();
  ASSERT_TRUE(profile);

  password_manager::PasswordForm form;
  form.scheme = password_manager::PasswordForm::Scheme::kHtml;
  form.url = GURL("http://accounts.google.com/LoginAuth");
  form.signon_realm = "http://accounts.google.com/";
  form.username_value = u"my_username";
  form.password_value = u"my_password";
  form.blocked_by_user = false;

  scoped_refptr<password_manager::PasswordStoreInterface> password_store =
      ProfilePasswordStoreFactory::GetForProfile(
          profile, ServiceAccessType::EXPLICIT_ACCESS)
          .get();
  ASSERT_TRUE(password_store.get());

  password_store->AddLogin(form);
  PasswordStoreConsumerVerifier verify_add;
  password_store->GetAutofillableLogins(verify_add.GetWeakPtr());
  verify_add.Wait();
  EXPECT_EQ(1u, verify_add.GetPasswords().size());

  MultipleProfileDeletionObserver profile_deletion_observer(1U);
  g_browser_process->profile_manager()
      ->GetDeleteProfileHelper()
      .MaybeScheduleProfileForDeletion(
          profile->GetPath(), base::DoNothing(),
          ProfileMetrics::DELETE_PROFILE_USER_MANAGER);
  // run_loop.Run();
  profile_deletion_observer.Wait();

  PasswordStoreConsumerVerifier verify_delete;
  password_store->GetAutofillableLogins(verify_delete.GetWeakPtr());
  verify_delete.Wait();
  EXPECT_EQ(0u, verify_delete.GetPasswords().size());
}
#endif  // !BUILDFLAG(IS_WIN) && !BUILDFLAG(IS_CHROMEOS)

// Tests Profile::HasOffTheRecordProfile, Profile::IsValidProfile and the
// profile counts in ProfileManager with respect to the creation and destruction
// of incognito profiles.
IN_PROC_BROWSER_TEST_P(ProfileManagerBrowserTest, IncognitoProfile) {
  Profile* profile = browser()->profile();
  ASSERT_TRUE(profile);
  EXPECT_FALSE(profile->HasPrimaryOTRProfile());

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ASSERT_TRUE(profile_manager);
  size_t initial_profile_count = profile_manager->GetNumberOfProfiles();

  // Create an incognito profile.
  Profile* incognito_profile =
      profile->GetPrimaryOTRProfile(/*create_if_needed=*/true);

  EXPECT_TRUE(profile->HasPrimaryOTRProfile());
  ASSERT_TRUE(profile_manager->IsValidProfile(incognito_profile));
  EXPECT_EQ(initial_profile_count, profile_manager->GetNumberOfProfiles());

  // Check that a default save path is not empty, since it's taken from the
  // main profile preferences, set it to empty and verify that it becomes
  // empty.
  EXPECT_FALSE(incognito_profile->GetPrefs()
                   ->GetFilePath(prefs::kSaveFileDefaultDirectory)
                   .empty());
  incognito_profile->GetPrefs()->SetFilePath(prefs::kSaveFileDefaultDirectory,
                                             base::FilePath());
  EXPECT_TRUE(incognito_profile->GetPrefs()
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
  // When Incognito profile does not exist, GetReadOnlyOffTheRecordPrefs gives
  // the OTR prefs.
  EXPECT_FALSE(profile->GetReadOnlyOffTheRecordPrefs()
                   ->GetFilePath(prefs::kSaveFileDefaultDirectory)
                   .empty());
}

#if BUILDFLAG(IS_CHROMEOS)
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
#endif  // BUILDFLAG(IS_CHROMEOS)

#if !BUILDFLAG(IS_CHROMEOS)

const base::FilePath::CharType kNonAsciiProfileDir[] =
    FILE_PATH_LITERAL("\u0645\u0635\u0631");

class ProfileManagerNonAsciiBrowserTest : public ProfileManagerBrowserTestBase {
 protected:
  ProfileManagerNonAsciiBrowserTest() = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ProfileManagerBrowserTestBase::SetUpCommandLine(command_line);
    command_line->AppendSwitchNative(switches::kProfileDirectory,
                                     kNonAsciiProfileDir);
  }
};

IN_PROC_BROWSER_TEST_F(ProfileManagerNonAsciiBrowserTest,
                       LaunchInNonAsciiProfileDirectoryDoesntCrash) {
  std::vector<base::FilePath::StringType> expected_paths = {
      kNonAsciiProfileDir};
  std::vector<ProfileAttributesEntry*> entries =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetAllProfilesAttributes();
  EXPECT_THAT(base::ToVector(entries,
                             [](const auto* entry) {
                               return entry->GetPath().BaseName().value();
                             }),
              ::testing::UnorderedElementsAreArray(expected_paths));
}

#endif  //! BUILDFLAG(IS_CHROMEOS)
