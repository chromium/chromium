// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/rand_util.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_result_reporter.h"

namespace base {

namespace {

constexpr char kMetricPrefix[] = "RandUtil.";
constexpr char kThroughput[] = "throughput";

}  // namespace

TEST(RandUtilPerfTest, RandUint64) {
  uint64_t inclusive_or = 0;
  constexpr int kIterations = 1e7;

  auto before = base::TimeTicks::Now();
  for (int iter = 0; iter < kIterations; iter++) {
    inclusive_or |= base::RandUint64();
  }
  auto after = base::TimeTicks::Now();

  perf_test::PerfResultReporter reporter(kMetricPrefix, "RandUint64");
  reporter.RegisterImportantMetric(kThroughput, "ns / iteration");

  uint64_t nanos_per_iteration = (after - before).InNanoseconds() / kIterations;
  reporter.AddResult("throughput", static_cast<size_t>(nanos_per_iteration));
  ASSERT_NE(inclusive_or, static_cast<uint64_t>(0));
}

TEST(RandUtilPerfTest, InsecureRandomRandUint64) {
  base::InsecureRandomGenerator gen;

  uint64_t inclusive_or = 0;
  constexpr int kIterations = 1e7;

  auto before = base::TimeTicks::Now();
  for (int iter = 0; iter < kIterations; iter++) {
    inclusive_or |= gen.RandUint64();
  }
  auto after = base::TimeTicks::Now();

  perf_test::PerfResultReporter reporter(kMetricPrefix,
                                         "InsecureRandomRandUint64");
  reporter.RegisterImportantMetric(kThroughput, "ns / iteration");

  uint64_t nanos_per_iteration = (after - before).InNanoseconds() / kIterations;
  reporter.AddResult("throughput", static_cast<size_t>(nanos_per_iteration));
  ASSERT_NE(inclusive_or, static_cast<uint64_t>(0));
}

}  // namespace base
