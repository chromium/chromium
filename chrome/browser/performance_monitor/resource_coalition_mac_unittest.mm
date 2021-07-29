// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_monitor/resource_coalition_mac.h"

#include <cstdint>
#include <limits>
#include <memory>

#include "base/logging.h"
#include "base/rand_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "chrome/browser/performance_monitor/resource_coalition_internal_types_mac.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_monitor {

TEST(ResourceCoalitionTests, Basics) {
  base::HistogramTester histogram_tester;
  ResourceCoalition coalition;
  // Tests are usually run from a terminal and so they share their coalition ID
  // with it. This will fail if the tests is started with |launchd| or with
  // |open|.
  histogram_tester.ExpectUniqueSample(
      "PerformanceMonitor.ResourceCoalition.Availability",
      4 /* kNotAloneInCoalition */, 1);
  EXPECT_FALSE(coalition.IsAvailable());

  coalition.SetCoalitionIDToCurrentProcessIdForTesting();
  EXPECT_TRUE(coalition.IsAvailable());

  base::TimeTicks begin = base::TimeTicks::Now();
  constexpr base::TimeDelta busy_time = base::TimeDelta::FromSeconds(1);
  double number = 1;
  while (base::TimeTicks::Now() < (begin + busy_time)) {
    for (int i = 0; i < 10000; ++i) {
      number *= base::RandDouble() / std::numeric_limits<double>::max() * 2;
    }
  }

  auto sample = coalition.GetDataRate();
  EXPECT_TRUE(sample.has_value());
  // The busy loop should cause a high CPU time per minute value (close to 100%
  // of one core in practice). Use a really conservative value here to reduce
  // flakiness, the main goal of this check is to ensure that the reported
  // CPU time is representative of the workload (i.e. that it's not negligible).
  EXPECT_GE(sample->cpu_time_per_second, 0.2);
}

TEST(ResourceCoalitionTests, GetDataRate) {
  base::HistogramTester histogram_tester;
  ResourceCoalition coalition;
  coalition.SetCoalitionIDToCurrentProcessIdForTesting();
  EXPECT_TRUE(coalition.IsAvailable());

  // Keep the initial data zero initialized.
  std::unique_ptr<coalition_resource_usage> t0_data =
      std::make_unique<coalition_resource_usage>();

  std::unique_ptr<coalition_resource_usage> t1_data =
      std::make_unique<coalition_resource_usage>();

  constexpr base::TimeDelta kIntervalLength =
      base::TimeDelta::FromSecondsD(2.5);
  constexpr double kExpectedCPUUsagePerSecondPercent = 0.7;
  constexpr double kExpectedGPUUsagePerSecondPercent = 0.3;
  // Note: The following counters must have an integral value once multiplied by
  // the interval length in seconds (2.5).
  constexpr double kExpectedInterruptWakeUpPerSecond = 0.4;
  constexpr double kExpectedPlatformIdleWakeUpPerSecond = 10;
  constexpr double kExpectedBytesReadPerSecond = 0.8;
  constexpr double kExpectedBytesWrittenPerSecond = 1.6;
  constexpr double kExpectedPowerNW = 10000.0;

  t1_data->cpu_time =
      kExpectedCPUUsagePerSecondPercent * kIntervalLength.InNanoseconds();
  t1_data->interrupt_wakeups =
      kExpectedInterruptWakeUpPerSecond * kIntervalLength.InSecondsF();
  t1_data->platform_idle_wakeups =
      kExpectedPlatformIdleWakeUpPerSecond * kIntervalLength.InSecondsF();
  t1_data->bytesread =
      kExpectedBytesReadPerSecond * kIntervalLength.InSecondsF();
  t1_data->byteswritten =
      kExpectedBytesWrittenPerSecond * kIntervalLength.InSecondsF();
  t1_data->gpu_time =
      kExpectedGPUUsagePerSecondPercent * kIntervalLength.InNanoseconds();
  t1_data->energy = kExpectedPowerNW * kIntervalLength.InSecondsF();

  auto data_rate = coalition.GetDataRateFromFakeDataForTesting(
      std::move(t0_data), std::move(t1_data), kIntervalLength);
  EXPECT_TRUE(data_rate);
  EXPECT_EQ(kExpectedCPUUsagePerSecondPercent, data_rate->cpu_time_per_second);
  EXPECT_EQ(kExpectedInterruptWakeUpPerSecond,
            data_rate->interrupt_wakeups_per_second);
  EXPECT_EQ(kExpectedPlatformIdleWakeUpPerSecond,
            data_rate->platform_idle_wakeups_per_second);
  EXPECT_EQ(kExpectedBytesReadPerSecond, data_rate->bytesread_per_second);
  EXPECT_EQ(kExpectedBytesWrittenPerSecond, data_rate->byteswritten_per_second);
  EXPECT_EQ(kExpectedGPUUsagePerSecondPercent, data_rate->gpu_time_per_second);
  EXPECT_EQ(kExpectedPowerNW, data_rate->power_nw);
}

}  // namespace performance_monitor
