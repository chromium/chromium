// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/incognito_mode_prefs.h"

#include <stdint.h>

#include "base/command_line.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
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

// static
// Sadly, this is required until c++17.
constexpr IncognitoModePrefs::Availability
    IncognitoModePrefs::kDefaultAvailability;

// static
bool IncognitoModePrefs::IntToAvailability(int in_value,
                                           Availability* out_value) {
  if (in_value < 0 || in_value >= static_cast<int>(Availability::kNumTypes)) {
    *out_value = kDefaultAvailability;
    return false;
  }
  *out_value = static_cast<Availability>(in_value);
  return true;
}

// static
IncognitoModePrefs::Availability IncognitoModePrefs::GetAvailability(
    const PrefService* pref_service) {
  return GetAvailabilityInternal(pref_service, CHECK_PARENTAL_CONTROLS);
}

// static
void IncognitoModePrefs::SetAvailability(PrefService* prefs,
                                         const Availability availability) {
  prefs->SetInteger(prefs::kIncognitoModeAvailability,
                    static_cast<int>(availability));
}

// static
void IncognitoModePrefs::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterIntegerPref(prefs::kIncognitoModeAvailability,
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
  // Note: This code only checks parental controls if the user requested
  // to launch in incognito mode or if it was forced via prefs. This way,
  // the parental controls check (which can be quite slow) can be avoided
  // most of the time.
  bool should_use_incognito =
      command_line.HasSwitch(switches::kIncognito) ||
      GetAvailabilityInternal(prefs, DONT_CHECK_PARENTAL_CONTROLS) ==
          IncognitoModePrefs::Availability::kForced;
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  auto* init_params = chromeos::BrowserParamsProxy::Get();
  should_use_incognito |=
      init_params->InitialBrowserAction() ==
      crosapi::mojom::InitialBrowserAction::kOpenIncognitoWindow;
#endif
  return should_use_incognito &&
         GetAvailabilityInternal(prefs, CHECK_PARENTAL_CONTROLS) !=
             IncognitoModePrefs::Availability::kDisabled;
}

// static
bool IncognitoModePrefs::CanOpenBrowser(Profile* profile) {
  switch (GetAvailability(profile->GetPrefs())) {
    case IncognitoModePrefs::Availability::kEnabled:
      return true;

    case IncognitoModePrefs::Availability::kDisabled:
      return !profile->IsIncognitoProfile();

    case IncognitoModePrefs::Availability::kForced:
      return profile->IsIncognitoProfile();

    default:
      NOTREACHED();
      return false;
  }
}

// static
bool IncognitoModePrefs::IsIncognitoAllowed(Profile* profile) {
  return !profile->IsGuestSession() &&
         IncognitoModePrefs::GetAvailability(profile->GetPrefs()) !=
             IncognitoModePrefs::Availability::kDisabled;
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
IncognitoModePrefs::Availability IncognitoModePrefs::GetAvailabilityInternal(
    const PrefService* pref_service,
    GetAvailabilityMode mode) {
  DCHECK(pref_service);
  int pref_value = pref_service->GetInteger(prefs::kIncognitoModeAvailability);
  Availability result = kDefaultAvailability;
  bool valid = IntToAvailability(pref_value, &result);
  DCHECK(valid);
  if (result != IncognitoModePrefs::Availability::kDisabled &&
      mode == CHECK_PARENTAL_CONTROLS && ArePlatformParentalControlsEnabled()) {
    if (result == IncognitoModePrefs::Availability::kForced)
      LOG(ERROR) << "Ignoring FORCED incognito. Parental control logging on";
    return IncognitoModePrefs::Availability::kDisabled;
  }
  return result;
}
