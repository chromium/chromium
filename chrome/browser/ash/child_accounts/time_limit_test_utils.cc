// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/child_accounts/time_limit_test_utils.h"

#include <optional>
#include <utility>

#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"

namespace ash {
namespace time_limit_test_utils {
namespace {

// Definition of private constants.
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

}  // namespace

// Definition of public constants.
const char kMonday[] = "MONDAY";
const char kTuesday[] = "TUESDAY";
const char kWednesday[] = "WEDNESDAY";
const char kThursday[] = "THURSDAY";
const char kFriday[] = "FRIDAY";
const char kSaturday[] = "SATURDAY";
const char kSunday[] = "SUNDAY";

base::Time TimeFromString(const char* time_string) {
  base::Time time;
  if (!base::Time::FromUTCString(time_string, &time))
    LOG(ERROR) << "Wrong time string format.";

  return time;
}

std::string CreatePolicyTimestamp(const char* time_string) {
  base::Time time = TimeFromString(time_string);
  return CreatePolicyTimestamp(time);
}

std::string CreatePolicyTimestamp(base::Time time) {
  return base::NumberToString(
      (time - base::Time::UnixEpoch()).InMilliseconds());
}

base::TimeDelta CreateTime(int hour, int minute) {
  DCHECK_LT(hour, 24);
  DCHECK_GE(hour, 0);
  DCHECK_LT(minute, 60);
  DCHECK_GE(minute, 0);
  return base::Minutes(hour * 60 + minute);
}

base::Value::Dict CreatePolicyTime(base::TimeDelta time) {
  DCHECK_EQ(time.InNanoseconds() % base::Minutes(1).InNanoseconds(), 0);
  DCHECK_LT(time, base::Hours(24));

  int hour = time.InHours();
  int minute = time.InMinutes() - time.InHours() * base::Hours(1).InMinutes();
  base::Value::Dict policyTime;
  policyTime.Set(kWindowLimitEntryTimeHour, base::Value(hour));
  policyTime.Set(kWindowLimitEntryTimeMinute, base::Value(minute));
  return policyTime;
}

base::Value::Dict CreateTimeWindow(const std::string& day,
                                   base::TimeDelta start_time,
                                   base::TimeDelta end_time,
                                   base::Time last_updated) {
  base::Value::Dict time_window;
  time_window.Set(kWindowLimitEntryEffectiveDay, base::Value(day));
  time_window.Set(kWindowLimitEntryStartsAt, CreatePolicyTime(start_time));
  time_window.Set(kWindowLimitEntryEndsAt, CreatePolicyTime(end_time));
  time_window.Set(kTimeLimitLastUpdatedAt,
                  base::Value(CreatePolicyTimestamp(last_updated)));
  return time_window;
}

base::Value::Dict CreateTimeUsage(base::TimeDelta usage_quota,
                                  base::Time last_updated) {
  base::Value::Dict time_usage;
  time_usage.Set(kUsageLimitUsageQuota, base::Value(usage_quota.InMinutes()));
  time_usage.Set(kTimeLimitLastUpdatedAt,
                 base::Value(CreatePolicyTimestamp(last_updated)));
  return time_usage;
}

base::Value::Dict CreateTimeLimitPolicy(base::TimeDelta reset_time) {
  base::Value::Dict time_usage_limit;
  time_usage_limit.Set(kUsageLimitResetAt, CreatePolicyTime(reset_time));

  base::Value::Dict time_limit;
  time_limit.Set(kTimeUsageLimit, std::move(time_usage_limit));

  return time_limit;
}

void AddTimeUsageLimit(base::Value::Dict* policy,
                       std::string day,
                       base::TimeDelta quota,
                       base::Time last_updated) {
  // Asserts that the usage limit quota in minutes corresponds to an integer
  // number.
  DCHECK_EQ(quota.InNanoseconds() % base::Minutes(1).InNanoseconds(), 0);
  DCHECK_LT(quota, base::Hours(24));

  base::ranges::transform(day, day.begin(), ::tolower);
  policy->Find(kTimeUsageLimit)
      ->GetDict()
      .Set(day, CreateTimeUsage(quota, last_updated));
}

void AddTimeWindowLimit(base::Value::Dict* policy,
                        const std::string& day,
                        base::TimeDelta start_time,
                        base::TimeDelta end_time,
                        base::Time last_updated) {
  base::Value::Dict* time_window_limit = policy->EnsureDict(kTimeWindowLimit);
  base::Value::List* window_limit_entries =
      time_window_limit->EnsureList(kWindowLimitEntries);
  window_limit_entries->Append(
      CreateTimeWindow(day, start_time, end_time, last_updated));
}

void AddOverride(base::Value::Dict* policy,
                 usage_time_limit::TimeLimitOverride::Action action,
                 base::Time created_at) {
  base::Value::List* overrides = policy->EnsureList(
      usage_time_limit::TimeLimitOverride::kOverridesDictKey);
  usage_time_limit::TimeLimitOverride new_override(action, created_at,
                                                   std::nullopt);
  overrides->Append(new_override.ToDictionary());
}

void AddOverrideWithDuration(base::Value::Dict* policy,
                             usage_time_limit::TimeLimitOverride::Action action,
                             base::Time created_at,
                             base::TimeDelta duration) {
  base::Value::List* overrides = policy->EnsureList(
      usage_time_limit::TimeLimitOverride::kOverridesDictKey);
  usage_time_limit::TimeLimitOverride new_override(action, created_at,
                                                   duration);
  overrides->Append(new_override.ToDictionary());
}

std::string PolicyToString(const base::Value::Dict& policy) {
  std::string json_string;
  base::JSONWriter::Write(policy, &json_string);
  return json_string;
}

}  // namespace time_limit_test_utils
}  // namespace ash
