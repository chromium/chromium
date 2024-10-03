// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PERFORMANCE_DOZE_MODE_POWER_STATUS_SCHEDULER_H_
#define CHROME_BROWSER_ASH_PERFORMANCE_DOZE_MODE_POWER_STATUS_SCHEDULER_H_

#include <memory>
#include <optional>

#include "ash/components/arc/power/arc_power_bridge.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/session/session_controller_impl.h"
#include "ash/wm/video_detector.h"
#include "base/component_export.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/ash/arc/session/arc_session_manager_observer.h"
#include "chrome/browser/ash/performance/pausable_timer.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/dbus/power_manager/power_supply_properties.pb.h"
#include "components/metrics/daily_event.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "ui/base/user_activity/user_activity_detector.h"
#include "ui/base/user_activity/user_activity_observer.h"

namespace ash {

// DozeModePowerStatusScheduler observes power changes, arc idle state changes
// and user activities to decide whether send simulated battery status to
// arcvm.
class DozeModePowerStatusScheduler
    : public arc::ArcSessionManagerObserver,
      public arc::ArcPowerBridge::Observer,
      public ash::SessionObserver,
      public ash::VideoDetector::Observer,
      public ui::UserActivityObserver,
      public chromeos::PowerManagerClient::Observer {
 public:
  enum class PowerStatus {
    kRealPower,    // Goldfish battery device should reflect the real AC power
                   // status that it gets from powerd.
    kRealBattery,  // Goldfish battery device should reflect the real battery
                   // power status that it gets from powerd.
    kSimulatedBattery,  // Goldfish battery device should be on battery despite
                        // power status from powerd.
  };
  // Registers local state prefs used to record power statuses durations.
  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  explicit DozeModePowerStatusScheduler(PrefService* local_state);
  DozeModePowerStatusScheduler(const DozeModePowerStatusScheduler&) = delete;
  DozeModePowerStatusScheduler& operator=(const DozeModePowerStatusScheduler&) =
      delete;
  ~DozeModePowerStatusScheduler() override;

  // Start timers and observations and initialize members. Called in
  // |OnArcStarted()|;
  void Start();

  // Stop timers and reset observations and reset members.
  void Stop();

  // arc::ArcSessionManagerObserver:
  void OnArcStarted() override;
  void OnArcSessionStopped(arc::ArcStopReason stop_reason) override;
  void OnShutdown() override;

  // arc::ArcPowerBridge::Observer:
  void OnAndroidIdleStateChange(arc::mojom::IdleState state) override;
  void OnWillDestroyArcPowerBridge() override;

  // chromeos::PowerManagerClient::Observer:
  void PowerChanged(const power_manager::PowerSupplyProperties& proto) override;

  // ash::SessionObserver:
  void OnLockStateChanged(bool locked) override;

  // ash::VideoDetector::Observer:
  void OnVideoStateChanged(ash::VideoDetector::State state) override;

  // ui::UserActivityObserver:
  void OnUserActivity(const ui::Event* event) override;

 private:
  class DailyEventObserver;

  // Report UMA metrics daily.
  void ReportDailyMetrics(metrics::DailyEvent::IntervalType type);

  // Called when `user_active_timer_` fires.
  void OnUserActiveTimerTick();

  // Called when `simulated_battery_timer_` fires.
  void OnSimulatedBatteryTimerTicks();

  // Called when `force_real_power_timer_` fires.
  void OnRealPowerTimerTicks();

  // Update power status and send it to crosvm if it changes.
  void MaybeUpdatePowerStatus();

  // Calculate power status based on current conditions.
  PowerStatus CalculatePowerStatus();

  // Send power status to crosvm.
  void SendPowerStatus(PowerStatus status);

  base::ScopedObservation<arc::ArcPowerBridge, arc::ArcPowerBridge::Observer>
      arc_power_bridge_observation_{this};
  base::ScopedObservation<chromeos::PowerManagerClient,
                          chromeos::PowerManagerClient::Observer>
      power_manager_client_observation_{this};
  base::ScopedObservation<ash::SessionControllerImpl, ash::SessionObserver>
      session_controller_impl_observation_{this};
  base::ScopedObservation<ash::VideoDetector, ash::VideoDetector::Observer>
      video_detector_observation_{this};
  base::ScopedObservation<ui::UserActivityDetector, ui::UserActivityObserver>
      user_activity_observation_{this};
  base::ScopedObservation<arc::ArcSessionManager,
                          arc::ArcSessionManagerObserver>
      arc_session_manager_observation_{this};

  std::optional<PowerStatus> last_power_status_sent_;
  std::optional<base::Time> last_power_status_sent_time_;

  bool has_power_ = false;
  bool doze_mode_enabled_ = false;
  bool screen_locked_ = false;
  bool video_playing_ = false;

  base::RetainingOneShotTimer user_active_timer_;

  PausableTimer simulated_battery_timer_;
  PausableTimer force_real_power_timer_;

  const raw_ptr<PrefService> local_state_;
  std::optional<metrics::DailyEvent> daily_event_;
  base::RepeatingTimer daily_event_timer_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PERFORMANCE_DOZE_MODE_POWER_STATUS_SCHEDULER_H_
