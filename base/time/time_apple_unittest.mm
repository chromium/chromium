// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/time/time.h"

#include "base/test/gtest_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class ScopedTimebase {
 public:
  explicit ScopedTimebase(mach_timebase_info_data_t timebase) {
    orig_timebase_ = base::TimeTicks::SetMachTimebaseInfoForTesting(timebase);
  }

  ScopedTimebase(const ScopedTimebase&) = delete;

  ScopedTimebase& operator=(const ScopedTimebase&) = delete;

  ~ScopedTimebase() {
    base::TimeTicks::SetMachTimebaseInfoForTesting(orig_timebase_);
  }

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
  const base::Time::Exploded exploded = {
      .year = year, .month = month, .day_of_month = day, .hour = 12};
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
  EXPECT_EQ(Nanoseconds(kArbitraryTicks), result);
}

TEST(TimeMacTest, MachTimeToMicrosecondsM1Timebase) {
  ScopedTimebase timebase(kM1Timebase);

  // Use a tick count that's divisible by 3.
  const uint64_t kArbitraryTicks = 92738127000;
  TimeDelta result = TimeDelta::FromMachTime(kArbitraryTicks);

  const uint64_t kExpectedResult =
      kArbitraryTicks * kM1Timebase.numer / kM1Timebase.denom;
  EXPECT_EQ(Nanoseconds(kExpectedResult), result);
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
  EXPECT_EQ(Nanoseconds(kArbitraryTicks), result);
}

TEST(TimeMacTest, MachTimeToMicrosecondsOverflowDetection) {
  const uint32_t kArbitraryNumer = 1234567;
  ScopedTimebase timebase({kArbitraryNumer, 1});

  // Expect an overflow.
  EXPECT_CHECK_DEATH(
      TimeDelta::FromMachTime(std::numeric_limits<uint64_t>::max()));
}

// Tests that there's no overflow in MachTimeToMicroseconds even with
// std::numeric_limits<uint64_t>::max() ticks on Intel.
TEST(TimeMacTest, MachTimeToMicrosecondsNoOverflowIntel) {
  ScopedTimebase timebase(kIntelTimebase);

  // The incoming Mach time ticks are on the order of nanoseconds while the
  // return result is microseconds. Even though we're passing in the largest
  // tick count the result should be orders of magnitude smaller. On Intel the
  // mapping from ticks to nanoseconds is 1:1 so we wouldn't ever expect an
  // overflow when applying the timebase conversion.
  TimeDelta::FromMachTime(std::numeric_limits<uint64_t>::max());
}

// Tests that there's no overflow in MachTimeToMicroseconds even with
// std::numeric_limits<uint64_t>::max() ticks on M1.
TEST(TimeMacTest, MachTimeToMicrosecondsNoOverflowM1) {
  ScopedTimebase timebase(kM1Timebase);

  // The incoming Mach time ticks are on the order of nanoseconds while the
  // return result is microseconds. Even though we're passing in the largest
  // tick count the result should be orders of magnitude smaller. Expect that
  // FromMachTime(), when applying the timebase conversion, is smart enough to
  // not multiply first and generate an overflow.
  TimeDelta::FromMachTime(std::numeric_limits<uint64_t>::max());
}

// Tests that there's no underflow in MachTimeToMicroseconds on Intel.
TEST(TimeMacTest, MachTimeToMicrosecondsNoUnderflowIntel) {
  ScopedTimebase timebase(kIntelTimebase);

  // On Intel the timebase conversion is 1:1, so min ticks is one microsecond
  // worth of nanoseconds.
  const uint64_t kMinimumTicks = base::Time::kNanosecondsPerMicrosecond;
  const uint64_t kOneMicrosecond = 1;
  EXPECT_EQ(kOneMicrosecond,
            TimeDelta::FromMachTime(kMinimumTicks).InMicroseconds() * 1UL);

  // If we have even one fewer tick (i.e. not enough ticks to constitute a full
  // microsecond) the integer rounding should result in 0 microseconds.
  const uint64_t kZeroMicroseconds = 0;
  EXPECT_EQ(kZeroMicroseconds,
            TimeDelta::FromMachTime(kMinimumTicks - 1).InMicroseconds() * 1UL);
}

// Tests that there's no underflow in MachTimeToMicroseconds for M1.
TEST(TimeMacTest, MachTimeToMicrosecondsNoUnderflowM1) {
  ScopedTimebase timebase(kM1Timebase);

  // Microseconds is mach_time multiplied by kM1Timebase.numer /
  // (kM1Timebase.denom * base::Time::kNanosecondsPerMicrosecond). Inverting
  // that should be the minimum number of ticks to get a single microsecond in
  // return. If we get zero it means an underflow in the conversion. For example
  // if FromMachTime() first divides mach_time by kM1Timebase.denom *
  // base::Time::kNanosecondsPerMicrosecond we'll get zero back.
  const uint64_t kMinimumTicks =
      (kM1Timebase.denom * base::Time::kNanosecondsPerMicrosecond) /
      kM1Timebase.numer;
  const uint64_t kOneMicrosecond = 1;
  EXPECT_EQ(kOneMicrosecond,
            TimeDelta::FromMachTime(kMinimumTicks).InMicroseconds() * 1UL);

  // If we have even one fewer tick (i.e. not enough ticks to constitute a full
  // microsecond) the integer rounding should result in 0 microseconds.
  const uint64_t kZeroMicroseconds = 0;
  EXPECT_EQ(kZeroMicroseconds,
            TimeDelta::FromMachTime(kMinimumTicks - 1).InMicroseconds() * 1UL);
}

