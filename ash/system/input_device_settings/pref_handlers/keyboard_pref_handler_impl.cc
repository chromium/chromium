// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/pref_handlers/keyboard_pref_handler_impl.h"

#include "ash/constants/ash_features.h"
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
#include "base/values.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/known_user.h"
#include "ui/events/ash/mojom/modifier_key.mojom-shared.h"
#include "ui/events/ash/mojom/modifier_key.mojom.h"
#include "ui/events/ash/mojom/six_pack_shortcut_modifier.mojom-shared.h"
#include "ui/events/ash/mojom/six_pack_shortcut_modifier.mojom.h"
#include "ui/events/ash/pref_names.h"

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

bool GetDefaultTopRowAreFKeysValue(
    const mojom::KeyboardPolicies& keyboard_policies,
    const mojom::Keyboard& keyboard) {
  if (keyboard_policies.top_row_are_fkeys_policy &&
      keyboard_policies.top_row_are_fkeys_policy->policy_status ==
          mojom::PolicyStatus::kRecommended) {
    return keyboard_policies.top_row_are_fkeys_policy->value;
  }

  return keyboard.is_external ? kDefaultTopRowAreFKeysExternal
                              : kDefaultTopRowAreFKeys;
}

mojom::KeyboardSettingsPtr GetDefaultKeyboardSettings(
    const mojom::KeyboardPolicies& keyboard_policies,
    const mojom::Keyboard& keyboard) {
  mojom::KeyboardSettingsPtr settings = mojom::KeyboardSettings::New();
  settings->suppress_meta_fkey_rewrites = kDefaultSuppressMetaFKeyRewrites;
  settings->top_row_are_fkeys =
      GetDefaultTopRowAreFKeysValue(keyboard_policies, keyboard);
  // Switch control and command for Apple keyboards.
  if (keyboard.meta_key == mojom::MetaKey::kCommand) {
    settings->modifier_remappings[ui::mojom::ModifierKey::kControl] =
        ui::mojom::ModifierKey::kMeta;
    settings->modifier_remappings[ui::mojom::ModifierKey::kMeta] =
        ui::mojom::ModifierKey::kControl;
  }

  if (features::IsAltClickAndSixPackCustomizationEnabled()) {
    settings->six_pack_key_remappings = mojom::SixPackKeyInfo::New();
  }
  return settings;
}

int GetSixPackKeyPrefCount(PrefService* prefs, const char* pref_name) {
  const auto* pref = prefs->GetUserPrefValue(pref_name);
  return pref ? pref->GetInt() : 0;
}

// Return the modifier used more frequently, in case of a tie, Search will
// be preferred to avoid Alt-based issues.
// Default setting:
//  Pref contains a positive value: SixPackShortcutModifier::kAlt
//  Pref contains a negative value: SixPackShortcutModifier::kSearch
ui::mojom::SixPackShortcutModifier GetSixPackKeyModifierFromPrefCount(
    int count) {
  return count <= 0 ? ui::mojom::SixPackShortcutModifier::kSearch
                    : ui::mojom::SixPackShortcutModifier::kAlt;
}

ui::mojom::SixPackShortcutModifier GetSixPackShortcutModifierFromSettingsDict(
    const base::Value::Dict& six_pack_key_remappings_dict,
    const char* pref_name) {
  return static_cast<ui::mojom::SixPackShortcutModifier>(
      *six_pack_key_remappings_dict.FindInt(pref_name));
}

