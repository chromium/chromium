// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_monitor/process_metrics_recorder_util.h"

#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "chrome/browser/performance_monitor/resource_coalition_mac.h"
#include "testing/gtest/include/gtest/gtest.h"

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
  coalition_data.power_nw = 1000;

  metrics.coalition_data = coalition_data;
  RecordCoalitionData(metrics);

  // These histograms reports the CPU/GPU times as a percentage of time with a
  // permyriad granularity, 10% (0.1) will be represented as 1000.
  histogram_tester.ExpectUniqueSample(
      "PerformanceMonitor.ResourceCoalition.CPUTime2",
      coalition_data.cpu_time_per_second * 10000, 1);
  histogram_tester.ExpectUniqueSample(
      "PerformanceMonitor.ResourceCoalition.GPUTime2",
      coalition_data.gpu_time_per_second * 10000, 1);

  // These histograms report counts with a millievent/second granularity.
  histogram_tester.ExpectUniqueSample(
      "PerformanceMonitor.ResourceCoalition.InterruptWakeupsPerSecond",
      coalition_data.interrupt_wakeups_per_second * 1000, 1);
  histogram_tester.ExpectUniqueSample(
      "PerformanceMonitor.ResourceCoalition.PlatformIdleWakeupsPerSecond",
      coalition_data.platform_idle_wakeups_per_second * 1000, 1);
  histogram_tester.ExpectUniqueSample(
      "PerformanceMonitor.ResourceCoalition.BytesReadPerSecond",
      coalition_data.bytesread_per_second * 1000, 1);
  histogram_tester.ExpectUniqueSample(
      "PerformanceMonitor.ResourceCoalition.BytesWrittenPerSecond",
      coalition_data.byteswritten_per_second * 1000, 1);
  // Power is reported in milliwatts (mj/s), the data is in nj/s so it has to
  // be divided by 1000000.
  histogram_tester.ExpectUniqueSample(
      "PerformanceMonitor.ResourceCoalition.Power",
      coalition_data.power_nw / 1000000, 1);
}
#endif

}  // namespace performance_monitor
