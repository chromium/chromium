// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_profile_configuration.h"

#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace glic {

GlicProfileConfiguration::GlicProfileConfiguration(Profile* profile)
    : profile_(*profile) {
  pref_registrar_.Init(profile_->GetPrefs());
  pref_registrar_.Add(
      prefs::kGlicEnabledByPolicy,
      base::BindRepeating(&GlicProfileConfiguration::OnEnabledByPolicyChanged,
                          base::Unretained(this)));
}

GlicProfileConfiguration::~GlicProfileConfiguration() = default;

// static
void GlicProfileConfiguration::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kGlicEnabledByPolicy, true);
  registry->RegisterBooleanPref(prefs::kGlicMicrophoneEnabled, false);
  registry->RegisterBooleanPref(prefs::kGlicGeolocationEnabled, false);
  registry->RegisterBooleanPref(prefs::kGlicTabContextEnabled, false);
}

bool GlicProfileConfiguration::IsEnabledByPolicy() const {
  return profile_->GetPrefs()->GetBoolean(prefs::kGlicEnabledByPolicy);
}

void GlicProfileConfiguration::OnEnabledByPolicyChanged() {
  // TODO(crbug.com/382722218): Update UI in each window to remove/add Glic
  // button.
  // TODO(crbug.com/382722218): Update background mode in response to changed
  // policy.
}

}  // namespace glic
