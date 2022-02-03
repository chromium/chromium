// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_monitor/resource_coalition_mac.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/rand_util.h"
#include "base/strings/string_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "chrome/common/chrome_paths.h"
#include "components/power_metrics/resource_coalition_internal_types_mac.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace performance_monitor {

namespace {

constexpr mach_timebase_info_data_t kIntelTimebase = {1, 1};

// Initializes to no EI constants and the Intel timebase.
class TestResourceCoalition : public ResourceCoalition {
 public:
  TestResourceCoalition() {
    SetEnergyImpactCoefficientsForTesting(absl::nullopt);
    SetMachTimebaseForTesting(kIntelTimebase);
  }
  // Expose as public for testing.
  using ResourceCoalition::SetCoalitionIDToCurrentProcessIdForTesting;
  using ResourceCoalition::GetDataRateFromFakeDataForTesting;
  using ResourceCoalition::SetEnergyImpactCoefficientsForTesting;
  using ResourceCoalition::SetMachTimebaseForTesting;
};

}  // namespace

TEST(ResourceCoalitionTests, Basics) {
  base::HistogramTester histogram_tester;
  TestResourceCoalition coalition;
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
  constexpr base::TimeDelta busy_time = base::Seconds(1);
  [[maybe_unused]] volatile double number = 1;
  while (base::TimeTicks::Now() < (begin + busy_time)) {
    for (int i = 0; i < 10000; ++i) {
      number *= base::RandDouble() / std::numeric_limits<double>::max() * 2;
    }
  }

  auto sample = coalition.GetDataRate();
  EXPECT_TRUE(sample.has_value());
  EXPECT_NE(sample->cpu_time_per_second, 0);
}

}  // namespace performance_monitor
