// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_IDLE_MANAGER_ARC_ON_BATTERY_OBSERVER_H_
#define CHROME_BROWSER_ASH_ARC_IDLE_MANAGER_ARC_ON_BATTERY_OBSERVER_H_

#include "chromeos/ash/components/throttle/throttle_observer.h"
#include "chromeos/dbus/power/power_manager_client.h"

namespace arc {

constexpr char kArcOnBatteryObserverName[] = "ArcOnBatteryObserver";

// Listens ARC power events and lifts CPU throttling when needed.
class ArcOnBatteryObserver : public ash::ThrottleObserver,
                             public chromeos::PowerManagerClient::Observer {
 public:
  ArcOnBatteryObserver();

  ArcOnBatteryObserver(const ArcOnBatteryObserver&) = delete;
  ArcOnBatteryObserver& operator=(const ArcOnBatteryObserver&) = delete;

  ~ArcOnBatteryObserver() override = default;

  // chromeos::ThrottleObserver:
  void StartObserving(content::BrowserContext* context,
                      const ObserverStateChangedCallback& callback) override;
  // Callers must call StopObserving() once for every use of StartObserving(),
  // and must not destroy this object while in Observing state.
  void StopObserving() override;

  // chromeos::PowerManagerClient::Observer:
  void PowerChanged(const power_manager::PowerSupplyProperties& proto) override;
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_IDLE_MANAGER_ARC_ON_BATTERY_OBSERVER_H_
