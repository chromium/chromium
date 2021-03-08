// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/saml/saml_profile_prefs.h"

#include "chrome/browser/ash/login/login_pref_names.h"
#include "chromeos/login/auth/saml_password_attributes.h"
#include "components/prefs/pref_registry_simple.h"

namespace {

// The value -1 means that online authentication will not be enforced by
// `OfflineSigninLimiter` so the user will be allowed to use offline
// authentication until a different reason than this policy enforces an online
// login.
constexpr int kDefaultGaiaOfflineSigninTimeLimitDays = -1;
constexpr int kDefaultSAMLOfflineSigninTimeLimit =
    base::TimeDelta::FromDays(14).InSeconds();
constexpr int kDefaultGaiaLockScreenOfflineSigninTimeLimitDays = -1;
constexpr int kDefaultSamlLockScreenOfflineSigninTimeLimitDays = -1;

// In-session password-change feature (includes password expiry notifications).
const bool kDefaultSamlInSessionPasswordChangeEnabled = false;
const int kDefaultSamlPasswordExpirationAdvanceWarningDays = 14;

// Online reauthentication on the lock screen.
const bool kDefaultLockScreenReauthenticationEnabled = false;

}  // namespace

namespace chromeos {

void RegisterSamlProfilePrefs(PrefRegistrySimple* registry) {
  // All SAML prefs are not syncable by default. In order to make a new pref
  // syncable across user devices SYNCABLE_PREF must be set in the optional
  // flags argument of RegisterPref.
  registry->RegisterIntegerPref(prefs::kGaiaOfflineSigninTimeLimitDays,
                                kDefaultGaiaOfflineSigninTimeLimitDays);
  registry->RegisterTimePref(prefs::kGaiaLastOnlineSignInTime, base::Time());

  registry->RegisterIntegerPref(prefs::kSAMLOfflineSigninTimeLimit,
                                kDefaultSAMLOfflineSigninTimeLimit);
  registry->RegisterTimePref(prefs::kSAMLLastGAIASignInTime, base::Time());

  registry->RegisterIntegerPref(
      prefs::kGaiaLockScreenOfflineSigninTimeLimitDays,
      kDefaultGaiaLockScreenOfflineSigninTimeLimitDays);

  registry->RegisterIntegerPref(
      prefs::kSamlLockScreenOfflineSigninTimeLimitDays,
      kDefaultSamlLockScreenOfflineSigninTimeLimitDays);

  registry->RegisterBooleanPref(prefs::kSamlInSessionPasswordChangeEnabled,
                                kDefaultSamlInSessionPasswordChangeEnabled);
  registry->RegisterIntegerPref(
      prefs::kSamlPasswordExpirationAdvanceWarningDays,
      kDefaultSamlPasswordExpirationAdvanceWarningDays);

  registry->RegisterBooleanPref(prefs::kLockScreenReauthenticationEnabled,
                                kDefaultLockScreenReauthenticationEnabled);
  registry->RegisterStringPref(prefs::kSamlPasswordSyncToken, std::string());

  SamlPasswordAttributes::RegisterProfilePrefs(registry);
}

}  // namespace chromeos
