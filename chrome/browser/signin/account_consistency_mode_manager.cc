// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/account_consistency_mode_manager.h"

#include <string>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/account_consistency_mode_manager_factory.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "google_apis/google_api_keys.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/account_manager/account_manager_util.h"
#endif

using signin::AccountConsistencyMethod;

// TODO(droger): Verify if this feature flag is still required now that
// DICE migration was enabled by default for all users.
const base::Feature kAccountConsistencyFeature{
    "AccountConsistency", base::FEATURE_ENABLED_BY_DEFAULT};
const char kAccountConsistencyFeatureMethodParameter[] = "method";
const char kAccountConsistencyFeatureMethodMirror[] = "mirror";
const char kAccountConsistencyFeatureMethodDiceMigration[] = "dice_migration";
// TODO(msalama): Remove this method.
const char kAccountConsistencyFeatureMethodDice[] = "dice";

const base::Feature kForceDiceMigration{"ForceDiceMigration",
                                        base::FEATURE_ENABLED_BY_DEFAULT};

namespace {

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
// Preference indicating that the Dice migration should happen at the next
// Chrome startup.
const char kDiceMigrationOnStartupPref[] =
    "signin.AccountReconcilor.kDiceMigrationOnStartup2";
// Preference indicating that the Dice migraton has happened.
const char kDiceMigrationCompletePref[] = "signin.DiceMigrationComplete";

const char kDiceMigrationStatusHistogram[] = "Signin.DiceMigrationStatus";

// Used for UMA histogram kDiceMigrationStatusHistogram.
// Do not remove or re-order values.
enum class DiceMigrationStatus {
  kEnabled,
  kDisabledReadyForMigration,
  kDisabledNotReadyForMigration,
  kDisabled,

  // This is the last value. New values should be inserted above.
  kDiceMigrationStatusCount
};

DiceMigrationStatus GetDiceMigrationStatus(
    AccountConsistencyMethod account_consistency) {
  switch (account_consistency) {
    case AccountConsistencyMethod::kDice:
      return DiceMigrationStatus::kEnabled;
    case AccountConsistencyMethod::kDiceMigration:
      return DiceMigrationStatus::kDisabledNotReadyForMigration;
    case AccountConsistencyMethod::kDisabled:
      return DiceMigrationStatus::kDisabled;
    case AccountConsistencyMethod::kMirror:
      NOTREACHED();
      return DiceMigrationStatus::kDisabled;
  }
}
#endif

}  // namespace

bool AccountConsistencyModeManager::ignore_missing_oauth_client_for_testing_ =
    false;

// static
AccountConsistencyModeManager* AccountConsistencyModeManager::GetForProfile(
    Profile* profile) {
  return AccountConsistencyModeManagerFactory::GetForProfile(profile);
}

AccountConsistencyModeManager::AccountConsistencyModeManager(Profile* profile)
    : profile_(profile),
      account_consistency_(signin::AccountConsistencyMethod::kDisabled),
      account_consistency_initialized_(false) {
  DCHECK(profile_);
  DCHECK(ShouldBuildServiceForProfile(profile));

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  PrefService* prefs = profile->GetPrefs();
  // Propagate settings changes from the previous launch to the signin-allowed
  // pref.
  bool signin_allowed =
      prefs->GetBoolean(prefs::kSigninAllowedOnNextStartup) &&
      !base::CommandLine::ForCurrentProcess()->HasSwitch("disallow-signin");
  prefs->SetBoolean(prefs::kSigninAllowed, signin_allowed);

  UMA_HISTOGRAM_BOOLEAN("Signin.SigninAllowed", signin_allowed);
#endif

  account_consistency_ = ComputeAccountConsistencyMethod(profile_);

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  if (signin::DiceMethodGreaterOrEqual(
          account_consistency_, AccountConsistencyMethod::kDiceMigration)) {
    if (IsReadyForDiceMigration(profile_)) {
      // Note: Even when |account_consistency_| is kDice, there may be cases
      // when |kDiceMigrationCompletePref| preference is not set (e.g. browser
      // tests that force set DICE state to kDice).
      SetDiceMigrationCompleted();
      account_consistency_ = AccountConsistencyMethod::kDice;
    }

    if (account_consistency_ == AccountConsistencyMethod::kDiceMigration) {
      UMA_HISTOGRAM_BOOLEAN(
          "Signin.TokenServiceDiceCompatible",
          prefs->GetBoolean(prefs::kTokenServiceDiceCompatible));
    }
  }

  UMA_HISTOGRAM_ENUMERATION(kDiceMigrationStatusHistogram,
                            GetDiceMigrationStatus(account_consistency_),
                            DiceMigrationStatus::kDiceMigrationStatusCount);
#endif

  DCHECK_EQ(account_consistency_, ComputeAccountConsistencyMethod(profile_));
  account_consistency_initialized_ = true;
}

AccountConsistencyModeManager::~AccountConsistencyModeManager() {}

// static
void AccountConsistencyModeManager::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  registry->RegisterBooleanPref(kDiceMigrationOnStartupPref, false);
  registry->RegisterBooleanPref(kDiceMigrationCompletePref, false);
#endif
#if defined(OS_CHROMEOS)
  registry->RegisterBooleanPref(prefs::kAccountConsistencyMirrorRequired,
                                false);
#endif
  registry->RegisterBooleanPref(prefs::kSigninAllowedOnNextStartup, true);
}

// static
AccountConsistencyMethod AccountConsistencyModeManager::GetMethodForProfile(
    Profile* profile) {
  if (!ShouldBuildServiceForProfile(profile))
    return AccountConsistencyMethod::kDisabled;

  return AccountConsistencyModeManager::GetForProfile(profile)
      ->GetAccountConsistencyMethod();
}

