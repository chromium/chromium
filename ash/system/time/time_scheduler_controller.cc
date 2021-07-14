// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/time/time_scheduler_controller.h"

#include <cmath>
#include <memory>

#include "ash/constants/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/bind.h"
#include "base/i18n/time_formatting.h"
#include "base/logging.h"
#include "base/numerics/ranges.h"
#include "base/time/time.h"
#include "components/prefs/pref_service.h"
#include "third_party/icu/source/i18n/astro.h"
#include "ui/aura/env.h"

namespace ash {

namespace {

class TimeSchedulerControllerDelegateImpl
    : public TimeSchedulerController::Delegate {
 public:
  TimeSchedulerControllerDelegateImpl() = default;
  ~TimeSchedulerControllerDelegateImpl() override = default;

  // ash::NightLightControllerImpl::Delegate:
  base::Time GetNow() const override { return base::Time::Now(); }
  base::Time GetSunsetTime() const override { return GetSunRiseSet(false); }
  base::Time GetSunriseTime() const override { return GetSunRiseSet(true); }
  bool SetGeoposition(const SimpleGeoposition& position) override {
    if (geoposition_ && *geoposition_ == position)
      return false;

    geoposition_ = std::make_unique<SimpleGeoposition>(position);
    return true;
  }
  bool HasGeoposition() const override { return !!geoposition_; }

 private:
  // Note that the below computation is intentionally performed every time
  // GetSunsetTime() or GetSunriseTime() is called rather than once whenever we
  // receive a geoposition (which happens at least once a day). This increases
  // the chances of getting accurate values, especially around DST changes.
  base::Time GetSunRiseSet(bool sunrise) const {
    if (!HasGeoposition()) {
      LOG(ERROR) << "Invalid geoposition. Using default time for "
                 << (sunrise ? "sunrise." : "sunset.");
      return sunrise
                 ? TimeOfDay(
                       TimeSchedulerController::kDefaultEndTimeOffsetMinutes)
                       .ToTimeToday()
                 : TimeOfDay(
                       TimeSchedulerController::kDefaultStartTimeOffsetMinutes)
                       .ToTimeToday();
    }

    icu::CalendarAstronomer astro(geoposition_->longitude,
                                  geoposition_->latitude);
    // For sunset and sunrise times calculations to be correct, the time of the
    // icu::CalendarAstronomer object should be set to a time near local noon.
    // This avoids having the computation flopping over into an adjacent day.
    // See the documentation of icu::CalendarAstronomer::getSunRiseSet().
    // Note that the icu calendar works with milliseconds since epoch, and
    // base::Time::FromDoubleT() / ToDoubleT() work with seconds since epoch.
    const double midday_today_sec =
        TimeOfDay(12 * 60).ToTimeToday().ToDoubleT();
    astro.setTime(midday_today_sec * 1000.0);
    const double sun_rise_set_ms = astro.getSunRiseSet(sunrise);
    return base::Time::FromDoubleT(sun_rise_set_ms / 1000.0);
  }

  std::unique_ptr<SimpleGeoposition> geoposition_;
};

}  // namespace

TimeSchedulerController::TimeSchedulerController() {}

TimeSchedulerController::TimeSchedulerController(
    const std::string prefs_path_enabled,
    const std::string prefs_path_latitude,
    const std::string prefs_path_longitude,
    base::RepeatingCallback<void(bool, TimeSetterSource)> set_enabled_callback,
    base::RepeatingCallback<void()> refresh_state_callback)
    : prefs_path_enabled_(prefs_path_enabled),
      prefs_path_latitude_(prefs_path_latitude),
      prefs_path_longitude_(prefs_path_longitude),
      set_enabled_callback_(std::move(set_enabled_callback)),
      refresh_state_callback_(std::move(refresh_state_callback)),
      delegate_(std::make_unique<TimeSchedulerControllerDelegateImpl>()) {}

TimeSchedulerController::~TimeSchedulerController() {}

void TimeSchedulerController::RefreshScheduleTimer(
    PrefService* active_user_pref_service,
    base::Time start_time,
    base::Time end_time,
    bool did_schedule_change,
    bool keep_manual_toggles_during_schedules) {
  bool is_enabled = GetEnabled(active_user_pref_service);
  if (keep_manual_toggles_during_schedules &&
      MaybeRestoreSchedule(active_user_pref_service)) {
    refresh_state_callback_.Run();
    return;
  }

  // NOTE: Users can set any weird combinations.
  const base::Time now = delegate()->GetNow();
  if (end_time <= start_time) {
    // Example:
    // Start: 9:00 PM, End: 6:00 AM.
    //
    //       6:00                21:00
    // <----- + ------------------ + ----->
    //        |                    |
    //       end                 start
    //
    // Note that the above times are times of day (today). It is important to
    // know where "now" is with respect to these times to decide how to adjust
    // them.
    if (end_time >= now) {
      // If the end time (today) is greater than the time now, this means "now"
      // is within the NightLight schedule, and the start time is actually
      // yesterday. The above timeline is interpreted as:
      //
      //   21:00 (-1day)              6:00
      // <----- + ----------- + ------ + ----->
      //        |             |        |
      //      start          now      end
      //
      start_time -= base::TimeDelta::FromDays(1);
    } else {
      // Two possibilities here:
      // - Either "now" is greater than the end time, but less than start time.
      //   This means NightLight is outside the schedule, waiting for the next
      //   start time. The end time is actually a day later.
      // - Or "now" is greater than both the start and end times. This means
      //   NightLight is within the schedule, waiting to turn off at the next
      //   end time, which is also a day later.
      end_time += base::TimeDelta::FromDays(1);
    }
  }

  DCHECK_GE(end_time, start_time);

  // The target status that we need to set NightLight to now if a change of
  // status is needed immediately.
  bool enable_now = false;

  // Where are we now with respect to the start and end times?
  if (now < start_time) {
    // Example:
    // Start: 6:00 PM today, End: 6:00 AM tomorrow, Now: 4:00 PM.
    //
    // <----- + ----------- + ----------- + ----->
    //        |             |             |
    //       now          start          end
    //
    // In this case, we need to disable NightLight immediately if it's enabled.
    enable_now = false;
  } else if (now >= start_time && now < end_time) {
    // Example:
    // Start: 6:00 PM today, End: 6:00 AM tomorrow, Now: 11:00 PM.
    //
    // <----- + ----------- + ----------- + ----->
    //        |             |             |
    //      start          now           end
    //
    // Start NightLight right away. Our future start time is a day later than
    // its current value.
    enable_now = true;
    start_time += base::TimeDelta::FromDays(1);
  } else {  // now >= end_time.
    // Example:
    // Start: 6:00 PM today, End: 10:00 PM today, Now: 11:00 PM.
    //
    // <----- + ----------- + ----------- + ----->
    //        |             |             |
    //      start          end           now
    //
    // In this case, our future start and end times are a day later from their
    // current values. NightLight needs to be ended immediately if it's already
    // enabled.
    enable_now = false;
    start_time += base::TimeDelta::FromDays(1);
    end_time += base::TimeDelta::FromDays(1);
  }

  // After the above processing, the start and end time are all in the future.
  DCHECK_GE(start_time, now);
  DCHECK_GE(end_time, now);

  if (did_schedule_change && enable_now != is_enabled) {
    // If the change in the schedule introduces a change in the status, then
    // calling SetEnabled() is all we need, since it will trigger a change in
    // the user prefs to which we will respond by calling Refresh(). This will
    // end up in this function again, adjusting all the needed schedules.
    set_enabled_callback_.Run(enable_now, TimeSetterSource::kUser);
    return;
  }

  // We reach here in one of the following conditions:
  // 1) If schedule changes don't result in changes in the status, we need to
  // explicitly update the timer to re-schedule the next toggle to account for
  // any changes.
  // 2) The user has just manually toggled the status of NightLight either from
  // the System Menu or System Settings. In this case, we respect the user
  // wish and maintain the current status that they desire, but we schedule the
  // status to be toggled according to the time that corresponds with the
  // opposite status of the current one.
  ScheduleNextToggle(active_user_pref_service,
                     is_enabled ? end_time - now : start_time - now,
                     is_enabled);
  refresh_state_callback_.Run();
}

void TimeSchedulerController::ScheduleNextToggle(
    PrefService* active_user_pref_service,
    base::TimeDelta delay,
    bool old_status) {
  const bool new_status = !old_status;
  const base::Time target_time = delegate()->GetNow() + delay;

  per_user_schedule_target_state_[active_user_pref_service] =
      ScheduleTargetState{target_time, new_status};

  VLOG(1) << "Setting Night Light to toggle to "
          << (new_status ? "enabled" : "disabled") << " at "
          << base::TimeFormatTimeOfDay(target_time);
  timer_.Start(FROM_HERE, delay,
               base::BindRepeating(set_enabled_callback_, new_status,
                                   TimeSetterSource::kAutomatic));
}

void TimeSchedulerController::StopTimer() {
  timer_.Stop();
}

void TimeSchedulerController::LoadCachedGeopositionIfNeeded(
    PrefService* active_user_pref_service) {
  DCHECK(active_user_pref_service);

  // Even if there is a geoposition, but it's coming from a previously cached
  // value, switching users should load the currently saved values for the
  // new user. This is to keep users' prefs completely separate. We only ignore
  // the cached values once we have a valid non-cached geoposition from any
  // user in the same session.
  if (delegate_->HasGeoposition() && !is_current_geoposition_from_cache_)
    return;

  if (!active_user_pref_service->HasPrefPath(prefs_path_latitude_) ||
      !active_user_pref_service->HasPrefPath(prefs_path_longitude_)) {
    VLOG(1) << "No valid current geoposition and no valid cached geoposition"
               " are available. Will use default times for sunset / sunrise.";
    return;
  }

  VLOG(1) << "Temporarily using a previously cached geoposition.";
  delegate_->SetGeoposition(SimpleGeoposition{
      active_user_pref_service->GetDouble(prefs_path_latitude_),
      active_user_pref_service->GetDouble(prefs_path_longitude_)});
  is_current_geoposition_from_cache_ = true;
}

void TimeSchedulerController::StoreCachedGeoposition(
    const SimpleGeoposition& position) {
  is_current_geoposition_from_cache_ = false;
  const SessionControllerImpl* session_controller =
      Shell::Get()->session_controller();
  for (const auto& user_session : session_controller->GetUserSessions()) {
    PrefService* pref_service = session_controller->GetUserPrefServiceForUser(
        user_session->user_info.account_id);
    if (!pref_service)
      continue;

    pref_service->SetDouble(prefs_path_latitude_, position.latitude);
    pref_service->SetDouble(prefs_path_longitude_, position.longitude);
  }
}

bool TimeSchedulerController::MaybeRestoreSchedule(
    PrefService* active_user_pref_service) {
  bool is_enabled = GetEnabled(active_user_pref_service);
  auto iter = per_user_schedule_target_state_.find(active_user_pref_service);
  if (iter == per_user_schedule_target_state_.end())
    return false;

  ScheduleTargetState& target_state = iter->second;
  // It may be that the device was suspended for a very long time that the
  // target time is no longer valid.
  if (target_state.target_time <= delegate_->GetNow())
    return false;

  VLOG(1) << "Restoring a previous schedule.";
  DCHECK_NE(is_enabled, target_state.target_status);
  ScheduleNextToggle(active_user_pref_service,
                     target_state.target_time - delegate_->GetNow(),
                     is_enabled);
  return true;
}

bool TimeSchedulerController::GetEnabled(
    PrefService* active_user_pref_service) const {
  return active_user_pref_service &&
         active_user_pref_service->GetBoolean(prefs_path_enabled_);
}

base::Time TimeSchedulerController::GetSunRiseSet(bool sunrise) const {
  if (!delegate_->HasGeoposition()) {
    LOG(ERROR) << "Invalid geoposition. Using default time for "
               << (sunrise ? "sunrise." : "sunset.");
    return sunrise
               ? TimeOfDay(
                     TimeSchedulerController::kDefaultEndTimeOffsetMinutes)
                     .ToTimeToday()
               : TimeOfDay(
                     TimeSchedulerController::kDefaultStartTimeOffsetMinutes)
                     .ToTimeToday();
  }

  icu::CalendarAstronomer astro(geoposition_->longitude,
                                geoposition_->latitude);
  // For sunset and sunrise times calculations to be correct, the time of the
  // icu::CalendarAstronomer object should be set to a time near local noon.
  // This avoids having the computation flopping over into an adjacent day.
  // See the documentation of icu::CalendarAstronomer::getSunRiseSet().
  // Note that the icu calendar works with milliseconds since epoch, and
  // base::Time::FromDoubleT() / ToDoubleT() work with seconds since epoch.
  const double midday_today_sec = TimeOfDay(12 * 60).ToTimeToday().ToDoubleT();
  astro.setTime(midday_today_sec * 1000.0);
  const double sun_rise_set_ms = astro.getSunRiseSet(sunrise);
  return base::Time::FromDoubleT(sun_rise_set_ms / 1000.0);
}

}  // namespace ash
