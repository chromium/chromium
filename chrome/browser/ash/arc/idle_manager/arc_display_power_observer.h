// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_IDLE_MANAGER_ARC_DISPLAY_POWER_OBSERVER_H_
#define CHROME_BROWSER_ASH_ARC_IDLE_MANAGER_ARC_DISPLAY_POWER_OBSERVER_H_

#include "chromeos/ash/components/throttle/throttle_observer.h"
#include "ui/display/manager/display_configurator.h"

namespace arc {

constexpr char kArcDisplayPowerObserverName[] = "ArcDisplayPowerObserver";

// Listens to ARC power events and enforces throttle when display is off.
// Enforcing throttle leads to idle state, ultimately leading to doze mode.
class ArcDisplayPowerObserver : public ash::ThrottleObserver,
                                public display::DisplayConfigurator::Observer {
 public:
  ArcDisplayPowerObserver();

  ArcDisplayPowerObserver(const ArcDisplayPowerObserver&) = delete;
  ArcDisplayPowerObserver& operator=(const ArcDisplayPowerObserver&) = delete;

  ~ArcDisplayPowerObserver() override = default;

  // chromeos::ThrottleObserver:
  void StartObserving(content::BrowserContext* context,
                      const ObserverStateChangedCallback& callback) override;
  // Callers must call StopObserving() once for every use of StartObserving(),
  // and must not destroy this object while in Observing state.
  void StopObserving() override;

  // DisplayConfigurator::Observer:
  void OnPowerStateChanged(chromeos::DisplayPowerState power_state) override;
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_IDLE_MANAGER_ARC_DISPLAY_POWER_OBSERVER_H_
