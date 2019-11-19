// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/keyboard/virtual_keyboard_controller.h"

#include <vector>

#include "ash/ime/ime_controller.h"
#include "ash/keyboard/keyboard_controller_impl.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/keyboard/ui/keyboard_util.h"
#include "ash/public/cpp/keyboard/keyboard_switches.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/strings/string_util.h"
#include "ui/base/emoji/emoji_panel_helper.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/devices/touchscreen_device.h"

namespace ash {
namespace {

// Checks if virtual keyboard is force-enabled by enable-virtual-keyboard flag.
bool IsVirtualKeyboardEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      keyboard::switches::kEnableVirtualKeyboard);
}

void ResetVirtualKeyboard() {
  keyboard::SetKeyboardEnabledFromShelf(false);

  // Reset the keyset after disabling the virtual keyboard to prevent the IME
  // extension from accidentally loading the default keyset while it's shutting
  // down. See https://crbug.com/875456.
  Shell::Get()->ime_controller()->OverrideKeyboardKeyset(
      chromeos::input_method::mojom::ImeKeyset::kNone);
}

}  // namespace

VirtualKeyboardController::VirtualKeyboardController()
    : has_external_keyboard_(false),
      has_internal_keyboard_(false),
      has_touchscreen_(false),
      ignore_external_keyboard_(false) {
  Shell::Get()->tablet_mode_controller()->AddObserver(this);
  Shell::Get()->session_controller()->AddObserver(this);
  ui::DeviceDataManager::GetInstance()->AddObserver(this);
  UpdateDevices();

  // Set callback to show the emoji panel
  ui::SetShowEmojiKeyboardCallback(base::BindRepeating(
      &VirtualKeyboardController::ForceShowKeyboardWithKeyset,
      base::Unretained(this),
      chromeos::input_method::mojom::ImeKeyset::kEmoji));

  keyboard::KeyboardUIController::Get()->AddObserver(this);

  bluetooth_devices_observer_ =
      std::make_unique<BluetoothDevicesObserver>(base::BindRepeating(
          &VirtualKeyboardController::OnBluetoothAdapterOrDeviceChanged,
          base::Unretained(this)));
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
    chromeos::input_method::mojom::ImeKeyset keyset) {
  Shell::Get()->ime_controller()->OverrideKeyboardKeyset(
      keyset, base::BindOnce(&VirtualKeyboardController::ForceShowKeyboard,
                             base::Unretained(this)));
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

  // Checks for touchscreens.
  has_touchscreen_ = device_data_manager->GetTouchscreenDevices().size() > 0;

  // Checks for keyboards.
  has_external_keyboard_ = false;
  has_internal_keyboard_ = false;
  for (const ui::InputDevice& device :
       device_data_manager->GetKeyboardDevices()) {
    if (has_internal_keyboard_ && has_external_keyboard_)
      break;
    ui::InputDeviceType type = device.type;
    if (type == ui::InputDeviceType::INPUT_DEVICE_INTERNAL)
      has_internal_keyboard_ = true;
    if (type == ui::InputDeviceType::INPUT_DEVICE_USB ||
        (type == ui::InputDeviceType::INPUT_DEVICE_BLUETOOTH &&
         bluetooth_devices_observer_->IsConnectedBluetoothDevice(device))) {
      has_external_keyboard_ = true;
    }
  }
  // Update keyboard state.
  UpdateKeyboardEnabled();
}

void VirtualKeyboardController::UpdateKeyboardEnabled() {
  if (IsVirtualKeyboardEnabled()) {
    keyboard::SetTouchKeyboardEnabled(
        Shell::Get()
            ->tablet_mode_controller()
            ->AreInternalInputDeviceEventsBlocked());
    return;
  }
  bool ignore_internal_keyboard = Shell::Get()
                                      ->tablet_mode_controller()
                                      ->AreInternalInputDeviceEventsBlocked();
  bool is_internal_keyboard_active =
      has_internal_keyboard_ && !ignore_internal_keyboard;
  keyboard::SetTouchKeyboardEnabled(
      !is_internal_keyboard_active && has_touchscreen_ &&
      (!has_external_keyboard_ || ignore_external_keyboard_));
  Shell::Get()->system_tray_notifier()->NotifyVirtualKeyboardSuppressionChanged(
      !is_internal_keyboard_active && has_touchscreen_ &&
      has_external_keyboard_);
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

void VirtualKeyboardController::OnKeyboardEnabledChanged(bool is_enabled) {
  if (!is_enabled) {
    // TODO(shend/shuchen): Consider moving this logic to ImeController.
    // https://crbug.com/896284.
    Shell::Get()->ime_controller()->OverrideKeyboardKeyset(
        chromeos::input_method::mojom::ImeKeyset::kNone);
  }
}

void VirtualKeyboardController::OnKeyboardHidden(bool is_temporary_hide) {
  // The keyboard may temporarily hide (e.g. to change container behaviors).
  // The keyset should not be reset in this case.
  if (is_temporary_hide)
    return;

  // Post a task to reset the virtual keyboard to its original state.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
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

}  // namespace ash
