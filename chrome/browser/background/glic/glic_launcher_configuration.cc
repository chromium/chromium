// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/background/glic/glic_launcher_configuration.h"

#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "base/values.h"
#include "base/version_info/channel.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_features.h"
#include "components/prefs/pref_service.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/accelerators/command.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace glic {

namespace {

ui::Accelerator GetAcceleratorFromPreference(const char* pref_name) {
  PrefService* const local_state = g_browser_process->local_state();
  if (!local_state) {
    return ui::Accelerator();
  }
  const ui::Accelerator hotkey =
      ui::Command::StringToAccelerator(local_state->GetString(pref_name));

  // Return empty accelerator if an invalid modifier was set.
  if (!hotkey.IsEmpty() &&
      ui::Accelerator::MaskOutKeyEventFlags(hotkey.modifiers()) == 0) {
    return ui::Accelerator();
  }

  return hotkey;
}

base::RepeatingClosure& GetCheckDefaultBrowserTestOverride() {
  static base::NoDestructor<base::RepeatingClosure> callback;
  return *callback;
}

}  // namespace

GlicLauncherConfiguration::GlicLauncherConfiguration(Observer* manager)
    : manager_(manager) {
  if (PrefService* local_state = g_browser_process->local_state()) {
    // Update the default hotkey value once `FeatureList` is initialized.
    const std::string default_hotkey = features::kGlicDefaultHotkey.Get();
    if (!default_hotkey.empty()) {
      local_state->SetDefaultPrefValue(prefs::kGlicLauncherHotkey,
                                       base::Value(default_hotkey));
    }

    if (base::FeatureList::IsEnabled(features::kGlicHotkeyLocalScope) &&
        !local_state->GetBoolean(prefs::kGlicHotkeyGlobalScopeMigrated)) {
      std::string hotkey_str =
          local_state->GetString(prefs::kGlicLauncherHotkey);
      bool is_default_or_empty = hotkey_str.empty();
      if (!is_default_or_empty) {
        const base::Value* default_value =
            local_state->GetDefaultPrefValue(prefs::kGlicLauncherHotkey);
        if (default_value && default_value->GetString() == hotkey_str) {
          is_default_or_empty = true;
        }
      }
      local_state->SetBoolean(prefs::kGlicHotkeyGlobalScopeEnabled,
                              !is_default_or_empty);
      local_state->SetBoolean(prefs::kGlicHotkeyGlobalScopeMigrated, true);
    }

    pref_registrar_.Init(local_state);
    pref_registrar_.Add(
        prefs::kGlicLauncherEnabled,
        base::BindRepeating(&GlicLauncherConfiguration::OnEnabledPrefChanged,
                            base::Unretained(this)));
    pref_registrar_.Add(
        prefs::kGlicLauncherHotkey,
        base::BindRepeating(
            &GlicLauncherConfiguration::OnGlobalHotkeyPrefChanged,
            base::Unretained(this)));
    pref_registrar_.Add(
        prefs::kGlicSelectionHotkey,
        base::BindRepeating(
            &GlicLauncherConfiguration::OnGlobalHotkeyPrefChanged,
            base::Unretained(this)));
    pref_registrar_.Add(
        prefs::kGlicHotkeyGlobalScopeEnabled,
        base::BindRepeating(
            &GlicLauncherConfiguration::OnGlobalHotkeyPrefChanged,
            base::Unretained(this)));
  }
}

GlicLauncherConfiguration::~GlicLauncherConfiguration() = default;

// static
bool GlicLauncherConfiguration::IsEnabled(bool* is_default_value) {
  PrefService* const pref_service = g_browser_process->local_state();
  if (!pref_service) {
    if (is_default_value) {
      *is_default_value = false;
    }
    return false;
  }
  if (is_default_value) {
    *is_default_value =
        pref_service->FindPreference(prefs::kGlicLauncherEnabled)
            ->IsDefaultValue();
  }

  return pref_service->GetBoolean(prefs::kGlicLauncherEnabled);
}

// static
ui::Accelerator GlicLauncherConfiguration::GetToggleHotkey() {
  return GetAcceleratorFromPreference(prefs::kGlicLauncherHotkey);
}

// static
ui::Accelerator GlicLauncherConfiguration::GetSelectionHotkey() {
  if (!base::FeatureList::IsEnabled(features::kGlicCaptureRegion)) {
    return ui::Accelerator();
  }
  return GetAcceleratorFromPreference(prefs::kGlicSelectionHotkey);
}

// static
void GlicLauncherConfiguration::OnCheckIsDefaultBrowserFinished(
    version_info::Channel channel,
    shell_integration::DefaultWebClientState state) {
  // Don't do anything because a different channel is the default browser
  if (state ==
      shell_integration::DefaultWebClientState::OTHER_MODE_IS_DEFAULT) {
    return;
  }

  // Enables the launcher if the current browser is the default or
  // is on the stable channel.
  if (g_browser_process &&
      (state == shell_integration::DefaultWebClientState::IS_DEFAULT ||
       channel == version_info::Channel::STABLE)) {
    g_browser_process->local_state()->SetBoolean(prefs::kGlicLauncherEnabled,
                                                 true);
  }
}

// static
void GlicLauncherConfiguration::CheckDefaultBrowserToEnableLauncher() {
  bool is_enabled_default = false;
  const bool is_launcher_enabled = IsEnabled(&is_enabled_default);
  if (is_enabled_default && !is_launcher_enabled) {
    auto& callback = GetCheckDefaultBrowserTestOverride();
    if (callback) {
      callback.Run();
      return;
    }

    base::MakeRefCounted<shell_integration::DefaultBrowserWorker>()
        ->StartCheckIsDefault(base::BindOnce(
            &GlicLauncherConfiguration::OnCheckIsDefaultBrowserFinished,
            chrome::GetChannel()));
  }
}

// static
void GlicLauncherConfiguration::
    SetCheckDefaultBrowserCallbackForTesting(  // IN-TEST
        base::RepeatingClosure callback) {
  GetCheckDefaultBrowserTestOverride() = std::move(callback);
}

void GlicLauncherConfiguration::OnEnabledPrefChanged() {
  manager_->OnEnabledChanged(IsEnabled());
}

void GlicLauncherConfiguration::OnGlobalHotkeyPrefChanged() {
  manager_->OnGlobalHotkeyChanged();
}

}  // namespace glic
