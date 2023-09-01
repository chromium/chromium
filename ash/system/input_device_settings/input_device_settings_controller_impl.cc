// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/input_device_settings_controller_impl.h"

#include <iterator>
#include <memory>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/mojom/input_device_settings.mojom-shared.h"
#include "ash/public/mojom/input_device_settings.mojom.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/input_device_settings/input_device_key_alias_manager.h"
#include "ash/system/input_device_settings/input_device_notifier.h"
#include "ash/system/input_device_settings/input_device_settings_defaults.h"
#include "ash/system/input_device_settings/input_device_settings_policy_handler.h"
#include "ash/system/input_device_settings/input_device_settings_pref_names.h"
#include "ash/system/input_device_settings/input_device_settings_utils.h"
#include "ash/system/input_device_settings/pref_handlers/graphics_tablet_pref_handler_impl.h"
#include "ash/system/input_device_settings/pref_handlers/keyboard_pref_handler_impl.h"
#include "ash/system/input_device_settings/pref_handlers/mouse_pref_handler_impl.h"
#include "ash/system/input_device_settings/pref_handlers/pointing_stick_pref_handler_impl.h"
#include "ash/system/input_device_settings/pref_handlers/touchpad_pref_handler_impl.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/sequenced_task_runner.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "ui/events/ash/keyboard_capability.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/devices/keyboard_device.h"
#include "ui/events/devices/touchpad_device.h"

namespace ash {

namespace {

const int kMaxButtonNameLength = 64;

mojom::MetaKey GetMetaKeyForKeyboard(const ui::KeyboardDevice& keyboard) {
  const auto device_type =
      Shell::Get()->keyboard_capability()->GetDeviceType(keyboard);
  switch (device_type) {
    case ui::KeyboardCapability::DeviceType::kDeviceInternalKeyboard:
    case ui::KeyboardCapability::DeviceType::kDeviceExternalChromeOsKeyboard:
    case ui::KeyboardCapability::DeviceType::kDeviceHotrodRemote:
    case ui::KeyboardCapability::DeviceType::kDeviceVirtualCoreKeyboard:
      return Shell::Get()->keyboard_capability()->HasLauncherButton(keyboard)
                 ? mojom::MetaKey::kLauncher
                 : mojom::MetaKey::kSearch;
    case ui::KeyboardCapability::DeviceType::kDeviceExternalAppleKeyboard:
      return mojom::MetaKey::kCommand;
    case ui::KeyboardCapability::DeviceType::kDeviceUnknown:
    case ui::KeyboardCapability::DeviceType::kDeviceExternalGenericKeyboard:
    case ui::KeyboardCapability::DeviceType::kDeviceExternalUnknown:
    case ui::KeyboardCapability::DeviceType::kDeviceInternalRevenKeyboard:
      return mojom::MetaKey::kExternalMeta;
  };
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
    mojom_keyboard->meta_key = GetMetaKeyForKeyboard(keyboard);
  }
  return mojom_keyboard;
}

mojom::MousePtr BuildMojomMouse(const ui::InputDevice& mouse) {
  mojom::MousePtr mojom_mouse = mojom::Mouse::New();
  mojom_mouse->id = mouse.id;
  mojom_mouse->name = mouse.name;
  mojom_mouse->device_key =
      Shell::Get()->input_device_key_alias_manager()->GetAliasedDeviceKey(
          mouse);
  mojom_mouse->is_external =
      mouse.type != ui::InputDeviceType::INPUT_DEVICE_INTERNAL;
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
    const ui::InputDevice& graphics_tablet) {
  mojom::GraphicsTabletPtr mojom_graphics_tablet = mojom::GraphicsTablet::New();
  mojom_graphics_tablet->id = graphics_tablet.id;
  mojom_graphics_tablet->device_key =
      Shell::Get()->input_device_key_alias_manager()->GetAliasedDeviceKey(
          graphics_tablet);
  return mojom_graphics_tablet;
}

}  // namespace

