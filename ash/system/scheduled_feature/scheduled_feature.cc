// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/scheduled_feature/scheduled_feature.h"

#include <cmath>
#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/geolocation/geolocation_controller.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/scheduled_feature/schedule_utils.h"
#include "base/cxx17_backports.h"
#include "base/functional/bind.h"
#include "base/i18n/time_formatting.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "third_party/icu/source/i18n/astro.h"
#include "ui/aura/env.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/vector3d_f.h"

namespace ash {

namespace {

// Default start time at 6:00 PM as an offset from 00:00.
constexpr int kDefaultStartTimeOffsetMinutes = 18 * 60;

// Default end time at 6:00 AM as an offset from 00:00.
constexpr int kDefaultEndTimeOffsetMinutes = 6 * 60;

bool IsEnabledAtCheckpoint(SunsetToSunriseCheckpoint checkpoint) {
  switch (checkpoint) {
    case SunsetToSunriseCheckpoint::kSunrise:
    case SunsetToSunriseCheckpoint::kMorning:
    case SunsetToSunriseCheckpoint::kLateAfternoon:
      return false;
    case SunsetToSunriseCheckpoint::kSunset:
      return true;
  }
}

}  // namespace

base::Time ScheduledFeature::Clock::Now() const {
  return base::Time::Now();
}

base::TimeTicks ScheduledFeature::Clock::NowTicks() const {
  return base::TimeTicks::Now();
}

ScheduledFeature::ScheduledFeature(
    const std::string prefs_path_enabled,
    const std::string prefs_path_schedule_type,
    const std::string prefs_path_custom_start_time,
    const std::string prefs_path_custom_end_time)
    : timer_(std::make_unique<base::OneShotTimer>()),
      prefs_path_enabled_(prefs_path_enabled),
      prefs_path_schedule_type_(prefs_path_schedule_type),
      prefs_path_custom_start_time_(prefs_path_custom_start_time),
      prefs_path_custom_end_time_(prefs_path_custom_end_time),
      geolocation_controller_(ash::GeolocationController::Get()),
      clock_(&default_clock_) {
  Shell::Get()->session_controller()->AddObserver(this);
  aura::Env::GetInstance()->AddObserver(this);
  chromeos::PowerManagerClient::Get()->AddObserver(this);
  // Check that both start or end times are supplied or both are absent.
  DCHECK_EQ(prefs_path_custom_start_time_.empty(),
            prefs_path_custom_end_time_.empty());
}

ScheduledFeature::~ScheduledFeature() {
  chromeos::PowerManagerClient::Get()->RemoveObserver(this);
  aura::Env::GetInstance()->RemoveObserver(this);
  Shell::Get()->session_controller()->RemoveObserver(this);
}

bool ScheduledFeature::GetEnabled() const {
  return active_user_pref_service_ &&
         active_user_pref_service_->GetBoolean(prefs_path_enabled_);
}

ScheduleType ScheduledFeature::GetScheduleType() const {
  if (active_user_pref_service_) {
    return static_cast<ScheduleType>(
        active_user_pref_service_->GetInteger(prefs_path_schedule_type_));
  }

  return ScheduleType::kNone;
}

TimeOfDay ScheduledFeature::GetCustomStartTime() const {
  DCHECK(!prefs_path_custom_start_time_.empty());
  return TimeOfDay(active_user_pref_service_
                       ? active_user_pref_service_->GetInteger(
                             prefs_path_custom_start_time_)
                       : kDefaultStartTimeOffsetMinutes)
      .SetClock(clock_);
}

TimeOfDay ScheduledFeature::GetCustomEndTime() const {
  DCHECK(!prefs_path_custom_end_time_.empty());
  return TimeOfDay(active_user_pref_service_
                       ? active_user_pref_service_->GetInteger(
                             prefs_path_custom_end_time_)
                       : kDefaultEndTimeOffsetMinutes)
      .SetClock(clock_);
}

bool ScheduledFeature::IsNowWithinSunsetSunrise() const {
  // The times below are all on the same calendar day.
  const base::Time now = clock_->Now();
  return now < geolocation_controller_->GetSunriseTime() ||
         now > geolocation_controller_->GetSunsetTime();
}

void ScheduledFeature::SetEnabled(bool enabled) {
  if (active_user_pref_service_)
    active_user_pref_service_->SetBoolean(prefs_path_enabled_, enabled);
}

void ScheduledFeature::SetScheduleType(ScheduleType type) {
  if (!active_user_pref_service_)
    return;

  if (type == ScheduleType::kCustom && (prefs_path_custom_start_time_.empty() ||
                                        prefs_path_custom_end_time_.empty())) {
    NOTREACHED();
    return;
  }

  active_user_pref_service_->SetInteger(prefs_path_schedule_type_,
                                        static_cast<int>(type));
}

void ScheduledFeature::SetCustomStartTime(TimeOfDay start_time) {
  DCHECK(!prefs_path_custom_start_time_.empty());
  if (active_user_pref_service_) {
    active_user_pref_service_->SetInteger(
        prefs_path_custom_start_time_,
        start_time.offset_minutes_from_zero_hour());
  }
}

void ScheduledFeature::SetCustomEndTime(TimeOfDay end_time) {
  DCHECK(!prefs_path_custom_end_time_.empty());
  if (active_user_pref_service_) {
    active_user_pref_service_->SetInteger(
        prefs_path_custom_end_time_, end_time.offset_minutes_from_zero_hour());
  }
}

void ScheduledFeature::OnActiveUserPrefServiceChanged(
    PrefService* pref_service) {
  if (pref_service == active_user_pref_service_)
    return;

  // Initial login and user switching in multi profiles.
  active_user_pref_service_ = pref_service;
  InitFromUserPrefs();
}

void ScheduledFeature::OnGeopositionChanged(bool possible_change_in_timezone) {
  DCHECK(GetScheduleType() != ScheduleType::kNone);

  VLOG(1) << "Received new geoposition.";

  // We only keep manual toggles if there's no change in timezone.
  const bool keep_manual_toggles_during_schedules =
      !possible_change_in_timezone;

  Refresh(/*did_schedule_change=*/true, keep_manual_toggles_during_schedules);
}

void ScheduledFeature::SuspendDone(base::TimeDelta sleep_duration) {
  // Time changes while the device is suspended. We need to refresh the schedule
  // upon device resume to know what the status should be now.
  Refresh(/*did_schedule_change=*/true,
          /*keep_manual_toggles_during_schedules=*/true);
}

void ScheduledFeature::SetClockForTesting(const Clock* clock) {
  CHECK(clock);
  clock_ = clock;
  CHECK(!timer_->IsRunning());
  timer_ = std::make_unique<base::OneShotTimer>(clock_);
}

bool ScheduledFeature::MaybeRestoreSchedule() {
  DCHECK(active_user_pref_service_);
  DCHECK_NE(GetScheduleType(), ScheduleType::kNone);

  auto iter = per_user_schedule_target_state_.find(active_user_pref_service_);
  if (iter == per_user_schedule_target_state_.end())
    return false;

  const ScheduleTargetState& target_state = iter->second;
  const base::Time now = clock_->Now();
  // It may be that the device was suspended for a very long time that the
  // target time is no longer valid.
  if (target_state.target_time <= now)
    return false;

  VLOG(1) << "Restoring a previous schedule.";
  ScheduleNextRefresh(target_state.target_time - now,
                      target_state.target_status);
  return true;
}

void ScheduledFeature::StartWatchingPrefsChanges() {
  DCHECK(active_user_pref_service_);

  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(active_user_pref_service_);
  pref_change_registrar_->Add(
      prefs_path_enabled_,
      base::BindRepeating(&ScheduledFeature::OnEnabledPrefChanged,
                          base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs_path_schedule_type_,
      base::BindRepeating(&ScheduledFeature::OnScheduleTypePrefChanged,
                          base::Unretained(this),
                          /*keep_manual_toggles_during_schedules=*/false));

  if (!prefs_path_custom_start_time_.empty()) {
    pref_change_registrar_->Add(
        prefs_path_custom_start_time_,
        base::BindRepeating(&ScheduledFeature::OnCustomSchedulePrefsChanged,
                            base::Unretained(this)));
  }
  if (!prefs_path_custom_end_time_.empty()) {
    pref_change_registrar_->Add(
        prefs_path_custom_end_time_,
        base::BindRepeating(&ScheduledFeature::OnCustomSchedulePrefsChanged,
                            base::Unretained(this)));
  }
}

void ScheduledFeature::InitFromUserPrefs() {
  StartWatchingPrefsChanges();
  OnScheduleTypePrefChanged(/*keep_manual_toggles_during_schedules=*/true);
  is_first_user_init_ = false;
}

void ScheduledFeature::OnEnabledPrefChanged() {
  const bool enabled = GetEnabled();
  VLOG(1) << "Enable state changed. New state: " << enabled << ".";
  DCHECK(active_user_pref_service_);
  Refresh(/*did_schedule_change=*/false,
          /*keep_manual_toggles_during_schedules=*/false);
}

void ScheduledFeature::OnScheduleTypePrefChanged(
    bool keep_manual_toggles_during_schedules) {
  const ScheduleType schedule_type = GetScheduleType();
  // To prevent adding (or removing) an observer twice in a row when switching
  // between different users, we need to check `is_observing_geolocation_`.
  if (schedule_type == ScheduleType::kNone && is_observing_geolocation_) {
    geolocation_controller_->RemoveObserver(this);
    is_observing_geolocation_ = false;
  } else if (schedule_type != ScheduleType::kNone &&
             !is_observing_geolocation_) {
    geolocation_controller_->AddObserver(this);
    is_observing_geolocation_ = true;
  }
  Refresh(/*did_schedule_change=*/true, keep_manual_toggles_during_schedules);
}

void ScheduledFeature::OnCustomSchedulePrefsChanged() {
  DCHECK(active_user_pref_service_);
  Refresh(/*did_schedule_change=*/true,
          /*keep_manual_toggles_during_schedules=*/false);
}

void ScheduledFeature::Refresh(bool did_schedule_change,
                               bool keep_manual_toggles_during_schedules) {
  switch (GetScheduleType()) {
    case ScheduleType::kNone:
      timer_->Stop();
      RefreshFeatureState();
      return;
    case ScheduleType::kSunsetToSunrise:
      RefreshScheduleTimer(geolocation_controller_->GetSunsetTime(),
                           geolocation_controller_->GetSunriseTime(),
                           did_schedule_change,
                           keep_manual_toggles_during_schedules);
      return;
    case ScheduleType::kCustom:
      RefreshScheduleTimer(
          GetCustomStartTime().ToTimeToday(), GetCustomEndTime().ToTimeToday(),
          did_schedule_change, keep_manual_toggles_during_schedules);
      return;
  }
}

// The `SunsetToSunriseCheckpoint` usage in this method does not directly apply
// to `ScheduleType::kCustom`, but the business logic still works for that
// `ScheduleType` with no caller-facing impact. The internal `timer_` may just
// fire a couple more times a day and be no-ops.
void ScheduledFeature::RefreshScheduleTimer(
    base::Time start_time,
    base::Time end_time,
    bool did_schedule_change,
    bool keep_manual_toggles_during_schedules) {
  DCHECK(GetScheduleType() != ScheduleType::kNone);

  if (keep_manual_toggles_during_schedules && MaybeRestoreSchedule()) {
    RefreshFeatureState();
    return;
  }

  const base::Time now = clock_->Now();
  const schedule_utils::Position schedule_position =
      schedule_utils::GetCurrentPosition(
          /*sunrise_time=*/end_time, /*sunset_time=*/start_time, now);
  const bool enable_now =
      IsEnabledAtCheckpoint(schedule_position.current_checkpoint);
  const bool current_enabled = GetEnabled();

  base::TimeDelta time_until_next_refresh;
  bool next_feature_status = false;
  if (enable_now == current_enabled) {
    // The most standard case:
    next_feature_status =
        IsEnabledAtCheckpoint(schedule_position.next_checkpoint);
    time_until_next_refresh = schedule_position.time_until_next_checkpoint;
  } else if (did_schedule_change) {  // && enable_now != current_enabled
    // If the change in the schedule introduces a change in the status, then
    // calling SetEnabled() is all we need, since it will trigger a change in
    // the user prefs to which we will respond by calling Refresh(). This will
    // end up in this function again and enter the case above, adjusting all
    // the needed schedules.
    SetEnabled(enable_now);
    return;
  } else {  // enable_now != current_enabled && !did_schedule_change
    // The user manually toggled the feature status to the opposite of what the
    // schedule says. In this case, ignore the current `schedule_position` since
    // it doesn't apply anymore. Just find the next time that the feature will
    // flip back to a status that matches the schedule, and then normal
    // scheduling logic (the 2 cases above) will resume.
    next_feature_status = !current_enabled;
    const base::Time next_toggle_time = schedule_utils::ShiftWithinOneDayFrom(
        now, current_enabled ? end_time : start_time);
    time_until_next_refresh = next_toggle_time - now;
  }

  // We reach here in one of the following conditions:
  // 1) If schedule changes don't result in changes in the status, we need to
  // explicitly update the timer to re-schedule the next refresh to account for
  // any changes.
  // 2) The user has just manually toggled the status of the feature either from
  // the System Menu or System Settings. In this case, we respect the user
  // wish and maintain the current status that they desire, but we schedule the
  // status to be toggled according to the time that corresponds with the
  // opposite status of the current one.
  ScheduleNextRefresh(time_until_next_refresh, next_feature_status);
  RefreshFeatureState();
}

void ScheduledFeature::ScheduleNextRefresh(base::TimeDelta delay,
                                           bool target_status) {
  DCHECK(active_user_pref_service_);
  DCHECK_GE(delay, base::TimeDelta());

  const base::Time target_time = clock_->Now() + delay;
  per_user_schedule_target_state_[active_user_pref_service_] =
      ScheduleTargetState{target_time, target_status};
  base::OnceClosure timer_cb;
  if (target_status == GetEnabled()) {
    timer_cb =
        base::BindOnce(&ScheduledFeature::Refresh, base::Unretained(this),
                       /*did_schedule_change=*/false,
                       /*keep_manual_toggles_during_schedules=*/false);
  } else {
    timer_cb = base::BindOnce(&ScheduledFeature::SetEnabled,
                              base::Unretained(this), target_status);
  }
  VLOG(1) << "Setting " << GetFeatureName() << " to refresh to "
          << (target_status ? "enabled" : "disabled") << " at "
          << base::TimeFormatTimeOfDay(target_time);
  timer_->Start(FROM_HERE, delay, std::move(timer_cb));
}

}  // namespace ash
