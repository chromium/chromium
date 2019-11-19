// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/auto_screen_brightness/als_samples.h"

#include <cmath>
#include <vector>

#include "base/logging.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace power {
namespace auto_screen_brightness {

namespace {

// Checks |result.avg| and |result.stddev| are the same as that
// calculated from the |expected_data| vector.
void CheckAverageAndStdDev(const AlsAvgStdDev& result,
                           const std::vector<double>& expected_data) {
  const size_t count = expected_data.size();
  CHECK_NE(count, 0u);
  double expected_avg = 0;
  double expected_stddev = 0;

  for (const auto& i : expected_data) {
    expected_avg += i;
    expected_stddev += i * i;
  }

  expected_avg = expected_avg / count;
  expected_stddev =
      std::sqrt(expected_stddev / count - expected_avg * expected_avg);
  EXPECT_DOUBLE_EQ(result.avg, expected_avg);
  EXPECT_DOUBLE_EQ(result.stddev, expected_stddev);
}

}  // namespace

TEST(AmbientLightSampleBufferTest, Basic) {
  base::SimpleTestTickClock tick_clock;
  AmbientLightSampleBuffer buffer(base::TimeDelta::FromSeconds(5));
  std::vector<double> expected_data;
  for (int i = 1; i < 6; ++i) {
    tick_clock.Advance(base::TimeDelta::FromSeconds(1));
    const AmbientLightSampleBuffer::Sample sample = {i, tick_clock.NowTicks()};
    expected_data.push_back(i);
    buffer.SaveToBuffer(sample);
    EXPECT_EQ(buffer.NumberOfSamplesForTesting(), static_cast<size_t>(i));
    const AlsAvgStdDev avg_std =
        buffer.AverageAmbientWithStdDev(tick_clock.NowTicks()).value();

    CheckAverageAndStdDev(avg_std, expected_data);

    EXPECT_EQ(buffer.NumberOfSamplesForTesting(), static_cast<size_t>(i));
  }

  // Add another two items that will push out the oldest.
  tick_clock.Advance(base::TimeDelta::FromSeconds(1));
  buffer.SaveToBuffer({10, tick_clock.NowTicks()});

  EXPECT_EQ(buffer.NumberOfSamplesForTesting(), 5u);
  CheckAverageAndStdDev(
      buffer.AverageAmbientWithStdDev(tick_clock.NowTicks()).value(),
      {2, 3, 4, 5, 10});
  EXPECT_EQ(buffer.NumberOfSamplesForTesting(), 5u);

  tick_clock.Advance(base::TimeDelta::FromSeconds(1));
  buffer.SaveToBuffer({20, tick_clock.NowTicks()});
  EXPECT_EQ(buffer.NumberOfSamplesForTesting(), 5u);
  CheckAverageAndStdDev(
      buffer.AverageAmbientWithStdDev(tick_clock.NowTicks()).value(),
      {3, 4, 5, 10, 20});
  EXPECT_EQ(buffer.NumberOfSamplesForTesting(), 5u);

  // Add another item but it doesn't push out the oldest.
  tick_clock.Advance(base::TimeDelta::FromMilliseconds(1));
  buffer.SaveToBuffer({100, tick_clock.NowTicks()});
  EXPECT_EQ(buffer.NumberOfSamplesForTesting(), 6u);
  CheckAverageAndStdDev(
      buffer.AverageAmbientWithStdDev(tick_clock.NowTicks()).value(),
      {3, 4, 5, 10, 20, 100});
  EXPECT_EQ(buffer.NumberOfSamplesForTesting(), 6u);
}

TEST(AmbientLightSampleBufferTest, LargeSampleTimeGap) {
  base::SimpleTestTickClock tick_clock;
  AmbientLightSampleBuffer buffer(base::TimeDelta::FromSeconds(5));
  tick_clock.Advance(base::TimeDelta::FromSeconds(1));
  const AmbientLightSampleBuffer::Sample sample = {10, tick_clock.NowTicks()};
  buffer.SaveToBuffer(sample);
  EXPECT_EQ(buffer.NumberOfSamplesForTesting(), 1u);
  CheckAverageAndStdDev(
      buffer.AverageAmbientWithStdDev(tick_clock.NowTicks()).value(), {10});
  EXPECT_EQ(buffer.NumberOfSamplesForTesting(), 1u);

  // Another samples arrives sufficiently late so the 1st sample is pushed out.
  tick_clock.Advance(base::TimeDelta::FromSeconds(5));
  buffer.SaveToBuffer({20, tick_clock.NowTicks()});
  EXPECT_EQ(buffer.NumberOfSamplesForTesting(), 1u);
  CheckAverageAndStdDev(
      buffer.AverageAmbientWithStdDev(tick_clock.NowTicks()).value(), {20});
  EXPECT_EQ(buffer.NumberOfSamplesForTesting(), 1u);
}

TEST(AmbientLightSampleBufferTest, AverageTimeTooLate) {
  base::SimpleTestTickClock tick_clock;
  AmbientLightSampleBuffer buffer(base::TimeDelta::FromSeconds(5));
  tick_clock.Advance(base::TimeDelta::FromSeconds(1));
  const AmbientLightSampleBuffer::Sample sample = {10, tick_clock.NowTicks()};
  buffer.SaveToBuffer(sample);
  EXPECT_EQ(buffer.NumberOfSamplesForTesting(), 1u);
  CheckAverageAndStdDev(
      buffer.AverageAmbientWithStdDev(tick_clock.NowTicks()).value(), {10});
  EXPECT_EQ(buffer.NumberOfSamplesForTesting(), 1u);

  // When average is calculated, all samples are too old, hence average is
  // nullopt.
  tick_clock.Advance(base::TimeDelta::FromSeconds(5));
  EXPECT_EQ(buffer.NumberOfSamplesForTesting(), 1u);
  EXPECT_FALSE(
      buffer.AverageAmbientWithStdDev(tick_clock.NowTicks()).has_value());
  EXPECT_EQ(buffer.NumberOfSamplesForTesting(), 0u);
}

TEST(AmbientLightSampleBufferTest, BufferCleared) {
  base::SimpleTestTickClock tick_clock;
  AmbientLightSampleBuffer buffer(base::TimeDelta::FromSeconds(5));

  // Save two data points and verify.
  tick_clock.Advance(base::TimeDelta::FromSeconds(1));
  buffer.SaveToBuffer({10, tick_clock.NowTicks()});

  tick_clock.Advance(base::TimeDelta::FromSeconds(1));
  buffer.SaveToBuffer({20, tick_clock.NowTicks()});

  EXPECT_EQ(buffer.NumberOfSamplesForTesting(), 2u);
  AlsAvgStdDev avg_std =
      buffer.AverageAmbientWithStdDev(tick_clock.NowTicks()).value();

  CheckAverageAndStdDev(avg_std, {10, 20});

  // Clear buffer and verify.
  buffer.ClearBuffer();
  EXPECT_EQ(buffer.NumberOfSamplesForTesting(), 0u);
  EXPECT_FALSE(
      buffer.AverageAmbientWithStdDev(tick_clock.NowTicks()).has_value());

  // Save another two data points and verify.
  tick_clock.Advance(base::TimeDelta::FromSeconds(1));
  buffer.SaveToBuffer({30, tick_clock.NowTicks()});

  tick_clock.Advance(base::TimeDelta::FromSeconds(1));
  buffer.SaveToBuffer({40, tick_clock.NowTicks()});
  avg_std = buffer.AverageAmbientWithStdDev(tick_clock.NowTicks()).value();
  CheckAverageAndStdDev(avg_std, {30, 40});
}

}  // namespace auto_screen_brightness
}  // namespace power
}  // namespace chromeos
