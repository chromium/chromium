// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/ml/boot_clock.h"

#include "base/test/task_environment.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace power {
namespace ml {

TEST(BootClockTest, Basic) {
  BootClock boot_clock;

  constexpr base::TimeDelta sleep_duration =
      base::TimeDelta::FromMilliseconds(10);
  const base::TimeDelta init_time_since_boot = boot_clock.GetTimeSinceBoot();
  EXPECT_GE(init_time_since_boot, base::TimeDelta());
  const base::TimeDelta expected_end_time_since_boot =
      init_time_since_boot + sleep_duration;

  base::PlatformThread::Sleep(sleep_duration);
  EXPECT_GE(boot_clock.GetTimeSinceBoot(), expected_end_time_since_boot);
}

TEST(BootClockTest, UnderMockTime) {
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  BootClock boot_clock;

  constexpr base::TimeDelta mock_sleep_duration =
      base::TimeDelta::FromSeconds(200);
  const base::TimeDelta init_time_since_boot = boot_clock.GetTimeSinceBoot();
  EXPECT_GE(init_time_since_boot, base::TimeDelta());
  const base::TimeDelta expected_end_time_since_boot =
      init_time_since_boot + mock_sleep_duration;

  task_environment.FastForwardBy(mock_sleep_duration);
  EXPECT_EQ(boot_clock.GetTimeSinceBoot(), expected_end_time_since_boot);
}

}  // namespace ml
}  // namespace power
}  // namespace chromeos
