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
#include "ash/system/input_device_settings/input_device_settings_metrics_manager.h"
#include "ash/system/input_device_settings/input_device_settings_pref_names.h"
#include "ash/system/input_device_settings/input_device_settings_utils.h"
#include "ash/system/input_device_settings/input_device_tracker.h"
#include "ash/system/input_device_settings/settings_updated_metrics_info.h"
#include "base/containers/contains.h"
#include "base/containers/fixed_flat_map.h"
#include "base/containers/flat_map.h"
#include "base/json/values_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/known_user.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/ash/mojom/extended_fkeys_modifier.mojom-shared.h"
#include "ui/events/ash/mojom/extended_fkeys_modifier.mojom.h"
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

bool IsAppleKeyboardDefaultModifierRemapping(ui::mojom::ModifierKey from,
                                             ui::mojom::ModifierKey to) {
  return (from == ui::mojom::ModifierKey::kMeta &&
          to == ui::mojom::ModifierKey::kControl) ||
         (from == ui::mojom::ModifierKey::kControl &&
          to == ui::mojom::ModifierKey::kMeta);
}

bool ShouldAddExtendedFkeyProperties(const mojom::Keyboard& keyboard) {
  return ::features::AreF11AndF12ShortcutsEnabled() &&
         IsChromeOSKeyboard(keyboard);
}

const char* GetDefaultKeyboardPref(const mojom::Keyboard& keyboard) {
  return IsChromeOSKeyboard(keyboard)
             ? prefs::kKeyboardDefaultChromeOSSettings
             : prefs::kKeyboardDefaultNonChromeOSSettings;
}

ui::mojom::ExtendedFkeysModifier GetDefaultExtendedFkeysValue(
    const mojom::KeyboardPolicies& keyboard_policies,
    const mojom::Keyboard& keyboard) {
  if (keyboard_policies.extended_fkeys_policy &&
      keyboard_policies.extended_fkeys_policy->policy_status ==
          mojom::PolicyStatus::kRecommended) {
    return keyboard_policies.extended_fkeys_policy->value;
  }

  return kDefaultFkey;
}

