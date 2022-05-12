// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/adaptive_charging_controller.h"

#include "base/scoped_observation.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/dbus/power_manager/power_supply_properties.pb.h"

namespace ash {

AdaptiveChargingController::AdaptiveChargingController()
    : nudge_controller_(std::make_unique<AdaptiveChargingNudgeController>()),
      notification_controller_(
          std::make_unique<AdaptiveChargingNotificationController>()) {
  power_manager_observation_.Observe(chromeos::PowerManagerClient::Get());
}

AdaptiveChargingController::~AdaptiveChargingController() = default;

bool AdaptiveChargingController::IsAdaptiveChargingSupported() {
  const absl::optional<power_manager::PowerSupplyProperties>&
      power_supply_proto = chromeos::PowerManagerClient::Get()->GetLastStatus();

  return power_supply_proto.has_value() &&
         power_supply_proto->adaptive_charging_supported();
}

void AdaptiveChargingController::PowerChanged(
    const power_manager::PowerSupplyProperties& proto) {
  // Return if this change does not contain any adaptive_delaying_charge info.
  if (!proto.has_adaptive_delaying_charge())
    return;

  // We only care about the change in this field.
  if (is_adaptive_delaying_charge_ == proto.adaptive_delaying_charge())
    return;

  is_adaptive_delaying_charge_ = proto.adaptive_delaying_charge();

  // Nudge and notification should be shown only if heuristic is enabled for
  // this user and the adaptive charging is actually active.
  if (!proto.has_adaptive_charging_heuristic_enabled() ||
      !proto.adaptive_charging_heuristic_enabled() ||
      !is_adaptive_delaying_charge_) {
    return;
  }

  // The nudge will only be shown alongside the notification once.
  nudge_controller_->ShowNudge();

  if (proto.has_battery_time_to_full_sec() &&
      proto.battery_time_to_full_sec() > 0) {
    // Converts time to full from second to hours.
    notification_controller_->ShowAdaptiveChargingNotification(
        static_cast<int>(proto.battery_time_to_full_sec() / 3600));
  } else {
    notification_controller_->ShowAdaptiveChargingNotification();
  }
}

}  // namespace ash
