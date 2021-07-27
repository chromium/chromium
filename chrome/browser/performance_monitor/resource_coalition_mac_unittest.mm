// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_monitor/resource_coalition_mac.h"

#include <cstdint>
#include <limits>

#include "base/logging.h"
#include "base/rand_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
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

}  // namespace performance_monitor
