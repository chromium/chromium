// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/accelerator_alias_converter.h"

#include <optional>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/display/privacy_screen_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/input_device_settings/input_device_settings_controller_impl.h"
#include "ash/system/input_device_settings/input_device_settings_utils.h"
#include "base/containers/fixed_flat_map.h"
#include "base/containers/fixed_flat_set.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/notreached.h"
#include "chromeos/constants/devicetype.h"
#include "components/prefs/pref_service.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/ash/keyboard_capability.h"
#include "ui/events/ash/keyboard_layout_util.h"
#include "ui/events/ash/mojom/modifier_key.mojom-shared.h"
#include "ui/events/ash/mojom/six_pack_shortcut_modifier.mojom-shared.h"
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
std::optional<ui::KeyboardDevice> GetPriorityExternalKeyboard() {
  std::optional<ui::KeyboardDevice> priority_keyboard;
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
      case ui::KeyboardCapability::DeviceType::
          kDeviceExternalNullTopRowChromeOsKeyboard:
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

std::optional<ui::KeyboardDevice> GetInternalKeyboard() {
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
      case ui::KeyboardCapability::DeviceType::
          kDeviceExternalNullTopRowChromeOsKeyboard:
      case DeviceType::kDeviceExternalUnknown:
      case DeviceType::kDeviceHotrodRemote:
      case DeviceType::kDeviceVirtualCoreKeyboard:
        break;
    }
  }
  return std::nullopt;
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
    case ui::TopRowActionKey::kPrivacyScreenToggle:
    case ui::TopRowActionKey::kAllApplications:
    case ui::TopRowActionKey::kAccessibility:
      return false;
    case ui::TopRowActionKey::kDictation:
    case ui::TopRowActionKey::kFullscreen:
    case ui::TopRowActionKey::kOverview:
    case ui::TopRowActionKey::kScreenBrightnessDown:
    case ui::TopRowActionKey::kScreenBrightnessUp:
    case ui::TopRowActionKey::kKeyboardBacklightDown:
    case ui::TopRowActionKey::kKeyboardBacklightUp:
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
  return settings && settings->suppress_meta_fkey_rewrites;
}

bool AreTopRowFKeys(const ui::InputDevice& keyboard) {
  if (!features::IsInputDeviceSettingsSplitEnabled()) {
    PrefService* pref_service =
        Shell::Get()->session_controller()->GetActivePrefService();
    return pref_service && pref_service->GetBoolean(prefs::kSendFunctionKeys);
  }

  const auto* settings =
      Shell::Get()->input_device_settings_controller()->GetKeyboardSettings(
          keyboard.id);
  return settings && settings->top_row_are_fkeys;
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

ui::mojom::SixPackShortcutModifier GetSixPackShortcutModifier(
    ui::KeyboardCode key_code,
    std::optional<int> device_id) {
  if (!features::IsAltClickAndSixPackCustomizationEnabled() ||
      !device_id.has_value()) {
    return ui::mojom::SixPackShortcutModifier::kSearch;
  }
  CHECK(ui::KeyboardCapability::IsSixPackKey(key_code));

  const auto* settings =
      Shell::Get()->input_device_settings_controller()->GetKeyboardSettings(
          device_id.value());
  if (!settings) {
    return ui::mojom::SixPackShortcutModifier::kSearch;
  }

  switch (key_code) {
    case ui::VKEY_DELETE:
      return settings->six_pack_key_remappings->del;
    case ui::VKEY_INSERT:
      return settings->six_pack_key_remappings->insert;
    case ui::VKEY_HOME:
      return settings->six_pack_key_remappings->home;
    case ui::VKEY_END:
      return settings->six_pack_key_remappings->end;
    case ui::VKEY_PRIOR:
      return settings->six_pack_key_remappings->page_up;
    case ui::VKEY_NEXT:
      return settings->six_pack_key_remappings->page_down;
    default:
      NOTREACHED();
  }
}

ui::mojom::ExtendedFkeysModifier GetExtendedFkeysModifier(
    ui::KeyboardCode key_code,
    std::optional<int> device_id) {
  if (!features::IsInputDeviceSettingsSplitEnabled() ||
      !::features::AreF11AndF12ShortcutsEnabled() || !device_id.has_value() ||
      !ui::KeyboardCapability::IsF11OrF12(key_code)) {
    return ui::mojom::ExtendedFkeysModifier::kDisabled;
  }

  auto* controller = Shell::Get()->input_device_settings_controller();
  CHECK(controller);
  const auto* settings = controller->GetKeyboardSettings(device_id.value());

  // Settings are only supported for F11 and F12.
  if (!settings) {
    return ui::mojom::ExtendedFkeysModifier::kDisabled;
  }

  if (key_code == ui::VKEY_F11) {
    return settings->f11.value();
  }

  return settings->f12.value();
}

bool HasRightAltKeyViaModifierRemapping(const ui::KeyboardDevice& keyboard) {
  if (!features::IsInputDeviceSettingsSplitEnabled()) {
    return false;
  }

  auto* settings =
      Shell::Get()->input_device_settings_controller()->GetKeyboardSettings(
          keyboard.id);
  if (!settings) {
    return false;
  }

  bool has_right_alt_key = false;
  for (const auto& [_, to] : settings->modifier_remappings) {
    if (to == ui::mojom::ModifierKey::kRightAlt) {
      has_right_alt_key = true;
      break;
    }
  }
  return has_right_alt_key;
}

}  // namespace

