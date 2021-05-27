// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/gtest_util.h"
#include "base/time/time.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace {
class ScopedTimebase {
 public:
  ScopedTimebase(mach_timebase_info_data_t timebase)
      : orig_timebase_(*base::TimeTicks::MachTimebaseInfo()) {
    base::TimeTicks::MachTimebaseInfo()->numer = timebase.numer;
    base::TimeTicks::MachTimebaseInfo()->denom = timebase.denom;
  }

  ScopedTimebase(const ScopedTimebase&) = delete;

  ScopedTimebase& operator=(const ScopedTimebase&) = delete;

  ~ScopedTimebase() { *base::TimeTicks::MachTimebaseInfo() = orig_timebase_; }

 private:
  mach_timebase_info_data_t orig_timebase_;
};

mach_timebase_info_data_t kIntelTimebase = {1, 1};

// A sample (not definitive) timebase for M1.
mach_timebase_info_data_t kM1Timebase = {125, 3};

}  // namespace

namespace base {
namespace {

base::Time NoonOnDate(int year, int month, int day) {
  base::Time::Exploded exploded;
  exploded.year = year;
  exploded.month = month;
  exploded.day_of_week = 0;  // Not correct, but FromExploded permits it
  exploded.day_of_month = day;
  exploded.hour = 12;
  exploded.minute = 0;
  exploded.second = 0;
  exploded.millisecond = 0;
  base::Time imploded;
  CHECK(base::Time::FromUTCExploded(exploded, &imploded));
  return imploded;
}

void CheckRoundTrip(int y, int m, int d) {
  base::Time original = NoonOnDate(y, m, d);
  base::Time roundtrip = Time::FromNSDate(original.ToNSDate());
  EXPECT_EQ(original, roundtrip);
}

TEST(TimeMacTest, RoundTripNSDate) {
  CheckRoundTrip(1911, 12, 14);
  CheckRoundTrip(1924, 9, 28);
  CheckRoundTrip(1926, 5, 12);
  CheckRoundTrip(1969, 7, 24);
}

TEST(TimeMacTest, MachTimeToMicrosecondsIntelTimebase) {
  ScopedTimebase timebase(kIntelTimebase);

  // Perform the conversion.
  uint64_t kArbitraryTicks = 59090101000;
  TimeDelta result = TimeDelta::FromMachTime(kArbitraryTicks);

  // With Intel the output should be the input.
  EXPECT_EQ(TimeDelta::FromNanoseconds(kArbitraryTicks), result);
}

TEST(TimeMacTest, MachTimeToMicrosecondsM1Timebase) {
  ScopedTimebase timebase(kM1Timebase);

  // Use a tick count that's divisible by 3.
  const uint64_t kArbitraryTicks = 92738127000;
  TimeDelta result = TimeDelta::FromMachTime(kArbitraryTicks);

  const uint64_t kExpectedResult =
      kArbitraryTicks * kM1Timebase.numer / kM1Timebase.denom;
  EXPECT_EQ(TimeDelta::FromNanoseconds(kExpectedResult), result);
}

// Tests MachTimeToMicroseconds when
// mach_timebase_info_data_t.numer and mach_timebase_info_data_t.denom
// are equal.
TEST(TimeMacTest, MachTimeToMicrosecondsEqualTimebaseMembers) {
  // These members would produce overflow but don't because
  // MachTimeToMicroseconds should skip the timebase conversion
  // when they're equal.
  ScopedTimebase timebase({UINT_MAX, UINT_MAX});

  uint64_t kArbitraryTicks = 175920053729;
  TimeDelta result = TimeDelta::FromMachTime(kArbitraryTicks);

  // With a unity timebase the output should be the input.
  EXPECT_EQ(TimeDelta::FromNanoseconds(kArbitraryTicks), result);
}

TEST(TimeMacTest, MachTimeToMicrosecondsOverflowDetection) {
  const uint32_t kArbitraryNumer = 1234567;
  ScopedTimebase timebase({kArbitraryNumer, 1});

  // Expect an overflow.
  EXPECT_CHECK_DEATH(TimeDelta::FromMachTime(ULLONG_MAX));
}

// Tests that there's no overflow in MachTimeToMicroseconds even with
// ULLONG_MAX ticks on Intel.
TEST(TimeMacTest, MachTimeToMicrosecondsNoOverflowIntel) {
  ScopedTimebase timebase(kIntelTimebase);

  // Don't expect an overflow.
  TimeDelta::FromMachTime(ULLONG_MAX);
}

// Tests that there's no overflow in MachTimeToMicroseconds even with
// ULLONG_MAX ticks on M1.
TEST(TimeMacTest, MachTimeToMicrosecondsNoOverflowM1) {
  ScopedTimebase timebase(kM1Timebase);

  // Don't expect an overflow.
  TimeDelta::FromMachTime(ULLONG_MAX);
}
}  // namespace
}  // namespace base
