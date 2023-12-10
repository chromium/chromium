// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/generated_https_first_mode_pref.h"

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "chrome/browser/extensions/api/settings_private/prefs_util_enums.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager_factory.h"
#include "chrome/browser/ssl/https_first_mode_settings_tracker.h"
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
  user_prefs_registrar_.Add(
      prefs::kHttpsFirstModeIncognito,
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

// Convert the setting selection into values for the two underlying boolean
// prefs.
extensions::settings_private::SetPrefResult
GeneratedHttpsFirstModePref::SetPref(const base::Value* value) {
  if (!value->is_int()) {
    return extensions::settings_private::SetPrefResult::PREF_TYPE_MISMATCH;
  }

  auto selection = static_cast<HttpsFirstModeSetting>(value->GetInt());

  if (selection != HttpsFirstModeSetting::kDisabled &&
      selection != HttpsFirstModeSetting::kEnabledIncognito &&
      selection != HttpsFirstModeSetting::kEnabledFull) {
    return extensions::settings_private::SetPrefResult::PREF_TYPE_MISMATCH;
  }

  if (!base::FeatureList::IsEnabled(features::kHttpsFirstModeIncognito) &&
      selection == HttpsFirstModeSetting::kEnabledIncognito) {
    return extensions::settings_private::SetPrefResult::PREF_TYPE_UNSUPPORTED;
  }

  // kHttpsOnlyModeEnabled is considered the canonical source for HFM
  // management. Policy enforcement turns it fully on or fully off.
  const PrefService::Preference* enabled_pref =
      profile_->GetPrefs()->FindPreference(prefs::kHttpsOnlyModeEnabled);
  if (!enabled_pref->IsUserModifiable()) {
    return extensions::settings_private::SetPrefResult::PREF_NOT_MODIFIABLE;
  }

  // Update both HTTPS-First Mode preferences to match the selection.
  //
  // Note that the HttpsFirstModeSetting::kEnabledIncognito is not available by
  // default. If the feature flag is disabled, then the kEnabledFull and
  // kDisabled settings will only be mapped to the kHttpsOnlyModeEnabled pref.
  if (base::FeatureList::IsEnabled(features::kHttpsFirstModeIncognito)) {
    profile_->GetPrefs()->SetBoolean(
        prefs::kHttpsFirstModeIncognito,
        selection != HttpsFirstModeSetting::kDisabled);
  }
  profile_->GetPrefs()->SetBoolean(
      prefs::kHttpsOnlyModeEnabled,
      selection == HttpsFirstModeSetting::kEnabledFull);

  return extensions::settings_private::SetPrefResult::SUCCESS;
}

// Convert the underlying boolean prefs into the setting selection.
settings_api::PrefObject GeneratedHttpsFirstModePref::GetPrefObject() const {
  bool is_advanced_protection_enabled =
      safe_browsing::AdvancedProtectionStatusManagerFactory::GetForProfile(
          profile_)
          ->IsUnderAdvancedProtection();

  // prefs::kHttpsOnlyModeEnabled is the backing pref that can be controlled by
  // enterprise policy.
  auto* hfm_fully_enabled_pref =
      profile_->GetPrefs()->FindPreference(prefs::kHttpsOnlyModeEnabled);

  // The HttpsFirstModeSetting::kEnabledIncognito setting is not available  by
  // default -- if the feature flag is disabled, then the kEnabledFull and
  // kDisabled settings will be mapped to the kHttpsOnlyModeEnabled pref on its
  // own.
  bool hfm_incognito_enabled =
      base::FeatureList::IsEnabled(features::kHttpsFirstModeIncognito) &&
      profile_->GetPrefs()->GetBoolean(prefs::kHttpsFirstModeIncognito);

  bool fully_enabled = hfm_fully_enabled_pref->GetValue()->GetBool() ||
                       is_advanced_protection_enabled;

  settings_api::PrefObject pref_object;
  pref_object.key = kGeneratedHttpsFirstModePref;
  pref_object.type = settings_api::PrefType::kNumber;

  // Map the two boolean prefs to the enum setting states.
  if (fully_enabled) {
    pref_object.value =
        base::Value(static_cast<int>(HttpsFirstModeSetting::kEnabledFull));
  } else if (hfm_incognito_enabled) {
    pref_object.value =
        base::Value(static_cast<int>(HttpsFirstModeSetting::kEnabledIncognito));
  } else {
    pref_object.value =
        base::Value(static_cast<int>(HttpsFirstModeSetting::kDisabled));
  }

  pref_object.user_control_disabled = is_advanced_protection_enabled;

  if (!hfm_fully_enabled_pref->IsUserModifiable()) {
    // The pref was controlled by the enterprise policy.
    pref_object.enforcement = settings_api::Enforcement::kEnforced;
    extensions::settings_private::GeneratedPref::ApplyControlledByFromPref(
        &pref_object, hfm_fully_enabled_pref);
  } else if (hfm_fully_enabled_pref->GetRecommendedValue()) {
    // Policy can set a recommended setting of fully enabled or fully disabled.
    // Map those to the enum values.
    pref_object.enforcement = settings_api::Enforcement::kRecommended;
    if (hfm_fully_enabled_pref->GetRecommendedValue()->GetBool()) {
      pref_object.recommended_value =
          base::Value(static_cast<int>(HttpsFirstModeSetting::kEnabledFull));
    } else {
      // TODO(crbug.com/1494186): Consider supporting a recommended value of
      // kEnabledIncognito after the enterprise policy support is updated.
      pref_object.recommended_value =
          base::Value(static_cast<int>(HttpsFirstModeSetting::kDisabled));
    }
  }

  return pref_object;
}
