// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/pref_handlers/keyboard_pref_handler_impl.h"

#include "ash/public/mojom/input_device_settings.mojom.h"
#include "ash/system/input_device_settings/input_device_settings_pref_names.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "components/prefs/pref_service.h"

namespace ash {

KeyboardPrefHandlerImpl::KeyboardPrefHandlerImpl() = default;
KeyboardPrefHandlerImpl::~KeyboardPrefHandlerImpl() = default;

// TODO(dpad): Implement keyboard settings initialization.
void KeyboardPrefHandlerImpl::InitializeKeyboardSettings(
    PrefService* pref_service,
    mojom::Keyboard* keyboard) {
  NOTIMPLEMENTED();
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
  // `mojom::ModifierKey` enum to ints. Since `base::Value::Dict` only supports
  // strings as keys, this is then converted into a string.
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
