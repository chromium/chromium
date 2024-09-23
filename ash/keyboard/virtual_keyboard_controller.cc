// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/keyboard/virtual_keyboard_controller.h"

#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/ime/ime_controller_impl.h"
#include "ash/keyboard/keyboard_controller_impl.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/keyboard/ui/keyboard_util.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_runner.h"
#include "ui/base/emoji/emoji_panel_helper.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/input_device.h"

namespace ash {
namespace {

void ResetVirtualKeyboard() {
  keyboard::SetKeyboardEnabledFromShelf(false);

  // This function can get called asynchronously after the shell has been
  // destroyed, so check for an instance.
  if (Shell::HasInstance()) {
    // Reset the keyset after disabling the virtual keyboard to prevent the IME
    // extension from accidentally loading the default keyset while it's
    // shutting down. See https://crbug.com/875456.
    Shell::Get()->ime_controller()->OverrideKeyboardKeyset(
        input_method::ImeKeyset::kNone);
  }
}

}  // namespace

VirtualKeyboardController::VirtualKeyboardController()
    : ignore_external_keyboard_(false),
      ignore_internal_keyboard_(false),
      bluetooth_devices_observer_(
          std::make_unique<BluetoothDevicesObserver>(base::BindRepeating(
              &VirtualKeyboardController::OnBluetoothAdapterOrDeviceChanged,
              base::Unretained(this)))) {
  Shell::Get()->tablet_mode_controller()->AddObserver(this);
  Shell::Get()->session_controller()->AddObserver(this);
  ui::DeviceDataManager::GetInstance()->AddObserver(this);
  UpdateDevices();

  // Set callback to show the emoji panel
  ui::SetTabletModeShowEmojiKeyboardCallback(base::BindRepeating(
      &VirtualKeyboardController::ForceShowKeyboardWithKeyset,
      base::Unretained(this), input_method::ImeKeyset::kEmoji));
  keyboard::KeyboardUIController::Get()->AddObserver(this);
}

VirtualKeyboardController::~VirtualKeyboardController() {
  keyboard::KeyboardUIController::Get()->RemoveObserver(this);

  if (Shell::Get()->tablet_mode_controller())
    Shell::Get()->tablet_mode_controller()->RemoveObserver(this);
  if (Shell::Get()->session_controller())
    Shell::Get()->session_controller()->RemoveObserver(this);
  ui::DeviceDataManager::GetInstance()->RemoveObserver(this);

  // Reset the emoji panel callback
  ui::SetShowEmojiKeyboardCallback(base::DoNothing());
}

void VirtualKeyboardController::ForceShowKeyboardWithKeyset(
    input_method::ImeKeyset keyset) {
  Shell::Get()->ime_controller()->OverrideKeyboardKeyset(
      keyset, base::BindOnce(&VirtualKeyboardController::ForceShowKeyboard,
                             base::Unretained(this)));
}

void VirtualKeyboardController::ForceShowKeyboard() {
  // If the virtual keyboard is enabled, show the keyboard directly.
  auto* keyboard_controller = keyboard::KeyboardUIController::Get();
  if (keyboard_controller->IsEnabled()) {
    keyboard_controller->ShowKeyboard(false /* locked */);
    return;
  }

  // Otherwise, temporarily enable the virtual keyboard until it is dismissed.
  DCHECK(!keyboard::GetKeyboardEnabledFromShelf());
  keyboard::SetKeyboardEnabledFromShelf(true);
  keyboard_controller->ShowKeyboard(false);
}

void VirtualKeyboardController::OnTabletModeEventsBlockingChanged() {
  UpdateKeyboardEnabled();
}

void VirtualKeyboardController::OnInputDeviceConfigurationChanged(
    uint8_t input_device_types) {
  if (input_device_types & (ui::InputDeviceEventObserver::kKeyboard |
                            ui::InputDeviceEventObserver::kTouchscreen)) {
    UpdateDevices();
  }
}

void VirtualKeyboardController::ToggleIgnoreExternalKeyboard() {
  ignore_external_keyboard_ = !ignore_external_keyboard_;
  UpdateKeyboardEnabled();
}

void VirtualKeyboardController::UpdateDevices() {
  ui::DeviceDataManager* device_data_manager =
      ui::DeviceDataManager::GetInstance();

  touchscreens_ = device_data_manager->GetTouchscreenDevices();
  // Checks for keyboards.
  external_keyboards_.clear();
  internal_keyboard_name_.reset();
  for (const ui::InputDevice& device :
       device_data_manager->GetKeyboardDevices()) {
    ui::InputDeviceType type = device.type;
    if (type == ui::InputDeviceType::INPUT_DEVICE_INTERNAL)
      internal_keyboard_name_ = device.name;
    if ((type == ui::InputDeviceType::INPUT_DEVICE_USB ||
         (type == ui::InputDeviceType::INPUT_DEVICE_BLUETOOTH &&
          bluetooth_devices_observer_->IsConnectedBluetoothDevice(device))) &&
        !device.suspected_keyboard_imposter) {
      external_keyboards_.push_back(device);
    }
  }
  // Update keyboard state.
  UpdateKeyboardEnabled();
}

void VirtualKeyboardController::UpdateKeyboardEnabled() {
  ignore_internal_keyboard_ = Shell::Get()
                                  ->tablet_mode_controller()
                                  ->AreInternalInputDeviceEventsBlocked();
  bool is_internal_keyboard_active =
      internal_keyboard_name_ && !ignore_internal_keyboard_;
  keyboard::SetTouchKeyboardEnabled(
      !is_internal_keyboard_active && !touchscreens_.empty() &&
      (external_keyboards_.empty() || ignore_external_keyboard_));
  Shell::Get()->system_tray_notifier()->NotifyVirtualKeyboardSuppressionChanged(
      !is_internal_keyboard_active && !touchscreens_.empty() &&
      !external_keyboards_.empty());
}

void VirtualKeyboardController::OnKeyboardHidden(bool is_temporary_hide) {
  // The keyboard may temporarily hide (e.g. to change container behaviors).
  // The keyset should not be reset in this case.
  if (is_temporary_hide)
    return;

  // Post a task to reset the virtual keyboard to its original state.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(ResetVirtualKeyboard));
}

void VirtualKeyboardController::OnActiveUserSessionChanged(
    const AccountId& account_id) {
  // Force on-screen keyboard to reset.
  Shell::Get()->keyboard_controller()->RebuildKeyboardIfEnabled();
}

void VirtualKeyboardController::OnBluetoothAdapterOrDeviceChanged(
    device::BluetoothDevice* device) {
  // We only care about keyboard type bluetooth device change.
  if (!device ||
      device->GetDeviceType() == device::BluetoothDeviceType::KEYBOARD ||
      device->GetDeviceType() ==
          device::BluetoothDeviceType::KEYBOARD_MOUSE_COMBO) {
    UpdateDevices();
  }
}

const std::optional<std::string>&
VirtualKeyboardController::GetInternalKeyboardName() const {
  return internal_keyboard_name_;
}

const std::vector<ui::InputDevice>&
VirtualKeyboardController::GetExternalKeyboards() const {
  return external_keyboards_;
}

const std::vector<ui::TouchscreenDevice>&
VirtualKeyboardController::GetTouchscreens() const {
  return touchscreens_;
}

bool VirtualKeyboardController::IsInternalKeyboardIgnored() const {
  return ignore_internal_keyboard_;
}

bool VirtualKeyboardController::IsExternalKeyboardIgnored() const {
  return ignore_external_keyboard_;
}

}  // namespace ash
