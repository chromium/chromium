// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/account_consistency_mode_manager.h"

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/signin/account_consistency_mode_manager_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/enterprise/isolated_mode/isolated_mode_features.h"
#include "components/enterprise/isolated_mode/prefs.h"
#include "components/prefs/pref_notifier_impl.h"
#include "components/prefs/testing_pref_store.h"
#include "components/signin/public/base/account_consistency_method.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

// Check the default account consistency method.
TEST(AccountConsistencyModeManagerTest, DefaultValue) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;

  signin::AccountConsistencyMethod method =
#if BUILDFLAG(ENABLE_MIRROR)
      signin::AccountConsistencyMethod::kMirror;
#elif BUILDFLAG(ENABLE_DICE_SUPPORT)
      signin::AccountConsistencyMethod::kDice;
#else
#error Either Dice or Mirror should be enabled
#endif

  EXPECT_EQ(method,
            AccountConsistencyModeManager::GetMethodForProfile(&profile));
  EXPECT_EQ(method == signin::AccountConsistencyMethod::kMirror,
            AccountConsistencyModeManager::IsMirrorEnabledForProfile(&profile));
  EXPECT_EQ(method == signin::AccountConsistencyMethod::kDice,
            AccountConsistencyModeManager::IsDiceEnabledForProfile(&profile));
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
namespace {

std::unique_ptr<TestingProfile> BuildTestingProfile(bool is_new_profile) {
  TestingProfile::Builder profile_builder;
  profile_builder.SetIsNewProfile(is_new_profile);
  std::unique_ptr<TestingProfile> profile = profile_builder.Build();
  // Detach the factory-created `AccountConsistencyModeManager` after
  // `TestingProfile` initialization finishes (other services created with the
  // browser context, such as `DiceBoundSessionCookieServiceFactory`, depend on
  // `AccountConsistencyModeManager` being non-null during `Build()`), and reset
  // the preferences it touched so tests can manually construct
  // `AccountConsistencyModeManager` across simulated startups.
  AccountConsistencyModeManagerFactory::GetInstance()->SetTestingFactory(
      profile.get(), BrowserContextKeyedServiceFactory::TestingFactory());
  profile->GetPrefs()->ClearPref(prefs::kSigninAllowed);
  profile->GetPrefs()->ClearPref(prefs::kSigninAllowedOnNextStartup);
  profile->GetPrefs()->ClearPref(prefs::kSigninAllowedOnNextStartupCache);
  EXPECT_EQ(is_new_profile, profile->IsNewProfile());
  return profile;
}

}  // namespace
// Checks that changing the signin-allowed pref changes the Dice state on next
// startup.
TEST(AccountConsistencyModeManagerTest, SigninAllowedChangesDiceState) {
  content::BrowserTaskEnvironment task_environment;
  std::unique_ptr<TestingProfile> profile =
      BuildTestingProfile(/*is_new_profile=*/false);

  {
    // First startup.
    base::HistogramTester histogram_tester;
    AccountConsistencyModeManager manager(profile.get());
    EXPECT_TRUE(profile->GetPrefs()->GetBoolean(prefs::kSigninAllowed));
    EXPECT_TRUE(
        profile->GetPrefs()->GetBoolean(prefs::kSigninAllowedOnNextStartup));
    EXPECT_EQ(signin::AccountConsistencyMethod::kDice,
              manager.GetAccountConsistencyMethod());
    histogram_tester.ExpectUniqueSample(
        "Signin.SigninAllowedOnStartupState",
        AccountConsistencyModeManager::SigninAllowedOnStartupState::
            kAllowedByPref,
        1);

    // User changes their settings.
    profile->GetPrefs()->SetBoolean(prefs::kSigninAllowedOnNextStartup, false);
    // Dice should remain in the same state until restart.
    EXPECT_EQ(signin::AccountConsistencyMethod::kDice,
              manager.GetAccountConsistencyMethod());
  }

  {
    // Second startup.
    base::HistogramTester histogram_tester;
    AccountConsistencyModeManager manager(profile.get());
    // The signin-allowed pref should be disabled.
    EXPECT_FALSE(profile->GetPrefs()->GetBoolean(prefs::kSigninAllowed));
    EXPECT_FALSE(
        profile->GetPrefs()->GetBoolean(prefs::kSigninAllowedOnNextStartup));
    // Dice should be disabled.
    EXPECT_EQ(signin::AccountConsistencyMethod::kDisabled,
              manager.GetAccountConsistencyMethod());
    histogram_tester.ExpectUniqueSample(
        "Signin.SigninAllowedOnStartupState",
        AccountConsistencyModeManager::SigninAllowedOnStartupState::
            kDisallowedByPref,
        1);
  }
}