// suppress_meta_fkey_rewrites must never be non-default for internal
// keyboards, otherwise the keyboard settings are not valid.
// Modifier remappings must only contain valid modifiers within the
// modifier_keys array. Settings are invalid if top_row_are_fkeys_policy exists
// and policy status is kManaged and the top_row_are_fkeys_policy's value is
// different from the settings top_row_are_fkeys value.
bool KeyboardSettingsAreValid(
    const mojom::Keyboard& keyboard,
    const mojom::KeyboardSettings& settings,
    const mojom::KeyboardPolicies& keyboard_policies) {
  for (const auto& remapping : settings.modifier_remappings) {
    auto it = base::ranges::find(keyboard.modifier_keys, remapping.first);
    if (it == keyboard.modifier_keys.end()) {
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
      (keyboard.meta_key != mojom::MetaKey::kLauncher &&
       keyboard.meta_key != mojom::MetaKey::kSearch);
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
// button remapping names should be fewer than 64 characters.
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
    if (!found || new_remapping->name.size() >= kMaxButtonNameLength) {
      return false;
    }
  }

  return true;
}

// Valid graphics tablet settings should have the same tablet and pen buttons
// as these in the graphics tablet and all the button remapping names should be
// fewer than 64 characters.
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
// fewer than 64 characters.
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
  InitializePolicyHandler();
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

InputDeviceSettingsControllerImpl::~InputDeviceSettingsControllerImpl() {
  Shell::Get()->session_controller()->RemoveObserver(this);
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
  pref_registry->RegisterListPref(prefs::kKeyboardDeviceImpostersListPref);
  pref_registry->RegisterDictionaryPref(prefs::kMouseButtonRemappingsDictPref);
  pref_registry->RegisterDictionaryPref(
      prefs::kGraphicsTabletTabletButtonRemappingsDictPref);
  pref_registry->RegisterDictionaryPref(
      prefs::kGraphicsTabletPenButtonRemappingsDictPref);
}

void InputDeviceSettingsControllerImpl::OnActiveUserPrefServiceChanged(
    PrefService* pref_service) {
  // If the flag is disabled, clear all the settings dictionaries.
  if (!features::IsInputDeviceSettingsSplitEnabled()) {
    active_pref_service_ = nullptr;
    pref_service->SetDict(prefs::kKeyboardDeviceSettingsDictPref, {});
    pref_service->SetDict(prefs::kMouseDeviceSettingsDictPref, {});
    pref_service->SetDict(prefs::kPointingStickDeviceSettingsDictPref, {});
    pref_service->SetDict(prefs::kTouchpadDeviceSettingsDictPref, {});
    pref_service->SetList(prefs::kKeyboardDeviceImpostersListPref, {});
    return;
  }

  if (!features::IsPeripheralCustomizationEnabled()) {
    pref_service->ClearPref(
        prefs::kGraphicsTabletTabletButtonRemappingsDictPref);
    pref_service->ClearPref(prefs::kGraphicsTabletPenButtonRemappingsDictPref);
    pref_service->ClearPref(prefs::kMouseButtonRemappingsDictPref);
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

    pref_service->ClearPref(prefs::kRemapToRightClickNotificationsRemaining);
    pref_service->ClearPref(prefs::kSixPackKeyDeleteNotificationsRemaining);
    pref_service->ClearPref(prefs::kSixPackKeyHomeNotificationsRemaining);
    pref_service->ClearPref(prefs::kSixPackKeyEndNotificationsRemaining);
    pref_service->ClearPref(prefs::kSixPackKeyPageUpNotificationsRemaining);
    pref_service->ClearPref(prefs::kSixPackKeyPageDownNotificationsRemaining);
    pref_service->ClearPref(prefs::kSixPackKeyInsertNotificationsRemaining);
  }
  active_pref_service_ = pref_service;
  active_account_id_ = Shell::Get()->session_controller()->GetActiveAccountId();
  InitializePolicyHandler();

  // Device settings must be refreshed when the user pref service is updated,
  // but all dependencies of `InputDeviceSettingsControllerImpl` must be
  // updated due to the active pref service change first. Therefore, schedule
  // a task so other dependencies are updated first.
  ScheduleDeviceSettingsRefresh();
}

void InputDeviceSettingsControllerImpl::ScheduleDeviceSettingsRefresh() {
  if (!settings_refresh_pending_) {
    settings_refresh_pending_ = true;
    sequenced_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &InputDeviceSettingsControllerImpl::RefreshAllDeviceSettings,
            weak_ptr_factory_.GetWeakPtr()));
  }
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

  RefreshStoredLoginScreenKeyboardSettings();
  RefreshStoredLoginScreenMouseSettings();
  RefreshStoredLoginScreenTouchpadSettings();
  RefreshStoredLoginScreenPointingStickSettings();
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
  }

  if (internal_iter != keyboards_.rend()) {
    auto& internal_keyboard = *internal_iter->second;
    keyboard_pref_handler_->UpdateLoginScreenKeyboardSettings(
        local_state_, active_account_id_.value(),
        policy_handler_->keyboard_policies(), internal_keyboard);
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
  }

  if (internal_iter != mice_.rend()) {
    auto& internal_mouse = *internal_iter->second;
    mouse_pref_handler_->UpdateLoginScreenMouseSettings(
        local_state_, active_account_id_.value(),
        policy_handler_->mouse_policies(), internal_mouse);
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
  }

  if (internal_iter != pointing_sticks_.rend()) {
    auto& internal_pointing_stick = *internal_iter->second;
    pointing_stick_pref_handler_->UpdateLoginScreenPointingStickSettings(
        local_state_, active_account_id_.value(), internal_pointing_stick);
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
  }

  if (internal_iter != touchpads_.rend()) {
    auto& internal_touchpad = *internal_iter->second;
    touchpad_pref_handler_->UpdateLoginScreenTouchpadSettings(
        local_state_, active_account_id_.value(), internal_touchpad);
  }
}

