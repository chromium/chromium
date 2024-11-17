// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/account_consistency_mode_manager.h"

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/test/scoped_command_line.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/signin/account_consistency_mode_manager_factory.h"
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
  profile_builder.SetIsNewProfile(is_new_profile);
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

  signin::AccountConsistencyMethod method =
#if BUILDFLAG(ENABLE_MIRROR)
      signin::AccountConsistencyMethod::kMirror;
#elif BUILDFLAG(ENABLE_DICE_SUPPORT)
      signin::AccountConsistencyMethod::kDice;
#else
#error Either Dice or Mirror should be enabled
#endif

  EXPECT_EQ(method,
            AccountConsistencyModeManager::GetMethodForProfile(profile.get()));
  EXPECT_EQ(
      method == signin::AccountConsistencyMethod::kMirror,
      AccountConsistencyModeManager::IsMirrorEnabledForProfile(profile.get()));
  EXPECT_EQ(
      method == signin::AccountConsistencyMethod::kDice,
      AccountConsistencyModeManager::IsDiceEnabledForProfile(profile.get()));
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
// Checks that changing the signin-allowed pref changes the Dice state on next
// startup.
TEST(AccountConsistencyModeManagerTest, SigninAllowedChangesDiceState) {
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

TEST(AccountConsistencyModeManagerTest, AllowBrowserSigninSwitch) {
  content::BrowserTaskEnvironment task_environment;
  std::unique_ptr<TestingProfile> profile =
      BuildTestingProfile(/*is_new_profile=*/false);
  {
    base::test::ScopedCommandLine scoped_command_line;
    scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
        "allow-browser-signin", "false");
    AccountConsistencyModeManager manager(profile.get());
    EXPECT_FALSE(profile->GetPrefs()->GetBoolean(prefs::kSigninAllowed));
    // Dice should be disabled.
    EXPECT_EQ(signin::AccountConsistencyMethod::kDisabled,
              manager.GetAccountConsistencyMethod());
  }

  {
    base::test::ScopedCommandLine scoped_command_line;
    scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
        "allow-browser-signin", "true");
    AccountConsistencyModeManager manager(profile.get());
    EXPECT_TRUE(profile->GetPrefs()->GetBoolean(prefs::kSigninAllowed));
    // Dice should be enabled.
    EXPECT_EQ(signin::AccountConsistencyMethod::kDice,
              manager.GetAccountConsistencyMethod());
  }

  {
    AccountConsistencyModeManager manager(profile.get());
    EXPECT_TRUE(profile->GetPrefs()->GetBoolean(prefs::kSigninAllowed));
    EXPECT_TRUE(
        profile->GetPrefs()->GetBoolean(prefs::kSigninAllowedOnNextStartup));
    // Dice should be enabled.
    EXPECT_EQ(signin::AccountConsistencyMethod::kDice,
              manager.GetAccountConsistencyMethod());
  }
}

// Checks that Dice is enabled for new profiles.
TEST(AccountConsistencyModeManagerTest, DiceEnabledForNewProfiles) {
  content::BrowserTaskEnvironment task_environment;
  std::unique_ptr<TestingProfile> profile =
      BuildTestingProfile(/*is_new_profile=*/false);
  AccountConsistencyModeManager manager(profile.get());
  EXPECT_EQ(signin::AccountConsistencyMethod::kDice,
            manager.GetAccountConsistencyMethod());
}

