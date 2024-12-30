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

GlicProfileConfiguration::~GlicProfileConfiguration() = default;

// static
void GlicProfileConfiguration::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kGlicMicrophoneEnabled, false);
  registry->RegisterBooleanPref(prefs::kGlicGeolocationEnabled, false);
  registry->RegisterBooleanPref(prefs::kGlicTabContextEnabled, false);
}

}  // namespace glic
