// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/pref_handlers/keyboard_pref_handler_impl.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/public/mojom/input_device_settings.mojom-shared.h"
#include "ash/public/mojom/input_device_settings.mojom.h"
#include "ash/shell.h"
#include "ash/system/input_device_settings/input_device_settings_defaults.h"
#include "ash/system/input_device_settings/input_device_settings_pref_names.h"
#include "ash/system/input_device_settings/input_device_settings_utils.h"
#include "ash/system/input_device_settings/input_device_tracker.h"
#include "base/containers/fixed_flat_map.h"
#include "base/containers/flat_map.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece_forward.h"
#include "base/time/time.h"
#include "components/prefs/pref_service.h"
#include "ui/chromeos/events/mojom/modifier_key.mojom-shared.h"
#include "ui/chromeos/events/mojom/modifier_key.mojom.h"
#include "ui/chromeos/events/pref_names.h"

namespace ash {
namespace {

// Whether or not settings taken during the transition period should be
// persisted to the prefs. Values should only ever be true if the original
// setting was a user-configured value.

// Modifier remappings are not included here as it is only ever persisted if it
// is non-default.
struct ForceKeyboardSettingPersistence {
  bool top_row_are_fkeys = false;
  bool suppress_meta_fkey_rewrites = false;
};

static constexpr auto kKeyboardModifierMappings =
    base::MakeFixedFlatMap<ui::mojom::ModifierKey, const char*>(
        {{ui::mojom::ModifierKey::kAlt, ::prefs::kLanguageRemapAltKeyTo},
         {ui::mojom::ModifierKey::kControl,
          ::prefs::kLanguageRemapControlKeyTo},
         {ui::mojom::ModifierKey::kEscape, ::prefs::kLanguageRemapEscapeKeyTo},
         {ui::mojom::ModifierKey::kBackspace,
          ::prefs::kLanguageRemapBackspaceKeyTo},
         {ui::mojom::ModifierKey::kAssistant,
          ::prefs::kLanguageRemapAssistantKeyTo},
         {ui::mojom::ModifierKey::kCapsLock,
          ::prefs::kLanguageRemapCapsLockKeyTo}});

static constexpr auto kMetaKeyMapping =
    base::MakeFixedFlatMap<mojom::MetaKey, const char*>(
        {{mojom::MetaKey::kSearch, ::prefs::kLanguageRemapSearchKeyTo},
         {mojom::MetaKey::kLauncher, ::prefs::kLanguageRemapSearchKeyTo},
         {mojom::MetaKey::kExternalMeta,
          ::prefs::kLanguageRemapExternalMetaKeyTo},
         {mojom::MetaKey::kCommand,
          ::prefs::kLanguageRemapExternalCommandKeyTo}});

mojom::KeyboardSettingsPtr GetDefaultKeyboardSettings(bool is_external,
                                                      mojom::MetaKey meta_key) {
  mojom::KeyboardSettingsPtr settings = mojom::KeyboardSettings::New();
  settings->suppress_meta_fkey_rewrites = kDefaultSuppressMetaFKeyRewrites;
  // This setting should be enabled by default for external keyboards.
  settings->top_row_are_fkeys =
      is_external ? kDefaultTopRowAreFKeysExternal : kDefaultTopRowAreFKeys;
  // Switch control and command for Apple keyboards.
  if (meta_key == mojom::MetaKey::kCommand) {
    settings->modifier_remappings[ui::mojom::ModifierKey::kControl] =
        ui::mojom::ModifierKey::kMeta;
    settings->modifier_remappings[ui::mojom::ModifierKey::kMeta] =
        ui::mojom::ModifierKey::kControl;
  }
  return settings;
}

base::flat_map<ui::mojom::ModifierKey, ui::mojom::ModifierKey>
GetModifierRemappings(PrefService* prefs, const mojom::Keyboard& keyboard) {
  base::flat_map<ui::mojom::ModifierKey, ui::mojom::ModifierKey> remappings;

  for (const auto& modifier_key : keyboard.modifier_keys) {
    if (modifier_key == ui::mojom::ModifierKey::kMeta) {
      // The meta key is handled separately.
      continue;
    }
    auto* it = kKeyboardModifierMappings.find(modifier_key);
    DCHECK(it != kKeyboardModifierMappings.end());
    const auto pref_modifier_key =
        static_cast<ui::mojom::ModifierKey>(prefs->GetInteger(it->second));
    if (modifier_key != pref_modifier_key) {
      remappings.emplace(modifier_key, pref_modifier_key);
    }
  }

  const auto meta_key_pref_value = static_cast<ui::mojom::ModifierKey>(
      prefs->GetInteger(kMetaKeyMapping.at(keyboard.meta_key)));
  if (ui::mojom::ModifierKey::kMeta != meta_key_pref_value) {
    remappings.emplace(ui::mojom::ModifierKey::kMeta, meta_key_pref_value);
  }
  return remappings;
}

mojom::KeyboardSettingsPtr GetKeyboardSettingsFromGlobalPrefs(
    PrefService* prefs,
    const mojom::Keyboard& keyboard,
    ForceKeyboardSettingPersistence& force_persistence) {
  mojom::KeyboardSettingsPtr settings = mojom::KeyboardSettings::New();

  const auto* top_row_are_fkeys_preference =
      prefs->GetUserPrefValue(prefs::kSendFunctionKeys);
  settings->top_row_are_fkeys = top_row_are_fkeys_preference
                                    ? top_row_are_fkeys_preference->GetBool()
                                    : kDefaultTopRowAreFKeys;
  force_persistence.top_row_are_fkeys = top_row_are_fkeys_preference != nullptr;

  settings->suppress_meta_fkey_rewrites = kDefaultSuppressMetaFKeyRewrites;
  // Do not persist as default should not be persisted.
  force_persistence.suppress_meta_fkey_rewrites = false;

  settings->modifier_remappings = GetModifierRemappings(prefs, keyboard);
  return settings;
}

bool ExistingSettingsHasValue(base::StringPiece setting_key,
                              const base::Value::Dict* existing_settings_dict) {
  if (!existing_settings_dict) {
    return false;
  }

  return existing_settings_dict->Find(setting_key) != nullptr;
}

mojom::KeyboardSettingsPtr RetrieveKeyboardSettings(
    PrefService* pref_service,
    const mojom::Keyboard& keyboard,
    const base::Value::Dict& settings_dict) {
  mojom::KeyboardSettingsPtr settings = mojom::KeyboardSettings::New();
  settings->suppress_meta_fkey_rewrites =
      settings_dict.FindBool(prefs::kKeyboardSettingSuppressMetaFKeyRewrites)
          .value_or(kDefaultSuppressMetaFKeyRewrites);
  settings->top_row_are_fkeys =
      settings_dict.FindBool(prefs::kKeyboardSettingTopRowAreFKeys)
          .value_or(keyboard.is_external ? kDefaultTopRowAreFKeysExternal
                                         : kDefaultTopRowAreFKeys);

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

void UpdateKeyboardSettingsImpl(
    PrefService* pref_service,
    const mojom::Keyboard& keyboard,
    const ForceKeyboardSettingPersistence& force_persistence) {
  DCHECK(keyboard.settings);

  base::Value::Dict devices_dict =
      pref_service->GetDict(prefs::kKeyboardDeviceSettingsDictPref).Clone();
  base::Value::Dict* existing_settings_dict =
      devices_dict.FindDict(keyboard.device_key);
  const mojom::KeyboardSettings& settings = *keyboard.settings;

  // Populate `settings_dict` with all settings in `settings`.
  base::Value::Dict settings_dict;

  // Settings should only be persisted if one or more of the following is true:
  // - Setting was previously persisted to storage
  // - `force_persistence` requires the setting to be persisted, this means this
  //   device is being transitioned from the old global settings to per-device
  //   settings and the user specified the specific value for this setting.
  // - Setting is different than the default, which means the user manually
  //   changed the value.

  if (ExistingSettingsHasValue(prefs::kKeyboardSettingSuppressMetaFKeyRewrites,
                               existing_settings_dict) ||
      force_persistence.suppress_meta_fkey_rewrites ||
      settings.suppress_meta_fkey_rewrites !=
          kDefaultSuppressMetaFKeyRewrites) {
    settings_dict.Set(prefs::kKeyboardSettingSuppressMetaFKeyRewrites,
                      settings.suppress_meta_fkey_rewrites);
  }

  if (ExistingSettingsHasValue(prefs::kKeyboardSettingTopRowAreFKeys,
                               existing_settings_dict) ||
              force_persistence.top_row_are_fkeys ||
              settings.top_row_are_fkeys != keyboard.is_external
          ? kDefaultTopRowAreFKeysExternal
          : kDefaultTopRowAreFKeys) {
    settings_dict.Set(prefs::kKeyboardSettingTopRowAreFKeys,
                      settings.top_row_are_fkeys);
  }

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

  // If an old settings dict already exists for the device, merge the updated
  // settings into the old settings. Otherwise, insert the dict at
  // `keyboard.device_key`.
  if (existing_settings_dict) {
    existing_settings_dict->Merge(std::move(settings_dict));
  } else {
    devices_dict.Set(keyboard.device_key, std::move(settings_dict));
  }

  pref_service->SetDict(std::string(prefs::kKeyboardDeviceSettingsDictPref),
                        std::move(devices_dict));
}

}  // namespace

KeyboardPrefHandlerImpl::KeyboardPrefHandlerImpl() = default;
KeyboardPrefHandlerImpl::~KeyboardPrefHandlerImpl() = default;

void KeyboardPrefHandlerImpl::InitializeKeyboardSettings(
    PrefService* pref_service,
    mojom::Keyboard* keyboard) {
  if (!pref_service) {
    keyboard->settings =
        GetDefaultKeyboardSettings(keyboard->is_external, keyboard->meta_key);
    return;
  }

  const auto& devices_dict =
      pref_service->GetDict(prefs::kKeyboardDeviceSettingsDictPref);
  const auto* settings_dict = devices_dict.FindDict(keyboard->device_key);
  ForceKeyboardSettingPersistence force_persistence;

  if (settings_dict) {
    keyboard->settings =
        RetrieveKeyboardSettings(pref_service, *keyboard, *settings_dict);
  } else if (Shell::Get()->input_device_tracker()->WasDevicePreviouslyConnected(
                 InputDeviceTracker::InputDeviceCategory::kKeyboard,
                 keyboard->device_key)) {
    keyboard->settings = GetKeyboardSettingsFromGlobalPrefs(
        pref_service, *keyboard, force_persistence);
  } else {
    keyboard->settings =
        GetDefaultKeyboardSettings(keyboard->is_external, keyboard->meta_key);
  }
  DCHECK(keyboard->settings);

  UpdateKeyboardSettingsImpl(pref_service, *keyboard, force_persistence);
}

void KeyboardPrefHandlerImpl::UpdateKeyboardSettings(
    PrefService* pref_service,
    const mojom::Keyboard& keyboard) {
  UpdateKeyboardSettingsImpl(pref_service, keyboard, /*force_persistence=*/{});
}

}  // namespace ash
