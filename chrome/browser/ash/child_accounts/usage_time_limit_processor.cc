// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ash/child_accounts/usage_time_limit_processor.h"

#include <optional>
#include <string>
#include <utility>

#include "base/check_deref.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/ash/child_accounts/time_limit_override.h"

namespace ash::usage_time_limit {
namespace internal {
namespace {

constexpr char kTimeLimitLastUpdatedAt[] = "last_updated_millis";
constexpr char kTimeWindowLimit[] = "time_window_limit";
constexpr char kTimeUsageLimit[] = "time_usage_limit";
constexpr char kUsageLimitResetAt[] = "reset_at";
constexpr char kUsageLimitUsageQuota[] = "usage_quota_mins";
constexpr char kWindowLimitEntries[] = "entries";
constexpr char kWindowLimitEntryEffectiveDay[] = "effective_day";
constexpr char kWindowLimitEntryEndsAt[] = "ends_at";
constexpr char kWindowLimitEntryStartsAt[] = "starts_at";
constexpr char kWindowLimitEntryTimeHour[] = "hour";
constexpr char kWindowLimitEntryTimeMinute[] = "minute";
constexpr const char* kTimeLimitWeekdays[] = {
    "sunday",   "monday", "tuesday", "wednesday",
    "thursday", "friday", "saturday"};

// Defaults to midnight.
constexpr base::TimeDelta kDefaultUsageLimitResetTime;

// Whether a timestamp is inside a window.
bool ContainsTime(base::Time start, base::Time end, base::Time now) {
  return now >= start && now < end;
}

// Returns true when a < b. When b is null, this returns true.
bool IsBefore(base::Time a, base::Time b) {
  return b.is_null() || a < b;
}

// Implements the modulo operation. E.g. modulo(10, 7) == 3, modulo(-1, 7) == 6.
int Modulo(int dividend, int divisor) {
  int remainder = dividend % divisor;
  if (remainder < 0)
    remainder += divisor;
  return remainder;
}

// Shifts the current weekday, if the value is positive shifts forward and if
// negative backwards.
Weekday WeekdayShift(Weekday current_day, int shift) {
  return static_cast<Weekday>(Modulo(static_cast<int>(current_day) + shift,
                                     static_cast<int>(Weekday::kCount)));
}

// Returns usage limit reset time or default value if |time_usage_limit| is
// invalid.
base::TimeDelta GetUsageLimitResetTime(
    const std::optional<internal::TimeUsageLimit>& time_usage_limit) {
  if (time_usage_limit)
    return time_usage_limit->resets_at;
  return kDefaultUsageLimitResetTime;
}

// Helper class to process the UsageTimeLimit policy.
class UsageTimeLimitProcessor {
 public:
  UsageTimeLimitProcessor(
      std::optional<internal::TimeWindowLimit> time_window_limit,
      std::optional<internal::TimeUsageLimit> time_usage_limit,
      std::optional<TimeLimitOverride> time_limit_override,
      std::optional<TimeLimitOverride> local_time_limit_override,
      const base::TimeDelta& used_time,
      const base::Time& usage_timestamp,
      const base::Time& current_time,
      const icu::TimeZone* const time_zone,
      const std::optional<State>& previous_state);

  ~UsageTimeLimitProcessor() = default;

  // Current user's session state.
  State GetState();

  // Expected time when the user's usage quota should be reset.
  base::Time GetExpectedResetTime();

  // Difference between today user's usage quota and usage time.
  std::optional<base::TimeDelta> GetRemainingTimeUsage();

 private:
  // Get the active time window limit.
  std::optional<internal::TimeWindowLimitEntry> GetActiveTimeWindowLimit();

  // Get the active time usage limit.
  std::optional<internal::TimeUsageLimitEntry> GetActiveTimeUsageLimit();

  // Get the enabled time usage limit.
  std::optional<internal::TimeUsageLimitEntry> GetEnabledTimeUsageLimit();

  // Returns the duration of all the consuctive time window limit starting at
  // the given weekday.
  base::TimeDelta GetConsecutiveTimeWindowLimitDuration(
      internal::Weekday weekday);

  // Whether there's an override whose duration has finished.
  bool IsOverrideDurationFinished();

  // Whether the device should be locked by an override.
  bool ShouldBeLockedByOverride();

  // Whether there is a valid override. It includes lock, unlock or unlock with
  // duration override.
  bool HasActiveOverride();

  // Whether there's an active override with duration.
  bool HasActiveOverrideWithDuration();

  // Whether the user's session should be locked.
  bool IsLocked();

  // Which policy is currently active.
  PolicyType GetActivePolicy();

  // Gets the time when the active time limit will end.
  base::Time GetActiveTimeLimitEndTime();

  // Gets the next time when usage time limit will reset.
  base::Time GetNextUsageLimitResetTime();

  // Gets the time when the current override will end. If there's no override,
  // returns base::Time().
  base::Time GetCurrentOverrideEndTime();

  // Gets the time when the lock override will end.
  base::Time GetLockOverrideEndTime();

  // Next time when the user session will be unlocked.
  base::Time GetNextUnlockTime();

  // Expected time when the state will change.
  base::Time GetNextStateChangeTime(PolicyType* out_next_active);

  // Whether the time window limit defined in the given weekday is overridden.
  bool IsWindowLimitOverridden(internal::Weekday weekday);

  // Whether the time usage limit defined in the given weekday is overridden.
  bool IsUsageLimitOverridden(internal::Weekday weekday);

  // Whether the current override was canceled by the window limit update in the
  // given weekday.
  bool WasOverrideCanceledByWindowLimit(internal::Weekday weekday);

  // Whether the current override was canceled by the usage time limit update in
  // the given weekday.
  bool WasOverrideCanceledByUsageTimeLimit(internal::Weekday weekday);

  // When the lock override should reset.
  base::TimeDelta LockOverrideResetTime();

  // When the usage limit should reset the usage quota.
  base::TimeDelta UsageLimitResetTime();

  // Checks if the time window limit entry for the current weekday is active.
  bool IsTodayTimeWindowLimitActive();

  // Local midnight.
  base::Time LocalMidnight(base::Time time);

  // Get the current weekday.
  Weekday GetCurrentWeekday();

  // Get the time zone offset. Used to convert GMT time to local time.
  base::TimeDelta GetTimeZoneOffset(base::Time time);

  // Converts the policy time, which is a delta from midnight, to a timestamp.
  // Since this is done based on the current time, a shift in days param is
  // available.
  base::Time ConvertPolicyTime(base::TimeDelta policy_time, int shift_in_days);

