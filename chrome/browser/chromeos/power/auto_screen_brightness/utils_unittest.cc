// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/auto_screen_brightness/utils.h"

#include <cmath>

#include "base/logging.h"
#include "base/test/simple_test_tick_clock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace power {
namespace auto_screen_brightness {

TEST(AverageAmbient, FullBufferTest) {
  base::SimpleTestTickClock tick_clock;
  base::RingBuffer<AmbientLightSample, 5> data;
  for (int i = 0; i < 5; ++i) {
    const AmbientLightSample sample = {i, tick_clock.NowTicks()};
    data.SaveToBuffer(sample);
  }

  double sum = 0;
  for (int i = 0; i < 5; ++i) {
    const int num_recent = i + 1;
    // Reverse order.
    const int head_value = 4 - i;
    sum += head_value;
    const double expected = sum / num_recent;
    EXPECT_DOUBLE_EQ(AverageAmbient(data, num_recent), expected);
  }

  EXPECT_DOUBLE_EQ(AverageAmbient(data, -1), sum / 5);
}

TEST(AverageAmbient, NonFullBufferTest) {
  base::SimpleTestTickClock tick_clock;
  base::RingBuffer<AmbientLightSample, 5> data;
  for (int i = 0; i < 3; ++i) {
    const AmbientLightSample sample = {i, tick_clock.NowTicks()};
    data.SaveToBuffer(sample);
  }

  double sum = 0;
  for (int i = 0; i < 3; ++i) {
    const int num_recent = i + 1;
    // Reverse order.
    const int head_value = 2 - i;
    sum += head_value;
    const double expected = sum / num_recent;
    EXPECT_DOUBLE_EQ(AverageAmbient(data, num_recent), expected);
  }

  const double expected = sum / 3;
  for (int i = 3; i < 5; ++i) {
    const int num_recent = i + 1;
    EXPECT_DOUBLE_EQ(AverageAmbient(data, num_recent), expected);
  }

  EXPECT_DOUBLE_EQ(AverageAmbient(data, -1), expected);
}

TEST(AverageAmbient, EmptyBufferTest) {
  base::RingBuffer<AmbientLightSample, 5> data;
  EXPECT_DOUBLE_EQ(AverageAmbient(data, 2), 0);
  EXPECT_DOUBLE_EQ(AverageAmbient(data, -1), 0);
}

}  // namespace auto_screen_brightness
}  // namespace power
}  // namespace chromeos
