// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_USER_TUNING_USER_PERFORMANCE_TUNING_MANAGER_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_USER_TUNING_USER_PERFORMANCE_TUNING_MANAGER_H_

#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "components/prefs/pref_change_registrar.h"

class PrefService;

namespace performance_manager::user_tuning {

class UserPerformanceTuningManager {
 public:
  class FrameThrottlingDelegate {
   public:
    virtual void StartThrottlingAllFrameSinks() = 0;
    virtual void StopThrottlingAllFrameSinks() = 0;

    virtual ~FrameThrottlingDelegate() = default;
  };

  class Observer : public base::CheckedObserver {
   public:
    // Raised when the battery saver mode interventions are activated or
    // deactivated
    virtual void OnBatterySaverModeChanged(bool is_active) = 0;

    // Raised when the device is plugged in or unplugged
    // Can be used by the UI to show a promo if BSM isn't configured to be
    // enabled when on battery power.
    virtual void OnExternalPowerConnectedChanged(
        bool external_power_connected) = 0;

    // Raised when the battery has reached the X% threshold
    // Can be used by the UI to show a promo if BSM isn't configured to be
    // enabled when on battery power under a certain threshold.
    virtual void OnBatteryThresholdReached() = 0;

    // Raised when the total memory footprint reaches X%.
    // Can be used by the UI to show a promo
    virtual void OnMemoryThresholdReached() = 0;

    // Raised when the tab count reaches X.
    // Can be used by the UI to show a promo
    virtual void OnTabCountThresholdReached() = 0;

    // Raised when the count of janky intervals reaches X.
    // Can be used by the UI to show a promo
    virtual void OnJankThresholdReached() = 0;
  };

  explicit UserPerformanceTuningManager(
      PrefService* local_state,
      std::unique_ptr<FrameThrottlingDelegate> frame_throttling_delegate =
          nullptr);
  ~UserPerformanceTuningManager();

  void AddObserver(Observer* o);
  void RemoveObserver(Observer* o);

  // Returns true if the device is a portable device that can run on battery
  // power, false otherwise.
  bool DeviceHasBattery() const;

  // Called to enable or disable the temporary battery saver mode.
  void SetTemporaryBatterySaver(bool enabled);

  // Returns true if Battery Saver Mode interventions are active. If any state
  // transitions cause an observer notification, this is guaranteed to reflect
  // the *new* value when the observers are notified so the UI layer can make
  // decisions based on the most up-to-date state.
  bool IsBatterySaverActive() const;

 private:
  void OnHighEfficiencyModePrefChanged();
  void OnBatterySaverModePrefChanged();

  void UpdateBatterySaverModeState();

  bool battery_saver_mode_enabled_ = false;
  bool temporary_battery_saver_enabled_ = false;
  std::unique_ptr<FrameThrottlingDelegate> frame_throttling_delegate_;

  PrefChangeRegistrar pref_change_registrar_;
  base::ObserverList<Observer> observers_;
};

}  // namespace performance_manager::user_tuning

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_USER_TUNING_USER_PERFORMANCE_TUNING_MANAGER_H_
