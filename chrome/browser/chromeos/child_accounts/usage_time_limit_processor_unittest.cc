// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/child_accounts/usage_time_limit_processor.h"

#include <memory>
#include <string>
#include <utility>

#include "base/values.h"
#include "chromeos/settings/timezone_settings.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace usage_time_limit {

using UsageTimeLimitProcessorTest = testing::Test;

base::Value CreateTime(int hour, int minute) {
  base::Value time(base::Value::Type::DICTIONARY);
  time.SetKey("hour", base::Value(hour));
  time.SetKey("minute", base::Value(minute));
  return time;
}

base::Value CreateTimeWindow(base::Value day,
                             base::Value start,
                             base::Value end,
                             base::Value last_updated) {
  base::Value time_window(base::Value::Type::DICTIONARY);
  time_window.SetKey("effective_day", std::move(day));
  time_window.SetKey("starts_at", std::move(start));
  time_window.SetKey("ends_at", std::move(end));
  time_window.SetKey("last_updated_millis", std::move(last_updated));
  return time_window;
}

base::Value CreateTimeUsage(base::Value usage_quota, base::Value last_updated) {
  base::Value time_usage(base::Value::Type::DICTIONARY);
  time_usage.SetKey("usage_quota_mins", std::move(usage_quota));
  time_usage.SetKey("last_updated_millis", std::move(last_updated));
  return time_usage;
}

base::Time TimeFromString(const char* time_string) {
  base::Time time;
  if (!base::Time::FromUTCString(time_string, &time))
    LOG(ERROR) << "Wrong time string format.";

  return time;
}

std::string CreatePolicyTimestamp(const char* time_string) {
  base::Time time = TimeFromString(time_string);

  return std::to_string(
      base::TimeDelta(time - base::Time::UnixEpoch()).InMilliseconds());
}

void AssertEqState(State expected, State actual) {
  ASSERT_EQ(expected.is_locked, actual.is_locked);
  ASSERT_EQ(expected.active_policy, actual.active_policy);
  ASSERT_EQ(expected.is_time_usage_limit_enabled,
            actual.is_time_usage_limit_enabled);

  if (actual.is_time_usage_limit_enabled) {
    ASSERT_EQ(expected.remaining_usage, actual.remaining_usage);
    if (actual.remaining_usage <= base::TimeDelta::FromMinutes(0)) {
      ASSERT_EQ(expected.time_usage_limit_started,
                actual.time_usage_limit_started);
    }
  }

  ASSERT_EQ(expected.next_state_change_time, actual.next_state_change_time);
  ASSERT_EQ(expected.next_state_active_policy, actual.next_state_active_policy);

  if (actual.is_locked)
    ASSERT_EQ(expected.next_unlock_time, actual.next_unlock_time);

  ASSERT_EQ(expected.last_state_changed, actual.last_state_changed);
}