std::vector<ui::Accelerator> AcceleratorAliasConverter::CreateAcceleratorAlias(
    const ui::Accelerator& accelerator) const {
  std::optional<ui::KeyboardDevice> priority_external_keyboard =
      GetPriorityExternalKeyboard();
  std::optional<ui::KeyboardDevice> internal_keyboard = GetInternalKeyboard();
  std::optional<int> device_id = std::nullopt;
  if (priority_external_keyboard.has_value()) {
    device_id = priority_external_keyboard->id;
  } else if (internal_keyboard.has_value()) {
    device_id = internal_keyboard->id;
  }

  // If the external and internal keyboards are either both non-chromeos
  // keyboards (ex ChromeOS flex devices) or if they are both ChromeOS keyboards
  // (ex ChromeOS external keyboard), do not show aliases for the internal
  // keyboard.
  if (priority_external_keyboard && internal_keyboard &&
      (IsChromeOSKeyboard(*priority_external_keyboard) ==
       IsChromeOSKeyboard(*internal_keyboard))) {
    internal_keyboard = std::nullopt;
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
      // Always add the original accelerator if an external keyboard is present.
      aliases_set.insert(accelerator);
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
  // keyboard for CapsLock key.
  if (priority_external_keyboard) {
    if (const auto alias =
            CreateCapsLockAliases(*priority_external_keyboard, accelerator);
        alias) {
      aliases_set.insert(*alias);
      // Always add the original accelerator if an external keyboard is present.
      aliases_set.insert(accelerator);
    }
  }
  if (internal_keyboard) {
    if (const auto alias =
            CreateCapsLockAliases(*internal_keyboard, accelerator);
        alias) {
      aliases_set.insert(*alias);
      // Always add the original accelerator if an internal keyboard is present.
      aliases_set.insert(accelerator);
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
    if (ui::KeyboardCapability::IsF11OrF12(accelerator.key_code()) &&
        Shell::Get()->keyboard_capability()->IsChromeOSKeyboard(
            internal_keyboard->id)) {
      if (const auto alias = CreateExtendedFKeysAliases(
              *internal_keyboard, accelerator, internal_keyboard->id);
          alias) {
        aliases_set.insert(*alias);
        // If there is an external keyboard connected, we show both versions of
        // the shortcut (base accelerator + alias).
        if (priority_external_keyboard) {
          aliases_set.insert(accelerator);
        }
      }
    }
  }
  if (!aliases_set.empty()) {
    return FilterAliasBySupportedKeys(std::move(aliases_set).extract());
  }

  // For |six_pack_key|, show both the base
  // accelerator and the remapped accelerator if applicable. Otherwise, only
  // show base accelerator.
  std::vector<ui::Accelerator> aliases =
      CreateSixPackAliases(accelerator, device_id);

  // Add base accelerator.
  aliases.push_back(accelerator);
  return FilterAliasBySupportedKeys(aliases);
}

std::optional<ui::Accelerator>
AcceleratorAliasConverter::CreateFunctionKeyAliases(
    const ui::KeyboardDevice& keyboard,
    const ui::Accelerator& accelerator) const {
  // Avoid remapping if [Search] is part of the original accelerator.
  if (accelerator.IsCmdDown()) {
    return std::nullopt;
  }

  // Only attempt to alias if the provided accelerator is for an F-Key.
  if (accelerator.key_code() < ui::VKEY_F1 ||
      accelerator.key_code() > ui::VKEY_F24) {
    return std::nullopt;
  }

  // Attempt to get the corresponding `ui::TopRowActionKey` for the given F-Key.
  std::optional<ui::TopRowActionKey> action_key =
      Shell::Get()->keyboard_capability()->GetCorrespondingActionKeyForFKey(
          keyboard, accelerator.key_code());
  if (!action_key) {
    return std::nullopt;
  }

  // Convert the `ui::TopRowActionKey` to the corresponding `ui::KeyboardCode`
  std::optional<ui::KeyboardCode> action_vkey =
      ui::KeyboardCapability::ConvertToKeyboardCode(*action_key);
  if (!action_vkey) {
    return std::nullopt;
  }

  const bool top_row_are_fkeys = AreTopRowFKeys(keyboard);
  if (IsSplitModifierKeyboard(keyboard.id)) {
    // If its a split modifier Keyboard, the UI should show the Action Key
    // glyph. If `top_row_are_fkeys` is false, function key must be added so
    // convert the "F-Key" into the action key.
    if (top_row_are_fkeys) {
      return {ui::Accelerator(*action_vkey, accelerator.modifiers(),
                              accelerator.key_state())};
    } else {
      return {ui::Accelerator(*action_vkey,
                              accelerator.modifiers() | ui::EF_FUNCTION_DOWN,
                              accelerator.key_state())};
    }
  } else if (IsChromeOSKeyboard(keyboard)) {
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

std::optional<ui::Accelerator>
AcceleratorAliasConverter::CreateExtendedFKeysAliases(
    const ui::KeyboardDevice& keyboard,
    const ui::Accelerator& accelerator,
    std::optional<int> device_id) const {
  if (!::features::AreF11AndF12ShortcutsEnabled() ||
      !ui::KeyboardCapability::IsF11OrF12(accelerator.key_code()) ||
      !device_id.has_value()) {
    return std::nullopt;
  }
  const ui::KeyboardCode accel_key_code = accelerator.key_code();
  const ui::mojom::ExtendedFkeysModifier fkey_modifier =
      GetExtendedFkeysModifier(accel_key_code, device_id);

  int modifiers;
  const bool top_row_are_fkeys = AreTopRowFKeys(keyboard);

  bool avoid_remapping = false;
  switch (fkey_modifier) {
    case ui::mojom::ExtendedFkeysModifier::kDisabled:
      avoid_remapping = true;
      break;
    case ui::mojom::ExtendedFkeysModifier::kAlt:
      avoid_remapping = accelerator.IsAltDown();
      modifiers = ui::EF_ALT_DOWN;
      break;
    case ui::mojom::ExtendedFkeysModifier::kShift:
      avoid_remapping = accelerator.IsShiftDown();
      modifiers = ui::EF_SHIFT_DOWN;
      break;
    case ui::mojom::ExtendedFkeysModifier::kCtrlShift:
      avoid_remapping = accelerator.IsShiftDown() || accelerator.IsCtrlDown();
      modifiers = (ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN);
      break;
  }
  if (avoid_remapping) {
    return std::nullopt;
  }

  ui::KeyboardCode key_code;
  const auto* top_row_keys =
      Shell::Get()->keyboard_capability()->GetTopRowActionKeys(keyboard);
  if (!top_row_keys) {
    return std::nullopt;
  }
  const int top_row_key_index = accel_key_code - ui::KeyboardCode::VKEY_F11;
  CHECK(0 <= top_row_key_index && top_row_key_index <= 1);
  key_code = Shell::Get()
                 ->keyboard_capability()
                 ->ConvertToKeyboardCode((*top_row_keys)[top_row_key_index])
                 .value();
  modifiers = accelerator.modifiers() |
              (top_row_are_fkeys ? modifiers : modifiers | ui::EF_COMMAND_DOWN);
  return ui::Accelerator(key_code, modifiers);
}

std::optional<ui::Accelerator> AcceleratorAliasConverter::CreateCapsLockAliases(
    const ui::KeyboardDevice& keyboard,
    const ui::Accelerator& accelerator) const {
  if (accelerator.key_code() != ui::VKEY_CAPITAL) {
    return std::nullopt;
  }

  if (Shell::Get()->keyboard_capability()->HasFunctionKey(keyboard)) {
    return {ui::Accelerator(ui::VKEY_RIGHT_ALT, ui::EF_FUNCTION_DOWN)};
  }

  return accelerator;
}

std::optional<ui::Accelerator> AcceleratorAliasConverter::CreateTopRowAliases(
    const ui::KeyboardDevice& keyboard,
    const ui::Accelerator& accelerator) const {
  // Avoid remapping if [Search] is part of the original accelerator.
  if (accelerator.IsCmdDown()) {
    return std::nullopt;
  }

  // If the accelerator is not an action key, do no aliasing.
  std::optional<ui::TopRowActionKey> action_key =
      ui::KeyboardCapability::ConvertToTopRowActionKey(accelerator.key_code());
  if (!action_key) {
    return std::nullopt;
  }

  std::optional<ui::KeyboardCode> function_key =
      Shell::Get()->keyboard_capability()->GetCorrespondingFunctionKey(
          keyboard, *action_key);
  if (!function_key.has_value()) {
    return std::nullopt;
  }

  const bool top_row_are_fkeys = AreTopRowFKeys(keyboard);
  if (IsSplitModifierKeyboard(keyboard.id)) {
    // If its a split modifier Keyboard, the UI should show the Action Key
    // glyph. If `top_row_are_fkeys` is true, function key must be added so
    // convert the "F-Key" into the action key.
    if (top_row_are_fkeys) {
      return {ui::Accelerator(accelerator.key_code(),
                              accelerator.modifiers() | ui::EF_FUNCTION_DOWN,
                              accelerator.key_state())};
    } else {
      // Otherwise if `top_row_are_fkeys` is false, the identity accelerator
      // should be returned.
      return {accelerator};
    }
  } else if (IsChromeOSKeyboard(keyboard)) {
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
    const ui::Accelerator& accelerator,
    std::optional<int> device_id) const {
  if (!::features::IsImprovedKeyboardShortcutsEnabled() ||
      !ui::KeyboardCapability::IsSixPackKey(accelerator.key_code())) {
    return std::vector<ui::Accelerator>();
  }

  if (features::IsAltClickAndSixPackCustomizationEnabled() &&
      !device_id.has_value()) {
    return std::vector<ui::Accelerator>();
  }

  if (device_id.has_value() && IsSplitModifierKeyboard(device_id.value())) {
    const auto iter = ui::kSixPackKeyToFnKeyMap.find(accelerator.key_code());
    // [Insert] is technically a six pack key but has no Fn based rewrite. Need
    // to make sure we return no aliased accelerator for this case.
    if (iter == ui::kSixPackKeyToFnKeyMap.end()) {
      return std::vector<ui::Accelerator>();
    }

    return {ui::Accelerator(iter->second,
                            accelerator.modifiers() | ui::EF_FUNCTION_DOWN,
                            accelerator.key_state())};
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

  const ui::KeyboardCode accel_key_code = accelerator.key_code();
  const ui::mojom::SixPackShortcutModifier six_pack_shortcut_modifier =
      GetSixPackShortcutModifier(accel_key_code, device_id);

  if (six_pack_shortcut_modifier == ui::mojom::SixPackShortcutModifier::kNone) {
    return std::vector<ui::Accelerator>();
  }

  // For all |six_pack_keys|, avoid remapping if the six-pack remap modifier
  // (Search or Alt) is part of the original accelerator.
  if (ui::mojom::SixPackShortcutModifier::kSearch ==
          six_pack_shortcut_modifier &&
      accelerator.IsCmdDown()) {
    return std::vector<ui::Accelerator>();
  }

  if (ui::mojom::SixPackShortcutModifier::kAlt == six_pack_shortcut_modifier &&
      accelerator.IsAltDown()) {
    return std::vector<ui::Accelerator>();
  }

  // For the Home and End Alt-based aliases, they additionally include the
  // Ctrl modifier, so the original accelerator must not include Ctrl.
  if (ui::mojom::SixPackShortcutModifier::kAlt == six_pack_shortcut_modifier &&
      accelerator.IsCtrlDown() &&
      (accel_key_code == ui::VKEY_HOME || accel_key_code == ui::VKEY_END)) {
    return std::vector<ui::Accelerator>();
  }

  int modifiers;
  ui::KeyboardCode key_code;
  if (six_pack_shortcut_modifier ==
      ui::mojom::SixPackShortcutModifier::kSearch) {
    key_code = ui::kSixPackKeyToSearchSystemKeyMap.at(accel_key_code);
    modifiers = ui::EF_COMMAND_DOWN;
  } else {
    key_code = ui::kSixPackKeyToAltSystemKeyMap.at(accel_key_code);
    modifiers = (accel_key_code == ui::KeyboardCode::VKEY_END ||
                 accel_key_code == ui::KeyboardCode::VKEY_HOME)
                    ? (ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN)
                    : ui::EF_ALT_DOWN;
  }

  // For Insert: [modifiers] = [Search] + [Shift] + [original_modifiers].
  // For other |six_pack_keys|:
  // [modifiers] = [Search/Alt] + [original_modifiers].
  int updated_modifiers =
      accel_key_code == ui::KeyboardCode::VKEY_INSERT
          ? accelerator.modifiers() | ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN
          : accelerator.modifiers() | modifiers;
  return {
      ui::Accelerator(key_code, updated_modifiers, accelerator.key_state())};
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
    internal_keyboard = std::nullopt;
  }

  for (const auto& accelerator : accelerators) {
    // TODO(dpad): Handle privacy screen correctly. This can be simplified once
    // drallion devices are properly handled in `KeyboardCapability`.
    if (auto action_key = ui::KeyboardCapability::ConvertToTopRowActionKey(
            accelerator.key_code());
        action_key.has_value() &&
        action_key != ui::TopRowActionKey::kPrivacyScreenToggle) {
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
      if (Shell::Get()->privacy_screen_controller()->IsSupported()) {
        for (const ui::KeyboardDevice& keyboard :
             ui::DeviceDataManager::GetInstance()->GetKeyboardDevices()) {
          if (keyboard_capability->GetDeviceType(keyboard) ==
              ui::KeyboardCapability::DeviceType::kDeviceInternalKeyboard) {
            filtered_accelerators.push_back(accelerator);
            break;
          }
        }
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

    // Add [CapsLock] to the accelerators list if keyboard has CapsLock.
    if (accelerator.key_code() == ui::VKEY_CAPITAL) {
      if ((priority_keyboard &&
           keyboard_capability->HasCapsLockKey(*priority_keyboard)) ||
          (internal_keyboard &&
           keyboard_capability->HasCapsLockKey(*internal_keyboard))) {
        filtered_accelerators.push_back(accelerator);
      }
      continue;
    }

    // Add Alt + Search to the accelerators list.
    if (accelerator.key_code() == ui::VKEY_LWIN &&
        accelerator.modifiers() == ui::EF_ALT_DOWN) {
      if ((internal_keyboard &&
           !IsSplitModifierKeyboard(internal_keyboard->id)) ||
          priority_keyboard) {
        filtered_accelerators.push_back(accelerator);
      }
      continue;
    }

    if (accelerator.key_code() == ui::VKEY_RIGHT_ALT) {
      if (internal_keyboard && IsSplitModifierKeyboard(internal_keyboard->id)) {
        filtered_accelerators.push_back(accelerator);
      } else if ((internal_keyboard &&
                  HasRightAltKeyViaModifierRemapping(*internal_keyboard)) ||
                 (priority_keyboard &&
                  HasRightAltKeyViaModifierRemapping(*priority_keyboard))) {
        filtered_accelerators.push_back(accelerator);
      }
      continue;
    }

    // Otherwise, always copy the accelerator.
    filtered_accelerators.push_back(accelerator);
  }

  return filtered_accelerators;
}

}  // namespace ash
