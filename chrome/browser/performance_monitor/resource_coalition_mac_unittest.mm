// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_monitor/resource_coalition_mac.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>

#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "chrome/browser/performance_monitor/resource_coalition_internal_types_mac.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_monitor {

namespace {

static constexpr base::TimeDelta kIntervalLength =
    base::TimeDelta::FromSecondsD(2.5);

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
  EXPECT_NE(sample->cpu_time_per_second, 0);
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

  constexpr double kExpectedCPUUsagePerSecondPercent = 0.7;
  constexpr double kExpectedGPUUsagePerSecondPercent = 0.3;
  // Note: The following counters must have an integral value once multiplied by
  // the interval length in seconds (2.5).
  constexpr double kExpectedInterruptWakeUpPerSecond = 0.4;
  constexpr double kExpectedPlatformIdleWakeUpPerSecond = 10;
  constexpr double kExpectedBytesReadPerSecond = 0.8;
  constexpr double kExpectedBytesWrittenPerSecond = 1.6;
  constexpr double kExpectedPowerNW = 10000.0;

  // This number will be multiplied by the int value associated with a QoS level
  // to compute the expected time spent in this QoS level. E.g.
  // |QoSLevels::kUtility == 3| so the time spent in the utility QoS state will
  // be set to 3 * 0.1 = 30%.
  constexpr double kExpectedQoSTimeBucketIdMultiplier = 0.1;

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
  for (int i = 0; i < COALITION_NUM_THREAD_QOS_TYPES; ++i) {
    t1_data->cpu_time_eqos[i] = i * kExpectedQoSTimeBucketIdMultiplier *
                                kIntervalLength.InNanoseconds();
  }

  t1_data->cpu_time_eqos_len = COALITION_NUM_THREAD_QOS_TYPES;

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

  for (int i = 0; i < COALITION_NUM_THREAD_QOS_TYPES; ++i) {
    EXPECT_DOUBLE_EQ(i * kExpectedQoSTimeBucketIdMultiplier,
                     data_rate->qos_time_per_second[i]);
  }
}

namespace {

bool DataOverflowInvalidatesDiffImpl(
    ResourceCoalition& coalition,
    std::unique_ptr<coalition_resource_usage> t0,
    std::unique_ptr<coalition_resource_usage> t1,
    uint64_t* field_to_overflow) {
  // Initialize all fields to a non zero value.
  ::memset(t0.get(), 1000, sizeof(coalition_resource_usage));
  ::memset(t1.get(), 1000, sizeof(coalition_resource_usage));
  *field_to_overflow = 0;

  t1->cpu_time_eqos_len = COALITION_NUM_THREAD_QOS_TYPES;
  return !coalition
              .GetDataRateFromFakeDataForTesting(std::move(t0), std::move(t1),
                                                 kIntervalLength)
              .has_value();
}

bool DataOverflowInvalidatesDiff(
    ResourceCoalition& coalition,
    uint64_t coalition_resource_usage::*member_ptr) {
  std::unique_ptr<coalition_resource_usage> t0_data =
      std::make_unique<coalition_resource_usage>();
  std::unique_ptr<coalition_resource_usage> t1_data =
      std::make_unique<coalition_resource_usage>();
  auto* ptr = &(t1_data.get()->*member_ptr);
  return DataOverflowInvalidatesDiffImpl(coalition, std::move(t0_data),
                                         std::move(t1_data), ptr);
}

bool DataOverflowInvalidatesDiff(
    ResourceCoalition& coalition,
    uint64_t (
        coalition_resource_usage::*member_ptr)[COALITION_NUM_THREAD_QOS_TYPES],
    int index_to_check) {
  std::unique_ptr<coalition_resource_usage> t0_data =
      std::make_unique<coalition_resource_usage>();
  std::unique_ptr<coalition_resource_usage> t1_data =
      std::make_unique<coalition_resource_usage>();
  auto* ptr = &(t1_data.get()->*member_ptr)[index_to_check];
  return DataOverflowInvalidatesDiffImpl(coalition, std::move(t0_data),
                                         std::move(t1_data), ptr);
}

}  // namespace

