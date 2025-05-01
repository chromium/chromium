// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_pref_names.h"

#include "chrome/browser/background/glic/glic_launcher_configuration.h"
#include "chrome/browser/glic/widget/local_hotkey_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry.h"
#include "components/prefs/pref_registry_simple.h"
#include "ui/base/accelerators/command.h"

namespace glic::prefs {

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(kGlicPinnedToTabstrip, true);
  registry->RegisterBooleanPref(kGlicMicrophoneEnabled, false);
  registry->RegisterBooleanPref(kGlicGeolocationEnabled, false);
  registry->RegisterBooleanPref(kGlicTabContextEnabled, false);
  registry->RegisterBooleanPref(
      kGlicRolloutEligibility, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PRIORITY_PREF);
  registry->RegisterIntegerPref(
      kGlicCompletedFre, static_cast<int>(prefs::FreStatus::kNotStarted));
  registry->RegisterTimePref(kGlicWindowLastDismissedTime, base::Time());

  // The default value is not used. If not set the default position is
  // calculated based on the entrypoint and current active browser.
  registry->RegisterIntegerPref(kGlicPreviousPositionX, 0,
                                PrefRegistry::LOSSY_PREF);
  registry->RegisterIntegerPref(kGlicPreviousPositionY, 0,
                                PrefRegistry::LOSSY_PREF);

  // Dict pref to store GlicUserStatus information.
  registry->RegisterDictionaryPref(prefs::kGlicUserStatus);

  // Boolean pref for the closed captioning setting.
  registry->RegisterBooleanPref(prefs::kGlicClosedCaptioningEnabled, false);
}

void RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kGlicLauncherEnabled, false);
  registry->RegisterStringPref(
      prefs::kGlicLauncherHotkey,
      ui::Command::AcceleratorToString(
          GlicLauncherConfiguration::GetDefaultHotkey()));
  registry->RegisterStringPref(
      prefs::kGlicFocusToggleHotkey,
      ui::Command::AcceleratorToString(
          LocalHotkeyManager::GetDefaultAccelerator(
              LocalHotkeyManager::Hotkey::kFocusToggle)));
}

}  // namespace glic::prefs
