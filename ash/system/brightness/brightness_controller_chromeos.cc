// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/brightness/brightness_controller_chromeos.h"

#include <utility>

#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/dbus/power_manager/backlight.pb.h"

namespace ash {
namespace system {

void BrightnessControllerChromeos::HandleBrightnessDown() {
  chromeos::PowerManagerClient::Get()->DecreaseScreenBrightness(true);
}

void BrightnessControllerChromeos::HandleBrightnessUp() {
  chromeos::PowerManagerClient::Get()->IncreaseScreenBrightness();
}

void BrightnessControllerChromeos::SetBrightnessPercent(double percent,
                                                        bool gradual) {
  power_manager::SetBacklightBrightnessRequest request;
  request.set_percent(percent);
  request.set_transition(
      gradual
          ? power_manager::SetBacklightBrightnessRequest_Transition_FAST
          : power_manager::SetBacklightBrightnessRequest_Transition_INSTANT);
  request.set_cause(
      power_manager::SetBacklightBrightnessRequest_Cause_USER_REQUEST);
  chromeos::PowerManagerClient::Get()->SetScreenBrightness(request);
}

void BrightnessControllerChromeos::GetBrightnessPercent(
    base::OnceCallback<void(std::optional<double>)> callback) {
  chromeos::PowerManagerClient::Get()->GetScreenBrightnessPercent(
      std::move(callback));
}

}  // namespace system
}  // namespace ash