TEST(AccountConsistencyModeManagerTest, DiceOnlyForRegularProfile) {
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
    Profile* incognito_profile =
        profile.GetPrimaryOTRProfile(/*create_if_needed=*/true);
    EXPECT_FALSE(AccountConsistencyModeManager::IsDiceEnabledForProfile(
        incognito_profile));
    EXPECT_FALSE(
        AccountConsistencyModeManagerFactory::GetForProfile(incognito_profile));
    EXPECT_EQ(
        signin::AccountConsistencyMethod::kDisabled,
        AccountConsistencyModeManager::GetMethodForProfile(incognito_profile));
    EXPECT_FALSE(AccountConsistencyModeManager::ShouldBuildServiceForProfile(
        incognito_profile));

    // Non-primary off-the-record profile.
    Profile* otr_profile = profile.GetOffTheRecordProfile(
        Profile::OTRProfileID::CreateUniqueForTesting(),
        /*create_if_needed=*/true);
    EXPECT_FALSE(
        AccountConsistencyModeManager::IsDiceEnabledForProfile(otr_profile));
    EXPECT_FALSE(
        AccountConsistencyModeManagerFactory::GetForProfile(otr_profile));
    EXPECT_EQ(signin::AccountConsistencyMethod::kDisabled,
              AccountConsistencyModeManager::GetMethodForProfile(otr_profile));
    EXPECT_FALSE(AccountConsistencyModeManager::ShouldBuildServiceForProfile(
        otr_profile));
  }

  // Guest profile.
  {
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
}
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

#if BUILDFLAG(ENABLE_MIRROR)
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

TEST(AccountConsistencyModeManagerTest, MirrorDisabledForOffTheRecordProfile) {
  // Creation of this object sets the current thread's id as UI thread.
  content::BrowserTaskEnvironment task_environment;

  TestingProfile profile;
  Profile* incognito_profile =
      profile.GetPrimaryOTRProfile(/*create_if_needed=*/true);
  EXPECT_FALSE(AccountConsistencyModeManager::IsMirrorEnabledForProfile(
      incognito_profile));
  EXPECT_FALSE(AccountConsistencyModeManager::IsDiceEnabledForProfile(
      incognito_profile));
  EXPECT_EQ(
      signin::AccountConsistencyMethod::kDisabled,
      AccountConsistencyModeManager::GetMethodForProfile(incognito_profile));

  Profile* otr_profile = profile.GetOffTheRecordProfile(
      Profile::OTRProfileID::CreateUniqueForTesting(),
      /*create_if_needed=*/true);
  EXPECT_FALSE(
      AccountConsistencyModeManager::IsMirrorEnabledForProfile(otr_profile));
  EXPECT_FALSE(
      AccountConsistencyModeManager::IsDiceEnabledForProfile(otr_profile));
  EXPECT_EQ(signin::AccountConsistencyMethod::kDisabled,
            AccountConsistencyModeManager::GetMethodForProfile(otr_profile));
}

#endif  // BUILDFLAG(ENABLE_MIRROR)

#if BUILDFLAG(IS_CHROMEOS_LACROS)

// In CrosApi guest sessions, profiles can be both regular and guest.
class TestingProfileForCrosApiGuestSession : public TestingProfile {
 public:
  bool IsGuestSession() const override { return true; }
};

TEST(AccountConsistencyModeManagerTest,
     MirrorDisabledForCrosApiGuestSessionType) {
  // Creation of this object sets the current thread's id as UI thread.
  content::BrowserTaskEnvironment task_environment;

  TestingProfileForCrosApiGuestSession profile;
  ASSERT_TRUE(profile.IsRegularProfile());
  ASSERT_TRUE(profile.IsGuestSession());

  // Re-create the AccountConsistencyModeManager, because `IsGuestSession()` is
  // virtual and `TestingProfile` initializes its factories in its constructor.
  AccountConsistencyModeManagerFactory::GetInstance()->SetTestingFactory(
      &profile, base::BindRepeating([](content::BrowserContext* context)
                                        -> std::unique_ptr<KeyedService> {
        return std::make_unique<AccountConsistencyModeManager>(
            Profile::FromBrowserContext(context));
      }));

  EXPECT_FALSE(
      AccountConsistencyModeManager::IsMirrorEnabledForProfile(&profile));
  EXPECT_FALSE(
      AccountConsistencyModeManager::IsDiceEnabledForProfile(&profile));
  EXPECT_EQ(signin::AccountConsistencyMethod::kDisabled,
            AccountConsistencyModeManager::GetMethodForProfile(&profile));
}

#endif