void InputDeviceSettingsControllerImpl::OnLoginScreenFocusedPodChanged(
    const AccountId& account_id) {
  active_account_id_ = account_id;

  for (const auto& [id, keyboard] : keyboards_) {
    keyboard_pref_handler_->InitializeLoginScreenKeyboardSettings(
        local_state_, account_id, policy_handler_->keyboard_policies(),
        keyboard.get());
    DispatchKeyboardSettingsChanged(id);
  }

  for (const auto& [id, mouse] : mice_) {
    mouse_pref_handler_->InitializeLoginScreenMouseSettings(
        local_state_, account_id, policy_handler_->mouse_policies(),
        mouse.get());
    DispatchMouseSettingsChanged(id);
  }

  for (const auto& [id, pointing_stick] : pointing_sticks_) {
    pointing_stick_pref_handler_->InitializeLoginScreenPointingStickSettings(
        local_state_, account_id, pointing_stick.get());
    DispatchPointingStickSettingsChanged(id);
  }

  for (const auto& [id, touchpad] : touchpads_) {
    touchpad_pref_handler_->InitializeLoginScreenTouchpadSettings(
        local_state_, account_id, touchpad.get());
    DispatchTouchpadSettingsChanged(id);
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
    mouse_pref_handler_->InitializeMouseSettings(
        active_pref_service_, policy_handler_->mouse_policies(), mouse.get());
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
  auto iter = keyboards_.find(id);
  if (iter == keyboards_.end()) {
    return nullptr;
  }
  return iter->second->settings.get();
}

const mojom::MouseSettings* InputDeviceSettingsControllerImpl::GetMouseSettings(
    DeviceId id) {
  auto iter = mice_.find(id);
  if (iter == mice_.end()) {
    return nullptr;
  }
  return iter->second->settings.get();
}

const mojom::TouchpadSettings*
InputDeviceSettingsControllerImpl::GetTouchpadSettings(DeviceId id) {
  auto iter = touchpads_.find(id);
  if (iter == touchpads_.end()) {
    return nullptr;
  }
  return iter->second->settings.get();
}

const mojom::PointingStickSettings*
InputDeviceSettingsControllerImpl::GetPointingStickSettings(DeviceId id) {
  auto iter = pointing_sticks_.find(id);
  if (iter == pointing_sticks_.end()) {
    return nullptr;
  }
  return iter->second->settings.get();
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

void InputDeviceSettingsControllerImpl::SetKeyboardSettings(
    DeviceId id,
    mojom::KeyboardSettingsPtr settings) {
  DCHECK(active_pref_service_);

  // If a device with the given id does not exist, do nothing.
  auto found_keyboard_iter = keyboards_.find(id);
  if (found_keyboard_iter == keyboards_.end()) {
    RecordSetKeyboardSettingsValidMetric(/*is_valid=*/false);
    return;
  }

  auto& found_keyboard = *found_keyboard_iter->second;
  if (!KeyboardSettingsAreValid(found_keyboard, *settings,
                                policy_handler_->keyboard_policies())) {
    RecordSetKeyboardSettingsValidMetric(/*is_valid=*/false);
    return;
  }
  RecordSetKeyboardSettingsValidMetric(/*is_valid=*/true);
  const auto old_settings = std::move(found_keyboard.settings);
  found_keyboard.settings = settings.Clone();
  keyboard_pref_handler_->UpdateKeyboardSettings(
      active_pref_service_, policy_handler_->keyboard_policies(),
      found_keyboard);
  metrics_manager_->RecordKeyboardChangedMetrics(found_keyboard, *old_settings);
  DispatchKeyboardSettingsChanged(id);
  // Check the list of keyboards to see if any have the same |device_key|.
  // If so, their settings need to also be updated.
  for (const auto& [device_id, keyboard] : keyboards_) {
    if (device_id != found_keyboard.id &&
        keyboard->device_key == found_keyboard.device_key) {
      keyboard->settings = settings->Clone();
      DispatchKeyboardSettingsChanged(device_id);
    }
  }

  RefreshStoredLoginScreenKeyboardSettings();
}

void InputDeviceSettingsControllerImpl::SetTouchpadSettings(
    DeviceId id,
    mojom::TouchpadSettingsPtr settings) {
  DCHECK(active_pref_service_);

  // If a device with the given id does not exist, do nothing.
  auto found_touchpad_iter = touchpads_.find(id);
  if (found_touchpad_iter == touchpads_.end()) {
    RecordSetTouchpadSettingsValidMetric(/*is_valid=*/false);
    return;
  }

  auto& found_touchpad = *found_touchpad_iter->second;
  if (!TouchpadSettingsAreValid(found_touchpad, *settings)) {
    RecordSetTouchpadSettingsValidMetric(/*is_valid=*/false);
    return;
  }
  RecordSetTouchpadSettingsValidMetric(/*is_valid=*/true);
  const auto old_settings = std::move(found_touchpad.settings);
  found_touchpad.settings = settings.Clone();
  touchpad_pref_handler_->UpdateTouchpadSettings(active_pref_service_,
                                                 found_touchpad);
  metrics_manager_->RecordTouchpadChangedMetrics(found_touchpad, *old_settings);
  DispatchTouchpadSettingsChanged(id);
  // Check the list of touchpads to see if any have the same |device_key|.
  // If so, their settings need to also be updated.
  for (const auto& [device_id, touchpad] : touchpads_) {
    if (device_id != found_touchpad.id &&
        touchpad->device_key == found_touchpad.device_key) {
      touchpad->settings = settings->Clone();
      DispatchTouchpadSettingsChanged(device_id);
    }
  }

  RefreshStoredLoginScreenTouchpadSettings();
}

void InputDeviceSettingsControllerImpl::SetMouseSettings(
    DeviceId id,
    mojom::MouseSettingsPtr settings) {
  DCHECK(active_pref_service_);

  // If a device with the given id does not exist, do nothing.
  auto found_mouse_iter = mice_.find(id);
  if (found_mouse_iter == mice_.end()) {
    RecordSetMouseSettingsValidMetric(/*is_valid=*/false);
    return;
  }

  auto& found_mouse = *found_mouse_iter->second;
  if (!MouseSettingsAreValid(found_mouse, *settings)) {
    RecordSetMouseSettingsValidMetric(/*is_valid=*/false);
    return;
  }
  RecordSetMouseSettingsValidMetric(/*is_valid=*/true);
  const auto old_settings = std::move(found_mouse.settings);
  found_mouse.settings = settings.Clone();
  mouse_pref_handler_->UpdateMouseSettings(
      active_pref_service_, policy_handler_->mouse_policies(), found_mouse);
  metrics_manager_->RecordMouseChangedMetrics(found_mouse, *old_settings);
  DispatchMouseSettingsChanged(id);
  // Check the list of mice to see if any have the same |device_key|.
  // If so, their settings need to also be updated.
  for (const auto& [device_id, mouse] : mice_) {
    if (device_id != found_mouse.id &&
        mouse->device_key == found_mouse.device_key) {
      mouse->settings = settings->Clone();
      DispatchMouseSettingsChanged(device_id);
    }
  }
  RefreshStoredLoginScreenMouseSettings();
}

void InputDeviceSettingsControllerImpl::SetPointingStickSettings(
    DeviceId id,
    mojom::PointingStickSettingsPtr settings) {
  DCHECK(active_pref_service_);

  // If a device with the given id does not exist, do nothing.
  auto found_pointing_stick_iter = pointing_sticks_.find(id);
  if (found_pointing_stick_iter == pointing_sticks_.end()) {
    RecordSetPointingStickSettingsValidMetric(/*is_valid=*/false);
    return;
  }
  RecordSetPointingStickSettingsValidMetric(/*is_valid=*/true);

  auto& found_pointing_stick = *found_pointing_stick_iter->second;
  const auto old_settings = std::move(found_pointing_stick.settings);
  found_pointing_stick.settings = settings.Clone();
  pointing_stick_pref_handler_->UpdatePointingStickSettings(
      active_pref_service_, found_pointing_stick);
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
}

void InputDeviceSettingsControllerImpl::SetGraphicsTabletSettings(
    DeviceId id,
    mojom::GraphicsTabletSettingsPtr settings) {
  DCHECK(active_pref_service_);

  // If a device with the given id does not exist, do nothing.
  auto found_graphics_tablet_iter = graphics_tablets_.find(id);
  if (found_graphics_tablet_iter == graphics_tablets_.end()) {
    return;
  }

  auto& found_graphics_tablet = *found_graphics_tablet_iter->second;
  if (!GraphicsTabletSettingsAreValid(found_graphics_tablet, *settings)) {
    return;
  }
  found_graphics_tablet.settings = settings.Clone();
  graphics_tablet_pref_handler_->UpdateGraphicsTabletSettings(
      active_pref_service_, found_graphics_tablet);
  DispatchGraphicsTabletSettingsChanged(id);
  // Check the list of graphics tablets to see if any have the same
  // |device_key|. If so, their settings need to also be updated.
  for (const auto& [device_id, graphics_tablet] : graphics_tablets_) {
    if (device_id != found_graphics_tablet.id &&
        graphics_tablet->device_key == found_graphics_tablet.device_key) {
      graphics_tablet->settings = settings->Clone();
      DispatchGraphicsTabletSettingsChanged(device_id);
    }
  }
}

void InputDeviceSettingsControllerImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void InputDeviceSettingsControllerImpl::RemoveObserver(Observer* observer) {
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

void InputDeviceSettingsControllerImpl::OnKeyboardListUpdated(
    std::vector<ui::KeyboardDevice> keyboards_to_add,
    std::vector<DeviceId> keyboard_ids_to_remove) {
  for (const auto& keyboard : keyboards_to_add) {
    // Get initial settings from the pref manager and generate our local
    // storage of the device.
    auto mojom_keyboard = BuildMojomKeyboard(keyboard);
    InitializeKeyboardSettings(mojom_keyboard.get());
    keyboards_.insert_or_assign(keyboard.id, std::move(mojom_keyboard));
    DispatchKeyboardConnected(keyboard.id);
  }

  for (const auto id : keyboard_ids_to_remove) {
    DispatchKeyboardDisconnectedAndEraseFromList(id);
  }

  RefreshStoredLoginScreenKeyboardSettings();
}

void InputDeviceSettingsControllerImpl::OnTouchpadListUpdated(
    std::vector<ui::TouchpadDevice> touchpads_to_add,
    std::vector<DeviceId> touchpad_ids_to_remove) {
  for (const auto& touchpad : touchpads_to_add) {
    auto mojom_touchpad = BuildMojomTouchpad(touchpad);
    InitializeTouchpadSettings(mojom_touchpad.get());
    touchpads_.insert_or_assign(touchpad.id, std::move(mojom_touchpad));
    DispatchTouchpadConnected(touchpad.id);
  }

  for (const auto id : touchpad_ids_to_remove) {
    DispatchTouchpadDisconnectedAndEraseFromList(id);
  }

  RefreshStoredLoginScreenTouchpadSettings();
}

void InputDeviceSettingsControllerImpl::OnMouseListUpdated(
    std::vector<ui::InputDevice> mice_to_add,
    std::vector<DeviceId> mouse_ids_to_remove) {
  for (const auto& mouse : mice_to_add) {
    auto mojom_mouse = BuildMojomMouse(mouse);
    InitializeMouseSettings(mojom_mouse.get());
    mice_.insert_or_assign(mouse.id, std::move(mojom_mouse));
    DispatchMouseConnected(mouse.id);
  }

  for (const auto id : mouse_ids_to_remove) {
    DispatchMouseDisconnectedAndEraseFromList(id);
  }

  RefreshStoredLoginScreenMouseSettings();
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
    auto mojom_graphics_tablet = BuildMojomGraphicsTablet(graphics_tablet);
    // TODO(wangdanny): Add InitializeGraphicsTabletSettings.
    std::vector<mojom::ButtonRemappingPtr> tablet_button_remappings;
    std::vector<mojom::ButtonRemappingPtr> pen_button_remappings;
    tablet_button_remappings.push_back(mojom::ButtonRemapping::New(
        /*name=*/"test1",
        /*button=*/
        mojom::Button::NewCustomizableButton(mojom::CustomizableButton::kBack),
        /*remapping_action=*/
        mojom::RemappingAction::NewAction(
            ash::AcceleratorAction::kBrightnessUp)));
    mojom_graphics_tablet->settings = mojom::GraphicsTabletSettings::New(
        std::move(tablet_button_remappings), std::move(pen_button_remappings));
    graphics_tablets_.insert_or_assign(graphics_tablet.id,
                                       std::move(mojom_graphics_tablet));
    DispatchGraphicsTabletConnected(graphics_tablet.id);
  }

  for (const auto id : graphics_tablet_ids_to_remove) {
    DispatchGraphicsTabletDisconnectedAndEraseFromList(id);
  }
  // TODO(wangdanny): Add RefreshStoredLoginScreenGraphicsTabletSettings.
}

void InputDeviceSettingsControllerImpl::RestoreDefaultKeyboardRemappings(
    DeviceId id) {
  DCHECK(base::Contains(keyboards_, id));
  auto& keyboard = *keyboards_.at(id);
  mojom::KeyboardSettingsPtr new_settings = keyboard.settings->Clone();
  new_settings->modifier_remappings = {};
  new_settings->six_pack_key_remappings = mojom::SixPackKeyInfo::New();
  if (keyboard.meta_key == mojom::MetaKey::kCommand) {
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
    metrics_manager_->RecordKeyboardInitialMetrics(*keyboard);
    return;
  }

  // Ensure `keyboard.settings` is left in a valid state. This state occurs
  // during OOBE setup and when signing in a new user.
  if (!active_account_id_.has_value() || !local_state_) {
    keyboard_pref_handler_->InitializeWithDefaultKeyboardSettings(
        policy_handler_->keyboard_policies(), keyboard);
    return;
  }

  keyboard_pref_handler_->InitializeLoginScreenKeyboardSettings(
      local_state_, active_account_id_.value(),
      policy_handler_->keyboard_policies(), keyboard);
}

// GetGeneralizedTopRowAreFKeys returns false if there is no keyboard. If there
// is only internal keyboard, GetGeneralizedTopRowAreFKeys returns the
// top_row_are_fkeys of it. If there are multiple keyboards,
// GetGeneralizedTopRowAreFKeys returns the top_row_are_fkeys of latest external
// keyboard which has the largest device id.
bool InputDeviceSettingsControllerImpl::GetGeneralizedTopRowAreFKeys() {
  auto external_iter = base::ranges::find(
      keyboards_.rbegin(), keyboards_.rend(), /*value=*/true,
      [](const auto& keyboard) { return keyboard.second->is_external; });
  auto internal_iter = base::ranges::find(
      keyboards_.rbegin(), keyboards_.rend(), /*value=*/false,
      [](const auto& keyboard) { return keyboard.second->is_external; });
  if (external_iter != keyboards_.rend()) {
    return external_iter->second->settings->top_row_are_fkeys;
  }
  if (internal_iter != keyboards_.rend()) {
    return internal_iter->second->settings->top_row_are_fkeys;
  }
  return false;
}

void InputDeviceSettingsControllerImpl::InitializeMouseSettings(
    mojom::Mouse* mouse) {
  if (active_pref_service_) {
    mouse_pref_handler_->InitializeMouseSettings(
        active_pref_service_, policy_handler_->mouse_policies(), mouse);
    metrics_manager_->RecordMouseInitialMetrics(*mouse);
    return;
  }

  // Ensure `mouse.settings` is left in a valid state. This state occurs
  // during OOBE setup and when signing in a new user.
  if (!active_account_id_.has_value() || !local_state_) {
    mouse_pref_handler_->InitializeWithDefaultMouseSettings(
        policy_handler_->mouse_policies(), mouse);
    return;
  }

  mouse_pref_handler_->InitializeLoginScreenMouseSettings(
      local_state_, active_account_id_.value(),
      policy_handler_->mouse_policies(), mouse);
}

void InputDeviceSettingsControllerImpl::InitializePointingStickSettings(
    mojom::PointingStick* pointing_stick) {
  if (active_pref_service_) {
    pointing_stick_pref_handler_->InitializePointingStickSettings(
        active_pref_service_, pointing_stick);
    metrics_manager_->RecordPointingStickInitialMetrics(*pointing_stick);
    return;
  }

  // Ensure `pointing_stick.settings` is left in a valid state. This state
  // occurs during OOBE setup and when signing in a new user.
  if (!active_account_id_.has_value() || !local_state_) {
    pointing_stick_pref_handler_->InitializeWithDefaultPointingStickSettings(
        pointing_stick);
    return;
  }

  pointing_stick_pref_handler_->InitializeLoginScreenPointingStickSettings(
      local_state_, active_account_id_.value(), pointing_stick);
}

void InputDeviceSettingsControllerImpl::InitializeTouchpadSettings(
    mojom::Touchpad* touchpad) {
  if (active_pref_service_) {
    touchpad_pref_handler_->InitializeTouchpadSettings(active_pref_service_,
                                                       touchpad);
    metrics_manager_->RecordTouchpadInitialMetrics(*touchpad);
    return;
  }

  // Ensure `touchpad.settings` is left in a valid state. This state occurs
  // during OOBE setup and when signing in a new user.
  if (!active_account_id_.has_value() || !local_state_) {
    touchpad_pref_handler_->InitializeWithDefaultTouchpadSettings(touchpad);
    return;
  }

  touchpad_pref_handler_->InitializeLoginScreenTouchpadSettings(
      local_state_, active_account_id_.value(), touchpad);
}

}  // namespace ash