// Each pref contains an integer value which may be positive or negative
// depending on whether the user uses the Alt or Search based rewrite more
// frequently. When dealing with the grouped 6-pack keys
// (PageUp/PageDown, Home/End), both prefs will be used when determining what
// value to set to avoid setting inconsistent values for similar 6-pack keys.
mojom::SixPackKeyInfoPtr GetSixPackKeyRemappings(PrefService* prefs) {
  mojom::SixPackKeyInfoPtr six_pack_key_info = mojom::SixPackKeyInfo::New();
  const auto page_up_down_modifier = GetSixPackKeyModifierFromPrefCount(
      GetSixPackKeyPrefCount(prefs, prefs::kKeyEventRemappedToSixPackPageDown) +
      GetSixPackKeyPrefCount(prefs, prefs::kKeyEventRemappedToSixPackPageUp));
  six_pack_key_info->page_down = page_up_down_modifier;
  six_pack_key_info->page_up = page_up_down_modifier;

  const auto home_end_modifier = GetSixPackKeyModifierFromPrefCount(
      GetSixPackKeyPrefCount(prefs, prefs::kKeyEventRemappedToSixPackHome) +
      GetSixPackKeyPrefCount(prefs, prefs::kKeyEventRemappedToSixPackEnd));
  six_pack_key_info->home = home_end_modifier;
  six_pack_key_info->end = home_end_modifier;

  six_pack_key_info->del = GetSixPackKeyModifierFromPrefCount(
      GetSixPackKeyPrefCount(prefs, prefs::kKeyEventRemappedToSixPackDelete));

  // The "Insert" key is always set to `kSearch` since the
  // (Search+Shift+Backspace) rewrite is the only way to emit an "Insert" key
  // event.
  six_pack_key_info->insert = ui::mojom::SixPackShortcutModifier::kSearch;
  return six_pack_key_info;
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

base::flat_map<ui::mojom::ModifierKey, ui::mojom::ModifierKey>
GetModifierRemappingsKnownUser(const user_manager::KnownUser& known_user,
                               const AccountId& account_id,
                               const mojom::Keyboard& keyboard) {
  base::flat_map<ui::mojom::ModifierKey, ui::mojom::ModifierKey> remappings;

  for (const auto& modifier_key : keyboard.modifier_keys) {
    if (modifier_key == ui::mojom::ModifierKey::kMeta) {
      // The meta key is handled separately.
      continue;
    }
    auto* it = kKeyboardModifierMappings.find(modifier_key);
    DCHECK(it != kKeyboardModifierMappings.end());
    const auto pref_modifier_key = static_cast<ui::mojom::ModifierKey>(
        known_user.FindIntPath(account_id, it->second)
            .value_or(static_cast<int>(modifier_key)));
    if (modifier_key != pref_modifier_key) {
      remappings.emplace(modifier_key, pref_modifier_key);
    }
  }

  const auto meta_key_pref_value = static_cast<ui::mojom::ModifierKey>(
      known_user.FindIntPath(account_id, kMetaKeyMapping.at(keyboard.meta_key))
          .value_or(static_cast<int>(ui::mojom::ModifierKey::kMeta)));
  if (ui::mojom::ModifierKey::kMeta != meta_key_pref_value) {
    remappings.emplace(ui::mojom::ModifierKey::kMeta, meta_key_pref_value);
  }
  return remappings;
}

mojom::KeyboardSettingsPtr GetKeyboardSettingsFromGlobalPrefs(
    PrefService* prefs,
    const mojom::KeyboardPolicies& keyboard_policies,
    const mojom::Keyboard& keyboard,
    ForceKeyboardSettingPersistence& force_persistence) {
  mojom::KeyboardSettingsPtr settings = mojom::KeyboardSettings::New();

  // For the transition period, since the default behavior changed for external
  // keyboards, the value from prefs must always be used even if the user did
  // not explicitly configure it. Users expect their settings to remain
  // consistent even if we think they may like the new default better.
  settings->top_row_are_fkeys = prefs->GetBoolean(prefs::kSendFunctionKeys);
  force_persistence.top_row_are_fkeys =
      prefs->GetUserPrefValue(prefs::kSendFunctionKeys) != nullptr;

  settings->suppress_meta_fkey_rewrites = kDefaultSuppressMetaFKeyRewrites;
  // Do not persist as default should not be persisted.
  force_persistence.suppress_meta_fkey_rewrites = false;

  settings->modifier_remappings = GetModifierRemappings(prefs, keyboard);

  if (features::IsAltClickAndSixPackCustomizationEnabled()) {
    settings->six_pack_key_remappings = GetSixPackKeyRemappings(prefs);
  }
  return settings;
}

mojom::SixPackKeyInfoPtr RetrieveSixPackRemappings(
    PrefService* pref_service,
    const base::Value::Dict& settings_dict) {
  const auto* six_pack_key_remappings_dict =
      settings_dict.FindDict(prefs::kKeyboardSettingSixPackKeyRemappings);
  if (!six_pack_key_remappings_dict) {
    return GetSixPackKeyRemappings(pref_service);
  } else {
    mojom::SixPackKeyInfoPtr six_pack_key_info = mojom::SixPackKeyInfo::New();
    six_pack_key_info->page_up = GetSixPackShortcutModifierFromSettingsDict(
        *six_pack_key_remappings_dict, prefs::kSixPackKeyPageUp);
    six_pack_key_info->page_down = GetSixPackShortcutModifierFromSettingsDict(
        *six_pack_key_remappings_dict, prefs::kSixPackKeyPageDown);
    six_pack_key_info->home = GetSixPackShortcutModifierFromSettingsDict(
        *six_pack_key_remappings_dict, prefs::kSixPackKeyHome);
    six_pack_key_info->end = GetSixPackShortcutModifierFromSettingsDict(
        *six_pack_key_remappings_dict, prefs::kSixPackKeyEnd);
    six_pack_key_info->del = GetSixPackShortcutModifierFromSettingsDict(
        *six_pack_key_remappings_dict, prefs::kSixPackKeyDelete);
    six_pack_key_info->insert = GetSixPackShortcutModifierFromSettingsDict(
        *six_pack_key_remappings_dict, prefs::kSixPackKeyInsert);

    return six_pack_key_info;
  }
}

mojom::KeyboardSettingsPtr RetrieveKeyboardSettings(
    const mojom::KeyboardPolicies& keyboard_policies,
    const mojom::Keyboard& keyboard,
    const base::Value::Dict& settings_dict) {
  mojom::KeyboardSettingsPtr settings = mojom::KeyboardSettings::New();
  settings->suppress_meta_fkey_rewrites =
      settings_dict.FindBool(prefs::kKeyboardSettingSuppressMetaFKeyRewrites)
          .value_or(kDefaultSuppressMetaFKeyRewrites);
  settings->top_row_are_fkeys =
      settings_dict.FindBool(prefs::kKeyboardSettingTopRowAreFKeys)
          .value_or(GetDefaultTopRowAreFKeysValue(keyboard_policies, keyboard));

  const auto* modifier_remappings_dict =
      settings_dict.FindDict(prefs::kKeyboardSettingModifierRemappings);
  if (!modifier_remappings_dict) {
    return settings;
  }

  for (const auto [from, to] : *modifier_remappings_dict) {
    // `from` must be a string which can be converted to an int and `to` must be
    // an int.
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

base::Value::Dict ConvertSettingsToDict(
    const mojom::Keyboard& keyboard,
    const mojom::KeyboardPolicies& keyboard_policies,
    const ForceKeyboardSettingPersistence& force_persistence,
    const base::Value::Dict* existing_settings_dict) {
  // Populate `settings_dict` with all settings in `settings`.
  base::Value::Dict settings_dict;

  if (ShouldPersistSetting(prefs::kKeyboardSettingSuppressMetaFKeyRewrites,
                           keyboard.settings->suppress_meta_fkey_rewrites,
                           kDefaultSuppressMetaFKeyRewrites,
                           force_persistence.suppress_meta_fkey_rewrites,
                           existing_settings_dict)) {
    settings_dict.Set(prefs::kKeyboardSettingSuppressMetaFKeyRewrites,
                      keyboard.settings->suppress_meta_fkey_rewrites);
  }

  if (ShouldPersistSetting(
          keyboard_policies.top_row_are_fkeys_policy,
          prefs::kKeyboardSettingTopRowAreFKeys,
          keyboard.settings->top_row_are_fkeys,
          GetDefaultTopRowAreFKeysValue(keyboard_policies, keyboard),
          force_persistence.top_row_are_fkeys, existing_settings_dict)) {
    settings_dict.Set(prefs::kKeyboardSettingTopRowAreFKeys,
                      keyboard.settings->top_row_are_fkeys);
  }

  if (features::IsAltClickAndSixPackCustomizationEnabled()) {
    base::Value::Dict six_pack_key_remappings;
    six_pack_key_remappings.Set(
        prefs::kSixPackKeyPageUp,
        static_cast<int>(keyboard.settings->six_pack_key_remappings->page_up));
    six_pack_key_remappings.Set(
        prefs::kSixPackKeyPageDown,
        static_cast<int>(
            keyboard.settings->six_pack_key_remappings->page_down));
    six_pack_key_remappings.Set(
        prefs::kSixPackKeyHome,
        static_cast<int>(keyboard.settings->six_pack_key_remappings->home));
    six_pack_key_remappings.Set(
        prefs::kSixPackKeyEnd,
        static_cast<int>(keyboard.settings->six_pack_key_remappings->end));
    six_pack_key_remappings.Set(
        prefs::kSixPackKeyDelete,
        static_cast<int>(keyboard.settings->six_pack_key_remappings->del));
    six_pack_key_remappings.Set(
        prefs::kSixPackKeyInsert,
        static_cast<int>(keyboard.settings->six_pack_key_remappings->insert));
    settings_dict.Set(prefs::kKeyboardSettingSixPackKeyRemappings,
                      std::move(six_pack_key_remappings));
  }

  // Modifier remappings get stored in a dict by casting the
  // `ui::mojom::ModifierKey` enum to ints. Since `base::Value::Dict` only
  // supports strings as keys, this is then converted into a string.
  base::Value::Dict modifier_remappings;
  for (const auto& [from, to] : keyboard.settings->modifier_remappings) {
    modifier_remappings.Set(base::NumberToString(static_cast<int>(from)),
                            static_cast<int>(to));
  }
  settings_dict.Set(prefs::kKeyboardSettingModifierRemappings,
                    std::move(modifier_remappings));
  return settings_dict;
}

void UpdateKeyboardSettingsImpl(
    PrefService* pref_service,
    const mojom::KeyboardPolicies& keyboard_policies,
    const mojom::Keyboard& keyboard,
    const ForceKeyboardSettingPersistence& force_persistence) {
  DCHECK(keyboard.settings);

  base::Value::Dict devices_dict =
      pref_service->GetDict(prefs::kKeyboardDeviceSettingsDictPref).Clone();
  base::Value::Dict* existing_settings_dict =
      devices_dict.FindDict(keyboard.device_key);
  base::Value::Dict settings_dict = ConvertSettingsToDict(
      keyboard, keyboard_policies, force_persistence, existing_settings_dict);

  if (existing_settings_dict) {
    // Merge all settings except modifier remappings. Modifier remappings need
    // to overwrite what was previously stored.
    auto modifier_remappings_dict =
        settings_dict.Extract(prefs::kKeyboardSettingModifierRemappings);
    existing_settings_dict->Merge(std::move(settings_dict));
    existing_settings_dict->Set(prefs::kKeyboardSettingModifierRemappings,
                                std::move(*modifier_remappings_dict));
    if (features::IsAltClickAndSixPackCustomizationEnabled()) {
      // 6-pack key remappings need to overwrite what was previously stored.
      auto six_pack_key_remappings_dict =
          settings_dict.Extract(prefs::kKeyboardSettingSixPackKeyRemappings);
      if (six_pack_key_remappings_dict.has_value()) {
        existing_settings_dict->Set(prefs::kKeyboardSettingSixPackKeyRemappings,
                                    std::move(*six_pack_key_remappings_dict));
      }
    }

  } else {
    devices_dict.Set(keyboard.device_key, std::move(settings_dict));
  }

  pref_service->SetDict(std::string(prefs::kKeyboardDeviceSettingsDictPref),
                        std::move(devices_dict));
}

mojom::KeyboardSettingsPtr GetKeyboardSettingsFromOldLocalStatePrefs(
    PrefService* local_state,
    const AccountId& account_id,
    const mojom::KeyboardPolicies& keyboard_policies,
    const mojom::Keyboard& keyboard) {
  mojom::KeyboardSettingsPtr settings =
      GetDefaultKeyboardSettings(keyboard_policies, keyboard);

  settings->modifier_remappings = GetModifierRemappingsKnownUser(
      user_manager::KnownUser(local_state), account_id, keyboard);

  return settings;
}

}  // namespace

KeyboardPrefHandlerImpl::KeyboardPrefHandlerImpl() = default;
KeyboardPrefHandlerImpl::~KeyboardPrefHandlerImpl() = default;

void KeyboardPrefHandlerImpl::InitializeKeyboardSettings(
    PrefService* pref_service,
    const mojom::KeyboardPolicies& keyboard_policies,
    mojom::Keyboard* keyboard) {
  if (!pref_service) {
    keyboard->settings =
        GetDefaultKeyboardSettings(keyboard_policies, *keyboard);
    return;
  }

  const auto& devices_dict =
      pref_service->GetDict(prefs::kKeyboardDeviceSettingsDictPref);
  const auto* settings_dict = devices_dict.FindDict(keyboard->device_key);
  ForceKeyboardSettingPersistence force_persistence;

  if (settings_dict) {
    keyboard->settings =
        RetrieveKeyboardSettings(keyboard_policies, *keyboard, *settings_dict);
    if (features::IsAltClickAndSixPackCustomizationEnabled()) {
      keyboard->settings->six_pack_key_remappings =
          RetrieveSixPackRemappings(pref_service, *settings_dict);
    }

  } else if (Shell::Get()->input_device_tracker()->WasDevicePreviouslyConnected(
                 InputDeviceTracker::InputDeviceCategory::kKeyboard,
                 keyboard->device_key)) {
    keyboard->settings = GetKeyboardSettingsFromGlobalPrefs(
        pref_service, keyboard_policies, *keyboard, force_persistence);
  } else {
    keyboard->settings =
        GetDefaultKeyboardSettings(keyboard_policies, *keyboard);
  }
  DCHECK(keyboard->settings);

  UpdateKeyboardSettingsImpl(pref_service, keyboard_policies, *keyboard,
                             force_persistence);

  if (keyboard_policies.top_row_are_fkeys_policy &&
      keyboard_policies.top_row_are_fkeys_policy->policy_status ==
          mojom::PolicyStatus::kManaged) {
    keyboard->settings->top_row_are_fkeys =
        keyboard_policies.top_row_are_fkeys_policy->value;
  }
}

void KeyboardPrefHandlerImpl::InitializeLoginScreenKeyboardSettings(
    PrefService* local_state,
    const AccountId& account_id,
    const mojom::KeyboardPolicies& keyboard_policies,
    mojom::Keyboard* keyboard) {
  CHECK(local_state);
  // If the flag is disabled, clear all the settings dictionaries.
  if (!features::IsInputDeviceSettingsSplitEnabled()) {
    user_manager::KnownUser known_user(local_state);
    known_user.SetPath(account_id,
                       prefs::kKeyboardLoginScreenInternalSettingsPref,
                       absl::nullopt);
    known_user.SetPath(account_id,
                       prefs::kKeyboardLoginScreenExternalSettingsPref,
                       absl::nullopt);
    return;
  }

  const auto* settings_dict = GetLoginScreenSettingsDict(
      local_state, account_id,
      keyboard->is_external ? prefs::kKeyboardLoginScreenExternalSettingsPref
                            : prefs::kKeyboardLoginScreenInternalSettingsPref);
  if (settings_dict) {
    keyboard->settings =
        RetrieveKeyboardSettings(keyboard_policies, *keyboard, *settings_dict);
  } else {
    keyboard->settings = GetKeyboardSettingsFromOldLocalStatePrefs(
        local_state, account_id, keyboard_policies, *keyboard);
  }

  if (features::IsAltClickAndSixPackCustomizationEnabled()) {
    keyboard->settings->six_pack_key_remappings = mojom::SixPackKeyInfo::New();
  }
}

void KeyboardPrefHandlerImpl::UpdateKeyboardSettings(
    PrefService* pref_service,
    const mojom::KeyboardPolicies& keyboard_policies,
    const mojom::Keyboard& keyboard) {
  UpdateKeyboardSettingsImpl(pref_service, keyboard_policies, keyboard,
                             /*force_persistence=*/{});
}

void KeyboardPrefHandlerImpl::UpdateLoginScreenKeyboardSettings(
    PrefService* local_state,
    const AccountId& account_id,
    const mojom::KeyboardPolicies& keyboard_policies,
    const mojom::Keyboard& keyboard) {
  CHECK(local_state);
  const auto* pref_name = keyboard.is_external
                              ? prefs::kKeyboardLoginScreenExternalSettingsPref
                              : prefs::kKeyboardLoginScreenInternalSettingsPref;
  auto* settings_dict =
      GetLoginScreenSettingsDict(local_state, account_id, pref_name);
  user_manager::KnownUser(local_state)
      .SetPath(account_id, pref_name,
               absl::make_optional<base::Value>(ConvertSettingsToDict(
                   keyboard, keyboard_policies, /*force_persistence=*/{},
                   settings_dict)));
}

void KeyboardPrefHandlerImpl::InitializeWithDefaultKeyboardSettings(
    const mojom::KeyboardPolicies& keyboard_policies,
    mojom::Keyboard* keyboard) {
  keyboard->settings = GetDefaultKeyboardSettings(keyboard_policies, *keyboard);
}

}  // namespace ash
