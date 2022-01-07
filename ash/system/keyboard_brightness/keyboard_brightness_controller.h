// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_KEYBOARD_BRIGHTNESS_KEYBOARD_BRIGHTNESS_CONTROLLER_H_
#define ASH_SYSTEM_KEYBOARD_BRIGHTNESS_KEYBOARD_BRIGHTNESS_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/system/keyboard_brightness_control_delegate.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

// A class which controls keyboard brightness when Alt+F6, Alt+F7 or a
// multimedia key for keyboard brightness is pressed.
class ASH_EXPORT KeyboardBrightnessController
    : public KeyboardBrightnessControlDelegate,
      public chromeos::PowerManagerClient::Observer {
 public:
  KeyboardBrightnessController();

  KeyboardBrightnessController(const KeyboardBrightnessController&) = delete;
  KeyboardBrightnessController& operator=(const KeyboardBrightnessController&) =
      delete;

  ~KeyboardBrightnessController() override;

  // chromeos::PowerManagerClient
  void KeyboardBrightnessChanged(
      const power_manager::BacklightBrightnessChange& change) override;

 private:
  // Overridden from KeyboardBrightnessControlDelegate:
  void HandleKeyboardBrightnessDown(
      const ui::Accelerator& accelerator) override;
  void HandleKeyboardBrightnessUp(const ui::Accelerator& accelerator) override;
  void HandleToggleKeyboardBacklight() override;

  // Callbacks:
  void KeyboardBacklightToggledOffReceived(absl::optional<bool> toggled_off);

  // The current toggle state of the keyboard backlight.  This value is true
  // if the KBL is toggled off, false otherwise.  No value present means
  // power_manager has not yet informed us one way or the other.
  absl::optional<bool> keyboard_backlight_toggled_off_;

  // This has to be last, so it gets destroyed last.
  base::WeakPtrFactory<KeyboardBrightnessController> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_KEYBOARD_BRIGHTNESS_KEYBOARD_BRIGHTNESS_CONTROLLER_H_
