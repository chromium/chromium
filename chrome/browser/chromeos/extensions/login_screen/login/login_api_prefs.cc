// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/login_screen/login/login_api_prefs.h"

#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "components/prefs/pref_registry_simple.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace extensions::login_api {

#if BUILDFLAG(IS_CHROMEOS_ASH)
void RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kLoginExtensionApiDataForNextLoginAttempt,
                               "");
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterListPref(
      prefs::kRestrictedManagedGuestSessionExtensionCleanupExemptList);
}

}  // namespace extensions::login_api
