// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/intent_helper/chrome_arc_settings_app_delegate.h"

#include "ash/components/arc/arc_prefs.h"
#include "ash/constants/ash_pref_names.h"
#include "base/logging.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"

#include <string>

namespace arc {

ChromeArcSettingsAppDelegate::ChromeArcSettingsAppDelegate(Profile* profile)
    : profile_(profile) {}

ChromeArcSettingsAppDelegate::~ChromeArcSettingsAppDelegate() = default;

void ChromeArcSettingsAppDelegate::HandleUpdateAndroidSettings(
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
      UpdateLocationSettings(is_enabled);
      return;
    case mojom::AndroidSetting::kUnknown:
      break;
  }
  NOTREACHED() << "Unknown Android Setting: " << setting;
}

void ChromeArcSettingsAppDelegate::UpdateLocationSettings(bool is_enabled) {
  DCHECK(profile_);
  VLOG(1) << "UpdateLocation toggle called with value: " << is_enabled;
  profile_->GetPrefs()->SetBoolean(ash::prefs::kUserGeolocationAllowed,
                                   is_enabled);
}

bool ChromeArcSettingsAppDelegate::IsInitialLocationSettingsSyncRequired() {
  DCHECK(profile_);
  return profile_->GetPrefs()->GetBoolean(
      prefs::kArcInitialLocationSettingSyncRequired);
}
}  // namespace arc
