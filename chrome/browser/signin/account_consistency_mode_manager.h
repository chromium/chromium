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
#include "components/prefs/pref_member.h"
#include "components/signin/public/base/account_consistency_method.h"
#include "components/signin/public/base/signin_buildflags.h"

namespace user_prefs {
class PrefRegistrySyncable;
}

class Profile;
class ProfileAttributesEntry;

// Manages the account consistency mode for each profile.
class AccountConsistencyModeManager : public KeyedService {
 public:
  explicit AccountConsistencyModeManager(Profile* profile);

  AccountConsistencyModeManager(const AccountConsistencyModeManager&) = delete;
  AccountConsistencyModeManager& operator=(
      const AccountConsistencyModeManager&) = delete;

  ~AccountConsistencyModeManager() override;

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
                           MigrateAtCreation);
  FRIEND_TEST_ALL_PREFIXES(AccountConsistencyModeManagerTest,
                           SigninAllowedChangesDiceState);
  FRIEND_TEST_ALL_PREFIXES(AccountConsistencyModeManagerTest,
                           AllowBrowserSigninSwitch);
  FRIEND_TEST_ALL_PREFIXES(AccountConsistencyModeManagerTest,
                           DiceEnabledForNewProfiles);

  // Returns the account consistency method for the current profile.
  signin::AccountConsistencyMethod GetAccountConsistencyMethod();

  // Computes the account consistency method for the current profile. This is
  // only called from the constructor, the account consistency method cannot
  // change during the lifetime of a profile.
  static signin::AccountConsistencyMethod ComputeAccountConsistencyMethod(
      Profile* profile);

  raw_ptr<Profile> profile_;
  signin::AccountConsistencyMethod account_consistency_;
  bool account_consistency_initialized_;
};

#endif  // CHROME_BROWSER_SIGNIN_ACCOUNT_CONSISTENCY_MODE_MANAGER_H_
