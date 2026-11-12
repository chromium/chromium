// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_ACCOUNT_CONSISTENCY_MODE_MANAGER_H_
#define CHROME_BROWSER_SIGNIN_ACCOUNT_CONSISTENCY_MODE_MANAGER_H_

#include "base/feature_list.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "build/buildflag.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/signin/public/base/account_consistency_method.h"
#include "components/signin/public/base/signin_buildflags.h"

namespace user_prefs {
class PrefRegistrySyncable;
}

class Profile;
class ProfileAttributesEntry;

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
BASE_DECLARE_FEATURE(kCacheSigninAllowedOnNextStartup);
#endif

// Manages the account consistency mode for each profile.
class AccountConsistencyModeManager : public KeyedService {
 public:
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  // State of sign-in allowed resolution on startup.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  // LINT.IfChange(SigninAllowedOnStartupState)
  enum class SigninAllowedOnStartupState {
    // Sign-in is allowed by `kSigninAllowedOnNextStartup` (user-modifiable,
    // `true`), and the cache is either unset or `true`.
    kAllowedByPref = 0,
    // Sign-in is allowed by a policy or custodian override
    // (`!IsUserModifiable()`, `true`) loaded at startup.
    kAllowedByPolicy = 1,
    // `kSigninAllowedOnNextStartup` is user-modifiable and `false`, but the
    // cache from the previous session is `true`. Sign-in is allowed by the
    // cache.
    kAllowedByCache = 2,
    // Sign-in is disallowed by `kSigninAllowedOnNextStartup` (user-modifiable,
    // `false`), and the cache is either unset or `false`.
    kDisallowedByPref = 3,
    // Sign-in is disallowed by a policy or custodian override
    // (`!IsUserModifiable()`, `false`) loaded at startup.
    kDisallowedByPolicy = 4,
    // `kSigninAllowedOnNextStartup` is user-modifiable and `true` at startup,
    // but the cache from the previous session is `false` (e.g. startup policy
    // race). Sign-in is disallowed by the cache.
    kDisallowedByCache = 5,
    // Sign-in is disallowed regardless of `kSigninAllowedOnNextStartup` or its
    // cache (e.g. `--allow-browser-signin=false`, missing OAuth client keys,
    // Dasherless OIDC or enrollment-token profile, or
    // `--experimental-ai-stable-channel`).
    kDisallowedByCommandLineOrProfile = 6,
    kMaxValue = kDisallowedByCommandLineOrProfile,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/signin/enums.xml:SigninAllowedOnStartupState)
#endif

  explicit AccountConsistencyModeManager(Profile* profile);

  AccountConsistencyModeManager(const AccountConsistencyModeManager&) = delete;
  AccountConsistencyModeManager& operator=(
      const AccountConsistencyModeManager&) = delete;

  ~AccountConsistencyModeManager() override;

  // KeyedService:
  void Shutdown() override;

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Helper method, shorthand for calling GetAccountConsistencyMethod().
  // TODO(crbug.com/40780204): Migrate usages to
  // `IdentityManager::GetAccountConsistency`.
  static signin::AccountConsistencyMethod GetMethodForProfile(Profile* profile);

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  // This is a pre-requisite of IsDiceEnabledForProfile(), independent of
  // particular profile type or profile prefs.
  // `entry` should be nullptr for profiles that are not registered in the
  // `ProfileAttributesStorage` (e.g. the system profile). Profiles with a
  // managed using a profile-level management token are not allowed to sign in
  // with a Google account.
  static bool IsDiceSignInAllowed(ProfileAttributesEntry* entry = nullptr);
#endif

  // If true, then account management is done through Gaia webpages.
  // Can only be used on the UI thread.
  // Returns false if |profile| is in Guest or Incognito mode.
  // A given |profile| will have only one of Mirror or Dice consistency
  // behaviour enabled.
  static bool IsDiceEnabledForProfile(Profile* profile);

  // Returns |true| if Mirror account consistency is enabled for |profile|.
  // Can only be used on the UI thread.
  // A given |profile| will have only one of Mirror or Dice consistency
  // behaviour enabled.
  static bool IsMirrorEnabledForProfile(Profile* profile);

  // By default, Desktop Identity Consistency (aka Dice) is not enabled in
  // builds lacking an API key. For testing, set to have Dice enabled in tests.
  static void SetIgnoreMissingOAuthClientForTesting();

  // Returns true is the AccountConsistencyModeManager should be instantiated
  // for the profile. Guest, incognito and system sessions do not instantiate
  // the service.
  static bool ShouldBuildServiceForProfile(Profile* profile);

 private:
  FRIEND_TEST_ALL_PREFIXES(AccountConsistencyModeManagerTest,
                           SigninAllowedChangesDiceState);
  FRIEND_TEST_ALL_PREFIXES(AccountConsistencyModeManagerTest,
                           SigninAllowedOnNextStartupPolicyRace);
  FRIEND_TEST_ALL_PREFIXES(AccountConsistencyModeManagerTest,
                           AllowBrowserSigninSwitch);
  FRIEND_TEST_ALL_PREFIXES(AccountConsistencyModeManagerTest,
                           DiceEnabledForNewProfiles);

  // Returns the account consistency method for the current profile.
  signin::AccountConsistencyMethod GetAccountConsistencyMethod();

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  // Updates `prefs::kSigninAllowedOnNextStartupCache` when
  // `prefs::kSigninAllowedOnNextStartup` changes.
  void UpdateSigninAllowedOnNextStartupCache();
#endif

  const raw_ptr<Profile> profile_;
  signin::AccountConsistencyMethod account_consistency_ =
      signin::AccountConsistencyMethod::kDisabled;
  bool account_consistency_initialized_ = false;
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  PrefChangeRegistrar pref_change_registrar_;
#endif
};

#endif  // CHROME_BROWSER_SIGNIN_ACCOUNT_CONSISTENCY_MODE_MANAGER_H_