// Checks that a policy disabling sign-in on next startup persists across a
// subsequent startup where policy loading races with profile initialization
// (crbug.com/503263627).
TEST(AccountConsistencyModeManagerTest, SigninAllowedOnNextStartupPolicyRace) {
  content::BrowserTaskEnvironment task_environment;
  std::unique_ptr<TestingProfile> profile =
      BuildTestingProfile(/*is_new_profile=*/false);

  EXPECT_FALSE(profile->GetPrefs()->HasPrefPath(
      prefs::kSigninAllowedOnNextStartupCache));

  {
    // First startup: policy has not loaded yet at profile creation. Also set an
    // explicit `true` value in `USER_STORE` so `kSigninAllowedOnNextStartup` is
    // non-default (`!IsDefaultValue()`) when the managed policy is absent.
    base::HistogramTester histogram_tester;
    profile->GetPrefs()->SetBoolean(prefs::kSigninAllowedOnNextStartup, true);
    AccountConsistencyModeManager manager(profile.get());
    EXPECT_TRUE(profile->GetPrefs()->GetBoolean(prefs::kSigninAllowed));
    EXPECT_TRUE(
        profile->GetPrefs()->GetBoolean(prefs::kSigninAllowedOnNextStartup));
    EXPECT_TRUE(profile->GetPrefs()->GetBoolean(
        prefs::kSigninAllowedOnNextStartupCache));
    EXPECT_EQ(signin::AccountConsistencyMethod::kDice,
              manager.GetAccountConsistencyMethod());
    histogram_tester.ExpectUniqueSample(
        "Signin.SigninAllowedOnStartupState",
        AccountConsistencyModeManager::SigninAllowedOnStartupState::
            kAllowedByPref,
        1);

    // Policy loads asynchronously during the session and disables sign-in for
    // the next startup.
    profile->GetTestingPrefService()->SetManagedPref(
        prefs::kSigninAllowedOnNextStartup, base::Value(false));
    EXPECT_FALSE(profile->GetPrefs()->GetBoolean(
        prefs::kSigninAllowedOnNextStartupCache));
    // Dice remains enabled for the current session.
    EXPECT_EQ(signin::AccountConsistencyMethod::kDice,
              manager.GetAccountConsistencyMethod());

    // Simulate `KeyedService::Shutdown()` before policy provider teardown or
    // the next startup's policy race: removing the managed pref after
    // `Shutdown()` must not overwrite `kSigninAllowedOnNextStartupCache`.
    manager.Shutdown();
    profile->GetTestingPrefService()->RemoveManagedPref(
        prefs::kSigninAllowedOnNextStartup);
    EXPECT_FALSE(profile->GetPrefs()->GetBoolean(
        prefs::kSigninAllowedOnNextStartupCache));
  }

  {
    // Second startup: simulate a startup race where the managed policy is not
    // yet present in the managed pref store when the profile is initialized,
    // even though `USER_STORE` has an explicit `true` value.
    base::HistogramTester histogram_tester;
    EXPECT_TRUE(
        profile->GetPrefs()->GetBoolean(prefs::kSigninAllowedOnNextStartup));
    EXPECT_FALSE(profile->GetPrefs()->GetBoolean(
        prefs::kSigninAllowedOnNextStartupCache));

    AccountConsistencyModeManager manager(profile.get());
    // Sign-in and Dice must remain disabled using the cached value from the
    // previous session, while the cache itself is reset to the current value
    // of `kSigninAllowedOnNextStartup` until the policy finishes loading.
    EXPECT_FALSE(profile->GetPrefs()->GetBoolean(prefs::kSigninAllowed));
    EXPECT_EQ(signin::AccountConsistencyMethod::kDisabled,
              manager.GetAccountConsistencyMethod());
    EXPECT_TRUE(profile->GetPrefs()->GetBoolean(
        prefs::kSigninAllowedOnNextStartupCache));
    histogram_tester.ExpectUniqueSample(
        "Signin.SigninAllowedOnStartupState",
        AccountConsistencyModeManager::SigninAllowedOnStartupState::
            kDisallowedByCache,
        1);

    // Policy finishes loading asynchronously during the second session.
    profile->GetTestingPrefService()->SetManagedPref(
        prefs::kSigninAllowedOnNextStartup, base::Value(false));
    EXPECT_FALSE(profile->GetPrefs()->GetBoolean(
        prefs::kSigninAllowedOnNextStartupCache));
    EXPECT_EQ(signin::AccountConsistencyMethod::kDisabled,
              manager.GetAccountConsistencyMethod());
  }

  {
    // Third startup: another startup policy race still keeps sign-in disabled,
    // because the async policy reload in the second session updated the cache.
    base::HistogramTester histogram_tester;
    profile->GetTestingPrefService()->RemoveManagedPref(
        prefs::kSigninAllowedOnNextStartup);

    AccountConsistencyModeManager manager(profile.get());
    EXPECT_FALSE(profile->GetPrefs()->GetBoolean(prefs::kSigninAllowed));
    EXPECT_EQ(signin::AccountConsistencyMethod::kDisabled,
              manager.GetAccountConsistencyMethod());
    histogram_tester.ExpectUniqueSample(
        "Signin.SigninAllowedOnStartupState",
        AccountConsistencyModeManager::SigninAllowedOnStartupState::
            kDisallowedByCache,
        1);
  }

  {
    // Fourth startup: since the policy was never applied in the third session
    // (e.g. policy was unconfigured by the admin), sign-in is re-enabled on
    // the following startup.
    base::HistogramTester histogram_tester;
    AccountConsistencyModeManager manager(profile.get());
    EXPECT_TRUE(profile->GetPrefs()->GetBoolean(prefs::kSigninAllowed));
    EXPECT_EQ(signin::AccountConsistencyMethod::kDice,
              manager.GetAccountConsistencyMethod());
    histogram_tester.ExpectUniqueSample(
        "Signin.SigninAllowedOnStartupState",
        AccountConsistencyModeManager::SigninAllowedOnStartupState::
            kAllowedByPref,
        1);
  }

  {
    // Fifth startup: when the policy is present in the managed store at
    // startup, it takes effect immediately even if the cached value was true.
    base::HistogramTester histogram_tester;
    profile->GetTestingPrefService()->SetManagedPref(
        prefs::kSigninAllowedOnNextStartup, base::Value(false));

    AccountConsistencyModeManager manager(profile.get());
    EXPECT_FALSE(profile->GetPrefs()->GetBoolean(prefs::kSigninAllowed));
    EXPECT_EQ(signin::AccountConsistencyMethod::kDisabled,
              manager.GetAccountConsistencyMethod());
    histogram_tester.ExpectUniqueSample(
        "Signin.SigninAllowedOnStartupState",
        AccountConsistencyModeManager::SigninAllowedOnStartupState::
            kDisallowedByPolicy,
        1);
  }

  {
    // Sixth startup: switching the policy to true while Chrome is closed also
    // takes effect immediately on the next startup.
    base::HistogramTester histogram_tester;
    profile->GetTestingPrefService()->SetManagedPref(
        prefs::kSigninAllowedOnNextStartup, base::Value(true));

    AccountConsistencyModeManager manager(profile.get());
    EXPECT_TRUE(profile->GetPrefs()->GetBoolean(prefs::kSigninAllowed));
    EXPECT_EQ(signin::AccountConsistencyMethod::kDice,
              manager.GetAccountConsistencyMethod());
    histogram_tester.ExpectUniqueSample(
        "Signin.SigninAllowedOnStartupState",
        AccountConsistencyModeManager::SigninAllowedOnStartupState::
            kAllowedByPolicy,
        1);

    // Policy disables sign-in again during the session, caching `false`.
    profile->GetTestingPrefService()->SetManagedPref(
        prefs::kSigninAllowedOnNextStartup, base::Value(false));
    EXPECT_FALSE(profile->GetPrefs()->GetBoolean(
        prefs::kSigninAllowedOnNextStartupCache));
  }

  {
    // Seventh startup: if `kCacheSigninAllowedOnNextStartup` is disabled
    // (kill-switch), the cached `false` value is ignored and startup falls back
    // directly to `kSigninAllowedOnNextStartup`, while the histogram still
    // records `kDisallowedByCache`.
    base::HistogramTester histogram_tester;
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndDisableFeature(kCacheSigninAllowedOnNextStartup);
    profile->GetTestingPrefService()->RemoveManagedPref(
        prefs::kSigninAllowedOnNextStartup);

    AccountConsistencyModeManager manager(profile.get());
    EXPECT_TRUE(profile->GetPrefs()->GetBoolean(prefs::kSigninAllowed));
    EXPECT_EQ(signin::AccountConsistencyMethod::kDice,
              manager.GetAccountConsistencyMethod());
    histogram_tester.ExpectUniqueSample(
        "Signin.SigninAllowedOnStartupState",
        AccountConsistencyModeManager::SigninAllowedOnStartupState::
            kDisallowedByCache,
        1);
  }
}

