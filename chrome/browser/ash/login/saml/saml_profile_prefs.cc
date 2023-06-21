// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/saml/saml_profile_prefs.h"

#include "chrome/browser/ash/login/login_constants.h"
#include "chrome/browser/ash/login/login_pref_names.h"
#include "chromeos/ash/components/login/auth/public/saml_password_attributes.h"
#include "components/prefs/pref_registry_simple.h"

namespace ash {

void RegisterSamlProfilePrefs(PrefRegistrySimple* registry) {
  // All SAML prefs are not syncable by default. In order to make a new pref
  // syncable across user devices SYNCABLE_PREF must be set in the optional
  // flags argument of RegisterPref.
  registry->RegisterIntegerPref(
      prefs::kGaiaOfflineSigninTimeLimitDays,
      constants::kDefaultGaiaOfflineSigninTimeLimitDays);

  registry->RegisterIntegerPref(prefs::kSAMLOfflineSigninTimeLimit,
                                constants::kDefaultSAMLOfflineSigninTimeLimit);

  registry->RegisterIntegerPref(
      prefs::kGaiaLockScreenOfflineSigninTimeLimitDays,
      constants::kDefaultGaiaLockScreenOfflineSigninTimeLimitDays);

  registry->RegisterIntegerPref(
      prefs::kSamlLockScreenOfflineSigninTimeLimitDays,
      constants::kDefaultSamlLockScreenOfflineSigninTimeLimitDays);

  registry->RegisterBooleanPref(
      prefs::kSamlInSessionPasswordChangeEnabled,
      constants::kDefaultSamlInSessionPasswordChangeEnabled);
  registry->RegisterIntegerPref(
      prefs::kSamlPasswordExpirationAdvanceWarningDays,
      constants::kDefaultSamlPasswordExpirationAdvanceWarningDays);

  registry->RegisterBooleanPref(
      prefs::kLockScreenReauthenticationEnabled,
      constants::kDefaultLockScreenReauthenticationEnabled);

  SamlPasswordAttributes::RegisterProfilePrefs(registry);
}

}  // namespace ash