namespace internal {

using UsageTimeLimitProcessorInternalTest = testing::Test;

// TODO: improve tests to cover bugs on crbug/894047
TEST_F(UsageTimeLimitProcessorInternalTest, TimeLimitWindowValid) {
  std::string last_updated_millis =
      CreatePolicyTimestamp("1 Jan 1970 00:00:00");
  base::Value monday_time_limit =
      CreateTimeWindow(base::Value("MONDAY"), CreateTime(22, 30),
                       CreateTime(7, 30), base::Value(last_updated_millis));
  base::Value friday_time_limit =
      CreateTimeWindow(base::Value("FRIDAY"), CreateTime(23, 0),
                       CreateTime(8, 20), base::Value(last_updated_millis));

  base::Value window_limit_entries(base::Value::Type::LIST);
  window_limit_entries.GetList().push_back(std::move(monday_time_limit));
  window_limit_entries.GetList().push_back(std::move(friday_time_limit));

  base::Value time_window_limit = base::Value(base::Value::Type::DICTIONARY);
  time_window_limit.SetKey("entries", std::move(window_limit_entries));

  // Call tested function.
  TimeWindowLimit window_limit_struct(time_window_limit);

  ASSERT_TRUE(window_limit_struct.entries[Weekday::kMonday]);
  ASSERT_EQ(window_limit_struct.entries[Weekday::kMonday]
                .value()
                .starts_at.InMinutes(),
            22 * 60 + 30);
  ASSERT_EQ(
      window_limit_struct.entries[Weekday::kMonday].value().ends_at.InMinutes(),
      7 * 60 + 30);
  ASSERT_EQ(window_limit_struct.entries[Weekday::kMonday].value().last_updated,
            base::Time::UnixEpoch());

  ASSERT_TRUE(window_limit_struct.entries[Weekday::kFriday]);
  ASSERT_EQ(window_limit_struct.entries[Weekday::kFriday]
                .value()
                .starts_at.InMinutes(),
            23 * 60);
  ASSERT_EQ(
      window_limit_struct.entries[Weekday::kFriday].value().ends_at.InMinutes(),
      8 * 60 + 20);
  ASSERT_EQ(window_limit_struct.entries[Weekday::kFriday].value().last_updated,
            base::Time::UnixEpoch());

  // Assert that weekdays without time_window_limits are not set.
  ASSERT_FALSE(window_limit_struct.entries[Weekday::kTuesday]);
  ASSERT_FALSE(window_limit_struct.entries[Weekday::kWednesday]);
  ASSERT_FALSE(window_limit_struct.entries[Weekday::kThursday]);
  ASSERT_FALSE(window_limit_struct.entries[Weekday::kSaturday]);
  ASSERT_FALSE(window_limit_struct.entries[Weekday::kSunday]);
}

// Validates that a well formed dictionary containing the time_usage_limit
// information from the UsageTimeLimit policy is converted to its intermediate
// representation correctly.
TEST_F(UsageTimeLimitProcessorInternalTest, TimeUsageWindowValid) {
  // Create dictionary containing the policy information.
  std::string last_updated_millis_one =
      CreatePolicyTimestamp("1 Jan 2018 10:00:00");
  std::string last_updated_millis_two =
      CreatePolicyTimestamp("1 Jan 2018 11:00:00");
  base::Value tuesday_time_usage =
      CreateTimeUsage(base::Value(120), base::Value(last_updated_millis_one));
  base::Value thursday_time_usage =
      CreateTimeUsage(base::Value(80), base::Value(last_updated_millis_two));

  base::Value time_usage_limit = base::Value(base::Value::Type::DICTIONARY);
  time_usage_limit.SetKey("tuesday", std::move(tuesday_time_usage));
  time_usage_limit.SetKey("thursday", std::move(thursday_time_usage));
  time_usage_limit.SetKey("reset_at", CreateTime(8, 0));

  // Call tested functions.
  TimeUsageLimit usage_limit_struct(time_usage_limit);

  ASSERT_EQ(usage_limit_struct.resets_at.InMinutes(), 8 * 60);

  ASSERT_EQ(usage_limit_struct.entries[Weekday::kTuesday]
                .value()
                .usage_quota.InMinutes(),
            120);
  ASSERT_EQ(usage_limit_struct.entries[Weekday::kTuesday].value().last_updated,
            base::Time::FromDoubleT(1514800800));

  ASSERT_EQ(usage_limit_struct.entries[Weekday::kThursday]
                .value()
                .usage_quota.InMinutes(),
            80);
  ASSERT_EQ(usage_limit_struct.entries[Weekday::kThursday].value().last_updated,
            base::Time::FromDoubleT(1514804400));

  // Assert that weekdays without time_usage_limits are not set.
  ASSERT_FALSE(usage_limit_struct.entries[Weekday::kMonday]);
  ASSERT_FALSE(usage_limit_struct.entries[Weekday::kWednesday]);
  ASSERT_FALSE(usage_limit_struct.entries[Weekday::kFriday]);
  ASSERT_FALSE(usage_limit_struct.entries[Weekday::kSaturday]);
  ASSERT_FALSE(usage_limit_struct.entries[Weekday::kSunday]);
}

// Validates that a well formed dictionary containing the override information
// from the UsageTimeLimit policy is converted to its intermediate
// representation correctly.
TEST_F(UsageTimeLimitProcessorInternalTest, OverrideValid) {
  // Create policy information.
  std::string created_at_millis = CreatePolicyTimestamp("1 Jan 2018 10:00:00");
  base::Value override_one = base::Value(base::Value::Type::DICTIONARY);
  override_one.SetKey("action", base::Value("UNLOCK"));
  override_one.SetKey("created_at_millis", base::Value(created_at_millis));

  base::Value override_two = base::Value(base::Value::Type::DICTIONARY);
  override_two.SetKey("action", base::Value("LOCK"));
  override_two.SetKey("created_at_millis",
                      base::Value(CreatePolicyTimestamp("1 Jan 2018 9:00:00")));

  base::Value overrides(base::Value::Type::LIST);
  overrides.GetList().push_back(std::move(override_one));
  overrides.GetList().push_back(std::move(override_two));

  // Call tested functions.
  TimeLimitOverride override_struct(overrides);

  // Assert right fields are set.
  ASSERT_EQ(override_struct.action, TimeLimitOverride::Action::kUnlock);
  ASSERT_EQ(override_struct.created_at, TimeFromString("1 Jan 2018 10:00:00"));
  ASSERT_FALSE(override_struct.duration);
}

}  // namespace internal

