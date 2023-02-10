// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/pref_handlers/keyboard_pref_handler_impl.h"
#include "ash/constants/ash_pref_names.h"

#include "ash/public/mojom/input_device_settings.mojom.h"
#include "ash/shell.h"
#include "ash/system/input_device_settings/input_device_settings_defaults.h"
#include "ash/system/input_device_settings/input_device_settings_pref_names.h"
#include "ash/system/input_device_settings/input_device_settings_utils.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "components/prefs/pref_service.h"
#include "ui/chromeos/events/mojom/modifier_key.mojom.h"

namespace ash {
namespace {
mojom::KeyboardSettingsPtr GetDefaultKeyboardSettings() {
  mojom::KeyboardSettingsPtr settings = mojom::KeyboardSettings::New();
  settings->auto_repeat_delay = kDefaultAutoRepeatDelay;
  settings->auto_repeat_interval = kDefaultAutoRepeatInterval;
  settings->auto_repeat_enabled = kDefaultAutoRepeatEnabled;
  settings->suppress_meta_fkey_rewrites = kDefaultSuppressMetaFKeyRewrites;
  settings->top_row_are_fkeys = kDefaultTopRowAreFKeys;
  settings->suppress_meta_fkey_rewrites = kDefaultSuppressMetaFKeyRewrites;
  return settings;
}

mojom::KeyboardSettingsPtr GetKeyboardSettingsFromGlobalPrefs(
    PrefService* prefs) {
  mojom::KeyboardSettingsPtr settings = mojom::KeyboardSettings::New();
  settings->auto_repeat_delay =
      base::Milliseconds(prefs->GetInteger(prefs::kXkbAutoRepeatDelay));
  settings->auto_repeat_interval =
      base::Milliseconds(prefs->GetInteger(prefs::kXkbAutoRepeatInterval));
  settings->auto_repeat_enabled =
      prefs->GetBoolean(prefs::kXkbAutoRepeatEnabled);
  settings->top_row_are_fkeys = prefs->GetBoolean(prefs::kSendFunctionKeys);
  settings->suppress_meta_fkey_rewrites = kDefaultSuppressMetaFKeyRewrites;
  return settings;
}

}  // namespace

KeyboardPrefHandlerImpl::KeyboardPrefHandlerImpl() = default;
KeyboardPrefHandlerImpl::~KeyboardPrefHandlerImpl() = default;

void KeyboardPrefHandlerImpl::InitializeKeyboardSettings(
    PrefService* pref_service,
    mojom::Keyboard* keyboard) {
  const auto& devices_dict =
      pref_service->GetDict(prefs::kKeyboardDeviceSettingsDictPref);
  const auto* settings_dict = devices_dict.FindDict(keyboard->device_key);
  if (!settings_dict) {
    keyboard->settings = GetNewKeyboardSettings(pref_service, *keyboard);
  } else {
    keyboard->settings =
        RetreiveKeyboardSettings(pref_service, *keyboard, *settings_dict);
  }
  DCHECK(keyboard->settings);

  UpdateKeyboardSettings(pref_service, *keyboard);
}

mojom::KeyboardSettingsPtr KeyboardPrefHandlerImpl::RetreiveKeyboardSettings(
    PrefService* pref_service,
    const mojom::Keyboard& keyboard,
    const base::Value::Dict& settings_dict) {
  mojom::KeyboardSettingsPtr settings = mojom::KeyboardSettings::New();
  settings->auto_repeat_enabled =
      settings_dict.FindBool(prefs::kKeyboardSettingAutoRepeatEnabled)
          .value_or(kDefaultAutoRepeatEnabled);
  settings->auto_repeat_delay = base::Milliseconds(
      settings_dict.FindInt(prefs::kKeyboardSettingAutoRepeatDelay)
          .value_or(kDefaultAutoRepeatDelay.InMilliseconds()));
  settings->auto_repeat_interval = base::Milliseconds(
      settings_dict.FindInt(prefs::kKeyboardSettingAutoRepeatInterval)
          .value_or(kDefaultAutoRepeatInterval.InMilliseconds()));
  settings->suppress_meta_fkey_rewrites =
      settings_dict.FindBool(prefs::kKeyboardSettingSuppressMetaFKeyRewrites)
          .value_or(kDefaultSuppressMetaFKeyRewrites);
  settings->top_row_are_fkeys =
      settings_dict.FindBool(prefs::kKeyboardSettingTopRowAreFKeys)
          .value_or(kDefaultTopRowAreFKeys);

  const auto* modifier_remappings_dict =
      settings_dict.FindDict(prefs::kKeyboardSettingModifierRemappings);
  if (!modifier_remappings_dict) {
    return settings;
  }

  for (const auto [from, to] : *modifier_remappings_dict) {
    // `from` must be a string which can be converted to an int and `to` must
    // be an int.
    int from_int, to_int;
    if (!to.is_int() || !base::StringToInt(from, &from_int)) {
      LOG(ERROR) << "Unable to parse modifier remappings from prefs. From: "
                 << from << " To: " << to.DebugString();
      continue;
    }
    to_int = to.GetInt();

    // Validate the ints can be cast to `ui::mojom::ModifierKey` and cast them.
    if (!IsValidModifier(from_int) || !IsValidModifier(to_int)) {
      LOG(ERROR) << "Read invalid modifier keys from pref. From: " << from_int
                 << " To: " << to_int;
      continue;
    }
    const ui::mojom::ModifierKey from_key =
        static_cast<ui::mojom::ModifierKey>(from_int);
    const ui::mojom::ModifierKey to_key =
        static_cast<ui::mojom::ModifierKey>(to_int);

    settings->modifier_remappings[from_key] = to_key;
  }

  return settings;
}

mojom::KeyboardSettingsPtr KeyboardPrefHandlerImpl::GetNewKeyboardSettings(
    PrefService* prefs,
    const mojom::Keyboard& keyboard) {
  // TODO(michaelcheco): Remove once transitioned to per-device settings.
  if (Shell::Get()->input_device_tracker()->WasDevicePreviouslyConnected(
          InputDeviceTracker::InputDeviceCategory::kKeyboard,
          keyboard.device_key)) {
    return GetKeyboardSettingsFromGlobalPrefs(prefs);
  }

  return GetDefaultKeyboardSettings();
}

void KeyboardPrefHandlerImpl::UpdateKeyboardSettings(
    PrefService* pref_service,
    const mojom::Keyboard& keyboard) {
  DCHECK(keyboard.settings);
  const mojom::KeyboardSettings& settings = *keyboard.settings;

  // Populate `settings_dict` with all settings in `settings`.
  base::Value::Dict settings_dict;
  settings_dict.Set(
      prefs::kKeyboardSettingAutoRepeatDelay,
      static_cast<int>(settings.auto_repeat_delay.InMilliseconds()));
  settings_dict.Set(
      prefs::kKeyboardSettingAutoRepeatInterval,
      static_cast<int>(settings.auto_repeat_interval.InMilliseconds()));
  settings_dict.Set(prefs::kKeyboardSettingAutoRepeatEnabled,
                    settings.auto_repeat_enabled);
  settings_dict.Set(prefs::kKeyboardSettingSuppressMetaFKeyRewrites,
                    settings.suppress_meta_fkey_rewrites);
  settings_dict.Set(prefs::kKeyboardSettingTopRowAreFKeys,
                    settings.top_row_are_fkeys);

  // Modifier remappings get stored in a dict by casting the
  // `ui::mojom::ModifierKey` enum to ints. Since `base::Value::Dict` only
  // supports strings as keys, this is then converted into a string.
  base::Value::Dict modifier_remappings;
  for (const auto& [from, to] : settings.modifier_remappings) {
    modifier_remappings.Set(base::NumberToString(static_cast<int>(from)),
                            static_cast<int>(to));
  }
  settings_dict.Set(prefs::kKeyboardSettingModifierRemappings,
                    std::move(modifier_remappings));

  // Retrieve old settings and merge with the new ones.
  base::Value::Dict devices_dict =
      pref_service->GetDict(prefs::kKeyboardDeviceSettingsDictPref).Clone();

  // If an old settings dict already exists for the device, merge the updated
  // settings into the old settings. Otherwise, insert the dict at
  // `keyboard.device_key`.
  base::Value::Dict* old_settings_dict =
      devices_dict.FindDict(keyboard.device_key);
  if (old_settings_dict) {
    old_settings_dict->Merge(std::move(settings_dict));
  } else {
    devices_dict.Set(keyboard.device_key, std::move(settings_dict));
  }

  pref_service->SetDict(std::string(prefs::kKeyboardDeviceSettingsDictPref),
                        std::move(devices_dict));
}

}  // namespace ash
