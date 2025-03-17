// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/login_screen/login/login_api_prefs.h"

#include "ash/constants/ash_pref_names.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace extensions::login_api {

void RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kLoginExtensionApiDataForNextLoginAttempt,
                               "");
}

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterListPref(
      prefs::kRestrictedManagedGuestSessionExtensionCleanupExemptList);

  // This pref is a per-session pref and must not be synced.
  registry->RegisterBooleanPref(
      ash::prefs::kLoginExtensionApiCanLockManagedGuestSession, false,
      PrefRegistry::NO_REGISTRATION_FLAGS);
}

}  // namespace extensions::login_api