TEST(TimeMacTest, MicrosecondsToMachTimeIntelTimebase) {
  ScopedTimebase timebase(kIntelTimebase);

  EXPECT_EQ(0u, TimeDelta().ToMachTime());
  EXPECT_EQ(0u, TimeTicks().ToMachAbsoluteTime());

  constexpr int64_t kArbitraryMicroseconds = 59090101;
  constexpr uint64_t kExpectedTicks =
      kArbitraryMicroseconds * Time::kNanosecondsPerMicrosecond;
  EXPECT_EQ(kExpectedTicks, Microseconds(kArbitraryMicroseconds).ToMachTime());

  const TimeTicks ticks = TimeTicks() + Microseconds(kArbitraryMicroseconds);
  EXPECT_EQ(kExpectedTicks, ticks.ToMachAbsoluteTime());
  EXPECT_EQ(ticks, TimeTicks::FromMachAbsoluteTime(ticks.ToMachAbsoluteTime()));
}

TEST(TimeMacTest, MicrosecondsToMachTimeM1Timebase) {
  ScopedTimebase timebase(kM1Timebase);

  EXPECT_EQ(0u, TimeDelta().ToMachTime());
  EXPECT_EQ(0u, TimeTicks().ToMachAbsoluteTime());

  // 1 microsecond at 24 MHz (numer = 125, denom = 3) is 24 Mach ticks.
  EXPECT_EQ(24u, Microseconds(1).ToMachTime());

  constexpr int64_t kArbitraryMicroseconds = 3864088625;
  const uint64_t kExpectedTicks =
      (static_cast<uint64_t>(kArbitraryMicroseconds) *
       Time::kNanosecondsPerMicrosecond * kM1Timebase.denom) /
      kM1Timebase.numer;
  EXPECT_EQ(kExpectedTicks, Microseconds(kArbitraryMicroseconds).ToMachTime());

  const TimeTicks ticks = TimeTicks() + Microseconds(kArbitraryMicroseconds);
  EXPECT_EQ(kExpectedTicks, ticks.ToMachAbsoluteTime());
  EXPECT_EQ(ticks, TimeTicks::FromMachAbsoluteTime(ticks.ToMachAbsoluteTime()));
}

TEST(TimeMacTest, MicrosecondsToMachTimeEqualTimebaseMembers) {
  ScopedTimebase timebase({UINT_MAX, UINT_MAX});

  constexpr int64_t kArbitraryMicroseconds = 175920053;
  EXPECT_EQ(static_cast<uint64_t>(kArbitraryMicroseconds) *
                Time::kNanosecondsPerMicrosecond,
            (TimeTicks() + Microseconds(kArbitraryMicroseconds))
                .ToMachAbsoluteTime());
}

TEST(TimeMacTest, MicrosecondsToMachTimeNoIntermediateOverflow) {
  // With 1 tick == 1 microsecond (numer = 1000, denom = 1), FiniteMax() fits
  // in uint64_t even though us * 1000 alone would overflow 64 bits.
  ScopedTimebase timebase({Time::kNanosecondsPerMicrosecond, 1});
  const TimeDelta max_finite = TimeDelta::FiniteMax();
  EXPECT_EQ(static_cast<uint64_t>(max_finite.InMicroseconds()),
            max_finite.ToMachTime());
}

TEST(TimeMacTest, RoundTripMachTime) {
  {
    ScopedTimebase timebase(kIntelTimebase);
    constexpr int64_t kUs = 59090101;
    const TimeDelta delta = Microseconds(kUs);
    const TimeTicks ticks = TimeTicks() + delta;
    EXPECT_EQ(delta, TimeDelta::FromMachTime(delta.ToMachTime()));
    EXPECT_EQ(ticks,
              TimeTicks::FromMachAbsoluteTime(ticks.ToMachAbsoluteTime()));

    // Microsecond-aligned Mach ticks (1000 ticks/us) round-trip exactly;
    // sub-microsecond ticks truncate to the microsecond boundary.
    constexpr uint64_t kAlignedMach = 59090101000;
    EXPECT_EQ(kAlignedMach, TimeDelta::FromMachTime(kAlignedMach).ToMachTime());
    EXPECT_EQ(
        kAlignedMach,
        TimeTicks::FromMachAbsoluteTime(kAlignedMach).ToMachAbsoluteTime());
    EXPECT_EQ(kAlignedMach, TimeTicks::FromMachAbsoluteTime(kAlignedMach + 999)
                                .ToMachAbsoluteTime());
  }

  {
    ScopedTimebase timebase(kM1Timebase);
    constexpr int64_t kUs = 3864088625;
    const TimeDelta delta = Microseconds(kUs);
    const TimeTicks ticks = TimeTicks() + delta;
    EXPECT_EQ(delta, TimeDelta::FromMachTime(delta.ToMachTime()));
    EXPECT_EQ(ticks,
              TimeTicks::FromMachAbsoluteTime(ticks.ToMachAbsoluteTime()));

    // At 24 MHz (24 ticks/us), multiples of 24 round-trip exactly and
    // sub-microsecond remainders (< 24 ticks) truncate to the microsecond.
    constexpr uint64_t kAlignedMach = 92738127000;  // 3864088625 * 24
    EXPECT_EQ(kAlignedMach, TimeDelta::FromMachTime(kAlignedMach).ToMachTime());
    EXPECT_EQ(
        kAlignedMach,
        TimeTicks::FromMachAbsoluteTime(kAlignedMach).ToMachAbsoluteTime());
    EXPECT_EQ(kAlignedMach, TimeTicks::FromMachAbsoluteTime(kAlignedMach + 23)
                                .ToMachAbsoluteTime());
  }

  const TimeTicks now = TimeTicks::Now();
  EXPECT_EQ(now, TimeTicks::FromMachAbsoluteTime(now.ToMachAbsoluteTime()));
}

}  // namespace
}  // namespace base
