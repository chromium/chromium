// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Helper functions to create and modify Usage Time Limit policies.

#ifndef CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_TIME_LIMIT_TEST_UTILS_H_
#define CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_TIME_LIMIT_TEST_UTILS_H_

#include <memory>
#include <string>

#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/chromeos/child_accounts/time_limit_override.h"

namespace chromeos {
namespace time_limit_test_utils {

// Days of the week that should be used to create the Time Limit policy.
extern const char kMonday[];
extern const char kTuesday[];
extern const char kWednesday[];
extern const char kThursday[];
extern const char kFriday[];
extern const char kSaturday[];
extern const char kSunday[];

// Parses a string time to a base::Time object, see
// |base::Time::FromUTCString| for compatible input formats.
base::Time TimeFromString(const char* time_string);

// Creates a timestamp with the correct format that is used in the Time Limit
// policy. See |base::Time::FromUTCString| for compatible input formats.
std::string CreatePolicyTimestamp(const char* time_string);
std::string CreatePolicyTimestamp(base::Time time);

// Creates a TimeDelta representing an hour in a day. This representation is
// widely used on the Time Limit policy.
base::TimeDelta CreateTime(int hour, int minute);

// Creates a time object with the correct format that is used in the Time Limit
// policy.
base::Value CreatePolicyTime(base::TimeDelta time);

// Creates a time window limit dictionary used in the Time Limit policy.
base::Value CreateTimeWindow(const std::string& day,
                             base::TimeDelta start_time,
                             base::TimeDelta end_time,
                             base::Time last_updated);

// Creates a time usage limit dictionary used in the Time Limit policy.
base::Value CreateTimeUsage(base::TimeDelta usage_quota,
                            base::Time last_updated);

// Creates dictionary with a minimalist Time Limit policy, containing only the
// time usage limit reset time.
base::Value CreateTimeLimitPolicy(base::TimeDelta reset_time);

// Adds a time usage limit dictionary to the provided Time Limit policy.
// |policy| needs to be a dictionary.
void AddTimeUsageLimit(base::Value* policy,
                       std::string day,
                       base::TimeDelta quota,
                       base::Time last_updated);

// Adds a time window limit dictionary to the provided Time Limit policy.
// |policy| needs to be a dictionary.
void AddTimeWindowLimit(base::Value* policy,
                        const std::string& day,
                        base::TimeDelta start_time,
                        base::TimeDelta end_time,
                        base::Time last_updated);

// Adds a time limit override dictionary to the provided Time Limit policy.
// |policy| needs to be a dictionary.
void AddOverride(base::Value* policy,
                 usage_time_limit::TimeLimitOverride::Action action,
                 base::Time created_at);

// Adds a time limit override with duration dictionary to the provided
// Time Limit policy. |policy| needs to be a dictionary.
void AddOverrideWithDuration(base::Value* policy,
                             usage_time_limit::TimeLimitOverride::Action action,
                             base::Time created_at,
                             base::TimeDelta duration);

// Converts the Time Limit policy to a string. |policy| needs to be a
// dictionary.
std::string PolicyToString(const base::Value& policy);

}  // namespace time_limit_test_utils
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_TIME_LIMIT_TEST_UTILS_H_