// Tests the GetState for a policy that only have the time window limit set. It
// is checked that the state is correct before, during and after the policy is
// enforced.
TEST_F(UsageTimeLimitProcessorTest, GetStateOnlyTimeWindowLimitSet) {
  std::unique_ptr<icu::TimeZone> timezone(
      icu::TimeZone::createTimeZone("GMT+0300"));

  // Set up policy.
  std::string last_updated_millis =
      CreatePolicyTimestamp("1 Jan 2018 10:00 GMT+0300");
  base::Value sunday_time_limit =
      CreateTimeWindow(base::Value("SUNDAY"), CreateTime(22, 0),
                       CreateTime(7, 30), base::Value(last_updated_millis));
  CreatePolicyTimestamp("1 Jan 2018 10:00 GMT+0300");
  base::Value monday_time_limit =
      CreateTimeWindow(base::Value("MONDAY"), CreateTime(21, 0),
                       CreateTime(7, 30), base::Value(last_updated_millis));
  base::Value tuesday_time_limit =
      CreateTimeWindow(base::Value("TUESDAY"), CreateTime(7, 30),
                       CreateTime(9, 0), base::Value(last_updated_millis));
  base::Value friday_time_limit =
      CreateTimeWindow(base::Value("FRIDAY"), CreateTime(21, 0),
                       CreateTime(7, 30), base::Value(last_updated_millis));

  base::Value window_limit_entries(base::Value::Type::LIST);
  window_limit_entries.GetList().push_back(std::move(sunday_time_limit));
  window_limit_entries.GetList().push_back(std::move(monday_time_limit));
  window_limit_entries.GetList().push_back(std::move(tuesday_time_limit));
  window_limit_entries.GetList().push_back(std::move(friday_time_limit));

  base::Value time_window_limit(base::Value::Type::DICTIONARY);
  time_window_limit.SetKey("entries", std::move(window_limit_entries));

  std::unique_ptr<base::Value> time_limit =
      std::make_unique<base::Value>(base::Value::Type::DICTIONARY);
  time_limit->SetKey("time_window_limit", std::move(time_window_limit));

  std::unique_ptr<base::DictionaryValue> time_limit_dictionary =
      base::DictionaryValue::From(std::move(time_limit));

  base::Time monday_time_window_limit_start =
      TimeFromString("Mon, 1 Jan 2018 21:00 GMT+0300");
  base::Time monday_time_window_limit_end =
      TimeFromString("Tue, 2 Jan 2018 7:30 GMT+0300");
  base::Time tuesday_time_window_limit_end =
      TimeFromString("Tue, 2 Jan 2018 9:00 GMT+0300");
  base::Time friday_time_window_limit_start =
      TimeFromString("Fri, 5 Jan 2018 21:00 GMT+0300");

  // Check state before Monday time window limit.
  base::Time time_one = TimeFromString("Mon, 1 Jan 2018 20:00 GMT+0300");
  State state_one =
      GetState(time_limit_dictionary, base::TimeDelta::FromMinutes(0), time_one,
               time_one, timezone.get(), base::nullopt);

  State expected_state_one;
  expected_state_one.is_locked = false;
  expected_state_one.active_policy = ActivePolicies::kNoActivePolicy;
  expected_state_one.is_time_usage_limit_enabled = false;
  expected_state_one.next_state_change_time = monday_time_window_limit_start;
  expected_state_one.next_state_active_policy = ActivePolicies::kFixedLimit;
  expected_state_one.last_state_changed = base::Time();

  AssertEqState(expected_state_one, state_one);

  // Check state during the Monday time window limit.
  base::Time time_two = TimeFromString("Mon, 1 Jan 2018 22:00 GMT+0300");
  State state_two =
      GetState(time_limit_dictionary, base::TimeDelta::FromMinutes(0), time_two,
               time_two, timezone.get(), state_one);

  State expected_state_two;
  expected_state_two.is_locked = true;
  expected_state_two.active_policy = ActivePolicies::kFixedLimit;
  expected_state_two.is_time_usage_limit_enabled = false;
  expected_state_two.next_state_change_time = monday_time_window_limit_end;
  expected_state_two.next_state_active_policy = ActivePolicies::kFixedLimit;
  expected_state_two.next_unlock_time = tuesday_time_window_limit_end;
  expected_state_two.last_state_changed = time_two;

  AssertEqState(expected_state_two, state_two);

  // Check state after the Monday time window limit.
  base::Time time_three = TimeFromString("Tue, 2 Jan 2018 9:00 GMT+0300");
  State state_three =
      GetState(time_limit_dictionary, base::TimeDelta::FromMinutes(0),
               time_three, time_three, timezone.get(), state_two);

  State expected_state_three;
  expected_state_three.is_locked = false;
  expected_state_three.active_policy = ActivePolicies::kNoActivePolicy;
  expected_state_three.is_time_usage_limit_enabled = false;
  expected_state_three.next_state_change_time = friday_time_window_limit_start;
  expected_state_three.next_state_active_policy = ActivePolicies::kFixedLimit;
  expected_state_three.last_state_changed = time_three;

  AssertEqState(expected_state_three, state_three);
}

// Tests the GetState for a policy that only have the time usage limit set. It
// is checked that the state is correct before and during the policy is
// enforced.
TEST_F(UsageTimeLimitProcessorTest, GetStateOnlyTimeUsageLimitSet) {
  std::unique_ptr<icu::TimeZone> timezone(icu::TimeZone::createTimeZone("GMT"));

  // Set up policy.
  std::string last_updated = CreatePolicyTimestamp("1 Jan 2018 8:00");
  base::Value tuesday_time_usage =
      CreateTimeUsage(base::Value(120), base::Value(last_updated));
  base::Value thursday_time_usage =
      CreateTimeUsage(base::Value(80), base::Value(last_updated));

  base::Value time_usage_limit = base::Value(base::Value::Type::DICTIONARY);
  time_usage_limit.SetKey("tuesday", std::move(tuesday_time_usage));
  time_usage_limit.SetKey("thursday", std::move(thursday_time_usage));
  time_usage_limit.SetKey("reset_at", CreateTime(8, 0));

  std::unique_ptr<base::Value> time_limit =
      std::make_unique<base::Value>(base::Value::Type::DICTIONARY);
  time_limit->SetKey("time_usage_limit", std::move(time_usage_limit));

  std::unique_ptr<base::DictionaryValue> time_limit_dictionary =
      base::DictionaryValue::From(std::move(time_limit));

  // Check state before time usage limit is enforced.
  base::Time time_one = TimeFromString("Mon, 1 Jan 2018 20:00");
  State state_one =
      GetState(time_limit_dictionary, base::TimeDelta::FromMinutes(120),
               time_one, time_one, timezone.get(), base::nullopt);

  State expected_state_one;
  expected_state_one.is_locked = false;
  expected_state_one.active_policy = ActivePolicies::kNoActivePolicy;
  expected_state_one.is_time_usage_limit_enabled = false;
  // Next state is the minimum time when the time usage limit could be enforced.
  expected_state_one.next_state_change_time =
      TimeFromString("Tue, 2 Jan 2018 10:00");
  expected_state_one.next_state_active_policy = ActivePolicies::kUsageLimit;
  expected_state_one.last_state_changed = base::Time();

  AssertEqState(expected_state_one, state_one);

  // Check state before time usage limit is enforced.
  base::Time time_two = TimeFromString("Tue, 2 Jan 2018 12:00");
  State state_two =
      GetState(time_limit_dictionary, base::TimeDelta::FromMinutes(60),
               time_two, time_two, timezone.get(), state_one);

  State expected_state_two;
  expected_state_two.is_locked = false;
  expected_state_two.active_policy = ActivePolicies::kNoActivePolicy;
  expected_state_two.is_time_usage_limit_enabled = true;
  expected_state_two.remaining_usage = base::TimeDelta::FromMinutes(60);
  expected_state_two.next_state_change_time =
      time_two + base::TimeDelta::FromMinutes(60);
  expected_state_two.next_state_active_policy = ActivePolicies::kUsageLimit;
  expected_state_two.last_state_changed = base::Time();

  AssertEqState(expected_state_two, state_two);

  // Check state when the time usage limit should be enforced.
  base::Time time_three = TimeFromString("Tue, 2 Jan 2018 21:00");
  State state_three =
      GetState(time_limit_dictionary, base::TimeDelta::FromMinutes(120),
               time_three, time_three, timezone.get(), state_two);

  base::Time wednesday_reset_time = TimeFromString("Wed, 3 Jan 2018 8:00");

  State expected_state_three;
  expected_state_three.is_locked = true;
  expected_state_three.active_policy = ActivePolicies::kUsageLimit;
  expected_state_three.is_time_usage_limit_enabled = true;
  expected_state_three.remaining_usage = base::TimeDelta::FromMinutes(0);
  expected_state_three.time_usage_limit_started = time_three;
  expected_state_three.next_state_change_time = wednesday_reset_time;
  expected_state_three.next_state_active_policy =
      ActivePolicies::kNoActivePolicy;
  expected_state_three.next_unlock_time = wednesday_reset_time;
  expected_state_three.last_state_changed = time_three;

  AssertEqState(expected_state_three, state_three);
}

