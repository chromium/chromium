// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/account_consistency_mode_manager.h"

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/test/scoped_command_line.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/signin/scoped_account_consistency.h"
#include "chrome/browser/supervised_user/supervised_user_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_notifier_impl.h"
#include "components/prefs/testing_pref_store.h"
#include "components/signin/public/base/account_consistency_method.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

std::unique_ptr<TestingProfile> BuildTestingProfile(bool is_new_profile) {
  TestingProfile::Builder profile_builder;
  profile_builder.OverrideIsNewProfile(is_new_profile);
  std::unique_ptr<TestingProfile> profile = profile_builder.Build();
  EXPECT_EQ(is_new_profile, profile->IsNewProfile());
  return profile;
}

}  // namespace

// Check the default account consistency method.
TEST(AccountConsistencyModeManagerTest, DefaultValue) {
  content::BrowserTaskEnvironment task_environment;
  std::unique_ptr<TestingProfile> profile =
      BuildTestingProfile(/*is_new_profile=*/false);

#if BUILDFLAG(ENABLE_MIRROR) || defined(OS_CHROMEOS)
  EXPECT_EQ(signin::AccountConsistencyMethod::kMirror,
            AccountConsistencyModeManager::GetMethodForProfile(profile.get()));
#elif BUILDFLAG(ENABLE_DICE_SUPPORT)
  EXPECT_EQ(signin::AccountConsistencyMethod::kDice,
            AccountConsistencyModeManager::GetMethodForProfile(profile.get()));
#else
  EXPECT_EQ(signin::AccountConsistencyMethod::kDisabled,
            AccountConsistencyModeManager::GetMethodForProfile(profile.get()));
  EXPECT_FALSE(
      AccountConsistencyModeManager::IsMirrorEnabledForProfile(profile.get()));
  EXPECT_FALSE(
      AccountConsistencyModeManager::IsDiceEnabledForProfile(profile.get()));
#endif
}

