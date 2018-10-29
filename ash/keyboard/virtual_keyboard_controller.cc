// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/keyboard/virtual_keyboard_controller.h"

#include <vector>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/ime/ime_controller.h"
#include "ash/keyboard/ash_keyboard_controller.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_util.h"
#include "base/command_line.h"
#include "base/strings/string_util.h"
#include "ui/base/emoji/emoji_panel_helper.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/devices/input_device_manager.h"
#include "ui/events/devices/touchscreen_device.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/keyboard/keyboard_switches.h"
#include "ui/keyboard/keyboard_util.h"

namespace ash {
namespace {

// Checks if virtual keyboard is force-enabled by enable-virtual-keyboard flag.
bool IsVirtualKeyboardEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      keyboard::switches::kEnableVirtualKeyboard);
}

void ResetVirtualKeyboard() {
  keyboard::SetKeyboardEnabledFromShelf(false);
  if (!keyboard::IsKeyboardEnabled())
    Shell::Get()->DisableKeyboard();

  // Reset the keyset after disabling the virtual keyboard to prevent the IME
  // extension from accidentally loading the default keyset while it's shutting
  // down. See https://crbug.com/875456.
  Shell::Get()->ime_controller()->OverrideKeyboardKeyset(
      chromeos::input_method::mojom::ImeKeyset::kNone);
}

void MoveKeyboardToDisplayInternal(const display::Display& display) {
  // Remove the keyboard from curent root window controller
  TRACE_EVENT0("vk", "MoveKeyboardToDisplayInternal");
  auto* ash_keyboard_controller = Shell::Get()->ash_keyboard_controller();
  ash_keyboard_controller->DeactivateKeyboard();

  for (RootWindowController* controller :
       Shell::Get()->GetAllRootWindowControllers()) {
    if (display::Screen::GetScreen()
            ->GetDisplayNearestWindow(controller->GetRootWindow())
            .id() == display.id()) {
      ash_keyboard_controller->ActivateKeyboardForRoot(controller);
      break;
    }
  }
}

bool HasTouchableDisplay() {
  for (const auto& display : display::Screen::GetScreen()->GetAllDisplays()) {
    if (display.touch_support() == display::Display::TouchSupport::AVAILABLE)
      return true;
  }
  return false;
}

void MoveKeyboardToFirstTouchableDisplay() {
  // Move the keyboard to the first display with touch capability.
  for (const auto& display : display::Screen::GetScreen()->GetAllDisplays()) {
    if (display.touch_support() == display::Display::TouchSupport::AVAILABLE) {
      MoveKeyboardToDisplayInternal(display);
      return;
    }
  }
}

}  // namespace

VirtualKeyboardController::VirtualKeyboardController()
    : has_external_keyboard_(false),
      has_internal_keyboard_(false),
      has_touchscreen_(false),
      ignore_external_keyboard_(false) {
  Shell::Get()->tablet_mode_controller()->AddObserver(this);
  Shell::Get()->session_controller()->AddObserver(this);
  ui::InputDeviceManager::GetInstance()->AddObserver(this);
  UpdateDevices();

  // Set callback to show the emoji panel
  ui::SetShowEmojiKeyboardCallback(base::BindRepeating(
      &VirtualKeyboardController::ForceShowKeyboardWithKeyset,
      base::Unretained(this),
      chromeos::input_method::mojom::ImeKeyset::kEmoji));

  keyboard::KeyboardController::Get()->AddObserver(this);

  bluetooth_devices_observer_ = std::make_unique<BluetoothDevicesObserver>(
      base::BindRepeating(&VirtualKeyboardController::UpdateBluetoothDevice,
                          base::Unretained(this)));
}

