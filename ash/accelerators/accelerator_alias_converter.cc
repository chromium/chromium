// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/accelerator_alias_converter.h"

#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "ash/system/input_device_settings/input_device_settings_controller_impl.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/ash/keyboard_capability.h"
#include "ui/events/ash/keyboard_layout_util.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

namespace ash {

namespace {

using DeviceType = ui::KeyboardCapability::DeviceType;

bool IsChromeOSKeyboard(const ui::InputDevice& keyboard) {
  const auto device_type =
      Shell::Get()->keyboard_capability()->GetDeviceType(keyboard);
  return device_type == DeviceType::kDeviceInternalKeyboard ||
         device_type == DeviceType::kDeviceExternalChromeOsKeyboard;
}

// Gets the most recently plugged in external keyboard. If there are no external
// keyboards, return the internal keyboard.
absl::optional<ui::InputDevice> GetPriorityKeyboard() {
  DeviceType priority_device_type = DeviceType::kDeviceUnknown;
  absl::optional<ui::InputDevice> priority_keyboard;
  for (const ui::InputDevice& keyboard :
       ui::DeviceDataManager::GetInstance()->GetKeyboardDevices()) {
    const auto device_type =
        Shell::Get()->keyboard_capability()->GetDeviceType(keyboard);
    switch (device_type) {
      case DeviceType::kDeviceUnknown:
      case DeviceType::kDeviceInternalKeyboard:
        if (!priority_keyboard) {
          priority_keyboard = keyboard;
          priority_device_type = DeviceType::kDeviceInternalKeyboard;
        }
        break;
      case DeviceType::kDeviceExternalChromeOsKeyboard:
      case DeviceType::kDeviceExternalAppleKeyboard:
      case DeviceType::kDeviceExternalGenericKeyboard:
      case DeviceType::kDeviceExternalUnknown:
      case DeviceType::kDeviceHotrodRemote:
      case DeviceType::kDeviceVirtualCoreKeyboard:
        if (!priority_keyboard ||
            priority_device_type == DeviceType::kDeviceInternalKeyboard ||
            keyboard.id > priority_keyboard->id) {
          priority_keyboard = keyboard;
          priority_device_type = device_type;
        }
        break;
    }
  }
  return priority_keyboard;
}

}  // namespace

// TODO(zhangwenyu): Handle cases when an accelerator should be suppressed
// because certain keys are unavailable.
std::vector<ui::Accelerator> AcceleratorAliasConverter::CreateAcceleratorAlias(
    const ui::Accelerator& accelerator) const {
  // For |top_row_key|, replace the base accelerator with top-row remapped
  // accelerator if applicable. Otherwise, only show base accelerator.
  std::vector<ui::Accelerator> aliases = CreateTopRowAliases(accelerator);
  // Return early here since an accelerator can never have a top row alias and a
  // six pack alias at the same time. Because top row keys and six pack keys are
  // two completely different sets of keys.
  if (!aliases.empty()) {
    return FilterAliasBySupportedKeys(aliases);
  }

  // For |six_pack_key| and |reversed_six_pack_key|, show both the base
  // accelerator and the remapped accelerator if applicable. Otherwise, only
  // show base accelerator.
  aliases = CreateSixPackAliases(accelerator);
  std::vector<ui::Accelerator> reversed_aliases =
      CreateReversedSixPackAliases(accelerator);
  // An accelerator can never have both six pack alias and reversed six
  // pack alias at the same time. Concatenating two vectors works here. Note
  // that both vectors could be empty.
  aliases.insert(aliases.end(), reversed_aliases.begin(),
                 reversed_aliases.end());

  // Add base accelerator.
  aliases.push_back(accelerator);
  return FilterAliasBySupportedKeys(aliases);
}

std::vector<ui::Accelerator> AcceleratorAliasConverter::CreateTopRowAliases(
    const ui::Accelerator& accelerator) const {
  // TODO(zhangwenyu): Handle the case when meta + top row key rewrite is
  // suppressed, following https://crrev.com/c/4160339.
  // Avoid remapping if [Search] is part of the original accelerator.
  if (accelerator.IsCmdDown()) {
    return {};
  }

  // If the accelerator is not an action key, do no aliasing.
  absl::optional<ui::TopRowActionKey> action_key =
      ui::KeyboardCapability::ConvertToTopRowActionKey(accelerator.key_code());
  if (!action_key) {
    return {};
  }

  absl::optional<ui::InputDevice> priority_keyboard = GetPriorityKeyboard();
  if (!priority_keyboard.has_value()) {
    return {};
  }

  const bool top_row_are_fkeys = [&]() -> bool {
    if (features::IsInputDeviceSettingsSplitEnabled()) {
      const auto* settings =
          Shell::Get()->input_device_settings_controller()->GetKeyboardSettings(
              priority_keyboard->id);
      return settings && settings->top_row_are_fkeys;
    }
    return Shell::Get()->keyboard_capability()->TopRowKeysAreFKeys();
  }();
  absl::optional<ui::KeyboardCode> function_key =
      Shell::Get()->keyboard_capability()->GetCorrespondingFunctionKey(
          *priority_keyboard, *action_key);
  if (!function_key.has_value()) {
    return {};
  }

  if (IsChromeOSKeyboard(*priority_keyboard)) {
    // If its a ChromeOS Keyboard, the UI should show the Action Key glyph. If
    // `top_row_are_fkeys` is true, Search must be added so convert the "F-Key"
    // into the action key.
    if (top_row_are_fkeys) {
      return {ui::Accelerator(accelerator.key_code(),
                              accelerator.modifiers() | ui::EF_COMMAND_DOWN,
                              accelerator.key_state())};
    } else {
      // Otherwise if `top_row_are_fkeys` is false, the identity accelerator
      // should be returned.
      return {accelerator};
    }
  } else {
    // If its an external, the F-Key glyph should be shown. If
    // `top_row_are_fkeys` is true, search must be added to convert the "F-Key"
    // into the action key. Otherwise, the "F-Key" is implicitly the action key.
    if (top_row_are_fkeys) {
      return {ui::Accelerator(*function_key,
                              accelerator.modifiers() | ui::EF_COMMAND_DOWN,
                              accelerator.key_state())};
    } else {
      return {ui::Accelerator(*function_key, accelerator.modifiers(),
                              accelerator.key_state())};
    }
  }
}

std::vector<ui::Accelerator> AcceleratorAliasConverter::CreateSixPackAliases(
    const ui::Accelerator& accelerator) const {
  // For all |six_pack_keys|, avoid remapping if [Search] is part of the
  // original accelerator.
  if (accelerator.IsCmdDown() ||
      !::features::IsImprovedKeyboardShortcutsEnabled() ||
      !ui::KeyboardCapability::IsSixPackKey(accelerator.key_code())) {
    return std::vector<ui::Accelerator>();
  }

  // Edge cases:
  // 1. [Shift] + [Delete] should not be remapped to [Shift] + [Search] +
  // [Back] (aka, Insert).
  // 2. For [Insert], avoid remapping if [Shift] is part of original
  // accelerator.
  if (accelerator.IsShiftDown() &&
      (accelerator.key_code() == ui::KeyboardCode::VKEY_DELETE ||
       accelerator.key_code() == ui::KeyboardCode::VKEY_INSERT)) {
    return std::vector<ui::Accelerator>();
  }

  // For Insert: [modifiers] = [Search] + [Shift] + [original_modifiers].
  // For other |six_pack_keys|: [modifiers] = [Search] + [original_modifiers].
  int updated_modifiers =
      accelerator.key_code() == ui::KeyboardCode::VKEY_INSERT
          ? accelerator.modifiers() | ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN
          : accelerator.modifiers() | ui::EF_COMMAND_DOWN;
  return {
      ui::Accelerator(ui::kSixPackKeyToSystemKeyMap.at(accelerator.key_code()),
                      updated_modifiers, accelerator.key_state())};
}

std::vector<ui::Accelerator>
AcceleratorAliasConverter::CreateReversedSixPackAliases(
    const ui::Accelerator& accelerator) const {
  // To find the reversed six pack alias, an accelerator must include [Search]
  // key, and must be one of the reversed six pack keys. And the connected
  // keyboards must have six pack keys.
  if (!accelerator.IsCmdDown() ||
      !::features::IsImprovedKeyboardShortcutsEnabled() ||
      !ui::KeyboardCapability::IsReversedSixPackKey(accelerator.key_code()) ||
      !ui::KeyboardCapability::HasSixPackOnAnyKeyboard()) {
    return std::vector<ui::Accelerator>();
  }

  int modifiers = accelerator.modifiers() & ~ui::EF_COMMAND_DOWN;
  // [Back] maps back to [Insert] if modifier contains [Shift]. Otherwise,
  // it maps back to [Delete].
  if (accelerator.key_code() == ui::KeyboardCode::VKEY_BACK) {
    if (!accelerator.IsShiftDown()) {
      return {ui::Accelerator(ui::KeyboardCode::VKEY_DELETE, modifiers,
                              accelerator.key_state())};
    }

    modifiers &= ~ui::EF_SHIFT_DOWN;
    return {ui::Accelerator(ui::KeyboardCode::VKEY_INSERT, modifiers,
                            accelerator.key_state())};
  }

  // Handle modifiers other than [Back].
  return {ui::Accelerator(
      ui::kReversedSixPackKeyToSystemKeyMap.at(accelerator.key_code()),
      modifiers, accelerator.key_state())};
}

std::vector<ui::Accelerator>
AcceleratorAliasConverter::FilterAliasBySupportedKeys(
    const std::vector<ui::Accelerator>& accelerators) const {
  const auto* keyboard_capability = Shell::Get()->keyboard_capability();
  CHECK(keyboard_capability);

  std::vector<ui::Accelerator> filtered_accelerators;
  auto priority_keyboard = GetPriorityKeyboard();

  for (const auto& accelerator : accelerators) {
    if (auto action_key = ui::KeyboardCapability::ConvertToTopRowActionKey(
            accelerator.key_code());
        action_key.has_value()) {
      if (priority_keyboard && keyboard_capability->HasTopRowActionKey(
                                   *priority_keyboard, *action_key)) {
        filtered_accelerators.push_back(accelerator);
      }
      continue;
    }

    if (ui::KeyboardCapability::IsSixPackKey(accelerator.key_code())) {
      if (ui::KeyboardCapability::HasSixPackOnAnyKeyboard()) {
        filtered_accelerators.push_back(accelerator);
      }
      continue;
    }

    if (accelerator.key_code() == ui::VKEY_ASSISTANT) {
      if (ui::DeviceKeyboardHasAssistantKey()) {
        filtered_accelerators.push_back(accelerator);
      }
      continue;
    }

    if (accelerator.key_code() == ui::VKEY_MODECHANGE) {
      if (keyboard_capability->HasGlobeKeyOnAnyKeyboard()) {
        filtered_accelerators.push_back(accelerator);
      }
      continue;
    }

    // VKEY_MEDIA_LAUNCH_APP2 is the "Calculator" button on many external
    // keyboards.
    if (accelerator.key_code() == ui::VKEY_MEDIA_LAUNCH_APP2) {
      if (keyboard_capability->HasCalculatorKeyOnAnyKeyboard()) {
        filtered_accelerators.push_back(accelerator);
      }
      continue;
    }

    if (accelerator.key_code() == ui::VKEY_PRIVACY_SCREEN_TOGGLE) {
      if (keyboard_capability->HasPrivacyScreenKeyOnAnyKeyboard()) {
        filtered_accelerators.push_back(accelerator);
      }
      continue;
    }

    // If the accelerator is pressing Search + Alt to do capslock, only Alt +
    // Search should be shown in the shortcuts app.
    if (accelerator.key_code() == ui::VKEY_MENU &&
        accelerator.modifiers() == ui::EF_COMMAND_DOWN) {
      continue;
    }

    // Otherwise, always copy the accelerator.
    filtered_accelerators.push_back(accelerator);
  }

  return filtered_accelerators;
}

}  // namespace ash
