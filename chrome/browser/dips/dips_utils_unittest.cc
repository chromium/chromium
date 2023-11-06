// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dips/dips_utils.h"

#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Eq;
using ::testing::Pair;

TEST(TimestampRangeTest, UpdateTimestampRangeEmpty) {
  const base::Time time = base::Time::FromSecondsSinceUnixEpoch(1);

  TimestampRange range;
  EXPECT_TRUE(UpdateTimestampRange(range, time));
  EXPECT_EQ(range.value(), std::make_pair(time, time));
}

TEST(TimestampRangeTest, UpdateTimestampRange_SetLast) {
  const base::Time time1 = base::Time::FromSecondsSinceUnixEpoch(1);
  const base::Time time2 = base::Time::FromSecondsSinceUnixEpoch(2);
  const base::Time time3 = base::Time::FromSecondsSinceUnixEpoch(3);

  TimestampRange range = {{time1, time2}};
  EXPECT_TRUE(UpdateTimestampRange(range, time3));
  EXPECT_EQ(range.value(), std::make_pair(time1, time3));
}

TEST(TimestampRangeTest, UpdateTimestampRange_SetFirst) {
  const base::Time time1 = base::Time::FromSecondsSinceUnixEpoch(1);
  const base::Time time2 = base::Time::FromSecondsSinceUnixEpoch(2);
  const base::Time time3 = base::Time::FromSecondsSinceUnixEpoch(3);

  TimestampRange range = {{time2, time3}};
  EXPECT_TRUE(UpdateTimestampRange(range, time1));
  EXPECT_EQ(range.value(), std::make_pair(time1, time3));
}

TEST(TimestampRangeTest, UpdateTimestampRange_Unmodified) {
  const base::Time time1 = base::Time::FromSecondsSinceUnixEpoch(1);
  const base::Time time2 = base::Time::FromSecondsSinceUnixEpoch(2);
  const base::Time time3 = base::Time::FromSecondsSinceUnixEpoch(3);

  TimestampRange range = {{time1, time3}};
  EXPECT_FALSE(UpdateTimestampRange(range, time2));
  EXPECT_EQ(range.value(), std::make_pair(time1, time3));
}

TEST(TimestampRangeTest, IsNullOrWithin_BothEmpty) {
  EXPECT_TRUE(IsNullOrWithin(/*inner=*/{}, /*outer=*/{}));
}

TEST(TimestampRangeTest, IsNullOrWithin_NothingWithinEmptyOuter) {
  TimestampRange inner = {{base::Time::FromSecondsSinceUnixEpoch(1),
                           base::Time::FromSecondsSinceUnixEpoch(1)}};
  TimestampRange outer = {};
  EXPECT_FALSE(IsNullOrWithin(inner, outer));
}

TEST(TimestampRangeTest, IsNullOrWithin_EmptyInnerWithin) {
  TimestampRange inner = {};
  TimestampRange outer = {{base::Time::FromSecondsSinceUnixEpoch(1),
                           base::Time::FromSecondsSinceUnixEpoch(1)}};
  EXPECT_TRUE(IsNullOrWithin(inner, outer));
}

TEST(TimestampRangeTest, IsNullOrWithin_ChecksLowerBound) {
  TimestampRange outer = {{base::Time::FromSecondsSinceUnixEpoch(2),
                           base::Time::FromSecondsSinceUnixEpoch(5)}};
  TimestampRange starts_on_time = {{base::Time::FromSecondsSinceUnixEpoch(3),
                                    base::Time::FromSecondsSinceUnixEpoch(4)}};
  TimestampRange starts_too_early = {
      {base::Time::FromSecondsSinceUnixEpoch(1),
       base::Time::FromSecondsSinceUnixEpoch(4)}};

  EXPECT_FALSE(IsNullOrWithin(starts_too_early, outer));
  EXPECT_TRUE(IsNullOrWithin(starts_on_time, outer));
}

TEST(TimestampRangeTest, IsNullOrWithin_ChecksUpperBound) {
  TimestampRange outer = {{base::Time::FromSecondsSinceUnixEpoch(2),
                           base::Time::FromSecondsSinceUnixEpoch(5)}};
  TimestampRange ends_in_time = {{base::Time::FromSecondsSinceUnixEpoch(3),
                                  base::Time::FromSecondsSinceUnixEpoch(4)}};
  TimestampRange ends_too_late = {{base::Time::FromSecondsSinceUnixEpoch(3),
                                   base::Time::FromSecondsSinceUnixEpoch(10)}};

  EXPECT_TRUE(IsNullOrWithin(ends_in_time, outer));
  EXPECT_FALSE(IsNullOrWithin(ends_too_late, outer));
}

TEST(TimestampRangeTest, IsNullOrWithin_AllowsEquals) {
  TimestampRange range = {{base::Time::FromSecondsSinceUnixEpoch(1),
                           base::Time::FromSecondsSinceUnixEpoch(1)}};
  EXPECT_TRUE(IsNullOrWithin(range, range));
}

TEST(BucketizeBounceDelayTest, BucketizeBounceDelay) {
  // any TimeDelta in (-inf, 1s) should return 0
  EXPECT_EQ(0, BucketizeBounceDelay(base::Days(-1)));
  EXPECT_EQ(0, BucketizeBounceDelay(base::Milliseconds(0)));
  EXPECT_EQ(0, BucketizeBounceDelay(base::Milliseconds(999)));
  // anything in [1s, 2s) should return 1
  EXPECT_EQ(1, BucketizeBounceDelay(base::Milliseconds(1000)));
  EXPECT_EQ(1, BucketizeBounceDelay(base::Milliseconds(1999)));
  // similarly for [2s, 3s)
  EXPECT_EQ(2, BucketizeBounceDelay(base::Milliseconds(2000)));
  EXPECT_EQ(2, BucketizeBounceDelay(base::Milliseconds(2999)));
  // ...
  EXPECT_EQ(9, BucketizeBounceDelay(base::Milliseconds(9999)));
  // anything in [10s, inf) should return 10
  EXPECT_EQ(10, BucketizeBounceDelay(base::Milliseconds(10000)));
  EXPECT_EQ(10, BucketizeBounceDelay(base::Milliseconds(10001)));
  EXPECT_EQ(10, BucketizeBounceDelay(base::Days(1)));
}
