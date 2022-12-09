// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/keyboard_brightness/keyboard_brightness_controller.h"

#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/dbus/power_manager/backlight.pb.h"

namespace ash {

void KeyboardBrightnessController::HandleKeyboardBrightnessDown() {
  chromeos::PowerManagerClient::Get()->DecreaseKeyboardBrightness();
}

void KeyboardBrightnessController::HandleKeyboardBrightnessUp() {
  chromeos::PowerManagerClient::Get()->IncreaseKeyboardBrightness();
}

void KeyboardBrightnessController::HandleToggleKeyboardBacklight() {
  chromeos::PowerManagerClient::Get()->ToggleKeyboardBacklight();
}

}  // namespace ash
