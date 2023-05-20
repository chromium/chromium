// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/access_code/access_code_cast_feature.h"

#include "base/command_line.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_cast_constants.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/profiles/profile.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/user_prefs/user_prefs.h"

#if !BUILDFLAG(IS_ANDROID)
#include "components/prefs/pref_registry_simple.h"
#endif

namespace features {
// Enables remembering of access code cast devices.
BASE_FEATURE(kAccessCodeCastRememberDevices,
             "AccessCodeCastRememberDevices",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Provide a tab switching UI bar while casting (mirroring) when AccessCodeCast
// is enabled.
BASE_FEATURE(kAccessCodeCastTabSwitchingUI,
             "AccessCodeCastTabSwitchingUI",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Provide functionality to freeze the casting session when AccessCodeCast is
// enabled.
BASE_FEATURE(kAccessCodeCastFreezeUI,
             "AccessCodeCastFreezeUI",
             base::FEATURE_ENABLED_BY_DEFAULT);

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

bool GetAccessCodeCastEnabledPref(Profile* profile) {
  return profile->GetPrefs()->GetBoolean(prefs::kAccessCodeCastEnabled) &&
         MediaRouterEnabled(profile);
}

base::TimeDelta GetAccessCodeDeviceDurationPref(Profile* profile) {
  if (!GetAccessCodeCastEnabledPref(profile) ||
      !base::FeatureList::IsEnabled(features::kAccessCodeCastRememberDevices)) {
    return base::Seconds(0);
  }

  // Return the value set by the policy pref.
  return base::Seconds(
      profile->GetPrefs()->GetInteger(prefs::kAccessCodeCastDeviceDuration));
}

bool IsAccessCodeCastTabSwitchingUiEnabled(Profile* profile) {
  return profile && GetAccessCodeCastEnabledPref(profile) &&
         base::FeatureList::IsEnabled(features::kAccessCodeCastTabSwitchingUI);
}

bool IsAccessCodeCastFreezeUiEnabled(Profile* profile) {
  return profile && GetAccessCodeCastEnabledPref(profile) &&
         base::FeatureList::IsEnabled(features::kAccessCodeCastFreezeUI);
}

#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace media_router
