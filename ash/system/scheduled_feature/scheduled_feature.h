// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_SCHEDULED_FEATURE_SCHEDULED_FEATURE_H_
#define ASH_SYSTEM_SCHEDULED_FEATURE_SCHEDULED_FEATURE_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/public/cpp/schedule_enums.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/system/geolocation/geolocation_controller.h"
#include "ash/system/time/time_of_day.h"
#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/prefs/pref_change_registrar.h"
#include "ui/aura/env_observer.h"

class PrefService;

namespace ash {

// ScheduledFeature represents a feature that can be automatically scheduled to
// be on and off at a specific time. By default, it supports no scheduler and
// auto scheduler (enable during sunset to sunrise). Optionally it may support
// a custom scheduler with a custom start and end time.
class ASH_EXPORT ScheduledFeature
    : public GeolocationController::Observer,
      public aura::EnvObserver,
      public SessionObserver,
      public chromeos::PowerManagerClient::Observer {
 public:
  // `prefs_path_custom_start_time` and `prefs_path_custom_end_time` can be
  // empty strings. Supplying only one of the custom time prefs is invalid,
  // while supplying both of them enables the custom scheduling support.
  ScheduledFeature(const std::string prefs_path_enabled,
                   const std::string prefs_path_schedule_type,
                   const std::string prefs_path_custom_start_time,
                   const std::string prefs_path_custom_end_time);

  ScheduledFeature(const ScheduledFeature&) = delete;
  ScheduledFeature& operator=(const ScheduledFeature&) = delete;
  ~ScheduledFeature() override;

  PrefService* active_user_pref_service() const {
    return active_user_pref_service_;
  }
  base::OneShotTimer* timer() { return &timer_; }

  bool GetEnabled() const;
  ScheduleType GetScheduleType() const;
  TimeOfDay GetCustomStartTime() const;
  TimeOfDay GetCustomEndTime() const;

  // Get whether the current time is after sunset and before sunrise.
  bool IsNowWithinSunsetSunrise() const;

  // Set the desired ScheduledFeature settings in the current active user
  // prefs.
  void SetEnabled(bool enabled);
  void SetScheduleType(ScheduleType type);
  void SetCustomStartTime(TimeOfDay start_time);
  void SetCustomEndTime(TimeOfDay end_time);

  // SessionObserver:
  void OnActiveUserPrefServiceChanged(PrefService* pref_service) override;

  // GeolocationController::Observer:
  void OnGeopositionChanged(bool possible_change_in_timezone) override;

  // chromeos::PowerManagerClient::Observer:
  void SuspendDone(base::TimeDelta sleep_duration) override;

  void SetClockForTesting(base::Clock* clock);

 protected:
  // Called by `Refresh()` and `RefreshScheduleTimer()` to refresh the feature
  // state such as display temperature in Night Light.
  virtual void RefreshFeatureState() {}

 private:
  virtual const char* GetFeatureName() const = 0;

  // Gets now time from the `clock_`, used for testing, or `base::Time::Now()`
  // if `clock_` does not exist.
  base::Time GetNow() const;

  // Attempts restoring a previously stored schedule for the current user if
  // possible and returns true if so, false otherwise.
  bool MaybeRestoreSchedule();

  void StartWatchingPrefsChanges();

  void InitFromUserPrefs();

  // Called when the user pref for the enabled status of ScheduledFeature is
  // changed.
  void OnEnabledPrefChanged();

  // Called when the user pref for the schedule type is changed or initialized.
  // During initialization, `keep_manual_toggles_during_schedules` is set to
  // true, so the load user pref override any user current toggled setting. For
  // more detail about `keep_manual_toggles_during_schedules`, see `Refresh()`.
  void OnScheduleTypePrefChanged(bool keep_manual_toggles_during_schedules);

  // Called when either of the custom schedule prefs (custom start or end times)
  // are changed.
  void OnCustomSchedulePrefsChanged();

  // Refreshes the state of ScheduledFeature according to the currently set
  // parameters. `did_schedule_change` is true when Refresh() is called as a
  // result of a change in one of the schedule related prefs, and false
  // otherwise.
  // If `keep_manual_toggles_during_schedules` is true, refreshing the schedule
  // will not override a previous user's decision to toggle the
  // ScheduledFeature status while the schedule is being used.
  void Refresh(bool did_schedule_change,
               bool keep_manual_toggles_during_schedules);

  // Given the desired start and end times that determine the time interval
  // during which the feature will be ON, depending on the time of "now", it
  // refreshes the `timer_` to either schedule the future start or end of
  // the feature, as well as update the current status if needed.
  // For `did_schedule_change` and `keep_manual_toggles_during_schedules`, see
  // Refresh() above.
  // This function should never be called if the schedule type is `kNone`.
  void RefreshScheduleTimer(base::Time start_time,
                            base::Time end_time,
                            bool did_schedule_change,
                            bool keep_manual_toggles_during_schedules);

  // Schedule the upcoming next toggle of the feature status.
  void ScheduleNextToggle(base::TimeDelta delay);

  // The pref service of the currently active user. Can be null in
  // ash_unittests.
  PrefService* active_user_pref_service_ = nullptr;

  // Tracks the upcoming feature state changes per each user due to automatic
  // schedules. This can be used to restore a manually toggled status while the
  // schedule is being used. See MaybeRestoreSchedule().
  struct ScheduleTargetState {
    // The time at which the feature will switch to `target_status` defined
    // below.
    base::Time target_time;
    bool target_status;
  };
  base::flat_map<PrefService*, ScheduleTargetState>
      per_user_schedule_target_state_;

  // The timer that schedules the start and end of this feature when the
  // schedule type is either kSunsetToSunrise or kCustom.
  base::OneShotTimer timer_;

  // True only until this feature is initialized from the very first user
  // session. After that, it is set to false.
  bool is_first_user_init_ = true;

  // The registrar used to watch prefs changes in the above
  // `active_user_pref_service_` from outside ash.
  // NOTE: Prefs are how Chrome communicates changes to the ScheduledFeature
  // settings controlled by this class from the WebUI settings.
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  const std::string prefs_path_enabled_;
  const std::string prefs_path_schedule_type_;
  const std::string prefs_path_custom_start_time_;
  const std::string prefs_path_custom_end_time_;
  const std::string prefs_path_latitude_;
  const std::string prefs_path_longitude_;

  GeolocationController* geolocation_controller_;

  // Track if this is `GeolocationController::Observer` to make sure it is not
  // added twice if it is already an observer.
  bool is_observing_geolocation_ = false;

  // Optional Used in tests to override the time of "Now".
  base::Clock* clock_ = nullptr;  // Not owned.
};

}  // namespace ash

#endif  // ASH_SYSTEM_SCHEDULED_FEATURE_SCHEDULED_FEATURE_H_
