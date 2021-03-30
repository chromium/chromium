// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/debug/stack_trace.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/timer/lap_timer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_result_reporter.h"

namespace base {
namespace debug {

// Change kTimeLimit to something higher if you need more time to capture a
// trace.
constexpr base::TimeDelta kTimeLimit = base::TimeDelta::FromSeconds(3);
constexpr int kWarmupRuns = 100;
constexpr int kTimeCheckInterval = 1000;
constexpr char kMetricStackTraceDuration[] = ".duration_per_run";
constexpr char kMetricStackTraceThroughput[] = ".throughput";
constexpr int kNumTracerObjAllocs = 5000;

perf_test::PerfResultReporter SetUpReporter(const std::string& story_name) {
  perf_test::PerfResultReporter reporter("StackTracePerf", story_name);
  reporter.RegisterImportantMetric(kMetricStackTraceDuration, "ns");
  reporter.RegisterImportantMetric(kMetricStackTraceThroughput, "runs/s");
  return reporter;
}

class StackTracer {
 public:
  StackTracer(size_t trace_count) : trace_count(trace_count) {}
  void Trace() {
    size_t tmp;
    base::debug::StackTrace st(trace_count);
    const void* addresses = st.Addresses(&tmp);
    // make sure a valid array of stack frames is returned
    EXPECT_NE(addresses, nullptr);
    // make sure the test generates the intended count of stack frames
    EXPECT_EQ(trace_count, tmp);
  }

 private:
  const size_t trace_count;
};

void MultiObjTest(size_t trace_count) {
  // Measures average stack trace generation (unwinding) performance across
  // multiple objects to get a more realistic figure. Calling
  // base::debug::StraceTrace() repeatedly from the same object may lead to
  // unrealistic performance figures that are optimised by the host (for
  // example, CPU caches distorting the results), whereas MTE requires
  // unwinding for allocations that occur all over the place.
  perf_test::PerfResultReporter reporter =
      SetUpReporter(base::StringPrintf("trace_count_%zu", trace_count));
  LapTimer timer(kWarmupRuns, kTimeLimit, kTimeCheckInterval,
                 LapTimer::TimerMethod::kUseTimeTicks);
  std::vector<std::unique_ptr<StackTracer>> tracers;
  for (int i = 0; i < kNumTracerObjAllocs; ++i) {
    tracers.push_back(std::make_unique<StackTracer>(trace_count));
  }
  std::vector<std::unique_ptr<StackTracer>>::iterator it = tracers.begin();
  timer.Start();
  do {
    (*it)->Trace();
    if (++it == tracers.end())
      it = tracers.begin();
    timer.NextLap();
  } while (!timer.HasTimeLimitExpired());
  reporter.AddResult(kMetricStackTraceDuration, timer.TimePerLap());
  reporter.AddResult(kMetricStackTraceThroughput, timer.LapsPerSecond());
}

class StackTracePerfTest : public testing::TestWithParam<size_t> {};

INSTANTIATE_TEST_SUITE_P(,
                         StackTracePerfTest,
                         ::testing::Range(size_t(4), size_t(16), size_t(4)));

TEST_P(StackTracePerfTest, MultiObj) {
  size_t parm = GetParam();
  MultiObjTest(parm);
}

}  // namespace debug
}  // namespace base