bool GetDefaultSuppressMetaFkeyRewritesValue(
    const mojom::KeyboardPolicies& keyboard_policies,
    const mojom::Keyboard& keyboard) {
  if (keyboard_policies.enable_meta_fkey_rewrites_policy &&
      keyboard_policies.enable_meta_fkey_rewrites_policy->policy_status ==
          mojom::PolicyStatus::kRecommended) {
    // Invert the value of the policy when getting the default value for the
    // setting, because the policy determines whether meta fkey rewrites are
    // enabled, and the setting controls whether meta fkey rewrites are
    // disabled.
    return !keyboard_policies.enable_meta_fkey_rewrites_policy->value;
  }

  return kDefaultSuppressMetaFKeyRewrites;
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
// If `prefs` is nullptr, return defaults.
mojom::SixPackKeyInfoPtr GetSixPackKeyRemappings(PrefService* prefs) {
  mojom::SixPackKeyInfoPtr six_pack_key_info = mojom::SixPackKeyInfo::New();
  if (!prefs) {
    return six_pack_key_info;
  }

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

  settings->suppress_meta_fkey_rewrites =
      GetDefaultSuppressMetaFkeyRewritesValue(keyboard_policies, keyboard);
  // Do not persist as default should not be persisted.
  force_persistence.suppress_meta_fkey_rewrites = false;

  settings->modifier_remappings = GetModifierRemappings(prefs, keyboard);

  if (features::IsAltClickAndSixPackCustomizationEnabled()) {
    settings->six_pack_key_remappings = GetSixPackKeyRemappings(prefs);
  }

  if (ShouldAddExtendedFkeyProperties(keyboard)) {
    const auto fkey_modifier =
        GetDefaultExtendedFkeysValue(keyboard_policies, keyboard);
    settings->f11 = fkey_modifier;
    settings->f12 = fkey_modifier;
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

base::flat_map<ui::mojom::ModifierKey, ui::mojom::ModifierKey>
RetrieveModifierRemappings(const mojom::Keyboard& keyboard,
                           const base::Value::Dict& modifier_remappings_dict) {
  base::flat_map<ui::mojom::ModifierKey, ui::mojom::ModifierKey>
      modifier_remappings;

  for (const auto [from, to] : modifier_remappings_dict) {
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
    if (from_key == to_key) {
      continue;
    }

    // Do not add modifier remappings for modifier keys that do not exist on the
    // given keyboard.
    if (!base::Contains(keyboard.modifier_keys, from_key)) {
      continue;
    }

    modifier_remappings[from_key] = to_key;
  }

  if (keyboard.meta_key == mojom::MetaKey::kCommand) {
    if (!modifier_remappings_dict.contains(base::NumberToString(
            static_cast<int>(ui::mojom::ModifierKey::kMeta)))) {
      modifier_remappings[ui::mojom::ModifierKey::kMeta] =
          ui::mojom::ModifierKey::kControl;
    }

    if (!modifier_remappings_dict.contains(base::NumberToString(
            static_cast<int>(ui::mojom::ModifierKey::kControl)))) {
      modifier_remappings[ui::mojom::ModifierKey::kControl] =
          ui::mojom::ModifierKey::kMeta;
    }
  }

  return modifier_remappings;
}

mojom::KeyboardSettingsPtr RetrieveKeyboardSettings(
    PrefService* pref_service,
    const mojom::KeyboardPolicies& keyboard_policies,
    const mojom::Keyboard& keyboard,
    const base::Value::Dict& settings_dict) {
  mojom::KeyboardSettingsPtr settings = mojom::KeyboardSettings::New();
  settings->suppress_meta_fkey_rewrites =
      settings_dict.FindBool(prefs::kKeyboardSettingSuppressMetaFKeyRewrites)
          .value_or(GetDefaultSuppressMetaFkeyRewritesValue(keyboard_policies,
                                                            keyboard));
  settings->top_row_are_fkeys =
      settings_dict.FindBool(prefs::kKeyboardSettingTopRowAreFKeys)
          .value_or(GetDefaultTopRowAreFKeysValue(keyboard_policies, keyboard));

  if (ShouldAddExtendedFkeyProperties(keyboard)) {
    settings->f11 =
        settings_dict.Find(prefs::kKeyboardSettingF11)
            ? static_cast<ui::mojom::ExtendedFkeysModifier>(
                  settings_dict.FindInt(prefs::kKeyboardSettingF11).value())
            : GetDefaultExtendedFkeysValue(keyboard_policies, keyboard);

    settings->f12 =
        settings_dict.Find(prefs::kKeyboardSettingF12)
            ? static_cast<ui::mojom::ExtendedFkeysModifier>(
                  settings_dict.FindInt(prefs::kKeyboardSettingF12).value())
            : GetDefaultExtendedFkeysValue(keyboard_policies, keyboard);
  }

  const auto* modifier_remappings_dict =
      settings_dict.FindDict(prefs::kKeyboardSettingModifierRemappings);
  if (modifier_remappings_dict) {
    settings->modifier_remappings =
        RetrieveModifierRemappings(keyboard, *modifier_remappings_dict);
  } else {
    settings->modifier_remappings =
        RetrieveModifierRemappings(keyboard, /*modifier_remappings_dict=*/{});
  }

  if (features::IsAltClickAndSixPackCustomizationEnabled()) {
    settings->six_pack_key_remappings =
        RetrieveSixPackRemappings(pref_service, settings_dict);
  }

  return settings;
}

mojom::KeyboardSettingsPtr GetDefaultKeyboardSettings(
    PrefService* pref_service,
    const mojom::KeyboardPolicies& keyboard_policies,
    const mojom::Keyboard& keyboard) {
  if (pref_service) {
    const auto& default_settings_dict =
        pref_service->GetDict(GetDefaultKeyboardPref(keyboard));
    return RetrieveKeyboardSettings(pref_service, keyboard_policies, keyboard,
                                    default_settings_dict);
  }

  return RetrieveKeyboardSettings(pref_service, keyboard_policies, keyboard,
                                  /*settings_dict=*/{});
}

base::Value::Dict ConvertModifierRemappingsToDict(
    const mojom::Keyboard& keyboard) {
  // Modifier remappings get stored in a dict by casting the
  // `ui::mojom::ModifierKey` enum to ints. Since `base::Value::Dict` only
  // supports strings as keys, this is then converted into a string.
  base::Value::Dict modifier_remappings;
  for (const auto& [from, to] : keyboard.settings->modifier_remappings) {
    // Avoid saving modifier remappings that are default for apple keyboards.
    if (keyboard.meta_key == mojom::MetaKey::kCommand &&
        IsAppleKeyboardDefaultModifierRemapping(from, to)) {
      continue;
    }

    modifier_remappings.Set(base::NumberToString(static_cast<int>(from)),
                            static_cast<int>(to));
  }

  // Since Apple keyboards default remaps Meta -> Control and Control -> Meta,
  // this must be taken in to account when saving prefs so we store them when
  // they are non-default.
  if (keyboard.meta_key == mojom::MetaKey::kCommand) {
    if (!keyboard.settings->modifier_remappings.contains(
            ui::mojom::ModifierKey::kMeta)) {
      modifier_remappings.Set(
          base::NumberToString(static_cast<int>(ui::mojom::ModifierKey::kMeta)),
          static_cast<int>(ui::mojom::ModifierKey::kMeta));
    }

    if (!keyboard.settings->modifier_remappings.contains(
            ui::mojom::ModifierKey::kControl)) {
      modifier_remappings.Set(
          base::NumberToString(
              static_cast<int>(ui::mojom::ModifierKey::kControl)),
          static_cast<int>(ui::mojom::ModifierKey::kControl));
    }
  }

  return modifier_remappings;
}

base::Value::Dict ConvertSettingsToDict(
    const mojom::Keyboard& keyboard,
    const mojom::KeyboardPolicies& keyboard_policies,
    const ForceKeyboardSettingPersistence& force_persistence,
    const base::Value::Dict* existing_settings_dict) {
  // Populate `settings_dict` with all settings in `settings`.
  base::Value::Dict settings_dict;

  if (ShouldPersistSetting(
          prefs::kKeyboardSettingSuppressMetaFKeyRewrites,
          keyboard.settings->suppress_meta_fkey_rewrites,
          GetDefaultSuppressMetaFkeyRewritesValue(keyboard_policies, keyboard),
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

  if (ShouldAddExtendedFkeyProperties(keyboard)) {
    if (ShouldPersistFkeySetting(
            keyboard_policies.extended_fkeys_policy, prefs::kKeyboardSettingF11,
            keyboard.settings->f11,
            GetDefaultExtendedFkeysValue(keyboard_policies, keyboard),
            existing_settings_dict)) {
      settings_dict.Set(prefs::kKeyboardSettingF11,
                        static_cast<int>(keyboard.settings->f11.value()));
    }
    if (ShouldPersistFkeySetting(
            keyboard_policies.extended_fkeys_policy, prefs::kKeyboardSettingF12,
            keyboard.settings->f12,
            GetDefaultExtendedFkeysValue(keyboard_policies, keyboard),
            existing_settings_dict)) {
      settings_dict.Set(prefs::kKeyboardSettingF12,
                        static_cast<int>(keyboard.settings->f12.value()));
    }
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

  settings_dict.Set(prefs::kKeyboardSettingModifierRemappings,
                    ConvertModifierRemappingsToDict(keyboard));
  return settings_dict;
}

void UpdateInternalKeyboardSettingsImpl(
    PrefService* pref_service,
    const mojom::KeyboardPolicies& keyboard_policies,
    const mojom::Keyboard& keyboard,
    const ForceKeyboardSettingPersistence& force_persistence) {
  CHECK(keyboard.settings);
  CHECK(!keyboard.is_external);

  base::Value::Dict existing_settings_dict =
      pref_service->GetDict(prefs::kKeyboardInternalSettings).Clone();
  base::Value::Dict settings_dict = ConvertSettingsToDict(
      keyboard, keyboard_policies, force_persistence, &existing_settings_dict);

  // Merge all settings except modifier remappings. Modifier remappings need
  // to overwrite what was previously stored.
  auto modifier_remappings_dict =
      settings_dict.Extract(prefs::kKeyboardSettingModifierRemappings);
  existing_settings_dict.Merge(std::move(settings_dict));
  existing_settings_dict.Set(prefs::kKeyboardSettingModifierRemappings,
                             std::move(*modifier_remappings_dict));

  pref_service->SetDict(prefs::kKeyboardInternalSettings,
                        std::move(existing_settings_dict));
}

void UpdateKeyboardSettingsImpl(
    PrefService* pref_service,
    const mojom::KeyboardPolicies& keyboard_policies,
    const mojom::Keyboard& keyboard,
    const ForceKeyboardSettingPersistence& force_persistence) {
  DCHECK(keyboard.settings);

  if (!keyboard.is_external) {
    UpdateInternalKeyboardSettingsImpl(pref_service, keyboard_policies,
                                       keyboard, force_persistence);
    return;
  }

  base::Value::Dict devices_dict =
      pref_service->GetDict(prefs::kKeyboardDeviceSettingsDictPref).Clone();
  base::Value::Dict* existing_settings_dict =
      devices_dict.FindDict(keyboard.device_key);
  base::Value::Dict settings_dict = ConvertSettingsToDict(
      keyboard, keyboard_policies, force_persistence, existing_settings_dict);
  const base::Time time_stamp = base::Time::Now();
  settings_dict.Set(prefs::kLastUpdatedKey, base::TimeToValue(time_stamp));

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
    mojom::Keyboard& keyboard) {
  mojom::KeyboardSettingsPtr settings = GetDefaultKeyboardSettings(
      /*pref_service=*/nullptr, keyboard_policies, keyboard);

  settings->modifier_remappings = GetModifierRemappingsKnownUser(
      user_manager::KnownUser(local_state), account_id, keyboard);

  return settings;
}

bool HasDefaultSettings(PrefService* pref_service,
                        const mojom::Keyboard& keyboard) {
  const auto* pref =
      pref_service->FindPreference(GetDefaultKeyboardPref(keyboard));
  return pref && pref->HasUserSetting();
}

void InitializeSettingsUpdateMetricInfo(
    PrefService* pref_service,
    const mojom::Keyboard& keyboard,
    SettingsUpdatedMetricsInfo::Category category) {
  CHECK(pref_service);

  const auto& settings_metric_info =
      pref_service->GetDict(prefs::kKeyboardUpdateSettingsMetricInfo);
  const auto* device_metric_info =
      settings_metric_info.Find(keyboard.device_key);
  if (device_metric_info) {
    return;
  }

  auto updated_metric_info = settings_metric_info.Clone();

  const SettingsUpdatedMetricsInfo metrics_info(category, base::Time::Now());
  updated_metric_info.Set(keyboard.device_key, metrics_info.ToDict());

  pref_service->SetDict(prefs::kKeyboardUpdateSettingsMetricInfo,
                        std::move(updated_metric_info));
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
        GetDefaultKeyboardSettings(pref_service, keyboard_policies, *keyboard);
    return;
  }

  const base::Value::Dict* settings_dict = nullptr;
  if (!keyboard->is_external) {
    settings_dict = &pref_service->GetDict(prefs::kKeyboardInternalSettings);
    if (settings_dict->empty()) {
      settings_dict = nullptr;
    }
  } else {
    const auto& devices_dict =
        pref_service->GetDict(prefs::kKeyboardDeviceSettingsDictPref);
    settings_dict = devices_dict.FindDict(keyboard->device_key);
  }

  ForceKeyboardSettingPersistence force_persistence;
  SettingsUpdatedMetricsInfo::Category category;
  if (settings_dict) {
    category = SettingsUpdatedMetricsInfo::Category::kSynced;
    keyboard->settings = RetrieveKeyboardSettings(
        pref_service, keyboard_policies, *keyboard, *settings_dict);
  } else if (Shell::Get()->input_device_tracker()->WasDevicePreviouslyConnected(
                 InputDeviceTracker::InputDeviceCategory::kKeyboard,
                 keyboard->device_key)) {
    category = SettingsUpdatedMetricsInfo::Category::kDefault;
    keyboard->settings = GetKeyboardSettingsFromGlobalPrefs(
        pref_service, keyboard_policies, *keyboard, force_persistence);
  } else {
    keyboard->settings =
        GetDefaultKeyboardSettings(pref_service, keyboard_policies, *keyboard);
    category = HasDefaultSettings(pref_service, *keyboard)
                   ? SettingsUpdatedMetricsInfo::Category::kDefault
                   : SettingsUpdatedMetricsInfo::Category::kFirstEver;
  }
  DCHECK(keyboard->settings);
  InitializeSettingsUpdateMetricInfo(pref_service, *keyboard, category);

  UpdateKeyboardSettingsImpl(pref_service, keyboard_policies, *keyboard,
                             force_persistence);

  if (keyboard_policies.top_row_are_fkeys_policy &&
      keyboard_policies.top_row_are_fkeys_policy->policy_status ==
          mojom::PolicyStatus::kManaged) {
    keyboard->settings->top_row_are_fkeys =
        keyboard_policies.top_row_are_fkeys_policy->value;
  }

  if (keyboard->is_external &&
      keyboard_policies.enable_meta_fkey_rewrites_policy &&
      keyboard_policies.enable_meta_fkey_rewrites_policy->policy_status ==
          mojom::PolicyStatus::kManaged) {
    // Invert the value of the policy when saving the setting, because the
    // policy determines whether meta fkey rewrites are enabled, and the setting
    // controls whether meta fkey rewrites are disabled.
    keyboard->settings->suppress_meta_fkey_rewrites =
        !keyboard_policies.enable_meta_fkey_rewrites_policy->value;
  }

  if (ShouldAddExtendedFkeyProperties(*keyboard) &&
      keyboard_policies.extended_fkeys_policy &&
      keyboard_policies.extended_fkeys_policy->policy_status ==
          mojom::PolicyStatus::kManaged) {
    keyboard->settings->f11 = keyboard_policies.extended_fkeys_policy->value;
    keyboard->settings->f12 = keyboard_policies.extended_fkeys_policy->value;
  }
}

void KeyboardPrefHandlerImpl::InitializeLoginScreenKeyboardSettings(
    PrefService* local_state,
    const AccountId& account_id,
    const mojom::KeyboardPolicies& keyboard_policies,
    mojom::Keyboard* keyboard) {
  // Verify if the flag is enabled.
  if (!features::IsInputDeviceSettingsSplitEnabled()) {
    return;
  }
  CHECK(local_state);

  const auto* settings_dict = GetLoginScreenSettingsDict(
      local_state, account_id,
      keyboard->is_external ? prefs::kKeyboardLoginScreenExternalSettingsPref
                            : prefs::kKeyboardLoginScreenInternalSettingsPref);
  if (settings_dict) {
    keyboard->settings = RetrieveKeyboardSettings(
        /*pref_service=*/nullptr, keyboard_policies, *keyboard, *settings_dict);
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
               std::make_optional<base::Value>(ConvertSettingsToDict(
                   keyboard, keyboard_policies, /*force_persistence=*/{},
                   settings_dict)));
}

void KeyboardPrefHandlerImpl::InitializeWithDefaultKeyboardSettings(
    const mojom::KeyboardPolicies& keyboard_policies,
    mojom::Keyboard* keyboard) {
  keyboard->settings = GetDefaultKeyboardSettings(/*pref_service=*/nullptr,
                                                  keyboard_policies, *keyboard);
}

void KeyboardPrefHandlerImpl::UpdateDefaultChromeOSKeyboardSettings(
    PrefService* pref_service,
    const mojom::KeyboardPolicies& keyboard_policies,
    const mojom::Keyboard& keyboard) {
  CHECK(IsChromeOSKeyboard(keyboard));
  // All settings should be persisted fully when storing defaults.
  auto settings_dict = ConvertSettingsToDict(
      keyboard, keyboard_policies, /*force_persistence=*/{true},
      /*existing_settings_dict=*/nullptr);
  pref_service->SetDict(prefs::kKeyboardDefaultChromeOSSettings,
                        std::move(settings_dict));
}

void KeyboardPrefHandlerImpl::UpdateDefaultNonChromeOSKeyboardSettings(
    PrefService* pref_service,
    const mojom::KeyboardPolicies& keyboard_policies,
    const mojom::Keyboard& keyboard) {
  CHECK(!IsChromeOSKeyboard(keyboard));
  // All settings should be persisted fully when storing defaults.
  auto settings_dict = ConvertSettingsToDict(
      keyboard, keyboard_policies, /*force_persistence=*/{true},
      /*existing_settings_dict=*/nullptr);
  pref_service->SetDict(prefs::kKeyboardDefaultNonChromeOSSettings,
                        std::move(settings_dict));
}

}  // namespace ash
