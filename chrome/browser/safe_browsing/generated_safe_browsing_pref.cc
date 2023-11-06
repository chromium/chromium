// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/generated_safe_browsing_pref.h"

#include "base/types/cxx23_to_underlying.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/settings_private.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"

namespace settings_api = extensions::api::settings_private;

namespace safe_browsing {

const char kGeneratedSafeBrowsingPref[] = "generated.safe_browsing";

GeneratedSafeBrowsingPref::GeneratedSafeBrowsingPref(Profile* profile)
    : profile_(profile) {
  user_prefs_registrar_.Init(profile->GetPrefs());
  user_prefs_registrar_.Add(
      prefs::kSafeBrowsingEnabled,
      base::BindRepeating(
          &GeneratedSafeBrowsingPref::OnSafeBrowsingPreferencesChanged,
          base::Unretained(this)));
  user_prefs_registrar_.Add(
      prefs::kSafeBrowsingEnhanced,
      base::BindRepeating(
          &GeneratedSafeBrowsingPref::OnSafeBrowsingPreferencesChanged,
          base::Unretained(this)));
  user_prefs_registrar_.Add(
      prefs::kSafeBrowsingScoutReportingEnabled,
      base::BindRepeating(
          &GeneratedSafeBrowsingPref::OnSafeBrowsingPreferencesChanged,
          base::Unretained(this)));
}

extensions::settings_private::SetPrefResult GeneratedSafeBrowsingPref::SetPref(
    const base::Value* value) {
  if (!value->is_int())
    return extensions::settings_private::SetPrefResult::PREF_TYPE_MISMATCH;

  auto selection = static_cast<SafeBrowsingSetting>(value->GetInt());

  if (selection != SafeBrowsingSetting::DISABLED &&
      selection != SafeBrowsingSetting::STANDARD &&
      selection != SafeBrowsingSetting::ENHANCED)
    return extensions::settings_private::SetPrefResult::PREF_TYPE_MISMATCH;

  // If SBER is forcefully disabled, Enhanced cannot be selected by the user.
  const PrefService::Preference* reporting_pref =
      profile_->GetPrefs()->FindPreference(
          prefs::kSafeBrowsingScoutReportingEnabled);
  const bool reporting_on = reporting_pref->GetValue()->GetBool();
  const bool reporting_enforced = !reporting_pref->IsUserModifiable();

  if (reporting_enforced && !reporting_on &&
      selection == SafeBrowsingSetting::ENHANCED) {
    return extensions::settings_private::SetPrefResult::PREF_NOT_MODIFIABLE;
  }

  // kSafeBrowsingEnabled is considered the canonical source for Safe Browsing
  // management.
  const PrefService::Preference* enabled_pref =
      profile_->GetPrefs()->FindPreference(prefs::kSafeBrowsingEnabled);
  if (!enabled_pref->IsUserModifiable()) {
    return extensions::settings_private::SetPrefResult::PREF_NOT_MODIFIABLE;
  }

  // Update both Safe Browsing preferences to match selection.
  profile_->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled,
                                   selection != SafeBrowsingSetting::DISABLED);
  profile_->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnhanced,
                                   selection == SafeBrowsingSetting::ENHANCED);

  // Set ESB not set in sync with Account ESB through TailoredSecurity.
  if (selection == SafeBrowsingSetting::ENHANCED) {
    profile_->GetPrefs()->SetBoolean(
        prefs::kEnhancedProtectionEnabledViaTailoredSecurity, false);
  }

  return extensions::settings_private::SetPrefResult::SUCCESS;
}

