// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/background/glic/glic_launcher_configuration.h"

#include "base/no_destructor.h"
#include "base/values.h"
#include "base/version_info/channel.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/glic_pref_names.h"
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
  }
}

GlicLauncherConfiguration::~GlicLauncherConfiguration() = default;

// static
bool GlicLauncherConfiguration::IsEnabled(bool* is_default_value) {
  PrefService* const pref_service = g_browser_process->local_state();
  if (is_default_value) {
    *is_default_value =
        pref_service->FindPreference(prefs::kGlicLauncherEnabled)
            ->IsDefaultValue();
  }

  return pref_service->GetBoolean(prefs::kGlicLauncherEnabled);
}

// static
ui::Accelerator GlicLauncherConfiguration::GetGlobalHotkey() {
  const ui::Accelerator hotkey = ui::Command::StringToAccelerator(
      g_browser_process->local_state()->GetString(prefs::kGlicLauncherHotkey));

  // Return empty accelerator if an invalid modifier was set.
  if (!hotkey.IsEmpty() &&
      ui::Accelerator::MaskOutKeyEventFlags(hotkey.modifiers()) == 0) {
    return ui::Accelerator();
  }

  return hotkey;
}

// static
ui::Accelerator GlicLauncherConfiguration::GetDefaultHotkey() {
#if BUILDFLAG(IS_MAC)
  const ui::EventFlags modifiers = ui::EF_CONTROL_DOWN;
#elif BUILDFLAG(IS_CHROMEOS)
  // This is the search key on ChromeOS keyboard.
  const ui::EventFlags modifiers = ui::EF_COMMAND_DOWN;
#else
  const ui::EventFlags modifiers = ui::EF_ALT_DOWN;
#endif

  return ui::Accelerator(ui::KeyboardCode::VKEY_G, modifiers);
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
  manager_->OnGlobalHotkeyChanged(GetGlobalHotkey());
}

}  // namespace glic
