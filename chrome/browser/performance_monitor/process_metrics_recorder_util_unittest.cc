// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_monitor/process_metrics_recorder_util.h"

#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_MAC)
#include "chrome/browser/performance_monitor/resource_coalition_mac.h"
#endif

namespace performance_monitor {

#if defined(OS_MAC)
TEST(ProcessMetricsRecorderUtilTest, RecordCoalitionData) {
  base::HistogramTester histogram_tester;
  ProcessMonitor::Metrics metrics;
  ResourceCoalition::DataRate coalition_data = {};

  coalition_data.cpu_time_per_second = 0.1;
  coalition_data.interrupt_wakeups_per_second = 0.3;
  coalition_data.platform_idle_wakeups_per_second = 2;
  coalition_data.bytesread_per_second = 10;
  coalition_data.byteswritten_per_second = 0.1;
  coalition_data.gpu_time_per_second = 0.8;
  coalition_data.energy_impact_per_second = 3.0;
  coalition_data.power_nw = 1000;

  for (int i = 0;
       i < static_cast<int>(ResourceCoalition::QoSLevels::kMaxValue) + 1; ++i) {
    coalition_data.qos_time_per_second[i] = i * 0.1;
  }

  metrics.coalition_data = coalition_data;

  std::vector<const char*> suffixes = {"", ".Foo", ".Bar"};
  RecordCoalitionData(metrics, suffixes);

  for (const char* scenario_suffix : suffixes) {
    // These histograms reports the CPU/GPU times as a percentage of time with a
    // permyriad granularity, 10% (0.1) will be represented as 1000.
    histogram_tester.ExpectUniqueSample(
        base::StrCat(
            {"PerformanceMonitor.ResourceCoalition.CPUTime2", scenario_suffix}),
        coalition_data.cpu_time_per_second * 10000, 1);
    histogram_tester.ExpectUniqueSample(
        base::StrCat(
            {"PerformanceMonitor.ResourceCoalition.GPUTime2", scenario_suffix}),
        coalition_data.gpu_time_per_second * 10000, 1);

    // These histograms report counts with a millievent/second granularity.
    histogram_tester.ExpectUniqueSample(
        base::StrCat(
            {"PerformanceMonitor.ResourceCoalition.InterruptWakeupsPerSecond",
             scenario_suffix}),
        coalition_data.interrupt_wakeups_per_second * 1000, 1);
    histogram_tester.ExpectUniqueSample(
        base::StrCat({"PerformanceMonitor.ResourceCoalition."
                      "PlatformIdleWakeupsPerSecond",
                      scenario_suffix}),
        coalition_data.platform_idle_wakeups_per_second * 1000, 1);
    histogram_tester.ExpectUniqueSample(
        base::StrCat({"PerformanceMonitor.ResourceCoalition.BytesReadPerSecond",
                      scenario_suffix}),
        coalition_data.bytesread_per_second * 1000, 1);
    histogram_tester.ExpectUniqueSample(
        base::StrCat(
            {"PerformanceMonitor.ResourceCoalition.BytesWrittenPerSecond",
             scenario_suffix}),
        coalition_data.byteswritten_per_second * 1000, 1);
    // EI is reported in centi-EI so the data needs to be multiplied by 100.0.
    histogram_tester.ExpectUniqueSample(
        base::StrCat({"PerformanceMonitor.ResourceCoalition.EnergyImpact",
                      scenario_suffix}),
        coalition_data.energy_impact_per_second * 100.0, 1);

    // Power is reported in milliwatts (mj/s), the data is in nj/s so it has to
    // be divided by 1000000.
    histogram_tester.ExpectUniqueSample(
        base::StrCat(
            {"PerformanceMonitor.ResourceCoalition.Power", scenario_suffix}),
        coalition_data.power_nw / 1000000, 1);

    // The QoS histograms also reports the CPU times as a percentage of time
    // with a permyriad granularity.
    histogram_tester.ExpectUniqueSample(
        base::StrCat({"PerformanceMonitor.ResourceCoalition.QoSLevel.Default",
                      scenario_suffix}),
        coalition_data.qos_time_per_second[0] * 10000, 1);
    histogram_tester.ExpectUniqueSample(
        base::StrCat(
            {"PerformanceMonitor.ResourceCoalition.QoSLevel.Maintenance",
             scenario_suffix}),
        coalition_data.qos_time_per_second[1] * 10000, 1);
    histogram_tester.ExpectUniqueSample(
        base::StrCat(
            {"PerformanceMonitor.ResourceCoalition.QoSLevel.Background",
             scenario_suffix}),
        coalition_data.qos_time_per_second[2] * 10000, 1);
    histogram_tester.ExpectUniqueSample(
        base::StrCat({"PerformanceMonitor.ResourceCoalition.QoSLevel.Utility",
                      scenario_suffix}),
        coalition_data.qos_time_per_second[3] * 10000, 1);
    histogram_tester.ExpectUniqueSample(
        base::StrCat({"PerformanceMonitor.ResourceCoalition.QoSLevel.Legacy",
                      scenario_suffix}),
        coalition_data.qos_time_per_second[4] * 10000, 1);
    histogram_tester.ExpectUniqueSample(
        base::StrCat(
            {"PerformanceMonitor.ResourceCoalition.QoSLevel.UserInitiated",
             scenario_suffix}),
        coalition_data.qos_time_per_second[5] * 10000, 1);
    histogram_tester.ExpectUniqueSample(
        base::StrCat(
            {"PerformanceMonitor.ResourceCoalition.QoSLevel.UserInteractive",
             scenario_suffix}),
        coalition_data.qos_time_per_second[6] * 10000, 1);
  }
}
#endif

}  // namespace performance_monitor
