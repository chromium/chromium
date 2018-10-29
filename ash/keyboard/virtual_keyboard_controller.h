// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_KEYBOARD_VIRTUAL_KEYBOARD_CONTROLLER_H_
#define ASH_KEYBOARD_VIRTUAL_KEYBOARD_CONTROLLER_H_

#include <stdint.h>

#include "ash/ash_export.h"
#include "ash/bluetooth_devices_observer.h"
#include "ash/session/session_observer.h"
#include "ash/wm/tablet_mode/tablet_mode_observer.h"
#include "base/macros.h"
#include "ui/base/ime/chromeos/public/interfaces/ime_keyset.mojom.h"
#include "ui/events/devices/input_device_event_observer.h"
#include "ui/keyboard/keyboard_controller_observer.h"
#include "ui/keyboard/keyboard_layout_delegate.h"

namespace ash {

// This class observes input device changes for the virtual keyboard.
// TODO(https://crbug.com/849995): Should rename this to not confuse it with
// KeyboardController. |ForceShowKeyboardWithKeyset| also does not really
// belong here based on the current class description.
class ASH_EXPORT VirtualKeyboardController
    : public TabletModeObserver,
      public ui::InputDeviceEventObserver,
      public keyboard::KeyboardLayoutDelegate,
      public keyboard::KeyboardControllerObserver,
      public SessionObserver {
 public:
  VirtualKeyboardController();
  ~VirtualKeyboardController() override;

  // Force enable the keyboard and show it with the given keyset: none, emoji,
  // handwriting or voice. Works even in laptop mode.
  void ForceShowKeyboardWithKeyset(
      chromeos::input_method::mojom::ImeKeyset keyset);

  // TabletModeObserver:
  void OnTabletModeEventsBlockingChanged() override;

  // ui::InputDeviceObserver:
  void OnTouchscreenDeviceConfigurationChanged() override;
  void OnKeyboardDeviceConfigurationChanged() override;

  // Toggles whether the presence of an external keyboard should be ignored
  // when determining whether or not to show the on-screen keyboard.
  void ToggleIgnoreExternalKeyboard();

  // keyboard::KeyboardLayoutDelegate:
  void MoveKeyboardToDisplay(const display::Display& display) override;
  void MoveKeyboardToTouchableDisplay() override;

  // keyboard::KeyboardControllerObserver:
  void OnKeyboardEnabledChanged(bool is_enabled) override;
  void OnKeyboardHidden(bool is_temporary_hide) override;

  // SessionObserver:
  void OnActiveUserSessionChanged(const AccountId& account_id) override;

 private:
  // Updates the list of active input devices.
  void UpdateDevices();

  // Updates the keyboard state.
  void UpdateKeyboardEnabled();

  // Creates the keyboard if |enabled|, else destroys it.
  void SetKeyboardEnabled(bool enabled);

  // Force enable the keyboard and show it, even in laptop mode.
  void ForceShowKeyboard();

  // Callback function of |bluetooth_devices_observer_|. Called when |device|
  // changes.
  void UpdateBluetoothDevice(device::BluetoothDevice* device);

  // True if an external keyboard is connected.
  bool has_external_keyboard_;
  // True if an internal keyboard is connected.
  bool has_internal_keyboard_;
  // True if a touchscreen is connected.
  bool has_touchscreen_;
  // True if the presence of an external keyboard should be ignored.
  bool ignore_external_keyboard_;

  // Observer to observe the bluetooth devices.
  std::unique_ptr<BluetoothDevicesObserver> bluetooth_devices_observer_;

  DISALLOW_COPY_AND_ASSIGN(VirtualKeyboardController);
};

}  // namespace ash

#endif  // ASH_KEYBOARD_VIRTUAL_KEYBOARD_CONTROLLER_H_
