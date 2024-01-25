// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/scheduled_feature/schedule_utils.h"

#include <string_view>

#include "ash/public/cpp/schedule_enums.h"
#include "ash/system/time/time_of_day.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Eq;
using ::testing::FieldsAre;

namespace ash::schedule_utils {
namespace {

class ScheduleUtilsTest : public ::testing::Test {
 protected:
  ScheduleUtilsTest() {
    // Pick an arbitrary time to start the clock at for determinism.
    clock_.SetNow(BuildTime("00:00:00"));
  }

  void BuildTimeWithAssert(std::string_view time_of_day, base::Time* output) {
    // The date here is arbitrary.
    ASSERT_TRUE(base::Time::FromString(
        base::StrCat({"23 Dec 2021 ", time_of_day}).c_str(), output));
  }

  base::Time BuildTime(std::string_view time_of_day) {
    base::Time output;
    BuildTimeWithAssert(time_of_day, &output);
    return output;
  }

  void AdvanceClockTo(std::string_view time_of_day) {
    const base::Time target_time = BuildTime(time_of_day);
    const base::TimeDelta target_time_adjustment =
        (target_time - clock_.Now()).FloorToMultiple(base::Days(1));
    clock_.SetNow(target_time - target_time_adjustment);
  }

  base::SimpleTestClock clock_;
};

TEST_F(ScheduleUtilsTest, SunsetToSunriseDetectsAllCheckpoints) {
  // Sunrise: 6 AM
  // Morning: 10 AM
  // LateAfternoon: 4 PM
  // Sunset: 6 PM
  const base::Time sunrise_time = BuildTime("06:00:00");
  const base::Time sunset_time = BuildTime("18:00:00");
  AdvanceClockTo("05:59:59");
  EXPECT_THAT(GetCurrentPosition(clock_.Now(), sunset_time, sunrise_time,
                                 ScheduleType::kSunsetToSunrise),
              FieldsAre(ScheduleCheckpoint::kSunset,
                        ScheduleCheckpoint::kSunrise, base::Seconds(1)));

  AdvanceClockTo("06:00:00");
  EXPECT_THAT(GetCurrentPosition(clock_.Now(), sunset_time, sunrise_time,
                                 ScheduleType::kSunsetToSunrise),
              FieldsAre(ScheduleCheckpoint::kSunrise,
                        ScheduleCheckpoint::kMorning, base::Hours(4)));

  AdvanceClockTo("09:59:59");
  EXPECT_THAT(GetCurrentPosition(clock_.Now(), sunset_time, sunrise_time,
                                 ScheduleType::kSunsetToSunrise),
              FieldsAre(ScheduleCheckpoint::kSunrise,
                        ScheduleCheckpoint::kMorning, base::Seconds(1)));

  AdvanceClockTo("10:00:00");
  EXPECT_THAT(GetCurrentPosition(clock_.Now(), sunset_time, sunrise_time,
                                 ScheduleType::kSunsetToSunrise),
              FieldsAre(ScheduleCheckpoint::kMorning,
                        ScheduleCheckpoint::kLateAfternoon, base::Hours(6)));

  AdvanceClockTo("15:59:59");
  EXPECT_THAT(GetCurrentPosition(clock_.Now(), sunset_time, sunrise_time,
                                 ScheduleType::kSunsetToSunrise),
              FieldsAre(ScheduleCheckpoint::kMorning,
                        ScheduleCheckpoint::kLateAfternoon, base::Seconds(1)));

  AdvanceClockTo("16:00:00");
  EXPECT_THAT(GetCurrentPosition(clock_.Now(), sunset_time, sunrise_time,
                                 ScheduleType::kSunsetToSunrise),
              FieldsAre(ScheduleCheckpoint::kLateAfternoon,
                        ScheduleCheckpoint::kSunset, base::Hours(2)));

  AdvanceClockTo("17:59:59");
  EXPECT_THAT(GetCurrentPosition(clock_.Now(), sunset_time, sunrise_time,
                                 ScheduleType::kSunsetToSunrise),
              FieldsAre(ScheduleCheckpoint::kLateAfternoon,
                        ScheduleCheckpoint::kSunset, base::Seconds(1)));

  AdvanceClockTo("18:00:00");
  EXPECT_THAT(GetCurrentPosition(clock_.Now(), sunset_time, sunrise_time,
                                 ScheduleType::kSunsetToSunrise),
              FieldsAre(ScheduleCheckpoint::kSunset,
                        ScheduleCheckpoint::kSunrise, base::Hours(12)));
}

TEST_F(ScheduleUtilsTest, SunsetToSunriseSunsetJustAfterSunrise) {
  // Sunrise: 12:00 PM
  // Morning: 12:00 PM + (1.5 hours / 3) = 12:30 PM (usually 10 AM)
  // LateAfternoon: 1:30 PM - (1.5 hours / 6) = 1:15 PM (usually 4 PM)
  // Sunset: 1:30 PM
  const base::Time sunrise_time = BuildTime("12:00:00");
  const base::Time sunset_time = BuildTime("13:30:00");
  AdvanceClockTo("12:00:00");
  EXPECT_THAT(GetCurrentPosition(clock_.Now(), sunset_time, sunrise_time,
                                 ScheduleType::kSunsetToSunrise),
              FieldsAre(ScheduleCheckpoint::kSunrise,
                        ScheduleCheckpoint::kMorning, base::Minutes(30)));

  AdvanceClockTo("12:30:00");
  EXPECT_THAT(GetCurrentPosition(clock_.Now(), sunset_time, sunrise_time,
                                 ScheduleType::kSunsetToSunrise),
              FieldsAre(ScheduleCheckpoint::kMorning,
                        ScheduleCheckpoint::kLateAfternoon, base::Minutes(45)));

  AdvanceClockTo("13:15:00");
  EXPECT_THAT(GetCurrentPosition(clock_.Now(), sunset_time, sunrise_time,
                                 ScheduleType::kSunsetToSunrise),
              FieldsAre(ScheduleCheckpoint::kLateAfternoon,
                        ScheduleCheckpoint::kSunset, base::Minutes(15)));

  AdvanceClockTo("13:30:00");
  EXPECT_THAT(
      GetCurrentPosition(clock_.Now(), sunset_time, sunrise_time,
                         ScheduleType::kSunsetToSunrise),
      FieldsAre(ScheduleCheckpoint::kSunset, ScheduleCheckpoint::kSunrise,
                base::Days(1) - base::Minutes(90)));
}

TEST_F(ScheduleUtilsTest, SunsetToSunriseSunriseJustAfterSunset) {
  // Sunrise: 2:00 PM
  // Morning: 2:00 PM + (22 hours / 3) = 9:20 PM (usually 10 AM)
  // LateAfternoon: 12:00 PM - (22 hours / 6) = 8:20 AM (usually 4 PM)
  // Sunset: 12:00 PM
  const base::Time sunrise_time = BuildTime("14:00:00");
  const base::Time sunset_time = BuildTime("12:00:00");
  AdvanceClockTo("14:00:00");
  EXPECT_THAT(
      GetCurrentPosition(clock_.Now(), sunset_time, sunrise_time,
                         ScheduleType::kSunsetToSunrise),
      FieldsAre(ScheduleCheckpoint::kSunrise, ScheduleCheckpoint::kMorning,
                base::Hours(7) + base::Minutes(20)));

  AdvanceClockTo("21:20:00");
  EXPECT_THAT(GetCurrentPosition(clock_.Now(), sunset_time, sunrise_time,
                                 ScheduleType::kSunsetToSunrise),
              FieldsAre(ScheduleCheckpoint::kMorning,
                        ScheduleCheckpoint::kLateAfternoon, base::Hours(11)));

  AdvanceClockTo("08:20:00");
  EXPECT_THAT(
      GetCurrentPosition(clock_.Now(), sunset_time, sunrise_time,
                         ScheduleType::kSunsetToSunrise),
      FieldsAre(ScheduleCheckpoint::kLateAfternoon, ScheduleCheckpoint::kSunset,
                base::Hours(3) + base::Minutes(40)));

  AdvanceClockTo("12:00:00");
  EXPECT_THAT(GetCurrentPosition(clock_.Now(), sunset_time, sunrise_time,
                                 ScheduleType::kSunsetToSunrise),
              FieldsAre(ScheduleCheckpoint::kSunset,
                        ScheduleCheckpoint::kSunrise, base::Hours(2)));
}

TEST_F(ScheduleUtilsTest, SunsetToSunriseShiftsScheduleToMatchDate) {
  // Sunrise: 6 AM
  // Morning: 10 AM
  // LateAfternoon: 4 PM
  // Sunset: 6 PM
  const base::Time sunrise_time = BuildTime("06:00:00");
  const base::Time sunset_time = BuildTime("18:00:00");
  const auto test_all_checkpoints = [this, sunrise_time, sunset_time]() {
    AdvanceClockTo("08:00:00");
    EXPECT_THAT(GetCurrentPosition(clock_.Now(), sunset_time, sunrise_time,
                                   ScheduleType::kSunsetToSunrise),
                FieldsAre(ScheduleCheckpoint::kSunrise,
                          ScheduleCheckpoint::kMorning, base::Hours(2)));
    AdvanceClockTo("12:00:00");
    EXPECT_THAT(GetCurrentPosition(clock_.Now(), sunset_time, sunrise_time,
                                   ScheduleType::kSunsetToSunrise),
                FieldsAre(ScheduleCheckpoint::kMorning,
                          ScheduleCheckpoint::kLateAfternoon, base::Hours(4)));
    AdvanceClockTo("17:00:00");
    EXPECT_THAT(GetCurrentPosition(clock_.Now(), sunset_time, sunrise_time,
                                   ScheduleType::kSunsetToSunrise),
                FieldsAre(ScheduleCheckpoint::kLateAfternoon,
                          ScheduleCheckpoint::kSunset, base::Hours(1)));
    AdvanceClockTo("00:00:00");
    EXPECT_THAT(GetCurrentPosition(clock_.Now(), sunset_time, sunrise_time,
                                   ScheduleType::kSunsetToSunrise),
                FieldsAre(ScheduleCheckpoint::kSunset,
                          ScheduleCheckpoint::kSunrise, base::Hours(6)));
  };

  // Set now to be 5 days earlier. The schedule should still hold.
  clock_.SetNow(BuildTime("00:00:00") - base::Days(5));
  test_all_checkpoints();
  // Set now to be 5 days later. The schedule should still hold.
  clock_.SetNow(BuildTime("00:00:00") + base::Days(5));
  test_all_checkpoints();
}

TEST_F(ScheduleUtilsTest, SunsetToSunriseMinimalSpaceFromSunriseToSunset) {
  // 6 seconds from sunrise to sunset
  base::Time sunrise_time = BuildTime("00:00:00");
  base::Time sunset_time = sunrise_time + base::Microseconds(6);
  AdvanceClockTo("00:00:00");
  EXPECT_THAT(GetCurrentPosition(clock_.Now(), sunset_time, sunrise_time,
                                 ScheduleType::kSunsetToSunrise),
              FieldsAre(ScheduleCheckpoint::kSunrise,
                        ScheduleCheckpoint::kMorning, base::Microseconds(2)));
  clock_.Advance(base::Microseconds(2));
  EXPECT_THAT(
      GetCurrentPosition(clock_.Now(), sunset_time, sunrise_time,
                         ScheduleType::kSunsetToSunrise),
      FieldsAre(ScheduleCheckpoint::kMorning,
                ScheduleCheckpoint::kLateAfternoon, base::Microseconds(3)));
  clock_.Advance(base::Microseconds(3));
  EXPECT_THAT(GetCurrentPosition(clock_.Now(), sunset_time, sunrise_time,
                                 ScheduleType::kSunsetToSunrise),
              FieldsAre(ScheduleCheckpoint::kLateAfternoon,
                        ScheduleCheckpoint::kSunset, base::Microseconds(1)));
  clock_.Advance(base::Microseconds(1));
  EXPECT_THAT(GetCurrentPosition(clock_.Now(), sunset_time, sunrise_time,
                                 ScheduleType::kSunsetToSunrise),
              FieldsAre(ScheduleCheckpoint::kSunset, _, _));
}

TEST_F(ScheduleUtilsTest, SunsetToSunriseSunriseExactlyEqualsSunset) {
  // Sunrise and sunset are equal. In this case, there literally is no sunset
  // or sunrise. The main thing is that this does not crash the device or cause
  // the code to enter some bad unpredictable state.
  const base::Time sunrise_time = BuildTime("00:00:00");
  const base::Time sunset_time = sunrise_time;
  AdvanceClockTo("00:00:00");
  Position current_position = GetCurrentPosition(
      clock_.Now(), sunset_time, sunrise_time, ScheduleType::kSunsetToSunrise);
  EXPECT_EQ(current_position.current_checkpoint,
            current_position.next_checkpoint);
  EXPECT_EQ(current_position.time_until_next_checkpoint, base::Days(1));

  AdvanceClockTo("12:00:00");
  current_position = GetCurrentPosition(clock_.Now(), sunset_time, sunrise_time,
                                        ScheduleType::kSunsetToSunrise);
  EXPECT_EQ(current_position.current_checkpoint,
            current_position.next_checkpoint);
  EXPECT_EQ(current_position.time_until_next_checkpoint, base::Hours(12));
}

TEST_F(ScheduleUtilsTest, SunsetToSunriseNoSpaceForMorningAndLateAfternoon) {
  // Sunrise and sunset are 1 microsecond apart. It's impossible to place
  // morning and late afternoon between them since that's the smallest
  // resolution of `base::Time`. In this case, morning and late afternoon
  // should be omitted.
  const base::Time sunrise_time = BuildTime("00:00:00");
  const base::Time sunset_time = sunrise_time + base::Microseconds(1);
  AdvanceClockTo("00:00:00");
  EXPECT_THAT(GetCurrentPosition(clock_.Now(), sunset_time, sunrise_time,
                                 ScheduleType::kSunsetToSunrise),
              FieldsAre(ScheduleCheckpoint::kSunrise,
                        ScheduleCheckpoint::kSunset, base::Microseconds(1)));
  clock_.Advance(base::Microseconds(1));
  EXPECT_THAT(
      GetCurrentPosition(clock_.Now(), sunset_time, sunrise_time,
                         ScheduleType::kSunsetToSunrise),
      FieldsAre(ScheduleCheckpoint::kSunset, ScheduleCheckpoint::kSunrise,
                base::Days(1) - base::Microseconds(1)));
}

TEST_F(ScheduleUtilsTest, CustomDetectsAllCheckpoints) {
  // End: 6 AM
  // Start: 6 PM
  const base::Time end_time = BuildTime("06:00:00");
  const base::Time start_time = BuildTime("18:00:00");
  AdvanceClockTo("05:59:59");
  EXPECT_THAT(GetCurrentPosition(clock_.Now(), start_time, end_time,
                                 ScheduleType::kCustom),
              FieldsAre(ScheduleCheckpoint::kEnabled,
                        ScheduleCheckpoint::kDisabled, base::Seconds(1)));

  AdvanceClockTo("06:00:00");
  EXPECT_THAT(GetCurrentPosition(clock_.Now(), start_time, end_time,
                                 ScheduleType::kCustom),
              FieldsAre(ScheduleCheckpoint::kDisabled,
                        ScheduleCheckpoint::kEnabled, base::Hours(12)));

  AdvanceClockTo("17:59:59");
  EXPECT_THAT(GetCurrentPosition(clock_.Now(), start_time, end_time,
                                 ScheduleType::kCustom),
              FieldsAre(ScheduleCheckpoint::kDisabled,
                        ScheduleCheckpoint::kEnabled, base::Seconds(1)));

  AdvanceClockTo("18:00:00");
  EXPECT_THAT(GetCurrentPosition(clock_.Now(), start_time, end_time,
                                 ScheduleType::kCustom),
              FieldsAre(ScheduleCheckpoint::kEnabled,
                        ScheduleCheckpoint::kDisabled, base::Hours(12)));
}

TEST_F(ScheduleUtilsTest, SunsetToSunriseGetTimeUntilNextEvent) {
  const base::Time origin = clock_.Now();
  // Event time is same as now.
  EXPECT_EQ(ShiftWithinOneDayFrom(origin, origin), origin);

  // Event time is ahead of now.
  EXPECT_EQ(ShiftWithinOneDayFrom(origin, origin + base::Hours(6)),
            origin + base::Hours(6));
  EXPECT_EQ(ShiftWithinOneDayFrom(origin, origin + base::Hours(18)),
            origin + base::Hours(18));
  EXPECT_EQ(ShiftWithinOneDayFrom(origin, origin + base::Days(1)), origin);
  EXPECT_EQ(
      ShiftWithinOneDayFrom(origin, origin + base::Days(1) + base::Hours(6)),
      origin + base::Hours(6));
  EXPECT_EQ(
      ShiftWithinOneDayFrom(origin, origin + base::Days(1) + base::Hours(18)),
      origin + base::Hours(18));
  EXPECT_EQ(ShiftWithinOneDayFrom(origin, origin + base::Days(2)), origin);

  // Event time is behind now.
  EXPECT_EQ(ShiftWithinOneDayFrom(origin, origin - base::Hours(6)),
            origin + base::Hours(18));
  EXPECT_EQ(ShiftWithinOneDayFrom(origin, origin - base::Hours(18)),
            origin + base::Hours(6));
  EXPECT_EQ(ShiftWithinOneDayFrom(origin, origin - base::Days(1)), origin);
  EXPECT_EQ(
      ShiftWithinOneDayFrom(origin, origin - base::Days(1) - base::Hours(6)),
      origin + base::Hours(18));
  EXPECT_EQ(
      ShiftWithinOneDayFrom(origin, origin - base::Days(1) - base::Hours(18)),
      origin + base::Hours(6));
  EXPECT_EQ(ShiftWithinOneDayFrom(origin, origin - base::Days(2)), origin);
}

}  // namespace
}  // namespace ash::schedule_utils
