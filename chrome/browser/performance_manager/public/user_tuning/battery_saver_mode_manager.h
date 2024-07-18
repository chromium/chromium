// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_PUBLIC_USER_TUNING_BATTERY_SAVER_MODE_MANAGER_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_PUBLIC_USER_TUNING_BATTERY_SAVER_MODE_MANAGER_H_

#include <memory>

#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "components/performance_manager/public/user_tuning/prefs.h"
#include "components/prefs/pref_change_registrar.h"

class ChromeBrowserMainExtraPartsPerformanceManager;
class PrefService;
class BatteryDischargeReporterTest;

namespace performance_manager::user_tuning {

// This singleton is responsible for managing the state of battery saver mode,
// as well as the different signals surrounding its toggling.
//
// It is created and owned by `ChromeBrowserMainExtraPartsPerformanceManager`
// and initialized in 2 parts:
// - Created in PostCreateThreads (so that UI can start observing it as soon as
// the first views are created) and
// - Starts to manage the mode when Start() is called in PreMainMessageLoopRun.
//
// This object lives on the main thread and should be used from it exclusively.
class BatterySaverModeManager {
 public:
  // The percentage of battery that is considered "low". For instance, this
  // would be `20` for 20%.
  static const uint64_t kLowBatteryThresholdPercent;

  // Command line switch for overriding the device has battery flag.
  static const char kForceDeviceHasBatterySwitch[];

  class FrameThrottlingDelegate {
   public:
    virtual void StartThrottlingAllFrameSinks() = 0;
    virtual void StopThrottlingAllFrameSinks() = 0;

    virtual ~FrameThrottlingDelegate() = default;
  };

  class ChildProcessTuningDelegate {
   public:
    virtual void SetBatterySaverModeForAllChildProcessHosts(bool enabled) = 0;

    virtual ~ChildProcessTuningDelegate() = default;
  };

  class FreezingDelegate {
   public:
    virtual void ToggleFreezingOnBatterySaverMode(bool is_enabled) = 0;

    virtual ~FreezingDelegate() = default;
  };

  class Observer : public base::CheckedObserver {
   public:
    // Raised when the browser level battery saver mode is enabled or disabled.
    // Both `kEnabledOnBattery` and `kEnabledBelowThreshold` are considered
    // enabled. This does not imply whether the mode is active or not.
    virtual void OnBatterySaverModeChanged(bool is_enabled) {}

    // Raised when the battery saver mode interventions are activated or
    // deactivated
    virtual void OnBatterySaverActiveChanged(bool is_active) {}

    // Raised when the device is plugged in or unplugged
    // Can be used by the UI to show a promo if BSM isn't configured to be
    // enabled when on battery power.
    // If the connection/disconnection from power causes battery saver to be
    // enabled/disabled, the state of battery saver will not yet be updated when
    // this is invoked. `OnBatterySaverActiveChanged` will be invoked after the
    // state is updated.
    virtual void OnExternalPowerConnectedChanged(bool on_battery_power) {}

    // Raised when it becomes known that the device has a battery installed, or
    // when a device that previously had a battery is now reported as not having
    // one anymore. Overloading this function is particularly useful for code
    // that wants to know if the device has a battery during startup, because
    // `DeviceHasBattery` can wrongly return `false` for an unbounded period
    // of time until the OS provides battery data.
    virtual void OnDeviceHasBatteryChanged(bool device_has_battery) {}

    // Raised when the battery has reached the 20% threshold
    // Can be used by the UI to show a promo if BSM isn't configured to be
    // enabled when on battery power under a certain threshold.
    virtual void OnBatteryThresholdReached() {}
  };

  // Returns whether a BatterySaverModeManager was created and installed.
  // Should only return false in unit tests.
  static bool HasInstance();
  static BatterySaverModeManager* GetInstance();

  ~BatterySaverModeManager();

  void AddObserver(Observer* o);
  void RemoveObserver(Observer* o);

  // Returns true if the device is a portable device that can run on battery
  // power, false otherwise.
  // This is determined asynchronously, so it may indicate false for an
  // undetermined amount of time at startup, until the battery state is
  // sampled for the first time.
  bool DeviceHasBattery() const;

