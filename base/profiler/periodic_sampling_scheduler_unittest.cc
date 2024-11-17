// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/profiler/periodic_sampling_scheduler.h"

#include "base/test/bind.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace {

class TestScheduler : public PeriodicSamplingScheduler {
 public:
  TestScheduler(TimeDelta sampling_duration,
                double fraction_of_execution_time_to_sample)
      : PeriodicSamplingScheduler(sampling_duration,
                                  fraction_of_execution_time_to_sample,
                                  kStartTime) {
    tick_clock_.SetNowTicks(kStartTime);
  }

  TestScheduler(const TestScheduler&) = delete;
  TestScheduler& operator=(const TestScheduler&) = delete;

  double RandDouble() const override { return rand_double_value_; }
  TimeTicks Now() const override { return tick_clock_.NowTicks(); }

  void SetRandDouble(double value) { rand_double_value_ = value; }
  SimpleTestTickClock& tick_clock() { return tick_clock_; }

 private:
  static constexpr TimeTicks kStartTime = TimeTicks();
  SimpleTestTickClock tick_clock_;
  double rand_double_value_ = 0.0;
};

constexpr TimeTicks TestScheduler::kStartTime;

TEST(PeriodicSamplingSchedulerTest, ScheduleCollections) {
  const TimeDelta sampling_duration = Seconds(30);
  const double fraction_of_execution_time_to_sample = 0.01;

  const TimeDelta expected_period =
      sampling_duration / fraction_of_execution_time_to_sample;

  TestScheduler scheduler(sampling_duration,
                          fraction_of_execution_time_to_sample);

  // The first collection should be exactly at the start time, since the random
  // value is 0.0.
  scheduler.SetRandDouble(0.0);
  EXPECT_EQ(Seconds(0), scheduler.GetTimeToNextCollection());

  // With a random value of 1.0 the second collection should be at the end of
  // the second period.
  scheduler.SetRandDouble(1.0);
  EXPECT_EQ(2 * expected_period - sampling_duration,
            scheduler.GetTimeToNextCollection());

  // With a random value of 0.25 the second collection should be a quarter into
  // the third period exclusive of the sampling duration.
  scheduler.SetRandDouble(0.25);
  EXPECT_EQ(2 * expected_period + 0.25 * (expected_period - sampling_duration),
            scheduler.GetTimeToNextCollection());
}

TEST(PeriodicSamplingSchedulerTest, ScheduleWithJumpInTimeTicks) {
  const TimeDelta sampling_duration = Seconds(30);
  const double fraction_of_execution_time_to_sample = 0.01;

  const TimeDelta expected_period =
      sampling_duration / fraction_of_execution_time_to_sample;

  TestScheduler scheduler(sampling_duration,
                          fraction_of_execution_time_to_sample);

  // The first collection should be exactly at the start time, since the random
  // value is 0.0.
  scheduler.SetRandDouble(0.0);
  EXPECT_EQ(Seconds(0), scheduler.GetTimeToNextCollection());

  // Simulate a non-continuous jump in the current TimeTicks such that the next
  // period would start before the current time. In this case the
  // period start should be reset to the current time, and the next collection
  // chosen within that period.
  scheduler.tick_clock().Advance(expected_period + Seconds(1));
  scheduler.SetRandDouble(0.5);
  EXPECT_EQ(0.5 * (expected_period - sampling_duration),
            scheduler.GetTimeToNextCollection());
}

}  // namespace
}  // namespace base
