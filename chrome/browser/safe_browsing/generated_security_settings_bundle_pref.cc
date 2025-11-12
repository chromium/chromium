// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/generated_security_settings_bundle_pref.h"

#include "base/types/cxx23_to_underlying.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/settings_private.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"

namespace settings_api = extensions::api::settings_private;

namespace safe_browsing {

const char kGeneratedSecuritySettingsBundlePref[] =
    "generated.security_settings_bundle";

GeneratedSecuritySettingsBundlePref::GeneratedSecuritySettingsBundlePref(
    Profile* profile)
    : profile_(profile) {
  user_prefs_registrar_.Init(profile->GetPrefs());
  user_prefs_registrar_.Add(
      prefs::kSecuritySettingsBundle,
      base::BindRepeating(&GeneratedSecuritySettingsBundlePref::
                              OnSecuritySettingsBundlePreferencesChanged,
                          base::Unretained(this)));
}

extensions::settings_private::SetPrefResult
GeneratedSecuritySettingsBundlePref::SetPref(const base::Value* value) {
  if (!value->is_int()) {
    return extensions::settings_private::SetPrefResult::PREF_TYPE_MISMATCH;
  }

  auto selection = static_cast<int>(value->GetInt());

  if (selection != static_cast<int>(SecuritySettingsBundleSetting::STANDARD) &&
      selection != static_cast<int>(SecuritySettingsBundleSetting::ENHANCED)) {
    return extensions::settings_private::SetPrefResult::PREF_TYPE_MISMATCH;
  }

  // Update Security Settings Bundle preference to match selection.
  profile_->GetPrefs()->SetInteger(prefs::kSecuritySettingsBundle, selection);

  return extensions::settings_private::SetPrefResult::SUCCESS;
}

extensions::api::settings_private::PrefObject
GeneratedSecuritySettingsBundlePref::GetPrefObject() const {
  extensions::api::settings_private::PrefObject pref_object;
  pref_object.key = kGeneratedSecuritySettingsBundlePref;
  pref_object.type = extensions::api::settings_private::PrefType::kNumber;

  auto security_settings_bundle_enabled =
      profile_->GetPrefs()->GetInteger(prefs::kSecuritySettingsBundle);

  if (security_settings_bundle_enabled ==
      static_cast<int>(SecuritySettingsBundleSetting::ENHANCED)) {
    pref_object.value =
        base::Value(static_cast<int>(SecuritySettingsBundleSetting::ENHANCED));
  } else {
    pref_object.value =
        base::Value(static_cast<int>(SecuritySettingsBundleSetting::STANDARD));
  }

  return pref_object;
}

void GeneratedSecuritySettingsBundlePref::
    OnSecuritySettingsBundlePreferencesChanged() {
  NotifyObservers(kGeneratedSecuritySettingsBundlePref);
}

}  // namespace safe_browsing
