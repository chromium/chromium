// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_POWER_MONITOR_THERMAL_STATE_OBSERVER_MAC_H_
#define BASE_POWER_MONITOR_THERMAL_STATE_OBSERVER_MAC_H_

#include <objc/objc.h>

#include "base/base_export.h"
#include "base/callback.h"
#include "base/power_monitor/power_observer.h"

namespace base {

// This class is used to listen for the thermal state change notification
// NSProcessInfoThermalStateDidChangeNotification, routing it to
// PowerMonitorSource.
class BASE_EXPORT ThermalStateObserverMac {
 public:
  using StateUpdateCallback =
      base::RepeatingCallback<void(PowerThermalObserver::DeviceThermalState)>;

  explicit ThermalStateObserverMac(StateUpdateCallback state_update_callback);
  ~ThermalStateObserverMac();

  PowerThermalObserver::DeviceThermalState GetCurrentThermalState();

 private:
  FRIEND_TEST_ALL_PREFIXES(ThermalStateObserverMacTest, StateChange);
  PowerThermalObserver::DeviceThermalState state_for_testing_ =
      PowerThermalObserver::DeviceThermalState::kUnknown;

  id thermal_state_update_observer_;
};

}  // namespace base

#endif  // BASE_POWER_MONITOR_THERMAL_STATE_OBSERVER_MAC_H_
