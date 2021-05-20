// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/time/time_delta_from_string.h"

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

// Adapted from Abseil's TEST(Duration, ParseDuration):
// https://cs.chromium.org/chromium/src/third_party/abseil-cpp/absl/time/duration_test.cc?l=1660&rcl=93c58ec988d77f4277f9c9d237d3507991fbd719
TEST(TimeDeltaFromStringTest, ParseTimeDeltaTest) {
  // No specified unit. Should only work for zero and infinity.
  EXPECT_EQ(TimeDeltaFromString("0"), TimeDelta());
  EXPECT_EQ(TimeDeltaFromString("+0"), TimeDelta());
  EXPECT_EQ(TimeDeltaFromString("-0"), TimeDelta());

  EXPECT_EQ(TimeDeltaFromString("inf"), TimeDelta::Max());
  EXPECT_EQ(TimeDeltaFromString("+inf"), TimeDelta::Max());
  EXPECT_EQ(TimeDeltaFromString("-inf"), TimeDelta::Min());
  EXPECT_EQ(TimeDeltaFromString("infBlah"), absl::nullopt);

  // Illegal input forms.
  EXPECT_EQ(TimeDeltaFromString(""), absl::nullopt);
  EXPECT_EQ(TimeDeltaFromString("0.0"), absl::nullopt);
  EXPECT_EQ(TimeDeltaFromString(".0"), absl::nullopt);
  EXPECT_EQ(TimeDeltaFromString("."), absl::nullopt);
  EXPECT_EQ(TimeDeltaFromString("01"), absl::nullopt);
  EXPECT_EQ(TimeDeltaFromString("1"), absl::nullopt);
  EXPECT_EQ(TimeDeltaFromString("-1"), absl::nullopt);
  EXPECT_EQ(TimeDeltaFromString("2"), absl::nullopt);
  EXPECT_EQ(TimeDeltaFromString("2 s"), absl::nullopt);
  EXPECT_EQ(TimeDeltaFromString(".s"), absl::nullopt);
  EXPECT_EQ(TimeDeltaFromString("-.s"), absl::nullopt);
  EXPECT_EQ(TimeDeltaFromString("s"), absl::nullopt);
  EXPECT_EQ(TimeDeltaFromString(" 2s"), absl::nullopt);
  EXPECT_EQ(TimeDeltaFromString("2s "), absl::nullopt);
  EXPECT_EQ(TimeDeltaFromString(" 2s "), absl::nullopt);
  EXPECT_EQ(TimeDeltaFromString("2mt"), absl::nullopt);
  EXPECT_EQ(TimeDeltaFromString("1e3s"), absl::nullopt);

  // One unit type.
  EXPECT_EQ(TimeDeltaFromString("1ns"), TimeDelta::FromNanoseconds(1));
  EXPECT_EQ(TimeDeltaFromString("1us"), TimeDelta::FromMicroseconds(1));
  EXPECT_EQ(TimeDeltaFromString("1ms"), TimeDelta::FromMilliseconds(1));
  EXPECT_EQ(TimeDeltaFromString("1s"), TimeDelta::FromSeconds(1));
  EXPECT_EQ(TimeDeltaFromString("2m"), TimeDelta::FromMinutes(2));
  EXPECT_EQ(TimeDeltaFromString("2h"), TimeDelta::FromHours(2));

  // Huge counts of a unit. 9223372036854775807 == 2^63 - 1.
  EXPECT_EQ(TimeDeltaFromString("9223372036854775807us"),
            TimeDelta::FromMicroseconds(9223372036854775807));
  EXPECT_EQ(TimeDeltaFromString("-9223372036854775807us"),
            TimeDelta::FromMicroseconds(-9223372036854775807));

  // Overflow count. Note the "93" at the beginning (instead of "92").
  EXPECT_EQ(TimeDeltaFromString("9323372036854775807us"), absl::nullopt);
  // Overflow overall duration.
  EXPECT_EQ(TimeDeltaFromString("9323372036854s"), TimeDelta::Max());
  EXPECT_EQ(TimeDeltaFromString("-9323372036854s"), TimeDelta::Min());

  // Multiple units.
  EXPECT_EQ(TimeDeltaFromString("2h3m4s"), TimeDelta::FromHours(2) +
                                               TimeDelta::FromMinutes(3) +
                                               TimeDelta::FromSeconds(4));
  EXPECT_EQ(TimeDeltaFromString("3m4s5us"), TimeDelta::FromMinutes(3) +
                                                TimeDelta::FromSeconds(4) +
                                                TimeDelta::FromMicroseconds(5));
  EXPECT_EQ(TimeDeltaFromString("2h3m4s5ms6us7ns"),
            TimeDelta::FromHours(2) + TimeDelta::FromMinutes(3) +
                TimeDelta::FromSeconds(4) + TimeDelta::FromMilliseconds(5) +
                TimeDelta::FromMicroseconds(6) + TimeDelta::FromNanoseconds(7));

  // Multiple units out of order.
  EXPECT_EQ(TimeDeltaFromString("2us3m4s5h"),
            TimeDelta::FromHours(5) + TimeDelta::FromMinutes(3) +
                TimeDelta::FromSeconds(4) + TimeDelta::FromMicroseconds(2));

  // Fractional values of units.
  EXPECT_EQ(TimeDeltaFromString("1.5ns"), 1.5 * TimeDelta::FromNanoseconds(1));
  EXPECT_EQ(TimeDeltaFromString("1.5us"), 1.5 * TimeDelta::FromMicroseconds(1));
  EXPECT_EQ(TimeDeltaFromString("1.5ms"), 1.5 * TimeDelta::FromMilliseconds(1));
  EXPECT_EQ(TimeDeltaFromString("1.5s"), 1.5 * TimeDelta::FromSeconds(1));
  EXPECT_EQ(TimeDeltaFromString("1.5m"), 1.5 * TimeDelta::FromMinutes(1));
  EXPECT_EQ(TimeDeltaFromString("1.5h"), 1.5 * TimeDelta::FromHours(1));

  // Huge fractional counts of a unit.
  EXPECT_EQ(TimeDeltaFromString("0.4294967295s"),
            TimeDelta::FromNanoseconds(429496729) +
                TimeDelta::FromNanoseconds(1) / 2);
  EXPECT_EQ(TimeDeltaFromString("0.429496729501234567890123456789s"),
            TimeDelta::FromNanoseconds(429496729) +
                TimeDelta::FromNanoseconds(1) / 2);

  // Negative durations.
  EXPECT_EQ(TimeDeltaFromString("-1s"), TimeDelta::FromSeconds(-1));
  EXPECT_EQ(TimeDeltaFromString("-1m"), TimeDelta::FromMinutes(-1));
  EXPECT_EQ(TimeDeltaFromString("-1h"), TimeDelta::FromHours(-1));

  EXPECT_EQ(TimeDeltaFromString("-1h2s"),
            -(TimeDelta::FromHours(1) + TimeDelta::FromSeconds(2)));
  EXPECT_EQ(TimeDeltaFromString("1h-2s"), absl::nullopt);
  EXPECT_EQ(TimeDeltaFromString("-1h-2s"), absl::nullopt);
  EXPECT_EQ(TimeDeltaFromString("-1h -2s"), absl::nullopt);
}

}  // namespace

}  // namespace base
