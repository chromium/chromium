// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/keyboard_brightness/keyboard_brightness_controller.h"

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/dbus/power_manager/backlight.pb.h"

namespace ash {

KeyboardBrightnessController::KeyboardBrightnessController() {
  chromeos::PowerManagerClient* power_manager_client =
      chromeos::PowerManagerClient::Get();
  DCHECK(power_manager_client);
  // Record whether the keyboard has a backlight for metric collection.
  power_manager_client->HasKeyboardBacklight(base::BindOnce(
      &KeyboardBrightnessController::OnReceiveHasKeyboardBacklight,
      weak_ptr_factory_.GetWeakPtr()));
}

KeyboardBrightnessController::~KeyboardBrightnessController() = default;

void KeyboardBrightnessController::HandleKeyboardBrightnessDown() {
  chromeos::PowerManagerClient::Get()->DecreaseKeyboardBrightness();
}

void KeyboardBrightnessController::HandleKeyboardBrightnessUp() {
  chromeos::PowerManagerClient::Get()->IncreaseKeyboardBrightness();
}

void KeyboardBrightnessController::HandleToggleKeyboardBacklight() {
  chromeos::PowerManagerClient::Get()->ToggleKeyboardBacklight();
}

void KeyboardBrightnessController::HandleSetKeyboardBrightness(double percent,
                                                               bool gradual) {
  power_manager::SetBacklightBrightnessRequest request;
  request.set_percent(percent);
  request.set_transition(
      gradual
          ? power_manager::SetBacklightBrightnessRequest_Transition_FAST
          : power_manager::SetBacklightBrightnessRequest_Transition_INSTANT);
  request.set_cause(
      power_manager::SetBacklightBrightnessRequest_Cause_USER_REQUEST);
  chromeos::PowerManagerClient::Get()->SetKeyboardBrightness(request);
}

void KeyboardBrightnessController::HandleGetKeyboardBrightness(
    base::OnceCallback<void(std::optional<double>)> callback) {
  chromeos::PowerManagerClient::Get()->GetScreenBrightnessPercent(
      std::move(callback));
}

void KeyboardBrightnessController::OnReceiveHasKeyboardBacklight(
    std::optional<bool> has_keyboard_backlight) {
  if (has_keyboard_backlight.has_value()) {
    base::UmaHistogramBoolean("ChromeOS.Keyboard.HasBacklight",
                              has_keyboard_backlight.value());
    return;
  }
  LOG(ERROR) << "KeyboardBrightnessController: Failed to get the keyboard "
                "backlight status";
}

}  // namespace ash
