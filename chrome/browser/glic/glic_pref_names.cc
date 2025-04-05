// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_pref_names.h"

#include "chrome/browser/background/glic/glic_launcher_configuration.h"
#include "components/prefs/pref_registry_simple.h"
#include "ui/base/accelerators/command.h"

namespace glic::prefs {

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(kGlicPinnedToTabstrip, true);
  registry->RegisterBooleanPref(kGlicMicrophoneEnabled, false);
  registry->RegisterBooleanPref(kGlicGeolocationEnabled, false);
  registry->RegisterBooleanPref(kGlicTabContextEnabled, false);
  registry->RegisterIntegerPref(
      kGlicCompletedFre, static_cast<int>(prefs::FreStatus::kNotStarted));
  registry->RegisterTimePref(kGlicWindowLastDismissedTime, base::Time());
}

void RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kGlicLauncherEnabled, false);
  registry->RegisterStringPref(
      prefs::kGlicLauncherHotkey,
      ui::Command::AcceleratorToString(
          GlicLauncherConfiguration::GetDefaultHotkey()));
}

}  // namespace glic::prefs