  // The policy time window limit object.
  std::optional<internal::TimeWindowLimit> time_window_limit_;

  // The policy time usage limit object.
  std::optional<internal::TimeUsageLimit> time_usage_limit_;

  // The policy override object.
  std::optional<TimeLimitOverride> time_limit_override_;

  // The local override object.
  std::optional<TimeLimitOverride> local_time_limit_override_;

  // How long the user has used the device.
  const base::TimeDelta used_time_;

  // When the used_time_ data was collected.
  const base::Time usage_timestamp_;

  // The current time, not necessarily equal to usage_timestamp_.
  const base::Time current_time_;

  // Unowned. The device's timezone.
  const raw_ptr<const icu::TimeZone> time_zone_;

  // Current weekday, extracted from current time.
  internal::Weekday current_weekday_;

  // The previous state calculated by this class.
  const raw_ref<const std::optional<State>> previous_state_;

  // The active time window limit. If this is set, it means that the user
  // session should be locked, in other words, there is a time window limit set
  // for the current day, the current time is inside that window and no unlock
  // override is preventing it to be locked.
  std::optional<internal::TimeWindowLimitEntry> active_time_window_limit_;

  // The active time usage limit. If this is set, it means that the user session
  // should be locked, in other words, there is a time usage limit set for the
  // current day, the user has used all their usage quota and no unlock override
  // is preventing it to be locked.
  std::optional<internal::TimeUsageLimitEntry> active_time_usage_limit_;

  // If this is set, it means that there is a time usage limit set for today,
  // but it is not necessarily active. It could be inactive either because the
  // user haven't used all their quota or because there is an unlock override
  // active.
  std::optional<internal::TimeUsageLimitEntry> enabled_time_usage_limit_;

  // Whether there is a window limit overridden.
  bool overridden_window_limit_ = false;

