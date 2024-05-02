// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_KEYBOARD_BRIGHTNESS_KEYBOARD_BRIGHTNESS_CONTROLLER_H_
#define ASH_SYSTEM_KEYBOARD_BRIGHTNESS_KEYBOARD_BRIGHTNESS_CONTROLLER_H_

#include <optional>

#include "ash/ash_export.h"
#include "ash/system/keyboard_brightness_control_delegate.h"
#include "base/memory/weak_ptr.h"

namespace ash {

// A class which controls keyboard brightness when Alt+F6, Alt+F7 or a
// multimedia key for keyboard brightness is pressed.
class ASH_EXPORT KeyboardBrightnessController
    : public KeyboardBrightnessControlDelegate {
 public:
  KeyboardBrightnessController();

  // Disallow copy and move.
  KeyboardBrightnessController(const KeyboardBrightnessController&) = delete;
  KeyboardBrightnessController& operator=(const KeyboardBrightnessController&) =
      delete;

  ~KeyboardBrightnessController() override;

 private:
  // Overridden from KeyboardBrightnessControlDelegate:
  void HandleKeyboardBrightnessDown() override;
  void HandleKeyboardBrightnessUp() override;
  void HandleToggleKeyboardBacklight() override;
  void HandleSetKeyboardBrightness(double percent, bool gradual) override;
  void HandleGetKeyboardBrightness(
      base::OnceCallback<void(std::optional<double>)> callback) override;
  void HandleSetKeyboardAmbientLightSensorEnabled(bool enabled) override;
  void HandleGetKeyboardAmbientLightSensorEnabled(
      base::OnceCallback<void(std::optional<bool>)> callback) override;

  void OnReceiveHasKeyboardBacklight(std::optional<bool> has_backlight);

  base::WeakPtrFactory<KeyboardBrightnessController> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_KEYBOARD_BRIGHTNESS_KEYBOARD_BRIGHTNESS_CONTROLLER_H_
