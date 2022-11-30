// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dips/dips_utils.h"

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(TimestampRangeTest, UpdateEmpty) {
  const base::Time time = base::Time::FromDoubleT(1);

  TimestampRange range;
  EXPECT_TRUE(range.Update(time));
  EXPECT_EQ(range.first, time);
  EXPECT_EQ(range.last, time);
}

TEST(TimestampRangeTest, Update_SetLast) {
  const base::Time time1 = base::Time::FromDoubleT(1);
  const base::Time time2 = base::Time::FromDoubleT(2);
  const base::Time time3 = base::Time::FromDoubleT(3);

  TimestampRange range = {time1, time2};
  EXPECT_TRUE(range.Update(time3));
  EXPECT_EQ(range.first, time1);
  EXPECT_EQ(range.last, time3);
}

TEST(TimestampRangeTest, Update_SetFirst) {
  const base::Time time1 = base::Time::FromDoubleT(1);
  const base::Time time2 = base::Time::FromDoubleT(2);
  const base::Time time3 = base::Time::FromDoubleT(3);

  TimestampRange range = {time2, time3};
  EXPECT_TRUE(range.Update(time1));
  EXPECT_EQ(range.first, time1);
  EXPECT_EQ(range.last, time3);
}

TEST(TimestampRangeTest, Update_Unmodified) {
  const base::Time time1 = base::Time::FromDoubleT(1);
  const base::Time time2 = base::Time::FromDoubleT(2);
  const base::Time time3 = base::Time::FromDoubleT(3);

  TimestampRange range = {time1, time3};
  EXPECT_FALSE(range.Update(time2));
  EXPECT_EQ(range.first, time1);
  EXPECT_EQ(range.last, time3);
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
