// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/launcher/glic_configuration.h"

#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace glic {

GlicConfiguration::GlicConfiguration(Observer* manager) : manager_(manager) {
  if (PrefService* local_state = g_browser_process->local_state()) {
    pref_registrar_.Init(local_state);
    pref_registrar_.Add(
        prefs::kGlicLauncherEnabled,
        base::BindRepeating(&GlicConfiguration::OnEnabledPrefChanged,
                            base::Unretained(this)));
    pref_registrar_.Add(
        prefs::kGlicLauncherGlobalHotkey,
        base::BindRepeating(&GlicConfiguration::OnGlobalHotkeyPrefChanged,
                            base::Unretained(this)));
  }
}

GlicConfiguration::~GlicConfiguration() = default;

// static
void GlicConfiguration::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kGlicLauncherEnabled, false);
  registry->RegisterBooleanPref(prefs::kGlicMicrophoneEnabled, false);
  registry->RegisterBooleanPref(prefs::kGlicGeolocationEnabled, false);
  registry->RegisterBooleanPref(prefs::kGlicTabContextEnabled, false);
  registry->RegisterDictionaryPref(
      prefs::kGlicLauncherGlobalHotkey,
      base::Value::Dict()
          .Set(kHotkeyKeyCode, ui::KeyboardCode::VKEY_UNKNOWN)
          .Set(kHotkeyModifiers, ui::EF_NONE));
}

bool GlicConfiguration::IsEnabled() {
  return g_browser_process->local_state()->GetBoolean(
      prefs::kGlicLauncherEnabled);
}

ui::Accelerator GlicConfiguration::GetGlobalHotkey() {
  const base::Value::Dict& hotkey_dictionary =
      g_browser_process->local_state()->GetDict(
          prefs::kGlicLauncherGlobalHotkey);
  const int key_code = hotkey_dictionary.Find(kHotkeyKeyCode)->GetInt();
  const int modifiers = hotkey_dictionary.Find(kHotkeyModifiers)->GetInt();
  const ui::Accelerator hotkey =
      ui::Accelerator(static_cast<ui::KeyboardCode>(key_code), modifiers);

  // Return empty accelerator if an invalid modifier was set.
  if (!hotkey.IsEmpty() &&
      ui::Accelerator::MaskOutKeyEventFlags(modifiers) == 0) {
    return ui::Accelerator();
  }

  return hotkey;
}

void GlicConfiguration::OnEnabledPrefChanged() {
  manager_->OnEnabledChanged(IsEnabled());
}

void GlicConfiguration::OnGlobalHotkeyPrefChanged() {
  manager_->OnGlobalHotkeyChanged(GetGlobalHotkey());
}

}  // namespace glic
