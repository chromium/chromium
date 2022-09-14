// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/keyboard_brightness/keyboard_brightness_controller.h"

#include "base/logging.h"
#include "chromeos/dbus/power_manager/backlight.pb.h"

namespace ash {

KeyboardBrightnessController::KeyboardBrightnessController() {
  chromeos::PowerManagerClient::Get()->AddObserver(this);
  chromeos::PowerManagerClient::Get()->GetKeyboardBacklightToggledOff(
      base::BindOnce(
          &KeyboardBrightnessController::KeyboardBacklightToggledOffReceived,
          weak_ptr_factory_.GetWeakPtr()));
}

KeyboardBrightnessController::~KeyboardBrightnessController() {
  chromeos::PowerManagerClient::Get()->RemoveObserver(this);
}

void KeyboardBrightnessController::HandleKeyboardBrightnessDown() {
  chromeos::PowerManagerClient::Get()->DecreaseKeyboardBrightness();
}

void KeyboardBrightnessController::HandleKeyboardBrightnessUp() {
  chromeos::PowerManagerClient::Get()->IncreaseKeyboardBrightness();
}

void KeyboardBrightnessController::KeyboardBrightnessChanged(
    const power_manager::BacklightBrightnessChange& change) {
  // Officially set our stored toggle state.  We're toggled off if
  // power_manager says we are, otherwise we're toggled on.
  keyboard_backlight_toggled_off_ =
      change.cause() ==
      power_manager::BacklightBrightnessChange_Cause_USER_TOGGLED_OFF;
}

void KeyboardBrightnessController::KeyboardBacklightToggledOffReceived(
    absl::optional<bool> toggled_off) {
  // Initialize our toggle state.
  if (!toggled_off.has_value()) {
    LOG(ERROR) << __FUNCTION__ << " No value present for toggled_off";
    return;
  }
  keyboard_backlight_toggled_off_ = toggled_off.value();
}

void KeyboardBrightnessController::HandleToggleKeyboardBacklight() {
  // User has explicitly toggled the KBL.  A toggle state with no value means
  // we have yet to receive the initial value from power_manager.
  if (keyboard_backlight_toggled_off_.has_value()) {
    // Tell power_manager to set the toggle state to the opposite of our
    // stored value.  We'll update the stored value when we receive the next
    // brightness change.
    chromeos::PowerManagerClient::Get()->SetKeyboardBacklightToggledOff(
        !keyboard_backlight_toggled_off_.value());
  }
}

}  // namespace ash