  // Whether there is a usage limit overridden.
  bool overridden_usage_limit_ = false;
};

UsageTimeLimitProcessor::UsageTimeLimitProcessor(
    std::optional<internal::TimeWindowLimit> time_window_limit,
    std::optional<internal::TimeUsageLimit> time_usage_limit,
    std::optional<TimeLimitOverride> time_limit_override,
    std::optional<TimeLimitOverride> local_time_limit_override,
    const base::TimeDelta& used_time,
    const base::Time& usage_timestamp,
    const base::Time& current_time,
    const icu::TimeZone* const time_zone,
    const std::optional<State>& previous_state)
    : time_window_limit_(std::move(time_window_limit)),
      time_usage_limit_(std::move(time_usage_limit)),
      used_time_(used_time),
      usage_timestamp_(usage_timestamp),
      current_time_(current_time),
      time_zone_(time_zone),
      current_weekday_(GetCurrentWeekday()),
      previous_state_(previous_state),
      enabled_time_usage_limit_(GetEnabledTimeUsageLimit()) {
  // Use local override if it is newer than policy override, otherwise ignore
  // local override.
  // Note: |time_limit_override_| needs to be set before calculating
  // |active_time_window_limit_| and |active_time_usage_limit_|.
  bool should_use_local_override = local_time_limit_override.has_value() &&
                                   (!time_limit_override.has_value() ||
                                    local_time_limit_override->created_at() >
                                        time_limit_override->created_at());
  time_limit_override_ = should_use_local_override
                             ? std::move(local_time_limit_override)
                             : std::move(time_limit_override);

  // This will also set overridden_window_limit_ to true if applicable.
  // TODO: refactor GetActiveTimeWindowLimit to stop updating the state on a
  // getter method.
  active_time_window_limit_ = GetActiveTimeWindowLimit();
  // This will also sets overridden_usage_limit_ to true if applicable.
  // TODO: refactor GetActiveTimeUsageLimit to stop updating the state on a
  // getter method.
  active_time_usage_limit_ = GetActiveTimeUsageLimit();
}

base::Time UsageTimeLimitProcessor::GetExpectedResetTime() {
  base::TimeDelta delta_from_midnight =
      current_time_ - LocalMidnight(current_time_);
  int shift_in_days = 1;
  if (delta_from_midnight < UsageLimitResetTime())
    shift_in_days = 0;
  return ConvertPolicyTime(UsageLimitResetTime(), shift_in_days);
}

std::optional<base::TimeDelta>
UsageTimeLimitProcessor::GetRemainingTimeUsage() {
  if (!enabled_time_usage_limit_)
    return std::nullopt;
  return std::max(enabled_time_usage_limit_->usage_quota - used_time_,
                  base::Minutes(0));
}

State UsageTimeLimitProcessor::GetState() {
  State state;
  state.is_locked = IsLocked();
  state.active_policy = GetActivePolicy();

  // Time usage limit is enabled if there is an entry for the current day and it
  // is not overridden.
  std::optional<base::TimeDelta> remaining_usage = GetRemainingTimeUsage();
  if (remaining_usage) {
    state.is_time_usage_limit_enabled = true;
    state.remaining_usage = remaining_usage.value();
  }

  const base::TimeDelta delta_zero = base::Minutes(0);
  bool current_state_above_usage_limit =
      state.is_time_usage_limit_enabled && state.remaining_usage <= delta_zero;
  bool previous_state_below_usage_limit =
      previous_state_->has_value() &&
      (*previous_state_)->is_time_usage_limit_enabled &&
      (*previous_state_)->remaining_usage > delta_zero;
  bool previous_state_no_usage_limit =
      previous_state_->has_value() &&
      !(*previous_state_)->is_time_usage_limit_enabled;
  bool previous_state_above_usage_limit =
      previous_state_->has_value() &&
      (*previous_state_)->is_time_usage_limit_enabled &&
      (*previous_state_)->remaining_usage <= delta_zero;
  if ((previous_state_below_usage_limit || previous_state_no_usage_limit ||
       !previous_state_->has_value()) &&
      current_state_above_usage_limit) {
    // Time usage limit just started being enforced.
    state.time_usage_limit_started = usage_timestamp_;
  } else if (previous_state_above_usage_limit) {
    // Time usage limit was already enforced.
    state.time_usage_limit_started =
        (*previous_state_)->time_usage_limit_started;
  }

  state.next_state_change_time =
      GetNextStateChangeTime(&state.next_state_active_policy);

  state.next_unlock_time = GetNextUnlockTime();

  return state;
}

base::TimeDelta UsageTimeLimitProcessor::GetConsecutiveTimeWindowLimitDuration(
    internal::Weekday weekday) {
  base::TimeDelta duration = base::Minutes(0);
  std::optional<internal::TimeWindowLimitEntry> current_day_entry =
      time_window_limit_->entries[weekday];

  if (!time_window_limit_ || !current_day_entry)
    return duration;

  // Iterate throught entries as long as they are consecutive, or overlap.
  base::TimeDelta last_entry_end = current_day_entry->starts_at;
  for (int i = 0; i < static_cast<int>(internal::Weekday::kCount); i++) {
    std::optional<internal::TimeWindowLimitEntry> window_limit_entry =
        time_window_limit_->entries[internal::WeekdayShift(weekday, i)];

    // It is not consecutive.
    if (!window_limit_entry || window_limit_entry->starts_at > last_entry_end)
      break;

    if (window_limit_entry->IsOvernight()) {
      duration += base::TimeDelta(base::Hours(24) - last_entry_end) +
                  base::TimeDelta(window_limit_entry->ends_at);
    } else {
      duration += std::max(window_limit_entry->ends_at - last_entry_end,
                           base::Minutes(0));
      // This entry is not overnight, so the next one cannot be a consecutive
      // window.
      break;
    }
    last_entry_end = window_limit_entry->ends_at;
  }

  return duration;
}

bool UsageTimeLimitProcessor::IsWindowLimitOverridden(
    internal::Weekday weekday) {
  if (!time_window_limit_ || !time_limit_override_ ||
      time_limit_override_->IsLock()) {
    return false;
  }

  // If there's an override with duration, the window limit is overridden only
  // if the override is active and duration is not over, since it works
  // as a lock override after duration.
  if (time_limit_override_->duration())
    return HasActiveOverrideWithDuration() && !IsOverrideDurationFinished();

  if (WasOverrideCanceledByWindowLimit(weekday))
    return false;

  std::optional<internal::TimeWindowLimitEntry> window_limit_entry =
      time_window_limit_->entries[weekday];

  int days_behind = 0;
  for (int i = 0; i < static_cast<int>(internal::Weekday::kCount); i++) {
    if (internal::WeekdayShift(weekday, i) == current_weekday_) {
      days_behind = i;
      break;
    }
  }

  base::Time window_limit_start =
      ConvertPolicyTime(window_limit_entry->starts_at, -days_behind);
  base::Time window_limit_end =
      window_limit_start + GetConsecutiveTimeWindowLimitDuration(weekday);

  return ContainsTime(window_limit_start, window_limit_end,
                      time_limit_override_->created_at());
}

bool UsageTimeLimitProcessor::IsUsageLimitOverridden(
    internal::Weekday weekday) {
  if (!time_limit_override_ || time_limit_override_->IsLock()) {
    return false;
  }

  if (!time_usage_limit_ || !previous_state_->has_value()) {
    return false;
  }

  // If there's an override with duration, the usage limit is overridden only
  // if the override is active and duration is not over, since it works
  // as a lock override after duration.
  if (time_limit_override_->duration())
    return HasActiveOverrideWithDuration() && !IsOverrideDurationFinished();

  if (WasOverrideCanceledByUsageTimeLimit(weekday))
    return false;

  base::Time last_reset_time = ConvertPolicyTime(LockOverrideResetTime(), 0);
  bool usage_limit_enforced_previously =
      (*previous_state_)->is_time_usage_limit_enabled &&
      (*previous_state_)->remaining_usage <= base::Minutes(0);
  bool override_created_after_usage_limit_start =
      !(*previous_state_)->time_usage_limit_started.is_null() &&
      time_limit_override_->created_at() >
          (*previous_state_)->time_usage_limit_started &&
      time_limit_override_->created_at() >= last_reset_time;
  return usage_limit_enforced_previously &&
         override_created_after_usage_limit_start;
}

bool UsageTimeLimitProcessor::WasOverrideCanceledByUsageTimeLimit(
    internal::Weekday weekday) {
  if (!time_usage_limit_ || !time_limit_override_)
    return false;

  std::optional<internal::TimeUsageLimitEntry> usage_limit_entry =
      time_usage_limit_->entries[weekday];

  // If the time usage limit has been updated since the override, the
  // override is cancelled.
  return usage_limit_entry &&
         usage_limit_entry->last_updated > time_limit_override_->created_at();
}

bool UsageTimeLimitProcessor::WasOverrideCanceledByWindowLimit(
    internal::Weekday weekday) {
  if (!time_window_limit_ || !time_limit_override_)
    return false;

  std::optional<TimeWindowLimitEntry> window_limit =
      time_window_limit_->entries[weekday];

  // If the window limit has been updated since the override, the
  // override is cancelled.
  if (window_limit &&
      window_limit->last_updated > time_limit_override_->created_at())
    return true;

  return false;
}

bool UsageTimeLimitProcessor::HasActiveOverrideWithDuration() {
  if (!time_limit_override_ || time_limit_override_->IsLock() ||
      !time_limit_override_->duration()) {
    return false;
  }

  internal::Weekday current_usage_limit_day =
      current_time_ > ConvertPolicyTime(UsageLimitResetTime(), 0)
          ? current_weekday_
          : internal::WeekdayShift(current_weekday_, -1);

  if (current_time_ >= GetCurrentOverrideEndTime() ||
      WasOverrideCanceledByUsageTimeLimit(current_usage_limit_day))
    return false;

  if (!time_window_limit_)
    return true;

  internal::Weekday previous_weekday =
      internal::WeekdayShift(current_weekday_, -1);
  std::optional<internal::TimeWindowLimitEntry> previous_day_entry =
      time_window_limit_->entries[previous_weekday];

  // Active time window limit that started on the previous day.
  if (previous_day_entry && previous_day_entry->IsOvernight() &&
      WasOverrideCanceledByWindowLimit(previous_weekday)) {
    return false;
  }

  return !WasOverrideCanceledByWindowLimit(current_weekday_);
}

std::optional<internal::TimeWindowLimitEntry>
UsageTimeLimitProcessor::GetActiveTimeWindowLimit() {
  if (!time_window_limit_)
    return std::nullopt;

  internal::Weekday previous_weekday =
      internal::WeekdayShift(current_weekday_, -1);
  std::optional<internal::TimeWindowLimitEntry> previous_day_entry =
      time_window_limit_->entries[previous_weekday];

  // Active time window limit that started on the previous day.
  std::optional<internal::TimeWindowLimitEntry> previous_day_active_entry;
  if (previous_day_entry && previous_day_entry->IsOvernight()) {
    base::Time limit_start =
        ConvertPolicyTime(previous_day_entry->starts_at, -1);
    base::Time limit_end = ConvertPolicyTime(previous_day_entry->ends_at, 0);

    if (ContainsTime(limit_start, limit_end, current_time_)) {
      if (IsWindowLimitOverridden(previous_weekday)) {
        overridden_window_limit_ = true;
      } else {
        previous_day_active_entry = previous_day_entry;
      }
    }
  }

  std::optional<internal::TimeWindowLimitEntry> current_day_entry =
      time_window_limit_->entries[current_weekday_];

  // Active time window limit that started today.
  std::optional<internal::TimeWindowLimitEntry> current_day_active_entry;
  if (current_day_entry) {
    base::Time limit_start = ConvertPolicyTime(current_day_entry->starts_at, 0);
    base::Time limit_end = ConvertPolicyTime(
        current_day_entry->ends_at, current_day_entry->IsOvernight() ? 1 : 0);

    if (ContainsTime(limit_start, limit_end, current_time_)) {
      if (IsWindowLimitOverridden(current_weekday_)) {
        overridden_window_limit_ = true;
      } else {
        current_day_active_entry = current_day_entry;
      }
    }
  }

  if (current_day_active_entry && previous_day_active_entry) {
    // If two windows overlap and are active now we must return the one that
    // ends later.
    if (current_day_active_entry->IsOvernight() ||
        current_day_active_entry->ends_at >
            previous_day_active_entry->ends_at) {
      return current_day_active_entry;
    }
    return previous_day_active_entry;
  }

  if (current_day_active_entry)
    return current_day_active_entry;

  return previous_day_active_entry;
}

std::optional<internal::TimeUsageLimitEntry>
UsageTimeLimitProcessor::GetEnabledTimeUsageLimit() {
  if (!time_usage_limit_)
    return std::nullopt;

  internal::Weekday current_usage_limit_day =
      current_time_ >= ConvertPolicyTime(UsageLimitResetTime(), 0)
          ? current_weekday_
          : internal::WeekdayShift(current_weekday_, -1);
  return time_usage_limit_->entries[current_usage_limit_day];
}

std::optional<internal::TimeUsageLimitEntry>
UsageTimeLimitProcessor::GetActiveTimeUsageLimit() {
  if (!time_usage_limit_)
    return std::nullopt;

  internal::Weekday current_usage_limit_day =
      current_time_ > ConvertPolicyTime(UsageLimitResetTime(), 0)
          ? current_weekday_
          : internal::WeekdayShift(current_weekday_, -1);

  std::optional<internal::TimeUsageLimitEntry> current_usage_limit =
      GetEnabledTimeUsageLimit();

  if (IsUsageLimitOverridden(current_usage_limit_day)) {
    overridden_usage_limit_ = true;
    return std::nullopt;
  }

  if (current_usage_limit && used_time_ >= current_usage_limit->usage_quota)
    return current_usage_limit;

  return std::nullopt;
}

bool UsageTimeLimitProcessor::IsOverrideDurationFinished() {
  if (!time_limit_override_ || time_limit_override_->IsLock() ||
      !time_limit_override_->duration())
    return false;

  base::Time lock_time = time_limit_override_->created_at() +
                         time_limit_override_->duration().value();
  if (ContainsTime(time_limit_override_->created_at(), lock_time,
                   current_time_))
    return false;

  return true;
}

bool UsageTimeLimitProcessor::ShouldBeLockedByOverride() {
  return (HasActiveOverride() && time_limit_override_->IsLock()) ||
         (HasActiveOverrideWithDuration() && IsOverrideDurationFinished());
}

bool UsageTimeLimitProcessor::HasActiveOverride() {
  if (!time_limit_override_ || active_time_window_limit_ ||
      active_time_usage_limit_) {
    return false;
  }

  if (overridden_window_limit_ || overridden_usage_limit_)
    return true;

  if (time_limit_override_->duration())
    return HasActiveOverrideWithDuration();

  base::Time last_reset_time = ConvertPolicyTime(LockOverrideResetTime(), 0);
  if (current_time_ < last_reset_time)
    last_reset_time -= base::Days(1);

  bool override_cancelled_by_window_limit = false;
  if (time_window_limit_) {
    // Check if yesterdays or todays window limit ended after override was
    // created.
    for (int i = -1; i <= 0; i++) {
      internal::Weekday weekday = WeekdayShift(current_weekday_, i);
      std::optional<TimeWindowLimitEntry> window_limit =
          time_window_limit_->entries[weekday];
      if (window_limit) {
        base::Time window_limit_start =
            ConvertPolicyTime(window_limit->starts_at, i);
        base::Time window_limit_end =
            window_limit_start + GetConsecutiveTimeWindowLimitDuration(weekday);
        if (current_time_ >= window_limit_end &&
            window_limit_end > time_limit_override_->created_at()) {
          override_cancelled_by_window_limit = true;
          break;
        }
      }
    }
  }

  bool has_valid_lock_override =
      time_limit_override_->created_at() > last_reset_time &&
      !override_cancelled_by_window_limit;

  if (!has_valid_lock_override)
    return false;

  // Check if the usage time was increased before the override creation, which
  // invalidates it.
  if (previous_state_->has_value() &&
      (*previous_state_)->is_time_usage_limit_enabled &&
      (*previous_state_)->remaining_usage <= base::Minutes(0)) {
    if (enabled_time_usage_limit_ &&
        time_limit_override_->created_at() <
            enabled_time_usage_limit_->last_updated) {
      return false;
    }
  }

  return true;
}

bool UsageTimeLimitProcessor::IsLocked() {
  return active_time_usage_limit_ || active_time_window_limit_ ||
         ShouldBeLockedByOverride();
}

PolicyType UsageTimeLimitProcessor::GetActivePolicy() {
  // If there's an active override with duration, the active policy is always
  // override.
  if (HasActiveOverrideWithDuration())
    return PolicyType::kOverride;

  if (active_time_window_limit_)
    return PolicyType::kFixedLimit;

  if (active_time_usage_limit_)
    return PolicyType::kUsageLimit;

  if (HasActiveOverride())
    return PolicyType::kOverride;

  return PolicyType::kNoPolicy;
}

base::Time UsageTimeLimitProcessor::GetActiveTimeLimitEndTime() {
  if (!active_time_window_limit_)
    return base::Time();

  base::TimeDelta window_limit_duration =
      IsTodayTimeWindowLimitActive()
          ? GetConsecutiveTimeWindowLimitDuration(current_weekday_)
          : GetConsecutiveTimeWindowLimitDuration(
                internal::WeekdayShift(current_weekday_, -1));
  return ConvertPolicyTime(active_time_window_limit_->starts_at,
                           IsTodayTimeWindowLimitActive() ? 0 : -1) +
         window_limit_duration;
}

base::Time UsageTimeLimitProcessor::GetNextUsageLimitResetTime() {
  bool has_reset_today =
      (current_time_ - LocalMidnight(current_time_)) >= UsageLimitResetTime();
  return ConvertPolicyTime(UsageLimitResetTime(), has_reset_today ? 1 : 0);
}

base::Time UsageTimeLimitProcessor::GetCurrentOverrideEndTime() {
  if (!time_limit_override_)
    return base::Time();

  base::Time reset_time = LocalMidnight(time_limit_override_->created_at()) +
                          LockOverrideResetTime();

  if (IsBefore(reset_time, time_limit_override_->created_at()))
    reset_time = reset_time + base::Days(1);

  return reset_time;
}

base::Time UsageTimeLimitProcessor::GetLockOverrideEndTime() {
  if (!ShouldBeLockedByOverride()) {
    return base::Time();
  }
  return GetCurrentOverrideEndTime();
}

base::Time UsageTimeLimitProcessor::GetNextUnlockTime() {
  if (!IsLocked())
    return base::Time();

  base::Time unlock_time;

  // When the current active time window limit ends.
  if (active_time_window_limit_)
    unlock_time = std::max(unlock_time, GetActiveTimeLimitEndTime());

  // When the usage quota resets.
  if (active_time_usage_limit_) {
    base::Time next_usage_limit_reset = GetNextUsageLimitResetTime();
    unlock_time = std::max(unlock_time, next_usage_limit_reset);
    // The usage limit could reset when a window limit is active, we must check
    // that, and if this is the case calculate the end of the window limit.
    if (time_window_limit_) {
      // Check if yesterdays, todays or tomorrows window limit will be active
      // when the reset happens.
      for (int i = -1; i <= 1; i++) {
        std::optional<TimeWindowLimitEntry> window_limit =
            time_window_limit_->entries[WeekdayShift(current_weekday_, i)];
        if (window_limit) {
          TimeWindowLimitBoundaries limits = window_limit->GetLimits(
              LocalMidnight(current_time_) + base::Days(i));
          // Ignores time window limit if it is overridden.
          if (overridden_window_limit_ &&
              ContainsTime(limits.starts, limits.ends, current_time_)) {
            continue;
          }
          if (ContainsTime(limits.starts, limits.ends, next_usage_limit_reset))
            unlock_time = std::max(unlock_time, limits.ends);
        }
      }
    }
  }

  // When a lock override will become inactive.
  if (ShouldBeLockedByOverride()) {
    // The lock override ends either on the next reset time or when a bedtime
    // ends.
    base::Time lock_override_ends;
    if (time_window_limit_ && !IsOverrideDurationFinished()) {
      // Search a time window limit that could start before the lock override
      // ends and retrieve its end time. It could be yesterday's, today's or
      // tomorrow's time window limit.
      for (int i = -1; i <= 1; i++) {
        internal::Weekday weekday = WeekdayShift(current_weekday_, i);
        std::optional<TimeWindowLimitEntry> window_limit =
            time_window_limit_->entries[weekday];
        if (window_limit) {
          base::Time window_limit_start =
              ConvertPolicyTime(window_limit->starts_at, i);

          // Window limit starts after the end of override.
          if (window_limit_start > GetLockOverrideEndTime())
            continue;

          base::Time window_limit_end =
              window_limit_start +
              GetConsecutiveTimeWindowLimitDuration(weekday);

          if (window_limit_end > time_limit_override_->created_at() &&
              IsBefore(window_limit_end, lock_override_ends)) {
            lock_override_ends = window_limit_end;
          }
        }
      }
    }
    // Set override end to default reset time when:
    // 1. No window limit starts before the reset time;
    // 2. Window limit starts and ends before reset time and there is an unlock
    // with duration active. Unlock with duration must lock device at least
    // until the reset time.
    if (lock_override_ends.is_null() || IsOverrideDurationFinished()) {
      lock_override_ends =
          std::max(lock_override_ends, GetLockOverrideEndTime());
    }
    unlock_time = std::max(unlock_time, lock_override_ends);
  }

  return unlock_time;
}

base::Time UsageTimeLimitProcessor::GetNextStateChangeTime(
    PolicyType* out_next_active) {
  base::Time next_change;
  *out_next_active = PolicyType::kNoPolicy;

  base::Time active_time_window_limit_ends = GetActiveTimeLimitEndTime();
  base::Time next_usage_quota_reset = GetNextUsageLimitResetTime();

  // Check when next time window limit starts.
  if (time_window_limit_ && !active_time_window_limit_) {
    internal::Weekday start_day = internal::WeekdayShift(current_weekday_, 1);
    base::TimeDelta delta_from_midnight =
        current_time_ - LocalMidnight(current_time_);
    bool todays_time_limit_not_started =
        time_window_limit_->entries[current_weekday_] &&
        time_window_limit_->entries[current_weekday_]->starts_at >
            delta_from_midnight;
    // If today's time limit has not started yet, start search today.
    if (todays_time_limit_not_started)
      start_day = current_weekday_;

    // Search a time window limit in the next following days.
    for (int i = 0; i < static_cast<int>(internal::Weekday::kCount); i++) {
      std::optional<internal::TimeWindowLimitEntry> entry =
          time_window_limit_.value()
              .entries[internal::WeekdayShift(start_day, i)];
      if (entry) {
        int shift = start_day == current_weekday_ ? 0 : 1;
        base::Time start_time = ConvertPolicyTime(entry->starts_at, i + shift);
        if (IsBefore(start_time, next_change)) {
          next_change = start_time;
          *out_next_active = PolicyType::kFixedLimit;
        }
        break;
      }
    }
  }

  // Minimum time when the current time usage quota could end. Not calculated
  // when time usage limit has already finished.
  if (time_usage_limit_ && !active_time_usage_limit_ &&
      !overridden_usage_limit_ && !active_time_window_limit_) {
    // If there is an active time usage, we just look when it would lock the
    // session if the user don't stop using it.
    if (enabled_time_usage_limit_) {
      base::Time quota_ends =
          current_time_ + (enabled_time_usage_limit_->usage_quota - used_time_);
      if (IsBefore(quota_ends, next_change)) {
        next_change = quota_ends;
        *out_next_active = PolicyType::kUsageLimit;
      }
    }
  }

  // Look for the next time usage, and calculate the minimum time when it could
  // end.
  if (time_usage_limit_) {
    for (int i = 1; i < static_cast<int>(internal::Weekday::kCount); i++) {
      std::optional<internal::TimeUsageLimitEntry> usage_limit_entry =
          time_usage_limit_
              ->entries[internal::WeekdayShift(current_weekday_, i)];
      if (usage_limit_entry) {
        base::Time quota_ends = ConvertPolicyTime(UsageLimitResetTime(), i) +
                                usage_limit_entry->usage_quota;
        if (IsBefore(quota_ends, next_change)) {
          next_change = quota_ends;
          *out_next_active = PolicyType::kUsageLimit;
        }
        break;
      }
    }
  }

  // When the current active time window limit ends.
  if (active_time_window_limit_) {
    if (IsBefore(active_time_window_limit_ends, next_change)) {
      next_change = active_time_window_limit_ends;
      if (active_time_usage_limit_ &&
          used_time_ >= active_time_usage_limit_->usage_quota &&
          active_time_window_limit_ends < next_usage_quota_reset) {
        *out_next_active = PolicyType::kUsageLimit;
      } else {
        *out_next_active = PolicyType::kNoPolicy;
      }
    }
  }

  // When the usage quota resets. Only calculated if there is an enforced time
  // usage limit, and when it ends no other policy would be active.
  if (active_time_usage_limit_ &&
      (!active_time_window_limit_ ||
       active_time_window_limit_->ends_at < UsageLimitResetTime())) {
    if (IsBefore(next_usage_quota_reset, next_change)) {
      next_change = next_usage_quota_reset;
      *out_next_active = PolicyType::kNoPolicy;
    }
  }

  // When a lock override will become inactive. Lock overrides are disabled at
  // the same time as time usage limit resets.
  if (HasActiveOverride() && time_limit_override_->IsLock()) {
    base::Time lock_end = GetLockOverrideEndTime();

    if (IsBefore(lock_end, next_change)) {
      next_change = lock_end;
      if (active_time_window_limit_ &&
          active_time_window_limit_ends > next_usage_quota_reset) {
        *out_next_active = PolicyType::kFixedLimit;
      } else {
        *out_next_active = PolicyType::kNoPolicy;
      }
    }
  }

  // When an override with duration will change the state. It will change either
  // when the duration is over (then the next state will work as a lock
  // override) or at the same time as time usage limit resets.
  if (HasActiveOverrideWithDuration()) {
    base::Time lock_time = time_limit_override_->created_at() +
                           time_limit_override_->duration().value();
    if (!IsOverrideDurationFinished()) {
      next_change = lock_time;
      *out_next_active = PolicyType::kOverride;
    } else {
      next_change = GetLockOverrideEndTime();
      *out_next_active = PolicyType::kNoPolicy;

      if (time_window_limit_) {
        // Check yesterdays, todays or tomorrows window limit, since these can
        // end after the next change time or it can starts at the next change
        // time, if it happens, the next active policy should be fixed limit.
        for (int i = -1; i <= 1; i++) {
          internal::Weekday weekday = WeekdayShift(current_weekday_, i);
          std::optional<TimeWindowLimitEntry> window_limit =
              time_window_limit_->entries[weekday];
          if (window_limit) {
            base::Time window_start =
                ConvertPolicyTime(window_limit->starts_at, i);
            base::Time window_end =
                window_start + GetConsecutiveTimeWindowLimitDuration(weekday);

            if (ContainsTime(window_start, window_end, next_change) ||
                next_change == window_start)
              *out_next_active = PolicyType::kFixedLimit;
          }
        }
      }
    }
  }
  return next_change;
}

bool UsageTimeLimitProcessor::IsTodayTimeWindowLimitActive() {
  if (!time_window_limit_)
    return false;

  std::optional<internal::TimeWindowLimitEntry> yesterday_window_limit =
      time_window_limit_.value()
          .entries[internal::WeekdayShift(current_weekday_, -1)];
  base::TimeDelta delta_from_midnight =
      current_time_ - LocalMidnight(current_time_);

  if ((active_time_window_limit_ || overridden_window_limit_) &&
      (!yesterday_window_limit || !yesterday_window_limit->IsOvernight() ||
       yesterday_window_limit->ends_at < delta_from_midnight)) {
    return true;
  }
  return false;
}

base::TimeDelta UsageTimeLimitProcessor::UsageLimitResetTime() {
  return GetUsageLimitResetTime(time_usage_limit_);
}

base::TimeDelta UsageTimeLimitProcessor::LockOverrideResetTime() {
  // The default behavior is to stop enforcing the lock override at the same
  // time as the time usage limit resets.
  return UsageLimitResetTime();
}

base::Time UsageTimeLimitProcessor::ConvertPolicyTime(
    base::TimeDelta policy_time,
    int shift_in_days) {
  return LocalMidnight(current_time_) + base::Days(shift_in_days) + policy_time;
}

base::Time UsageTimeLimitProcessor::LocalMidnight(base::Time time) {
  base::TimeDelta time_zone_offset = GetTimeZoneOffset(time);
  return (time + time_zone_offset).UTCMidnight() - time_zone_offset;
}

Weekday UsageTimeLimitProcessor::GetCurrentWeekday() {
  base::TimeDelta time_zone_offset = GetTimeZoneOffset(current_time_);
  base::TimeDelta midnight_delta = current_time_ - current_time_.UTCMidnight();
  // Shift in days due to the timezone.
  int time_zone_shift = 0;
  if (midnight_delta + time_zone_offset < base::Hours(0)) {
    time_zone_shift = -1;
  } else if (midnight_delta + time_zone_offset >= base::Hours(24)) {
    time_zone_shift = 1;
  }

  base::Time::Exploded exploded;
  current_time_.UTCExplode(&exploded);
  return WeekdayShift(static_cast<Weekday>(exploded.day_of_week),
                      time_zone_shift);
}

base::TimeDelta UsageTimeLimitProcessor::GetTimeZoneOffset(base::Time time) {
  int32_t raw_offset, dst_offset;
  UErrorCode status = U_ZERO_ERROR;
  time_zone_->getOffset(
      time.InSecondsFSinceUnixEpoch() * base::Time::kMillisecondsPerSecond,
      true /* local */, raw_offset, dst_offset, status);
  base::TimeDelta time_zone_offset =
      base::Milliseconds(raw_offset + dst_offset);
  if (U_FAILURE(status)) {
    LOG(ERROR) << "Failed to get time zone offset, error code: " << status;
    // The fallback case is to get the raw timezone offset ignoring the daylight
    // saving time.
    time_zone_offset = base::Milliseconds(time_zone_->getRawOffset());
  }
  return time_zone_offset;
}

// Transforms the time dictionary sent on the UsageTimeLimit policy to a
// TimeDelta, that represents the distance from midnight.
base::TimeDelta DictToTimeDelta(const base::Value::Dict& policy_time) {
  int hour = policy_time.FindInt(kWindowLimitEntryTimeHour).value();
  int minute = policy_time.FindInt(kWindowLimitEntryTimeMinute).value();
  return base::Minutes(hour * 60 + minute);
}

// Transforms weekday strings into the Weekday enum.
Weekday GetWeekday(std::string weekday) {
  base::ranges::transform(weekday, weekday.begin(), ::tolower);
  for (int i = 0; i < static_cast<int>(Weekday::kCount); i++) {
    if (weekday == kTimeLimitWeekdays[i]) {
      return static_cast<Weekday>(i);
    }
  }

  LOG(ERROR) << "Unexpected weekday " << weekday;
  return Weekday::kSunday;
}

}  // namespace

TimeWindowLimitEntry::TimeWindowLimitEntry() = default;

bool TimeWindowLimitEntry::operator==(const TimeWindowLimitEntry& rhs) const {
  return starts_at == rhs.starts_at && ends_at == rhs.ends_at &&
         last_updated == rhs.last_updated;
}

bool TimeWindowLimitEntry::IsOvernight() const {
  return ends_at < starts_at;
}

TimeWindowLimitBoundaries TimeWindowLimitEntry::GetLimits(
    base::Time start_day_midnight) {
  TimeWindowLimitBoundaries limit;
  limit.starts = start_day_midnight + starts_at;
  limit.ends = start_day_midnight + base::Days(IsOvernight() ? 1 : 0) + ends_at;
  return limit;
}

TimeWindowLimit::TimeWindowLimit(const base::Value& window_limit_val) {
  const base::Value::Dict& window_limit_dict = window_limit_val.GetDict();
  if (!window_limit_dict.contains(kWindowLimitEntries)) {
    return;
  }

  for (const base::Value& entry_val :
       CHECK_DEREF(window_limit_dict.FindList(kWindowLimitEntries))) {
    const base::Value::Dict& entry_dict = entry_val.GetDict();
    const std::string* effective_day =
        entry_dict.FindString(kWindowLimitEntryEffectiveDay);
    const base::Value::Dict* starts_at =
        entry_dict.FindDict(kWindowLimitEntryStartsAt);
    const base::Value::Dict* ends_at =
        entry_dict.FindDict(kWindowLimitEntryEndsAt);
    const std::string* last_updated_value =
        entry_dict.FindString(kTimeLimitLastUpdatedAt);

    if (!effective_day || !starts_at || !ends_at || !last_updated_value) {
      // Missing information, so this entry will be ignored.
      continue;
    }

    int64_t last_updated;
    if (!base::StringToInt64(*last_updated_value, &last_updated)) {
      // Cannot process entry without a valid last updated.
      continue;
    }

    TimeWindowLimitEntry entry;
    entry.starts_at = DictToTimeDelta(*starts_at);
    entry.ends_at = DictToTimeDelta(*ends_at);
    entry.last_updated =
        base::Time::UnixEpoch() + base::Milliseconds(last_updated);

    Weekday weekday = GetWeekday(*effective_day);
    // We only support one time_limit_window per day. If more than one is sent
    // we only use the latest updated.
    if (!entries[weekday] ||
        entries[weekday]->last_updated < entry.last_updated) {
      entries[weekday] = std::move(entry);
    }
  }
}

TimeWindowLimit::~TimeWindowLimit() = default;

TimeWindowLimit::TimeWindowLimit(TimeWindowLimit&&) = default;

TimeWindowLimit& TimeWindowLimit::operator=(TimeWindowLimit&&) = default;

bool TimeWindowLimit::operator==(const TimeWindowLimit& rhs) const {
  return entries == rhs.entries;
}

TimeUsageLimitEntry::TimeUsageLimitEntry() = default;

bool TimeUsageLimitEntry::operator==(const TimeUsageLimitEntry& rhs) const {
  return usage_quota == rhs.usage_quota && last_updated == rhs.last_updated;
}

TimeUsageLimit::TimeUsageLimit(const base::Value::Dict& usage_limit_dict)
    // Default reset time is midnight.
    : resets_at(base::Minutes(0)) {
  const base::Value::Dict* resets_at_value =
      usage_limit_dict.FindDict(kUsageLimitResetAt);
  if (resets_at_value) {
    resets_at = DictToTimeDelta(*resets_at_value);
  }

  for (const std::string& weekday_key : kTimeLimitWeekdays) {
    const base::Value::Dict* entry_dict =
        usage_limit_dict.FindDict(weekday_key);
    if (!entry_dict) {
      continue;
    }

    const std::optional<int> usage_quota =
        entry_dict->FindInt(kUsageLimitUsageQuota);
    const std::string* last_updated_value =
        entry_dict->FindString(kTimeLimitLastUpdatedAt);

    int64_t last_updated;
    if (!base::StringToInt64(CHECK_DEREF(last_updated_value), &last_updated)) {
      // Cannot process entry without a valid last updated.
      continue;
    }

    Weekday weekday = GetWeekday(weekday_key);
    TimeUsageLimitEntry entry;
    entry.usage_quota = base::Minutes(usage_quota.value());
    entry.last_updated =
        base::Time::UnixEpoch() + base::Milliseconds(last_updated);
    entries[weekday] = std::move(entry);
  }
}

TimeUsageLimit::~TimeUsageLimit() = default;

bool TimeUsageLimit::operator==(const TimeUsageLimit& rhs) const {
  return entries == rhs.entries && resets_at == rhs.resets_at;
}

TimeUsageLimit::TimeUsageLimit(TimeUsageLimit&&) = default;

TimeUsageLimit& TimeUsageLimit::operator=(TimeUsageLimit&&) = default;

}  // namespace internal

std::optional<internal::TimeWindowLimit> TimeWindowLimitFromPolicy(
    const base::Value::Dict& time_limit) {
  const base::Value* time_window_limit_value =
      time_limit.Find(internal::kTimeWindowLimit);
  if (!time_window_limit_value)
    return std::nullopt;
  return internal::TimeWindowLimit(*time_window_limit_value);
}

std::optional<internal::TimeUsageLimit> TimeUsageLimitFromPolicy(
    const base::Value::Dict& time_limit) {
  const base::Value* time_usage_limit_value =
      time_limit.Find(internal::kTimeUsageLimit);
  if (!time_usage_limit_value)
    return std::nullopt;
  return internal::TimeUsageLimit(time_usage_limit_value->GetDict());
}

std::optional<TimeLimitOverride> OverrideFromPolicy(
    const base::Value::Dict& time_limit) {
  const base::Value::List* override_value =
      time_limit.FindList(TimeLimitOverride::kOverridesDictKey);
  return TimeLimitOverride::MostRecentFromList(override_value);
}

State GetState(const base::Value::Dict& time_limit,
               const base::Value::Dict* local_override,
               const base::TimeDelta& used_time,
               const base::Time& usage_timestamp,
               const base::Time& current_time,
               const icu::TimeZone* const time_zone,
               const std::optional<State>& previous_state) {
  std::optional<internal::TimeWindowLimit> time_window_limit =
      TimeWindowLimitFromPolicy(time_limit);
  std::optional<internal::TimeUsageLimit> time_usage_limit =
      TimeUsageLimitFromPolicy(time_limit);
  std::optional<TimeLimitOverride> time_limit_override =
      OverrideFromPolicy(time_limit);
  std::optional<TimeLimitOverride> local_time_limit_override =
      TimeLimitOverride::FromDictionary(local_override);
  // TODO(agawronska): Pass |usage_timestamp| instead of second |current_time|.
  return internal::UsageTimeLimitProcessor(
             std::move(time_window_limit), std::move(time_usage_limit),
             std::move(time_limit_override),
             std::move(local_time_limit_override), used_time, current_time,
             current_time, time_zone, previous_state)
      .GetState();
}

base::Time GetExpectedResetTime(const base::Value::Dict& time_limit,
                                const base::Value::Dict* local_override,
                                const base::Time current_time,
                                const icu::TimeZone* const time_zone) {
  std::optional<internal::TimeWindowLimit> time_window_limit =
      TimeWindowLimitFromPolicy(time_limit);
  std::optional<internal::TimeUsageLimit> time_usage_limit =
      TimeUsageLimitFromPolicy(time_limit);
  std::optional<TimeLimitOverride> time_limit_override =
      OverrideFromPolicy(time_limit);
  std::optional<TimeLimitOverride> local_time_limit_override =
      TimeLimitOverride::FromDictionary(local_override);
  return internal::UsageTimeLimitProcessor(
             std::move(time_window_limit), std::move(time_usage_limit),
             std::move(time_limit_override),
             std::move(local_time_limit_override), base::Minutes(0),
             base::Time(), current_time, time_zone, std::nullopt)
      .GetExpectedResetTime();
}

std::optional<base::TimeDelta> GetRemainingTimeUsage(
    const base::Value::Dict& time_limit,
    const base::Value::Dict* local_override,
    const base::Time current_time,
    const base::TimeDelta& used_time,
    const icu::TimeZone* const time_zone) {
  std::optional<internal::TimeWindowLimit> time_window_limit =
      TimeWindowLimitFromPolicy(time_limit);
  std::optional<internal::TimeUsageLimit> time_usage_limit =
      TimeUsageLimitFromPolicy(time_limit);
  std::optional<TimeLimitOverride> time_limit_override =
      OverrideFromPolicy(time_limit);
  std::optional<TimeLimitOverride> local_time_limit_override =
      TimeLimitOverride::FromDictionary(local_override);
  return internal::UsageTimeLimitProcessor(
             std::move(time_window_limit), std::move(time_usage_limit),
             std::move(time_limit_override),
             std::move(local_time_limit_override), used_time, base::Time(),
             current_time, time_zone, std::nullopt)
      .GetRemainingTimeUsage();
}

base::TimeDelta GetTimeUsageLimitResetTime(
    const base::Value::Dict& time_limit) {
  return internal::GetUsageLimitResetTime(TimeUsageLimitFromPolicy(time_limit));
}

std::set<PolicyType> UpdatedPolicyTypes(const base::Value::Dict& old_policy,
                                        const base::Value::Dict& new_policy) {
  std::set<PolicyType> updated_policies;
  if (TimeUsageLimitFromPolicy(old_policy) !=
      TimeUsageLimitFromPolicy(new_policy)) {
    updated_policies.insert(PolicyType::kUsageLimit);
  }
  if (TimeWindowLimitFromPolicy(old_policy) !=
      TimeWindowLimitFromPolicy(new_policy)) {
    updated_policies.insert(PolicyType::kFixedLimit);
  }

  std::optional<TimeLimitOverride> old_override =
      OverrideFromPolicy(old_policy);
  std::optional<TimeLimitOverride> new_override =
      OverrideFromPolicy(new_policy);
  // Override changes are added only when the new override has a duration.
  if (old_override != new_override && new_override &&
      new_override->duration()) {
    updated_policies.insert(PolicyType::kOverride);
  }
  return updated_policies;
}

std::set<PolicyType> GetEnabledTimeLimitPolicies(
    const base::Value::Dict& time_limit_prefs) {
  std::set<PolicyType> enabled_policies;

  std::optional<internal::TimeWindowLimit> time_window_limit =
      TimeWindowLimitFromPolicy(time_limit_prefs);
  if (time_window_limit && !time_window_limit->entries.empty()) {
    enabled_policies.insert(PolicyType::kFixedLimit);
  }

  std::optional<internal::TimeUsageLimit> time_usage_limit =
      TimeUsageLimitFromPolicy(time_limit_prefs);
  if (time_usage_limit && !time_usage_limit->entries.empty()) {
    enabled_policies.insert(PolicyType::kUsageLimit);
  }

  std::optional<TimeLimitOverride> time_limit_override =
      OverrideFromPolicy(time_limit_prefs);
  base::Time now = base::Time::Now();
  // Ignores the override time limit that is not created within 1 day.
  if (time_limit_override && now > time_limit_override->created_at() &&
      now - time_limit_override->created_at() < base::Days(1)) {
    enabled_policies.insert(PolicyType::kOverride);
  }

  return enabled_policies;
}

}  // namespace ash::usage_time_limit
