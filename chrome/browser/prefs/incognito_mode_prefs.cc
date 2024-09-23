// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/incognito_mode_prefs.h"

#include <stdint.h>

#include "base/command_line.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/win/parental_controls.h"
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/android/partner_browser_customizations.h"
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/startup/browser_params_proxy.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

using policy::IncognitoModeAvailability;

// static
const IncognitoModeAvailability IncognitoModePrefs::kDefaultAvailability =
    policy::IncognitoModeAvailability::kEnabled;

// static
bool IncognitoModePrefs::IntToAvailability(
    int in_value,
    IncognitoModeAvailability* out_value) {
  if (in_value < 0 ||
      in_value >= static_cast<int>(IncognitoModeAvailability::kNumTypes)) {
    *out_value = kDefaultAvailability;
    return false;
  }
  *out_value = static_cast<IncognitoModeAvailability>(in_value);
  return true;
}

// static
IncognitoModeAvailability IncognitoModePrefs::GetAvailability(
    const PrefService* pref_service) {
  return GetAvailabilityInternal(pref_service, CHECK_PARENTAL_CONTROLS);
}

// static
void IncognitoModePrefs::SetAvailability(
    PrefService* prefs,
    const IncognitoModeAvailability availability) {
  prefs->SetInteger(policy::policy_prefs::kIncognitoModeAvailability,
                    static_cast<int>(availability));
}

// static
void IncognitoModePrefs::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterIntegerPref(
      policy::policy_prefs::kIncognitoModeAvailability,
      static_cast<int>(kDefaultAvailability));
#if BUILDFLAG(IS_ANDROID)
  registry->RegisterBooleanPref(prefs::kIncognitoReauthenticationForAndroid,
                                false);
#endif
}

// static
bool IncognitoModePrefs::ShouldLaunchIncognito(
    const base::CommandLine& command_line,
    const PrefService* prefs) {
  return ShouldLaunchIncognitoInternal(command_line, prefs, false);
}

// static
bool IncognitoModePrefs::ShouldOpenSubsequentBrowsersInIncognito(
    const base::CommandLine& command_line,
    const PrefService* prefs) {
  return ShouldLaunchIncognitoInternal(command_line, prefs, true);
}

// static
bool IncognitoModePrefs::CanOpenBrowser(Profile* profile) {
  switch (GetAvailability(profile->GetPrefs())) {
    case IncognitoModeAvailability::kEnabled:
      return true;

    case IncognitoModeAvailability::kDisabled:
      return !profile->IsIncognitoProfile();

    case IncognitoModeAvailability::kForced:
      return profile->IsIncognitoProfile();

    default:
      NOTREACHED_IN_MIGRATION();
      return false;
  }
}

// static
bool IncognitoModePrefs::IsIncognitoAllowed(Profile* profile) {
  return !profile->IsGuestSession() &&
         IncognitoModePrefs::GetAvailability(profile->GetPrefs()) !=
             IncognitoModeAvailability::kDisabled;
}

// static
bool IncognitoModePrefs::ArePlatformParentalControlsEnabled() {
#if BUILDFLAG(IS_WIN)
  return GetWinParentalControls().logging_required;
#elif BUILDFLAG(IS_ANDROID)
  return chrome::android::PartnerBrowserCustomizations::IsIncognitoDisabled();
#else
  return false;
#endif
}

// static
IncognitoModeAvailability IncognitoModePrefs::GetAvailabilityInternal(
    const PrefService* pref_service,
    GetAvailabilityMode mode) {
  DCHECK(pref_service);
  int pref_value = pref_service->GetInteger(
      policy::policy_prefs::kIncognitoModeAvailability);
  IncognitoModeAvailability result = kDefaultAvailability;
  bool valid = IntToAvailability(pref_value, &result);
  DCHECK(valid);
  if (result != IncognitoModeAvailability::kDisabled &&
      mode == CHECK_PARENTAL_CONTROLS && ArePlatformParentalControlsEnabled()) {
    if (result == IncognitoModeAvailability::kForced) {
      LOG(ERROR) << "Ignoring FORCED incognito. Parental control logging on";
    }
    return IncognitoModeAvailability::kDisabled;
  }
  return result;
}

// static
bool IncognitoModePrefs::ShouldLaunchIncognitoInternal(
    const base::CommandLine& command_line,
    const PrefService* prefs,
    const bool for_subsequent_browsers) {
  // Note: This code only checks parental controls if the user requested
  // to launch in incognito mode or if it was forced via prefs. This way,
  // the parental controls check (which can be quite slow) can be avoided
  // most of the time.
  bool forced_by_switch = command_line.HasSwitch(switches::kIncognito);
  if (for_subsequent_browsers) {
    forced_by_switch =
        forced_by_switch &&
        browser_defaults::
            kAlwaysOpenIncognitoBrowserIfStartedWithIncognitoSwitch;
  }
  bool should_use_incognito =
      forced_by_switch ||
      GetAvailabilityInternal(prefs, DONT_CHECK_PARENTAL_CONTROLS) ==
          IncognitoModeAvailability::kForced;
  return should_use_incognito &&
         GetAvailabilityInternal(prefs, CHECK_PARENTAL_CONTROLS) !=
             IncognitoModeAvailability::kDisabled;
}