TEST(AccountConsistencyModeManagerTest, Basic) {
  content::BrowserTaskEnvironment task_environment;

  struct TestCase {
    signin::AccountConsistencyMethod method;
    bool expect_mirror_enabled;
    bool expect_dice_enabled;
  } test_cases[] = {
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
    {signin::AccountConsistencyMethod::kDice, false, true},
#else
    {signin::AccountConsistencyMethod::kMirror, true, false}
#endif
  };

  for (const TestCase& test_case : test_cases) {
    ScopedAccountConsistency scoped_method(test_case.method);
    std::unique_ptr<TestingProfile> profile =
        BuildTestingProfile(/*is_new_profile=*/false);

    EXPECT_EQ(
        test_case.method,
        AccountConsistencyModeManager::GetMethodForProfile(profile.get()));
    EXPECT_EQ(test_case.expect_mirror_enabled,
              AccountConsistencyModeManager::IsMirrorEnabledForProfile(
                  profile.get()));
    EXPECT_EQ(
        test_case.expect_dice_enabled,
        AccountConsistencyModeManager::IsDiceEnabledForProfile(profile.get()));
  }
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
// Checks that changing the signin-allowed pref changes the Dice state on next
// startup.
TEST(AccountConsistencyModeManagerTest, SigninAllowedChangesDiceState) {
  ScopedAccountConsistencyDice scoped_dice;
  content::BrowserTaskEnvironment task_environment;
  std::unique_ptr<TestingProfile> profile =
      BuildTestingProfile(/*is_new_profile=*/false);

  {
    // First startup.
    AccountConsistencyModeManager manager(profile.get());
    EXPECT_TRUE(profile->GetPrefs()->GetBoolean(prefs::kSigninAllowed));
    EXPECT_TRUE(
        profile->GetPrefs()->GetBoolean(prefs::kSigninAllowedOnNextStartup));
    EXPECT_EQ(signin::AccountConsistencyMethod::kDice,
              manager.GetAccountConsistencyMethod());

    // User changes their settings.
    profile->GetPrefs()->SetBoolean(prefs::kSigninAllowedOnNextStartup, false);
    // Dice should remain in the same state until restart.
    EXPECT_EQ(signin::AccountConsistencyMethod::kDice,
              manager.GetAccountConsistencyMethod());
  }

  {
    // Second startup.
    AccountConsistencyModeManager manager(profile.get());
    // The signin-allowed pref should be disabled.
    EXPECT_FALSE(profile->GetPrefs()->GetBoolean(prefs::kSigninAllowed));
    EXPECT_FALSE(
        profile->GetPrefs()->GetBoolean(prefs::kSigninAllowedOnNextStartup));
    // Dice should be disabled.
    EXPECT_EQ(signin::AccountConsistencyMethod::kDisabled,
              manager.GetAccountConsistencyMethod());
  }
}

// The command line switch "disallow-signin" only affects the current run.
TEST(AccountConsistencyModeManagerTest, DisallowSigninSwitch) {
  ScopedAccountConsistencyDice scoped_dice;
  content::BrowserTaskEnvironment task_environment;
  std::unique_ptr<TestingProfile> profile =
      BuildTestingProfile(/*is_new_profile=*/false);

  {
    // With the switch, signin is disallowed.
    base::test::ScopedCommandLine scoped_command_line;
    scoped_command_line.GetProcessCommandLine()->AppendSwitch(
        "disallow-signin");
    AccountConsistencyModeManager manager(profile.get());
    EXPECT_FALSE(profile->GetPrefs()->GetBoolean(prefs::kSigninAllowed));
    EXPECT_TRUE(
        profile->GetPrefs()->GetBoolean(prefs::kSigninAllowedOnNextStartup));
    // Dice should be disabled.
    EXPECT_EQ(signin::AccountConsistencyMethod::kDisabled,
              manager.GetAccountConsistencyMethod());
  }

  {
    // Remove the switch, signin is allowed again.
    AccountConsistencyModeManager manager(profile.get());
    EXPECT_TRUE(profile->GetPrefs()->GetBoolean(prefs::kSigninAllowed));
    EXPECT_TRUE(
        profile->GetPrefs()->GetBoolean(prefs::kSigninAllowedOnNextStartup));
    // Dice should be enabled.
    EXPECT_EQ(signin::AccountConsistencyMethod::kDice,
              manager.GetAccountConsistencyMethod());
  }
}

// Checks that Dice migration happens when the manager is created.
TEST(AccountConsistencyModeManagerTest, MigrateAtCreation) {
  content::BrowserTaskEnvironment task_environment;
  std::unique_ptr<TestingProfile> profile =
      BuildTestingProfile(/*is_new_profile=*/false);
  AccountConsistencyModeManager manager(profile.get());
  EXPECT_EQ(signin::AccountConsistencyMethod::kDice,
            manager.GetAccountConsistencyMethod());
}

TEST(AccountConsistencyModeManagerTest, ForceDiceMigration) {
  content::BrowserTaskEnvironment task_environment;
  std::unique_ptr<TestingProfile> profile =
      BuildTestingProfile(/*is_new_profile=*/false);
  profile->GetPrefs()->SetBoolean(prefs::kTokenServiceDiceCompatible, true);
  {
    AccountConsistencyModeManager manager(profile.get());
    EXPECT_EQ(signin::AccountConsistencyMethod::kDice,
              manager.GetAccountConsistencyMethod());
    EXPECT_FALSE(manager.IsReadyForDiceMigration(profile.get()));
    // Migration is not completed yet, |kDiceMigrationCompletePref| should not
    // be written.
    EXPECT_FALSE(manager.IsDiceMigrationCompleted(profile.get()));
  }

  AccountConsistencyModeManager::SetDiceMigrationOnStartup(profile->GetPrefs(),
                                                           true);
  {
    AccountConsistencyModeManager manager(profile.get());
    EXPECT_TRUE(manager.IsReadyForDiceMigration(profile.get()));
    EXPECT_EQ(signin::AccountConsistencyMethod::kDice,
              manager.GetAccountConsistencyMethod());
    // Migration completed.
    EXPECT_TRUE(manager.IsDiceMigrationCompleted(profile.get()));
  }
}

// Checks that new profiles are migrated at creation.
TEST(AccountConsistencyModeManagerTest, NewProfile) {
  content::BrowserTaskEnvironment task_environment;
  ScopedAccountConsistencyDiceMigration scoped_dice_migration;
  std::unique_ptr<TestingProfile> profile =
      BuildTestingProfile(/*is_new_profile=*/true);
  EXPECT_TRUE(
      AccountConsistencyModeManager::IsDiceEnabledForProfile(profile.get()));
}

TEST(AccountConsistencyModeManagerTest, DiceOnlyForRegularProfile) {
  ScopedAccountConsistencyDice scoped_dice;
  content::BrowserTaskEnvironment task_environment;

  {
    // Regular profile.
    TestingProfile profile;
    EXPECT_TRUE(
        AccountConsistencyModeManager::IsDiceEnabledForProfile(&profile));
    EXPECT_EQ(signin::AccountConsistencyMethod::kDice,
              AccountConsistencyModeManager::GetMethodForProfile(&profile));
    EXPECT_TRUE(
        AccountConsistencyModeManager::ShouldBuildServiceForProfile(&profile));

    // Incognito profile.
    Profile* incognito_profile = profile.GetOffTheRecordProfile();
    EXPECT_FALSE(AccountConsistencyModeManager::IsDiceEnabledForProfile(
        incognito_profile));
    EXPECT_FALSE(
        AccountConsistencyModeManager::GetForProfile(incognito_profile));
    EXPECT_EQ(
        signin::AccountConsistencyMethod::kDisabled,
        AccountConsistencyModeManager::GetMethodForProfile(incognito_profile));
    EXPECT_FALSE(AccountConsistencyModeManager::ShouldBuildServiceForProfile(
        incognito_profile));
  }

  {
    // Guest profile.
    TestingProfile::Builder profile_builder;
    profile_builder.SetGuestSession();
    std::unique_ptr<Profile> profile = profile_builder.Build();
    ASSERT_TRUE(profile->IsGuestSession());
    EXPECT_FALSE(
        AccountConsistencyModeManager::IsDiceEnabledForProfile(profile.get()));
    EXPECT_EQ(
        signin::AccountConsistencyMethod::kDisabled,
        AccountConsistencyModeManager::GetMethodForProfile(profile.get()));
    EXPECT_FALSE(AccountConsistencyModeManager::ShouldBuildServiceForProfile(
        profile.get()));
  }

  {
    // Legacy supervised profile.
    TestingProfile::Builder profile_builder;
    profile_builder.SetSupervisedUserId("supervised_id");
    std::unique_ptr<Profile> profile = profile_builder.Build();
    ASSERT_TRUE(profile->IsLegacySupervised());
    EXPECT_FALSE(
        AccountConsistencyModeManager::IsDiceEnabledForProfile(profile.get()));
    EXPECT_EQ(
        signin::AccountConsistencyMethod::kDisabled,
        AccountConsistencyModeManager::GetMethodForProfile(profile.get()));
  }
}
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

#if defined(OS_CHROMEOS)
// Mirror is enabled by default on Chrome OS, unless specified otherwise.
TEST(AccountConsistencyModeManagerTest, MirrorEnabledByDefault) {
  // Creation of this object sets the current thread's id as UI thread.
  content::BrowserTaskEnvironment task_environment;

  TestingProfile profile;
  EXPECT_TRUE(
      AccountConsistencyModeManager::IsMirrorEnabledForProfile(&profile));
  EXPECT_FALSE(
      AccountConsistencyModeManager::IsDiceEnabledForProfile(&profile));
  EXPECT_EQ(signin::AccountConsistencyMethod::kMirror,
            AccountConsistencyModeManager::GetMethodForProfile(&profile));
}

TEST(AccountConsistencyModeManagerTest, MirrorDisabledForGuestSession) {
  // Creation of this object sets the current thread's id as UI thread.
  content::BrowserTaskEnvironment task_environment;

  TestingProfile profile;
  profile.SetGuestSession(true);
  EXPECT_FALSE(
      AccountConsistencyModeManager::IsMirrorEnabledForProfile(&profile));
  EXPECT_FALSE(
      AccountConsistencyModeManager::IsDiceEnabledForProfile(&profile));
  EXPECT_EQ(signin::AccountConsistencyMethod::kDisabled,
            AccountConsistencyModeManager::GetMethodForProfile(&profile));
}

TEST(AccountConsistencyModeManagerTest, MirrorDisabledForIncognitoProfile) {
  // Creation of this object sets the current thread's id as UI thread.
  content::BrowserTaskEnvironment task_environment;

  TestingProfile profile;
  Profile* incognito_profile = profile.GetOffTheRecordProfile();
  EXPECT_FALSE(AccountConsistencyModeManager::IsMirrorEnabledForProfile(
      incognito_profile));
  EXPECT_FALSE(AccountConsistencyModeManager::IsDiceEnabledForProfile(
      incognito_profile));
  EXPECT_EQ(
      signin::AccountConsistencyMethod::kDisabled,
      AccountConsistencyModeManager::GetMethodForProfile(incognito_profile));
}

TEST(AccountConsistencyModeManagerTest, MirrorEnabledByPreference) {
  // Creation of this object sets the current thread's id as UI thread.
  content::BrowserTaskEnvironment task_environment;

  TestingProfile::Builder profile_builder;
  {
    std::unique_ptr<sync_preferences::TestingPrefServiceSyncable> pref_service =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    RegisterUserProfilePrefs(pref_service->registry());
    profile_builder.SetPrefService(std::move(pref_service));
  }
  std::unique_ptr<TestingProfile> profile = profile_builder.Build();
  profile->GetPrefs()->SetBoolean(prefs::kAccountConsistencyMirrorRequired,
                                  true);

  EXPECT_TRUE(
      AccountConsistencyModeManager::IsMirrorEnabledForProfile(profile.get()));
  EXPECT_FALSE(
      AccountConsistencyModeManager::IsDiceEnabledForProfile(profile.get()));
  EXPECT_EQ(signin::AccountConsistencyMethod::kMirror,
            AccountConsistencyModeManager::GetMethodForProfile(profile.get()));
}
#endif  // defined(OS_CHROMEOS)

#if BUILDFLAG(ENABLE_MIRROR)
// Test that Mirror is enabled for child accounts.
TEST(AccountConsistencyModeManagerTest, MirrorChildAccount) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  profile.SetSupervisedUserId(supervised_users::kChildAccountSUID);
  EXPECT_TRUE(
      AccountConsistencyModeManager::IsMirrorEnabledForProfile(&profile));
  EXPECT_FALSE(
      AccountConsistencyModeManager::IsDiceEnabledForProfile(&profile));
  EXPECT_EQ(signin::AccountConsistencyMethod::kMirror,
            AccountConsistencyModeManager::GetMethodForProfile(&profile));
}
#endif  // BUILDFLAG(ENABLE_MIRROR)
