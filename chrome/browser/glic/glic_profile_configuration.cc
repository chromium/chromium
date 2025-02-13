// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_profile_configuration.h"

#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/glic_keyed_service_factory.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/launcher/glic_background_mode_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
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
      prefs::kGlicSettingsPolicy,
      base::BindRepeating(&GlicProfileConfiguration::OnEnabledByPolicyChanged,
                          base::Unretained(this)));
}

GlicProfileConfiguration::~GlicProfileConfiguration() = default;

// static
void GlicProfileConfiguration::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(
      prefs::kGlicSettingsPolicy,
      static_cast<int>(prefs::SettingsPolicyState::kEnabled));
  registry->RegisterBooleanPref(prefs::kGlicPinnedToTabstrip, true);
  registry->RegisterBooleanPref(prefs::kGlicMicrophoneEnabled, false);
  registry->RegisterBooleanPref(prefs::kGlicGeolocationEnabled, false);
  registry->RegisterBooleanPref(prefs::kGlicTabContextEnabled, false);
  registry->RegisterBooleanPref(prefs::kGlicCompletedFre, false);
}

bool GlicProfileConfiguration::IsEnabledByPolicy() const {
  return profile_->GetPrefs()->GetInteger(prefs::kGlicSettingsPolicy) ==
         static_cast<int>(prefs::SettingsPolicyState::kEnabled);
}

void GlicProfileConfiguration::OnEnabledByPolicyChanged() {
  // Note: the pref listener can sometimes fire even if the value from
  // GetInteger doesn't change (e.g. value was set from multiple sources). See
  // GlicPolicyTest.PrefDisabledByPolicy for an example.

  if (!IsEnabledByPolicy()) {
    // If the policy becomes disabled, ensure an open Glic window is closed.  Do
    // this before updating the Glic button since closing the panel starts an
    // animation that relies on the button geometry for the animation.
    // TODO(https://crbug.com/391337606): Longer term, we may want to handle
    // this more gracefully but that'd require the client being aware that it's
    // been disabled while it's active.
    GlicKeyedServiceFactory::GetGlicKeyedService(&profile_.get())->ClosePanel();
  }

  GlicBackgroundModeManager::GetInstance()->OnPolicyChanged();
}

bool GlicProfileConfiguration::HasCompletedFre() const {
  return profile_->GetPrefs()->GetBoolean(prefs::kGlicCompletedFre);
}

}  // namespace glic
