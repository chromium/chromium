// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/power_monitor/thermal_state_observer_mac.h"

#import <Foundation/Foundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/pwr_mgt/IOPM.h>
#include <IOKit/pwr_mgt/IOPMKeys.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <IOKit/pwr_mgt/IOPMLibDefs.h>
#include <notify.h>

#include <memory>

#include "base/apple/scoped_cftyperef.h"
#include "base/logging.h"
#include "base/power_monitor/power_monitor.h"
#include "base/power_monitor/power_monitor_source.h"
#include "base/power_monitor/power_observer.h"

namespace {

base::PowerThermalObserver::DeviceThermalState
NSProcessInfoThermalStateToDeviceThermalState(
    NSProcessInfoThermalState nsinfo_state) {
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
}
}

namespace base {

struct ThermalStateObserverMac::ObjCStorage {
  id __strong thermal_state_update_observer = nil;
};

ThermalStateObserverMac::ThermalStateObserverMac(
    StateUpdateCallback state_update_callback,
    SpeedLimitUpdateCallback speed_limit_update_callback,
    const char* power_notification_key)
    : power_notification_key_(power_notification_key),
      objc_storage_(std::make_unique<ObjCStorage>()) {
  auto on_state_change_block = ^(NSNotification* notification) {
    auto state = PowerThermalObserver::DeviceThermalState::kUnknown;
    // |thermalState| is basically a scale of power usage and its associated
    // thermal dissipation increase, from Nominal upwards, see:
    // https://developer.apple.com/library/archive/documentation/Performance/Conceptual/power_efficiency_guidelines_osx/RespondToThermalStateChanges.html
    NSProcessInfoThermalState nsinfo_state =
        NSProcessInfo.processInfo.thermalState;
    state = NSProcessInfoThermalStateToDeviceThermalState(nsinfo_state);
    if (state_for_testing_ !=
        PowerThermalObserver::DeviceThermalState::kUnknown)
      state = state_for_testing_;
    DVLOG(1) << __func__ << ": "
             << PowerMonitorSource::DeviceThermalStateToString(state);
    state_update_callback.Run(state);
  };

  objc_storage_->thermal_state_update_observer =
      [NSNotificationCenter.defaultCenter
          addObserverForName:NSProcessInfoThermalStateDidChangeNotification
                      object:nil
                       queue:nil
                  usingBlock:on_state_change_block];

  auto on_speed_change_block = ^() {
    int speed_limit = GetCurrentSpeedLimit();
    DVLOG(1) << __func__ << ": " << speed_limit;
    speed_limit_update_callback.Run(speed_limit);
  };

  uint32_t result = notify_register_dispatch(power_notification_key_,
                                             &speed_limit_notification_token_,
                                             dispatch_get_main_queue(), ^(int) {
                                               on_speed_change_block();
                                             });
  LOG_IF(ERROR, result != NOTIFY_STATUS_OK)
      << __func__
      << " unable to register to power notifications. Result: " << result;

  // Force a first call to grab the current status.
  on_state_change_block(nil);
  on_speed_change_block();
}

ThermalStateObserverMac::~ThermalStateObserverMac() {
  [NSNotificationCenter.defaultCenter
      removeObserver:objc_storage_->thermal_state_update_observer];
  notify_cancel(speed_limit_notification_token_);
}

PowerThermalObserver::DeviceThermalState
ThermalStateObserverMac::GetCurrentThermalState() {
  if (state_for_testing_ != PowerThermalObserver::DeviceThermalState::kUnknown)
    return state_for_testing_;
  NSProcessInfoThermalState nsinfo_state =
      NSProcessInfo.processInfo.thermalState;
  return NSProcessInfoThermalStateToDeviceThermalState(nsinfo_state);
}

int ThermalStateObserverMac::GetCurrentSpeedLimit() const {
  apple::ScopedCFTypeRef<CFDictionaryRef> dictionary;
  IOReturn result = IOPMCopyCPUPowerStatus(dictionary.InitializeInto());
  if (result != kIOReturnSuccess) {
    DVLOG(1) << __func__
             << "Unable to get CPU power status, result = " << result;
    return PowerThermalObserver::kSpeedLimitMax;
  }
  if (CFTypeRef value = CFDictionaryGetValue(
          dictionary.get(), CFSTR(kIOPMCPUPowerLimitProcessorSpeedKey))) {
    int speed_limit = -1;
    if (CFNumberGetValue(reinterpret_cast<CFNumberRef>(value), kCFNumberIntType,
                         &speed_limit)) {
      return speed_limit;
    } else {
      DVLOG(1) << __func__ << "Unable to get speed limit value";
    }
  } else {
    DVLOG(1) << __func__ << "Unable to get speed limit";
  }
  return PowerThermalObserver::kSpeedLimitMax;
}
}
