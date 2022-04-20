// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/adaptive_charging_controller.h"

#include "base/scoped_observation.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/dbus/power_manager/power_supply_properties.pb.h"

namespace ash {

AdaptiveChargingController::AdaptiveChargingController() {
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

  is_adaptive_delaying_charge_ = proto.adaptive_delaying_charge();
}

}  // namespace ash
