// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/power/power_metrics.h"

#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

struct HistogramSampleExpectation {
  std::string histogram_name_prefix;
  base::Histogram::Sample sample;
};

// For each histogram named after the combination of prefixes from
// `expectations` and suffixes from `suffixes`, verifies that there is a
// unique sample `expectation.sample`.
void ExpectHistogramSamples(
    base::HistogramTester* histogram_tester,
    const std::vector<const char*>& suffixes,
    const std::vector<HistogramSampleExpectation>& expectations) {
  for (const char* suffix : suffixes) {
    for (const auto& expectation : expectations) {
      std::string histogram_name =
          base::StrCat({expectation.histogram_name_prefix, suffix});
      SCOPED_TRACE(histogram_name);
      histogram_tester->ExpectUniqueSample(histogram_name, expectation.sample,
                                           1);
    }
  }
}

}  // namespace

TEST(PowerMetricsTest, ReportAggregatedProcessMetricsHistograms) {
  base::HistogramTester histogram_tester;
  const std::vector<const char*> suffixes = {"", ".Foo", ".Bar"};

  ProcessMonitor::Metrics process_metrics;
  process_metrics.cpu_usage = 0.20;
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || \
    BUILDFLAG(IS_AIX)
  // Returns the number of average idle cpu wakeups per second since the last
  // time the metric was sampled.
  process_metrics.idle_wakeups = 51;
#endif
#if BUILDFLAG(IS_MAC)
  // The number of average "package idle exits" per second since the last
  // time the metric was sampled. See base/process/process_metrics.h for a
  // more detailed explanation.
  process_metrics.package_idle_wakeups = 52;
#endif

  ReportAggregatedProcessMetricsHistograms(process_metrics, suffixes);

  ExpectHistogramSamples(&histogram_tester, suffixes, {
// Windows ARM64 does not support Constant Rate TSC so
// PerformanceMonitor.AverageCPU8.Total is not recorded there.
#if !BUILDFLAG(IS_WIN) || !defined(ARCH_CPU_ARM64)
    {"PerformanceMonitor.AverageCPU8.Total", 20},
#endif

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || \
    BUILDFLAG(IS_AIX)
        {"PerformanceMonitor.IdleWakeups2.Total", 51},
#endif

#if BUILDFLAG(IS_MAC)
        {"PerformanceMonitor.PackageExitIdleWakeups2.Total", 52},
#endif
  });
}

TEST(PowerMetricsTest, CalculateDischargeRateMilliwatts_mWh) {
  int64_t discharge_rate = CalculateDischargeRateMilliwatts(
      base::BatteryLevelProvider::BatteryState{
          .battery_count = 1,
          .is_external_power_connected = false,
          .current_capacity = 100,
          .full_charged_capacity = 10000,
          .charge_unit = base::BatteryLevelProvider::BatteryLevelUnit::kMWh,
      },
      base::BatteryLevelProvider::BatteryState{
          .battery_count = 1,
          .is_external_power_connected = false,
          .current_capacity = 90,
          .full_charged_capacity = 10000,
          .charge_unit = base::BatteryLevelProvider::BatteryLevelUnit::kMWh,
      },
      base::Minutes(1));

  // 10 mWh discharge in 1 minute translates to 600 mWh in 1 hour.
  EXPECT_EQ(discharge_rate, 600);
}

TEST(PowerMetricsTest, CalculateDischargeRateMilliwatts_mAh) {
  int64_t discharge_rate = CalculateDischargeRateMilliwatts(
      base::BatteryLevelProvider::BatteryState{
          .battery_count = 1,
          .is_external_power_connected = false,
          .current_capacity = 100,
          .full_charged_capacity = 10000,
          .voltage_mv = 12100,
          .charge_unit = base::BatteryLevelProvider::BatteryLevelUnit::kMAh,
      },
      base::BatteryLevelProvider::BatteryState{
          .battery_count = 1,
          .is_external_power_connected = false,
          .current_capacity = 90,
          .full_charged_capacity = 10000,
          .voltage_mv = 11900,
          .charge_unit = base::BatteryLevelProvider::BatteryLevelUnit::kMAh,
      },
      base::Minutes(1));

  // 10 mAh discharge in 1 minute translates to 600 mWh in 1 hour. That value is
  // then multiplied by the average voltage (12v) to get 7200 milliwatts.
  EXPECT_EQ(discharge_rate, 7200);
}