TEST(ResourceCoalitionTests, Overflows) {
  ResourceCoalition coalition;
  coalition.SetCoalitionIDToCurrentProcessIdForTesting();
  EXPECT_TRUE(coalition.IsAvailable());

  // Keep the initial data zero initialized.
  std::unique_ptr<coalition_resource_usage> t0_data =
      std::make_unique<coalition_resource_usage>();

  {
    std::unique_ptr<coalition_resource_usage> t1_data =
        std::make_unique<coalition_resource_usage>();
    t1_data->cpu_time_eqos_len = COALITION_NUM_THREAD_QOS_TYPES;
    auto data_rate = coalition.GetDataRateFromFakeDataForTesting(
        std::move(t0_data), std::move(t1_data), kIntervalLength);
    // It's valid for the data from the 2 samples to be equal to one another.
    EXPECT_TRUE(data_rate);
  }

  // If one of these tests fails then it means that overflows on a newly tracked
  // coalition field aren't tracked properly in GetCoalitionDataDiff.
  EXPECT_FALSE(DataOverflowInvalidatesDiff(
      coalition, &coalition_resource_usage::energy_billed_to_me));
  EXPECT_FALSE(DataOverflowInvalidatesDiff(
      coalition, &coalition_resource_usage::tasks_started));
  EXPECT_FALSE(DataOverflowInvalidatesDiff(
      coalition, &coalition_resource_usage::tasks_exited));
  EXPECT_FALSE(DataOverflowInvalidatesDiff(
      coalition, &coalition_resource_usage::time_nonempty));
  EXPECT_TRUE(DataOverflowInvalidatesDiff(coalition,
                                          &coalition_resource_usage::cpu_time));
  EXPECT_TRUE(DataOverflowInvalidatesDiff(
      coalition, &coalition_resource_usage::interrupt_wakeups));
  EXPECT_TRUE(DataOverflowInvalidatesDiff(
      coalition, &coalition_resource_usage::platform_idle_wakeups));
  EXPECT_TRUE(DataOverflowInvalidatesDiff(
      coalition, &coalition_resource_usage::bytesread));
  EXPECT_TRUE(DataOverflowInvalidatesDiff(
      coalition, &coalition_resource_usage::byteswritten));
  EXPECT_TRUE(DataOverflowInvalidatesDiff(coalition,
                                          &coalition_resource_usage::gpu_time));
  EXPECT_FALSE(DataOverflowInvalidatesDiff(
      coalition, &coalition_resource_usage::cpu_time_billed_to_me));
  EXPECT_FALSE(DataOverflowInvalidatesDiff(
      coalition, &coalition_resource_usage::cpu_time_billed_to_others));
  EXPECT_TRUE(DataOverflowInvalidatesDiff(coalition,
                                          &coalition_resource_usage::energy));
  EXPECT_FALSE(DataOverflowInvalidatesDiff(
      coalition, &coalition_resource_usage::logical_immediate_writes));
  EXPECT_FALSE(DataOverflowInvalidatesDiff(
      coalition, &coalition_resource_usage::logical_deferred_writes));
  EXPECT_FALSE(DataOverflowInvalidatesDiff(
      coalition, &coalition_resource_usage::logical_invalidated_writes));
  EXPECT_FALSE(DataOverflowInvalidatesDiff(
      coalition, &coalition_resource_usage::logical_metadata_writes));
  EXPECT_FALSE(DataOverflowInvalidatesDiff(
      coalition,
      &coalition_resource_usage::logical_immediate_writes_to_external));
  EXPECT_FALSE(DataOverflowInvalidatesDiff(
      coalition,
      &coalition_resource_usage::logical_deferred_writes_to_external));
  EXPECT_FALSE(DataOverflowInvalidatesDiff(
      coalition,
      &coalition_resource_usage::logical_invalidated_writes_to_external));
  EXPECT_FALSE(DataOverflowInvalidatesDiff(
      coalition,
      &coalition_resource_usage::logical_metadata_writes_to_external));
  EXPECT_FALSE(DataOverflowInvalidatesDiff(
      coalition, &coalition_resource_usage::energy_billed_to_me));
  EXPECT_FALSE(DataOverflowInvalidatesDiff(
      coalition, &coalition_resource_usage::energy_billed_to_others));
  EXPECT_FALSE(DataOverflowInvalidatesDiff(
      coalition, &coalition_resource_usage::cpu_ptime));
  for (int i = 0; i < COALITION_NUM_THREAD_QOS_TYPES; ++i) {
    EXPECT_TRUE(DataOverflowInvalidatesDiff(
        coalition, &coalition_resource_usage::cpu_time_eqos, i));
  }
  EXPECT_FALSE(DataOverflowInvalidatesDiff(
      coalition, &coalition_resource_usage::cpu_instructions));
  EXPECT_FALSE(DataOverflowInvalidatesDiff(
      coalition, &coalition_resource_usage::cpu_cycles));
  EXPECT_FALSE(DataOverflowInvalidatesDiff(
      coalition, &coalition_resource_usage::fs_metadata_writes));
  EXPECT_FALSE(DataOverflowInvalidatesDiff(
      coalition, &coalition_resource_usage::pm_writes));
}

}  // namespace

}  // namespace performance_monitor
