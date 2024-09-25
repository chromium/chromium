// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/input_device_settings_controller_impl.h"

#include <iterator>
#include <memory>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/events/event_rewriter_controller_impl.h"
#include "ash/events/peripheral_customization_event_rewriter.h"
#include "ash/public/cpp/accelerators_util.h"
#include "ash/public/cpp/input_device_settings_controller.h"
#include "ash/public/mojom/input_device_settings.mojom-forward.h"
#include "ash/public/mojom/input_device_settings.mojom-shared.h"
#include "ash/public/mojom/input_device_settings.mojom.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/input_device_settings/input_device_duplicate_id_finder.h"
#include "ash/system/input_device_settings/input_device_key_alias_manager.h"
#include "ash/system/input_device_settings/input_device_notifier.h"
#include "ash/system/input_device_settings/input_device_settings_defaults.h"
#include "ash/system/input_device_settings/input_device_settings_logging.h"
#include "ash/system/input_device_settings/input_device_settings_metadata.h"
#include "ash/system/input_device_settings/input_device_settings_metadata_manager.h"
#include "ash/system/input_device_settings/input_device_settings_notification_controller.h"
#include "ash/system/input_device_settings/input_device_settings_policy_handler.h"
#include "ash/system/input_device_settings/input_device_settings_pref_names.h"
#include "ash/system/input_device_settings/input_device_settings_utils.h"
#include "ash/system/input_device_settings/modifier_split_bypass_checker.h"
#include "ash/system/input_device_settings/pref_handlers/graphics_tablet_pref_handler_impl.h"
#include "ash/system/input_device_settings/pref_handlers/keyboard_pref_handler_impl.h"
#include "ash/system/input_device_settings/pref_handlers/mouse_pref_handler_impl.h"
#include "ash/system/input_device_settings/pref_handlers/pointing_stick_pref_handler_impl.h"
#include "ash/system/input_device_settings/pref_handlers/touchpad_pref_handler_impl.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/hash/sha1.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/to_string.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "components/account_id/account_id.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/services/app_service/public/cpp/app_registry_cache_wrapper.h"
#include "components/services/app_service/public/cpp/types_util.h"
#include "components/session_manager/session_manager_types.h"
#include "components/user_manager/known_user.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_device.h"
#include "mojo/public/cpp/bindings/struct_ptr.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/ash/keyboard_capability.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/devices/keyboard_device.h"
#include "ui/events/devices/touchpad_device.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/ozone/layout/keyboard_layout_engine.h"
#include "ui/events/ozone/layout/keyboard_layout_engine_manager.h"
#include "ui/message_center/message_center.h"

