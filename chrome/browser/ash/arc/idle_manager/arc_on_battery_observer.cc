// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/idle_manager/arc_on_battery_observer.h"

namespace arc {

ArcOnBatteryObserver::ArcOnBatteryObserver()
    : ThrottleObserver(kArcOnBatteryObserverName) {}

void ArcOnBatteryObserver::StartObserving(
    content::BrowserContext* context,
    const ObserverStateChangedCallback& callback) {
  ThrottleObserver::StartObserving(context, callback);
  chromeos::PowerManagerClient::Get()->AddObserver(this);
  chromeos::PowerManagerClient::Get()->RequestStatusUpdate();
}

void ArcOnBatteryObserver::StopObserving() {
  chromeos::PowerManagerClient::Get()->RemoveObserver(this);
  ThrottleObserver::StopObserving();
}

void ArcOnBatteryObserver::PowerChanged(
    const power_manager::PowerSupplyProperties& proto) {
  const bool is_on_battery_power =
      !proto.has_external_power() ||
      (proto.external_power() ==
       power_manager::PowerSupplyProperties_ExternalPower_DISCONNECTED);

  SetActive(!is_on_battery_power);
}

}  // namespace arc
