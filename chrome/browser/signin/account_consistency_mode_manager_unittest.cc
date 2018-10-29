// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/account_consistency_mode_manager.h"

#include <memory>
#include <utility>

#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/signin/scoped_account_consistency.h"
#include "chrome/browser/supervised_user/supervised_user_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_notifier_impl.h"
#include "components/prefs/testing_pref_store.h"
#include "components/signin/core/browser/profile_management_switches.h"
#include "components/signin/core/browser/signin_buildflags.h"
#include "components/signin/core/browser/signin_pref_names.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX)
#include "base/test/scoped_feature_list.h"
#include "ui/base/ui_base_features.h"
#endif

// Check the default account consistency method.
TEST(AccountConsistencyModeManagerTest, DefaultValue) {
  content::TestBrowserThreadBundle test_thread_bundle;
  TestingProfile profile;

#if BUILDFLAG(ENABLE_MIRROR)
  EXPECT_EQ(signin::AccountConsistencyMethod::kMirror,
            AccountConsistencyModeManager::GetMethodForProfile(&profile));
#elif BUILDFLAG(ENABLE_DICE_SUPPORT)
  EXPECT_EQ(signin::AccountConsistencyMethod::kDiceMigration,
            AccountConsistencyModeManager::GetMethodForProfile(&profile));
#else
  EXPECT_EQ(signin::AccountConsistencyMethod::kDisabled,
            AccountConsistencyModeManager::GetMethodForProfile(&profile));
  EXPECT_FALSE(
      AccountConsistencyModeManager::IsMirrorEnabledForProfile(&profile));
  EXPECT_FALSE(
      AccountConsistencyModeManager::IsDiceEnabledForProfile(&profile));
#endif
}

