// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/generated_security_settings_bundle_pref.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/secure_dns_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/settings_private.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"

namespace safe_browsing {

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
  SetSecurityBundleSetting(
      *(profile_->GetPrefs()),
      static_cast<SecuritySettingsBundleSetting>(selection));

  PrefService* local_state = g_browser_process->local_state();

  // This is how the bundle controls each security setting.
  if (base::FeatureList::IsEnabled(
          safe_browsing::kBundledSecuritySettingsSecureDnsV2)) {
    // Secure DNS Setting.
    // TODO(crbug.com/460180440): migrate to a per-profile setting.
    const bool fallback_to_doh =
        selection == static_cast<int>(SecuritySettingsBundleSetting::ENHANCED);
    local_state->SetString(prefs::kDnsOverHttpsMode,
                           SecureDnsConfig::kModeAutomatic);
    local_state->SetString(prefs::kDnsOverHttpsTemplates, "");
    local_state->SetBoolean(prefs::kDnsOverHttpsAutomaticModeFallbackToDoh,
                            fallback_to_doh);
  }

  return extensions::settings_private::SetPrefResult::SUCCESS;
}

extensions::api::settings_private::PrefObject
GeneratedSecuritySettingsBundlePref::GetPrefObject() const {
  SecuritySettingsBundleSetting bundle_setting =
      GetSecurityBundleSetting(*profile_->GetPrefs());

  extensions::api::settings_private::PrefObject pref_object;
  pref_object.key = kGeneratedSecuritySettingsBundlePref;
  pref_object.type = extensions::api::settings_private::PrefType::kNumber;
  pref_object.value = base::Value(static_cast<int>(bundle_setting));
  return pref_object;
}

void GeneratedSecuritySettingsBundlePref::
    OnSecuritySettingsBundlePreferencesChanged() {
  NotifyObservers(kGeneratedSecuritySettingsBundlePref);
}

}  // namespace safe_browsing