// static
bool AccountConsistencyModeManager::IsDiceEnabledForProfile(Profile* profile) {
  return GetMethodForProfile(profile) == AccountConsistencyMethod::kDice;
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
void AccountConsistencyModeManager::SetDiceMigrationCompleted() {
  VLOG(1) << "Dice migration completed.";
  profile_->GetPrefs()->SetBoolean(kDiceMigrationCompletePref, true);
}

void AccountConsistencyModeManager::SetReadyForDiceMigration(bool is_ready) {
  SetDiceMigrationOnStartup(profile_->GetPrefs(), is_ready);
}

// static
void AccountConsistencyModeManager::SetDiceMigrationOnStartup(
    PrefService* prefs,
    bool migrate) {
  VLOG(1) << "Dice migration on next startup: " << migrate;
  prefs->SetBoolean(kDiceMigrationOnStartupPref, migrate);
}

// static
bool AccountConsistencyModeManager::IsReadyForDiceMigration(Profile* profile) {
  return ShouldBuildServiceForProfile(profile) &&
         (profile->IsNewProfile() ||
          profile->GetPrefs()->GetBoolean(kDiceMigrationOnStartupPref));
}

// static
bool AccountConsistencyModeManager::IsDiceMigrationCompleted(Profile* profile) {
  return profile->GetPrefs()->GetBoolean(kDiceMigrationCompletePref);
}
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

// static
bool AccountConsistencyModeManager::IsMirrorEnabledForProfile(
    Profile* profile) {
  return GetMethodForProfile(profile) == AccountConsistencyMethod::kMirror;
}

// static
void AccountConsistencyModeManager::SetIgnoreMissingOAuthClientForTesting() {
  ignore_missing_oauth_client_for_testing_ = true;
}

// static
bool AccountConsistencyModeManager::ShouldBuildServiceForProfile(
    Profile* profile) {
  // IsGuestSession() returns true for the ProfileImpl associated with Guest
  // profiles. This profile manually sets the kSigninAllowed prference, which
  // causes crashes if the AccountConsistencyModeManager is instantiated. See
  // https://crbug.com/940026
  return profile->IsRegularProfile() && !profile->IsGuestSession() &&
         !profile->IsSystemProfile();
}

AccountConsistencyMethod
AccountConsistencyModeManager::GetAccountConsistencyMethod() {
#if defined(OS_CHROMEOS)
  // TODO(https://crbug.com/860671): ChromeOS should use the cached value.
  // Changing the value dynamically is not supported.
  return ComputeAccountConsistencyMethod(profile_);
#else
  // The account consistency method should not change during the lifetime of a
  // profile. We always return the cached value, but still check that it did not
  // change, in order to detect inconsisent states. See https://crbug.com/860471
  CHECK(account_consistency_initialized_);
  CHECK_EQ(ComputeAccountConsistencyMethod(profile_), account_consistency_);
  return account_consistency_;
#endif
}

// static
signin::AccountConsistencyMethod
AccountConsistencyModeManager::ComputeAccountConsistencyMethod(
    Profile* profile) {
  DCHECK(ShouldBuildServiceForProfile(profile));

#if BUILDFLAG(ENABLE_MIRROR)
  return AccountConsistencyMethod::kMirror;
#endif

  std::string method_value = base::GetFieldTrialParamValueByFeature(
      kAccountConsistencyFeature, kAccountConsistencyFeatureMethodParameter);

#if defined(OS_CHROMEOS)
  if (chromeos::IsAccountManagerAvailable(profile))
    return AccountConsistencyMethod::kMirror;

  return (method_value == kAccountConsistencyFeatureMethodMirror ||
          profile->GetPrefs()->GetBoolean(
              prefs::kAccountConsistencyMirrorRequired))
             ? AccountConsistencyMethod::kMirror
             : AccountConsistencyMethod::kDisabled;
#endif

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  AccountConsistencyMethod method = AccountConsistencyMethod::kDiceMigration;

  if (method_value == kAccountConsistencyFeatureMethodDiceMigration)
    method = AccountConsistencyMethod::kDiceMigration;
  else if (method_value == kAccountConsistencyFeatureMethodDice)
    method = AccountConsistencyMethod::kDice;

  DCHECK(signin::DiceMethodGreaterOrEqual(
      method, AccountConsistencyMethod::kDiceMigration));

  // Legacy supervised users cannot get Dice.
  // TODO(droger): remove this once legacy supervised users are no longer
  // supported.
  if (profile->IsLegacySupervised())
    return AccountConsistencyMethod::kDisabled;

  bool can_enable_dice_for_build = ignore_missing_oauth_client_for_testing_ ||
                                   google_apis::HasOAuthClientConfigured();
  if (!can_enable_dice_for_build) {
    LOG(WARNING) << "Desktop Identity Consistency cannot be enabled as no "
                    "OAuth client ID and client secret have been configured.";
    return AccountConsistencyMethod::kDisabled;
  }

  if (!profile->GetPrefs()->GetBoolean(prefs::kSigninAllowed)) {
    VLOG(1) << "Desktop Identity Consistency disabled as sign-in to Chrome"
               "is not allowed";
    return AccountConsistencyMethod::kDisabled;
  }

  if (method == AccountConsistencyMethod::kDiceMigration) {
    if (IsDiceMigrationCompleted(profile))
      return AccountConsistencyMethod::kDice;

    if (base::FeatureList::IsEnabled(kForceDiceMigration))
      return AccountConsistencyMethod::kDice;
  }

  return method;
#endif

  NOTREACHED();
  return AccountConsistencyMethod::kDisabled;
}
