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
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation_traits.h"
#include "base/time/clock.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/prefs/pref_change_registrar.h"
#include "net/base/backoff_entry.h"

class PrefService;

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace ash {

class LocalTimeConverter;

// ScheduledFeature represents a feature that can be automatically scheduled to
// be on and off at a specific time. By default, it supports no scheduler and
// auto scheduler (enable during sunset to sunrise). Optionally it may support
// a custom scheduler with a custom start and end time.
class ASH_EXPORT ScheduledFeature
    : public GeolocationController::Observer,
      public SessionObserver,
      public chromeos::PowerManagerClient::Observer {
 public:
  // Reasons why the feature's state may be modified.
  enum class RefreshReason {
    // An internally scheduled update to feature state (ex: sunrise or sunset
    // when the `kSunsetToSunrise` schedule type is active).
    kScheduled,
    // Schedule's settings have been changed by the user (schedule type, custom
    // start/end time, etc).
    kSettingsChanged,
    // Something about the environment has changed (ex: waking up after
    // suspend, new geoposition, etc), so the feature's state needs to be
    // recomputed.
    kReset,
    // An external caller explicitly modified the feature's state to something
    // potentially different from what the schedule dictates.
    kExternal
  };

  // May be overridden for testing purposes (see SetClockForTesting()). By
  // default, returns system time.
  class Clock : public base::Clock, public base::TickClock {
   public:
    // base::Clock:
    base::Time Now() const override;
    // base::TickClock:
    base::TimeTicks NowTicks() const override;
  };

  // For callers who are interested in feature state changes expressed using
  // `ScheduleCheckpoint`. Checkpoints are a finer-grained way of reading the
  // feature's "enabled" state; if this level of detail is not necessary, it's
  // sufficient to just observe the binary "enabled" state (see `GetEnabled()`).
  class CheckpointObserver : public base::CheckedObserver {
   public:
    // Invoked whenever a new `ScheduleCheckpoint` has been reached.
    //
    // The `src` is provided in case a caller is observing multiple
    // `ScheduledFeature`s and needs to know which feature this notification is
    // coming from.
    virtual void OnCheckpointChanged(const ScheduledFeature* src,
                                     ScheduleCheckpoint new_checkpoint) = 0;

   protected:
    ~CheckpointObserver() override = default;
  };

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
  ScheduleCheckpoint current_checkpoint() const { return current_checkpoint_; }

  bool GetEnabled() const;
  ScheduleType GetScheduleType() const;
  TimeOfDay GetCustomStartTime() const;
  TimeOfDay GetCustomEndTime() const;

  // Set the desired ScheduledFeature settings in the current active user
  // prefs.
  void SetEnabled(bool enabled);
  void SetScheduleType(ScheduleType type);
  void SetCustomStartTime(TimeOfDay start_time);
  void SetCustomEndTime(TimeOfDay end_time);

  void AddCheckpointObserver(CheckpointObserver* obs);
  void RemoveCheckpointObserver(CheckpointObserver* obs);

  // SessionObserver:
  void OnActiveUserPrefServiceChanged(PrefService* pref_service) override;

  // GeolocationController::Observer:
  void OnGeopositionChanged(bool possible_change_in_timezone) override;

  // chromeos::PowerManagerClient::Observer:
  void SuspendDone(base::TimeDelta sleep_duration) override;

  // Returns now time from the `clock_`.
  base::Time Now() const;

  void SetClockForTesting(const Clock* clock);
  void SetLocalTimeConverterForTesting(
      const LocalTimeConverter* local_time_converter);
  void SetTaskRunnerForTesting(
      scoped_refptr<base::SequencedTaskRunner> task_runner);

  base::OneShotTimer* timer() const { return timer_.get(); }

 protected:
  // Called by `Refresh()` and `RefreshScheduleTimer()` to refresh the feature
  // state such as display temperature in Night Light.
  virtual void RefreshFeatureState(RefreshReason reason) {}

 private:
  // Contains all of the data required to restore `ScheduledFeature` to a state
  // it was in previously. This is maintained per user mainly so that
  // `ScheduledFeature` can pick up where it left off when the active user
  // changes. This includes restoring a manually toggled feature status. See
  // `MaybeRestoreSchedule()`.
  struct ScheduleSnapshot {
    // The time at which the feature will switch to `target_status` defined
    // below. `target_status` is not necessarily a change in status. See
    // comments above `ScheduleNextRefresh()`.
    base::Time target_time;
    bool target_status;
    // The value of `current_checkpoint_` at the time this snapshot of the
    // feature's state was captured.
    ScheduleCheckpoint current_checkpoint;
  };

  virtual const char* GetFeatureName() const = 0;

  // Invoked whenever `OnActiveUserPrefServiceChanged()` is called.
  // `active_user_pref_service()` is guaranteed to be non-null within this
  // method and reflect the new active user. This is always called before the
  // first `RefreshFeatureState()` call is made for the new user.
  virtual void InitFeatureForNewActiveUser() {}

  // Optionally override to observe feature-specific prefs. Invoked whenever
  // `OnActiveUserPrefServiceChanged()` is called. Overrides can assume the
  // `pref_change_registrar` is already initialized.
  virtual void ListenForPrefChanges(
      PrefChangeRegistrar& pref_change_registrar) {}

  // Optional for recording a metric that tracks how often each `ScheduleType`
  // is used. Returns the full histogram name. By default, returns nullptr,
  // which disables the metric.
  virtual const char* GetScheduleTypeHistogramName() const;

  // Attempts restoring a previously stored schedule for the current user if
  // possible and returns true if so, false otherwise.
  bool MaybeRestoreSchedule();

  void StartWatchingPrefsChanges();

  void InitFromUserPrefs();

  void SetEnabledInternal(bool enabled, RefreshReason reason);

  // Called when the user pref for the enabled status of ScheduledFeature is
  // changed.
  void OnEnabledPrefChanged();

  // Called when the user pref for the schedule type is changed or initialized.
  void OnScheduleTypePrefChanged();

  // Refreshes feature state assuming `RefreshReason::kSettingsChanged`.
  // During initialization, `keep_manual_toggles_during_schedules` is set to
  // true, so the load user pref override any user current toggled setting. For
  // more detail about `keep_manual_toggles_during_schedules`, see `Refresh()`.
  void RefreshForSettingsChanged(bool keep_manual_toggles_during_schedules);

  // Called when either of the custom schedule prefs (custom start or end times)
  // are changed.
  void OnCustomSchedulePrefsChanged();

  // Refreshes the state of ScheduledFeature according to the currently set
  // parameters.
  //
  // If `keep_manual_toggles_during_schedules` is true, refreshing the schedule
  // will not override a previous user's decision to toggle the
  // ScheduledFeature status while the schedule is being used.
  void Refresh(RefreshReason reason, bool keep_manual_toggles_during_schedules);

  // Given the desired start and end times that determine the time interval
  // during which the feature will be ON, depending on the time of "now", it
  // refreshes the `timer_` to either schedule the future start or end of
  // the feature, as well as update the current status if needed.
  // This function should never be called if the schedule type is `kNone`.
  void RefreshScheduleTimer(base::Time start_time,
                            base::Time end_time,
                            RefreshReason reason,
                            bool keep_manual_toggles_during_schedules);

  // Schedule the next upcoming refresh of the feature state and save a copy of
  // the schedule's `current_snapshot` so that it can be restored in the future
  // for the current user if needed.
  //
  // `current_snapshot.target_status` may actually be the same as `GetEnabled()`
  // in some cases. For example, if it is currently `kSunrise` (`GetEnabled()`
  // is false), that means the next `ScheduleCheckpoint` is `kMorning`
  // (`target_status` is still false).
  void ScheduleNextRefresh(const ScheduleSnapshot& current_snapshot,
                           base::Time now);

  // Schedules the next call to retry a `Refresh()` operation after a failure.
  // Note this is for failure and `ScheduleNextRefresh()` is for success.
  void ScheduleNextRefreshRetry(bool keep_manual_toggles_during_schedules);

  void SetCurrentCheckpoint(ScheduleCheckpoint new_checkpoint);

  // The pref service of the currently active user. Can be null in
  // ash_unittests.
  raw_ptr<PrefService> active_user_pref_service_ = nullptr;

  base::flat_map<PrefService*, ScheduleSnapshot> per_user_schedule_snapshot_;

  // The timer that schedules the start and end of this feature when the
  // schedule type is either kSunsetToSunrise or kCustom. Safe to assume this is
  // never null; this is only reinitialized when the caller sets a new clock.
  std::unique_ptr<base::OneShotTimer> timer_;

  // The registrar used to watch prefs changes in the above
  // `active_user_pref_service_` from outside ash.
  // NOTE: Prefs are how Chrome communicates changes to the ScheduledFeature
  // settings controlled by this class from the WebUI settings.
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  const std::string prefs_path_enabled_;
  const std::string prefs_path_schedule_type_;
  const std::string prefs_path_custom_start_time_;
  const std::string prefs_path_custom_end_time_;

  raw_ptr<GeolocationController> geolocation_controller_;

  const Clock default_clock_;
  // May be reset in tests to override the time of "Now"; otherwise, points to
  // `default_clock_`. Should never be null.
  raw_ptr<const Clock> clock_ = nullptr;  // Not owned.

  // Optional Used in tests to override all local time operations.
  raw_ptr<const LocalTimeConverter> local_time_converter_ =
      nullptr;  // Not owned.

  // Never persisted anywhere. Must stay in sync with the feature's current
  // "enabled" state.
  ScheduleCheckpoint current_checkpoint_ = ScheduleCheckpoint::kDisabled;

  base::ObserverList<CheckpointObserver> checkpoint_observers_;

  // When a call to `Refresh()` fails, retry with exponential backoff.
  net::BackoffEntry refresh_failure_backoff_;

  // Transient storage of the `RefreshReason` to be plumbed from
  // `SetEnabledInternal()` to the `OnEnabledPrefChanged()` callback.
  RefreshReason set_enabled_refresh_reason_ = RefreshReason::kExternal;
};

}  // namespace ash

namespace base {

template <>
struct ScopedObservationTraits<ash::ScheduledFeature,
                               ash::ScheduledFeature::CheckpointObserver> {
  static void AddObserver(ash::ScheduledFeature* source,
                          ash::ScheduledFeature::CheckpointObserver* observer) {
    source->AddCheckpointObserver(observer);
  }
  static void RemoveObserver(
      ash::ScheduledFeature* source,
      ash::ScheduledFeature::CheckpointObserver* observer) {
    source->RemoveCheckpointObserver(observer);
  }
};

}  // namespace base

#endif  // ASH_SYSTEM_SCHEDULED_FEATURE_SCHEDULED_FEATURE_H_
