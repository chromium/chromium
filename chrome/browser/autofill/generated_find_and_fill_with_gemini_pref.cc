// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/generated_find_and_fill_with_gemini_pref.h"

#include <utility>

#include "base/check_deref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/settings_private.h"
#include "components/optimization_guide/core/feature_registry/feature_registration.h"
#include "components/optimization_guide/core/model_execution/model_execution_prefs.h"
#include "components/personal_context/core/personal_context_prefs.h"
#include "components/prefs/pref_service.h"

namespace settings_api = extensions::api::settings_private;

namespace autofill {

GeneratedFindAndFillWithGeminiPref::GeneratedFindAndFillWithGeminiPref(
    Profile* profile)
    : profile_(CHECK_DEREF(profile)) {
  prefs_registrar_.Init(profile_->GetPrefs());
  prefs_registrar_.Add(
      personal_context::prefs::kPersonalContextInAutofillSettingsToggleStatus,
      base::BindRepeating(
          &GeneratedFindAndFillWithGeminiPref::OnSourcePreferencesChanged,
          base::Unretained(this)));
  prefs_registrar_.Add(
      optimization_guide::prefs::kFindAndFillWithGeminiSettings,
      base::BindRepeating(
          &GeneratedFindAndFillWithGeminiPref::OnSourcePreferencesChanged,
          base::Unretained(this)));
}

GeneratedFindAndFillWithGeminiPref::~GeneratedFindAndFillWithGeminiPref() =
    default;

extensions::settings_private::SetPrefResult
GeneratedFindAndFillWithGeminiPref::SetPref(const base::Value* value) {
  if (!value->is_bool()) {
    return extensions::settings_private::SetPrefResult::PREF_TYPE_MISMATCH;
  }

  PrefService* const prefs = profile_->GetPrefs();
  const PrefService::Preference* const policy_pref = prefs->FindPreference(
      optimization_guide::prefs::kFindAndFillWithGeminiSettings);
  const int policy_value = policy_pref ? policy_pref->GetValue()->GetInt() : 0;

  if (policy_value ==
      std::to_underlying(optimization_guide::model_execution::prefs::
                             ModelExecutionEnterprisePolicyValue::kDisable)) {
    return extensions::settings_private::SetPrefResult::PREF_NOT_MODIFIABLE;
  }

  const PrefService::Preference* const backing_pref = prefs->FindPreference(
      personal_context::prefs::kPersonalContextInAutofillSettingsToggleStatus);
  if (backing_pref && !backing_pref->IsUserModifiable()) {
    return extensions::settings_private::SetPrefResult::PREF_NOT_MODIFIABLE;
  }

  prefs->SetBoolean(
      personal_context::prefs::kPersonalContextInAutofillSettingsToggleStatus,
      value->GetBool());
  return extensions::settings_private::SetPrefResult::SUCCESS;
}

settings_api::PrefObject GeneratedFindAndFillWithGeminiPref::GetPrefObject()
    const {
  settings_api::PrefObject pref_object;
  pref_object.key = kGeneratedFindAndFillWithGeminiPref;
  pref_object.type = settings_api::PrefType::kBoolean;

  const PrefService* const prefs = profile_->GetPrefs();
  const PrefService::Preference* const policy_pref = prefs->FindPreference(
      optimization_guide::prefs::kFindAndFillWithGeminiSettings);
  const PrefService::Preference* const backing_pref = prefs->FindPreference(
      personal_context::prefs::kPersonalContextInAutofillSettingsToggleStatus);

  const int policy_value = policy_pref ? policy_pref->GetValue()->GetInt() : 0;
  const bool is_disabled_by_policy =
      (policy_value ==
       std::to_underlying(optimization_guide::model_execution::prefs::
                              ModelExecutionEnterprisePolicyValue::kDisable));

  if (is_disabled_by_policy) {
    pref_object.value = base::Value(false);
    pref_object.enforcement = settings_api::Enforcement::kEnforced;
    if (!policy_pref->IsUserModifiable()) {
      ApplyControlledByFromPref(&pref_object, policy_pref);
    } else {
      pref_object.controlled_by = settings_api::ControlledBy::kUserPolicy;
    }
  } else {
    pref_object.value =
        base::Value(backing_pref ? backing_pref->GetValue()->GetBool() : false);
    if (backing_pref && !backing_pref->IsUserModifiable()) {
      pref_object.enforcement = settings_api::Enforcement::kEnforced;
      ApplyControlledByFromPref(&pref_object, backing_pref);
    }
  }

  return pref_object;
}

void GeneratedFindAndFillWithGeminiPref::OnSourcePreferencesChanged() {
  NotifyObservers(kGeneratedFindAndFillWithGeminiPref);
}

}  // namespace autofill
