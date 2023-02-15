// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/intent_helper/chrome_arc_settings_app_delegate.h"

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
    case mojom::AndroidSetting::kGeoLocation:
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

}  // namespace arc