// Test GetState with both time window limit and time usage limit defined.
TEST_F(UsageTimeLimitProcessorTest, GetStateWithTimeUsageAndWindowLimitActive) {
  std::unique_ptr<icu::TimeZone> timezone(icu::TimeZone::createTimeZone("GMT"));

  // Setup time window limit.
  std::string last_updated = CreatePolicyTimestamp("1 Jan 2018 8:00");
  base::Value monday_time_limit =
      CreateTimeWindow(base::Value("MONDAY"), CreateTime(21, 0),
                       CreateTime(8, 30), base::Value(last_updated));
  base::Value friday_time_limit =
      CreateTimeWindow(base::Value("FRIDAY"), CreateTime(21, 0),
                       CreateTime(8, 30), base::Value(last_updated));

  base::Value window_limit_entries(base::Value::Type::LIST);
  window_limit_entries.GetList().push_back(std::move(monday_time_limit));
  window_limit_entries.GetList().push_back(std::move(friday_time_limit));

  base::Value time_window_limit(base::Value::Type::DICTIONARY);
  time_window_limit.SetKey("entries", std::move(window_limit_entries));

  // Setup time usage limit.
  base::Value monday_time_usage =
      CreateTimeUsage(base::Value(120), base::Value(last_updated));

  base::Value time_usage_limit = base::Value(base::Value::Type::DICTIONARY);
  time_usage_limit.SetKey("monday", std::move(monday_time_usage));
  time_usage_limit.SetKey("reset_at", CreateTime(8, 0));

  // Setup policy.
  std::unique_ptr<base::Value> time_limit =
      std::make_unique<base::Value>(base::Value::Type::DICTIONARY);
  time_limit->SetKey("time_window_limit", std::move(time_window_limit));
  time_limit->SetKey("time_usage_limit", std::move(time_usage_limit));

  std::unique_ptr<base::DictionaryValue> time_limit_dictionary =
      base::DictionaryValue::From(std::move(time_limit));

  // Check state before any policy is enforced.
  base::Time time_one = TimeFromString("Mon, 1 Jan 2018 14:00");
  State state_one =
      GetState(time_limit_dictionary, base::TimeDelta::FromMinutes(80),
               time_one, time_one, timezone.get(), base::nullopt);

  State expected_state_one;
  expected_state_one.is_locked = false;
  expected_state_one.active_policy = ActivePolicies::kNoActivePolicy;
  expected_state_one.is_time_usage_limit_enabled = true;
  expected_state_one.remaining_usage = base::TimeDelta::FromMinutes(40);
  expected_state_one.next_state_change_time =
      time_one + base::TimeDelta::FromMinutes(40);
  expected_state_one.next_state_active_policy = ActivePolicies::kUsageLimit;
  expected_state_one.last_state_changed = base::Time();

  AssertEqState(expected_state_one, state_one);

  // Check state during time usage limit.
  base::Time time_two = TimeFromString("Mon, 1 Jan 2018 16:00");
  State state_two =
      GetState(time_limit_dictionary, base::TimeDelta::FromMinutes(121),
               time_two, time_two, timezone.get(), state_one);

  base::Time monday_time_window_limit_start =
      TimeFromString("Mon, 1 Jan 2018 21:00");

  State expected_state_two;
  expected_state_two.is_locked = true;
  expected_state_two.active_policy = ActivePolicies::kUsageLimit;
  expected_state_two.is_time_usage_limit_enabled = true;
  expected_state_two.remaining_usage = base::TimeDelta::FromMinutes(0);
  expected_state_two.time_usage_limit_started = time_two;
  expected_state_two.next_state_change_time = monday_time_window_limit_start;
  expected_state_two.next_state_active_policy = ActivePolicies::kFixedLimit;
  expected_state_two.next_unlock_time = TimeFromString("Tue, 2 Jan 2018 8:30");
  expected_state_two.last_state_changed = time_two;

  AssertEqState(expected_state_two, state_two);

  // Check state during time window limit and time usage limit enforced.
  base::Time time_three = TimeFromString("Mon, 1 Jan 2018 21:00");
  State state_three =
      GetState(time_limit_dictionary, base::TimeDelta::FromMinutes(120),
               time_three, time_three, timezone.get(), state_two);

  State expected_state_three;
  expected_state_three.is_locked = true;
  expected_state_three.active_policy = ActivePolicies::kFixedLimit;
  expected_state_three.is_time_usage_limit_enabled = true;
  expected_state_three.remaining_usage = base::TimeDelta::FromMinutes(0);
  expected_state_three.time_usage_limit_started = time_two;
  expected_state_three.next_state_change_time =
      TimeFromString("Tue, 2 Jan 2018 8:30");
  expected_state_three.next_state_active_policy =
      ActivePolicies::kNoActivePolicy;
  expected_state_three.next_unlock_time =
      TimeFromString("Tue, 2 Jan 2018 8:30");
  expected_state_three.last_state_changed = time_three;

  AssertEqState(expected_state_three, state_three);

  // Check state after time usage limit reset and window limit end.
  base::Time time_four = TimeFromString("Fri, 5 Jan 2018 8:30");
  State state_four =
      GetState(time_limit_dictionary, base::TimeDelta::FromMinutes(120),
               time_four, time_four, timezone.get(), state_three);

  State expected_state_four;
  expected_state_four.is_locked = false;
  expected_state_four.active_policy = ActivePolicies::kNoActivePolicy;
  expected_state_four.is_time_usage_limit_enabled = false;
  expected_state_four.next_state_change_time =
      TimeFromString("Fri, 5 Jan 2018 21:00");
  expected_state_four.next_state_active_policy = ActivePolicies::kFixedLimit;
  expected_state_four.last_state_changed = time_four;

  AssertEqState(expected_state_four, state_four);
}

