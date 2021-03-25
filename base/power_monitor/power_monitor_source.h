// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_POWER_MONITOR_POWER_MONITOR_SOURCE_H_
#define BASE_POWER_MONITOR_POWER_MONITOR_SOURCE_H_

#include "base/base_export.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/power_monitor/power_observer.h"
#include "base/synchronization/lock.h"
#include "build/build_config.h"

namespace base {

// Communicates power state changes to the power monitor.
class BASE_EXPORT PowerMonitorSource {
 public:
  PowerMonitorSource();
  virtual ~PowerMonitorSource();

  // Normalized list of power events.
  enum PowerEvent {
    POWER_STATE_EVENT,  // The Power status of the system has changed.
    SUSPEND_EVENT,      // The system is being suspended.
    RESUME_EVENT        // The system is being resumed.
  };

  // Reads the current DeviceThermalState, if available on the platform.
  // Otherwise, returns kUnknown.
  virtual PowerThermalObserver::DeviceThermalState GetCurrentThermalState();

  // Update the result of thermal state.
  virtual void SetCurrentThermalState(
      PowerThermalObserver::DeviceThermalState state);

  // Platform-specific method to check whether the system is currently
  // running on battery power.
  virtual bool IsOnBatteryPower() = 0;

#if defined(OS_ANDROID)
  // Read and return the current remaining battery capacity (microampere-hours).
  virtual int GetRemainingBatteryCapacity();
#endif  // defined(OS_ANDROID)

  static const char* DeviceThermalStateToString(
      PowerThermalObserver::DeviceThermalState state);

 protected:
  friend class PowerMonitorTest;

  // Friend function that is allowed to access the protected ProcessPowerEvent.
  friend void ProcessPowerEventHelper(PowerEvent);

  // Process*Event should only be called from a single thread, most likely
  // the UI thread or, in child processes, the IO thread.
  static void ProcessPowerEvent(PowerEvent event_id);
  static void ProcessThermalEvent(
      PowerThermalObserver::DeviceThermalState new_thermal_state);

 private:
  DISALLOW_COPY_AND_ASSIGN(PowerMonitorSource);
};

}  // namespace base

#endif  // BASE_POWER_MONITOR_POWER_MONITOR_SOURCE_H_
