// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/brightness/brightness_controller_chromeos.h"

#include <utility>

#include "base/metrics/user_metrics.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager/backlight.pb.h"
#include "chromeos/dbus/power_manager_client.h"
#include "ui/base/accelerators/accelerator.h"

namespace ash {
namespace system {

void BrightnessControllerChromeos::HandleBrightnessDown(
    const ui::Accelerator& accelerator) {
  if (accelerator.key_code() == ui::VKEY_BRIGHTNESS_DOWN)
    base::RecordAction(base::UserMetricsAction("Accel_BrightnessDown_F6"));

  chromeos::DBusThreadManager::Get()
      ->GetPowerManagerClient()
      ->DecreaseScreenBrightness(true);
}

void BrightnessControllerChromeos::HandleBrightnessUp(
    const ui::Accelerator& accelerator) {
  if (accelerator.key_code() == ui::VKEY_BRIGHTNESS_UP)
    base::RecordAction(base::UserMetricsAction("Accel_BrightnessUp_F7"));

  chromeos::DBusThreadManager::Get()
      ->GetPowerManagerClient()
      ->IncreaseScreenBrightness();
}

void BrightnessControllerChromeos::SetBrightnessPercent(double percent,
                                                        bool gradual) {
  power_manager::SetBacklightBrightnessRequest request;
  request.set_percent(percent);
  request.set_transition(
      gradual
          ? power_manager::SetBacklightBrightnessRequest_Transition_GRADUAL
          : power_manager::SetBacklightBrightnessRequest_Transition_INSTANT);
  request.set_cause(
      power_manager::SetBacklightBrightnessRequest_Cause_USER_REQUEST);
  chromeos::DBusThreadManager::Get()
      ->GetPowerManagerClient()
      ->SetScreenBrightness(request);
}

void BrightnessControllerChromeos::GetBrightnessPercent(
    base::OnceCallback<void(base::Optional<double>)> callback) {
  chromeos::DBusThreadManager::Get()
      ->GetPowerManagerClient()
      ->GetScreenBrightnessPercent(std::move(callback));
}

}  // namespace system
}  // namespace ash