// Check GetState when a lock override is active.
TEST_F(UsageTimeLimitProcessorTest, GetStateWithOverrideLock) {
  std::unique_ptr<icu::TimeZone> timezone(icu::TimeZone::createTimeZone("GMT"));

  std::string created_at = CreatePolicyTimestamp("1 Jan 2018 15:00");
  base::Value override = base::Value(base::Value::Type::DICTIONARY);
  override.SetKey("action", base::Value("LOCK"));
  override.SetKey("created_at_millis", base::Value(created_at));
  base::Value overrides(base::Value::Type::LIST);
  overrides.GetList().push_back(std::move(override));

  std::unique_ptr<base::Value> time_limit =
      std::make_unique<base::Value>(base::Value::Type::DICTIONARY);
  time_limit->SetKey("overrides", std::move(overrides));
  std::unique_ptr<base::DictionaryValue> time_limit_dictionary =
      base::DictionaryValue::From(std::move(time_limit));

  base::Time time_one = TimeFromString("Mon, 1 Jan 2018 15:05");
  State state_one =
      GetState(time_limit_dictionary, base::TimeDelta::FromMinutes(0), time_one,
               time_one, timezone.get(), base::nullopt);

  // Check that the device is locked until next morning.
  State expected_state_one;
  expected_state_one.is_locked = true;
  expected_state_one.active_policy = ActivePolicies::kOverride;
  expected_state_one.is_time_usage_limit_enabled = false;
  expected_state_one.next_state_change_time =
      TimeFromString("Tue, 2 Jan 2018 0:00");
  expected_state_one.next_state_active_policy = ActivePolicies::kNoActivePolicy;
  expected_state_one.next_unlock_time = TimeFromString("Tue, 2 Jan 2018 0:00");
  expected_state_one.last_state_changed = base::Time();

  AssertEqState(expected_state_one, state_one);
}

// Test GetState when a overridden time window limit has been updated, so the
// override should not be aplicable anymore.
TEST_F(UsageTimeLimitProcessorTest, GetStateUpdateUnlockedTimeWindowLimit) {
  std::unique_ptr<icu::TimeZone> timezone(
      icu::TimeZone::createTimeZone("GMT+0800"));

  // Setup time window limit.
  std::string last_updated =
      CreatePolicyTimestamp("Mon, 1 Jan 2018 8:00 GMT+0800");
  base::Value monday_time_limit =
      CreateTimeWindow(base::Value("MONDAY"), CreateTime(18, 0),
                       CreateTime(7, 30), base::Value(last_updated));

  base::Value window_limit_entries(base::Value::Type::LIST);
  window_limit_entries.GetList().push_back(std::move(monday_time_limit));

  base::Value time_window_limit(base::Value::Type::DICTIONARY);
  time_window_limit.SetKey("entries", std::move(window_limit_entries));

  // Setup override.
  std::string created_at =
      CreatePolicyTimestamp("Mon, 1 Jan 2018 18:30 GMT+0800");
  base::Value override = base::Value(base::Value::Type::DICTIONARY);
  override.SetKey("action", base::Value("UNLOCK"));
  override.SetKey("created_at_millis", base::Value(created_at));
  base::Value overrides(base::Value::Type::LIST);
  overrides.GetList().push_back(std::move(override));

  std::unique_ptr<base::Value> time_limit =
      std::make_unique<base::Value>(base::Value::Type::DICTIONARY);
  time_limit->SetKey("time_window_limit", std::move(time_window_limit));
  time_limit->SetKey("overrides", std::move(overrides));

  std::unique_ptr<base::DictionaryValue> time_limit_dictionary =
      base::DictionaryValue::From(std::move(time_limit));

  // Check that the override is invalidating the time window limit.
  base::Time time_one = TimeFromString("Mon, 1 Jan 2018 18:35 GMT+0800");
  State state_one =
      GetState(time_limit_dictionary, base::TimeDelta::FromMinutes(120),
               time_one, time_one, timezone.get(), base::nullopt);

  State expected_state_one;
  expected_state_one.is_locked = false;
  expected_state_one.active_policy = ActivePolicies::kOverride;
  expected_state_one.is_time_usage_limit_enabled = false;
  expected_state_one.next_state_change_time =
      TimeFromString("Mon, 8 Jan 2018 18:00 GMT+0800");
  expected_state_one.next_state_active_policy = ActivePolicies::kFixedLimit;
  expected_state_one.last_state_changed = base::Time();

  AssertEqState(expected_state_one, state_one);

  // Change time window limit
  std::string last_updated_two =
      CreatePolicyTimestamp("Mon, 1 Jan 2018 19:00 GMT+0800");
  base::Value monday_time_limit_two =
      CreateTimeWindow(base::Value("MONDAY"), CreateTime(18, 0),
                       CreateTime(8, 0), base::Value(last_updated_two));
  base::Value window_limit_entries_two(base::Value::Type::LIST);
  window_limit_entries_two.GetList().push_back(
      std::move(monday_time_limit_two));
  base::Value time_window_limit_two(base::Value::Type::DICTIONARY);
  time_window_limit_two.SetKey("entries", std::move(window_limit_entries_two));
  time_limit_dictionary->SetKey("time_window_limit",
                                std::move(time_window_limit_two));

  // Check that the new time window limit is enforced.
  base::Time time_two = TimeFromString("Mon, 1 Jan 2018 19:10 GMT+0800");
  State state_two =
      GetState(time_limit_dictionary, base::TimeDelta::FromMinutes(120),
               time_two, time_two, timezone.get(), state_one);

  State expected_state_two;
  expected_state_two.is_locked = true;
  expected_state_two.active_policy = ActivePolicies::kFixedLimit;
  expected_state_two.is_time_usage_limit_enabled = false;
  expected_state_two.next_state_change_time =
      TimeFromString("Tue, 2 Jan 2018 8:00 GMT+0800");
  expected_state_two.next_state_active_policy = ActivePolicies::kNoActivePolicy;
  expected_state_two.next_unlock_time =
      TimeFromString("Tue, 2 Jan 2018 8:00 GMT+0800");
  expected_state_two.last_state_changed = time_two;

  AssertEqState(expected_state_two, state_two);
}

