// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/util/memory_pressure/multi_source_memory_pressure_monitor.h"

#include "base/macros.h"
#include "base/test/bind_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace util {

TEST(MultiSourceMemoryPressureMonitorTest, RunDispatchCallback) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::TaskEnvironment::MainThreadType::IO);

  MultiSourceMemoryPressureMonitor monitor;
  monitor.Start();
  auto* aggregator = monitor.aggregator_for_testing();

  bool callback_called = false;
  monitor.SetDispatchCallback(base::BindLambdaForTesting(
      [&](base::MemoryPressureListener::MemoryPressureLevel) {
        callback_called = true;
      }));
  aggregator->OnVoteForTesting(
      base::nullopt, base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE);
  aggregator->NotifyListenersForTesting();
  EXPECT_TRUE(callback_called);

  // Clear vote so aggregator's destructor doesn't think there are loose voters.
  aggregator->OnVoteForTesting(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE, base::nullopt);
}

TEST(MultiSourceMemoryPressureMonitorTest, Histograms) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::TaskEnvironment::MainThreadType::IO,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);

  MultiSourceMemoryPressureMonitor monitor;
  base::HistogramTester histogram_tester;
  monitor.Start();
  auto* aggregator = monitor.aggregator_for_testing();

  // Moderate -> None.
  aggregator->OnVoteForTesting(
      base::nullopt,
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE);
  task_environment.AdvanceClock(base::TimeDelta::FromSeconds(12));
  aggregator->OnVoteForTesting(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE,
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE);
  histogram_tester.ExpectTimeBucketCount(
      "Memory.PressureWindowDuration.ModerateToNone",
      base::TimeDelta::FromSeconds(12), 1);

  // Moderate -> Critical.
  aggregator->OnVoteForTesting(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE,
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE);
  task_environment.AdvanceClock(base::TimeDelta::FromSeconds(20));
  aggregator->OnVoteForTesting(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE,
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);
  histogram_tester.ExpectTimeBucketCount(
      "Memory.PressureWindowDuration.ModerateToCritical",
      base::TimeDelta::FromSeconds(20), 1);

  // Critical -> None
  task_environment.AdvanceClock(base::TimeDelta::FromSeconds(25));
  aggregator->OnVoteForTesting(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL,
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE);
  histogram_tester.ExpectTimeBucketCount(
      "Memory.PressureWindowDuration.CriticalToNone",
      base::TimeDelta::FromSeconds(25), 1);

  // Critical -> Moderate
  aggregator->OnVoteForTesting(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE,
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);
  task_environment.AdvanceClock(base::TimeDelta::FromSeconds(27));
  aggregator->OnVoteForTesting(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL,
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE);
  histogram_tester.ExpectTimeBucketCount(
      "Memory.PressureWindowDuration.CriticalToModerate",
      base::TimeDelta::FromSeconds(27), 1);

  // Clear vote so aggregator's destructor doesn't think there are loose voters.
  aggregator->OnVoteForTesting(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE,
      base::nullopt);
}

}  // namespace util
