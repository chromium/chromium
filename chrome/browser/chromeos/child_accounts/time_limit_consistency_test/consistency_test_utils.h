// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Utilities to be used by the consistency golden converter unit tests.

#ifndef CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_TIME_LIMIT_CONSISTENCY_TEST_CONSISTENCY_TEST_UTILS_H_
#define CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_TIME_LIMIT_CONSISTENCY_TEST_CONSISTENCY_TEST_UTILS_H_

#include "base/optional.h"
#include "chrome/browser/chromeos/child_accounts/time_limit_consistency_test/goldens/consistency_golden.pb.h"

namespace chromeos {
namespace time_limit_consistency_utils {

// A time of day composed of hours and minutes, used when generating bedtime
// entries.
struct TimeOfDay {
  int hour;
  int minute;
};

// Adds a time window limit entry to the provided ConsistencyGoldenInput.
void AddWindowLimitEntryToGoldenInput(
    time_limit_consistency::ConsistencyGoldenInput* golden_input,
    time_limit_consistency::ConsistencyGoldenEffectiveDay effective_day,
    const TimeOfDay& starts_at,
    const TimeOfDay& ends_at,
    base::Optional<int64_t> last_updated);

// Adds a usage limit entry to the provided ConsistencyGoldenInput.
void AddUsageLimitEntryToGoldenInput(
    time_limit_consistency::ConsistencyGoldenInput* golden_input,
    time_limit_consistency::ConsistencyGoldenEffectiveDay effective_day,
    int usage_quota_mins,
    base::Optional<int64_t> last_updated);

// Adds an override to the provided ConsistencyGoldenInput. Must not be used
// for UNLOCK_UNTIL_LOCK_DEADLINE actions (will DCHECK()), use
// AddTimedOverrideToGoldenInput() instead.
void AddOverrideToGoldenInput(
    time_limit_consistency::ConsistencyGoldenInput* golden_input,
    time_limit_consistency::ConsistencyGoldenOverrideAction action,
    int64_t created_at);

// Adds a timed override (UNLOCK_UNTIL_LOCK_DEADLINE action) with duration set
// to |duration_millis| to the provided ConsistencyGoldenInput.
void AddTimedOverrideToGoldenInput(
    time_limit_consistency::ConsistencyGoldenInput* golden_input,
    int64_t duration_millis,
    int64_t created_at);

}  // namespace time_limit_consistency_utils
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_TIME_LIMIT_CONSISTENCY_TEST_CONSISTENCY_TEST_UTILS_H_
