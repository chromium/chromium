// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/access_code/access_code_cast_feature.h"

#include "base/command_line.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_cast_constants.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/user_prefs/user_prefs.h"

#if !BUILDFLAG(IS_ANDROID)
#include "components/prefs/pref_registry_simple.h"
#endif

namespace features {
// Enables remembering of access code cast devices.
const base::Feature kAccessCodeCastRememberDevices{
    "AccessCodeCastRememberDevices", base::FEATURE_ENABLED_BY_DEFAULT};
}  // namespace features

namespace media_router {

#if !BUILDFLAG(IS_ANDROID)

void RegisterAccessCodeProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kAccessCodeCastEnabled, false,
                                PrefRegistry::PUBLIC);
  registry->RegisterIntegerPref(prefs::kAccessCodeCastDeviceDuration, 0,
                                PrefRegistry::PUBLIC);
  registry->RegisterDictionaryPref(prefs::kAccessCodeCastDevices);
  registry->RegisterDictionaryPref(prefs::kAccessCodeCastDeviceAdditionTime);
}

bool GetAccessCodeCastEnabledPref(PrefService* pref_service) {
  return pref_service->GetBoolean(prefs::kAccessCodeCastEnabled);
}

base::TimeDelta GetAccessCodeDeviceDurationPref(PrefService* pref_service) {
  if (!GetAccessCodeCastEnabledPref(pref_service) ||
      !base::FeatureList::IsEnabled(features::kAccessCodeCastRememberDevices)) {
    return base::Seconds(0);
  }

  // Check to see if the command line was used to set the value.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (IsCommandLineSwitchSupported() &&
      command_line->HasSwitch(switches::kAccessCodeCastDeviceDurationSwitch)) {
    int value;
    base::StringToInt(command_line->GetSwitchValueASCII(
                          switches::kAccessCodeCastDeviceDurationSwitch),
                      &value);
    return base::Seconds(value);
  }
  // Return the value set by the policy pref.
  return base::Seconds(
      pref_service->GetInteger(prefs::kAccessCodeCastDeviceDuration));
}

#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace media_router
