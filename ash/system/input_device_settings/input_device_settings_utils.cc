// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/input_device_settings_utils.h"

#include "ash/public/mojom/input_device_settings.mojom.h"
#include "ash/system/input_device_settings/input_device_settings_pref_names.h"
#include "base/containers/fixed_flat_set.h"
#include "base/containers/flat_set.h"
#include "base/export_template.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/known_user.h"
#include "ui/events/ash/mojom/modifier_key.mojom.h"
#include "ui/events/ozone/evdev/keyboard_mouse_combo_device_metrics.h"

namespace ash {

namespace {

struct VendorProductId {
  uint16_t vendor_id;
  uint16_t product_id;
  constexpr bool operator<(const VendorProductId& other) const {
    return vendor_id == other.vendor_id ? product_id < other.product_id
                                        : vendor_id < other.vendor_id;
  }
};

std::string HexEncode(uint16_t v) {
  // Load the bytes into the bytes array in reverse order as hex number should
  // be read from left to right.
  uint8_t bytes[sizeof(uint16_t)];
  bytes[1] = v & 0xFF;
  bytes[0] = v >> 8;
  return base::ToLowerASCII(base::HexEncode(bytes));
}

bool ExistingSettingsHasValue(base::StringPiece setting_key,
                              const base::Value::Dict* existing_settings_dict) {
  if (!existing_settings_dict) {
    return false;
  }

  return existing_settings_dict->Find(setting_key) != nullptr;
}

}  // namespace

// `kIsoLevel5ShiftMod3` is not a valid modifier value.
bool IsValidModifier(int val) {
  return val >= static_cast<int>(ui::mojom::ModifierKey::kMinValue) &&
         val <= static_cast<int>(ui::mojom::ModifierKey::kMaxValue) &&
         val != static_cast<int>(ui::mojom::ModifierKey::kIsoLevel5ShiftMod3);
}

std::string BuildDeviceKey(const ui::InputDevice& device) {
  return base::StrCat(
      {HexEncode(device.vendor_id), ":", HexEncode(device.product_id)});
}

template <typename T>
bool ShouldPersistSetting(base::StringPiece setting_key,
                          T new_value,
                          T default_value,
                          bool force_persistence,
                          const base::Value::Dict* existing_settings_dict) {
  return ExistingSettingsHasValue(setting_key, existing_settings_dict) ||
         new_value != default_value || force_persistence;
}

bool ShouldPersistSetting(const mojom::InputDeviceSettingsPolicyPtr& policy,
                          base::StringPiece setting_key,
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

template EXPORT_TEMPLATE_DEFINE(ASH_EXPORT) bool ShouldPersistSetting(
    base::StringPiece setting_key,
    bool new_value,
    bool default_value,
    bool force_persistence,
    const base::Value::Dict* existing_settings_dict);

template EXPORT_TEMPLATE_DEFINE(ASH_EXPORT) bool ShouldPersistSetting(
    base::StringPiece setting_key,
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

bool IsKeyboardPretendingToBeMouse(const ui::InputDevice& device) {
  static base::NoDestructor<base::flat_set<VendorProductId>> logged_devices;
  static constexpr auto kKeyboardsPretendingToBeMice =
      base::MakeFixedFlatSet<VendorProductId>({
          {0x29ea, 0x0102},  // Kinesis Freestyle Edge RGB
          {0x046d, 0xc343},  // Logitech G915 TKL (USB)
          {0x046d, 0xb35f},  // Logitech G915 TKL (Bluetooth)
          {0x046d, 0x408a},  // Logitech MX Keys (Universal Receiver)
          {0x046d, 0xb35b},  // Logitech MX Keys (Bluetooth)
          {0x0951, 0x16e6},  // HyperX Alloy Origins Core
          {0x1532, 0x025e},  // Razer Cynosa V2
          {0x05ac, 0x0256},  // EGA MGK2 (USB)
          {0x05ac, 0x024f},  // EGA MGK2 (Bluetooth)
      });

  if (kKeyboardsPretendingToBeMice.contains(
          {device.vendor_id, device.product_id})) {
    auto [iter, inserted] =
        logged_devices->insert({device.vendor_id, device.product_id});
    if (inserted) {
      logged_devices->insert({device.vendor_id, device.product_id});
      base::UmaHistogramEnumeration(
          "ChromeOS.Inputs.ComboDeviceClassification",
          ui::ComboDeviceClassification::kKnownMouseImposter);
    }
    return true;
  }

  return false;
}

base::Value::Dict ConvertButtonRemappingToDict(
    const mojom::ButtonRemapping& remapping) {
  base::Value::Dict dict;

  dict.Set(prefs::kButtonRemappingName, remapping.name);
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
  } else if (remapping.remapping_action->is_action()) {
    dict.Set(prefs::kButtonRemappingAction,
             static_cast<int>(remapping.remapping_action->get_action()));
  }

  return dict;
}

base::Value::List ConvertButtonRemappingArrayToList(
    const std::vector<mojom::ButtonRemappingPtr>& remappings) {
  base::Value::List list;
  for (const auto& remapping : remappings) {
    base::Value::Dict dict = ConvertButtonRemappingToDict(*remapping);
    list.Append(std::move(dict));
  }
  return list;
}

std::vector<mojom::ButtonRemappingPtr> ConvertListToButtonRemappingArray(
    const base::Value::List& list) {
  std::vector<mojom::ButtonRemappingPtr> array;
  for (const auto& element : list) {
    if (!element.is_dict()) {
      continue;
    }
    const auto& dict = element.GetDict();
    auto remapping = ConvertDictToButtonRemapping(dict);
    if (remapping) {
      array.push_back(std::move(remapping));
    }
  }
  return array;
}

mojom::ButtonRemappingPtr ConvertDictToButtonRemapping(
    const base::Value::Dict& dict) {
  const std::string* name = dict.FindString(prefs::kButtonRemappingName);
  if (!name) {
    return nullptr;
  }

  // button is a union.
  mojom::ButtonPtr button;
  const absl::optional<int> customizable_button =
      dict.FindInt(prefs::kButtonRemappingCustomizableButton);
  const absl::optional<int> key_code =
      dict.FindInt(prefs::kButtonRemappingKeyboardCode);
  // Button can't be both a keyboard key and a customization button.
  if (customizable_button && key_code) {
    return nullptr;
  }
  // Button must exist.
  if (!customizable_button && !key_code) {
    return nullptr;
  }
  // Button can be either a keyboard key or a customization button.
  if (customizable_button) {
    button = mojom::Button::NewCustomizableButton(
        static_cast<mojom::CustomizableButton>(*customizable_button));
  } else {
    button = mojom::Button::NewVkey(static_cast<::ui::KeyboardCode>(*key_code));
  }

  // remapping_action is an optional union.
  mojom::RemappingActionPtr remapping_action;
  const base::Value::Dict* key_event =
      dict.FindDict(prefs::kButtonRemappingKeyEvent);
  const absl::optional<int> action =
      dict.FindInt(prefs::kButtonRemappingAction);
  // Remapping action can't be both a key event and an action.
  if (key_event && action) {
    return nullptr;
  }
  // Remapping action can be either a keyboard event or an action or null.
  if (key_event) {
    const absl::optional<int> dom_code =
        key_event->FindInt(prefs::kButtonRemappingDomCode);
    const absl::optional<int> vkey =
        key_event->FindInt(prefs::kButtonRemappingKeyboardCode);
    const absl::optional<int> dom_key =
        key_event->FindInt(prefs::kButtonRemappingDomKey);
    const absl::optional<int> modifiers =
        key_event->FindInt(prefs::kButtonRemappingModifiers);
    if (!dom_code || !vkey || !dom_key || !modifiers) {
      return nullptr;
    }
    remapping_action = mojom::RemappingAction::NewKeyEvent(
        mojom::KeyEvent::New(static_cast<::ui::KeyboardCode>(
                                 /*vkey=*/*vkey),
                             /*dom_code=*/*dom_code,
                             /*dom_key=*/*dom_key,
                             /*modifiers=*/*modifiers));
  } else if (action) {
    remapping_action = mojom::RemappingAction::NewAction(
        static_cast<ash::AcceleratorAction>(*action));
  }

  return mojom::ButtonRemapping::New(*name, std::move(button),
                                     std::move(remapping_action));
}

}  // namespace ash
