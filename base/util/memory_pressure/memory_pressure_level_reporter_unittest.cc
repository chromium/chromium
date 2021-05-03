// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/util/memory_pressure/memory_pressure_level_reporter.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace util {

TEST(MemoryPressureLevelReporterTest, PressureWindowDuration) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::TaskEnvironment::MainThreadType::IO,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);

  MemoryPressureLevelReporter reporter(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE);
  base::HistogramTester histogram_tester;

  // Moderate -> None.
  task_environment.AdvanceClock(base::TimeDelta::FromSeconds(12));
  reporter.OnMemoryPressureLevelChanged(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE);
  histogram_tester.ExpectTimeBucketCount(
      "Memory.PressureWindowDuration.ModerateToNone",
      base::TimeDelta::FromSeconds(12), 1);

  // Moderate -> Critical.
  reporter.OnMemoryPressureLevelChanged(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE);
  task_environment.AdvanceClock(base::TimeDelta::FromSeconds(20));
  reporter.OnMemoryPressureLevelChanged(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);
  histogram_tester.ExpectTimeBucketCount(
      "Memory.PressureWindowDuration.ModerateToCritical",
      base::TimeDelta::FromSeconds(20), 1);

  // Critical -> None
  task_environment.AdvanceClock(base::TimeDelta::FromSeconds(25));
  reporter.OnMemoryPressureLevelChanged(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE);
  histogram_tester.ExpectTimeBucketCount(
      "Memory.PressureWindowDuration.CriticalToNone",
      base::TimeDelta::FromSeconds(25), 1);

  // Critical -> Moderate
  reporter.OnMemoryPressureLevelChanged(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);
  task_environment.AdvanceClock(base::TimeDelta::FromSeconds(27));
  reporter.OnMemoryPressureLevelChanged(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE);
  histogram_tester.ExpectTimeBucketCount(
      "Memory.PressureWindowDuration.CriticalToModerate",
      base::TimeDelta::FromSeconds(27), 1);
}

TEST(MemoryPressureLevelReporterTest, MemoryPressureHistogram) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::TaskEnvironment::MainThreadType::IO,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);

  std::unique_ptr<MemoryPressureLevelReporter> reporter =
      std::make_unique<MemoryPressureLevelReporter>(
          base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE);
  base::HistogramTester histogram_tester;

  constexpr base::TimeDelta kDelay = base::TimeDelta::FromSeconds(12);
  const char* kHistogram = "Memory.PressureLevel2";

  // None -> Moderate.
  task_environment.AdvanceClock(kDelay);
  reporter->OnMemoryPressureLevelChanged(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE);
  // There one report for a |kdelay| MEMORY_PRESSURE_LEVEL_NONE session.
  histogram_tester.ExpectBucketCount(
      kHistogram,
      static_cast<int>(
          base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE),
      kDelay.InSeconds());

  task_environment.AdvanceClock(kDelay);
  reporter->OnMemoryPressureLevelChanged(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE);
  // There one report for a |kdelay| MEMORY_PRESSURE_LEVEL_MODERATE session.
  histogram_tester.ExpectBucketCount(
      kHistogram,
      static_cast<int>(
          base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE),
      kDelay.InSeconds());

  task_environment.AdvanceClock(kDelay);
  reporter->OnMemoryPressureLevelChanged(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);
  // There's now two reports for a |kdelay| MEMORY_PRESSURE_LEVEL_NONE session,
  // for a total of |2*kdelay|.
  histogram_tester.ExpectBucketCount(
      kHistogram,
      static_cast<int>(
          base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE),
      (2 * kDelay).InSeconds());

  task_environment.AdvanceClock(kDelay);
  histogram_tester.ExpectBucketCount(
      kHistogram,
      static_cast<int>(
          base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL),
      0);
  reporter.reset();
  // Releasing the reporter should report the data from the current pressure
  // session.
  histogram_tester.ExpectBucketCount(
      kHistogram,
      static_cast<int>(
          base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL),
      kDelay.InSeconds());
}

TEST(MemoryPressureLevelReporterTest, MemoryPressureHistogramAccumulatedTime) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::TaskEnvironment::MainThreadType::IO,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);

  MemoryPressureLevelReporter reporter(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE);
  base::HistogramTester histogram_tester;

  const char* kHistogram = "Memory.PressureLevel2";
  constexpr base::TimeDelta kHalfASecond =
      base::TimeDelta::FromMilliseconds(500);

  task_environment.AdvanceClock(kHalfASecond);
  reporter.OnMemoryPressureLevelChanged(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE);
  // The delay is inferior to one second, there should be no data reported.
  histogram_tester.ExpectBucketCount(
      kHistogram,
      static_cast<int>(
          base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE),
      0);

  reporter.OnMemoryPressureLevelChanged(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE);
  task_environment.AdvanceClock(kHalfASecond);
  reporter.OnMemoryPressureLevelChanged(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE);
  // The delay is inferior to one second, there should be no data reported.
  histogram_tester.ExpectBucketCount(
      kHistogram,
      static_cast<int>(
          base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE),
      1);
}

TEST(MemoryPressureLevelReporterTest,
     MemoryPressureHistogramPeriodicReporting) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::TaskEnvironment::MainThreadType::IO,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);

  MemoryPressureLevelReporter reporter(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE);
  base::HistogramTester histogram_tester;

  const char* kHistogram = "Memory.PressureLevel2";

  // Advancing the clock by a few seconds shouldn't cause any periodic
  // reporting.
  task_environment.FastForwardBy(base::TimeDelta::FromSeconds(10));
  histogram_tester.ExpectBucketCount(
      kHistogram,
      static_cast<int>(
          base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE),
      0);

  // Advancing the clock by a few minutes should cause periodic reporting.
  task_environment.FastForwardBy(base::TimeDelta::FromMinutes(5));
  histogram_tester.ExpectBucketCount(
      kHistogram,
      static_cast<int>(
          base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE),
      5 * 60 /* 5 minutes */);

  task_environment.FastForwardBy(base::TimeDelta::FromMinutes(5));
  histogram_tester.ExpectBucketCount(
      kHistogram,
      static_cast<int>(
          base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE),
      2 * 5 * 60 /* 2 x 5 minutes */);
}

}  // namespace util