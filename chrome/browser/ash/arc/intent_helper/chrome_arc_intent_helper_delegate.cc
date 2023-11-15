// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/intent_helper/chrome_arc_intent_helper_delegate.h"

#include <string>

#include "ash/components/arc/arc_prefs.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/system/privacy_hub/privacy_hub_controller.h"
#include "base/logging.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"

namespace arc {

ChromeArcIntentHelperDelegate::ChromeArcIntentHelperDelegate(Profile* profile)
    : profile_(profile) {}

ChromeArcIntentHelperDelegate::~ChromeArcIntentHelperDelegate() = default;

void ChromeArcIntentHelperDelegate::ResetArc() {
  ArcSessionManager::Get()->RequestArcDataRemoval();
  ArcSessionManager::Get()->StopAndEnableArc();
}

void ChromeArcIntentHelperDelegate::HandleUpdateAndroidSettings(
    mojom::AndroidSetting setting,
    bool is_enabled) {
  switch (setting) {
    // We need kGeoLocationAtBoot and kGeoLocationUserTriggered to avoid race
    // condition when we are trying to sync location setting from ChromeOS to
    // android as well as user trigger comes due to change in
    // kArcLocationServiceEnabled.
    // This is potentially seen in CTS/GTS test and hence splitting the two
    // update.
    case mojom::AndroidSetting::kGeoLocationAtBoot:
      if (IsInitialLocationSettingsSyncRequired()) {
        // This is to handle scenario when we migrate from existing settings.
        // Currently, android has location settings whereas ChromeOS doesn't.
        // We want to migrate the android setting one time from android to
        // ChromeOS.
        VLOG(1) << "Syncing initial location settings from Android.";
        UpdateLocationSettings(is_enabled);
        profile_->GetPrefs()->SetBoolean(
            prefs::kArcInitialLocationSettingSyncRequired, false);
      }
      return;
    case mojom::AndroidSetting::kGeoLocation:
    case mojom::AndroidSetting::kGeoLocationUserTriggered:
      // This path is also executed when location change is triggered from
      // ChromeOS. Android apps only prompt users to enable geolocation, so we
      // can simply drop the disable events, which creates ambiguity (whether
      // it's "Blocked for all" or "Only allowed for system services").
      // TODO(b/310168397): Redesign to avoid "disable" event filtration.
      if (is_enabled) {
        UpdateLocationSettings(is_enabled);
      }
      return;
    case mojom::AndroidSetting::kUnknown:
      break;
  }
  NOTREACHED() << "Unknown Android Setting: " << setting;
}

void ChromeArcIntentHelperDelegate::UpdateLocationSettings(bool is_enabled) {
  CHECK(profile_);
  VLOG(1) << "UpdateLocation toggle called with value: " << is_enabled;

  ash::GeolocationAccessLevel access_level_for_cros =
      ash::PrivacyHubController::ArcToCrosGeolocationPermissionMapping(
          is_enabled);
  profile_->GetPrefs()->SetInteger(ash::prefs::kUserGeolocationAccessLevel,
                                   static_cast<int>(access_level_for_cros));
}

bool ChromeArcIntentHelperDelegate::IsInitialLocationSettingsSyncRequired() {
  CHECK(profile_);
  return profile_->GetPrefs()->GetBoolean(
      prefs::kArcInitialLocationSettingSyncRequired);
}

}  // namespace arc