namespace ash {

namespace {

const int kMaxButtonNameLength = 32;

constexpr mojom::TopRowActionKey ConvertTopRowActionKey(
    ui::TopRowActionKey action_key) {
  switch (action_key) {
    case ui::TopRowActionKey::kBack:
      return mojom::TopRowActionKey::kBack;
    case ui::TopRowActionKey::kForward:
      return mojom::TopRowActionKey::kForward;
    case ui::TopRowActionKey::kRefresh:
      return mojom::TopRowActionKey::kRefresh;
    case ui::TopRowActionKey::kFullscreen:
      return mojom::TopRowActionKey::kFullscreen;
    case ui::TopRowActionKey::kOverview:
      return mojom::TopRowActionKey::kOverview;
    case ui::TopRowActionKey::kScreenshot:
      return mojom::TopRowActionKey::kScreenshot;
    case ui::TopRowActionKey::kScreenBrightnessDown:
      return mojom::TopRowActionKey::kScreenBrightnessDown;
    case ui::TopRowActionKey::kScreenBrightnessUp:
      return mojom::TopRowActionKey::kScreenBrightnessUp;
    case ui::TopRowActionKey::kMicrophoneMute:
      return mojom::TopRowActionKey::kMicrophoneMute;
    case ui::TopRowActionKey::kVolumeMute:
      return mojom::TopRowActionKey::kVolumeMute;
    case ui::TopRowActionKey::kVolumeDown:
      return mojom::TopRowActionKey::kVolumeDown;
    case ui::TopRowActionKey::kVolumeUp:
      return mojom::TopRowActionKey::kVolumeUp;
    case ui::TopRowActionKey::kKeyboardBacklightToggle:
      return mojom::TopRowActionKey::kKeyboardBacklightToggle;
    case ui::TopRowActionKey::kKeyboardBacklightDown:
      return mojom::TopRowActionKey::kKeyboardBacklightDown;
    case ui::TopRowActionKey::kKeyboardBacklightUp:
      return mojom::TopRowActionKey::kKeyboardBacklightUp;
    case ui::TopRowActionKey::kNextTrack:
      return mojom::TopRowActionKey::kNextTrack;
    case ui::TopRowActionKey::kPreviousTrack:
      return mojom::TopRowActionKey::kPreviousTrack;
    case ui::TopRowActionKey::kPlayPause:
      return mojom::TopRowActionKey::kPlayPause;
    case ui::TopRowActionKey::kPrivacyScreenToggle:
      return mojom::TopRowActionKey::kPrivacyScreenToggle;
    case ui::TopRowActionKey::kAllApplications:
      return mojom::TopRowActionKey::kAllApplications;
    case ui::TopRowActionKey::kEmojiPicker:
      return mojom::TopRowActionKey::kEmojiPicker;
    case ui::TopRowActionKey::kDictation:
      return mojom::TopRowActionKey::kDictation;
    case ui::TopRowActionKey::kAccessibility:
      return mojom::TopRowActionKey::kAccessibility;
    case ui::TopRowActionKey::kUnknown:
    case ui::TopRowActionKey::kNone:
      return mojom::TopRowActionKey::kNone;
  }
}

std::vector<mojom::TopRowActionKey> GetTopRowActionKeys(
    const ui::KeyboardDevice& keyboard) {
  const auto* action_keys =
      Shell::Get()->keyboard_capability()->GetTopRowActionKeys(keyboard);
  if (!action_keys) {
    return std::vector<mojom::TopRowActionKey>();
  }

  std::vector<mojom::TopRowActionKey> top_row_keys;
  for (const auto& key : *action_keys) {
    top_row_keys.push_back(ConvertTopRowActionKey(key));
  }
  return top_row_keys;
}

void RecordMouseMetadataTierMetrics(
    const ui::InputDevice& mouse,
    mojom::MouseButtonConfig mouse_button_config) {
  auto* metadata = GetMouseMetadata(mouse);
  MetadataTier tier;

  if (!metadata) {
    tier = MetadataTier::kNoMetadata;
  } else if (mouse_button_config == mojom::MouseButtonConfig::kNoConfig) {
    tier = MetadataTier::kClassificationOnly;
  } else {
    tier = MetadataTier::kHasButtonConfig;
  }

  base::UmaHistogramEnumeration("ChromeOS.Inputs.Mouse.MetadataTier", tier);
}

void RecordGraphicsTabletMetadataTierMetrics(
    const ui::InputDevice& graphics_tablet,
    mojom::GraphicsTabletButtonConfig graphics_tablet_button_config) {
  auto* metadata = GetGraphicsTabletMetadata(graphics_tablet);
  MetadataTier tier;

  if (!metadata) {
    tier = MetadataTier::kNoMetadata;
  } else if (graphics_tablet_button_config ==
             mojom::GraphicsTabletButtonConfig::kNoConfig) {
    tier = MetadataTier::kClassificationOnly;
  } else {
    tier = MetadataTier::kHasButtonConfig;
  }

  base::UmaHistogramEnumeration("ChromeOS.Inputs.GraphicsTablet.MetadataTier",
                                tier);
}

void RecordKeyboardMetadataTierMetrics(const ui::KeyboardDevice& keyboard) {
  auto* metadata = GetKeyboardMetadata(keyboard);
  MetadataTier tier;

  if (!metadata) {
    tier = MetadataTier::kNoMetadata;
  } else {
    tier = MetadataTier::kClassificationOnly;
  }

  base::UmaHistogramEnumeration("ChromeOS.Inputs.Keyboard.MetadataTier", tier);
}

mojom::ChargeState GetChargeStateFromBluetoothDevice(
    device::BluetoothDevice::BatteryInfo::ChargeState charge_state) {
  switch (charge_state) {
    case device::BluetoothDevice::BatteryInfo::ChargeState::kUnknown:
      return mojom::ChargeState::kUnknown;
    case device::BluetoothDevice::BatteryInfo::ChargeState::kCharging:
      return mojom::ChargeState::kCharging;
    case device::BluetoothDevice::BatteryInfo::ChargeState::kDischarging:
      return mojom::ChargeState::kDischarging;
  }
}

device::BluetoothDevice* GetBluetoothDevice(device::BluetoothAdapter* adapter,
                                            const std::string& device_key) {
  for (auto* device : adapter->GetDevices()) {
    if (Shell::Get()->input_device_key_alias_manager()->GetAliasedDeviceKey(
            device->GetVendorID(), device->GetProductID()) == device_key) {
      return device;
    }
  }
  return nullptr;
}

mojom::BatteryInfoPtr GetBatteryInfo(const device::BluetoothDevice& bt_device) {
  auto battery_info =
      bt_device.GetBatteryInfo(device::BluetoothDevice::BatteryType::kDefault);
  if (!battery_info || !battery_info->percentage.has_value()) {
    return nullptr;
  }
  const int percentage = battery_info->percentage.value();
  CHECK_GE(percentage, 0);
  CHECK_LE(percentage, 100);
  return mojom::BatteryInfo::New(percentage, GetChargeStateFromBluetoothDevice(
                                                 battery_info->charge_state));
}

mojom::KeyboardPtr BuildMojomKeyboard(const ui::KeyboardDevice& keyboard) {
  mojom::KeyboardPtr mojom_keyboard = mojom::Keyboard::New();
  mojom_keyboard->id = keyboard.id;
  mojom_keyboard->name = keyboard.name;
  mojom_keyboard->device_key =
      Shell::Get()->input_device_key_alias_manager()->GetAliasedDeviceKey(
          keyboard);
  mojom_keyboard->is_external =
      keyboard.type != ui::InputDeviceType::INPUT_DEVICE_INTERNAL;
  // Enable only when flag is enabled to avoid crashing while problem is
  // addressed. See b/272960076
  if (features::IsInputDeviceSettingsSplitEnabled()) {
    mojom_keyboard->modifier_keys =
        Shell::Get()->keyboard_capability()->GetModifierKeys(keyboard);
    mojom_keyboard->meta_key =
        Shell::Get()->keyboard_capability()->GetMetaKey(keyboard);
  }
  if (::features::AreF11AndF12ShortcutsEnabled()) {
    mojom_keyboard->top_row_action_keys = GetTopRowActionKeys(keyboard);
  }
  RecordKeyboardMetadataTierMetrics(keyboard);

  return mojom_keyboard;
}

mojom::MousePtr BuildMojomMouse(
    const ui::InputDevice& mouse,
    mojom::CustomizationRestriction customization_restriction,
    mojom::MouseButtonConfig mouse_button_config,
    const std::string& name) {
  mojom::MousePtr mojom_mouse = mojom::Mouse::New();
  mojom_mouse->id = mouse.id;
  mojom_mouse->name = name;
  mojom_mouse->customization_restriction = customization_restriction;
  mojom_mouse->mouse_button_config = mouse_button_config;
  mojom_mouse->device_key =
      Shell::Get()->input_device_key_alias_manager()->GetAliasedDeviceKey(
          mouse);
  mojom_mouse->is_external =
      mouse.type != ui::InputDeviceType::INPUT_DEVICE_INTERNAL;
  RecordMouseMetadataTierMetrics(mouse, mouse_button_config);

  return mojom_mouse;
}

mojom::TouchpadPtr BuildMojomTouchpad(const ui::TouchpadDevice& touchpad) {
  mojom::TouchpadPtr mojom_touchpad = mojom::Touchpad::New();
  mojom_touchpad->id = touchpad.id;
  mojom_touchpad->name = touchpad.name;
  mojom_touchpad->device_key =
      Shell::Get()->input_device_key_alias_manager()->GetAliasedDeviceKey(
          touchpad);
  mojom_touchpad->is_external =
      touchpad.type != ui::InputDeviceType::INPUT_DEVICE_INTERNAL;
  mojom_touchpad->is_haptic = touchpad.is_haptic;

  return mojom_touchpad;
}

mojom::PointingStickPtr BuildMojomPointingStick(
    const ui::InputDevice& touchpad) {
  mojom::PointingStickPtr mojom_pointing_stick = mojom::PointingStick::New();
  mojom_pointing_stick->id = touchpad.id;
  mojom_pointing_stick->name = touchpad.name;
  mojom_pointing_stick->device_key =
      Shell::Get()->input_device_key_alias_manager()->GetAliasedDeviceKey(
          touchpad);
  mojom_pointing_stick->is_external =
      touchpad.type != ui::InputDeviceType::INPUT_DEVICE_INTERNAL;
  return mojom_pointing_stick;
}

mojom::GraphicsTabletPtr BuildMojomGraphicsTablet(
    const ui::InputDevice& graphics_tablet,
    mojom::CustomizationRestriction customization_restriction,
    mojom::GraphicsTabletButtonConfig graphics_tablet_button_config) {
  mojom::GraphicsTabletPtr mojom_graphics_tablet = mojom::GraphicsTablet::New();
  auto* metadata = GetGraphicsTabletMetadata(graphics_tablet);
  std::string name = features::IsWelcomeExperienceEnabled() && metadata &&
                             metadata->name.has_value()
                         ? metadata->name.value()
                         : graphics_tablet.name;
  mojom_graphics_tablet->name = name;
  mojom_graphics_tablet->id = graphics_tablet.id;
  mojom_graphics_tablet->customization_restriction = customization_restriction;
  mojom_graphics_tablet->graphics_tablet_button_config =
      graphics_tablet_button_config;
  mojom_graphics_tablet->device_key =
      Shell::Get()->input_device_key_alias_manager()->GetAliasedDeviceKey(
          graphics_tablet);
  RecordGraphicsTabletMetadataTierMetrics(graphics_tablet,
                                          graphics_tablet_button_config);

  return mojom_graphics_tablet;
}

bool IsGraphicsTabletPenButton(const mojom::Button& button) {
  return button.is_customizable_button();
}

void AddButtonToButtonRemappingList(
    const mojom::Button& button,
    std::vector<mojom::ButtonRemappingPtr>& button_remappings,
    bool is_mouse_button_remapping) {
  std::string button_name;
  // If its a middle click, give it the default middle button name.
  if (is_mouse_button_remapping && button.is_customizable_button() &&
      button.get_customizable_button() == mojom::CustomizableButton::kMiddle) {
    button_name = l10n_util::GetStringUTF8(
        IDS_SETTINGS_CUSTOMIZATION_MIDDLE_BUTTON_DEFAULT_NAME);
  } else {
    // Otherwise, give it the default button name indexed at the number of
    // non-middle click buttons in `button_remappings` + 1.
    auto iter =
        base::ranges::find(button_remappings,
                           *mojom::Button::NewCustomizableButton(
                               mojom::CustomizableButton::kMiddle),
                           [](const mojom::ButtonRemappingPtr& remapping) {
                             return *remapping->button;
                           });

    int button_number = button_remappings.size() + 1;
    if (is_mouse_button_remapping && iter != button_remappings.end()) {
      --button_number;
    }
    button_name = l10n_util::GetStringFUTF8(
        IDS_SETTINGS_CUSTOMIZATION_OTHER_BUTTON_DEFAULT_NAME,
        base::NumberToString16(button_number));
  }

  button_remappings.push_back(mojom::ButtonRemapping::New(
      std::move(button_name), button.Clone(), /*remapping_action=*/nullptr));
}

// suppress_meta_fkey_rewrites must never be non-default for internal
// keyboards, otherwise the keyboard settings are not valid.
// Modifier remappings must only contain valid modifiers within the
// modifier_keys array. Settings are invalid if top_row_are_fkeys_policy exists
// and policy status is kManaged and the top_row_are_fkeys_policy's value is
// different from the settings top_row_are_fkeys value. F11/F12 settings
// should only be included for ChromeOS keyboards.
bool KeyboardSettingsAreValid(
    const mojom::Keyboard& keyboard,
    const mojom::KeyboardSettings& settings,
    const mojom::KeyboardPolicies& keyboard_policies) {
  const bool containsFnKey =
      base::Contains(keyboard.modifier_keys, ui::mojom::ModifierKey::kFunction);

  for (const auto& remapping : settings.modifier_remappings) {
    auto it = base::ranges::find(keyboard.modifier_keys, remapping.first);
    if (it == keyboard.modifier_keys.end()) {
      return false;
    }
    // No modifier key can be remapped to function key if function key is not a
    // modifier key.
    if (!containsFnKey &&
        remapping.second == ui::mojom::ModifierKey::kFunction) {
      return false;
    }
  }
  if (keyboard_policies.top_row_are_fkeys_policy &&
      keyboard_policies.top_row_are_fkeys_policy->policy_status ==
          mojom::PolicyStatus::kManaged &&
      keyboard_policies.top_row_are_fkeys_policy->value !=
          settings.top_row_are_fkeys) {
    return false;
  }

  const bool is_non_chromeos_keyboard =
      (keyboard.meta_key != ui::mojom::MetaKey::kLauncher &&
       keyboard.meta_key != ui::mojom::MetaKey::kSearch);
  if (is_non_chromeos_keyboard && ::features::AreF11AndF12ShortcutsEnabled() &&
      (settings.f11.has_value() || settings.f12.has_value())) {
    return false;
  }

  const bool is_meta_suppressed_setting_default =
      settings.suppress_meta_fkey_rewrites == kDefaultSuppressMetaFKeyRewrites;

  // The suppress_meta_fkey_rewrites setting can only be changed if the device
  // is a non-chromeos keyboard.
  return is_non_chromeos_keyboard || is_meta_suppressed_setting_default;
}

// The haptic_enabled and haptic_sensitivity are allowed to change only if the
// touchpad is haptic.
bool TouchpadSettingsAreValid(const mojom::Touchpad& touchpad,
                              const mojom::TouchpadSettings& settings) {
  return touchpad.is_haptic ||
         (touchpad.settings->haptic_enabled == settings.haptic_enabled &&
          touchpad.settings->haptic_sensitivity == settings.haptic_sensitivity);
}

// ValidateButtonRemappingList verifies if the new button remapping list has
// the same buttons as these in the original button remapping list and all the
// button remapping names should be fewer than 32 characters.
bool ValidateButtonRemappingList(
    const std::vector<mojom::ButtonRemappingPtr>& original_remapping_list,
    const std::vector<mojom::ButtonRemappingPtr>& new_remapping_list) {
  if (original_remapping_list.size() != new_remapping_list.size()) {
    return false;
  }

  for (const auto& new_remapping : new_remapping_list) {
    bool found = false;
    for (const auto& original_remapping : original_remapping_list) {
      if (*original_remapping->button == *new_remapping->button) {
        found = true;
        break;
      }
    }
    if (!found || new_remapping->name.size() > kMaxButtonNameLength) {
      return false;
    }
  }

  return true;
}

// Valid graphics tablet settings should have the same tablet and pen buttons
// as these in the graphics tablet and all the button remapping names should be
// fewer than 32 characters.
bool GraphicsTabletSettingsAreValid(
    const mojom::GraphicsTablet& graphics_tablet,
    const mojom::GraphicsTabletSettings& settings) {
  return ValidateButtonRemappingList(
             graphics_tablet.settings->tablet_button_remappings,
             settings.tablet_button_remappings) &&
         ValidateButtonRemappingList(
             graphics_tablet.settings->pen_button_remappings,
             settings.pen_button_remappings);
}

// Valid mouse settings should have the same buttons as those
// in the mouse and all the button remapping names should be
// fewer than 32 characters.
bool MouseSettingsAreValid(const mojom::Mouse& mouse,
                           const mojom::MouseSettings& settings) {
  if (!features::IsPeripheralCustomizationEnabled()) {
    return true;
  }
  return ValidateButtonRemappingList(mouse.settings->button_remappings,
                                     settings.button_remappings);
}

void RecordSetKeyboardSettingsValidMetric(bool is_valid) {
  base::UmaHistogramBoolean(
      "ChromeOS.Settings.Device.Keyboard.SetSettingsSucceeded", is_valid);
}

void RecordSetTouchpadSettingsValidMetric(bool is_valid) {
  base::UmaHistogramBoolean(
      "ChromeOS.Settings.Device.Touchpad.SetSettingsSucceeded", is_valid);
}

void RecordSetPointingStickSettingsValidMetric(bool is_valid) {
  base::UmaHistogramBoolean(
      "ChromeOS.Settings.Device.PointingStick.SetSettingsSucceeded", is_valid);
}

void RecordSetMouseSettingsValidMetric(bool is_valid) {
  base::UmaHistogramBoolean(
      "ChromeOS.Settings.Device.Mouse.SetSettingsSucceeded", is_valid);
}

// Check the list of keyboards to see if any have the same |device_key|.
// If so, their settings need to also be updated.
template <typename T>
void UpdateDuplicateDeviceSettings(
    const T& updated_device,
    base::flat_map<InputDeviceSettingsControllerImpl::DeviceId,
                   mojo::StructPtr<T>>& devices,
    base::RepeatingCallback<void(InputDeviceSettingsControllerImpl::DeviceId)>
        settings_updated_callback) {
  for (const auto& [device_id, device] : devices) {
    if (device_id != updated_device.id &&
        device->device_key == updated_device.device_key) {
      device->settings = updated_device.settings->Clone();
      settings_updated_callback.Run(device_id);
    }
  }
}

template <typename T>
T* FindDevice(InputDeviceSettingsControllerImpl::DeviceId id,
              const InputDeviceDuplicateIdFinder* deduper,
              base::flat_map<InputDeviceSettingsControllerImpl::DeviceId,
                             mojo::StructPtr<T>>& devices_) {
  auto iter = devices_.find(id);
  if (iter != devices_.end()) {
    return iter->second.get();
  }

  if (!deduper) {
    return nullptr;
  }

  const auto* duplicate_device_ids = deduper->GetDuplicateDeviceIds(id);
  if (!duplicate_device_ids) {
    return nullptr;
  }

  for (const auto& duplicate_id : *duplicate_device_ids) {
    iter = devices_.find(duplicate_id);
    if (iter != devices_.end()) {
      return iter->second.get();
    }
  }

  return nullptr;
}

void DeleteLoginScreenSettingsPrefWhenInputDeviceSettingsSplitDisabled(
    PrefService* local_state) {
  // local_state could be null in tests.
  if (!local_state) {
    return;
  }
  user_manager::KnownUser known_user(local_state);
  AccountId account_id =
      Shell::Get()->session_controller()->GetActiveAccountId();

  known_user.SetPath(account_id, prefs::kMouseLoginScreenInternalSettingsPref,
                     std::nullopt);
  known_user.SetPath(account_id, prefs::kMouseLoginScreenExternalSettingsPref,
                     std::nullopt);
  known_user.SetPath(account_id,
                     prefs::kKeyboardLoginScreenInternalSettingsPref,
                     std::nullopt);
  known_user.SetPath(account_id,
                     prefs::kKeyboardLoginScreenExternalSettingsPref,
                     std::nullopt);
  known_user.SetPath(account_id,
                     prefs::kPointingStickLoginScreenInternalSettingsPref,
                     std::nullopt);
  known_user.SetPath(account_id,
                     prefs::kPointingStickLoginScreenExternalSettingsPref,
                     std::nullopt);
  known_user.SetPath(account_id,
                     prefs::kTouchpadLoginScreenInternalSettingsPref,
                     std::nullopt);
  known_user.SetPath(account_id,
                     prefs::kTouchpadLoginScreenExternalSettingsPref,
                     std::nullopt);
}

void DeleteLoginScreenButtonRemappingListPrefWhenPeripheralCustomizationDisabled(
    PrefService* local_state) {
  // local_state could be null in tests.
  if (!local_state) {
    return;
  }
  user_manager::KnownUser known_user(local_state);
  AccountId account_id =
      Shell::Get()->session_controller()->GetActiveAccountId();

  known_user.SetPath(
      account_id,
      prefs::kGraphicsTabletLoginScreenTabletButtonRemappingListPref,
      std::nullopt);
  known_user.SetPath(
      account_id, prefs::kGraphicsTabletLoginScreenPenButtonRemappingListPref,
      std::nullopt);
  known_user.SetPath(account_id,
                     prefs::kMouseLoginScreenButtonRemappingListPref,
                     std::nullopt);
}

void RefreshKeyDisplayInRemappingList(
    std::vector<mojom::ButtonRemappingPtr>& remappings) {
  for (const auto& remapping : remappings) {
    if (!remapping->remapping_action ||
        !remapping->remapping_action->is_key_event()) {
      continue;
    }
    mojom::KeyEvent& key_event = *remapping->remapping_action->get_key_event();
    key_event.key_display = base::UTF16ToUTF8(GetKeyDisplay(key_event.vkey));
  }
}

void RefreshKeyDisplayMouse(mojom::Mouse& mouse) {
  RefreshKeyDisplayInRemappingList(mouse.settings->button_remappings);
}

void RefreshKeyDisplayGraphicsTablet(mojom::GraphicsTablet& graphics_tablet) {
  RefreshKeyDisplayInRemappingList(
      graphics_tablet.settings->tablet_button_remappings);
  RefreshKeyDisplayInRemappingList(
      graphics_tablet.settings->pen_button_remappings);
}

bool BatteryInfoChanged(const mojom::BatteryInfo& existing_info,
                        const mojom::BatteryInfo& new_info) {
  return existing_info.battery_percentage != new_info.battery_percentage ||
         existing_info.charge_state != new_info.charge_state;
}

}  // namespace

InputDeviceSettingsControllerImpl::InputDeviceSettingsControllerImpl(
    PrefService* local_state)
    : local_state_(local_state),
      keyboard_pref_handler_(std::make_unique<KeyboardPrefHandlerImpl>()),
      touchpad_pref_handler_(std::make_unique<TouchpadPrefHandlerImpl>()),
      mouse_pref_handler_(std::make_unique<MousePrefHandlerImpl>()),
      pointing_stick_pref_handler_(
          std::make_unique<PointingStickPrefHandlerImpl>()),
      graphics_tablet_pref_handler_(
          std::make_unique<GraphicsTabletPrefHandlerImpl>()),
      sequenced_task_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {
  Init();
}

InputDeviceSettingsControllerImpl::InputDeviceSettingsControllerImpl(
    PrefService* local_state,
    std::unique_ptr<KeyboardPrefHandler> keyboard_pref_handler,
    std::unique_ptr<TouchpadPrefHandler> touchpad_pref_handler,
    std::unique_ptr<MousePrefHandler> mouse_pref_handler,
    std::unique_ptr<PointingStickPrefHandler> pointing_stick_pref_handler,
    std::unique_ptr<GraphicsTabletPrefHandler> graphics_tablet_pref_handler,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : local_state_(local_state),
      keyboard_pref_handler_(std::move(keyboard_pref_handler)),
      touchpad_pref_handler_(std::move(touchpad_pref_handler)),
      mouse_pref_handler_(std::move(mouse_pref_handler)),
      pointing_stick_pref_handler_(std::move(pointing_stick_pref_handler)),
      graphics_tablet_pref_handler_(std::move(graphics_tablet_pref_handler)),
      sequenced_task_runner_(std::move(task_runner)) {
  Init();
}

void InputDeviceSettingsControllerImpl::Init() {
  Shell::Get()->session_controller()->AddObserver(this);
  CHECK(input_method::InputMethodManager::Get());
  input_method::InputMethodManager::Get()->AddObserver(this);

  if (features::IsModifierSplitEnabled() &&
      base::FeatureList::IsEnabled(features::kModifierSplitDeviceEnabled)) {
    modifier_split_bypass_checker_ =
        std::make_unique<ModifierSplitBypassChecker>();
  }

  if (features::IsWelcomeExperienceEnabled()) {
    message_center::MessageCenter::Get()->AddObserver(this);
    device::BluetoothAdapterFactory::Get()->GetAdapter(base::BindOnce(
        &InputDeviceSettingsControllerImpl::InitializeOnBluetoothReady,
        weak_ptr_factory_.GetWeakPtr()));
  }

  InitializePolicyHandler();
  // Initialize the duplicate id finder first then the notifiers to make sure
  // duplicate ids are up to date before the controller gets updates about
  // connected devices.
  if (features::IsPeripheralCustomizationEnabled()) {
    duplicate_id_finder_ = std::make_unique<InputDeviceDuplicateIdFinder>();
  }

  if (base::FeatureList::IsEnabled(features::kWelcomeExperience) ||
      base::FeatureList::IsEnabled(features::kPeripheralNotification)) {
    notification_controller_ =
        std::make_unique<InputDeviceSettingsNotificationController>(
            message_center::MessageCenter::Get());
  }

  if (features::IsWelcomeExperienceEnabled()) {
    metadata_manager_ = std::make_unique<InputDeviceSettingsMetadataManager>();
  }

  keyboard_notifier_ = std::make_unique<
      InputDeviceNotifier<mojom::KeyboardPtr, ui::KeyboardDevice>>(
      &keyboards_,
      base::BindRepeating(
          &InputDeviceSettingsControllerImpl::OnKeyboardListUpdated,
          base::Unretained(this)));
  mouse_notifier_ =
      std::make_unique<InputDeviceNotifier<mojom::MousePtr, ui::InputDevice>>(
          &mice_, base::BindRepeating(
                      &InputDeviceSettingsControllerImpl::OnMouseListUpdated,
                      base::Unretained(this)));
  touchpad_notifier_ = std::make_unique<
      InputDeviceNotifier<mojom::TouchpadPtr, ui::TouchpadDevice>>(
      &touchpads_,
      base::BindRepeating(
          &InputDeviceSettingsControllerImpl::OnTouchpadListUpdated,
          base::Unretained(this)));
  pointing_stick_notifier_ = std::make_unique<
      InputDeviceNotifier<mojom::PointingStickPtr, ui::InputDevice>>(
      &pointing_sticks_,
      base::BindRepeating(
          &InputDeviceSettingsControllerImpl::OnPointingStickListUpdated,
          base::Unretained(this)));
  if (features::IsPeripheralCustomizationEnabled()) {
    graphics_tablet_notifier_ = std::make_unique<
        InputDeviceNotifier<mojom::GraphicsTabletPtr, ui::InputDevice>>(
        &graphics_tablets_,
        base::BindRepeating(
            &InputDeviceSettingsControllerImpl::OnGraphicsTabletListUpdated,
            base::Unretained(this)));
  }
  metrics_manager_ = std::make_unique<InputDeviceSettingsMetricsManager>();
}

void InputDeviceSettingsControllerImpl::InitializePolicyHandler() {
  policy_handler_ = std::make_unique<InputDeviceSettingsPolicyHandler>(
      base::BindRepeating(
          &InputDeviceSettingsControllerImpl::OnKeyboardPoliciesChanged,
          base::Unretained(this)),
      base::BindRepeating(
          &InputDeviceSettingsControllerImpl::OnMousePoliciesChanged,
          base::Unretained(this)));
  // Only initialize if we have either local state or pref service.
  // `local_state` can be null in tests.
  if (local_state_ || active_pref_service_) {
    policy_handler_->Initialize(local_state_, active_pref_service_);
  }
}

void InputDeviceSettingsControllerImpl::
    RefreshBatteryInfoForConnectedDevices() {
  if (!bluetooth_adapter_) {
    return;
  }

  for (const auto& [id, keyboard] : keyboards_) {
    if (auto* device =
            GetBluetoothDevice(bluetooth_adapter_.get(), keyboard->device_key);
        device != nullptr) {
      auto updated_battery_info = GetBatteryInfo(*device);
      if (keyboard->battery_info.is_null() ||
          BatteryInfoChanged(*keyboard->battery_info, *updated_battery_info)) {
        keyboard->battery_info = std::move(updated_battery_info);
        DispatchKeyboardBatteryInfoChanged(id);
      }
    }
  }

  for (const auto& [id, touchpad] : touchpads_) {
    if (auto* device =
            GetBluetoothDevice(bluetooth_adapter_.get(), touchpad->device_key);
        device != nullptr) {
      auto updated_battery_info = GetBatteryInfo(*device);
      if (touchpad->battery_info.is_null() ||
          BatteryInfoChanged(*touchpad->battery_info, *updated_battery_info)) {
        touchpad->battery_info = std::move(updated_battery_info);
        DispatchTouchpadBatteryInfoChanged(id);
      }
    }
  }

  for (const auto& [id, mouse] : mice_) {
    if (auto* device =
            GetBluetoothDevice(bluetooth_adapter_.get(), mouse->device_key);
        device != nullptr) {
      auto updated_battery_info = GetBatteryInfo(*device);
      if (mouse->battery_info.is_null() ||
          BatteryInfoChanged(*mouse->battery_info, *updated_battery_info)) {
        mouse->battery_info = std::move(updated_battery_info);
        DispatchMouseBatteryInfoChanged(id);
      }
    }
  }

  for (const auto& [id, graphics_tablet] : graphics_tablets_) {
    if (auto* device = GetBluetoothDevice(bluetooth_adapter_.get(),
                                          graphics_tablet->device_key);
        device != nullptr) {
      auto updated_battery_info = GetBatteryInfo(*device);
      if (graphics_tablet->battery_info.is_null() ||
          BatteryInfoChanged(*graphics_tablet->battery_info,
                             *updated_battery_info)) {
        graphics_tablet->battery_info = std::move(updated_battery_info);
        DispatchGraphicsTabletBatteryInfoChanged(id);
      }
    }
  }
}

void InputDeviceSettingsControllerImpl::InitializeOnBluetoothReady(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  bluetooth_adapter_ = adapter;
  CHECK(bluetooth_adapter_);
  RefreshBatteryInfoForConnectedDevices();
  bluetooth_adapter_->AddObserver(this);
}

InputDeviceSettingsControllerImpl::~InputDeviceSettingsControllerImpl() {
  Shell::Get()->session_controller()->RemoveObserver(this);
  if (features::IsWelcomeExperienceEnabled()) {
    message_center::MessageCenter::Get()->RemoveObserver(this);
  }

  CHECK(input_method::InputMethodManager::Get());
  input_method::InputMethodManager::Get()->RemoveObserver(this);
  if (bluetooth_adapter_) {
    bluetooth_adapter_->RemoveObserver(this);
  }
  // Clear all dangling observers. Known dependency issue:
  // `InputDeviceSettingsControllerImpl` destructs before `ShortcutAppManager`.
  observers_.Clear();
}

void InputDeviceSettingsControllerImpl::RegisterProfilePrefs(
    PrefRegistrySimple* pref_registry) {
  pref_registry->RegisterDictionaryPref(prefs::kKeyboardDeviceSettingsDictPref);
  pref_registry->RegisterDictionaryPref(prefs::kMouseDeviceSettingsDictPref);
  pref_registry->RegisterDictionaryPref(
      prefs::kPointingStickDeviceSettingsDictPref);
  pref_registry->RegisterDictionaryPref(prefs::kTouchpadDeviceSettingsDictPref);
  pref_registry->RegisterDictionaryPref(prefs::kKeyboardInternalSettings);

  pref_registry->RegisterDictionaryPref(
      prefs::kTouchpadInternalSettings,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  pref_registry->RegisterDictionaryPref(
      prefs::kTouchpadDefaultSettings,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  pref_registry->RegisterDictionaryPref(
      prefs::kPointingStickInternalSettings,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  pref_registry->RegisterDictionaryPref(
      prefs::kMouseDefaultSettings,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  pref_registry->RegisterDictionaryPref(
      prefs::kKeyboardDefaultChromeOSSettings,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  pref_registry->RegisterDictionaryPref(
      prefs::kKeyboardDefaultNonChromeOSSettings,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  pref_registry->RegisterDictionaryPref(
      prefs::kKeyboardDefaultSplitModifierSettings,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  pref_registry->RegisterBooleanPref(
      prefs::kKeyboardHasSplitModifierKeyboard, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);

  pref_registry->RegisterDictionaryPref(
      prefs::kKeyboardUpdateSettingsMetricInfo);
  pref_registry->RegisterDictionaryPref(prefs::kMouseUpdateSettingsMetricInfo);
  pref_registry->RegisterDictionaryPref(
      prefs::kTouchpadUpdateSettingsMetricInfo);
  pref_registry->RegisterDictionaryPref(
      prefs::kPointingStickUpdateSettingsMetricInfo);

  pref_registry->RegisterListPref(prefs::kKeyboardDeviceImpostersListPref);
  pref_registry->RegisterListPref(prefs::kMouseDeviceImpostersListPref);
  pref_registry->RegisterDictionaryPref(prefs::kMouseButtonRemappingsDictPref);
  pref_registry->RegisterDictionaryPref(
      prefs::kGraphicsTabletTabletButtonRemappingsDictPref);
  pref_registry->RegisterDictionaryPref(
      prefs::kGraphicsTabletPenButtonRemappingsDictPref);
  pref_registry->RegisterIntegerPref(
      prefs::kF11KeyModifier,
      static_cast<int>(ui::mojom::ExtendedFkeysModifier::kDisabled));
  pref_registry->RegisterIntegerPref(
      prefs::kF12KeyModifier,
      static_cast<int>(ui::mojom::ExtendedFkeysModifier::kDisabled));
  pref_registry->RegisterIntegerPref(
      prefs::kHomeAndEndKeysModifier,
      static_cast<int>(ui::mojom::SixPackShortcutModifier::kNone));
  pref_registry->RegisterIntegerPref(
      prefs::kPageUpAndPageDownKeysModifier,
      static_cast<int>(ui::mojom::SixPackShortcutModifier::kNone));
  pref_registry->RegisterIntegerPref(
      prefs::kDeleteKeyModifier,
      static_cast<int>(ui::mojom::SixPackShortcutModifier::kNone));
  pref_registry->RegisterIntegerPref(
      prefs::kInsertKeyModifier,
      static_cast<int>(ui::mojom::SixPackShortcutModifier::kNone));
  pref_registry->RegisterIntegerPref(prefs::kPageUpRemappingNudgeShownCount, 0);
  pref_registry->RegisterIntegerPref(prefs::kPageDownRemappingNudgeShownCount,
                                     0);
  pref_registry->RegisterIntegerPref(prefs::kHomeRemappingNudgeShownCount, 0);
  pref_registry->RegisterIntegerPref(prefs::kEndRemappingNudgeShownCount, 0);
  pref_registry->RegisterIntegerPref(prefs::kDeleteRemappingNudgeShownCount, 0);
  pref_registry->RegisterIntegerPref(prefs::kInsertRemappingNudgeShownCount, 0);
  pref_registry->RegisterTimePref(prefs::kPageUpRemappingNudgeLastShown,
                                  base::Time());
  pref_registry->RegisterTimePref(prefs::kPageDownRemappingNudgeLastShown,
                                  base::Time());
  pref_registry->RegisterTimePref(prefs::kHomeRemappingNudgeLastShown,
                                  base::Time());
  pref_registry->RegisterTimePref(prefs::kEndRemappingNudgeLastShown,
                                  base::Time());
  pref_registry->RegisterTimePref(prefs::kDeleteRemappingNudgeLastShown,
                                  base::Time());
  pref_registry->RegisterTimePref(prefs::kInsertRemappingNudgeLastShown,
                                  base::Time());
}

void InputDeviceSettingsControllerImpl::OnSessionStateChanged(
    session_manager::SessionState state) {
  if (state != session_manager::SessionState::OOBE &&
      last_session_ == session_manager::SessionState::OOBE) {
    RefreshCachedKeyboardSettings();

    if (!active_pref_service_) {
      last_session_ = state;
      return;
    }

    for (auto& [id, keyboard] : keyboards_) {
      if (IsSplitModifierKeyboard(*keyboard)) {
        active_pref_service_->SetBoolean(
            prefs::kKeyboardHasSplitModifierKeyboard, true);
      }
    }
  }
  last_session_ = state;
}

void InputDeviceSettingsControllerImpl::OnActiveUserPrefServiceChanged(
    PrefService* pref_service) {
  if (!features::IsMouseImposterCheckEnabled()) {
    pref_service->ClearPref(prefs::kMouseDeviceImpostersListPref);
  }

  // If the flag is disabled, clear the button remapping dictionaries.
  if (!features::IsPeripheralCustomizationEnabled()) {
    pref_service->ClearPref(
        prefs::kGraphicsTabletTabletButtonRemappingsDictPref);
    pref_service->ClearPref(prefs::kGraphicsTabletPenButtonRemappingsDictPref);
    pref_service->ClearPref(prefs::kMouseButtonRemappingsDictPref);
    DeleteLoginScreenButtonRemappingListPrefWhenPeripheralCustomizationDisabled(
        local_state_);
  }

  if (!features::IsPeripheralNotificationEnabled()) {
    pref_service->ClearPref(prefs::kPeripheralNotificationMiceSeen);
    pref_service->ClearPref(prefs::kPeripheralNotificationGraphicsTabletsSeen);
  }

  if (!features::IsWelcomeExperienceEnabled()) {
    pref_service->ClearPref(prefs::kWelcomeExperienceNotificationSeen);
    if (local_state_) {
      local_state_->ClearPref(prefs::kDeviceImagesDictPref);
    }
  }

  // If the flag is disabled, clear all the settings dictionaries.
  if (!features::IsInputDeviceSettingsSplitEnabled()) {
    active_pref_service_ = nullptr;
    pref_service->SetDict(prefs::kKeyboardDeviceSettingsDictPref, {});
    pref_service->SetDict(prefs::kMouseDeviceSettingsDictPref, {});
    pref_service->SetDict(prefs::kPointingStickDeviceSettingsDictPref, {});
    pref_service->SetDict(prefs::kTouchpadDeviceSettingsDictPref, {});
    pref_service->SetList(prefs::kKeyboardDeviceImpostersListPref, {});
    pref_service->SetList(prefs::kMouseDeviceImpostersListPref, {});

    pref_service->ClearPref(prefs::kKeyboardInternalSettings);
    pref_service->ClearPref(prefs::kKeyboardUpdateSettingsMetricInfo);
    pref_service->ClearPref(prefs::kMouseUpdateSettingsMetricInfo);
    pref_service->ClearPref(prefs::kTouchpadUpdateSettingsMetricInfo);
    pref_service->ClearPref(prefs::kPointingStickUpdateSettingsMetricInfo);

    DeleteLoginScreenSettingsPrefWhenInputDeviceSettingsSplitDisabled(
        local_state_);
    return;
  }

  // If the flag is disabled, clear the new touchpad and keyboard settings from
  // all settings dictionaries and reset the notification prefs.
  if (!features::IsAltClickAndSixPackCustomizationEnabled() && pref_service) {
    base::Value::Dict updated_touchpad_dict =
        pref_service->GetDict(prefs::kTouchpadDeviceSettingsDictPref).Clone();
    for (auto [key, dict] : updated_touchpad_dict) {
      CHECK(dict.is_dict());
      dict.GetDict().Remove(prefs::kTouchpadSettingSimulateRightClick);
    }

    base::Value::Dict updated_keyboard_dict =
        pref_service->GetDict(prefs::kKeyboardDeviceSettingsDictPref).Clone();

    for (auto [key, dict] : updated_keyboard_dict) {
      CHECK(dict.is_dict());
      dict.GetDict().Remove(prefs::kKeyboardSettingSixPackKeyRemappings);
    }
    pref_service->SetDict(prefs::kTouchpadDeviceSettingsDictPref,
                          std::move(updated_touchpad_dict));
    pref_service->SetDict(prefs::kKeyboardDeviceSettingsDictPref,
                          std::move(updated_keyboard_dict));

    // Remove six pack remappings from internal keyboard as well.
    base::Value::Dict updated_internal_keyboard_dict =
        pref_service->GetDict(prefs::kKeyboardInternalSettings).Clone();
    updated_internal_keyboard_dict.Remove(
        prefs::kKeyboardSettingSixPackKeyRemappings);
    pref_service->SetDict(prefs::kKeyboardInternalSettings,
                          std::move(updated_internal_keyboard_dict));

    pref_service->ClearPref(prefs::kRemapToRightClickNotificationsRemaining);
    pref_service->ClearPref(prefs::kSixPackKeyDeleteNotificationsRemaining);
    pref_service->ClearPref(prefs::kSixPackKeyHomeNotificationsRemaining);
    pref_service->ClearPref(prefs::kSixPackKeyEndNotificationsRemaining);
    pref_service->ClearPref(prefs::kSixPackKeyPageUpNotificationsRemaining);
    pref_service->ClearPref(prefs::kSixPackKeyPageDownNotificationsRemaining);
    pref_service->ClearPref(prefs::kSixPackKeyInsertNotificationsRemaining);
  }

  if (!features::IsModifierSplitEnabled()) {
    pref_service->ClearPref(prefs::kTopRowRemappingNudgeShownCount);
    pref_service->ClearPref(prefs::kPageUpRemappingNudgeShownCount);
    pref_service->ClearPref(prefs::kPageDownRemappingNudgeShownCount);
    pref_service->ClearPref(prefs::kHomeRemappingNudgeShownCount);
    pref_service->ClearPref(prefs::kEndRemappingNudgeShownCount);
    pref_service->ClearPref(prefs::kDeleteRemappingNudgeShownCount);
    pref_service->ClearPref(prefs::kInsertRemappingNudgeShownCount);
    pref_service->ClearPref(prefs::kCapsLockRemappingNudgeShownCount);
    pref_service->ClearPref(prefs::kTopRowRemappingNudgeLastShown);
    pref_service->ClearPref(prefs::kPageUpRemappingNudgeLastShown);
    pref_service->ClearPref(prefs::kPageDownRemappingNudgeLastShown);
    pref_service->ClearPref(prefs::kHomeRemappingNudgeLastShown);
    pref_service->ClearPref(prefs::kEndRemappingNudgeLastShown);
    pref_service->ClearPref(prefs::kDeleteRemappingNudgeLastShown);
    pref_service->ClearPref(prefs::kInsertRemappingNudgeLastShown);
    pref_service->ClearPref(prefs::kCapsLockRemappingNudgeLastShown);
  }
  active_pref_service_ = pref_service;
  active_account_id_ = Shell::Get()->session_controller()->GetActiveAccountId();
  auto* cache = apps::AppRegistryCacheWrapper::Get().GetAppRegistryCache(
      active_account_id_.value());
  app_registry_cache_observer_.Reset();
  if (cache) {
    app_registry_cache_observer_.Observe(cache);
  }
  InitializePolicyHandler();

  // Observe changes to synced prefs to ensure updates made on other devices are
  // properly reflected.
  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  if (active_pref_service_) {
    pref_change_registrar_->Init(active_pref_service_);
    pref_change_registrar_->Add(
        prefs::kPointingStickInternalSettings,
        base::BindRepeating(&InputDeviceSettingsControllerImpl::
                                RefreshInternalPointingStickSettings,
                            weak_ptr_factory_.GetWeakPtr()));
    pref_change_registrar_->Add(
        prefs::kTouchpadInternalSettings,
        base::BindRepeating(
            &InputDeviceSettingsControllerImpl::RefreshInternalTouchpadSettings,
            weak_ptr_factory_.GetWeakPtr()));
    pref_change_registrar_->Add(
        prefs::kMouseDefaultSettings,
        base::BindRepeating(&InputDeviceSettingsControllerImpl::
                                ForceInitializeDefaultMouseSettings,
                            weak_ptr_factory_.GetWeakPtr()));
    pref_change_registrar_->Add(
        prefs::kTouchpadDefaultSettings,
        base::BindRepeating(&InputDeviceSettingsControllerImpl::
                                ForceInitializeDefaultTouchpadSettings,
                            weak_ptr_factory_.GetWeakPtr()));
    pref_change_registrar_->Add(
        prefs::kKeyboardDefaultChromeOSSettings,
        base::BindRepeating(&InputDeviceSettingsControllerImpl::
                                ForceInitializeDefaultChromeOSKeyboardSettings,
                            weak_ptr_factory_.GetWeakPtr()));
    pref_change_registrar_->Add(
        prefs::kKeyboardDefaultNonChromeOSSettings,
        base::BindRepeating(
            &InputDeviceSettingsControllerImpl::
                ForceInitializeDefaultNonChromeOSKeyboardSettings,
            weak_ptr_factory_.GetWeakPtr()));
    pref_change_registrar_->Add(
        prefs::kKeyboardDefaultSplitModifierSettings,
        base::BindRepeating(
            &InputDeviceSettingsControllerImpl::
                ForceInitializeDefaultSplitModifierKeyboardSettings,
            weak_ptr_factory_.GetWeakPtr()));
    pref_change_registrar_->Add(
        prefs::kKeyboardHasSplitModifierKeyboard,
        base::BindRepeating(
            &InputDeviceSettingsControllerImpl::
                ForceInitializeDefaultSplitModifierKeyboardSettings,
            weak_ptr_factory_.GetWeakPtr()));
  }

  // Device settings must be refreshed when the user pref service is updated,
  // but all dependencies of `InputDeviceSettingsControllerImpl` must be
  // updated due to the active pref service change first. Therefore, schedule
  // a task so other dependencies are updated first.
  ScheduleDeviceSettingsRefresh();

  if (features::IsPeripheralNotificationEnabled()) {
    // Delay showing the notification for already connected devices by 30
    // seconds so on first login the user does not get bombarded by a bunch of
    // notifications until after some delay.
    sequenced_task_runner_->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&InputDeviceSettingsControllerImpl::
                           ShowFirstTimeConnectedNotifications,
                       weak_ptr_factory_.GetWeakPtr()),
        base::Seconds(30));
  }
}

void InputDeviceSettingsControllerImpl::ShowFirstTimeConnectedNotifications() {
  for (const auto& [id, mouse] : mice_) {
    notification_controller_->NotifyMouseFirstTimeConnected(*mouse);
  }
  for (const auto& [id, graphics_tablet] : graphics_tablets_) {
    notification_controller_->NotifyGraphicsTabletFirstTimeConnected(
        *graphics_tablet);
  }
}

void InputDeviceSettingsControllerImpl::
    ForceKeyboardSettingRefreshWhenFeatureEnabled() {
  sequenced_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &InputDeviceSettingsControllerImpl::RefreshMetaAndModifierKeys,
          weak_ptr_factory_.GetWeakPtr()));
  ScheduleDeviceSettingsRefresh();
}

void InputDeviceSettingsControllerImpl::ScheduleDeviceSettingsRefresh() {
  if (settings_refresh_pending_) {
    return;
  }

  // Modifiers must be refreshed before settings so settings are retrieved for
  // the correct modifiers.
  if (features::IsModifierSplitDogfoodEnabled() &&
      Shell::Get()->keyboard_capability()->IsModifierSplitEnabled()) {
    sequenced_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &InputDeviceSettingsControllerImpl::RefreshMetaAndModifierKeys,
            weak_ptr_factory_.GetWeakPtr()));
  }

  settings_refresh_pending_ = true;
  sequenced_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &InputDeviceSettingsControllerImpl::RefreshAllDeviceSettings,
          weak_ptr_factory_.GetWeakPtr()));
}

void InputDeviceSettingsControllerImpl::RefreshAllDeviceSettings() {
  settings_refresh_pending_ = false;
  for (const auto& [id, keyboard] : keyboards_) {
    InitializeKeyboardSettings(keyboard.get());
    DispatchKeyboardSettingsChanged(id);
  }
  for (const auto& [id, touchpad] : touchpads_) {
    InitializeTouchpadSettings(touchpad.get());
    DispatchTouchpadSettingsChanged(id);
  }
  for (const auto& [id, mouse] : mice_) {
    InitializeMouseSettings(mouse.get());
    DispatchMouseSettingsChanged(id);
  }
  for (const auto& [id, pointing_stick] : pointing_sticks_) {
    InitializePointingStickSettings(pointing_stick.get());
    DispatchPointingStickSettingsChanged(id);
  }

  RefreshCachedKeyboardSettings();
  RefreshCachedMouseSettings();
  RefreshCachedTouchpadSettings();
  RefreshStoredLoginScreenPointingStickSettings();

  if (features::IsPeripheralCustomizationEnabled()) {
    for (const auto& [id, graphics_tablet] : graphics_tablets_) {
      InitializeGraphicsTabletSettings(graphics_tablet.get());
      DispatchGraphicsTabletSettingsChanged(id);
    }
    RefreshStoredLoginScreenGraphicsTabletSettings();
  }
}

void InputDeviceSettingsControllerImpl::
    RefreshStoredLoginScreenKeyboardSettings() {
  if (!local_state_ || !active_account_id_.has_value()) {
    return;
  }

  // Our map of keyboards is sorted so iterating in reverse order guarantees
  // that we'll select the most recently connected device.
  auto external_iter = base::ranges::find(
      keyboards_.rbegin(), keyboards_.rend(), /*value=*/true,
      [](const auto& keyboard) { return keyboard.second->is_external; });
  auto internal_iter = base::ranges::find(
      keyboards_.rbegin(), keyboards_.rend(), /*value=*/false,
      [](const auto& keyboard) { return keyboard.second->is_external; });

  if (external_iter != keyboards_.rend()) {
    auto& external_keyboard = *external_iter->second;
    keyboard_pref_handler_->UpdateLoginScreenKeyboardSettings(
        local_state_, active_account_id_.value(),
        policy_handler_->keyboard_policies(), external_keyboard);
    PR_LOG(INFO, Feature::IDS) << GetKeyboardSettingsLog(
        "Login screen external keyboard default settings updated",
        external_keyboard);
  }

  if (internal_iter != keyboards_.rend()) {
    auto& internal_keyboard = *internal_iter->second;
    keyboard_pref_handler_->UpdateLoginScreenKeyboardSettings(
        local_state_, active_account_id_.value(),
        policy_handler_->keyboard_policies(), internal_keyboard);
    PR_LOG(INFO, Feature::IDS) << GetKeyboardSettingsLog(
        "Login screen internal keyboard default settings updated",
        internal_keyboard);
  }
}

void InputDeviceSettingsControllerImpl::
    RefreshStoredLoginScreenMouseSettings() {
  if (!local_state_ || !active_account_id_.has_value()) {
    return;
  }

  // Our map of mice is sorted so iterating in reverse order guarantees
  // that we'll select the most recently connected device.
  auto external_iter = base::ranges::find(
      mice_.rbegin(), mice_.rend(), /*value=*/true,
      [](const auto& mouse) { return mouse.second->is_external; });
  auto internal_iter = base::ranges::find(
      mice_.rbegin(), mice_.rend(), /*value=*/false,
      [](const auto& mouse) { return mouse.second->is_external; });

  if (external_iter != mice_.rend()) {
    auto& external_mouse = *external_iter->second;
    mouse_pref_handler_->UpdateLoginScreenMouseSettings(
        local_state_, active_account_id_.value(),
        policy_handler_->mouse_policies(), external_mouse);
    PR_LOG(INFO, Feature::IDS) << GetMouseSettingsLog(
        "Login screen external mouse default settings updated", external_mouse);
  }

  if (internal_iter != mice_.rend()) {
    auto& internal_mouse = *internal_iter->second;
    mouse_pref_handler_->UpdateLoginScreenMouseSettings(
        local_state_, active_account_id_.value(),
        policy_handler_->mouse_policies(), internal_mouse);
    PR_LOG(INFO, Feature::IDS) << GetMouseSettingsLog(
        "Login screen internal mouse default settings updated", internal_mouse);
  }
}

void InputDeviceSettingsControllerImpl::
    RefreshStoredLoginScreenPointingStickSettings() {
  if (!local_state_ || !active_account_id_.has_value()) {
    return;
  }

  // Our map of pointing sticks is sorted so iterating in reverse order
  // guarantees that we'll select the most recently connected device.
  auto external_iter =
      base::ranges::find(pointing_sticks_.rbegin(), pointing_sticks_.rend(),
                         /*value=*/true, [](const auto& pointing_stick) {
                           return pointing_stick.second->is_external;
                         });
  auto internal_iter =
      base::ranges::find(pointing_sticks_.rbegin(), pointing_sticks_.rend(),
                         /*value=*/false, [](const auto& pointing_stick) {
                           return pointing_stick.second->is_external;
                         });

  if (external_iter != pointing_sticks_.rend()) {
    auto& external_pointing_stick = *external_iter->second;
    pointing_stick_pref_handler_->UpdateLoginScreenPointingStickSettings(
        local_state_, active_account_id_.value(), external_pointing_stick);
    PR_LOG(INFO, Feature::IDS) << GetPointingStickSettingsLog(
        "Login screen external pointing stick default settings updated",
        external_pointing_stick);
  }

  if (internal_iter != pointing_sticks_.rend()) {
    auto& internal_pointing_stick = *internal_iter->second;
    pointing_stick_pref_handler_->UpdateLoginScreenPointingStickSettings(
        local_state_, active_account_id_.value(), internal_pointing_stick);
    PR_LOG(INFO, Feature::IDS) << GetPointingStickSettingsLog(
        "Login screen internal pointing stick default settings updated",
        internal_pointing_stick);
  }
}

void InputDeviceSettingsControllerImpl::
    RefreshStoredLoginScreenTouchpadSettings() {
  if (!local_state_ || !active_account_id_.has_value()) {
    return;
  }

  // Our map of touchpads is sorted so iterating in reverse order guarantees
  // that we'll select the most recently connected device.
  auto external_iter = base::ranges::find(
      touchpads_.rbegin(), touchpads_.rend(), /*value=*/true,
      [](const auto& touchpad) { return touchpad.second->is_external; });
  auto internal_iter = base::ranges::find(
      touchpads_.rbegin(), touchpads_.rend(), /*value=*/false,
      [](const auto& touchpad) { return touchpad.second->is_external; });

  if (external_iter != touchpads_.rend()) {
    auto& external_touchpad = *external_iter->second;
    touchpad_pref_handler_->UpdateLoginScreenTouchpadSettings(
        local_state_, active_account_id_.value(), external_touchpad);
    PR_LOG(INFO, Feature::IDS) << GetTouchpadSettingsLog(
        "Login screen external touchpad default settings updated",
        external_touchpad);
  }

  if (internal_iter != touchpads_.rend()) {
    auto& internal_touchpad = *internal_iter->second;
    touchpad_pref_handler_->UpdateLoginScreenTouchpadSettings(
        local_state_, active_account_id_.value(), internal_touchpad);
    PR_LOG(INFO, Feature::IDS) << GetTouchpadSettingsLog(
        "Login screen internal touchpad default settings updated",
        internal_touchpad);
  }
}

void InputDeviceSettingsControllerImpl::
    RefreshStoredLoginScreenGraphicsTabletSettings() {
  if (!local_state_ || !active_account_id_.has_value()) {
    return;
  }
  if (graphics_tablets_.size() == 0) {
    return;
  }
  graphics_tablet_pref_handler_->UpdateLoginScreenGraphicsTabletSettings(
      local_state_, active_account_id_.value(),
      *graphics_tablets_.rbegin()->second);
  PR_LOG(INFO, Feature::IDS)
      << GetGraphicsTabletSettingsLog("Login screen default settings updated",
                                      *graphics_tablets_.rbegin()->second);
}

void InputDeviceSettingsControllerImpl::OnLoginScreenFocusedPodChanged(
    const AccountId& account_id) {
  active_account_id_ = account_id;

  for (const auto& [id, keyboard] : keyboards_) {
    keyboard_pref_handler_->InitializeLoginScreenKeyboardSettings(
        local_state_, account_id, policy_handler_->keyboard_policies(),
        keyboard.get());
    PR_LOG(INFO, Feature::IDS) << GetKeyboardSettingsLog(
        "Login screen default settings initialized", *keyboard.get());
    DispatchKeyboardSettingsChanged(id);
  }

  for (const auto& [id, mouse] : mice_) {
    mouse_pref_handler_->InitializeLoginScreenMouseSettings(
        local_state_, account_id, policy_handler_->mouse_policies(),
        mouse.get());
    PR_LOG(INFO, Feature::IDS) << GetMouseSettingsLog(
        "Login screen default settings initialized", *mouse.get());
    DispatchMouseSettingsChanged(id);
  }

  for (const auto& [id, pointing_stick] : pointing_sticks_) {
    pointing_stick_pref_handler_->InitializeLoginScreenPointingStickSettings(
        local_state_, account_id, pointing_stick.get());
    PR_LOG(INFO, Feature::IDS) << GetPointingStickSettingsLog(
        "Login screen default settings initialized", *pointing_stick.get());
    DispatchPointingStickSettingsChanged(id);
  }

  for (const auto& [id, touchpad] : touchpads_) {
    touchpad_pref_handler_->InitializeLoginScreenTouchpadSettings(
        local_state_, account_id, touchpad.get());
    PR_LOG(INFO, Feature::IDS) << GetTouchpadSettingsLog(
        "Login screen default settings initialized", *touchpad.get());
    DispatchTouchpadSettingsChanged(id);
  }

  if (features::IsPeripheralCustomizationEnabled()) {
    for (const auto& [id, graphics_tablet] : graphics_tablets_) {
      graphics_tablet_pref_handler_
          ->InitializeLoginScreenGraphicsTabletSettings(
              local_state_, account_id, graphics_tablet.get());
      PR_LOG(INFO, Feature::IDS) << GetGraphicsTabletSettingsLog(
          "Login screen default settings initialized", *graphics_tablet.get());
      DispatchGraphicsTabletSettingsChanged(id);
    }
  }
}

void InputDeviceSettingsControllerImpl::OnKeyboardPoliciesChanged() {
  for (auto& observer : observers_) {
    observer.OnKeyboardPoliciesUpdated(policy_handler_->keyboard_policies());
  }
  ScheduleDeviceSettingsRefresh();
}

void InputDeviceSettingsControllerImpl::OnMousePoliciesChanged() {
  for (const auto& [id, mouse] : mice_) {
    InitializeMouseSettings(mouse.get());
    PR_LOG(INFO, Feature::IDS)
        << GetMouseSettingsLog("Default settings initialized", *mouse.get());
    DispatchMouseSettingsChanged(id);
  }

  for (auto& observer : observers_) {
    observer.OnMousePoliciesUpdated(policy_handler_->mouse_policies());
  }
}

const mojom::KeyboardPolicies&
InputDeviceSettingsControllerImpl::GetKeyboardPolicies() {
  CHECK(policy_handler_);
  return policy_handler_->keyboard_policies();
}

const mojom::MousePolicies&
InputDeviceSettingsControllerImpl::GetMousePolicies() {
  CHECK(policy_handler_);
  return policy_handler_->mouse_policies();
}

const mojom::KeyboardSettings*
InputDeviceSettingsControllerImpl::GetKeyboardSettings(DeviceId id) {
  const auto* keyboard = FindKeyboard(id);
  return keyboard ? keyboard->settings.get() : nullptr;
}

const mojom::MouseSettings* InputDeviceSettingsControllerImpl::GetMouseSettings(
    DeviceId id) {
  const auto* mouse = FindMouse(id);
  return mouse ? mouse->settings.get() : nullptr;
}

const mojom::TouchpadSettings*
InputDeviceSettingsControllerImpl::GetTouchpadSettings(DeviceId id) {
  const auto* touchpad = FindTouchpad(id);
  return touchpad ? touchpad->settings.get() : nullptr;
}

const mojom::PointingStickSettings*
InputDeviceSettingsControllerImpl::GetPointingStickSettings(DeviceId id) {
  const auto* pointing_stick = FindPointingStick(id);
  return pointing_stick ? pointing_stick->settings.get() : nullptr;
}

const mojom::GraphicsTabletSettings*
InputDeviceSettingsControllerImpl::GetGraphicsTabletSettings(DeviceId id) {
  const auto* graphics_tablet = FindGraphicsTablet(id);
  return graphics_tablet ? graphics_tablet->settings.get() : nullptr;
}

std::vector<mojom::KeyboardPtr>
InputDeviceSettingsControllerImpl::GetConnectedKeyboards() {
  std::vector<mojom::KeyboardPtr> keyboard_vector;
  keyboard_vector.reserve(keyboards_.size());

  for (const auto& [_, keyboard] : keyboards_) {
    keyboard_vector.push_back(keyboard->Clone());
  }

  return keyboard_vector;
}

std::vector<mojom::TouchpadPtr>
InputDeviceSettingsControllerImpl::GetConnectedTouchpads() {
  std::vector<mojom::TouchpadPtr> mouse_vector;
  mouse_vector.reserve(touchpads_.size());

  for (const auto& [_, touchpad] : touchpads_) {
    mouse_vector.push_back(touchpad->Clone());
  }

  return mouse_vector;
}

std::vector<mojom::MousePtr>
InputDeviceSettingsControllerImpl::GetConnectedMice() {
  std::vector<mojom::MousePtr> mouse_vector;
  mouse_vector.reserve(mice_.size());

  for (const auto& [_, mouse] : mice_) {
    mouse_vector.push_back(mouse->Clone());
  }

  return mouse_vector;
}

std::vector<mojom::PointingStickPtr>
InputDeviceSettingsControllerImpl::GetConnectedPointingSticks() {
  std::vector<mojom::PointingStickPtr> pointing_stick_vector;
  pointing_stick_vector.reserve(pointing_sticks_.size());

  for (const auto& [_, pointing_stick] : pointing_sticks_) {
    pointing_stick_vector.push_back(pointing_stick->Clone());
  }

  return pointing_stick_vector;
}

std::vector<mojom::GraphicsTabletPtr>
InputDeviceSettingsControllerImpl::GetConnectedGraphicsTablets() {
  std::vector<mojom::GraphicsTabletPtr> graphics_tablet_vector;
  graphics_tablet_vector.reserve(graphics_tablets_.size());

  for (const auto& [_, graphics_tablet] : graphics_tablets_) {
    graphics_tablet_vector.push_back(graphics_tablet->Clone());
  }

  return graphics_tablet_vector;
}

bool InputDeviceSettingsControllerImpl::SetKeyboardSettings(
    DeviceId id,
    mojom::KeyboardSettingsPtr settings) {
  DCHECK(active_pref_service_);

  // If a device with the given id does not exist, do nothing.
  auto found_keyboard_iter = keyboards_.find(id);
  if (found_keyboard_iter == keyboards_.end()) {
    RecordSetKeyboardSettingsValidMetric(/*is_valid=*/false);
    return false;
  }
  auto& found_keyboard = *found_keyboard_iter->second;

  if (!KeyboardSettingsAreValid(found_keyboard, *settings,
                                policy_handler_->keyboard_policies())) {
    RecordSetKeyboardSettingsValidMetric(/*is_valid=*/false);
    return false;
  }
  RecordSetKeyboardSettingsValidMetric(/*is_valid=*/true);
  auto it =
      welcome_notification_clicked_device_keys_.find(found_keyboard.device_key);
  if (it != welcome_notification_clicked_device_keys_.end()) {
    base::UmaHistogramEnumeration(
        "ChromeOS.WelcomeExperienceNotificationEvent",
        InputDeviceSettingsMetricsManager::
            WelcomeExperienceNotificationEventType::kSettingChanged);
    welcome_notification_clicked_device_keys_.erase(it);
  }
  const auto old_settings = std::move(found_keyboard.settings);
  found_keyboard.settings = settings.Clone();
  keyboard_pref_handler_->UpdateKeyboardSettings(
      active_pref_service_, policy_handler_->keyboard_policies(),
      found_keyboard);
  PR_LOG(INFO, Feature::IDS)
      << GetKeyboardSettingsLog("Updated", found_keyboard);
  metrics_manager_->RecordKeyboardChangedMetrics(found_keyboard, *old_settings);
  DispatchKeyboardSettingsChanged(id);

  UpdateDuplicateDeviceSettings(
      found_keyboard, keyboards_,
      base::BindRepeating(
          &InputDeviceSettingsControllerImpl::DispatchKeyboardSettingsChanged,
          base::Unretained(this)));
  RefreshCachedKeyboardSettings();
  return true;
}

bool InputDeviceSettingsControllerImpl::SetTouchpadSettings(
    DeviceId id,
    mojom::TouchpadSettingsPtr settings) {
  DCHECK(active_pref_service_);

  // If a device with the given id does not exist, do nothing.
  auto found_touchpad_iter = touchpads_.find(id);
  if (found_touchpad_iter == touchpads_.end()) {
    RecordSetTouchpadSettingsValidMetric(/*is_valid=*/false);
    return false;
  }

  auto& found_touchpad = *found_touchpad_iter->second;
  if (!TouchpadSettingsAreValid(found_touchpad, *settings)) {
    RecordSetTouchpadSettingsValidMetric(/*is_valid=*/false);
    return false;
  }
  RecordSetTouchpadSettingsValidMetric(/*is_valid=*/true);
  auto it =
      welcome_notification_clicked_device_keys_.find(found_touchpad.device_key);
  if (it != welcome_notification_clicked_device_keys_.end()) {
    base::UmaHistogramEnumeration(
        "ChromeOS.WelcomeExperienceNotificationEvent",
        InputDeviceSettingsMetricsManager::
            WelcomeExperienceNotificationEventType::kSettingChanged);
    welcome_notification_clicked_device_keys_.erase(it);
  }
  const auto old_settings = std::move(found_touchpad.settings);
  found_touchpad.settings = settings.Clone();
  touchpad_pref_handler_->UpdateTouchpadSettings(active_pref_service_,
                                                 found_touchpad);
  PR_LOG(INFO, Feature::IDS)
      << GetTouchpadSettingsLog("Updated", found_touchpad);
  metrics_manager_->RecordTouchpadChangedMetrics(found_touchpad, *old_settings);
  DispatchTouchpadSettingsChanged(id);

  UpdateDuplicateDeviceSettings(
      found_touchpad, touchpads_,
      base::BindRepeating(
          &InputDeviceSettingsControllerImpl::DispatchTouchpadSettingsChanged,
          base::Unretained(this)));
  RefreshCachedTouchpadSettings();
  return true;
}

bool InputDeviceSettingsControllerImpl::SetMouseSettings(
    DeviceId id,
    mojom::MouseSettingsPtr settings) {
  DCHECK(active_pref_service_);

  // If a device with the given id does not exist, do nothing.
  auto found_mouse_iter = mice_.find(id);
  if (found_mouse_iter == mice_.end()) {
    RecordSetMouseSettingsValidMetric(/*is_valid=*/false);
    return false;
  }

  auto& found_mouse = *found_mouse_iter->second;
  if (!MouseSettingsAreValid(found_mouse, *settings)) {
    RecordSetMouseSettingsValidMetric(/*is_valid=*/false);
    return false;
  }
  RecordSetMouseSettingsValidMetric(/*is_valid=*/true);
  auto it =
      welcome_notification_clicked_device_keys_.find(found_mouse.device_key);
  if (it != welcome_notification_clicked_device_keys_.end()) {
    base::UmaHistogramEnumeration(
        "ChromeOS.WelcomeExperienceNotificationEvent",
        InputDeviceSettingsMetricsManager::
            WelcomeExperienceNotificationEventType::kSettingChanged);
    welcome_notification_clicked_device_keys_.erase(it);
  }
  const auto old_settings = std::move(found_mouse.settings);
  found_mouse.settings = settings.Clone();
  mouse_pref_handler_->UpdateMouseSettings(
      active_pref_service_, policy_handler_->mouse_policies(), found_mouse);
  PR_LOG(INFO, Feature::IDS) << GetMouseSettingsLog("Updated", found_mouse);
  metrics_manager_->RecordMouseChangedMetrics(found_mouse, *old_settings);
  DispatchMouseSettingsChanged(id);

  UpdateDuplicateDeviceSettings(
      found_mouse, mice_,
      base::BindRepeating(
          &InputDeviceSettingsControllerImpl::DispatchMouseSettingsChanged,
          base::Unretained(this)));
  RefreshCachedMouseSettings();
  return true;
}

bool InputDeviceSettingsControllerImpl::SetPointingStickSettings(
    DeviceId id,
    mojom::PointingStickSettingsPtr settings) {
  DCHECK(active_pref_service_);

  // If a device with the given id does not exist, do nothing.
  auto found_pointing_stick_iter = pointing_sticks_.find(id);
  if (found_pointing_stick_iter == pointing_sticks_.end()) {
    RecordSetPointingStickSettingsValidMetric(/*is_valid=*/false);
    return false;
  }
  RecordSetPointingStickSettingsValidMetric(/*is_valid=*/true);

  auto& found_pointing_stick = *found_pointing_stick_iter->second;
  const auto old_settings = std::move(found_pointing_stick.settings);
  found_pointing_stick.settings = settings.Clone();
  pointing_stick_pref_handler_->UpdatePointingStickSettings(
      active_pref_service_, found_pointing_stick);
  PR_LOG(INFO, Feature::IDS)
      << GetPointingStickSettingsLog("Updated", found_pointing_stick);
  metrics_manager_->RecordPointingStickChangedMetrics(found_pointing_stick,
                                                      *old_settings);
  DispatchPointingStickSettingsChanged(id);
  // Check the list of pointing sticks to see if any have the same
  // |device_key|. If so, their settings need to also be updated.
  for (const auto& [device_id, pointing_stick] : pointing_sticks_) {
    if (device_id != found_pointing_stick.id &&
        pointing_stick->device_key == found_pointing_stick.device_key) {
      pointing_stick->settings = settings->Clone();
      DispatchPointingStickSettingsChanged(device_id);
    }
  }

  RefreshStoredLoginScreenPointingStickSettings();
  return true;
}

bool InputDeviceSettingsControllerImpl::SetGraphicsTabletSettings(
    DeviceId id,
    mojom::GraphicsTabletSettingsPtr settings) {
  DCHECK(active_pref_service_);

  // If a device with the given id does not exist, do nothing.
  auto found_graphics_tablet_iter = graphics_tablets_.find(id);
  if (found_graphics_tablet_iter == graphics_tablets_.end()) {
    return false;
  }

  auto& found_graphics_tablet = *found_graphics_tablet_iter->second;
  if (!GraphicsTabletSettingsAreValid(found_graphics_tablet, *settings)) {
    return false;
  }

  auto it = welcome_notification_clicked_device_keys_.find(
      found_graphics_tablet.device_key);
  if (it != welcome_notification_clicked_device_keys_.end()) {
    base::UmaHistogramEnumeration(
        "ChromeOS.WelcomeExperienceNotificationEvent",
        InputDeviceSettingsMetricsManager::
            WelcomeExperienceNotificationEventType::kSettingChanged);
    welcome_notification_clicked_device_keys_.erase(it);
  }
  const auto old_settings = std::move(found_graphics_tablet.settings);
  found_graphics_tablet.settings = settings.Clone();
  graphics_tablet_pref_handler_->UpdateGraphicsTabletSettings(
      active_pref_service_, found_graphics_tablet);
  PR_LOG(INFO, Feature::IDS)
      << GetGraphicsTabletSettingsLog("Updated", found_graphics_tablet);
  metrics_manager_->RecordGraphicsTabletChangedMetrics(found_graphics_tablet,
                                                       *old_settings);
  DispatchGraphicsTabletSettingsChanged(id);

  UpdateDuplicateDeviceSettings(
      found_graphics_tablet, graphics_tablets_,
      base::BindRepeating(&InputDeviceSettingsControllerImpl::
                              DispatchGraphicsTabletSettingsChanged,
                          base::Unretained(this)));
  RefreshStoredLoginScreenGraphicsTabletSettings();
  return true;
}

void InputDeviceSettingsControllerImpl::AddObserver(
    InputDeviceSettingsController::Observer* observer) {
  observers_.AddObserver(observer);
}

void InputDeviceSettingsControllerImpl::RemoveObserver(
    InputDeviceSettingsController::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void InputDeviceSettingsControllerImpl::RecordComboDeviceMetric(
    const mojom::Keyboard& keyboard) {
  for (const auto& [_, mouse] : mice_) {
    if (mouse->device_key == keyboard.device_key) {
      metrics_manager_->RecordKeyboardMouseComboDeviceMetric(keyboard, *mouse);
    }
  }
}

void InputDeviceSettingsControllerImpl::RecordComboDeviceMetric(
    const mojom::Mouse& mouse) {
  for (const auto& [_, keyboard] : keyboards_) {
    if (keyboard->device_key == mouse.device_key) {
      metrics_manager_->RecordKeyboardMouseComboDeviceMetric(*keyboard, mouse);
    }
  }
}

void InputDeviceSettingsControllerImpl::DispatchKeyboardConnected(DeviceId id) {
  DCHECK(base::Contains(keyboards_, id));
  const auto& keyboard = *keyboards_.at(id);
  for (auto& observer : observers_) {
    observer.OnKeyboardConnected(keyboard);
  }
  RecordComboDeviceMetric(keyboard);
}

void InputDeviceSettingsControllerImpl::
    DispatchKeyboardDisconnectedAndEraseFromList(DeviceId id) {
  DCHECK(base::Contains(keyboards_, id));
  auto keyboard_iter = keyboards_.find(id);
  auto keyboard = std::move(keyboard_iter->second);
  keyboards_.erase(keyboard_iter);
  for (auto& observer : observers_) {
    observer.OnKeyboardDisconnected(*keyboard);
  }
}

void InputDeviceSettingsControllerImpl::DispatchKeyboardSettingsChanged(
    DeviceId id) {
  DCHECK(base::Contains(keyboards_, id));
  const auto& keyboard = *keyboards_.at(id);
  for (auto& observer : observers_) {
    observer.OnKeyboardSettingsUpdated(keyboard);
  }
}

void InputDeviceSettingsControllerImpl::DispatchTouchpadConnected(DeviceId id) {
  DCHECK(base::Contains(touchpads_, id));
  const auto& touchpad = *touchpads_.at(id);
  for (auto& observer : observers_) {
    observer.OnTouchpadConnected(touchpad);
  }
}

void InputDeviceSettingsControllerImpl::
    DispatchTouchpadDisconnectedAndEraseFromList(DeviceId id) {
  DCHECK(base::Contains(touchpads_, id));
  auto touchpad_iter = touchpads_.find(id);
  auto touchpad = std::move(touchpad_iter->second);
  touchpads_.erase(touchpad_iter);
  for (auto& observer : observers_) {
    observer.OnTouchpadDisconnected(*touchpad);
  }
}

void InputDeviceSettingsControllerImpl::DispatchTouchpadSettingsChanged(
    DeviceId id) {
  DCHECK(base::Contains(touchpads_, id));
  const auto& touchpad = *touchpads_.at(id);
  for (auto& observer : observers_) {
    observer.OnTouchpadSettingsUpdated(touchpad);
  }
}

void InputDeviceSettingsControllerImpl::DispatchMouseConnected(DeviceId id) {
  DCHECK(base::Contains(mice_, id));
  const auto& mouse = *mice_.at(id);
  for (auto& observer : observers_) {
    observer.OnMouseConnected(mouse);
  }
  RecordComboDeviceMetric(mouse);
}

void InputDeviceSettingsControllerImpl::
    DispatchMouseDisconnectedAndEraseFromList(DeviceId id) {
  DCHECK(base::Contains(mice_, id));
  auto mouse_iter = mice_.find(id);
  auto mouse = std::move(mouse_iter->second);
  mice_.erase(mouse_iter);
  for (auto& observer : observers_) {
    observer.OnMouseDisconnected(*mouse);
  }
}

void InputDeviceSettingsControllerImpl::DispatchMouseSettingsChanged(
    DeviceId id) {
  DCHECK(base::Contains(mice_, id));
  const auto& mouse = *mice_.at(id);
  for (auto& observer : observers_) {
    observer.OnMouseSettingsUpdated(mouse);
  }
}

void InputDeviceSettingsControllerImpl::DispatchPointingStickConnected(
    DeviceId id) {
  DCHECK(base::Contains(pointing_sticks_, id));
  const auto& pointing_stick = *pointing_sticks_.at(id);
  for (auto& observer : observers_) {
    observer.OnPointingStickConnected(pointing_stick);
  }
}

void InputDeviceSettingsControllerImpl::
    DispatchPointingStickDisconnectedAndEraseFromList(DeviceId id) {
  DCHECK(base::Contains(pointing_sticks_, id));
  auto pointing_stick_iter = pointing_sticks_.find(id);
  auto pointing_stick = std::move(pointing_stick_iter->second);
  pointing_sticks_.erase(pointing_stick_iter);
  for (auto& observer : observers_) {
    observer.OnPointingStickDisconnected(*pointing_stick);
  }
}

void InputDeviceSettingsControllerImpl::DispatchPointingStickSettingsChanged(
    DeviceId id) {
  DCHECK(base::Contains(pointing_sticks_, id));
  const auto& pointing_stick = *pointing_sticks_.at(id);
  for (auto& observer : observers_) {
    observer.OnPointingStickSettingsUpdated(pointing_stick);
  }
}

void InputDeviceSettingsControllerImpl::DispatchGraphicsTabletConnected(
    DeviceId id) {
  DCHECK(base::Contains(graphics_tablets_, id));
  const auto& graphics_tablet = *graphics_tablets_.at(id);
  for (auto& observer : observers_) {
    observer.OnGraphicsTabletConnected(graphics_tablet);
  }
}

void InputDeviceSettingsControllerImpl::
    DispatchGraphicsTabletDisconnectedAndEraseFromList(DeviceId id) {
  DCHECK(base::Contains(graphics_tablets_, id));
  auto graphics_tablet_iter = graphics_tablets_.find(id);
  auto graphics_tablet = std::move(graphics_tablet_iter->second);
  graphics_tablets_.erase(graphics_tablet_iter);
  for (auto& observer : observers_) {
    observer.OnGraphicsTabletDisconnected(*graphics_tablet);
  }
}

void InputDeviceSettingsControllerImpl::DispatchGraphicsTabletSettingsChanged(
    DeviceId id) {
  DCHECK(base::Contains(graphics_tablets_, id));
  const auto& graphics_tablet = *graphics_tablets_.at(id);
  for (auto& observer : observers_) {
    observer.OnGraphicsTabletSettingsUpdated(graphics_tablet);
  }
}

std::string GetMouseName(const ui::InputDevice& mouse) {
  const auto* mouse_metadata = GetMouseMetadata(mouse);
  if (!features::IsWelcomeExperienceEnabled() || !mouse_metadata ||
      !mouse_metadata->name.has_value()) {
    return mouse.name;
  }
  return mouse_metadata->name.value();
}

mojom::CustomizationRestriction
InputDeviceSettingsControllerImpl::GetMouseCustomizationRestriction(
    const ui::InputDevice& mouse) {
  const auto* mouse_metadata = GetMouseMetadata(mouse);
  if (mouse_metadata) {
    return mouse_metadata->customization_restriction;
  }
  const auto* keyboard_mouse_combo_metadata =
      GetKeyboardMouseComboMetadata(mouse);
  if (keyboard_mouse_combo_metadata) {
    return keyboard_mouse_combo_metadata->customization_restriction;
  }

  return mojom::CustomizationRestriction::kDisableKeyEventRewrites;
}

mojom::CustomizationRestriction
InputDeviceSettingsControllerImpl::GetGraphicsTabletCustomizationRestriction(
    const ui::InputDevice& graphics_tablet) {
  const auto* graphics_tablet_metadata =
      GetGraphicsTabletMetadata(graphics_tablet);
  if (graphics_tablet_metadata) {
    return graphics_tablet_metadata->customization_restriction;
  }

  return mojom::CustomizationRestriction::kAllowCustomizations;
}

mojom::MouseButtonConfig
InputDeviceSettingsControllerImpl::GetMouseButtonConfig(
    const ui::InputDevice& mouse) {
  const auto* mouse_metadata = GetMouseMetadata(mouse);
  if (mouse_metadata) {
    return mouse_metadata->mouse_button_config;
  }

  return mojom::MouseButtonConfig::kNoConfig;
}

mojom::GraphicsTabletButtonConfig
InputDeviceSettingsControllerImpl::GetGraphicsTabletButtonConfig(
    const ui::InputDevice& graphics_tablet) {
  const auto* graphics_tablet_metadata =
      GetGraphicsTabletMetadata(graphics_tablet);
  if (graphics_tablet_metadata) {
    return graphics_tablet_metadata->graphics_tablet_button_config;
  }

  return mojom::GraphicsTabletButtonConfig::kNoConfig;
}

void InputDeviceSettingsControllerImpl::OnKeyboardListUpdated(
    std::vector<ui::KeyboardDevice> keyboards_to_add,
    std::vector<DeviceId> keyboard_ids_to_remove) {
  for (const auto& keyboard : keyboards_to_add) {
    // Get initial settings from the pref manager and generate our local
    // storage of the device.
    auto mojom_keyboard = BuildMojomKeyboard(keyboard);
    InitializeKeyboardSettings(mojom_keyboard.get());
    if (ShouldFetchDeviceImage()) {
      GetDeviceImage(mojom_keyboard->device_key, mojom_keyboard->id);
    }
    keyboards_.insert_or_assign(keyboard.id, std::move(mojom_keyboard));
    DispatchKeyboardConnected(keyboard.id);
  }

  for (const auto id : keyboard_ids_to_remove) {
    DispatchKeyboardDisconnectedAndEraseFromList(id);
  }

  RefreshCachedKeyboardSettings();
  RefreshBatteryInfoForConnectedDevices();
  RefreshCompanionAppInfoForConnectedDevices();
}

void InputDeviceSettingsControllerImpl::OnTouchpadListUpdated(
    std::vector<ui::TouchpadDevice> touchpads_to_add,
    std::vector<DeviceId> touchpad_ids_to_remove) {
  for (const auto& touchpad : touchpads_to_add) {
    auto mojom_touchpad = BuildMojomTouchpad(touchpad);
    InitializeTouchpadSettings(mojom_touchpad.get());
    if (ShouldFetchDeviceImage()) {
      GetDeviceImage(mojom_touchpad->device_key, mojom_touchpad->id);
    }
    touchpads_.insert_or_assign(touchpad.id, std::move(mojom_touchpad));
    DispatchTouchpadConnected(touchpad.id);
  }

  for (const auto id : touchpad_ids_to_remove) {
    DispatchTouchpadDisconnectedAndEraseFromList(id);
  }

  RefreshCachedTouchpadSettings();
  RefreshBatteryInfoForConnectedDevices();
  RefreshCompanionAppInfoForConnectedDevices();
}

void InputDeviceSettingsControllerImpl::OnMouseListUpdated(
    std::vector<ui::InputDevice> mice_to_add,
    std::vector<DeviceId> mouse_ids_to_remove) {
  for (const auto& mouse : mice_to_add) {
    auto mojom_mouse =
        BuildMojomMouse(mouse, GetMouseCustomizationRestriction(mouse),
                        GetMouseButtonConfig(mouse), GetMouseName(mouse));
    InitializeMouseSettings(mojom_mouse.get());
    if (ShouldFetchDeviceImage()) {
      GetDeviceImage(mojom_mouse->device_key, mojom_mouse->id);
    } else if (features::IsPeripheralNotificationEnabled()) {
      notification_controller_->NotifyMouseFirstTimeConnected(*mojom_mouse);
    }

    mice_.insert_or_assign(mouse.id, std::move(mojom_mouse));
    DispatchMouseConnected(mouse.id);
  }

  for (const auto id : mouse_ids_to_remove) {
    DispatchMouseDisconnectedAndEraseFromList(id);
  }

  RefreshCachedMouseSettings();
  RefreshBatteryInfoForConnectedDevices();
  RefreshCompanionAppInfoForConnectedDevices();
}

void InputDeviceSettingsControllerImpl::OnPointingStickListUpdated(
    std::vector<ui::InputDevice> pointing_sticks_to_add,
    std::vector<DeviceId> pointing_stick_ids_to_remove) {
  for (const auto& pointing_stick : pointing_sticks_to_add) {
    auto mojom_pointing_stick = BuildMojomPointingStick(pointing_stick);
    InitializePointingStickSettings(mojom_pointing_stick.get());
    pointing_sticks_.insert_or_assign(pointing_stick.id,
                                      std::move(mojom_pointing_stick));
    DispatchPointingStickConnected(pointing_stick.id);
  }

  for (const auto id : pointing_stick_ids_to_remove) {
    DispatchPointingStickDisconnectedAndEraseFromList(id);
  }

  RefreshStoredLoginScreenPointingStickSettings();
}

void InputDeviceSettingsControllerImpl::OnGraphicsTabletListUpdated(
    std::vector<ui::InputDevice> graphics_tablets_to_add,
    std::vector<DeviceId> graphics_tablet_ids_to_remove) {
  for (const auto& graphics_tablet : graphics_tablets_to_add) {
    auto mojom_graphics_tablet = BuildMojomGraphicsTablet(
        graphics_tablet,
        GetGraphicsTabletCustomizationRestriction(graphics_tablet),
        GetGraphicsTabletButtonConfig(graphics_tablet));
    InitializeGraphicsTabletSettings(mojom_graphics_tablet.get());
    if (ShouldFetchDeviceImage()) {
      GetDeviceImage(mojom_graphics_tablet->device_key,
                     mojom_graphics_tablet->id);
    } else if (features::IsPeripheralNotificationEnabled()) {
      notification_controller_->NotifyGraphicsTabletFirstTimeConnected(
          *mojom_graphics_tablet);
    }

    graphics_tablets_.insert_or_assign(graphics_tablet.id,
                                       std::move(mojom_graphics_tablet));
    DispatchGraphicsTabletConnected(graphics_tablet.id);
  }

  for (const auto id : graphics_tablet_ids_to_remove) {
    DispatchGraphicsTabletDisconnectedAndEraseFromList(id);
  }
  RefreshStoredLoginScreenGraphicsTabletSettings();
  RefreshBatteryInfoForConnectedDevices();
  RefreshCompanionAppInfoForConnectedDevices();
}

void InputDeviceSettingsControllerImpl::RestoreDefaultKeyboardRemappings(
    DeviceId id) {
  DCHECK(base::Contains(keyboards_, id));
  auto& keyboard = *keyboards_.at(id);
  mojom::KeyboardSettingsPtr new_settings = keyboard.settings->Clone();
  new_settings->modifier_remappings = {};
  new_settings->six_pack_key_remappings = mojom::SixPackKeyInfo::New();
  if (keyboard.meta_key == ui::mojom::MetaKey::kCommand) {
    new_settings->modifier_remappings[ui::mojom::ModifierKey::kControl] =
        ui::mojom::ModifierKey::kMeta;
    new_settings->modifier_remappings[ui::mojom::ModifierKey::kMeta] =
        ui::mojom::ModifierKey::kControl;
  }
  metrics_manager_->RecordKeyboardNumberOfKeysReset(keyboard, *new_settings);
  SetKeyboardSettings(id, std::move(new_settings));
}

void InputDeviceSettingsControllerImpl::InitializeKeyboardSettings(
    mojom::Keyboard* keyboard) {
  if (active_pref_service_) {
    keyboard_pref_handler_->InitializeKeyboardSettings(
        active_pref_service_, policy_handler_->keyboard_policies(), keyboard);
    PR_LOG(INFO, Feature::IDS)
        << GetKeyboardSettingsLog("Connected", *keyboard);
    metrics_manager_->RecordKeyboardInitialMetrics(*keyboard);
    return;
  }

  // Ensure `keyboard.settings` is left in a valid state. This state occurs
  // during OOBE setup and when signing in a new user.
  if (!active_account_id_.has_value() || !local_state_) {
    keyboard_pref_handler_->InitializeWithDefaultKeyboardSettings(
        policy_handler_->keyboard_policies(), keyboard);
    PR_LOG(INFO, Feature::IDS)
        << GetKeyboardSettingsLog("Default settings initialized", *keyboard);
    return;
  }

  keyboard_pref_handler_->InitializeLoginScreenKeyboardSettings(
      local_state_, active_account_id_.value(),
      policy_handler_->keyboard_policies(), keyboard);
  PR_LOG(INFO, Feature::IDS)
      << GetKeyboardSettingsLog("Login screen settings initialized", *keyboard);
}

// GetGeneralizedKeyboard returns the keyboard for generalized settings. If
// there is only internal keyboard, GetGeneralizedKeyboard returns it.
// If there are multiple keyboards, GetGeneralizedKeyboard returns the
// latest external keyboard which has the largest device id.
const mojom::Keyboard*
InputDeviceSettingsControllerImpl::GetGeneralizedKeyboard() {
  auto external_iter = base::ranges::find(
      keyboards_.rbegin(), keyboards_.rend(), /*value=*/true,
      [](const auto& keyboard) { return keyboard.second->is_external; });
  auto internal_iter = base::ranges::find(
      keyboards_.rbegin(), keyboards_.rend(), /*value=*/false,
      [](const auto& keyboard) { return keyboard.second->is_external; });
  if (external_iter != keyboards_.rend()) {
    return external_iter->second.get();
  }

  if (internal_iter != keyboards_.rend()) {
    return internal_iter->second.get();
  }

  return nullptr;
}

// GetGeneralizedTopRowAreFKeys returns false if there is no keyboard. If there
// is only internal keyboard, GetGeneralizedTopRowAreFKeys returns the
// top_row_are_fkeys of it. If there are multiple keyboards,
// GetGeneralizedTopRowAreFKeys returns the top_row_are_fkeys of latest external
// keyboard which has the largest device id.
bool InputDeviceSettingsControllerImpl::GetGeneralizedTopRowAreFKeys() {
  const mojom::Keyboard* generalized_keyboard(
      InputDeviceSettingsControllerImpl::GetGeneralizedKeyboard());
  if (generalized_keyboard == nullptr) {
    return false;
  }

  return generalized_keyboard->settings->top_row_are_fkeys;
}

void InputDeviceSettingsControllerImpl::InitializeMouseSettings(
    mojom::Mouse* mouse) {
  if (active_pref_service_) {
    mouse_pref_handler_->InitializeMouseSettings(
        active_pref_service_, policy_handler_->mouse_policies(), mouse);
    PR_LOG(INFO, Feature::IDS) << GetMouseSettingsLog("Connected", *mouse);
    metrics_manager_->RecordMouseInitialMetrics(*mouse);
    return;
  }

  // Ensure `mouse.settings` is left in a valid state. This state occurs
  // during OOBE setup and when signing in a new user.
  if (!active_account_id_.has_value() || !local_state_) {
    mouse_pref_handler_->InitializeWithDefaultMouseSettings(
        policy_handler_->mouse_policies(), mouse);
    PR_LOG(INFO, Feature::IDS)
        << GetMouseSettingsLog("Default settings initialized", *mouse);
    return;
  }

  mouse_pref_handler_->InitializeLoginScreenMouseSettings(
      local_state_, active_account_id_.value(),
      policy_handler_->mouse_policies(), mouse);
  PR_LOG(INFO, Feature::IDS)
      << GetMouseSettingsLog("Login screen settings initialized", *mouse);
}

void InputDeviceSettingsControllerImpl::InitializePointingStickSettings(
    mojom::PointingStick* pointing_stick) {
  if (active_pref_service_) {
    pointing_stick_pref_handler_->InitializePointingStickSettings(
        active_pref_service_, pointing_stick);
    PR_LOG(INFO, Feature::IDS)
        << GetPointingStickSettingsLog("Connected", *pointing_stick);
    metrics_manager_->RecordPointingStickInitialMetrics(*pointing_stick);
    return;
  }

  // Ensure `pointing_stick.settings` is left in a valid state. This state
  // occurs during OOBE setup and when signing in a new user.
  if (!active_account_id_.has_value() || !local_state_) {
    pointing_stick_pref_handler_->InitializeWithDefaultPointingStickSettings(
        pointing_stick);
    PR_LOG(INFO, Feature::IDS) << GetPointingStickSettingsLog(
        "Default settings initialized", *pointing_stick);
    return;
  }

  pointing_stick_pref_handler_->InitializeLoginScreenPointingStickSettings(
      local_state_, active_account_id_.value(), pointing_stick);
  PR_LOG(INFO, Feature::IDS) << GetPointingStickSettingsLog(
      "Login screen settings initialized", *pointing_stick);
}

void InputDeviceSettingsControllerImpl::InitializeGraphicsTabletSettings(
    mojom::GraphicsTablet* graphics_tablet) {
  if (active_pref_service_) {
    graphics_tablet_pref_handler_->InitializeGraphicsTabletSettings(
        active_pref_service_, graphics_tablet);
    PR_LOG(INFO, Feature::IDS)
        << GetGraphicsTabletSettingsLog("Connected", *graphics_tablet);
    metrics_manager_->RecordGraphicsTabletInitialMetrics(*graphics_tablet);
    return;
  }

  // If there is no active account id or local state isn't initialized, use
  // default graphics tablet settings. This can happen as an uncommon race
  // condition when first signing in on the login screen.
  if (!active_account_id_ || !local_state_) {
    graphics_tablet->settings = mojom::GraphicsTabletSettings::New();
    return;
  }

  graphics_tablet_pref_handler_->InitializeLoginScreenGraphicsTabletSettings(
      local_state_, active_account_id_.value(), graphics_tablet);
  PR_LOG(INFO, Feature::IDS) << GetGraphicsTabletSettingsLog(
      "Login screen settings initialized", *graphics_tablet);
}

void InputDeviceSettingsControllerImpl::InitializeTouchpadSettings(
    mojom::Touchpad* touchpad) {
  if (active_pref_service_) {
    touchpad_pref_handler_->InitializeTouchpadSettings(active_pref_service_,
                                                       touchpad);
    PR_LOG(INFO, Feature::IDS)
        << GetTouchpadSettingsLog("Connected", *touchpad);
    metrics_manager_->RecordTouchpadInitialMetrics(*touchpad);
    return;
  }

  // Ensure `touchpad.settings` is left in a valid state. This state occurs
  // during OOBE setup and when signing in a new user.
  if (!active_account_id_.has_value() || !local_state_) {
    touchpad_pref_handler_->InitializeWithDefaultTouchpadSettings(touchpad);
    PR_LOG(INFO, Feature::IDS)
        << GetTouchpadSettingsLog("Default settings initialized", *touchpad);
    return;
  }

  touchpad_pref_handler_->InitializeLoginScreenTouchpadSettings(
      local_state_, active_account_id_.value(), touchpad);
  PR_LOG(INFO, Feature::IDS)
      << GetTouchpadSettingsLog("Login screen settings initialized", *touchpad);
}

const mojom::Mouse* InputDeviceSettingsControllerImpl::GetMouse(DeviceId id) {
  return FindMouse(id);
}

const mojom::Touchpad* InputDeviceSettingsControllerImpl::GetTouchpad(
    DeviceId id) {
  return FindTouchpad(id);
}

const mojom::Keyboard* InputDeviceSettingsControllerImpl::GetKeyboard(
    DeviceId id) {
  return FindKeyboard(id);
}

const mojom::GraphicsTablet*
InputDeviceSettingsControllerImpl::GetGraphicsTablet(DeviceId id) {
  return FindGraphicsTablet(id);
}

const mojom::PointingStick* InputDeviceSettingsControllerImpl::GetPointingStick(
    DeviceId id) {
  return FindPointingStick(id);
}

mojom::Mouse* InputDeviceSettingsControllerImpl::FindMouse(DeviceId id) {
  return FindDevice(id, duplicate_id_finder_.get(), mice_);
}

mojom::Touchpad* InputDeviceSettingsControllerImpl::FindTouchpad(DeviceId id) {
  return FindDevice(id, duplicate_id_finder_.get(), touchpads_);
}

mojom::Keyboard* InputDeviceSettingsControllerImpl::FindKeyboard(DeviceId id) {
  return FindDevice(id, duplicate_id_finder_.get(), keyboards_);
}

mojom::GraphicsTablet* InputDeviceSettingsControllerImpl::FindGraphicsTablet(
    DeviceId id) {
  return FindDevice(id, duplicate_id_finder_.get(), graphics_tablets_);
}

mojom::PointingStick* InputDeviceSettingsControllerImpl::FindPointingStick(
    DeviceId id) {
  return FindDevice(id, duplicate_id_finder_.get(), pointing_sticks_);
}

void InputDeviceSettingsControllerImpl::StartObservingButtons(DeviceId id) {
  DCHECK(features::IsPeripheralCustomizationEnabled());
  PeripheralCustomizationEventRewriter* rewriter =
      Shell::Get()
          ->event_rewriter_controller()
          ->peripheral_customization_event_rewriter();
  CHECK(rewriter);
  auto* mouse = FindMouse(id);
  if (mouse &&
      mouse->customization_restriction !=
          ash::mojom::CustomizationRestriction::kDisallowCustomizations) {
    const auto* duplicate_ids =
        duplicate_id_finder_->GetDuplicateDeviceIds(mouse->id);
    CHECK(duplicate_ids);
    for (const auto& duplicate_id : *duplicate_ids) {
      rewriter->StartObservingMouse(duplicate_id,
                                    mouse->customization_restriction);
    }
    for (auto& observer : observers_) {
      observer.OnCustomizableMouseObservingStarted(*mouse);
    }
    return;
  }

  auto* graphics_tablet = FindGraphicsTablet(id);
  if (graphics_tablet &&
      graphics_tablet->customization_restriction !=
          ash::mojom::CustomizationRestriction::kDisallowCustomizations) {
    const auto* duplicate_ids =
        duplicate_id_finder_->GetDuplicateDeviceIds(graphics_tablet->id);
    CHECK(duplicate_ids);
    for (const auto& duplicate_id : *duplicate_ids) {
      rewriter->StartObservingGraphicsTablet(
          duplicate_id, graphics_tablet->customization_restriction);
    }
    return;
  }
}

void InputDeviceSettingsControllerImpl::StopObservingButtons() {
  DCHECK(features::IsPeripheralCustomizationEnabled());
  PeripheralCustomizationEventRewriter* rewriter =
      Shell::Get()
          ->event_rewriter_controller()
          ->peripheral_customization_event_rewriter();
  CHECK(rewriter);
  rewriter->StopObserving();

  for (auto& observer : observers_) {
    observer.OnCustomizableMouseObservingStopped();
  }
}

void InputDeviceSettingsControllerImpl::OnMouseButtonPressed(
    DeviceId device_id,
    const mojom::Button& button) {
  DCHECK(features::IsPeripheralCustomizationEnabled());
  auto* mouse_ptr = FindMouse(device_id);
  if (!mouse_ptr) {
    return;
  }

  auto& mouse = *mouse_ptr;
  auto& button_remappings = mouse.settings->button_remappings;
  auto remapping_iter =
      base::ranges::find(button_remappings, button,
                         [](const mojom::ButtonRemappingPtr& remapping) {
                           return *remapping->button;
                         });
  if (remapping_iter != button_remappings.end()) {
    DispatchCustomizableMouseButtonPressed(mouse, button);
    return;
  }

  AddButtonToButtonRemappingList(button, button_remappings,
                                 /*is_mouse_button_remapping=*/true);
  mouse_pref_handler_->UpdateMouseSettings(
      active_pref_service_, policy_handler_->mouse_policies(), mouse);
  PR_LOG(INFO, Feature::IDS) << GetMouseSettingsLog("Updated", mouse);
  DispatchCustomizableMouseButtonPressed(mouse, button);
  metrics_manager_->RecordNewButtonRegisteredMetrics(
      button, InputDeviceSettingsMetricsManager::
                  PeripheralCustomizationMetricsType::kMouse);
  DispatchMouseSettingsChanged(mouse_ptr->id);

  UpdateDuplicateDeviceSettings(
      mouse, mice_,
      base::BindRepeating(
          &InputDeviceSettingsControllerImpl::DispatchMouseSettingsChanged,
          base::Unretained(this)));
}

void InputDeviceSettingsControllerImpl::OnDeviceImageForSettingsDownloaded(
    base::OnceCallback<void(const std::optional<std::string>&)> callback,
    const DeviceImage& device_image) {
  std::move(callback).Run(device_image.data_url());
}

void InputDeviceSettingsControllerImpl::GetDeviceImageDataUrl(
    const std::string& device_key,
    base::OnceCallback<void(const std::optional<std::string>&)> callback) {
  if (!ShouldFetchDeviceImage()) {
    std::move(callback).Run(std::nullopt);
    return;
  }
  metadata_manager_->GetDeviceImage(
      device_key, active_account_id_.value(), DeviceImageDestination::kSettings,
      base::BindOnce(&InputDeviceSettingsControllerImpl::
                         OnDeviceImageForSettingsDownloaded,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void InputDeviceSettingsControllerImpl::ResetNotificationDeviceTracking() {
  welcome_notification_clicked_device_keys_.clear();
}

void InputDeviceSettingsControllerImpl::OnGraphicsTabletButtonPressed(
    DeviceId device_id,
    const mojom::Button& button) {
  DCHECK(features::IsPeripheralCustomizationEnabled());
  auto* graphics_tablet_ptr = FindGraphicsTablet(device_id);
  if (!graphics_tablet_ptr) {
    return;
  }

  auto& graphics_tablet = *graphics_tablet_ptr;
  auto& tablet_button_remappings =
      graphics_tablet.settings->tablet_button_remappings;
  auto tablet_remapping_iter =
      base::ranges::find(tablet_button_remappings, button,
                         [](const mojom::ButtonRemappingPtr& remapping) {
                           return *remapping->button;
                         });
  if (tablet_remapping_iter != tablet_button_remappings.end()) {
    DispatchCustomizableTabletButtonPressed(graphics_tablet, button);
    return;
  }

  auto& pen_button_remappings = graphics_tablet.settings->pen_button_remappings;
  auto pen_remapping_iter =
      base::ranges::find(pen_button_remappings, button,
                         [](const mojom::ButtonRemappingPtr& remapping) {
                           return *remapping->button;
                         });
  if (pen_remapping_iter != pen_button_remappings.end()) {
    DispatchCustomizablePenButtonPressed(graphics_tablet, button);
    return;
  }

  if (IsGraphicsTabletPenButton(button)) {
    AddButtonToButtonRemappingList(button, pen_button_remappings,
                                   /*is_mouse_button_remapping=*/false);
    DispatchCustomizablePenButtonPressed(graphics_tablet, button);
    metrics_manager_->RecordNewButtonRegisteredMetrics(
        button, InputDeviceSettingsMetricsManager::
                    PeripheralCustomizationMetricsType::kGraphicsTabletPen);
  } else {
    AddButtonToButtonRemappingList(button, tablet_button_remappings,
                                   /*is_mouse_button_remapping=*/false);
    DispatchCustomizableTabletButtonPressed(graphics_tablet, button);
    metrics_manager_->RecordNewButtonRegisteredMetrics(
        button, InputDeviceSettingsMetricsManager::
                    PeripheralCustomizationMetricsType::kGraphicsTablet);
  }
  graphics_tablet_pref_handler_->UpdateGraphicsTabletSettings(
      active_pref_service_, graphics_tablet);
  PR_LOG(INFO, Feature::IDS)
      << GetGraphicsTabletSettingsLog("Updated", graphics_tablet);
  DispatchGraphicsTabletSettingsChanged(graphics_tablet_ptr->id);

  UpdateDuplicateDeviceSettings(
      graphics_tablet, graphics_tablets_,
      base::BindRepeating(&InputDeviceSettingsControllerImpl::
                              DispatchGraphicsTabletSettingsChanged,
                          base::Unretained(this)));
}

void InputDeviceSettingsControllerImpl::DispatchCustomizableMouseButtonPressed(
    const mojom::Mouse& mouse,
    const mojom::Button& button) {
  for (auto& observer : observers_) {
    observer.OnCustomizableMouseButtonPressed(mouse, button);
  }
}

void InputDeviceSettingsControllerImpl::DispatchCustomizableTabletButtonPressed(
    const mojom::GraphicsTablet& graphics_tablet,
    const mojom::Button& button) {
  for (auto& observer : observers_) {
    observer.OnCustomizableTabletButtonPressed(graphics_tablet, button);
  }
}

void InputDeviceSettingsControllerImpl::DispatchCustomizablePenButtonPressed(
    const mojom::GraphicsTablet& graphics_tablet,
    const mojom::Button& button) {
  for (auto& observer : observers_) {
    observer.OnCustomizablePenButtonPressed(graphics_tablet, button);
  }
}

void InputDeviceSettingsControllerImpl::DispatchKeyboardBatteryInfoChanged(
    DeviceId id) {
  CHECK(base::Contains(keyboards_, id));
  CHECK(features::IsWelcomeExperienceEnabled());
  const auto& keyboard = *keyboards_.at(id);
  for (auto& observer : observers_) {
    observer.OnKeyboardBatteryInfoChanged(keyboard);
  }
}

void InputDeviceSettingsControllerImpl::
    DispatchGraphicsTabletBatteryInfoChanged(DeviceId id) {
  CHECK(base::Contains(graphics_tablets_, id));
  CHECK(features::IsWelcomeExperienceEnabled());
  const auto& graphics_tablet = *graphics_tablets_.at(id);
  for (auto& observer : observers_) {
    observer.OnGraphicsTabletBatteryInfoChanged(graphics_tablet);
  }
}

void InputDeviceSettingsControllerImpl::DispatchMouseBatteryInfoChanged(
    DeviceId id) {
  CHECK(base::Contains(mice_, id));
  CHECK(features::IsWelcomeExperienceEnabled());
  const auto& mouse = *mice_.at(id);
  for (auto& observer : observers_) {
    observer.OnMouseBatteryInfoChanged(mouse);
  }
}

void InputDeviceSettingsControllerImpl::DispatchTouchpadBatteryInfoChanged(
    DeviceId id) {
  CHECK(base::Contains(touchpads_, id));
  CHECK(features::IsWelcomeExperienceEnabled());
  const auto& touchpad = *touchpads_.at(id);
  for (auto& observer : observers_) {
    observer.OnTouchpadBatteryInfoChanged(touchpad);
  }
}

void InputDeviceSettingsControllerImpl::OnOobeDialogStateChanged(
    OobeDialogState state) {
  oobe_state_ = state;
}

void InputDeviceSettingsControllerImpl::OnAppRegistryCacheWillBeDestroyed(
    apps::AppRegistryCache* cache) {
  app_registry_cache_observer_.Reset();
}

void InputDeviceSettingsControllerImpl::OnNotificationClicked(
    const std::string& notification_id,
    const std::optional<int>& button_index,
    const std::optional<std::u16string>& reply) {
  const auto device_key =
      notification_controller_->GetDeviceKeyForNotificationId(notification_id);
  if (device_key.has_value()) {
    welcome_notification_clicked_device_keys_.insert(device_key.value());
  }
}

void InputDeviceSettingsControllerImpl::OnAppUpdate(
    const apps::AppUpdate& update) {
  auto package_id = update.InstallerPackageId().has_value()
                        ? update.InstallerPackageId()->ToString()
                        : "";
  auto it = package_id_to_device_id_map_.find(package_id);
  if (it == package_id_to_device_id_map_.end()) {
    return;
  }

  auto state = apps_util::IsInstalled(update.Readiness())
                   ? mojom::CompanionAppState::kInstalled
                   : mojom::CompanionAppState::kAvailable;

  const bool is_installed = state == mojom::CompanionAppState::kInstalled;
  auto id = it->second;
  if (auto* mouse = FindMouse(id); mouse != nullptr) {
    mouse->app_info->state = state;
    if (is_installed) {
      metrics_manager_->RecordCompanionAppInstalled(mouse->device_key);
    }
    DispatchMouseCompanionAppInfoChanged(*mouse);
    return;
  }

  if (auto* keyboard = FindKeyboard(id); keyboard != nullptr) {
    keyboard->app_info->state = state;
    if (is_installed) {
      metrics_manager_->RecordCompanionAppInstalled(keyboard->device_key);
    }
    DispatchKeyboardCompanionAppInfoChanged(*keyboard);
    return;
  }

  if (auto* touchpad = FindTouchpad(id); touchpad != nullptr) {
    touchpad->app_info->state = state;
    if (is_installed) {
      metrics_manager_->RecordCompanionAppInstalled(touchpad->device_key);
    }
    DispatchTouchpadCompanionAppInfoChanged(*touchpad);
    return;
  }

  if (auto* graphics_tablet = FindGraphicsTablet(id);
      graphics_tablet != nullptr) {
    graphics_tablet->app_info->state = state;
    if (is_installed) {
      metrics_manager_->RecordCompanionAppInstalled(
          graphics_tablet->device_key);
    }
    DispatchGraphicsTabletCompanionAppInfoChanged(*graphics_tablet);
    return;
  }
}

void InputDeviceSettingsControllerImpl::RefreshInternalPointingStickSettings() {
  for (auto& [id, pointing_stick] : pointing_sticks_) {
    if (pointing_stick->is_external) {
      continue;
    }

    InitializePointingStickSettings(pointing_stick.get());
    DispatchPointingStickSettingsChanged(id);
  }
}

void InputDeviceSettingsControllerImpl::RefreshInternalTouchpadSettings() {
  for (auto& [id, touchpad] : touchpads_) {
    if (touchpad->is_external) {
      continue;
    }

    InitializeTouchpadSettings(touchpad.get());
    DispatchTouchpadSettingsChanged(id);
  }
}

void InputDeviceSettingsControllerImpl::
    ForceInitializeDefaultTouchpadSettings() {
  if (!IsOobe()) {
    return;
  }

  for (auto& [id, touchpad] : touchpads_) {
    // Internal touchpad settings are fully synced, no need to force refresh
    // them.
    if (!touchpad->is_external) {
      continue;
    }

    touchpad_pref_handler_->ForceInitializeWithDefaultSettings(
        active_pref_service_, touchpad.get());
  }
}

void InputDeviceSettingsControllerImpl::ForceInitializeDefaultMouseSettings() {
  if (!IsOobe()) {
    return;
  }

  for (auto& [id, mouse] : mice_) {
    mouse_pref_handler_->ForceInitializeWithDefaultSettings(
        active_pref_service_, policy_handler_->mouse_policies(), mouse.get());
  }
}

void InputDeviceSettingsControllerImpl::
    ForceInitializeDefaultChromeOSKeyboardSettings() {
  if (!IsOobe()) {
    return;
  }

  for (auto& [id, keyboard] : keyboards_) {
    // Only forward sync top_row_are_fkeys pref value from ChromeOS keyboard
    // settings to split modifier keyboard settings.
    if (IsSplitModifierKeyboard(*keyboard)) {
      auto top_row_are_fkeys =
          active_pref_service_->GetDict(prefs::kKeyboardDefaultChromeOSSettings)
              .FindBool(prefs::kKeyboardSettingTopRowAreFKeys);

      if (!top_row_are_fkeys || active_pref_service_->GetBoolean(
                                    prefs::kKeyboardHasSplitModifierKeyboard)) {
        continue;
      }
      keyboard->settings->top_row_are_fkeys = top_row_are_fkeys.value();
      keyboard_pref_handler_->UpdateKeyboardSettings(
          active_pref_service_, policy_handler_->keyboard_policies(),
          *keyboard);
      continue;
    }

    if (!IsChromeOSKeyboard(*keyboard)) {
      continue;
    }

    keyboard_pref_handler_->ForceInitializeWithDefaultSettings(
        active_pref_service_, policy_handler_->keyboard_policies(),
        keyboard.get());
  }
}

void InputDeviceSettingsControllerImpl::
    ForceInitializeDefaultNonChromeOSKeyboardSettings() {
  if (!IsOobe()) {
    return;
  }

  for (auto& [id, keyboard] : keyboards_) {
    if (IsChromeOSKeyboard(*keyboard) || IsSplitModifierKeyboard(*keyboard)) {
      continue;
    }

    keyboard_pref_handler_->ForceInitializeWithDefaultSettings(
        active_pref_service_, policy_handler_->keyboard_policies(),
        keyboard.get());
  }
}

void InputDeviceSettingsControllerImpl::
    ForceInitializeDefaultSplitModifierKeyboardSettings() {
  if (!IsOobe()) {
    return;
  }

  for (auto& [id, keyboard] : keyboards_) {
    if (!IsSplitModifierKeyboard(*keyboard)) {
      continue;
    }

    keyboard_pref_handler_->ForceInitializeWithDefaultSettings(
        active_pref_service_, policy_handler_->keyboard_policies(),
        keyboard.get());
  }
}

void InputDeviceSettingsControllerImpl::RefreshCachedMouseSettings() {
  RefreshStoredLoginScreenMouseSettings();
  RefreshMouseDefaultSettings();
}

void InputDeviceSettingsControllerImpl::RefreshCachedKeyboardSettings() {
  // Only refresh the settings after the OOBE session to avoid a race condition
  // in the order we receive synced prefs.
  if (IsOobe()) {
    return;
  }
  RefreshStoredLoginScreenKeyboardSettings();
  RefreshKeyboardDefaultSettings();
}

void InputDeviceSettingsControllerImpl::RefreshCachedTouchpadSettings() {
  RefreshStoredLoginScreenTouchpadSettings();
  RefreshTouchpadDefaultSettings();
}

void InputDeviceSettingsControllerImpl::RefreshMouseDefaultSettings() {
  if (!active_pref_service_ || mice_.empty()) {
    return;
  }

  mouse_pref_handler_->UpdateDefaultMouseSettings(
      active_pref_service_, policy_handler_->mouse_policies(),
      *mice_.rbegin()->second);
  PR_LOG(INFO, Feature::IDS) << GetMouseSettingsLog(
      "Default settings refreshed", *mice_.rbegin()->second);
}

void InputDeviceSettingsControllerImpl::RefreshKeyboardDefaultSettings() {
  if (!active_pref_service_) {
    return;
  }

  auto chromeos_iter =
      base::ranges::find(keyboards_.rbegin(), keyboards_.rend(), /*value=*/true,
                         [](const auto& keyboard) {
                           return IsChromeOSKeyboard(*keyboard.second);
                         });
  auto non_chromeos_iter =
      base::ranges::find(keyboards_.rbegin(), keyboards_.rend(),
                         /*value=*/false, [](const auto& keyboard) {
                           return IsChromeOSKeyboard(*keyboard.second);
                         });
  auto split_modifier_iter =
      base::ranges::find(keyboards_.rbegin(), keyboards_.rend(),
                         /*value=*/true, [](const auto& keyboard) {
                           return IsSplitModifierKeyboard(*keyboard.second);
                         });

  if (chromeos_iter != keyboards_.rend()) {
    keyboard_pref_handler_->UpdateDefaultChromeOSKeyboardSettings(
        active_pref_service_, policy_handler_->keyboard_policies(),
        *chromeos_iter->second);
    PR_LOG(INFO, Feature::IDS) << GetKeyboardSettingsLog(
        "Default ChromeOS Keyboard settings refreshed", *chromeos_iter->second);
  }

  if (non_chromeos_iter != keyboards_.rend()) {
    keyboard_pref_handler_->UpdateDefaultNonChromeOSKeyboardSettings(
        active_pref_service_, policy_handler_->keyboard_policies(),
        *non_chromeos_iter->second);
    PR_LOG(INFO, Feature::IDS) << GetKeyboardSettingsLog(
        "Default NonChromeOS Keyboard settings refreshed",
        *non_chromeos_iter->second);
  }

  if (split_modifier_iter != keyboards_.rend()) {
    keyboard_pref_handler_->UpdateDefaultSplitModifierKeyboardSettings(
        active_pref_service_, policy_handler_->keyboard_policies(),
        *split_modifier_iter->second);
    PR_LOG(INFO, Feature::IDS) << GetKeyboardSettingsLog(
        "Default Split Modifier Keyboard settings refreshed",
        *split_modifier_iter->second);
  }
}

void InputDeviceSettingsControllerImpl::RefreshTouchpadDefaultSettings() {
  if (!active_pref_service_ || touchpads_.empty()) {
    return;
  }

  touchpad_pref_handler_->UpdateDefaultTouchpadSettings(
      active_pref_service_, *touchpads_.rbegin()->second);
  PR_LOG(INFO, Feature::IDS) << GetTouchpadSettingsLog(
      "Default settings refreshed", *touchpads_.rbegin()->second);
}

void InputDeviceSettingsControllerImpl::RefreshKeyDisplay() {
  for (auto& [_, mouse] : mice_) {
    RefreshKeyDisplayMouse(*mouse);
    DispatchMouseSettingsChanged(mouse->id);
  }

  for (auto& [_, graphics_tablet] : graphics_tablets_) {
    RefreshKeyDisplayGraphicsTablet(*graphics_tablet);
    DispatchGraphicsTabletSettingsChanged(graphics_tablet->id);
  }
}

void InputDeviceSettingsControllerImpl::InputMethodChanged(
    input_method::InputMethodManager* manager,
    Profile* profile,
    bool show_message) {
  if (!features::IsPeripheralCustomizationEnabled()) {
    return;
  }

  // Must be posted as a task because the data source for key display values is
  // not yet updated right when this is called.
  sequenced_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&InputDeviceSettingsControllerImpl::RefreshKeyDisplay,
                     weak_ptr_factory_.GetWeakPtr()));
}

bool InputDeviceSettingsControllerImpl::ShouldFetchDeviceImage() {
  return features::IsWelcomeExperienceEnabled() && active_account_id_ &&
         active_pref_service_;
}

void InputDeviceSettingsControllerImpl::GetDeviceImage(
    const std::string& device_key,
    DeviceId id) {
  CHECK(features::IsWelcomeExperienceEnabled());
  CHECK(active_account_id_.has_value());
  metadata_manager_->GetDeviceImage(
      device_key, active_account_id_.value(),
      DeviceImageDestination::kNotification,
      base::BindOnce(&InputDeviceSettingsControllerImpl::
                         OnDeviceNotificationImageDownloaded,
                     weak_ptr_factory_.GetWeakPtr(), id));
}

// TODO(b/329686601): Handle case where a device is both a mouse and keyboard.
void InputDeviceSettingsControllerImpl::OnDeviceNotificationImageDownloaded(
    DeviceId id,
    const DeviceImage& device_image) {
  if (auto* kb = FindKeyboard(id); kb != nullptr) {
    notification_controller_->NotifyKeyboardFirstTimeConnected(
        *kb, device_image.gfx_image_skia());
    return;
  }

  if (auto* mouse = FindMouse(id); mouse != nullptr) {
    notification_controller_->NotifyMouseFirstTimeConnected(
        *mouse, device_image.gfx_image_skia());
    return;
  }

  if (auto* touchpad = FindTouchpad(id); touchpad != nullptr) {
    notification_controller_->NotifyTouchpadFirstTimeConnected(
        *touchpad, device_image.gfx_image_skia());
    return;
  }

  if (auto* graphics_tablet = FindGraphicsTablet(id);
      graphics_tablet != nullptr) {
    notification_controller_->NotifyGraphicsTabletFirstTimeConnected(
        *graphics_tablet, device_image.gfx_image_skia());
    return;
  }
}

void InputDeviceSettingsControllerImpl::DeviceBatteryChanged(
    device::BluetoothAdapter* adapter,
    device::BluetoothDevice* device,
    device::BluetoothDevice::BatteryType type) {
  CHECK(features::IsWelcomeExperienceEnabled());
  RefreshBatteryInfoForConnectedDevices();
}

void InputDeviceSettingsControllerImpl::RefreshMetaAndModifierKeys() {
  for (auto& [_, keyboard] : keyboards_) {
    keyboard->meta_key =
        Shell::Get()->keyboard_capability()->GetMetaKey(keyboard->id);
    keyboard->modifier_keys =
        Shell::Get()->keyboard_capability()->GetModifierKeys(keyboard->id);
  }
}

bool InputDeviceSettingsControllerImpl::IsOobe() const {
  session_manager::SessionState session_state =
      Shell::Get()->session_controller()->GetSessionState();
  // Default OOBE flow
  const bool is_default_oobe_flow =
      session_state == session_manager::SessionState::OOBE;
  // OOBE enterprise enrollment -> add person flow
  const bool is_add_person_flow =
      session_state == session_manager::SessionState::LOGIN_PRIMARY &&
      oobe_state_ != OobeDialogState::HIDDEN;
  return is_default_oobe_flow || is_add_person_flow;
}

void InputDeviceSettingsControllerImpl::DispatchMouseCompanionAppInfoChanged(
    const mojom::Mouse& mouse) {
  CHECK(features::IsWelcomeExperienceEnabled());
  for (auto& observer : observers_) {
    observer.OnMouseCompanionAppInfoChanged(mouse);
  }
}

void InputDeviceSettingsControllerImpl::DispatchKeyboardCompanionAppInfoChanged(
    const mojom::Keyboard& keyboard) {
  CHECK(features::IsWelcomeExperienceEnabled());
  for (auto& observer : observers_) {
    observer.OnKeyboardCompanionAppInfoChanged(keyboard);
  }
}

void InputDeviceSettingsControllerImpl::DispatchTouchpadCompanionAppInfoChanged(
    const mojom::Touchpad& touchpad) {
  CHECK(features::IsWelcomeExperienceEnabled());
  for (auto& observer : observers_) {
    observer.OnTouchpadCompanionAppInfoChanged(touchpad);
  }
}

void InputDeviceSettingsControllerImpl::
    DispatchGraphicsTabletCompanionAppInfoChanged(
        const mojom::GraphicsTablet& graphics_tablet) {
  CHECK(features::IsWelcomeExperienceEnabled());
  for (auto& observer : observers_) {
    observer.OnGraphicsTabletCompanionAppInfoChanged(graphics_tablet);
  }
}

void InputDeviceSettingsControllerImpl::SetPeripheralsAppDelegate(
    PeripheralsAppDelegate* delegate) {
  delegate_ = delegate;
  RefreshCompanionAppInfoForConnectedDevices();
}

void InputDeviceSettingsControllerImpl::OnCompanionAppInfoReceived(
    DeviceId id,
    const std::string& device_key,
    const std::optional<mojom::CompanionAppInfo>& info) {
  if (!info) {
    return;
  }

  metrics_manager_->RecordCompanionAppAvailable(device_key);

  if (auto* mouse = FindMouse(id); mouse != nullptr) {
    mouse->app_info = info->Clone();
    package_id_to_device_id_map_[mouse->app_info->package_id] = id;
    DispatchMouseCompanionAppInfoChanged(*mouse);
    return;
  }

  if (auto* keyboard = FindKeyboard(id); keyboard != nullptr) {
    keyboard->app_info = info->Clone();
    package_id_to_device_id_map_[keyboard->app_info->package_id] = id;
    DispatchKeyboardCompanionAppInfoChanged(*keyboard);
    return;
  }

  if (auto* touchpad = FindTouchpad(id); touchpad != nullptr) {
    touchpad->app_info = info->Clone();
    package_id_to_device_id_map_[touchpad->app_info->package_id] = id;
    DispatchTouchpadCompanionAppInfoChanged(*touchpad);
    return;
  }

  if (auto* graphics_tablet = FindGraphicsTablet(id);
      graphics_tablet != nullptr) {
    graphics_tablet->app_info = info->Clone();
    package_id_to_device_id_map_[graphics_tablet->app_info->package_id] = id;
    DispatchGraphicsTabletCompanionAppInfoChanged(*graphics_tablet);
    return;
  }
}

void InputDeviceSettingsControllerImpl::
    RefreshCompanionAppInfoForConnectedDevices() {
  if (!delegate_ || !active_pref_service_) {
    return;
  }

  for (const auto& [id, mouse] : mice_) {
    if (!mouse->is_external) {
      continue;
    }
    delegate_->GetCompanionAppInfo(
        mouse->device_key,
        base::BindOnce(
            &InputDeviceSettingsControllerImpl::OnCompanionAppInfoReceived,
            weak_ptr_factory_.GetWeakPtr(), id, mouse->device_key));
  }

  for (const auto& [id, touchpad] : touchpads_) {
    if (!touchpad->is_external) {
      continue;
    }
    delegate_->GetCompanionAppInfo(
        touchpad->device_key,
        base::BindOnce(
            &InputDeviceSettingsControllerImpl::OnCompanionAppInfoReceived,
            weak_ptr_factory_.GetWeakPtr(), id, touchpad->device_key));
  }

  for (const auto& [id, keyboard] : keyboards_) {
    if (!keyboard->is_external) {
      continue;
    }
    delegate_->GetCompanionAppInfo(
        keyboard->device_key,
        base::BindOnce(
            &InputDeviceSettingsControllerImpl::OnCompanionAppInfoReceived,
            weak_ptr_factory_.GetWeakPtr(), id, keyboard->device_key));
  }

  for (const auto& [id, graphics_tablet] : graphics_tablets_) {
    delegate_->GetCompanionAppInfo(
        graphics_tablet->device_key,
        base::BindOnce(
            &InputDeviceSettingsControllerImpl::OnCompanionAppInfoReceived,
            weak_ptr_factory_.GetWeakPtr(), id, graphics_tablet->device_key));
  }
}

}  // namespace ash
