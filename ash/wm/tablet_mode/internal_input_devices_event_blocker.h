// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_TABLET_MODE_INTERNAL_INPUT_DEVICES_EVENT_BLOCKER_H_
#define ASH_WM_TABLET_MODE_INTERNAL_INPUT_DEVICES_EVENT_BLOCKER_H_

#include "ash/ash_export.h"
#include "ui/events/devices/input_device_event_observer.h"

namespace ash {

// Helper class to temporarily disable the internal touchpad and keyboard.
class ASH_EXPORT InternalInputDevicesEventBlocker
    : public ui::InputDeviceEventObserver {
 public:
  InternalInputDevicesEventBlocker();

  InternalInputDevicesEventBlocker(const InternalInputDevicesEventBlocker&) =
      delete;
  InternalInputDevicesEventBlocker& operator=(
      const InternalInputDevicesEventBlocker&) = delete;

  ~InternalInputDevicesEventBlocker() override;

  // ui::InputDeviceEventObserver:
  void OnInputDeviceConfigurationChanged(uint8_t input_device_types) override;

  void UpdateInternalInputDevices(bool should_block);

  bool should_be_blocked() const { return should_be_blocked_; }

 private:
  bool HasInternalTouchpad();
  bool HasInternalKeyboard();

  void UpdateInternalTouchpad(bool should_block);
  void UpdateInternalKeyboard(bool should_block);

  // |should_be_blocked_| might not be equal to (|is_touchpad_blocked_| &&
  // |is_keyboard_blocked_|) as when UpdateInternalInputDevices() is called,
  // |should_be_blocked_| is guranteed to be updated, but |is_touchpad_blocked_|
  // or |is_keyboard_blocked_| might not be updated because there might not be
  // an internal touchpad or keyboard at that moment (currently it can only be
  // the case for Whiskers keyboard, which is a detachable keyboard but is
  // regarded as the internal keyboard for Moewth).
  bool should_be_blocked_ = false;

  bool is_touchpad_blocked_ = false;
  bool is_keyboard_blocked_ = false;
};

}  // namespace ash

#endif  // ASH_WM_TABLET_MODE_INTERNAL_INPUT_DEVICES_EVENT_BLOCKER_H_
