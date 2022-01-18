// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/generated_floc_pref.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/settings_private.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"

namespace settings_api = extensions::api::settings_private;

const char kGeneratedFlocPref[] = "generated.floc_enabled";

GeneratedFlocPref::GeneratedFlocPref(Profile* profile) : profile_(profile) {
  user_prefs_registrar_.Init(profile->GetPrefs());
  user_prefs_registrar_.Add(
      prefs::kPrivacySandboxApisEnabled,
      base::BindRepeating(&GeneratedFlocPref::OnSourcePreferencesChanged,
                          base::Unretained(this)));
  user_prefs_registrar_.Add(
      prefs::kPrivacySandboxFlocEnabled,
      base::BindRepeating(&GeneratedFlocPref::OnSourcePreferencesChanged,
                          base::Unretained(this)));
}

extensions::settings_private::SetPrefResult GeneratedFlocPref::SetPref(
    const base::Value* value) {
  if (!value->is_bool())
    return extensions::settings_private::SetPrefResult::PREF_TYPE_MISMATCH;

  // If the Privacy Sandbox APIs pref is disabled for any reason, the generated
  // pref cannot be changed.
  if (!profile_->GetPrefs()->GetBoolean(prefs::kPrivacySandboxApisEnabled))
    return extensions::settings_private::SetPrefResult::PREF_NOT_MODIFIABLE;

  profile_->GetPrefs()->SetBoolean(prefs::kPrivacySandboxFlocEnabled,
                                   value->GetBool());

  return extensions::settings_private::SetPrefResult::SUCCESS;
}

std::unique_ptr<extensions::api::settings_private::PrefObject>
GeneratedFlocPref::GetPrefObject() const {
  // Disable FLoC control while OT not active.
  // TODO(crbug.com/1287951): Perform cleanup / adjustment as required.
  auto floc_pref_object = std::make_unique<settings_api::PrefObject>();
  floc_pref_object->key = kGeneratedFlocPref;
  floc_pref_object->type = settings_api::PREF_TYPE_BOOLEAN;
  floc_pref_object->value = std::make_unique<base::Value>(false);
  floc_pref_object->user_control_disabled = std::make_unique<bool>(true);

  return floc_pref_object;
}

void GeneratedFlocPref::OnSourcePreferencesChanged() {
  NotifyObservers(kGeneratedFlocPref);
}