// Make sure that the override will only affect policies that started being
// enforced before it was created.
TEST_F(UsageTimeLimitProcessorTest, GetStateOverrideTimeWindowLimitOnly) {
  std::unique_ptr<icu::TimeZone> timezone(icu::TimeZone::createTimeZone("PST"));

  // Setup time window limit.
  std::string last_updated = CreatePolicyTimestamp("1 Jan 2018 8:00 PST");
  base::Value monday_time_limit =
      CreateTimeWindow(base::Value("MONDAY"), CreateTime(21, 0),
                       CreateTime(10, 0), base::Value(last_updated));

  base::Value window_limit_entries(base::Value::Type::LIST);
  window_limit_entries.GetList().push_back(std::move(monday_time_limit));

  base::Value time_window_limit(base::Value::Type::DICTIONARY);
  time_window_limit.SetKey("entries", std::move(window_limit_entries));

  // Setup time usage limit.
  base::Value monday_time_usage =
      CreateTimeUsage(base::Value(60), base::Value(last_updated));

  base::Value time_usage_limit = base::Value(base::Value::Type::DICTIONARY);
  time_usage_limit.SetKey("monday", std::move(monday_time_usage));
  time_usage_limit.SetKey("reset_at", CreateTime(8, 0));

  // Setup override.
  std::string created_at = CreatePolicyTimestamp("Mon, 1 Jan 2018 22:00 PST");
  base::Value override = base::Value(base::Value::Type::DICTIONARY);
  override.SetKey("action", base::Value("UNLOCK"));
  override.SetKey("created_at_millis", base::Value(created_at));
  base::Value overrides(base::Value::Type::LIST);
  overrides.GetList().push_back(std::move(override));

  // Setup policy.
  std::unique_ptr<base::Value> time_limit =
      std::make_unique<base::Value>(base::Value::Type::DICTIONARY);
  time_limit->SetKey("time_window_limit", std::move(time_window_limit));
  time_limit->SetKey("time_usage_limit", std::move(time_usage_limit));
  time_limit->SetKey("overrides", std::move(overrides));

  std::unique_ptr<base::DictionaryValue> time_limit_dictionary =
      base::DictionaryValue::From(std::move(time_limit));

  // Check that the override is unlocking the device that should be locked with
  // time window limit.
  base::Time time_one = TimeFromString("Mon, 1 Jan 2018 22:10 PST");
  State state_one =
      GetState(time_limit_dictionary, base::TimeDelta::FromMinutes(40),
               time_one, time_one, timezone.get(), base::nullopt);

  State expected_state_one;
  expected_state_one.is_locked = false;
  expected_state_one.active_policy = ActivePolicies::kOverride;
  expected_state_one.is_time_usage_limit_enabled = true;
  expected_state_one.remaining_usage = base::TimeDelta::FromMinutes(20);
  expected_state_one.next_state_change_time =
      TimeFromString("Mon, 1 Jan 2018 22:30 PST");
  expected_state_one.next_state_active_policy = ActivePolicies::kUsageLimit;
  expected_state_one.last_state_changed = base::Time();

  AssertEqState(expected_state_one, state_one);

  // Check that the override didn't unlock the device when the time usage limit
  // started, and that it will be locked until the time usage limit reset time,
  // and not when the time window limit ends.
  base::Time time_two = TimeFromString("Mon, 1 Jan 2018 22:30 PST");
  State state_two =
      GetState(time_limit_dictionary, base::TimeDelta::FromMinutes(60),
               time_two, time_two, timezone.get(), state_one);

  State expected_state_two;
  expected_state_two.is_locked = true;
  expected_state_two.active_policy = ActivePolicies::kUsageLimit;
  expected_state_two.is_time_usage_limit_enabled = true;
  expected_state_two.remaining_usage = base::TimeDelta::FromMinutes(0);
  expected_state_two.time_usage_limit_started = time_two;
  expected_state_two.next_state_change_time =
      TimeFromString("Tue, 2 Jan 2018 8:00 PST");
  expected_state_two.next_state_active_policy = ActivePolicies::kNoActivePolicy;
  expected_state_two.next_unlock_time =
      TimeFromString("Tue, 2 Jan 2018 8:00 PST");
  expected_state_two.last_state_changed = time_two;

  AssertEqState(expected_state_two, state_two);
}

