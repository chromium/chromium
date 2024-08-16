// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/generated_https_first_mode_pref.h"

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/extensions/api/settings_private/generated_pref.h"
#include "chrome/browser/extensions/api/settings_private/prefs_util_enums.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager_factory.h"
#include "chrome/browser/ssl/https_first_mode_settings_tracker.h"
#include "chrome/browser/ssl/https_upgrades_util.h"
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
      prefs::kHttpsFirstBalancedMode,
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
      selection != HttpsFirstModeSetting::kEnabledBalanced &&
      selection != HttpsFirstModeSetting::kEnabledFull) {
    return extensions::settings_private::SetPrefResult::PREF_TYPE_MISMATCH;
  }

  if (!IsBalancedModeAvailable() &&
      selection == HttpsFirstModeSetting::kEnabledBalanced) {
    return extensions::settings_private::SetPrefResult::PREF_TYPE_UNSUPPORTED;
  }

  // If the enterprise policy is enforced, then the kHttpsOnlyModeEnabled pref
  // will not be modifiable (for all policy values).
  const PrefService::Preference* fully_enabled_pref =
      profile_->GetPrefs()->FindPreference(prefs::kHttpsOnlyModeEnabled);
  if (!fully_enabled_pref->IsUserModifiable()) {
    return extensions::settings_private::SetPrefResult::PREF_NOT_MODIFIABLE;
  }

  // Update both HTTPS-First Mode preferences to match the selection.
  //
  // Note that the HttpsFirstModeSetting::kEnabledBalanced is not available by
  // default. If the feature flag is disabled, then the kEnabledFull and
  // kDisabled settings will only be mapped to the kHttpsOnlyModeEnabled pref.
  //
  // Note: The Security.HttpsFirstMode.SettingChanged* histograms are logged
  // here instead of in HttpsFirstModeService::OnHttpsFirstModePrefChanged()
  // because this will fire the pref observer _twice_, so logging the histogram
  // in the pref observer would cause double counting.
  if (IsBalancedModeAvailable()) {
    switch (selection) {
      case HttpsFirstModeSetting::kDisabled:
        base::UmaHistogramEnumeration("Security.HttpsFirstMode.SettingChanged2",
                                      HttpsFirstModeSetting::kDisabled);
        profile_->GetPrefs()->SetBoolean(prefs::kHttpsOnlyModeEnabled, false);
        profile_->GetPrefs()->SetBoolean(prefs::kHttpsFirstBalancedMode, false);
        break;
      case HttpsFirstModeSetting::kEnabledBalanced:
        base::UmaHistogramEnumeration("Security.HttpsFirstMode.SettingChanged2",
                                      HttpsFirstModeSetting::kEnabledBalanced);
        profile_->GetPrefs()->SetBoolean(prefs::kHttpsOnlyModeEnabled, false);
        profile_->GetPrefs()->SetBoolean(prefs::kHttpsFirstBalancedMode, true);
        break;
      case HttpsFirstModeSetting::kEnabledFull:
        base::UmaHistogramEnumeration("Security.HttpsFirstMode.SettingChanged2",
                                      HttpsFirstModeSetting::kEnabledFull);
        profile_->GetPrefs()->SetBoolean(prefs::kHttpsOnlyModeEnabled, true);
        profile_->GetPrefs()->SetBoolean(prefs::kHttpsFirstBalancedMode, false);
        break;
    }
  } else {
    // TODO(crbug.com/349860796): Remove old settings path once Balanced Mode
    // is launched.
    base::UmaHistogramBoolean("Security.HttpsFirstMode.SettingChanged",
                              selection == HttpsFirstModeSetting::kEnabledFull);
    profile_->GetPrefs()->SetBoolean(
        prefs::kHttpsOnlyModeEnabled,
        selection == HttpsFirstModeSetting::kEnabledFull);
  }

  return extensions::settings_private::SetPrefResult::SUCCESS;
}

