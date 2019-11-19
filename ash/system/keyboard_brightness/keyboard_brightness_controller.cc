// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/keyboard_brightness/keyboard_brightness_controller.h"

#include "base/metrics/user_metrics.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "ui/base/accelerators/accelerator.h"

using base::RecordAction;
using base::UserMetricsAction;

namespace ash {

KeyboardBrightnessController::KeyboardBrightnessController() = default;

KeyboardBrightnessController::~KeyboardBrightnessController() = default;

void KeyboardBrightnessController::HandleKeyboardBrightnessDown(
    const ui::Accelerator& accelerator) {
  if (accelerator.key_code() == ui::VKEY_BRIGHTNESS_DOWN)
    RecordAction(UserMetricsAction("Accel_KeyboardBrightnessDown_F6"));

  chromeos::PowerManagerClient::Get()->DecreaseKeyboardBrightness();
}

void KeyboardBrightnessController::HandleKeyboardBrightnessUp(
    const ui::Accelerator& accelerator) {
  if (accelerator.key_code() == ui::VKEY_BRIGHTNESS_UP)
    RecordAction(UserMetricsAction("Accel_KeyboardBrightnessUp_F7"));

  chromeos::PowerManagerClient::Get()->IncreaseKeyboardBrightness();
}

}  // namespace ash