// Test that the override created on the previous day, does not take effect
// after the reset time on the following day.
TEST_F(UsageTimeLimitProcessorTest, GetStateOldLockOverride) {
  std::unique_ptr<icu::TimeZone> timezone(icu::TimeZone::createTimeZone("PST"));

  // Setup time usage limit.
  base::Value time_usage_limit = base::Value(base::Value::Type::DICTIONARY);
  time_usage_limit.SetKey("reset_at", CreateTime(6, 0));

  // Setup override.
  std::string created_at = CreatePolicyTimestamp("Mon, 1 Jan 2018 21:00 PST");
  base::Value override = base::Value(base::Value::Type::DICTIONARY);
  override.SetKey("action", base::Value("LOCK"));
  override.SetKey("created_at_millis", base::Value(created_at));
  base::Value overrides(base::Value::Type::LIST);
  overrides.GetList().push_back(std::move(override));

  // Setup policy.
  std::unique_ptr<base::Value> time_limit =
      std::make_unique<base::Value>(base::Value::Type::DICTIONARY);
  time_limit->SetKey("time_usage_limit", std::move(time_usage_limit));
  time_limit->SetKey("overrides", std::move(overrides));

  std::unique_ptr<base::DictionaryValue> time_limit_dictionary =
      base::DictionaryValue::From(std::move(time_limit));

  // Check that the device is locked because of the override.
  base::Time time_one = TimeFromString("Mon, 1 Jan 2018 21:00 PST");
  State state_one =
      GetState(time_limit_dictionary, base::TimeDelta::FromMinutes(40),
               time_one, time_one, timezone.get(), base::nullopt);

  State expected_state_one;
  expected_state_one.is_locked = true;
  expected_state_one.active_policy = ActivePolicies::kOverride;
  expected_state_one.is_time_usage_limit_enabled = false;
  expected_state_one.next_state_change_time =
      TimeFromString("Tue, 2 Jan 2018 6:00 PST");
  expected_state_one.next_state_active_policy = ActivePolicies::kNoActivePolicy;
  expected_state_one.next_unlock_time =
      TimeFromString("Tue, 2 Jan 2018 6:00 PST");
  expected_state_one.last_state_changed = base::Time();

  AssertEqState(expected_state_one, state_one);

  // Check that the device is still locked after midnight.
  base::Time time_two = TimeFromString("Tue, 2 Jan 2018 1:00 PST");
  State state_two =
      GetState(time_limit_dictionary, base::TimeDelta::FromMinutes(0), time_two,
               time_two, timezone.get(), state_one);

  State expected_state_two;
  expected_state_two.is_locked = true;
  expected_state_two.active_policy = ActivePolicies::kOverride;
  expected_state_two.is_time_usage_limit_enabled = false;
  expected_state_two.next_state_change_time =
      TimeFromString("Tue, 2 Jan 2018 6:00 PST");
  expected_state_two.next_state_active_policy = ActivePolicies::kNoActivePolicy;
  expected_state_two.next_unlock_time =
      TimeFromString("Tue, 2 Jan 2018 6:00 PST");
  expected_state_two.last_state_changed = base::Time();

  AssertEqState(expected_state_two, state_two);

  // Check that the device is unlocked.
  base::Time time_three = TimeFromString("Tue, 2 Jan 2018 6:00 PST");
  State state_three =
      GetState(time_limit_dictionary, base::TimeDelta::FromMinutes(0),
               time_three, time_three, timezone.get(), state_two);

  State expected_state_three;
  expected_state_three.is_locked = false;
  expected_state_three.active_policy = ActivePolicies::kNoActivePolicy;
  expected_state_three.is_time_usage_limit_enabled = false;
  expected_state_three.next_state_change_time = base::Time();
  expected_state_three.next_state_active_policy =
      ActivePolicies::kNoActivePolicy;
  expected_state_three.next_unlock_time = base::Time();
  expected_state_three.last_state_changed = time_three;

  AssertEqState(expected_state_three, state_three);
}

// Make sure that the default bedtime is correctly processed
TEST_F(UsageTimeLimitProcessorTest, GetStateDefaultBedtime) {
  std::unique_ptr<icu::TimeZone> timezone(icu::TimeZone::createTimeZone("PST"));

  // Setup time window limit.
  std::string last_updated = CreatePolicyTimestamp("1 Jan 2018 8:00 PST");
  base::Value monday_time_limit =
      CreateTimeWindow(base::Value("MONDAY"), CreateTime(21, 0),
                       CreateTime(7, 0), base::Value(last_updated));
  base::Value tuesday_time_limit =
      CreateTimeWindow(base::Value("TUESDAY"), CreateTime(21, 0),
                       CreateTime(7, 0), base::Value(last_updated));
  base::Value wednesday_time_limit =
      CreateTimeWindow(base::Value("WEDNESDAY"), CreateTime(21, 0),
                       CreateTime(7, 0), base::Value(last_updated));
  base::Value thursday_time_limit =
      CreateTimeWindow(base::Value("THURSDAY"), CreateTime(21, 0),
                       CreateTime(7, 0), base::Value(last_updated));
  base::Value friday_time_limit =
      CreateTimeWindow(base::Value("FRIDAY"), CreateTime(21, 0),
                       CreateTime(7, 0), base::Value(last_updated));
  base::Value saturday_time_limit =
      CreateTimeWindow(base::Value("SATURDAY"), CreateTime(21, 0),
                       CreateTime(7, 0), base::Value(last_updated));
  base::Value sunday_time_limit =
      CreateTimeWindow(base::Value("SUNDAY"), CreateTime(21, 0),
                       CreateTime(7, 0), base::Value(last_updated));

  base::Value window_limit_entries(base::Value::Type::LIST);
  window_limit_entries.GetList().push_back(std::move(monday_time_limit));
  window_limit_entries.GetList().push_back(std::move(tuesday_time_limit));
  window_limit_entries.GetList().push_back(std::move(wednesday_time_limit));
  window_limit_entries.GetList().push_back(std::move(thursday_time_limit));
  window_limit_entries.GetList().push_back(std::move(friday_time_limit));
  window_limit_entries.GetList().push_back(std::move(saturday_time_limit));
  window_limit_entries.GetList().push_back(std::move(sunday_time_limit));

  base::Value time_window_limit(base::Value::Type::DICTIONARY);
  time_window_limit.SetKey("entries", std::move(window_limit_entries));

  base::Value time_usage_limit = base::Value(base::Value::Type::DICTIONARY);
  time_usage_limit.SetKey("reset_at", CreateTime(6, 0));

  // Setup policy.
  std::unique_ptr<base::Value> time_limit =
      std::make_unique<base::Value>(base::Value::Type::DICTIONARY);
  time_limit->SetKey("time_window_limit", std::move(time_window_limit));
  time_limit->SetKey("time_usage_limit", std::move(time_usage_limit));

  std::unique_ptr<base::DictionaryValue> time_limit_dictionary =
      base::DictionaryValue::From(std::move(time_limit));

  // Check that the device is locked, and that it should unlock on the next
  // morning.
  base::Time time_one = TimeFromString("Mon, 1 Jan 2018 22:00 PST");
  State state_one =
      GetState(time_limit_dictionary, base::TimeDelta::FromMinutes(40),
               time_one, time_one, timezone.get(), base::nullopt);

  State expected_state_one;
  expected_state_one.is_locked = true;
  expected_state_one.active_policy = ActivePolicies::kFixedLimit;
  expected_state_one.is_time_usage_limit_enabled = false;
  expected_state_one.remaining_usage = base::TimeDelta::FromMinutes(0);
  expected_state_one.next_state_change_time =
      TimeFromString("Tue, 2 Jan 2018 7:00 PST");
  expected_state_one.next_state_active_policy = ActivePolicies::kNoActivePolicy;
  expected_state_one.next_unlock_time =
      TimeFromString("Tue, 2 Jan 2018 7:00 PST");
  expected_state_one.last_state_changed = base::Time();

  AssertEqState(expected_state_one, state_one);
}

