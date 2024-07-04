// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_KEYBOARD_VIRTUAL_KEYBOARD_CONTROLLER_H_
#define ASH_KEYBOARD_VIRTUAL_KEYBOARD_CONTROLLER_H_

#include <stdint.h>
#include <vector>

#include "ash/ash_export.h"
#include "ash/bluetooth_devices_observer.h"
#include "ash/public/cpp/keyboard/keyboard_controller_observer.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/public/cpp/tablet_mode_observer.h"
#include "ui/base/ime/ash/ime_keyset.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/devices/input_device_event_observer.h"
#include "ui/events/devices/touchscreen_device.h"

namespace ash {

// This class observes input device changes for the virtual keyboard.
// TODO(crbug.com/41392988): Should rename this to not confuse it with
// KeyboardController. |ForceShowKeyboardWithKeyset| also does not really
// belong here based on the current class description.
class ASH_EXPORT VirtualKeyboardController
    : public TabletModeObserver,
      public ui::InputDeviceEventObserver,
      public KeyboardControllerObserver,
      public SessionObserver {
 public:
  VirtualKeyboardController();

  VirtualKeyboardController(const VirtualKeyboardController&) = delete;
  VirtualKeyboardController& operator=(const VirtualKeyboardController&) =
      delete;

  ~VirtualKeyboardController() override;

  // Force enable the keyboard and show it with the given keyset: none, emoji,
  // handwriting or voice. Works even in laptop mode.
  void ForceShowKeyboardWithKeyset(input_method::ImeKeyset keyset);

  // Force enable the keyboard and show it, even in laptop mode.
  void ForceShowKeyboard();

  // TabletModeObserver:
  void OnTabletModeEventsBlockingChanged() override;

  // ui::InputDeviceEventObserver:
  void OnInputDeviceConfigurationChanged(uint8_t input_device_types) override;

  // Toggles whether the presence of an external keyboard should be ignored
  // when determining whether or not to show the on-screen keyboard.
  void ToggleIgnoreExternalKeyboard();

  // KeyboardControllerObserver:
  void OnKeyboardHidden(bool is_temporary_hide) override;

  // SessionObserver:
  void OnActiveUserSessionChanged(const AccountId& account_id) override;

  const std::optional<std::string>& GetInternalKeyboardName() const;

  const std::vector<ui::InputDevice>& GetExternalKeyboards() const;

  const std::vector<ui::TouchscreenDevice>& GetTouchscreens() const;

  // Returns true if the device is in tablet mode, meaning the user does not
  // have access to the internal keyboard.
  bool IsInternalKeyboardIgnored() const;

  bool IsExternalKeyboardIgnored() const;

 private:
  // Updates the list of active input devices.
  void UpdateDevices();

  // Updates the keyboard state.
  void UpdateKeyboardEnabled();

  // Callback function of |bluetooth_devices_observer_|. Called when the
  // bluetooth adapter or |device| changes.
  void OnBluetoothAdapterOrDeviceChanged(device::BluetoothDevice* device);

  // True if an internal keyboard is connected.
  std::optional<std::string> internal_keyboard_name_;
  // Contains any potential external keyboards (May contain imposter keyboards).
  std::vector<ui::InputDevice> external_keyboards_;
  // Contains all touch screens devices (both internal and external).
  std::vector<ui::TouchscreenDevice> touchscreens_;

  // True if the presence of an external keyboard should be ignored.
  bool ignore_external_keyboard_;
  // True if the presence of an internal keyboard should be ignored.
  bool ignore_internal_keyboard_;

  // Observer to observe the bluetooth devices.
  std::unique_ptr<BluetoothDevicesObserver> bluetooth_devices_observer_;
};

}  // namespace ash

#endif  // ASH_KEYBOARD_VIRTUAL_KEYBOARD_CONTROLLER_H_
