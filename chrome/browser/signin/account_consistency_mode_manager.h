// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_ACCOUNT_CONSISTENCY_MODE_MANAGER_H_
#define CHROME_BROWSER_SIGNIN_ACCOUNT_CONSISTENCY_MODE_MANAGER_H_

#include "base/feature_list.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "build/buildflag.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_member.h"
#include "components/signin/public/base/account_consistency_method.h"
#include "components/signin/public/base/signin_buildflags.h"

namespace user_prefs {
class PrefRegistrySyncable;
}

class Profile;

// Account consistency feature. Only used on platforms where Mirror is not
// always enabled (ENABLE_MIRROR is false).
extern const base::Feature kAccountConsistencyFeature;

// The account consistency method feature parameter name.
extern const char kAccountConsistencyFeatureMethodParameter[];

// Account consistency method feature values.
extern const char kAccountConsistencyFeatureMethodMirror[];
extern const char kAccountConsistencyFeatureMethodDiceMigration[];
extern const char kAccountConsistencyFeatureMethodDice[];

extern const base::Feature kForceDiceMigration;

// Manages the account consistency mode for each profile.
class AccountConsistencyModeManager : public KeyedService {
 public:
  // Returns the AccountConsistencyModeManager associated with this profile.
  // May return nullptr if there is none (e.g. in incognito).
  static AccountConsistencyModeManager* GetForProfile(Profile* profile);

  explicit AccountConsistencyModeManager(Profile* profile);
  ~AccountConsistencyModeManager() override;

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Helper method, shorthand for calling GetAccountConsistencyMethod().
  static signin::AccountConsistencyMethod GetMethodForProfile(Profile* profile);

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  // Schedules migration to happen at next startup.
  void SetReadyForDiceMigration(bool is_ready);
  // Sets migration to Dice as completed.
  void SetDiceMigrationCompleted();
  // Returns true if migration can happen on the next startup.
  static bool IsReadyForDiceMigration(Profile* profile);
  // Returns true if migration to Dice is completed.
  static bool IsDiceMigrationCompleted(Profile* profile);
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
                           DisallowSigninSwitch);
  FRIEND_TEST_ALL_PREFIXES(AccountConsistencyModeManagerTest,
                           ForceDiceMigration);

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  // Schedules migration to happen at next startup. Exposed as a static function
  // for testing.
  static void SetDiceMigrationOnStartup(PrefService* prefs, bool migrate);
#endif

  // Returns the account consistency method for the current profile.
  signin::AccountConsistencyMethod GetAccountConsistencyMethod();

  // Computes the account consistency method for the current profile. This is
  // only called from the constructor, the account consistency method cannot
  // change during the lifetime of a profile.
  static signin::AccountConsistencyMethod ComputeAccountConsistencyMethod(
      Profile* profile);

  Profile* profile_;
  signin::AccountConsistencyMethod account_consistency_;
  bool account_consistency_initialized_;

  // By default, DICE is not enabled in builds lacking an API key. Set to true
  // for tests.
  static bool ignore_missing_oauth_client_for_testing_;

  DISALLOW_COPY_AND_ASSIGN(AccountConsistencyModeManager);
};

#endif  // CHROME_BROWSER_SIGNIN_ACCOUNT_CONSISTENCY_MODE_MANAGER_H_