extensions::api::settings_private::PrefObject
GeneratedSafeBrowsingPref::GetPrefObject() const {
  extensions::api::settings_private::PrefObject pref_object;
  pref_object.key = kGeneratedSafeBrowsingPref;
  pref_object.type = extensions::api::settings_private::PrefType::kNumber;

  auto safe_browsing_enabled =
      profile_->GetPrefs()->GetBoolean(prefs::kSafeBrowsingEnabled);
  auto safe_browsing_enhanced_enabled =
      profile_->GetPrefs()->GetBoolean(prefs::kSafeBrowsingEnhanced);

  if (safe_browsing_enhanced_enabled && safe_browsing_enabled) {
    pref_object.value =
        base::Value(static_cast<int>(SafeBrowsingSetting::ENHANCED));
  } else if (safe_browsing_enabled) {
    pref_object.value =
        base::Value(static_cast<int>(SafeBrowsingSetting::STANDARD));
  } else {
    pref_object.value =
        base::Value(static_cast<int>(SafeBrowsingSetting::DISABLED));
  }

  ApplySafeBrowsingManagementState(*profile_, pref_object);

  return pref_object;
}

void GeneratedSafeBrowsingPref::OnSafeBrowsingPreferencesChanged() {
  NotifyObservers(kGeneratedSafeBrowsingPref);
}

/* static */
void GeneratedSafeBrowsingPref::ApplySafeBrowsingManagementState(
    const Profile& profile,
    settings_api::PrefObject& pref_object) {
  // Computing the effective Safe Browsing managed state requires inspecting
  // three different preferences. It is possible that these may be in
  // temporarily conflicting managed states. The enabled preference is always
  // taken as the canonical source of management.
  const PrefService::Preference* enabled_pref =
      profile.GetPrefs()->FindPreference(prefs::kSafeBrowsingEnabled);
  const bool enabled_enforced = !enabled_pref->IsUserModifiable();
  const bool enabled_recommended =
      (enabled_pref && enabled_pref->GetRecommendedValue());
  const bool enabled_recommended_on =
      enabled_recommended && enabled_pref->GetRecommendedValue()->GetBool();

  // The enhanced preference may have a recommended setting. This only takes
  // effect if the enabled preference also has a recommended setting.
  const PrefService::Preference* enhanced_pref =
      profile.GetPrefs()->FindPreference(prefs::kSafeBrowsingEnhanced);
  const bool enhanced_recommended_on =
      enhanced_pref->GetRecommendedValue() &&
      enhanced_pref->GetRecommendedValue()->GetBool();

  // A forcefully disabled reporting preference will disallow enhanced from
  // being selected and thus it must also be considered.
  const PrefService::Preference* reporting_pref =
      profile.GetPrefs()->FindPreference(
          prefs::kSafeBrowsingScoutReportingEnabled);
  const bool reporting_on = reporting_pref->GetValue()->GetBool();
  const bool reporting_enforced = !reporting_pref->IsUserModifiable();

  if (!enabled_enforced && !enabled_recommended && !reporting_enforced) {
    // No relevant policies are applied.
    return;
  }

  if (enabled_enforced) {
    // Preference is fully controlled.
    pref_object.enforcement = settings_api::Enforcement::kEnforced;
    extensions::settings_private::GeneratedPref::ApplyControlledByFromPref(
        &pref_object, enabled_pref);
    return;
  }

  if (enabled_recommended) {
    // Set enforcement to recommended. This may be upgraded to enforced later
    // in this function.
    pref_object.enforcement = settings_api::Enforcement::kRecommended;
    if (enhanced_recommended_on) {
      pref_object.recommended_value =
          base::Value(static_cast<int>(SafeBrowsingSetting::ENHANCED));
    } else if (enabled_recommended_on) {
      pref_object.recommended_value =
          base::Value(static_cast<int>(SafeBrowsingSetting::STANDARD));
    } else {
      pref_object.recommended_value =
          base::Value(static_cast<int>(SafeBrowsingSetting::DISABLED));
    }
  }

  if (reporting_enforced && !reporting_on) {
    // Reporting has been forcefully disabled by policy. Enhanced protection is
    // thus also implicitly disabled by the same policy.
    pref_object.enforcement = settings_api::Enforcement::kEnforced;
    extensions::settings_private::GeneratedPref::ApplyControlledByFromPref(
        &pref_object, reporting_pref);

    pref_object.user_selectable_values.emplace();
    pref_object.user_selectable_values->Append(
        base::to_underlying(SafeBrowsingSetting::STANDARD));
    pref_object.user_selectable_values->Append(
        base::to_underlying(SafeBrowsingSetting::DISABLED));
  }
}

}  // namespace safe_browsing
