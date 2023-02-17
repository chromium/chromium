// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/power/power_metrics.h"

#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#endif

namespace {

#if BUILDFLAG(IS_MAC)
power_metrics::CoalitionResourceUsageRate GetFakeResourceUsageRate() {
  power_metrics::CoalitionResourceUsageRate rate;
  rate.cpu_time_per_second = 0.5;
  rate.interrupt_wakeups_per_second = 10;
  rate.platform_idle_wakeups_per_second = 11;
  rate.bytesread_per_second = 12;
  rate.byteswritten_per_second = 13;
  rate.gpu_time_per_second = 0.6;
  rate.energy_impact_per_second = 15;
  rate.power_nw = 1000000;

  for (int i = 0; i < COALITION_NUM_THREAD_QOS_TYPES; ++i)
    rate.qos_time_per_second[i] = 0.1 * i;

  return rate;
}
#endif  // BUILDFLAG(IS_MAC)

struct HistogramSampleExpectation {
  std::string histogram_name_prefix;
  base::Histogram::Sample sample;
};

// For each histogram named after the combination of prefixes from
// `expectations` and suffixes from `suffixes`, verifies that there is a unique
// sample `expectation.sample`.
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

  // "Energy Impact" is a synthetic power estimation metric displayed by macOS
  // in Activity Monitor and the battery menu.
  process_metrics.energy_impact = 10.00;
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
        {"PerformanceMonitor.EnergyImpact2.Total", 10},
#endif
  });
}

#if BUILDFLAG(IS_MAC)
TEST(PowerMetricsTest, ReportShortIntervalHistograms) {
  base::HistogramTester histogram_tester;
  const char* kScenarioSuffix = ".AllTabsHidden_Audio";

  ReportShortIntervalHistograms(kScenarioSuffix, GetFakeResourceUsageRate());

  const std::vector<const char*> suffixes({"", kScenarioSuffix});
  ExpectHistogramSamples(
      &histogram_tester, suffixes,
      {{"PerformanceMonitor.ResourceCoalition.CPUTime2_10sec", 5000}});
}

TEST(PowerMetricsTest, ReportResourceCoalitionHistograms) {
  base::HistogramTester histogram_tester;

  const std::vector<const char*> suffixes = {"", ".Foo", ".Bar"};
  ReportResourceCoalitionHistograms(GetFakeResourceUsageRate(), suffixes);

  ExpectHistogramSamples(
      &histogram_tester, suffixes,
      {// These histograms reports the CPU/GPU times as a percentage of
       // time with a permyriad granularity, 10% (0.1) will be represented
       // as 1000.
       {"PerformanceMonitor.ResourceCoalition.CPUTime2", 5000},
       {"PerformanceMonitor.ResourceCoalition.GPUTime2", 6000},
       // These histograms report counts with a millievent/second
       // granularity.
       {"PerformanceMonitor.ResourceCoalition.InterruptWakeupsPerSecond",
        10000},
       {"PerformanceMonitor.ResourceCoalition."
        "PlatformIdleWakeupsPerSecond",
        11000},
       {"PerformanceMonitor.ResourceCoalition.BytesReadPerSecond2", 12},
       {"PerformanceMonitor.ResourceCoalition.BytesWrittenPerSecond2", 13},
       // EI is reported in centi-EI so the data needs to be multiplied by
       // 100.0.
       {"PerformanceMonitor.ResourceCoalition.EnergyImpact", 1500},
       // The QoS histograms also reports the CPU times as a percentage of
       // time with a permyriad granularity.
       {"PerformanceMonitor.ResourceCoalition.QoSLevel.Default", 0},
       {"PerformanceMonitor.ResourceCoalition.QoSLevel.Maintenance", 1000},
       {"PerformanceMonitor.ResourceCoalition.QoSLevel.Background", 2000},
       {"PerformanceMonitor.ResourceCoalition.QoSLevel.Utility", 3000},
       {"PerformanceMonitor.ResourceCoalition.QoSLevel.Legacy", 4000},
       {"PerformanceMonitor.ResourceCoalition.QoSLevel.UserInitiated", 5000},
       {"PerformanceMonitor.ResourceCoalition.QoSLevel.UserInteractive",
        6000}});

  if (base::mac::GetCPUType() == base::mac::CPUType::kArm) {
    ExpectHistogramSamples(
        &histogram_tester, suffixes,
        {// Power is reported in milliwatts (mj/s), the data
         // is in nj/s so it has to be divided by 1000000.
         {"PerformanceMonitor.ResourceCoalition.Power2", 1}});
  } else {
    histogram_tester.ExpectTotalCount(
        "PerformanceMonitor.ResourceCoalition.Power2", 0);
  }
}

// Verify that no energy impact histogram is reported when
// `CoalitionResourceUsageRate::energy_impact_per_second` is nullopt.
TEST(PowerMetricsTest, ReportResourceCoalitionHistograms_NoEnergyImpact) {
  base::HistogramTester histogram_tester;
  power_metrics::CoalitionResourceUsageRate rate = GetFakeResourceUsageRate();
  rate.energy_impact_per_second.reset();

  std::vector<const char*> suffixes = {"", ".Foo"};
  ReportResourceCoalitionHistograms(rate, suffixes);

  histogram_tester.ExpectTotalCount(
      "PerformanceMonitor.ResourceCoalition.EnergyImpact", 0);
  histogram_tester.ExpectTotalCount(
      "PerformanceMonitor.ResourceCoalition.EnergyImpact.Foo", 0);
}
#endif  // BUILDFLAG(IS_MAC)

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
