// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/vlog.h"

#include "base/logging.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_result_reporter.h"

namespace logging {

TEST(VlogPerfTest, GetVlogLevel) {
  const std::string kMetricThroughput = "throughput";
  perf_test::PerfResultReporter reporter(/*metric_basename=*/"Vlog",
                                         /*story_name=*/"GetVlogLevel");
  reporter.RegisterImportantMetric(/*metric_suffix=*/kMetricThroughput,
                                   /*units=*/"gets/microsecond");

  const base::TimeTicks job_run_start(base::TimeTicks::Now());

  const char kVModuleSwitch[] =
      "foo/bar.cc=1,baz\\*\\qux.cc=2,*quux/*=3,*/*-inl.h=4";

  constexpr size_t kNumFilenames = 3;
  const char* const kFileNames[kNumFilenames] = {"baz/x/qux.cc", "foo/bar",
                                                 "x/y-inl.h"};
  int min_log_level = 0;
  VlogInfo vlog_info(std::string(), kVModuleSwitch, &min_log_level);

  constexpr size_t kNumSets = 1 << 21;
  constexpr size_t kNumReps = 3;

  for (size_t set = 0; set < kNumSets; set++) {
    const char* kFileName = kFileNames[set % kNumFilenames];
    for (size_t rep = 0; rep < kNumReps; rep++) {
      int res = vlog_info.GetVlogLevel(kFileName);
      ASSERT_GE(res, min_log_level);
      ASSERT_LE(res, 4);
    }
  }

  const base::TimeDelta kDuration = base::TimeTicks::Now() - job_run_start;
  const int64_t kDurationUs = kDuration.InMicroseconds();
  ASSERT_NE(kDurationUs, 0) << "Too fast, would divide by zero.";

  base::CheckedNumeric<size_t> gets_per_us_checked = kNumSets;
  gets_per_us_checked *= kNumReps;
  gets_per_us_checked /= kDurationUs;

  const size_t kGetsPerUs = gets_per_us_checked.ValueOrDie();
  reporter.AddResult(kMetricThroughput, kGetsPerUs);
}
}  // namespace logging
