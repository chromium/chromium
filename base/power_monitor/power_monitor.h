// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_POWER_MONITOR_POWER_MONITOR_H_
#define BASE_POWER_MONITOR_POWER_MONITOR_H_

#include "base/base_export.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/no_destructor.h"
#include "base/observer_list_threadsafe.h"
#include "base/power_monitor/power_observer.h"
#include "build/build_config.h"

namespace base {

class PowerMonitorSource;

// A class used to monitor the power state change and notify the observers about
// the change event. The threading model of this class is as follows:
// Once initialized, it is threadsafe. However, the client must ensure that
// initialization happens before any other methods are invoked, including
// IsInitialized(). IsInitialized() exists only as a convenience for detection
// of test contexts where the PowerMonitor global is never created.
class BASE_EXPORT PowerMonitor {
 public:
  // Initializes global PowerMonitor state. Takes ownership of |source|, which
  // will be leaked on process teardown. May only be called once. Not threadsafe
  // - no other PowerMonitor methods may be called on any thread while calling
  // Initialize(). |source| must not be nullptr.
  static void Initialize(std::unique_ptr<PowerMonitorSource> source);

  // Returns true if Initialize() has been called. Safe to call on any thread,
  // but must not be called while Initialize() or ShutdownForTesting() is being
  // invoked.
  static bool IsInitialized();

  // Add and remove an observer.
  // Can be called from any thread. |observer| is notified on the sequence
  // from which it was registered.
  // Must not be called from within a notification callback.
  //
  // It is safe to add observers before the PowerMonitor is initialized. It is
  // safe to remove an observer even if it was not added as an observer.
  static void AddPowerSuspendObserver(PowerSuspendObserver* observer);
  static void RemovePowerSuspendObserver(PowerSuspendObserver* observer);
  static void AddPowerStateObserver(PowerStateObserver* observer);
  static void RemovePowerStateObserver(PowerStateObserver* observer);
  static void AddPowerThermalObserver(PowerThermalObserver* observer);
  static void RemovePowerThermalObserver(PowerThermalObserver* observer);

  // Atomically add a PowerSuspendObserver and read the current power suspended
  // state. This variant must be used to avoid race between adding an observer
  // and reading the power state. The following code would be racy:
  //    AddOPowerSuspendbserver(...);
  //    if (PowerMonitor::IsSystemSuspended()) { ... }
  //
  // Returns true if the system is currently suspended.
  static bool AddPowerSuspendObserverAndReturnSuspendedState(
      PowerSuspendObserver* observer);
  // Returns true if the system is on-battery.
  static bool AddPowerStateObserverAndReturnOnBatteryState(
      PowerStateObserver* observer);
  // Returns the power thermal state.
  static PowerThermalObserver::DeviceThermalState
  AddPowerStateObserverAndReturnPowerThermalState(
      PowerThermalObserver* observer);

  // Is the computer currently on battery power. May only be called if the
  // PowerMonitor has been initialized.
  static bool IsOnBatteryPower();

  // Read the current DeviceThermalState if known. Can be called on any thread.
  // May only be called if the PowerMonitor has been initialized.
  static PowerThermalObserver::DeviceThermalState GetCurrentThermalState();

  // Update the result of thermal state.
  static void SetCurrentThermalState(
      PowerThermalObserver::DeviceThermalState state);

#if defined(OS_ANDROID)
  // Read and return the current remaining battery capacity (microampere-hours).
  // Only supported with a device power source (i.e. not in child processes in
  // Chrome) and on devices with Android >= Lollipop as well as a power supply
  // that supports this counter. Returns 0 if unsupported.
  static int GetRemainingBatteryCapacity();
#endif  // defined(OS_ANDROID)

  // Uninitializes the PowerMonitor. Should be called at the end of any unit
  // test that mocks out the PowerMonitor, to avoid affecting subsequent tests.
  // There must be no live observers when invoked. Safe to call even if the
  // PowerMonitor hasn't been initialized.
  static void ShutdownForTesting();

 private:
  friend class PowerMonitorSource;
  friend class base::NoDestructor<PowerMonitor>;

  PowerMonitor();
  ~PowerMonitor();

  static PowerMonitorSource* Source();

  static void NotifyPowerStateChange(bool on_battery_power);
  static void NotifySuspend();
  static void NotifyResume();
  static void NotifyThermalStateChange(
      PowerThermalObserver::DeviceThermalState new_state);

  static PowerMonitor* GetInstance();

  bool is_system_suspended_ GUARDED_BY(is_system_suspended_lock_) = false;
  Lock is_system_suspended_lock_;

  bool on_battery_power_ GUARDED_BY(on_battery_power_lock_) = false;
  Lock on_battery_power_lock_;

  PowerThermalObserver::DeviceThermalState power_thermal_state_
      GUARDED_BY(power_thermal_state_lock_) =
          PowerThermalObserver::DeviceThermalState::kUnknown;
  Lock power_thermal_state_lock_;

  scoped_refptr<ObserverListThreadSafe<PowerStateObserver>>
      power_state_observers_;
  scoped_refptr<ObserverListThreadSafe<PowerSuspendObserver>>
      power_suspend_observers_;
  scoped_refptr<ObserverListThreadSafe<PowerThermalObserver>>
      thermal_state_observers_;
  std::unique_ptr<PowerMonitorSource> source_;

  DISALLOW_COPY_AND_ASSIGN(PowerMonitor);
};

}  // namespace base

#endif  // BASE_POWER_MONITOR_POWER_MONITOR_H_