// Test GetExpectedResetTime with an empty policy.
TEST_F(UsageTimeLimitProcessorTest, GetExpectedResetTimeWithEmptyPolicy) {
  std::unique_ptr<icu::TimeZone> timezone(icu::TimeZone::createTimeZone("GMT"));

  // Setup policy.
  std::unique_ptr<base::Value> time_limit =
      std::make_unique<base::Value>(base::Value::Type::DICTIONARY);
  std::unique_ptr<base::DictionaryValue> time_limit_dictionary =
      base::DictionaryValue::From(std::move(time_limit));

  base::Time time_one = TimeFromString("Mon, 1 Jan 2018 22:00");
  base::Time reset_time =
      GetExpectedResetTime(time_limit_dictionary, time_one, timezone.get());

  ASSERT_EQ(reset_time, TimeFromString("Tue, 2 Jan 2018 0:00"));
}

// Test GetExpectedResetTime with a custom time usage limit reset time.
TEST_F(UsageTimeLimitProcessorTest, GetExpectedResetTimeWithCustomPolicy) {
  std::unique_ptr<icu::TimeZone> timezone(icu::TimeZone::createTimeZone("EST"));

  // Setup time usage limit.
  base::Value time_usage_limit = base::Value(base::Value::Type::DICTIONARY);
  time_usage_limit.SetKey("reset_at", CreateTime(8, 0));

  // Setup policy.
  std::unique_ptr<base::Value> time_limit =
      std::make_unique<base::Value>(base::Value::Type::DICTIONARY);
  time_limit->SetKey("time_usage_limit", std::move(time_usage_limit));
  std::unique_ptr<base::DictionaryValue> time_limit_dictionary =
      base::DictionaryValue::From(std::move(time_limit));

  // Check that it resets in the same day.
  base::Time time_one = TimeFromString("Tue, 2 Jan 2018 6:00 EST");
  base::Time reset_time_one =
      GetExpectedResetTime(time_limit_dictionary, time_one, timezone.get());

  ASSERT_EQ(reset_time_one, TimeFromString("Tue, 2 Jan 2018 8:00 EST"));

  // Checks that it resets on the following day.
  base::Time time_two = TimeFromString("Tue, 2 Jan 2018 10:00 EST");
  base::Time reset_time_two =
      GetExpectedResetTime(time_limit_dictionary, time_two, timezone.get());

  ASSERT_EQ(reset_time_two, TimeFromString("Wed, 3 Jan 2018 8:00 EST"));
}

TEST_F(UsageTimeLimitProcessorTest, GetTimeUsageLimitResetTime) {
  // If there is no valid time usage limit in the policy, default value
  // (midnight) should be returned.
  auto empty_time_limit =
      std::make_unique<base::Value>(base::Value::Type::DICTIONARY);
  auto empty_time_limit_dictionary =
      base::DictionaryValue::From(std::move(empty_time_limit));

  EXPECT_EQ(base::TimeDelta::FromHours(0),
            GetTimeUsageLimitResetTime(empty_time_limit_dictionary));

  // If reset time is specified in the time usage limit policy, its value should
  // be returned.
  const int kHour = 8;
  const int kMinutes = 30;
  auto time_usage_limit = base::Value(base::Value::Type::DICTIONARY);
  time_usage_limit.SetKey("reset_at", CreateTime(kHour, kMinutes));
  auto time_limit =
      std::make_unique<base::Value>(base::Value::Type::DICTIONARY);
  time_limit->SetKey("time_usage_limit", std::move(time_usage_limit));
  auto time_limit_dictionary =
      base::DictionaryValue::From(std::move(time_limit));

  EXPECT_EQ(base::TimeDelta::FromHours(kHour) +
                base::TimeDelta::FromMinutes(kMinutes),
            GetTimeUsageLimitResetTime(time_limit_dictionary));
}

}  // namespace usage_time_limit
}  // namespace chromeos
