// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/accelerator_alias_converter.h"

#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "ash/system/input_device_settings/input_device_settings_controller_impl.h"
#include "base/containers/fixed_flat_map.h"
#include "base/containers/fixed_flat_set.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/ash/keyboard_capability.h"
#include "ui/events/ash/keyboard_layout_util.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/devices/keyboard_device.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

namespace ash {

namespace {

using DeviceType = ui::KeyboardCapability::DeviceType;

bool IsChromeOSKeyboard(const ui::KeyboardDevice& keyboard) {
  const auto device_type =
      Shell::Get()->keyboard_capability()->GetDeviceType(keyboard);
  return device_type == DeviceType::kDeviceInternalKeyboard ||
         device_type == DeviceType::kDeviceExternalChromeOsKeyboard;
}

// Gets the most recently plugged in external keyboard. If there are no external
// keyboards, return the internal keyboard.
absl::optional<ui::KeyboardDevice> GetPriorityExternalKeyboard() {
  absl::optional<ui::KeyboardDevice> priority_keyboard;
  for (const ui::KeyboardDevice& keyboard :
       ui::DeviceDataManager::GetInstance()->GetKeyboardDevices()) {
    // If the input device settings controlled does not recognize the device as
    // a keyboard, skip it.
    if (features::IsInputDeviceSettingsSplitEnabled() &&
        Shell::Get()->input_device_settings_controller()->GetKeyboardSettings(
            keyboard.id) == nullptr) {
      continue;
    }

    const auto device_type =
        Shell::Get()->keyboard_capability()->GetDeviceType(keyboard);
    switch (device_type) {
      case DeviceType::kDeviceUnknown:
      case DeviceType::kDeviceInternalKeyboard:
      case DeviceType::kDeviceInternalRevenKeyboard:
        break;
      case DeviceType::kDeviceExternalChromeOsKeyboard:
      case DeviceType::kDeviceExternalAppleKeyboard:
      case DeviceType::kDeviceExternalGenericKeyboard:
      case DeviceType::kDeviceExternalUnknown:
      case DeviceType::kDeviceHotrodRemote:
      case DeviceType::kDeviceVirtualCoreKeyboard:
        if (!priority_keyboard || keyboard.id > priority_keyboard->id) {
          priority_keyboard = keyboard;
        }
        break;
    }
  }
  return priority_keyboard;
}

absl::optional<ui::KeyboardDevice> GetInternalKeyboard() {
  for (const ui::KeyboardDevice& keyboard :
       ui::DeviceDataManager::GetInstance()->GetKeyboardDevices()) {
    // If the input device settings controlled does not recognize the device as
    // a keyboard, skip it.
    if (features::IsInputDeviceSettingsSplitEnabled() &&
        Shell::Get()->input_device_settings_controller()->GetKeyboardSettings(
            keyboard.id) == nullptr) {
      continue;
    }

    const auto device_type =
        Shell::Get()->keyboard_capability()->GetDeviceType(keyboard);
    switch (device_type) {
      case DeviceType::kDeviceUnknown:
      case DeviceType::kDeviceInternalKeyboard:
      case DeviceType::kDeviceInternalRevenKeyboard:
        return keyboard;
      case DeviceType::kDeviceExternalChromeOsKeyboard:
      case DeviceType::kDeviceExternalAppleKeyboard:
      case DeviceType::kDeviceExternalGenericKeyboard:
      case DeviceType::kDeviceExternalUnknown:
      case DeviceType::kDeviceHotrodRemote:
      case DeviceType::kDeviceVirtualCoreKeyboard:
        break;
    }
  }
  return absl::nullopt;
}

// Identifies media keys which exist only on external keyboards.
bool IsMediaKey(ui::KeyboardCode key_code) {
  static constexpr auto kMediaKeyCodes =
      base::MakeFixedFlatSet<ui::KeyboardCode>(
          {ui::VKEY_MEDIA_PAUSE, ui::VKEY_MEDIA_PLAY,
           ui::VKEY_OEM_103,  // Media Rewind
           ui::VKEY_OEM_104,  // Media Fast Forward
           ui::VKEY_MEDIA_STOP});
  return kMediaKeyCodes.contains(key_code);
}

// Some `TopRowActionKey` values must always be shown if there is an external
// keyboard even if the top row of the keyboard does not technically support
// them. This is because many of these keys are common on external keyboards and
// are unable to be deduced properly.
bool ShouldAlwaysShowWithExternalKeyboard(ui::TopRowActionKey action_key) {
  switch (action_key) {
    case ui::TopRowActionKey::kNone:
    case ui::TopRowActionKey::kUnknown:
    case ui::TopRowActionKey::kBack:
    case ui::TopRowActionKey::kForward:
    case ui::TopRowActionKey::kRefresh:
    case ui::TopRowActionKey::kKeyboardBacklightToggle:
    case ui::TopRowActionKey::kKeyboardBacklightDown:
    case ui::TopRowActionKey::kKeyboardBacklightUp:
    case ui::TopRowActionKey::kAllApplications:
    case ui::TopRowActionKey::kDictation:
      return false;
    case ui::TopRowActionKey::kFullscreen:
    case ui::TopRowActionKey::kOverview:
    case ui::TopRowActionKey::kScreenBrightnessDown:
    case ui::TopRowActionKey::kScreenBrightnessUp:
    case ui::TopRowActionKey::kMicrophoneMute:
    case ui::TopRowActionKey::kVolumeMute:
    case ui::TopRowActionKey::kVolumeDown:
    case ui::TopRowActionKey::kVolumeUp:
    case ui::TopRowActionKey::kNextTrack:
    case ui::TopRowActionKey::kPreviousTrack:
    case ui::TopRowActionKey::kPlayPause:
    case ui::TopRowActionKey::kScreenshot:
    case ui::TopRowActionKey::kEmojiPicker:
      return true;
  }
}

bool MetaFKeyRewritesAreSuppressed(const ui::InputDevice& keyboard) {
  if (!features::IsInputDeviceSettingsSplitEnabled()) {
    return false;
  }

  const auto* settings =
      Shell::Get()->input_device_settings_controller()->GetKeyboardSettings(
          keyboard.id);
  if (!settings) {
    return false;
  }
  return settings->suppress_meta_fkey_rewrites;
}

bool ShouldShowExternalTopRowActionKeyAlias(
    const ui::KeyboardDevice& keyboard,
    ui::TopRowActionKey action_key,
    const ui::Accelerator& accelerator) {
  const bool should_show_action_key =
      Shell::Get()->keyboard_capability()->HasTopRowActionKey(keyboard,
                                                              action_key) ||
      ShouldAlwaysShowWithExternalKeyboard(action_key);

  // If Meta + F-Key rewrites are suppressed for the priority keyboard and
  // the accelerator contains the search key, we should not show the
  // accelerator.
  const bool alias_is_suppressed =
      accelerator.IsCmdDown() && MetaFKeyRewritesAreSuppressed(keyboard);
  return should_show_action_key && !alias_is_suppressed;
}

}  // namespace

std::vector<ui::Accelerator> AcceleratorAliasConverter::CreateAcceleratorAlias(
    const ui::Accelerator& accelerator) const {
  absl::optional<ui::KeyboardDevice> priority_external_keyboard =
      GetPriorityExternalKeyboard();
  absl::optional<ui::KeyboardDevice> internal_keyboard = GetInternalKeyboard();

  // If the external and internal keyboards are either both non-chromeos
  // keyboards (ex ChromeOS flex devices) or if they are both ChromeOS keyboards
  // (ex ChromeOS external keyboard), do not show aliases for the internal
  // keyboard.
  if (priority_external_keyboard && internal_keyboard &&
      (IsChromeOSKeyboard(*priority_external_keyboard) ==
       IsChromeOSKeyboard(*internal_keyboard))) {
    internal_keyboard = absl::nullopt;
  }

  // Set is used to get rid of possible duplicate accelerators.
  base::flat_set<ui::Accelerator> aliases_set;

  // Generate aliases for both the priority external keyboard + the internal
  // keyboard for top row action keys.
  if (priority_external_keyboard) {
    if (const auto alias =
            CreateTopRowAliases(*priority_external_keyboard, accelerator);
        alias) {
      aliases_set.insert(*alias);
    }
  }
  if (internal_keyboard) {
    if (const auto alias = CreateTopRowAliases(*internal_keyboard, accelerator);
        alias) {
      aliases_set.insert(*alias);
    }
  }
  if (!aliases_set.empty()) {
    return FilterAliasBySupportedKeys(std::move(aliases_set).extract());
  }

  // Generate aliases for both the priority external keyboard + the internal
  // keyboard for f-keys.
  if (priority_external_keyboard) {
    if (const auto alias =
            CreateFunctionKeyAliases(*priority_external_keyboard, accelerator);
        alias) {
      aliases_set.insert(*alias);
    }
  }
  if (internal_keyboard) {
    if (const auto alias =
            CreateFunctionKeyAliases(*internal_keyboard, accelerator);
        alias) {
      aliases_set.insert(*alias);
    }
  }
  if (!aliases_set.empty()) {
    return FilterAliasBySupportedKeys(std::move(aliases_set).extract());
  }

  // For |six_pack_key| and |reversed_six_pack_key|, show both the base
  // accelerator and the remapped accelerator if applicable. Otherwise, only
  // show base accelerator.
  std::vector<ui::Accelerator> aliases = CreateSixPackAliases(accelerator);
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

absl::optional<ui::Accelerator>
AcceleratorAliasConverter::CreateFunctionKeyAliases(
    const ui::KeyboardDevice& keyboard,
    const ui::Accelerator& accelerator) const {
  // Avoid remapping if [Search] is part of the original accelerator.
  if (accelerator.IsCmdDown()) {
    return {};
  }

  // Only attempt to alias if the provided accelerator is for an F-Key.
  if (accelerator.key_code() < ui::VKEY_F1 ||
      accelerator.key_code() > ui::VKEY_F24) {
    return {};
  }

  const bool top_row_are_fkeys = [&]() -> bool {
    if (features::IsInputDeviceSettingsSplitEnabled()) {
      const auto* settings =
          Shell::Get()->input_device_settings_controller()->GetKeyboardSettings(
              keyboard.id);
      return settings && settings->top_row_are_fkeys;
    }
    return Shell::Get()->keyboard_capability()->TopRowKeysAreFKeys();
  }();

  // Attempt to get the corresponding `ui::TopRowActionKey` for the given F-Key.
  absl::optional<ui::TopRowActionKey> action_key =
      Shell::Get()->keyboard_capability()->GetCorrespondingActionKeyForFKey(
          keyboard, accelerator.key_code());
  if (!action_key) {
    return {};
  }

  // Convert the `ui::TopRowActionKey` to the corresponding `ui::KeyboardCode`
  absl::optional<ui::KeyboardCode> action_vkey =
      ui::KeyboardCapability::ConvertToKeyboardCode(*action_key);
  if (!action_vkey) {
    return {};
  }

  if (IsChromeOSKeyboard(keyboard)) {
    // If `priority_keyboard` is a ChromeOS keyboard, the UI should show the
    // corresponding action key, the the F-Key glyph.
    if (top_row_are_fkeys) {
      return {ui::Accelerator(*action_vkey, accelerator.modifiers(),
                              accelerator.key_state())};
    } else {
      return {ui::Accelerator(*action_vkey,
                              accelerator.modifiers() | ui::EF_COMMAND_DOWN,
                              accelerator.key_state())};
    }
  } else {
    // On a non-chromeos keyboard, UI should show the F-Key instead.
    if (top_row_are_fkeys) {
      return {accelerator};
    } else {
      return {ui::Accelerator(accelerator.key_code(),
                              accelerator.modifiers() | ui::EF_COMMAND_DOWN,
                              accelerator.key_state())};
    }
  }
}

absl::optional<ui::Accelerator> AcceleratorAliasConverter::CreateTopRowAliases(
    const ui::KeyboardDevice& keyboard,
    const ui::Accelerator& accelerator) const {
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

  const bool top_row_are_fkeys = [&]() -> bool {
    if (features::IsInputDeviceSettingsSplitEnabled()) {
      const auto* settings =
          Shell::Get()->input_device_settings_controller()->GetKeyboardSettings(
              keyboard.id);
      return settings && settings->top_row_are_fkeys;
    }
    return Shell::Get()->keyboard_capability()->TopRowKeysAreFKeys();
  }();
  absl::optional<ui::KeyboardCode> function_key =
      Shell::Get()->keyboard_capability()->GetCorrespondingFunctionKey(
          keyboard, *action_key);
  if (!function_key.has_value()) {
    return {};
  }

  if (IsChromeOSKeyboard(keyboard)) {
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
  auto priority_keyboard = GetPriorityExternalKeyboard();
  auto internal_keyboard = GetInternalKeyboard();

  // If the external and internal keyboards are either both non-chromeos
  // keyboards (ex ChromeOS flex devices) or if they are both ChromeOS keyboards
  // (ex ChromeOS external keyboard), do not show aliases for the internal
  // keyboard.
  if (priority_keyboard && internal_keyboard &&
      (IsChromeOSKeyboard(*priority_keyboard) ==
       IsChromeOSKeyboard(*internal_keyboard))) {
    internal_keyboard = absl::nullopt;
  }

  for (const auto& accelerator : accelerators) {
    if (auto action_key = ui::KeyboardCapability::ConvertToTopRowActionKey(
            accelerator.key_code());
        action_key.has_value()) {
      // Accelerator should only be seen if the priority or internal keyboard
      // have the key OR if there is an external keyboard and the `action_key`
      // should always been shown when there is an external keyboard.
      if (priority_keyboard &&
          (ShouldShowExternalTopRowActionKeyAlias(*priority_keyboard,
                                                  *action_key, accelerator))) {
        filtered_accelerators.push_back(accelerator);
      } else if (internal_keyboard && keyboard_capability->HasTopRowActionKey(
                                          *internal_keyboard, *action_key)) {
        filtered_accelerators.push_back(accelerator);
      }
      continue;
    }

    // If the accelerator is for an FKey + Search, make sure it is only shown if
    // Meta + F-Key rewrites are allowed.
    if (accelerator.key_code() > ui::VKEY_F1 &&
        accelerator.key_code() < ui::VKEY_F24 && accelerator.IsCmdDown()) {
      if (priority_keyboard &&
          !MetaFKeyRewritesAreSuppressed(*priority_keyboard)) {
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
      if (priority_keyboard &&
          keyboard_capability->HasAssistantKey(*priority_keyboard)) {
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

    if (accelerator.key_code() == ui::VKEY_BROWSER_SEARCH) {
      if (keyboard_capability->HasBrowserSearchKeyOnAnyKeyboard()) {
        filtered_accelerators.push_back(accelerator);
      }
      continue;
    }

    if (IsMediaKey(accelerator.key_code())) {
      if (keyboard_capability->HasMediaKeysOnAnyKeyboard()) {
        filtered_accelerators.push_back(accelerator);
      }
      continue;
    }

    if (accelerator.key_code() == ui::VKEY_SETTINGS) {
      if (keyboard_capability->HasSettingsKeyOnAnyKeyboard()) {
        filtered_accelerators.push_back(accelerator);
      }
      continue;
    }

    if (accelerator.key_code() == ui::VKEY_HELP) {
      if (keyboard_capability->HasHelpKeyOnAnyKeyboard()) {
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

    // VKEY_PLAY/PAUSE should not be shown as they are conceptual duplicates of
    // VKEY_MEDIA_PLAY/VKEY_MEDIA_PAUSE.
    if (accelerator.key_code() == ui::VKEY_PLAY ||
        accelerator.key_code() == ui::VKEY_PAUSE) {
      continue;
    }

    // Otherwise, always copy the accelerator.
    filtered_accelerators.push_back(accelerator);
  }

  return filtered_accelerators;
}

}  // namespace ash
