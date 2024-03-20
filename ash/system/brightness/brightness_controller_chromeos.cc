// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/brightness/brightness_controller_chromeos.h"

#include <utility>

#include "ash/login_status.h"
#include "ash/public/cpp/session/session_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/system/power/power_status.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/dbus/power_manager/backlight.pb.h"
#include "components/session_manager/session_manager_types.h"

namespace ash::system {

namespace {

std::string GetBrightnessActionName(BrightnessAction brightness_action) {
  switch (brightness_action) {
    case BrightnessAction::kDecreaseBrightness:
      return "Decrease";
    case BrightnessAction::kIncreaseBrightness:
      return "Increase";
    case BrightnessAction::kSetBrightness:
      return "Set";
  }
}

// Returns true if the device is currently connected to a charger.
// Note: This is the same logic that ambient_controller.cc uses.
bool IsChargerConnected() {
  DCHECK(PowerStatus::IsInitialized());
  auto* power_status = PowerStatus::Get();
  if (power_status->IsBatteryPresent()) {
    // If battery is charging, that implies sufficient power is connected. If
    // battery is not charging, return true only if an official, non-USB charger
    // is connected. This will happen if the battery is fully charged or
    // charging is delayed by Adaptive Charging.
    return power_status->IsBatteryCharging() ||
           power_status->IsMainsChargerConnected();
  }

  // Chromeboxes have no battery.
  return power_status->IsLinePowerConnected();
}

}  // namespace

BrightnessControllerChromeos::BrightnessControllerChromeos(
    SessionControllerImpl* session_controller)
    : session_controller_(session_controller) {
  DCHECK(session_controller_);
  session_controller_->AddObserver(this);

  // Record a timestamp when this is constructed so last_session_change_time_ is
  // guaranteed to have a value.
  last_session_change_time_ = base::TimeTicks::Now();
}

BrightnessControllerChromeos::~BrightnessControllerChromeos() {
  DCHECK(session_controller_);
  session_controller_->RemoveObserver(this);
}

void BrightnessControllerChromeos::HandleBrightnessDown() {
  chromeos::PowerManagerClient::Get()->DecreaseScreenBrightness(true);
  RecordHistogramForBrightnessAction(BrightnessAction::kDecreaseBrightness);
}

void BrightnessControllerChromeos::HandleBrightnessUp() {
  chromeos::PowerManagerClient::Get()->IncreaseScreenBrightness();
  RecordHistogramForBrightnessAction(BrightnessAction::kIncreaseBrightness);
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
  RecordHistogramForBrightnessAction(BrightnessAction::kSetBrightness);
}

void BrightnessControllerChromeos::GetBrightnessPercent(
    base::OnceCallback<void(std::optional<double>)> callback) {
  chromeos::PowerManagerClient::Get()->GetScreenBrightnessPercent(
      std::move(callback));
}

void BrightnessControllerChromeos::OnSessionStateChanged(
    session_manager::SessionState state) {
  // Whenever the SessionState changes (e.g. LOGIN_PRIMARY to ACTIVE), record
  // the timestamp.
  last_session_change_time_ = base::TimeTicks::Now();
}

void BrightnessControllerChromeos::RecordHistogramForBrightnessAction(
    BrightnessAction brightness_action) {
  // Only record the first brightness adjustment (resets on reboot).
  if (has_brightness_been_adjusted_) {
    return;
  }
  has_brightness_been_adjusted_ = true;

  CHECK(!last_session_change_time_.is_null());

  const base::TimeDelta time_since_last_session_change =
      base::TimeTicks::Now() - last_session_change_time_;

  // Don't record a metric if the first brightness adjustment occurred >1 hour
  // after the last session change.
  if (time_since_last_session_change.InHours() >= 1) {
    return;
  }

  const session_manager::SessionState session_state =
      session_controller_->GetSessionState();
  const bool is_on_login_screen =
      session_state == session_manager::SessionState::LOGIN_PRIMARY ||
      session_state == session_manager::SessionState::LOGIN_SECONDARY;
  const bool is_active_session =
      session_state == session_manager::SessionState::ACTIVE;

  // Disregard brightness events that don't occur on the login screen or in an
  // active user session.
  if (!(is_on_login_screen || is_active_session)) {
    return;
  }

  base::UmaHistogramLongTimes100(
      base::StrCat({"ChromeOS.Display.TimeUntilFirstBrightnessChange.",
                    is_on_login_screen ? "OnLoginScreen" : "AfterLogin", ".",
                    GetBrightnessActionName(brightness_action), "Brightness.",
                    IsChargerConnected() ? "Charger" : "Battery", "Power"}),
      time_since_last_session_change);
}

}  // namespace ash::system
