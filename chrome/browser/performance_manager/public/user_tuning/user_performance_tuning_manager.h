// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_PUBLIC_USER_TUNING_USER_PERFORMANCE_TUNING_MANAGER_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_PUBLIC_USER_TUNING_USER_PERFORMANCE_TUNING_MANAGER_H_

#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/power_monitor/power_observer.h"
#include "chrome/browser/performance_manager/user_tuning/user_performance_tuning_notifier.h"
#include "components/prefs/pref_change_registrar.h"

class ChromeBrowserMainExtraPartsPerformanceManager;
class PerformanceManagerMetricsProviderTest;
class PrefService;

namespace performance_manager::user_tuning {

// This singleton is responsible for managing the state of high efficiency mode
// and battery saver mode, as well as the different signals surrounding their
// toggling.
//
// It is created and owned by `ChromeBrowserMainExtraPartsPerformanceManager`
// and initialized in 2 parts:
// - Created in PostCreateThreads (so that UI can start observing it as soon as
// the first views are created) and
// - Starts to manage the modes when Start() is called in PreMainMessageLoopRun.
//
// This object lives on the main thread and should be used from it exclusively.
class UserPerformanceTuningManager : public base::PowerStateObserver {
 public:
  class FrameThrottlingDelegate {
   public:
    virtual void StartThrottlingAllFrameSinks() = 0;
    virtual void StopThrottlingAllFrameSinks() = 0;

    virtual ~FrameThrottlingDelegate() = default;
  };

  class HighEfficiencyModeToggleDelegate {
   public:
    virtual void ToggleHighEfficiencyMode(bool enabled) = 0;
    virtual ~HighEfficiencyModeToggleDelegate() = default;
  };

  class Observer : public base::CheckedObserver {
   public:
    // Raised when the battery saver mode interventions are activated or
    // deactivated
    virtual void OnBatterySaverModeChanged(bool is_active) {}

    // Raised when the device is plugged in or unplugged
    // Can be used by the UI to show a promo if BSM isn't configured to be
    // enabled when on battery power.
    // If the connection/disconnection from power causes battery saver to be
    // enabled/disabled, the state of battery saver will not yet be updated when
    // this is invoked. `OnBatterySaverModeChanged` will be invoked after the
    // state is updated.
    virtual void OnExternalPowerConnectedChanged(bool on_battery_power) {}

    // Raised when the battery has reached the X% threshold
    // Can be used by the UI to show a promo if BSM isn't configured to be
    // enabled when on battery power under a certain threshold.
    virtual void OnBatteryThresholdReached() {}

    // Raised when the total memory footprint reaches X%.
    // Can be used by the UI to show a promo
    virtual void OnMemoryThresholdReached() {}

    // Raised when the tab count reaches X.
    // Can be used by the UI to show a promo
    virtual void OnTabCountThresholdReached() {}

    // Raised when the count of janky intervals reaches X.
    // Can be used by the UI to show a promo
    virtual void OnJankThresholdReached() {}
  };

  static UserPerformanceTuningManager* GetInstance();

  ~UserPerformanceTuningManager() override;

  void AddObserver(Observer* o);
  void RemoveObserver(Observer* o);

  // Returns true if the device is a portable device that can run on battery
  // power, false otherwise.
  bool DeviceHasBattery() const;

  // If called with `disabled = true`, will disable battery saver mode until the
  // device is plugged in or the user configures the battery saver mode state
  // preference.
  void SetTemporaryBatterySaverDisabledForSession(bool disabled);
  bool IsBatterySaverModeDisabledForSession() const;

  // Returns true if Battery Saver Mode interventions are active. If any state
  // transitions cause an observer notification, this is guaranteed to reflect
  // the *new* value when the observers are notified so the UI layer can make
  // decisions based on the most up-to-date state.
  bool IsBatterySaverActive() const;

 private:
  friend class ::ChromeBrowserMainExtraPartsPerformanceManager;
  friend class ::PerformanceManagerMetricsProviderTest;
  friend class UserPerformanceTuningManagerTest;
  friend class TestUserPerformanceTuningManagerEnvironment;

  // An implementation of UserPerformanceTuningNotifier::Receiver that
  // forwards the notifications to the UserPerformanceTuningManager on the Main
  // Thread.
  class UserPerformanceTuningReceiverImpl
      : public UserPerformanceTuningNotifier::Receiver {
   public:
    ~UserPerformanceTuningReceiverImpl() override;

    void NotifyTabCountThresholdReached() override;
  };

  explicit UserPerformanceTuningManager(
      PrefService* local_state,
      std::unique_ptr<UserPerformanceTuningNotifier> notifier = nullptr,
      std::unique_ptr<FrameThrottlingDelegate> frame_throttling_delegate =
          nullptr,
      std::unique_ptr<HighEfficiencyModeToggleDelegate>
          high_efficiency_mode_toggle_delegate = nullptr);

  void Start();

  void OnHighEfficiencyModePrefChanged();
  void OnBatterySaverModePrefChanged();

  void UpdateBatterySaverModeState();

  void NotifyTabCountThresholdReached();

  // base::PowerStateObserver:
  void OnPowerStateChange(bool on_battery_power) override;

  bool was_started_ = false;
  bool battery_saver_mode_enabled_ = false;
  bool battery_saver_mode_disabled_for_session_ = false;
  std::unique_ptr<FrameThrottlingDelegate> frame_throttling_delegate_;
  std::unique_ptr<HighEfficiencyModeToggleDelegate>
      high_efficiency_mode_toggle_delegate_;

  bool on_battery_power_ = false;

  PrefChangeRegistrar pref_change_registrar_;
  base::ObserverList<Observer> observers_;
};

}  // namespace performance_manager::user_tuning

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_PUBLIC_USER_TUNING_USER_PERFORMANCE_TUNING_MANAGER_H_