TEST(AccountConsistencyModeManagerTest, AllowBrowserSigninSwitch) {
  content::BrowserTaskEnvironment task_environment;
  std::unique_ptr<TestingProfile> profile =
      BuildTestingProfile(/*is_new_profile=*/false);
  {
    base::HistogramTester histogram_tester;
    base::test::ScopedCommandLine scoped_command_line;
    scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
        "allow-browser-signin", "false");
    AccountConsistencyModeManager manager(profile.get());
    EXPECT_FALSE(profile->GetPrefs()->GetBoolean(prefs::kSigninAllowed));
    // Dice should be disabled.
    EXPECT_EQ(signin::AccountConsistencyMethod::kDisabled,
              manager.GetAccountConsistencyMethod());
    histogram_tester.ExpectUniqueSample(
        "Signin.SigninAllowedOnStartupState",
        AccountConsistencyModeManager::SigninAllowedOnStartupState::
            kDisallowedByCommandLineOrProfile,
        1);
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

  // Isolated profile.
  {
    base::test::ScopedFeatureList scoped_feature_list(
        enterprise_isolated_mode::kEnableEnterpriseIsolatedMode);

    TestingProfile profile;
    profile.GetPrefs()->SetInteger(
        enterprise_isolated_mode::kEnterpriseIsolatedModeSettings,
        static_cast<int>(
            enterprise_isolated_mode::IsolatedModeSetting::kEnabled));

    Profile* isolated_profile =
        profile.GetPrimaryOTRProfile(/*create_if_needed=*/true);
    ASSERT_TRUE(isolated_profile->IsEnterpriseIsolatedModeProfile());

    EXPECT_FALSE(AccountConsistencyModeManager::IsDiceEnabledForProfile(
        isolated_profile));
    EXPECT_FALSE(AccountConsistencyModeManager::ShouldBuildServiceForProfile(
        isolated_profile));
    EXPECT_EQ(
        signin::AccountConsistencyMethod::kDisabled,
        AccountConsistencyModeManager::GetMethodForProfile(isolated_profile));
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
