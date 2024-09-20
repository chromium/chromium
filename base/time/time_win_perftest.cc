// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include <stdint.h>

#include <algorithm>
#include <cstdio>

#include "base/bit_cast.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_result_reporter.h"
#include "third_party/google_benchmark/src/include/benchmark/benchmark.h"

namespace base {
namespace {

constexpr char kCountDelta[] = ".count_time_imprecise_precise";
constexpr char kAvgDelta[] = ".avg_time_precise_imprecise";
constexpr char kMinDelta[] = ".min_time_precise_imprecise";
constexpr char kMaxDelta[] = ".max_time_precise_imprecise";

// Copied from base/time_win.cc.
// From MSDN, FILETIME "Contains a 64-bit value representing the number of
// 100-nanosecond intervals since January 1, 1601 (UTC)."
int64_t FileTimeToMicroseconds(const FILETIME& ft) {
  // Need to bit_cast to fix alignment, then divide by 10 to convert
  // 100-nanoseconds to microseconds. This only works on little-endian
  // machines.
  return bit_cast<int64_t, FILETIME>(ft) / 10;
}

int64_t CurrentTimePrecise() {
  FILETIME ft;
  ::GetSystemTimePreciseAsFileTime(&ft);
  return FileTimeToMicroseconds(ft);
}

int64_t CurrentTimeImprecise() {
  FILETIME ft;
  ::GetSystemTimeAsFileTime(&ft);
  return FileTimeToMicroseconds(ft);
}

}  // namespace

// This test case compares the performances of CurrentWallclockMicroseconds()
// implemented with using GetSystemTimeAsFileTime() or
// GetSystemTimePreciseAsFileTime().
TEST(WinTimePerfTest, Precise) {
  // The time interval that likely grabs a hardware timer interruption.
  static constexpr TimeDelta kInterval = Milliseconds(50);
  // The loop amount of calling the wall clock, it guaranties non zero amount of
  // time ticks.
  static constexpr int kLoop = 1000;

  int precise_counter = 0;
  TimeDelta precise_max_time;
  TimeDelta precise_min_time = TimeDelta::Max();

  TimeTicks begin = TimeTicks::Now();
  TimeTicks end = begin + kInterval;
  for (TimeTicks start = begin; start < end; start = TimeTicks::Now()) {
    for (int i = 0; i < kLoop; ++i) {
      int64_t current = CurrentTimePrecise();
      ::benchmark::DoNotOptimize(current);
    }

    const TimeDelta delta = TimeTicks::Now() - start;
    precise_min_time = std::min(precise_min_time, delta);
    precise_max_time = std::max(precise_max_time, delta);
    precise_counter += kLoop;
  }
  const TimeDelta precise_duration = TimeTicks::Now() - begin;

  int imprecise_counter = 0;
  TimeDelta imprecise_max_time;
  TimeDelta imprecise_min_time = TimeDelta::Max();

  begin = TimeTicks::Now();
  end = begin + kInterval;
  for (TimeTicks start = begin; start < end; start = TimeTicks::Now()) {
    for (int i = 0; i < kLoop; ++i) {
      int64_t current = CurrentTimeImprecise();
      ::benchmark::DoNotOptimize(current);
    }

    const TimeDelta delta = TimeTicks::Now() - start;
    imprecise_min_time = std::min(imprecise_min_time, delta);
    imprecise_max_time = std::max(imprecise_max_time, delta);
    imprecise_counter += kLoop;
  }
  const TimeDelta imprecise_duration = TimeTicks::Now() - begin;

  ASSERT_GT(precise_counter, 0);
  ASSERT_GT(imprecise_counter, 0);

  // Format output like in Google Benchmark.
  std::printf("----------------------------------------------------------\n");
  std::printf("             Min Time    Avg Time    Max Time   Iterations\n");
  std::printf("----------------------------------------------------------\n");
  std::printf("Precise   %8lld ns %8lld ns %8lld ns %12d\n",
              precise_min_time.InNanoseconds() / kLoop,
              precise_duration.InNanoseconds() / precise_counter,
              precise_max_time.InNanoseconds() / kLoop, precise_counter);
  std::printf("Imprecise %8lld ns %8lld ns %8lld ns %12d\n",
              imprecise_min_time.InNanoseconds() / kLoop,
              imprecise_duration.InNanoseconds() / imprecise_counter,
              imprecise_max_time.InNanoseconds() / kLoop, imprecise_counter);

  // Negative values mean the function ::GetSystemTimePreciseAsFileTime() wins.

  // Count of calls in kInterval (50) ms.
  const double count_delta = imprecise_counter - precise_counter;
  const double avg_delta = kInterval.InNanoseconds() / precise_counter -
                           kInterval.InNanoseconds() / imprecise_counter;
  const double min_delta =
      (precise_min_time.InNanoseconds() - imprecise_min_time.InNanoseconds()) /
      kLoop;
  const double max_delta =
      (precise_max_time.InNanoseconds() - imprecise_max_time.InNanoseconds()) /
      kLoop;

  perf_test::PerfResultReporter reporter("WinTime", "delta");
  reporter.RegisterFyiMetric(
      kCountDelta, StringPrintf("/%lldms", kInterval.InMilliseconds()));
  reporter.RegisterFyiMetric(kAvgDelta, "ns");
  reporter.RegisterFyiMetric(kMinDelta, "ns");
  reporter.RegisterFyiMetric(kMaxDelta, "ns");

  reporter.AddResult(kCountDelta, count_delta);
  reporter.AddResult(kAvgDelta, avg_delta);
  reporter.AddResult(kMinDelta, min_delta);
  reporter.AddResult(kMaxDelta, max_delta);
}

}  // namespace base
