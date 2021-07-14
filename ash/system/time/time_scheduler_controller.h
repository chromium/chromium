// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TIME_TIME_SCHEDULER_CONTROLLER_H_
#define ASH_SYSTEM_TIME_TIME_SCHEDULER_CONTROLLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/public/cpp/simple_geo_position.h"
#include "ash/system/time/time_of_day.h"
#include "base/containers/flat_map.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/prefs/pref_change_registrar.h"
#include "third_party/icu/source/i18n/astro.h"

class PrefService;

namespace ash {

// Schedule a timer to enable and disable a mode specified in a user pref
// `prefs_path_enabled`. This class also enforces a delegate which requires
// time zone, sunrise, and sunset time.
class ASH_EXPORT TimeSchedulerController {
 public:
  // This class enables us to inject fake values for "Now" as well as the sunset
  // and sunrise times, so that we can reliably test the behavior in various
  // schedule types and times.
  class Delegate {
   public:
    // TimeSchedulerController owns the delegate.
    virtual ~Delegate() = default;

    // Gets the current time.
    virtual base::Time GetNow() const = 0;

    // Gets the sunset and sunrise times.
    virtual base::Time GetSunsetTime() const = 0;
    virtual base::Time GetSunriseTime() const = 0;

    // Provides the delegate with the geoposition so that it can be used to
    // calculate sunset and sunrise times.
    // Returns true if |position| is different than the current known value,
    // potentially requiring a refresh of the schedule. False otherwise.
    virtual bool SetGeoposition(const SimpleGeoposition& position) = 0;

    // Returns true if a geoposition value is available.
    virtual bool HasGeoposition() const = 0;
  };

  enum class TimeSetterSource {
    // Short animation (2 seconds) used for manual changes of the status
    // by the user.
    kUser,

    // Long animation (20 seconds) used for applying the color temperature
    // gradually as a result of getting into or out of the automatically
    // scheduled the mode. This gives the user a smooth transition.
    kAutomatic,
  };

  static const int kDefaultStartTimeOffsetMinutes = 18 * 60;
  static const int kDefaultEndTimeOffsetMinutes = 6 * 60;

  TimeSchedulerController();
  TimeSchedulerController(
      const std::string prefs_path_enabled,
      const std::string prefs_path_latitude,
      const std::string prefs_path_longitude,
      base::RepeatingCallback<void(bool, TimeSetterSource)>
          set_enabled_callback,
      base::RepeatingCallback<void()> refresh_state_callback);
  ~TimeSchedulerController();

  base::OneShotTimer* timer() { return &timer_; }
  bool is_current_geoposition_from_cache() const {
    return is_current_geoposition_from_cache_;
  }
  Delegate* delegate() { return delegate_.get(); }

  // Given the desired start and end times that determine the time interval
  // during which the mode will be ON, depending on the time of "now", it
  // refreshes the |timer_| to either schedule the future start or end of
  // the mode, as well as update the current status if needed.
  // For |did_schedule_change| and |keep_manual_toggles_during_schedules|, see
  // Refresh() above.
  // This function should never be called if the schedule type is |kNone|.
  void RefreshScheduleTimer(PrefService* active_user_pref_service,
                            base::Time start_time,
                            base::Time end_time,
                            bool did_schedule_change,
                            bool keep_manual_toggles_during_schedules);

  // Schedule the upcoming next toggle of the mode. This is used for the
  // automatic status changes of the mode which always use an
  // AnimationDurationType::kLong.
  void ScheduleNextToggle(PrefService* active_user_pref_service,
                          base::TimeDelta delay,
                          bool old_status);

  void StopTimer();

  // Called only when the active user changes in order to see if we need to use
  // a previously cached geoposition value from the active user's prefs.
  void LoadCachedGeopositionIfNeeded(PrefService* active_user_pref_service);

  // Called whenever we receive a new geoposition update to cache it in all
  // logged-in users' prefs so that it can be used later in the event of not
  // being able to retrieve a valid geoposition.
  void StoreCachedGeoposition(const SimpleGeoposition& position);

  // Attempts restoring a previously stored schedule for the active user when
  // the current status is `is_enabled` if possible and returns true if so,
  // false otherwise.
  bool MaybeRestoreSchedule(PrefService* active_user_pref_service);

  void SetDelegateForTesting(std::unique_ptr<Delegate> delegate) {
    delegate_ = std::move(delegate);
  }

 private:
  // Return true if user pref `prefs_path_enabled_` indicates the enabled mode.
  bool GetEnabled(PrefService* active_user_pref_service) const;

  // Note that the below computation is intentionally performed every time
  // GetSunsetTime() or GetSunriseTime() is called rather than once whenever we
  // receive a geoposition (which happens at least once a day). This increases
  // the chances of getting accurate values, especially around DST changes.
  base::Time GetSunRiseSet(bool sunrise) const;

  // Tracks the upcoming state changes per each user due to automatic
  // schedules. This can be used to restore a manually toggled status while the
  // schedule is being used. See MaybeRestoreSchedule().
  struct ScheduleTargetState {
    // The time at which the mode will switch to |target_status| defined
    // below.
    base::Time target_time;
    bool target_status;
  };

  base::flat_map<PrefService*, ScheduleTargetState>
      per_user_schedule_target_state_;

  // The timer that schedules the start and end of the mode when the schedule
  // type is either kSunsetToSunrise or kCustom.
  base::OneShotTimer timer_;

  // True if the current geoposition value used by the Delegate is from a
  // previously cached value in the user prefs of any of the users in the
  // current session. It is reset to false once we receive a newly-updated
  // geoposition from the client.
  // This is used to treat the current geoposition as temporary until we receive
  // a valid geoposition update, and also not to let a cached geoposition value
  // to leak to another user for privacy reasons.
  bool is_current_geoposition_from_cache_ = false;

  const std::string prefs_path_enabled_;
  const std::string prefs_path_latitude_;
  const std::string prefs_path_longitude_;
  base::RepeatingCallback<void(bool, TimeSetterSource)> set_enabled_callback_;
  base::RepeatingCallback<void()> refresh_state_callback_;
  std::unique_ptr<SimpleGeoposition> geoposition_;

  std::unique_ptr<TimeSchedulerController::Delegate> delegate_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_TIME_TIME_SCHEDULER_CONTROLLER_H_