TEST(AccountConsistencyModeManagerTest, Basic) {
  content::TestBrowserThreadBundle test_thread_bundle;

  struct TestCase {
    signin::AccountConsistencyMethod method;
    bool expect_mirror_enabled;
    bool expect_dice_enabled;
  } test_cases[] = {
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
    {signin::AccountConsistencyMethod::kDiceFixAuthErrors, false, false},
    {signin::AccountConsistencyMethod::kDiceMigration, false, false},
    {signin::AccountConsistencyMethod::kDice, false, true},
#else
    {signin::AccountConsistencyMethod::kMirror, true, false}
#endif
  };

  for (const TestCase& test_case : test_cases) {
    ScopedAccountConsistency scoped_method(test_case.method);
    TestingProfile profile;

    EXPECT_EQ(test_case.method,
              AccountConsistencyModeManager::GetMethodForProfile(&profile));
    EXPECT_EQ(
        test_case.expect_mirror_enabled,
        AccountConsistencyModeManager::IsMirrorEnabledForProfile(&profile));
    EXPECT_EQ(test_case.expect_dice_enabled,
              AccountConsistencyModeManager::IsDiceEnabledForProfile(&profile));
  }
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
// Checks that changing the signin-allowed pref changes the Dice state on next
// startup.
TEST(AccountConsistencyModeManagerTest, SigninAllowedChangesDiceState) {
  ScopedAccountConsistencyDice scoped_dice;
  content::TestBrowserThreadBundle test_thread_bundle;
  TestingProfile profile;
  ASSERT_FALSE(profile.IsNewProfile());

  {
    // First startup.
    AccountConsistencyModeManager manager(&profile);
    EXPECT_TRUE(profile.GetPrefs()->GetBoolean(prefs::kSigninAllowed));
    EXPECT_TRUE(
        profile.GetPrefs()->GetBoolean(prefs::kSigninAllowedOnNextStartup));
    EXPECT_EQ(signin::AccountConsistencyMethod::kDice,
              manager.GetAccountConsistencyMethod());

    // User changes their settings.
    profile.GetPrefs()->SetBoolean(prefs::kSigninAllowedOnNextStartup, false);
    // Dice should remain in the same state until restart.
    EXPECT_EQ(signin::AccountConsistencyMethod::kDice,
              manager.GetAccountConsistencyMethod());
  }

  {
    // Second startup.
    AccountConsistencyModeManager manager(&profile);
    // The signin-allowed pref should be disabled.
    EXPECT_FALSE(profile.GetPrefs()->GetBoolean(prefs::kSigninAllowed));
    EXPECT_FALSE(
        profile.GetPrefs()->GetBoolean(prefs::kSigninAllowedOnNextStartup));
    // Dice should be disabled.
    EXPECT_EQ(signin::AccountConsistencyMethod::kDiceFixAuthErrors,
              manager.GetAccountConsistencyMethod());
  }
}

// Checks that Dice migration happens when the reconcilor is created.
TEST(AccountConsistencyModeManagerTest, MigrateAtCreation) {
  content::TestBrowserThreadBundle test_thread_bundle;
  TestingProfile profile;
  ASSERT_FALSE(profile.IsNewProfile());

  {
    // Migration does not happen if SetDiceMigrationOnStartup() is not called.
    ScopedAccountConsistencyDiceMigration scoped_dice_migration;
    AccountConsistencyModeManager manager(&profile);
    EXPECT_FALSE(manager.IsReadyForDiceMigration(&profile));
    EXPECT_NE(signin::AccountConsistencyMethod::kDice,
              manager.GetAccountConsistencyMethod());
  }

  AccountConsistencyModeManager::SetDiceMigrationOnStartup(profile.GetPrefs(),
                                                           true);

  {
    // Migration does not happen if Dice is not enabled.
    ScopedAccountConsistencyDiceFixAuthErrors scoped_dice_fix_errors;
    AccountConsistencyModeManager manager(&profile);
    EXPECT_TRUE(manager.IsReadyForDiceMigration(&profile));
    EXPECT_NE(signin::AccountConsistencyMethod::kDice,
              manager.GetAccountConsistencyMethod());
  }

  {
    // Migration happens.
    ScopedAccountConsistencyDiceMigration scoped_dice_migration;
    AccountConsistencyModeManager manager(&profile);
    EXPECT_TRUE(manager.IsReadyForDiceMigration(&profile));
    EXPECT_EQ(signin::AccountConsistencyMethod::kDice,
              manager.GetAccountConsistencyMethod());
  }
}

// Checks that new profiles are migrated at creation.
TEST(AccountConsistencyModeManagerTest, NewProfile) {
  content::TestBrowserThreadBundle test_thread_bundle;
  ScopedAccountConsistencyDiceMigration scoped_dice_migration;
  TestingProfile::Builder profile_builder;
  {
    TestingPrefStore* user_prefs = new TestingPrefStore();

    // Set the read error so that Profile::IsNewProfile() returns true.
    user_prefs->set_read_error(PersistentPrefStore::PREF_READ_ERROR_NO_FILE);

    std::unique_ptr<sync_preferences::TestingPrefServiceSyncable> pref_service =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>(
            new TestingPrefStore(), new TestingPrefStore(), user_prefs,
            new TestingPrefStore(), new user_prefs::PrefRegistrySyncable(),
            new PrefNotifierImpl());
    RegisterUserProfilePrefs(pref_service->registry());
    profile_builder.SetPrefService(std::move(pref_service));
  }
  std::unique_ptr<TestingProfile> profile = profile_builder.Build();
  ASSERT_TRUE(profile->IsNewProfile());
  EXPECT_TRUE(
      AccountConsistencyModeManager::IsDiceEnabledForProfile(profile.get()));
}

TEST(AccountConsistencyModeManagerTest, DiceOnlyForRegularProfile) {
  ScopedAccountConsistencyDice scoped_dice;
  content::TestBrowserThreadBundle test_thread_bundle;

  {
    // Regular profile.
    TestingProfile profile;
    EXPECT_TRUE(
        AccountConsistencyModeManager::IsDiceEnabledForProfile(&profile));
    EXPECT_EQ(signin::AccountConsistencyMethod::kDice,
              AccountConsistencyModeManager::GetMethodForProfile(&profile));

    // Incognito profile.
    Profile* incognito_profile = profile.GetOffTheRecordProfile();
    EXPECT_FALSE(AccountConsistencyModeManager::IsDiceEnabledForProfile(
        incognito_profile));
    EXPECT_FALSE(
        AccountConsistencyModeManager::GetForProfile(incognito_profile));
    EXPECT_EQ(
        signin::AccountConsistencyMethod::kDiceFixAuthErrors,
        AccountConsistencyModeManager::GetMethodForProfile(incognito_profile));
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
        signin::AccountConsistencyMethod::kDiceFixAuthErrors,
        AccountConsistencyModeManager::GetMethodForProfile(profile.get()));
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
        signin::AccountConsistencyMethod::kDiceFixAuthErrors,
        AccountConsistencyModeManager::GetMethodForProfile(profile.get()));
  }
}
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

#if defined(OS_CHROMEOS)
TEST(AccountConsistencyModeManagerTest, MirrorDisabledForNonUnicorn) {
  // Creation of this object sets the current thread's id as UI thread.
  content::TestBrowserThreadBundle test_thread_bundle;

  TestingProfile profile;
  EXPECT_FALSE(
      AccountConsistencyModeManager::IsMirrorEnabledForProfile(&profile));
  EXPECT_FALSE(
      AccountConsistencyModeManager::IsDiceEnabledForProfile(&profile));
  EXPECT_EQ(signin::AccountConsistencyMethod::kDisabled,
            AccountConsistencyModeManager::GetMethodForProfile(&profile));
}

TEST(AccountConsistencyModeManagerTest, MirrorEnabledByPreference) {
  // Creation of this object sets the current thread's id as UI thread.
  content::TestBrowserThreadBundle test_thread_bundle;

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
  content::TestBrowserThreadBundle test_thread_bundle;
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

#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX)
// Checks that the kExperimentalUi enables Dice migration.
TEST(AccountConsistencyModeManagerTest, ExperimentalUI) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kExperimentalUi);

  content::TestBrowserThreadBundle test_thread_bundle;
  TestingProfile profile;
#if defined(OS_CHROMEOS)
  EXPECT_EQ(signin::AccountConsistencyMethod::kDisabled,
            AccountConsistencyModeManager::GetMethodForProfile(&profile));
#else
  EXPECT_EQ(signin::AccountConsistencyMethod::kDiceMigration,
            AccountConsistencyModeManager::GetMethodForProfile(&profile));
#endif
}
#endif
