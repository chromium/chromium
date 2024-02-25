// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Processor for the UsageTimeLimit policy. Used to determine the current state
// of the client, for example if it is locked and the reason why it may be
// locked.

#ifndef CHROME_BROWSER_ASH_CHILD_ACCOUNTS_USAGE_TIME_LIMIT_PROCESSOR_H_
#define CHROME_BROWSER_ASH_CHILD_ACCOUNTS_USAGE_TIME_LIMIT_PROCESSOR_H_

#include <memory>
#include <optional>
#include <set>
#include <unordered_map>

#include "base/time/time.h"
#include "base/values.h"
#include "chromeos/ash/components/settings/timezone_settings.h"

namespace ash {
namespace usage_time_limit {
namespace internal {

enum class Weekday {
  kSunday = 0,
  kMonday,
  kTuesday,
  kWednesday,
  kThursday,
  kFriday,
  kSaturday,
  kCount,
};

struct TimeWindowLimitBoundaries {
  base::Time starts;
  base::Time ends;
};

struct TimeWindowLimitEntry {
  TimeWindowLimitEntry();
  bool operator==(const TimeWindowLimitEntry&) const;
  bool operator!=(const TimeWindowLimitEntry& rhs) const {
    return !(*this == rhs);
  }

  // Whether the time window limit entry ends on the following day from its
  // start.
  bool IsOvernight() const;

  // Returns a pair containing the timestamps for the start and end of a time
  // window limit. The input parameter is the UTC midnight on of the start day.
  TimeWindowLimitBoundaries GetLimits(base::Time start_day_midnight);

  // Start time of time window limit. This is the distance from midnight.
  base::TimeDelta starts_at;
  // End time of time window limit. This is the distance from midnight.
  base::TimeDelta ends_at;
  // Last time this entry was updated.
  base::Time last_updated;
};

class TimeWindowLimit {
 public:
  explicit TimeWindowLimit(const base::Value& window_limit_dict);

  TimeWindowLimit(const TimeWindowLimit&) = delete;
  TimeWindowLimit& operator=(const TimeWindowLimit&) = delete;

  ~TimeWindowLimit();
  TimeWindowLimit(TimeWindowLimit&&);
  TimeWindowLimit& operator=(TimeWindowLimit&&);
  bool operator==(const TimeWindowLimit&) const;
  bool operator!=(const TimeWindowLimit& rhs) const { return !(*this == rhs); }

  std::unordered_map<Weekday, std::optional<TimeWindowLimitEntry>> entries;
};

struct TimeUsageLimitEntry {
  TimeUsageLimitEntry();
  bool operator==(const TimeUsageLimitEntry&) const;
  bool operator!=(const TimeUsageLimitEntry& rhs) const {
    return !(*this == rhs);
  }

  base::TimeDelta usage_quota;
  base::Time last_updated;
};

class TimeUsageLimit {
 public:
  explicit TimeUsageLimit(const base::Value::Dict& usage_limit_dict);

  TimeUsageLimit(const TimeUsageLimit&) = delete;
  TimeUsageLimit& operator=(const TimeUsageLimit&) = delete;

  ~TimeUsageLimit();
  TimeUsageLimit(TimeUsageLimit&&);
  TimeUsageLimit& operator=(TimeUsageLimit&&);
  bool operator==(const TimeUsageLimit&) const;
  bool operator!=(const TimeUsageLimit& rhs) const { return !(*this == rhs); }

  std::unordered_map<Weekday, std::optional<TimeUsageLimitEntry>> entries;
  base::TimeDelta resets_at;
};
}  // namespace internal

enum class PolicyType {
  kNoPolicy,
  kOverride,
  kFixedLimit,  // Past bed time (ie, 9pm)
  kUsageLimit   // Too much time on screen (ie, 30 minutes per day)
};

struct State {
  // Whether the device is currently locked.
  bool is_locked = false;

  // Which policy is responsible for the current state.
  // If it is locked, one of [ kOverride, kFixedLimit, kUsageLimit ]
  // If it is not locked, one of [ kNoPolicy, kOverride ]
  PolicyType active_policy;

  // Whether time_usage_limit is currently active.
  bool is_time_usage_limit_enabled = false;

  // Remaining screen usage quota. Only available if
  // is_time_limit_enabled = true
  base::TimeDelta remaining_usage;

  // When the time usage limit started being enforced. Only available when
  // is_time_usage_limit_enabled = true and remaining_usage is 0, which means
  // that the time usage limit is enforced, and therefore should have a start
  // time.
  base::Time time_usage_limit_started;

  // Next epoch time that time limit state could change. This could be the
  // start time of the next fixed window limit, the end time of the current
  // fixed limit, the earliest time a usage limit could be reached, or the
  // next time when screen time will start.
  base::Time next_state_change_time;

  // The policy that will be active in the next state.
  PolicyType next_state_active_policy;

  // This is the next time that the user's session will be unlocked. This is
  // only set when is_locked=true;
  base::Time next_unlock_time;
};

// Returns the current state of the user session with the given usage time limit
// policy.
// |time_limit| dictionary with UsageTimeLimit policy data.
// |local_override| dictionary with data of the last local override (authorized
//                  by parent access code).
// |used_time| time used in the current day.
// |usage_timestamp| when was |used_time| data collected. Usually differs from
//                   |current_time| by milliseconds.
// |previous_state| state previously returned by UsageTimeLimitProcessor.
State GetState(const base::Value::Dict& time_limit,
               const base::Value::Dict* local_override,
               const base::TimeDelta& used_time,
               const base::Time& usage_timestamp,
               const base::Time& current_time,
               const icu::TimeZone* const time_zone,
               const std::optional<State>& previous_state);

// Returns the expected time that the used time stored should be reset.
// |time_limit| dictionary with UsageTimeLimit policy data.
// |local_override| dictionary with data of the last local override (authorized
//                  by parent access code).
base::Time GetExpectedResetTime(const base::Value::Dict& time_limit,
                                const base::Value::Dict* local_override,
                                base::Time current_time,
                                const icu::TimeZone* const time_zone);

// Returns the remaining time usage if the time usage limit is enabled.
// |time_limit| dictionary with UsageTimeLimit policy data.
// |local_override| dictionary with data of the last local override (authorized
//                  by parent access code).
// |used_time| time used in the current day.
std::optional<base::TimeDelta> GetRemainingTimeUsage(
    const base::Value::Dict& time_limit,
    const base::Value::Dict* local_override,
    const base::Time current_time,
    const base::TimeDelta& used_time,
    const icu::TimeZone* const time_zone);

// Returns time of the day when TimeUsageLimit policy is reset, represented by
// the distance from midnight.
base::TimeDelta GetTimeUsageLimitResetTime(const base::Value::Dict& time_limit);

// Compares two Usage Time Limit policy dictionaries and returns which
// PolicyTypes changed between the two versions. Changes on simple overrides are
// not reported, but changes on override with durations are, the reason is that
// this method is intended for notifications, and the former does not trigger
// those while the latter does.
std::set<PolicyType> UpdatedPolicyTypes(const base::Value::Dict& old_policy,
                                        const base::Value::Dict& new_policy);

// Returns the active time limit polices in `time_limit_prefs`.
// `time_limit_prefs` is the value of prefs::kUsageTimeLimit which stores the
// usage time limit preference of a user.
std::set<PolicyType> GetEnabledTimeLimitPolicies(
    const base::Value::Dict& time_limit_prefs);

}  // namespace usage_time_limit
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_CHILD_ACCOUNTS_USAGE_TIME_LIMIT_PROCESSOR_H_
