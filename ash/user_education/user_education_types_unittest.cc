// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/user_education/user_education_types.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

// UserEducationEnumTest -------------------------------------------------------

// Base class of tests that verify all valid enum values and no others are
// included in the relevant `base::EnumSet`s.
using UserEducationEnumTest = testing::Test;

// Tests -----------------------------------------------------------------------

TEST_F(UserEducationEnumTest, AllTimeBuckets) {
  // If a value in `TimeBucket` is added or deprecated, the below switch
  // statement must be modified accordingly. It should be a canonical list of
  // what values are considered valid.
  for (auto bucket : base::EnumSet<TimeBucket, TimeBucket::kMinValue,
                                   TimeBucket::kMaxValue>::All()) {
    bool should_exist_in_all_set = false;

    switch (bucket) {
      case TimeBucket::kOneMinute:
      case TimeBucket::kTenMinutes:
      case TimeBucket::kOneHour:
      case TimeBucket::kOneDay:
      case TimeBucket::kOneWeek:
      case TimeBucket::kTwoWeeks:
      case TimeBucket::kOverTwoWeeks:
        should_exist_in_all_set = true;
    }

    EXPECT_EQ(kAllTimeBucketsSet.Has(bucket), should_exist_in_all_set);
  }
}

}  // namespace ash
