// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/adaptive_charging_controller.h"

#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/dbus/power_manager/power_supply_properties.pb.h"

namespace ash {

AdaptiveChargingController::Observer::Observer() = default;

AdaptiveChargingController::Observer::~Observer() = default;

AdaptiveChargingController::AdaptiveChargingController() {
  power_manager_observation_.Observe(chromeos::PowerManagerClient::Get());
}

AdaptiveChargingController::~AdaptiveChargingController() = default;

void AdaptiveChargingController::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void AdaptiveChargingController::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

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

  // Return if the new value in adaptive_delaying_charge is the same as last
  // one.
  if (adaptive_delaying_charge_ == proto.adaptive_delaying_charge())
    return;

  adaptive_delaying_charge_ = proto.adaptive_delaying_charge();

  if (adaptive_delaying_charge_.value()) {
    for (auto& observer : observers_) {
      observer.OnAdaptiveChargingStarted();
    }
    return;
  }

  for (auto& observer : observers_) {
    observer.OnAdaptiveChargingStopped();
  }
}

}  // namespace ash
