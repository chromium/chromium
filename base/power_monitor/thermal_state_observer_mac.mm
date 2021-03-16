// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/power_monitor/thermal_state_observer_mac.h"

#import <Foundation/Foundation.h>

#include "base/logging.h"
#include "base/power_monitor/power_monitor.h"
#include "base/power_monitor/power_monitor_source.h"

namespace {

base::PowerThermalObserver::DeviceThermalState
NSProcessInfoThermalStateToDeviceThermalState(
    NSProcessInfoThermalState nsinfo_state) NS_AVAILABLE_MAC(10_10_3) {
  switch (nsinfo_state) {
    case NSProcessInfoThermalStateNominal:
      return base::PowerThermalObserver::DeviceThermalState::kNominal;
    case NSProcessInfoThermalStateFair:
      return base::PowerThermalObserver::DeviceThermalState::kFair;
    case NSProcessInfoThermalStateSerious:
      return base::PowerThermalObserver::DeviceThermalState::kSerious;
    case NSProcessInfoThermalStateCritical:
      return base::PowerThermalObserver::DeviceThermalState::kCritical;
  }
  NOTREACHED();
  return base::PowerThermalObserver::DeviceThermalState::kUnknown;
}
}

namespace base {

ThermalStateObserverMac::ThermalStateObserverMac(
    StateUpdateCallback state_update_callback) NS_AVAILABLE_MAC(10_10_3) {
  auto on_state_change_block = ^(NSNotification* notification) {
    auto state = PowerThermalObserver::DeviceThermalState::kUnknown;
    // |thermalState| is basically a scale of power usage and its associated
    // thermal dissipation increase, from Nominal upwards, see:
    // https://developer.apple.com/library/archive/documentation/Performance/Conceptual/power_efficiency_guidelines_osx/RespondToThermalStateChanges.html
    NSProcessInfoThermalState nsinfo_state =
        [[NSProcessInfo processInfo] thermalState];
    state = NSProcessInfoThermalStateToDeviceThermalState(nsinfo_state);
    if (state_for_testing_ !=
        PowerThermalObserver::DeviceThermalState::kUnknown)
      state = state_for_testing_;
    DVLOG(1) << __func__ << ": "
             << PowerMonitorSource::DeviceThermalStateToString(state);
    state_update_callback.Run(state);
  };

  thermal_state_update_observer_ = [[NSNotificationCenter defaultCenter]
      addObserverForName:NSProcessInfoThermalStateDidChangeNotification
                  object:nil
                   queue:nil
              usingBlock:on_state_change_block];

  // Force a first call to grab the current status.
  on_state_change_block(nil);
}

ThermalStateObserverMac::~ThermalStateObserverMac() {
  [[NSNotificationCenter defaultCenter]
      removeObserver:thermal_state_update_observer_];
}

PowerThermalObserver::DeviceThermalState
ThermalStateObserverMac::GetCurrentThermalState() NS_AVAILABLE_MAC(10_10_3) {
  if (state_for_testing_ != PowerThermalObserver::DeviceThermalState::kUnknown)
    return state_for_testing_;
  NSProcessInfoThermalState nsinfo_state =
      [[NSProcessInfo processInfo] thermalState];
  return NSProcessInfoThermalStateToDeviceThermalState(nsinfo_state);
}
}