// Convert the underlying boolean prefs into the setting selection.
settings_api::PrefObject GeneratedHttpsFirstModePref::GetPrefObject() const {
  bool is_advanced_protection_enabled =
      safe_browsing::AdvancedProtectionStatusManagerFactory::GetForProfile(
          profile_)
          ->IsUnderAdvancedProtection();

  auto* hfm_fully_enabled_pref =
      profile_->GetPrefs()->FindPreference(prefs::kHttpsOnlyModeEnabled);

  bool fully_enabled = hfm_fully_enabled_pref->GetValue()->GetBool() ||
                       is_advanced_protection_enabled;

  // Balanced Mode can be enabled by the user or automatically. In both cases,
  // the UI setting should look the same.
  bool balanced_enabled = IsBalancedModeEnabled(profile_->GetPrefs());

  settings_api::PrefObject pref_object;
  pref_object.key = kGeneratedHttpsFirstModePref;
  pref_object.type = settings_api::PrefType::kNumber;

  // Map the two boolean prefs to the enum setting states.
  if (fully_enabled) {
    pref_object.value =
        base::Value(static_cast<int>(HttpsFirstModeSetting::kEnabledFull));
  } else if (balanced_enabled) {
    pref_object.value =
        base::Value(static_cast<int>(HttpsFirstModeSetting::kEnabledBalanced));
  } else {
    pref_object.value =
        base::Value(static_cast<int>(HttpsFirstModeSetting::kDisabled));
  }

  pref_object.user_control_disabled = is_advanced_protection_enabled;

  if (IsBalancedModeAvailable()) {
    ApplyManagementState(*profile_, pref_object);
  } else {
    // TODO(crbug.com/349860796): Remove old settings path once Balanced Mode
    // is fully launched.
    if (!hfm_fully_enabled_pref->IsUserModifiable()) {
      // The pref was controlled by the enterprise policy.
      pref_object.enforcement = settings_api::Enforcement::kEnforced;
      extensions::settings_private::GeneratedPref::ApplyControlledByFromPref(
          &pref_object, hfm_fully_enabled_pref);
    } else if (hfm_fully_enabled_pref->GetRecommendedValue()) {
      // Policy can set a recommended setting of fully enabled or fully
      // disabled. Map those to the enum values.
      pref_object.enforcement = settings_api::Enforcement::kRecommended;
      if (hfm_fully_enabled_pref->GetRecommendedValue()->GetBool()) {
        pref_object.recommended_value =
            base::Value(static_cast<int>(HttpsFirstModeSetting::kEnabledFull));
      } else {
        pref_object.recommended_value =
            base::Value(static_cast<int>(HttpsFirstModeSetting::kDisabled));
      }
    }
  }
  return pref_object;
}

// static
void GeneratedHttpsFirstModePref::ApplyManagementState(
    const Profile& profile,
    settings_api::PrefObject& pref_object) {
  // Computing the effective HTTPS-First Mode managed state requires inspecting
  // both underlying preferences. Note that `prefs::kHttpsOnlyModeEnabled` will
  // always be marked as enforced or recommended if any policy state is set, so
  // can be treated as the canonical source for whether policy enforcement is
  // present.

  // Check fully enabled pref enforced and recommended state.
  const PrefService::Preference* fully_enabled_pref =
      profile.GetPrefs()->FindPreference(prefs::kHttpsOnlyModeEnabled);
  const bool fully_enabled_enforced = !fully_enabled_pref->IsUserModifiable();
  const bool fully_enabled_recommended =
      fully_enabled_pref && fully_enabled_pref->GetRecommendedValue();
  const bool fully_enabled_recommended_on =
      fully_enabled_recommended &&
      fully_enabled_pref->GetRecommendedValue()->GetBool();

  // Check the recommended state of the balanced mode pref.
  const PrefService::Preference* balanced_pref =
      profile.GetPrefs()->FindPreference(prefs::kHttpsFirstBalancedMode);
  const bool balanced_recommended_on =
      balanced_pref && balanced_pref->GetRecommendedValue() &&
      balanced_pref->GetRecommendedValue()->GetBool();

  if (!fully_enabled_enforced && !fully_enabled_recommended) {
    // No relevant policy is applied.
    return;
  }

  // If policy is enforcing a setting, then the fully enabled pref will be
  // enforced, so this covers all policy enforcement states.
  if (fully_enabled_enforced) {
    pref_object.enforcement = settings_api::Enforcement::kEnforced;
    extensions::settings_private::GeneratedPref::ApplyControlledByFromPref(
        &pref_object, fully_enabled_pref);
    return;
  }

  // If the policy is recommending a setting, then the fully enabled pref will
  // be recommended, so this covers all policy enforcement states.
  if (fully_enabled_recommended) {
    // Set enforcement to recommended.
    pref_object.enforcement = settings_api::Enforcement::kRecommended;
    // Determine what setting value is being recommended.
    if (fully_enabled_recommended_on) {
      pref_object.recommended_value =
          base::Value(static_cast<int>(HttpsFirstModeSetting::kEnabledFull));
    } else if (balanced_recommended_on) {
      pref_object.recommended_value = base::Value(
          static_cast<int>(HttpsFirstModeSetting::kEnabledBalanced));
    } else {
      pref_object.recommended_value =
          base::Value(static_cast<int>(HttpsFirstModeSetting::kDisabled));
    }
  }
}
