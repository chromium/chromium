// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/input_device_settings_utils.h"

#include <string_view>

#include "ash/public/cpp/accelerators_util.h"
#include "ash/public/mojom/input_device_settings.mojom-shared.h"
#include "ash/public/mojom/input_device_settings.mojom.h"
#include "ash/shell.h"
#include "ash/system/input_device_settings/input_device_settings_pref_names.h"
#include "base/containers/contains.h"
#include "base/containers/fixed_flat_set.h"
#include "base/containers/flat_set.h"
#include "base/export_template.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/known_user.h"
#include "ui/events/ash/keyboard_capability.h"
#include "ui/events/ash/mojom/extended_fkeys_modifier.mojom-shared.h"
#include "ui/events/ash/mojom/meta_key.mojom-shared.h"
#include "ui/events/ash/mojom/modifier_key.mojom.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/ozone/evdev/keyboard_mouse_combo_device_metrics.h"
#include "ui/events/types/event_type.h"

namespace ash {

namespace {

const char kRedactedButtonName[] = "REDACTED";

std::string HexEncode(uint16_t v) {
  // Load the bytes into the bytes array in reverse order as hex number should
  // be read from left to right.
  uint8_t bytes[sizeof(uint16_t)];
  bytes[1] = v & 0xFF;
  bytes[0] = v >> 8;
  return base::ToLowerASCII(base::HexEncode(bytes));
}

bool ExistingSettingsHasValue(std::string_view setting_key,
                              const base::Value::Dict* existing_settings_dict) {
  if (!existing_settings_dict) {
    return false;
  }

  return existing_settings_dict->Find(setting_key) != nullptr;
}

bool IsAlphaKeyboardCode(ui::KeyboardCode key_code) {
  return GetKeyInputTypeFromKeyEvent(ui::KeyEvent(
             ui::EventType::kKeyPressed, key_code, ui::DomCode::NONE,
             ui::EF_NONE)) == AcceleratorKeyInputType::kAlpha;
}

bool IsNumberKeyboardCode(ui::KeyboardCode key_code) {
  return GetKeyInputTypeFromKeyEvent(ui::KeyEvent(
             ui::EventType::kKeyPressed, key_code, ui::DomCode::NONE,
             ui::EF_NONE)) == AcceleratorKeyInputType::kDigit;
}

// Verify if the customization restriction blocks the button remapping.
// Block button remapping in the following cases:
// 1. Customization restriction is kAllowCustomizations.
// 2. Customization restriction is kDisableKeyEventRewrites and button is not
// a keyboard key.
// 3. Customization restriction is kAllowAlphabetKeyEventRewrites and button
// is a mouse button or alphabet/punctuation keyboard key.
// 4. Customization restriction is kAllowAlphabetOrNumberKeyEventRewrites and
// button is a mouse button or alphabet, punctuation, or number keyboard key.
// In other cases, block button remapping.
bool RestrictionBlocksRemapping(
    const mojom::ButtonRemapping& remapping,
    mojom::CustomizationRestriction customization_restriction) {
  switch (customization_restriction) {
    case mojom::CustomizationRestriction::kAllowCustomizations:
      return false;
    case mojom::CustomizationRestriction::kDisallowCustomizations:
      return true;
    case mojom::CustomizationRestriction::kDisableKeyEventRewrites:
      // No keyboard keys are allowed to be remapped.
      if (remapping.button->is_vkey()) {
        return true;
      }

      // No horizontal scroll events are allowed to be remapped.
      if (remapping.button->is_customizable_button()) {
        const auto& customizable_button =
            remapping.button->get_customizable_button();
        return customizable_button == mojom::CustomizableButton::kScrollLeft ||
               customizable_button == mojom::CustomizableButton::kScrollRight;
      }
      return false;
    case mojom::CustomizationRestriction::kAllowAlphabetKeyEventRewrites:
      if (remapping.button->is_vkey() &&
          !IsAlphaKeyboardCode(remapping.button->get_vkey())) {
        return true;
      }
      return false;
    case mojom::CustomizationRestriction::
        kAllowAlphabetOrNumberKeyEventRewrites:
      if (remapping.button->is_vkey() &&
          !IsAlphaKeyboardCode(remapping.button->get_vkey()) &&
          !IsNumberKeyboardCode(remapping.button->get_vkey())) {
        return true;
      }
      return false;
    case mojom::CustomizationRestriction::kAllowHorizontalScrollWheelRewrites:
      return remapping.button->is_vkey();
    case mojom::CustomizationRestriction::kAllowTabEventRewrites:
      if (remapping.button->is_customizable_button()) {
        return false;
      }
      return remapping.button->get_vkey() != ui::VKEY_TAB;
    case mojom::CustomizationRestriction::kAllowFKeyRewrites:
      if (remapping.button->is_customizable_button()) {
        return false;
      }
      return !(remapping.button->get_vkey() >= ui::VKEY_F1 &&
               remapping.button->get_vkey() <= ui::VKEY_F15);
  }
}

// "0111:185a" is from the list of supported device keys listed here:
// google3/chrome/chromeos/apps_foundation/almanac/fondue/boq/
// peripherals_service/manual_config/companion_apps.h
constexpr char kWelcomeExperienceTestDeviceKey[] = "0111:185a";

}  // namespace

bool VendorProductId::operator==(const VendorProductId& other) const {
  return vendor_id == other.vendor_id && product_id == other.product_id;
}

// `kIsoLevel5ShiftMod3` is not a valid modifier value.
bool IsValidModifier(int val) {
  return val >= static_cast<int>(ui::mojom::ModifierKey::kMinValue) &&
         val <= static_cast<int>(ui::mojom::ModifierKey::kMaxValue) &&
         val != static_cast<int>(ui::mojom::ModifierKey::kIsoLevel5ShiftMod3);
}

std::string BuildDeviceKey(const ui::InputDevice& device) {
  return BuildDeviceKey(device.vendor_id, device.product_id);
}

std::string BuildDeviceKey(uint16_t vendor_id, uint16_t product_id) {
  return base::StrCat({HexEncode(vendor_id), ":", HexEncode(product_id)});
}

template <typename T>
bool ShouldPersistSetting(std::string_view setting_key,
                          T new_value,
                          T default_value,
                          bool force_persistence,
                          const base::Value::Dict* existing_settings_dict) {
  return ExistingSettingsHasValue(setting_key, existing_settings_dict) ||
         new_value != default_value || force_persistence;
}

bool ShouldPersistSetting(const mojom::InputDeviceSettingsPolicyPtr& policy,
                          std::string_view setting_key,
                          bool new_value,
                          bool default_value,
                          bool force_persistence,
                          const base::Value::Dict* existing_settings_dict) {
  if (force_persistence) {
    return true;
  }

  if (!policy) {
    return ShouldPersistSetting(setting_key, new_value, default_value,
                                force_persistence, existing_settings_dict);
  }

  switch (policy->policy_status) {
    case mojom::PolicyStatus::kRecommended:
      return ExistingSettingsHasValue(setting_key, existing_settings_dict) ||
             new_value != policy->value;
    case mojom::PolicyStatus::kManaged:
      return false;
  }
}

bool ShouldPersistFkeySetting(
    const mojom::InputDeviceSettingsFkeyPolicyPtr& policy,
    std::string_view setting_key,
    std::optional<ui::mojom::ExtendedFkeysModifier> new_value,
    ui::mojom::ExtendedFkeysModifier default_value,
    const base::Value::Dict* existing_settings_dict) {
  if (!new_value.has_value()) {
    return false;
  }

  if (!policy) {
    return ShouldPersistSetting(setting_key, new_value.value(), default_value,
                                /*force_persistence=*/{},
                                existing_settings_dict);
  }

  switch (policy->policy_status) {
    case mojom::PolicyStatus::kRecommended:
      return ExistingSettingsHasValue(setting_key, existing_settings_dict) ||
             new_value.value() != policy->value;
    case mojom::PolicyStatus::kManaged:
      return false;
  }
}

template EXPORT_TEMPLATE_DEFINE(ASH_EXPORT) bool ShouldPersistSetting(
    std::string_view setting_key,
    bool new_value,
    bool default_value,
    bool force_persistence,
    const base::Value::Dict* existing_settings_dict);

template EXPORT_TEMPLATE_DEFINE(ASH_EXPORT) bool ShouldPersistSetting(
    std::string_view setting_key,
    int value,
    int default_value,
    bool force_persistence,
    const base::Value::Dict* existing_settings_dict);

const base::Value::Dict* GetLoginScreenSettingsDict(
    PrefService* local_state,
    AccountId account_id,
    const std::string& pref_name) {
  const auto* dict_value =
      user_manager::KnownUser(local_state).FindPath(account_id, pref_name);
  if (!dict_value || !dict_value->is_dict()) {
    return nullptr;
  }
  return &dict_value->GetDict();
}

const base::Value::List* GetLoginScreenButtonRemappingList(
    PrefService* local_state,
    AccountId account_id,
    const std::string& pref_name) {
  const auto* list_value =
      user_manager::KnownUser(local_state).FindPath(account_id, pref_name);
  if (!list_value || !list_value->is_list()) {
    return nullptr;
  }
  return &list_value->GetList();
}

base::Value::Dict ConvertButtonRemappingToDict(
    const mojom::ButtonRemapping& remapping,
    mojom::CustomizationRestriction customization_restriction,
    bool redact_button_names) {
  base::Value::Dict dict;

  if (RestrictionBlocksRemapping(remapping, customization_restriction)) {
    return dict;
  }

  dict.Set(prefs::kButtonRemappingName,
           redact_button_names ? kRedactedButtonName : remapping.name);
  if (remapping.button->is_customizable_button()) {
    dict.Set(prefs::kButtonRemappingCustomizableButton,
             static_cast<int>(remapping.button->get_customizable_button()));
  } else if (remapping.button->is_vkey()) {
    dict.Set(prefs::kButtonRemappingKeyboardCode,
             static_cast<int>(remapping.button->get_vkey()));
  }

  if (!remapping.remapping_action) {
    return dict;
  }
  if (remapping.remapping_action->is_key_event()) {
    base::Value::Dict key_event;
    key_event.Set(prefs::kButtonRemappingDomCode,
                  static_cast<int>(
                      remapping.remapping_action->get_key_event()->dom_code));
    key_event.Set(
        prefs::kButtonRemappingDomKey,
        static_cast<int>(remapping.remapping_action->get_key_event()->dom_key));
    key_event.Set(prefs::kButtonRemappingModifiers,
                  static_cast<int>(
                      remapping.remapping_action->get_key_event()->modifiers));
    key_event.Set(
        prefs::kButtonRemappingKeyboardCode,
        static_cast<int>(remapping.remapping_action->get_key_event()->vkey));
    dict.Set(prefs::kButtonRemappingKeyEvent, std::move(key_event));
  } else if (remapping.remapping_action->is_accelerator_action()) {
    dict.Set(
        prefs::kButtonRemappingAcceleratorAction,
        static_cast<int>(remapping.remapping_action->get_accelerator_action()));
  } else if (remapping.remapping_action->is_static_shortcut_action()) {
    dict.Set(prefs::kButtonRemappingStaticShortcutAction,
             static_cast<int>(
                 remapping.remapping_action->get_static_shortcut_action()));
  }

  return dict;
}

base::Value::List ConvertButtonRemappingArrayToList(
    const std::vector<mojom::ButtonRemappingPtr>& remappings,
    mojom::CustomizationRestriction customization_restriction,
    bool redact_button_names) {
  base::Value::List list;
  for (const auto& remapping : remappings) {
    base::Value::Dict dict = ConvertButtonRemappingToDict(
        *remapping, customization_restriction, redact_button_names);
    // Remove empty dicts.
    if (dict.empty()) {
      continue;
    }

    list.Append(std::move(dict));
  }
  return list;
}

std::vector<mojom::ButtonRemappingPtr> ConvertListToButtonRemappingArray(
    const base::Value::List& list,
    mojom::CustomizationRestriction customization_restriction) {
  std::vector<mojom::ButtonRemappingPtr> array;
  for (const auto& element : list) {
    if (!element.is_dict()) {
      continue;
    }
    const auto& dict = element.GetDict();
    auto remapping =
        ConvertDictToButtonRemapping(dict, customization_restriction);
    if (remapping) {
      array.push_back(std::move(remapping));
    }
  }
  return array;
}

mojom::ButtonRemappingPtr ConvertDictToButtonRemapping(
    const base::Value::Dict& dict,
    mojom::CustomizationRestriction customization_restriction) {
  if (customization_restriction ==
      mojom::CustomizationRestriction::kDisallowCustomizations) {
    return nullptr;
  }

  const std::string* name = dict.FindString(prefs::kButtonRemappingName);
  if (!name) {
    return nullptr;
  }

  // button is a union.
  mojom::ButtonPtr button;
  const std::optional<int> customizable_button =
      dict.FindInt(prefs::kButtonRemappingCustomizableButton);
  const std::optional<int> key_code =
      dict.FindInt(prefs::kButtonRemappingKeyboardCode);
  // Button can't be both a keyboard key and a customization button.
  if (customizable_button && key_code) {
    return nullptr;
  }
  // Button must exist.
  if (!customizable_button && !key_code) {
    return nullptr;
  }
  // Button can be either a keyboard key or a customization button. If
  // the customization_restriction is not kDisableKeyEventRewrites,
  // the button is allowed to be a keyboard key.
  if (customizable_button) {
    button = mojom::Button::NewCustomizableButton(
        static_cast<mojom::CustomizableButton>(*customizable_button));
  } else if (key_code &&
             customization_restriction !=
                 mojom::CustomizationRestriction::kDisableKeyEventRewrites) {
    // Do not allow the keycode to be an unknown key. This indicates an internal
    // error in the implementation and should not be allowed.
    if (*key_code == ui::VKEY_UNKNOWN) {
      return nullptr;
    }

    button = mojom::Button::NewVkey(static_cast<::ui::KeyboardCode>(*key_code));
  } else {
    return nullptr;
  }

  // remapping_action is an optional union.
  mojom::RemappingActionPtr remapping_action;
  const base::Value::Dict* key_event =
      dict.FindDict(prefs::kButtonRemappingKeyEvent);
  const std::optional<int> accelerator_action =
      dict.FindInt(prefs::kButtonRemappingAcceleratorAction);
  const std::optional<int> static_shortcut_action =
      dict.FindInt(prefs::kButtonRemappingStaticShortcutAction);
  // Remapping action can only have one value at most.
  if ((key_event && accelerator_action) ||
      (key_event && static_shortcut_action) ||
      (accelerator_action && static_shortcut_action)) {
    return nullptr;
  }
  // Remapping action can be either a keyboard event or an accelerator action
  // or static shortcut action or null.
  if (key_event) {
    const std::optional<int> dom_code =
        key_event->FindInt(prefs::kButtonRemappingDomCode);
    const std::optional<int> vkey =
        key_event->FindInt(prefs::kButtonRemappingKeyboardCode);
    const std::optional<int> dom_key =
        key_event->FindInt(prefs::kButtonRemappingDomKey);
    const std::optional<int> modifiers =
        key_event->FindInt(prefs::kButtonRemappingModifiers);
    if (!dom_code || !vkey || !dom_key || !modifiers) {
      return nullptr;
    }
    ui::KeyboardCode vkey_value = static_cast<ui::KeyboardCode>(*vkey);
    remapping_action = mojom::RemappingAction::NewKeyEvent(
        mojom::KeyEvent::New(vkey_value, *dom_code, *dom_key, *modifiers,
                             base::UTF16ToUTF8(GetKeyDisplay(vkey_value))));
  } else if (accelerator_action) {
    remapping_action = mojom::RemappingAction::NewAcceleratorAction(
        static_cast<ash::AcceleratorAction>(*accelerator_action));
  } else if (static_shortcut_action) {
    remapping_action = mojom::RemappingAction::NewStaticShortcutAction(
        static_cast<mojom::StaticShortcutAction>(*static_shortcut_action));
  }

  return mojom::ButtonRemapping::New(*name, std::move(button),
                                     std::move(remapping_action));
}

bool IsChromeOSKeyboard(const mojom::Keyboard& keyboard) {
  return keyboard.meta_key == ui::mojom::MetaKey::kLauncher ||
         keyboard.meta_key == ui::mojom::MetaKey::kSearch;
}

bool IsSplitModifierKeyboard(const mojom::Keyboard& keyboard) {
  return keyboard.meta_key == ui::mojom::MetaKey::kLauncherRefresh;
}

bool IsSplitModifierKeyboard(int device_id) {
  return Shell::Get()->keyboard_capability()->HasFunctionKey(device_id) &&
         Shell::Get()->keyboard_capability()->HasRightAltKey(device_id);
}

std::string GetDeviceKeyForMetadataRequest(const std::string& device_key) {
  if (features::IsWelcomeExperienceTestUnsupportedDevicesEnabled()) {
    return kWelcomeExperienceTestDeviceKey;
  }

  return device_key;
}

}  // namespace ash