  // Returns true if Battery Saver Mode is enabled for the user. If any state
  // transitions cause an observer notification, this is guaranteed to reflect
  // the *new* value when the observers are notified so the UI layer can make
  // decisions based on the most up-to-date state.
  bool IsBatterySaverModeEnabled();

  // Returns true if Battery Saver Mode is a managed pref. If any state
  // transitions cause an observer notification, this is guaranteed to reflect
  // the *new* value when the observers are notified so the UI layer can make
  // decisions based on the most up-to-date state.
  bool IsBatterySaverModeManaged() const;

  // Returns true if Battery Saver Mode interventions are active. If any state
  // transitions cause an observer notification, this is guaranteed to reflect
  // the *new* value when the observers are notified so the UI layer can make
  // decisions based on the most up-to-date state.
  bool IsBatterySaverActive() const;

  // Returns true if the device is unplugged and using battery power.
  bool IsUsingBatteryPower() const;

  // Returns the time of the last use of battery for the device.
  base::Time GetLastBatteryUsageTimestamp() const;

  // Returns the last sampled device battery percentage. A percentage of -1
  // indicates that the battery state has not been sampled yet.
  int SampledBatteryPercentage() const;

  // If called with `disabled = true`, will disable battery saver mode until
  // the device is plugged in or the user configures the battery saver mode
  // state preference.
  void SetTemporaryBatterySaverDisabledForSession(bool disabled);
  bool IsBatterySaverModeDisabledForSession() const;

 private:
  friend class ::ChromeBrowserMainExtraPartsPerformanceManager;
  friend class ChromeOSBatterySaverProvider;
  friend class DesktopBatterySaverProvider;
  friend class ::BatteryDischargeReporterTest;
  friend class BatterySaverModeManagerTest;
  friend class TestUserPerformanceTuningManagerEnvironment;

  class BatterySaverProvider {
   public:
    virtual ~BatterySaverProvider() = default;

    virtual bool DeviceHasBattery() const = 0;
    virtual bool IsBatterySaverModeEnabled() = 0;
    virtual bool IsBatterySaverModeManaged() = 0;
    virtual bool IsBatterySaverActive() const = 0;
    virtual bool IsUsingBatteryPower() const = 0;
    virtual base::Time GetLastBatteryUsageTimestamp() const = 0;
    virtual int SampledBatteryPercentage() const = 0;
    virtual void SetTemporaryBatterySaverDisabledForSession(bool disabled) = 0;
    virtual bool IsBatterySaverModeDisabledForSession() const = 0;
  };

  explicit BatterySaverModeManager(
      PrefService* local_state,
      std::unique_ptr<FrameThrottlingDelegate> frame_throttling_delegate =
          nullptr,
      std::unique_ptr<ChildProcessTuningDelegate>
          child_process_tuning_delegate = nullptr,
      std::unique_ptr<FreezingDelegate> freezing_delegate = nullptr);

  void Start();

  // Called from the installed BatterySaverProvider to signify a change in
  // battery saver mode related state.
  void NotifyOnBatterySaverModeChanged(bool battery_saver_mode_enabled);
  void NotifyOnBatterySaverActiveChanged(bool battery_saver_mode_active);
  void NotifyOnExternalPowerConnectedChanged(bool on_battery_power);
  void NotifyOnDeviceHasBatteryChanged(bool has_battery);
  void NotifyOnBatteryThresholdReached();

  std::unique_ptr<FrameThrottlingDelegate> frame_throttling_delegate_;
  std::unique_ptr<ChildProcessTuningDelegate> child_process_tuning_delegate_;
  std::unique_ptr<FreezingDelegate> freezing_delegate_;
  std::unique_ptr<BatterySaverProvider> battery_saver_provider_;

  PrefChangeRegistrar pref_change_registrar_;
  base::ObserverList<Observer> observers_;
};

}  // namespace performance_manager::user_tuning

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_PUBLIC_USER_TUNING_BATTERY_SAVER_MODE_MANAGER_H_
