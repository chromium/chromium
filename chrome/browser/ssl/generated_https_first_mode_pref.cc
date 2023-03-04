// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/generated_https_first_mode_pref.h"

#include "base/feature_list.h"
#include "base/logging.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/extensions/api/settings_private.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

namespace settings_api = extensions::api::settings_private;

const char kGeneratedHttpsFirstModePref[] =
    "generated.https_first_mode_enabled";

GeneratedHttpsFirstModePref::GeneratedHttpsFirstModePref(Profile* profile)
    : profile_(profile) {
  user_prefs_registrar_.Init(profile->GetPrefs());
  user_prefs_registrar_.Add(
      prefs::kHttpsOnlyModeEnabled,
      base::BindRepeating(
          &GeneratedHttpsFirstModePref::OnSourcePreferencesChanged,
          base::Unretained(this)));

  // Track Advanced Protection status.
  if (base::FeatureList::IsEnabled(
          features::kHttpsFirstModeForAdvancedProtectionUsers)) {
    obs_.Observe(
        safe_browsing::AdvancedProtectionStatusManagerFactory::GetForProfile(
            profile_));
    // On startup, AdvancedProtectionStatusManager runs before this class so we
    // don't get called back. Run the callback to get the AP setting.
    OnAdvancedProtectionStatusChanged(
        safe_browsing::AdvancedProtectionStatusManagerFactory::GetForProfile(
            profile_)
            ->IsUnderAdvancedProtection());
  }
}

GeneratedHttpsFirstModePref::~GeneratedHttpsFirstModePref() = default;

void GeneratedHttpsFirstModePref::OnSourcePreferencesChanged() {
  NotifyObservers(kGeneratedHttpsFirstModePref);
}

void GeneratedHttpsFirstModePref::OnAdvancedProtectionStatusChanged(
    bool enabled) {
  DCHECK(base::FeatureList::IsEnabled(
      features::kHttpsFirstModeForAdvancedProtectionUsers));
  NotifyObservers(kGeneratedHttpsFirstModePref);
}

extensions::settings_private::SetPrefResult
GeneratedHttpsFirstModePref::SetPref(const base::Value* value) {
  if (!value->is_bool()) {
    return extensions::settings_private::SetPrefResult::PREF_TYPE_MISMATCH;
  }

  if (!profile_->GetPrefs()
           ->FindPreference(prefs::kHttpsOnlyModeEnabled)
           ->IsUserModifiable()) {
    return extensions::settings_private::SetPrefResult::PREF_NOT_MODIFIABLE;
  }

  profile_->GetPrefs()->SetBoolean(prefs::kHttpsOnlyModeEnabled,
                                   value->GetBool());
  return extensions::settings_private::SetPrefResult::SUCCESS;
}

settings_api::PrefObject GeneratedHttpsFirstModePref::GetPrefObject() const {
  bool is_advanced_protection_enabled =
      safe_browsing::AdvancedProtectionStatusManagerFactory::GetForProfile(
          profile_)
          ->IsUnderAdvancedProtection();

  auto* backing_preference =
      profile_->GetPrefs()->FindPreference(prefs::kHttpsOnlyModeEnabled);

  settings_api::PrefObject pref_object;
  pref_object.key = kGeneratedHttpsFirstModePref;
  pref_object.type = settings_api::PREF_TYPE_BOOLEAN;
  pref_object.value = base::Value(backing_preference->GetValue()->GetBool() ||
                                  is_advanced_protection_enabled);
  pref_object.user_control_disabled = is_advanced_protection_enabled;

  if (!backing_preference->IsUserModifiable()) {
    // The pref was disabled by the enterprise policy.
    pref_object.enforcement = settings_api::Enforcement::ENFORCEMENT_ENFORCED;
    extensions::settings_private::GeneratedPref::ApplyControlledByFromPref(
        &pref_object, backing_preference);
  } else if (backing_preference->GetRecommendedValue()) {
    pref_object.enforcement =
        settings_api::Enforcement::ENFORCEMENT_RECOMMENDED;
    pref_object.recommended_value =
        base::Value(backing_preference->GetRecommendedValue()->GetBool());
  }

  return pref_object;
}