VirtualKeyboardController::~VirtualKeyboardController() {
  keyboard::KeyboardController::Get()->RemoveObserver(this);

  if (Shell::Get()->tablet_mode_controller())
    Shell::Get()->tablet_mode_controller()->RemoveObserver(this);
  if (Shell::Get()->session_controller())
    Shell::Get()->session_controller()->RemoveObserver(this);
  ui::InputDeviceManager::GetInstance()->RemoveObserver(this);

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

void VirtualKeyboardController::OnTouchscreenDeviceConfigurationChanged() {
  UpdateDevices();
}

void VirtualKeyboardController::OnKeyboardDeviceConfigurationChanged() {
  UpdateDevices();
}

void VirtualKeyboardController::ToggleIgnoreExternalKeyboard() {
  ignore_external_keyboard_ = !ignore_external_keyboard_;
  UpdateKeyboardEnabled();
}

void VirtualKeyboardController::MoveKeyboardToDisplay(
    const display::Display& display) {
  DCHECK(keyboard::KeyboardController::Get()->IsEnabled());
  DCHECK(display.is_valid());

  TRACE_EVENT0("vk", "MoveKeyboardToDisplay");

  aura::Window* keyboard_window =
      keyboard::KeyboardController::Get()->GetKeyboardWindow();
  DCHECK(keyboard_window);

  const display::Screen* screen = display::Screen::GetScreen();
  const display::Display current_display =
      screen->GetDisplayNearestWindow(keyboard_window);

  if (display.id() != current_display.id())
    MoveKeyboardToDisplayInternal(display);
}

void VirtualKeyboardController::MoveKeyboardToTouchableDisplay() {
  DCHECK(keyboard::KeyboardController::Get()->IsEnabled());

  TRACE_EVENT0("vk", "MoveKeyboardToTouchableDisplay");

  aura::Window* keyboard_window =
      keyboard::KeyboardController::Get()->GetKeyboardWindow();
  DCHECK(keyboard_window);

  const display::Screen* screen = display::Screen::GetScreen();
  const display::Display current_display =
      screen->GetDisplayNearestWindow(keyboard_window);

  if (wm::GetFocusedWindow()) {
    // Move the virtual keyboard to the focused display if that display has
    // touch capability or no other display has touch capability.
    const display::Display focused_display =
        display::Screen::GetScreen()->GetDisplayNearestWindow(
            wm::GetFocusedWindow());
    if (current_display.id() != focused_display.id() &&
        focused_display.is_valid() &&
        (focused_display.touch_support() ==
             display::Display::TouchSupport::AVAILABLE ||
         !HasTouchableDisplay())) {
      MoveKeyboardToDisplayInternal(focused_display);
      return;
    }
  }

  if (current_display.touch_support() !=
      display::Display::TouchSupport::AVAILABLE) {
    // The keyboard is currently on the display without touch capability.
    MoveKeyboardToFirstTouchableDisplay();
  }
}

void VirtualKeyboardController::UpdateDevices() {
  ui::InputDeviceManager* device_data_manager =
      ui::InputDeviceManager::GetInstance();

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
    SetKeyboardEnabled(Shell::Get()
                           ->tablet_mode_controller()
                           ->AreInternalInputDeviceEventsBlocked());
    return;
  }
  bool ignore_internal_keyboard = Shell::Get()
                                      ->tablet_mode_controller()
                                      ->AreInternalInputDeviceEventsBlocked();
  bool is_internal_keyboard_active =
      has_internal_keyboard_ && !ignore_internal_keyboard;
  SetKeyboardEnabled(!is_internal_keyboard_active && has_touchscreen_ &&
                     (!has_external_keyboard_ || ignore_external_keyboard_));
  Shell::Get()->system_tray_notifier()->NotifyVirtualKeyboardSuppressionChanged(
      !is_internal_keyboard_active && has_touchscreen_ &&
      has_external_keyboard_);
}

void VirtualKeyboardController::SetKeyboardEnabled(bool enabled) {
  bool was_enabled = keyboard::IsKeyboardEnabled();
  keyboard::SetTouchKeyboardEnabled(enabled);
  bool is_enabled = keyboard::IsKeyboardEnabled();
  if (is_enabled == was_enabled)
    return;
  if (is_enabled) {
    Shell::Get()->EnableKeyboard();
  } else {
    Shell::Get()->DisableKeyboard();
  }
}

void VirtualKeyboardController::ForceShowKeyboard() {
  // If the virtual keyboard is enabled, show the keyboard directly.
  auto* keyboard_controller = keyboard::KeyboardController::Get();
  if (keyboard_controller->IsEnabled()) {
    keyboard_controller->ShowKeyboard(false /* locked */);
    return;
  }

  // Otherwise, temporarily enable the virtual keyboard until it is dismissed.
  DCHECK(!keyboard::GetKeyboardEnabledFromShelf());
  keyboard::SetKeyboardEnabledFromShelf(true);
  Shell::Get()->EnableKeyboard();
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
  if (keyboard::IsKeyboardEnabled())
    Shell::Get()->EnableKeyboard();
}

void VirtualKeyboardController::UpdateBluetoothDevice(
    device::BluetoothDevice* device) {
  // We only care about keyboard type bluetooth device change.
  if (device->GetDeviceType() != device::BluetoothDeviceType::KEYBOARD &&
      device->GetDeviceType() !=
          device::BluetoothDeviceType::KEYBOARD_MOUSE_COMBO) {
    return;
  }

  UpdateDevices();
}

}  // namespace ash
