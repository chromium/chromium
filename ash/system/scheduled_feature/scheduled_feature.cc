// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/scheduled_feature/scheduled_feature.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/geolocation/geolocation_controller.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/scheduled_feature/schedule_utils.h"
#include "base/functional/bind.h"
#include "base/i18n/time_formatting.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "third_party/icu/source/i18n/astro.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/vector3d_f.h"

namespace ash {

namespace {

// Default start time at 6:00 PM as an offset from 00:00.
constexpr int kDefaultStartTimeOffsetMinutes = 18 * 60;

// Default end time at 6:00 AM as an offset from 00:00.
constexpr int kDefaultEndTimeOffsetMinutes = 6 * 60;

// The only known `Refresh()` failure currently is b/285187343, where getting
// the default local sunrise/sunset times fails. Getting local time is not
// a network request; the current theory is an unknown bad kernel state.
// Therefore, a more aggressive retry policy is acceptable here.
constexpr net::BackoffEntry::Policy kRefreshFailureBackoffPolicy = {
    0,          // Number of initial errors to ignore.
    500,        // Initial delay in ms.
    2.0,        // Factor by which the waiting time will be multiplied.
    0.2,        // Fuzzing percentage.
    60 * 1000,  // Maximum delay in ms. (1 minute)
    -1,         // Never discard the entry.
    true,       // Use initial delay.
};

bool IsEnabledAtCheckpoint(ScheduleCheckpoint checkpoint) {
  switch (checkpoint) {
    case ScheduleCheckpoint::kDisabled:
    case ScheduleCheckpoint::kSunrise:
    case ScheduleCheckpoint::kMorning:
    case ScheduleCheckpoint::kLateAfternoon:
      return false;
    case ScheduleCheckpoint::kEnabled:
    case ScheduleCheckpoint::kSunset:
      return true;
  }
}

// Converts a boolean feature `is_enabled` state to the appropriate
// `ScheduleCheckpoint` for the given `schedule_type`.
ScheduleCheckpoint GetCheckpointForEnabledState(bool is_enabled,
                                                ScheduleType schedule_type) {
  switch (schedule_type) {
    case ScheduleType::kNone:
    case ScheduleType::kCustom:
      return is_enabled ? ScheduleCheckpoint::kEnabled
                        : ScheduleCheckpoint::kDisabled;
    case ScheduleType::kSunsetToSunrise:
      return is_enabled ? ScheduleCheckpoint::kSunset
                        : ScheduleCheckpoint::kSunrise;
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
      clock_(&default_clock_),
      refresh_failure_backoff_(&kRefreshFailureBackoffPolicy) {
  Shell::Get()->session_controller()->AddObserver(this);
  chromeos::PowerManagerClient::Get()->AddObserver(this);
  // Check that both start or end times are supplied or both are absent.
  DCHECK_EQ(prefs_path_custom_start_time_.empty(),
            prefs_path_custom_end_time_.empty());
}

ScheduledFeature::~ScheduledFeature() {
  geolocation_controller_->RemoveObserver(this);
  chromeos::PowerManagerClient::Get()->RemoveObserver(this);
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
      .SetClock(clock_)
      .SetLocalTimeConverter(local_time_converter_);
}

TimeOfDay ScheduledFeature::GetCustomEndTime() const {
  DCHECK(!prefs_path_custom_end_time_.empty());
  return TimeOfDay(active_user_pref_service_
                       ? active_user_pref_service_->GetInteger(
                             prefs_path_custom_end_time_)
                       : kDefaultEndTimeOffsetMinutes)
      .SetClock(clock_)
      .SetLocalTimeConverter(local_time_converter_);
}

void ScheduledFeature::SetEnabled(bool enabled) {
  SetEnabledInternal(enabled, RefreshReason::kExternal);
}

void ScheduledFeature::SetScheduleType(ScheduleType type) {
  if (!active_user_pref_service_)
    return;

  if (type == ScheduleType::kCustom && (prefs_path_custom_start_time_.empty() ||
                                        prefs_path_custom_end_time_.empty())) {
    NOTREACHED();
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

void ScheduledFeature::AddCheckpointObserver(CheckpointObserver* obs) {
  checkpoint_observers_.AddObserver(obs);
}

void ScheduledFeature::RemoveCheckpointObserver(CheckpointObserver* obs) {
  checkpoint_observers_.RemoveObserver(obs);
}

void ScheduledFeature::OnActiveUserPrefServiceChanged(
    PrefService* pref_service) {
  if (pref_service == active_user_pref_service_)
    return;

  // TODO(afakhry|yjliu): Remove this VLOG when https://crbug.com/1015474 is
  // fixed.
  auto vlog_helper = [this](const PrefService* pref_service) -> std::string {
    if (!pref_service) {
      return "None";
    }
    return base::StringPrintf(
        "{State %s, Schedule Type: %d}",
        pref_service->GetBoolean(prefs_path_enabled_) ? "enabled" : "disabled",
        pref_service->GetInteger(prefs_path_schedule_type_));
  };
  VLOG(1) << "Switching user pref service from "
          << vlog_helper(active_user_pref_service_) << " to "
          << vlog_helper(pref_service) << ".";

  // Initial login and user switching in multi profiles.
  active_user_pref_service_ = pref_service;
  // Give the feature a chance to do its own initialization before the first
  // call to `RefreshFeatureState()` (made within `InitFromUserPrefs()`).
  InitFeatureForNewActiveUser();
  InitFromUserPrefs();
}

void ScheduledFeature::OnGeopositionChanged(bool possible_change_in_timezone) {
  DCHECK(GetScheduleType() != ScheduleType::kNone);

  VLOG(1) << "Received new geoposition.";

  // We only keep manual toggles if there's no change in timezone.
  const bool keep_manual_toggles_during_schedules =
      !possible_change_in_timezone;

  Refresh(RefreshReason::kReset, keep_manual_toggles_during_schedules);
}

void ScheduledFeature::SuspendDone(base::TimeDelta sleep_duration) {
  // Time changes while the device is suspended. We need to refresh the schedule
  // upon device resume to know what the status should be now.
  Refresh(RefreshReason::kReset,
          /*keep_manual_toggles_during_schedules=*/true);
}

base::Time ScheduledFeature::Now() const {
  return clock_->Now();
}

void ScheduledFeature::SetClockForTesting(const Clock* clock) {
  CHECK(clock);
  clock_ = clock;
  CHECK(!timer_->IsRunning());
  timer_ = std::make_unique<base::OneShotTimer>(clock_);
}

void ScheduledFeature::SetLocalTimeConverterForTesting(
    const LocalTimeConverter* local_time_converter) {
  local_time_converter_ = local_time_converter;
}

void ScheduledFeature::SetTaskRunnerForTesting(
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  CHECK(!timer_->IsRunning());
  timer_->SetTaskRunner(std::move(task_runner));
}

const char* ScheduledFeature::GetScheduleTypeHistogramName() const {
  return nullptr;
}

bool ScheduledFeature::MaybeRestoreSchedule() {
  DCHECK(active_user_pref_service_);
  DCHECK_NE(GetScheduleType(), ScheduleType::kNone);

  auto iter = per_user_schedule_snapshot_.find(active_user_pref_service_);
  if (iter == per_user_schedule_snapshot_.end()) {
    return false;
  }

  const ScheduleSnapshot& snapshot_to_restore = iter->second;
  const base::Time now = Now();
  // It may be that the device was suspended for a very long time that the
  // target time is no longer valid.
  if (snapshot_to_restore.target_time <= now) {
    return false;
  }

  VLOG(1) << "Restoring a previous schedule.";
  current_checkpoint_ = snapshot_to_restore.current_checkpoint;
  ScheduleNextRefresh(snapshot_to_restore, now);
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
                          base::Unretained(this)));

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
  ListenForPrefChanges(*pref_change_registrar_);
}

void ScheduledFeature::InitFromUserPrefs() {
  StartWatchingPrefsChanges();
  RefreshForSettingsChanged(/*keep_manual_toggles_during_schedules=*/true);
}

void ScheduledFeature::SetEnabledInternal(bool enabled, RefreshReason reason) {
  DVLOG(1) << "Setting " << GetFeatureName() << " enabled to " << enabled
           << " at " << Now();
  set_enabled_refresh_reason_ = reason;
  if (active_user_pref_service_)
    active_user_pref_service_->SetBoolean(prefs_path_enabled_, enabled);
}

void ScheduledFeature::OnEnabledPrefChanged() {
  const bool enabled = GetEnabled();
  VLOG(1) << "Enable state changed. New state: " << enabled << ".";
  DCHECK(active_user_pref_service_);
  const RefreshReason current_reason = set_enabled_refresh_reason_;
  // Reset the reason to `kExternal` in case an external caller directly
  // modifies the pref afterwards.
  set_enabled_refresh_reason_ = RefreshReason::kExternal;
  Refresh(current_reason,
          /*keep_manual_toggles_during_schedules=*/false);
}

void ScheduledFeature::OnScheduleTypePrefChanged() {
  const ScheduleType schedule_type = GetScheduleType();
  VLOG(1) << "Schedule type changed. New type: "
          << static_cast<int>(schedule_type) << ".";
  if (const char* const schedule_type_histogram =
          GetScheduleTypeHistogramName()) {
    base::UmaHistogramEnumeration(schedule_type_histogram, schedule_type);
  }
  RefreshForSettingsChanged(/*keep_manual_toggles_during_schedules=*/false);
}

void ScheduledFeature::RefreshForSettingsChanged(
    bool keep_manual_toggles_during_schedules) {
  // To prevent adding an observer twice in a row when switching between
  // different users, we need to check `HasObserver()`.
  if (GetScheduleType() == ScheduleType::kNone) {
    geolocation_controller_->RemoveObserver(this);
  } else if (!geolocation_controller_->HasObserver(this)) {
    geolocation_controller_->AddObserver(this);
  }
  Refresh(RefreshReason::kSettingsChanged,
          keep_manual_toggles_during_schedules);
}

void ScheduledFeature::OnCustomSchedulePrefsChanged() {
  DCHECK(active_user_pref_service_);
  Refresh(RefreshReason::kSettingsChanged,
          /*keep_manual_toggles_during_schedules=*/false);
}

void ScheduledFeature::Refresh(RefreshReason reason,
                               bool keep_manual_toggles_during_schedules) {
  std::optional<base::Time> start_time;
  std::optional<base::Time> end_time;
  const ScheduleType schedule_type = GetScheduleType();
  switch (schedule_type) {
    case ScheduleType::kNone:
      timer_->Stop();
      RefreshFeatureState(reason);
      SetCurrentCheckpoint(
          GetCheckpointForEnabledState(GetEnabled(), ScheduleType::kNone));
      return;
    case ScheduleType::kSunsetToSunrise: {
      const base::expected<base::Time, GeolocationController::SunRiseSetError>
          sunrise_time = geolocation_controller_->GetSunriseTime();
      const base::expected<base::Time, GeolocationController::SunRiseSetError>
          sunset_time = geolocation_controller_->GetSunsetTime();
      if (sunrise_time == GeolocationController::kNoSunRiseSet ||
          sunset_time == GeolocationController::kNoSunRiseSet) {
        // Simply disable the feature in this corner case. Since sunset and
        // sunrise are exactly the same, there is no time for it to be enabled.
        start_time = Now();
        end_time = start_time;
      } else if (sunrise_time.has_value() && sunset_time.has_value()) {
        start_time = sunset_time.value();
        end_time = sunrise_time.value();
      } else {
        // Sunrise or sunset is temporarily unavailable. Leave `start_time` and
        // `end_time` unset.
      }
      break;
    }
    case ScheduleType::kCustom:
      start_time = GetCustomStartTime().ToTimeToday();
      end_time = GetCustomEndTime().ToTimeToday();
      break;
  }

  // b/285187343: Timestamps can legitimately be null if getting local time
  // fails.
  if (!start_time || !end_time) {
    LOG(ERROR) << "Received null start/end times at " << Now();
    ScheduleNextRefreshRetry(keep_manual_toggles_during_schedules);
    // Best effort to still make `current_checkpoint_` as accurate as possible
    // before exiting and not be in an inconsistent state. The next successful
    // `Refresh()` will make `current_checkpoint_` 100% accurate again.
    SetCurrentCheckpoint(
        GetCheckpointForEnabledState(GetEnabled(), schedule_type));
    return;
  }

  RefreshScheduleTimer(*start_time, *end_time, reason,
                       keep_manual_toggles_during_schedules);
}

// The `ScheduleCheckpoint` usage in this method does not directly apply
// to `ScheduleType::kCustom`, but the business logic still works for that
// `ScheduleType` with no caller-facing impact. The internal `timer_` may just
// fire a couple more times a day and be no-ops.
void ScheduledFeature::RefreshScheduleTimer(
    base::Time start_time,
    base::Time end_time,
    RefreshReason reason,
    bool keep_manual_toggles_during_schedules) {
  const ScheduleType schedule_type = GetScheduleType();
  DCHECK(schedule_type != ScheduleType::kNone);

  if (keep_manual_toggles_during_schedules && MaybeRestoreSchedule()) {
    RefreshFeatureState(reason);
    return;
  }

  const base::Time now = Now();
  const schedule_utils::Position schedule_position =
      schedule_utils::GetCurrentPosition(now, start_time, end_time,
                                         schedule_type);
  const bool enable_now =
      IsEnabledAtCheckpoint(schedule_position.current_checkpoint);
  const bool current_enabled = GetEnabled();

  base::TimeDelta time_until_next_refresh;
  bool next_feature_status = false;
  ScheduleCheckpoint new_checkpoint = current_checkpoint_;
  if (enable_now == current_enabled) {
    // The most standard case:
    next_feature_status =
        IsEnabledAtCheckpoint(schedule_position.next_checkpoint);
    time_until_next_refresh = schedule_position.time_until_next_checkpoint;
    new_checkpoint = schedule_position.current_checkpoint;
  } else if (reason == RefreshReason::kSettingsChanged ||
             reason == RefreshReason::kReset) {
    // If the change in the schedule or environment introduces a change in the
    // status, then calling `SetEnabledInternal()` is all we need, since it will
    // trigger a change in the user prefs to which we will respond by calling
    // Refresh(). This will end up in this function again and enter the case
    // above, adjusting all the needed schedules.
    SetEnabledInternal(enable_now, reason);
    return;
  } else {
    // Either of these is true:
    // 1) The user manually toggled the feature status to the opposite of what
    //    the schedule says.
    // 2) Sunrise tomorrow is later in the day than sunrise today. For example:
    // * Sunrise Today: 6:00 AM
    // * Now/Sunset Today: 6:00 PM
    // * Calculated sunrise tomorrow: 6:00 AM + 1 day.
    // * Actual Sunrise Tomorrow: 6:01 AM
    // * At 6:00 AM the next day, feature is disabled. `RefreshScheduleTimer()`
    //   uses the new sunrise time of 6:01 AM. The feature's currently disabled
    //   even though today's sunrise/sunset times say it should be enabled. This
    //   effectively acts as a manual toggle.
    //
    // Maintain the current enabled status and keep scheduling refresh
    // operations until the enabled status matches the schedule again. When that
    // happens, the first case in this branch will be hit and normal scheduling
    // logic should resume thereafter.
    next_feature_status = current_enabled;
    time_until_next_refresh = schedule_position.time_until_next_checkpoint;
    new_checkpoint =
        GetCheckpointForEnabledState(current_enabled, schedule_type);
  }

  ScheduleNextRefresh(
      {now + time_until_next_refresh, next_feature_status, new_checkpoint},
      now);
  RefreshFeatureState(reason);
  // Should be called after `ScheduleNextRefresh` and `RefreshFeatureState()`
  // so that all of the feature's internal bookkeeping has been updated before
  // broadcasting to users that a new feature state has been reached. This
  // ensures that the feature is in a stable internal state in case a
  // `CheckpointObserver` tries to use the feature immediately within its
  // observer method.
  SetCurrentCheckpoint(new_checkpoint);
}

void ScheduledFeature::ScheduleNextRefresh(
    const ScheduleSnapshot& current_snapshot,
    base::Time now) {
  DCHECK(active_user_pref_service_);
  const base::TimeDelta delay = current_snapshot.target_time - now;
  DCHECK_GE(delay, base::TimeDelta());
  refresh_failure_backoff_.Reset();
  per_user_schedule_snapshot_[active_user_pref_service_] = current_snapshot;
  base::OnceClosure timer_cb;
  if (current_snapshot.target_status == GetEnabled()) {
    timer_cb = base::BindOnce(&ScheduledFeature::Refresh,
                              base::Unretained(this), RefreshReason::kScheduled,
                              /*keep_manual_toggles_during_schedules=*/false);
  } else {
    timer_cb = base::BindOnce(
        &ScheduledFeature::SetEnabledInternal, base::Unretained(this),
        current_snapshot.target_status, RefreshReason::kScheduled);
  }
  VLOG(1) << "Setting " << GetFeatureName() << " to refresh to "
          << (current_snapshot.target_status ? "enabled" : "disabled") << " at "
          << current_snapshot.target_time << " in " << delay << " now= " << now;
  timer_->Start(FROM_HERE, delay, std::move(timer_cb));
}

void ScheduledFeature::ScheduleNextRefreshRetry(
    bool keep_manual_toggles_during_schedules) {
  refresh_failure_backoff_.InformOfRequest(/*succeeded=*/false);
  const base::TimeDelta retry_delay =
      refresh_failure_backoff_.GetTimeUntilRelease();
  LOG(ERROR) << "Refresh() failed. Scheduling retry in " << retry_delay;
  // The refresh failure puts the schedule in an inaccurate state (the
  // feature can be the opposite of what the schedule says it should be).
  // 'RefreshReason::kReset` is appropriate and necessary to return it
  // to the correct state the next time `Refresh()` can succeed.
  timer_->Start(FROM_HERE, retry_delay,
                base::BindOnce(&ScheduledFeature::Refresh,
                               base::Unretained(this), RefreshReason::kReset,
                               keep_manual_toggles_during_schedules));
}

void ScheduledFeature::SetCurrentCheckpoint(ScheduleCheckpoint new_checkpoint) {
  if (new_checkpoint == current_checkpoint_) {
    return;
  }

  DVLOG(1) << "Setting " << GetFeatureName() << " ScheduleCheckpoint from "
           << current_checkpoint_ << " to " << new_checkpoint << " at "
           << Now();
  current_checkpoint_ = new_checkpoint;
  for (CheckpointObserver& obs : checkpoint_observers_) {
    obs.OnCheckpointChanged(this, current_checkpoint_);
  }
}

}  // namespace ash
