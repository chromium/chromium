// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/child_accounts/time_limit_test_utils.h"

#include <algorithm>
#include <utility>

#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/optional.h"

namespace chromeos {
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
  return std::to_string((time - base::Time::UnixEpoch()).InMilliseconds());
}

base::TimeDelta CreateTime(int hour, int minute) {
  DCHECK_LT(hour, 24);
  DCHECK_GE(hour, 0);
  DCHECK_LT(minute, 60);
  DCHECK_GE(minute, 0);
  return base::TimeDelta::FromMinutes(hour * 60 + minute);
}

base::Value CreatePolicyTime(base::TimeDelta time) {
  DCHECK_EQ(
      time.InNanoseconds() % base::TimeDelta::FromMinutes(1).InNanoseconds(),
      0);
  DCHECK_LT(time, base::TimeDelta::FromHours(24));

  int hour = time.InHours();
  int minute = time.InMinutes() -
               time.InHours() * base::TimeDelta::FromHours(1).InMinutes();
  base::Value policyTime(base::Value::Type::DICTIONARY);
  policyTime.SetKey(kWindowLimitEntryTimeHour, base::Value(hour));
  policyTime.SetKey(kWindowLimitEntryTimeMinute, base::Value(minute));
  return policyTime;
}

base::Value CreateTimeWindow(const std::string& day,
                             base::TimeDelta start_time,
                             base::TimeDelta end_time,
                             base::Time last_updated) {
  base::Value time_window(base::Value::Type::DICTIONARY);
  time_window.SetKey(kWindowLimitEntryEffectiveDay, base::Value(day));
  time_window.SetKey(kWindowLimitEntryStartsAt, CreatePolicyTime(start_time));
  time_window.SetKey(kWindowLimitEntryEndsAt, CreatePolicyTime(end_time));
  time_window.SetKey(kTimeLimitLastUpdatedAt,
                     base::Value(CreatePolicyTimestamp(last_updated)));
  return time_window;
}

base::Value CreateTimeUsage(base::TimeDelta usage_quota,
                            base::Time last_updated) {
  base::Value time_usage(base::Value::Type::DICTIONARY);
  time_usage.SetKey(kUsageLimitUsageQuota,
                    base::Value(usage_quota.InMinutes()));
  time_usage.SetKey(kTimeLimitLastUpdatedAt,
                    base::Value(CreatePolicyTimestamp(last_updated)));
  return time_usage;
}

base::Value CreateTimeLimitPolicy(base::TimeDelta reset_time) {
  base::Value time_usage_limit = base::Value(base::Value::Type::DICTIONARY);
  time_usage_limit.SetKey(kUsageLimitResetAt, CreatePolicyTime(reset_time));

  base::Value time_limit = base::Value(base::Value::Type::DICTIONARY);
  time_limit.SetKey(kTimeUsageLimit, std::move(time_usage_limit));

  return time_limit;
}

void AddTimeUsageLimit(base::Value* policy,
                       std::string day,
                       base::TimeDelta quota,
                       base::Time last_updated) {
  // Asserts that the usage limit quota in minutes corresponds to an integer
  // number.
  DCHECK_EQ(
      quota.InNanoseconds() % base::TimeDelta::FromMinutes(1).InNanoseconds(),
      0);
  DCHECK_LT(quota, base::TimeDelta::FromHours(24));

  std::transform(day.begin(), day.end(), day.begin(), ::tolower);
  policy->FindKey(kTimeUsageLimit)
      ->SetKey(day, CreateTimeUsage(quota, last_updated));
}

void AddTimeWindowLimit(base::Value* policy,
                        const std::string& day,
                        base::TimeDelta start_time,
                        base::TimeDelta end_time,
                        base::Time last_updated) {
  base::Value* time_window_limit = policy->FindKey(kTimeWindowLimit);
  if (!time_window_limit) {
    time_window_limit = policy->SetKey(
        kTimeWindowLimit, base::Value(base::Value::Type::DICTIONARY));
  }

  base::Value* window_limit_entries =
      time_window_limit->FindKey(kWindowLimitEntries);
  if (!window_limit_entries) {
    window_limit_entries = time_window_limit->SetKey(
        kWindowLimitEntries, base::Value(base::Value::Type::LIST));
  }

  window_limit_entries->Append(
      CreateTimeWindow(day, start_time, end_time, last_updated));
}

void AddOverride(base::Value* policy,
                 usage_time_limit::TimeLimitOverride::Action action,
                 base::Time created_at) {
  base::Value* overrides =
      policy->FindKey(usage_time_limit::TimeLimitOverride::kOverridesDictKey);
  if (!overrides) {
    overrides =
        policy->SetKey(usage_time_limit::TimeLimitOverride::kOverridesDictKey,
                       base::Value(base::Value::Type::LIST));
  }

  usage_time_limit::TimeLimitOverride new_override(action, created_at,
                                                   base::nullopt);
  overrides->Append(new_override.ToDictionary());
}

void AddOverrideWithDuration(base::Value* policy,
                             usage_time_limit::TimeLimitOverride::Action action,
                             base::Time created_at,
                             base::TimeDelta duration) {
  base::Value* overrides =
      policy->FindKey(usage_time_limit::TimeLimitOverride::kOverridesDictKey);
  if (!overrides) {
    overrides =
        policy->SetKey(usage_time_limit::TimeLimitOverride::kOverridesDictKey,
                       base::Value(base::Value::Type::LIST));
  }

  usage_time_limit::TimeLimitOverride new_override(action, created_at,
                                                   duration);
  overrides->Append(new_override.ToDictionary());
}

std::string PolicyToString(const base::Value& policy) {
  std::string json_string;
  base::JSONWriter::Write(policy, &json_string);
  return json_string;
}

}  // namespace time_limit_test_utils
}  // namespace chromeos
