// Copyright 2021 The Chromium Authors
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
  EXPECT_EQ(TimeDeltaFromString("infBlah"), std::nullopt);

  // Illegal input forms.
  EXPECT_EQ(TimeDeltaFromString(""), std::nullopt);
  EXPECT_EQ(TimeDeltaFromString("0.0"), std::nullopt);
  EXPECT_EQ(TimeDeltaFromString(".0"), std::nullopt);
  EXPECT_EQ(TimeDeltaFromString("."), std::nullopt);
  EXPECT_EQ(TimeDeltaFromString("01"), std::nullopt);
  EXPECT_EQ(TimeDeltaFromString("1"), std::nullopt);
  EXPECT_EQ(TimeDeltaFromString("-1"), std::nullopt);
  EXPECT_EQ(TimeDeltaFromString("2"), std::nullopt);
  EXPECT_EQ(TimeDeltaFromString("2 s"), std::nullopt);
  EXPECT_EQ(TimeDeltaFromString(".s"), std::nullopt);
  EXPECT_EQ(TimeDeltaFromString("-.s"), std::nullopt);
  EXPECT_EQ(TimeDeltaFromString("s"), std::nullopt);
  EXPECT_EQ(TimeDeltaFromString(" 2s"), std::nullopt);
  EXPECT_EQ(TimeDeltaFromString("2s "), std::nullopt);
  EXPECT_EQ(TimeDeltaFromString(" 2s "), std::nullopt);
  EXPECT_EQ(TimeDeltaFromString("2mt"), std::nullopt);
  EXPECT_EQ(TimeDeltaFromString("1e3s"), std::nullopt);

  // One unit type.
  EXPECT_EQ(TimeDeltaFromString("1ns"), Nanoseconds(1));
  EXPECT_EQ(TimeDeltaFromString("1us"), Microseconds(1));
  EXPECT_EQ(TimeDeltaFromString("1ms"), Milliseconds(1));
  EXPECT_EQ(TimeDeltaFromString("1s"), Seconds(1));
  EXPECT_EQ(TimeDeltaFromString("2m"), Minutes(2));
  EXPECT_EQ(TimeDeltaFromString("2h"), Hours(2));
  EXPECT_EQ(TimeDeltaFromString("2d"), Days(2));

  // Huge counts of a unit. 9223372036854775807 == 2^63 - 1.
  EXPECT_EQ(TimeDeltaFromString("9223372036854775807us"),
            Microseconds(9223372036854775807));
  EXPECT_EQ(TimeDeltaFromString("-9223372036854775807us"),
            Microseconds(-9223372036854775807));

  // Overflow count. Note the "93" at the beginning (instead of "92").
  EXPECT_EQ(TimeDeltaFromString("9323372036854775807us"), std::nullopt);
  // Overflow overall duration.
  EXPECT_EQ(TimeDeltaFromString("9323372036854s"), TimeDelta::Max());
  EXPECT_EQ(TimeDeltaFromString("-9323372036854s"), TimeDelta::Min());

  // Multiple units.
  EXPECT_EQ(TimeDeltaFromString("1d2h3m"), Days(1) + Hours(2) + Minutes(3));
  EXPECT_EQ(TimeDeltaFromString("2h3m4s"), Hours(2) + Minutes(3) + Seconds(4));
  EXPECT_EQ(TimeDeltaFromString("3m4s5us"),
            Minutes(3) + Seconds(4) + Microseconds(5));
  EXPECT_EQ(TimeDeltaFromString("2h3m4s5ms6us7ns"),
            Hours(2) + Minutes(3) + Seconds(4) + Milliseconds(5) +
                Microseconds(6) + Nanoseconds(7));

  // Multiple units out of order.
  EXPECT_EQ(TimeDeltaFromString("2us3m4s5h"),
            Hours(5) + Minutes(3) + Seconds(4) + Microseconds(2));

  // Fractional values of units.
  EXPECT_EQ(TimeDeltaFromString("1.5ns"), 1.5 * Nanoseconds(1));
  EXPECT_EQ(TimeDeltaFromString("1.5us"), 1.5 * Microseconds(1));
  EXPECT_EQ(TimeDeltaFromString("1.5ms"), 1.5 * Milliseconds(1));
  EXPECT_EQ(TimeDeltaFromString("1.5s"), 1.5 * Seconds(1));
  EXPECT_EQ(TimeDeltaFromString("1.5m"), 1.5 * Minutes(1));
  EXPECT_EQ(TimeDeltaFromString("1.5h"), 1.5 * Hours(1));
  EXPECT_EQ(TimeDeltaFromString("1.5d"), 1.5 * Days(1));

  // Huge fractional counts of a unit.
  EXPECT_EQ(TimeDeltaFromString("0.4294967295s"),
            Nanoseconds(429496729) + Nanoseconds(1) / 2);
  EXPECT_EQ(TimeDeltaFromString("0.429496729501234567890123456789s"),
            Nanoseconds(429496729) + Nanoseconds(1) / 2);

  // Negative durations.
  EXPECT_EQ(TimeDeltaFromString("-1s"), Seconds(-1));
  EXPECT_EQ(TimeDeltaFromString("-1m"), Minutes(-1));
  EXPECT_EQ(TimeDeltaFromString("-1h"), Hours(-1));
  EXPECT_EQ(TimeDeltaFromString("-1d"), Days(-1));

  EXPECT_EQ(TimeDeltaFromString("-1h2s"), -(Hours(1) + Seconds(2)));
  EXPECT_EQ(TimeDeltaFromString("1h-2s"), std::nullopt);
  EXPECT_EQ(TimeDeltaFromString("-1h-2s"), std::nullopt);
  EXPECT_EQ(TimeDeltaFromString("-1h -2s"), std::nullopt);
}

}  // namespace

}  // namespace base
