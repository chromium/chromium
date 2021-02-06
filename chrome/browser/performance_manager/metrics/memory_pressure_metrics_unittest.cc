// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/metrics/memory_pressure_metrics.h"
#include <memory>

#include "base/memory/memory_pressure_listener.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/util/memory_pressure/fake_memory_pressure_monitor.h"
#include "build/build_config.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {
namespace metrics {

class MemoryPressureMetricsTest : public GraphTestHarness {
 public:
  using Super = GraphTestHarness;

  MemoryPressureMetricsTest() = default;
  ~MemoryPressureMetricsTest() override = default;
  MemoryPressureMetricsTest(const MemoryPressureMetricsTest& other) = delete;
  MemoryPressureMetricsTest& operator=(const MemoryPressureMetricsTest&) =
      delete;

  void SetUp() override {
    Super::SetUp();
    std::unique_ptr<MemoryPressureMetrics> metrics =
        std::make_unique<MemoryPressureMetrics>();
    metrics_ = metrics.get();
    graph()->PassToGraph(std::move(metrics));

    process_node_ = CreateNode<performance_manager::ProcessNodeImpl>();
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  void TearDown() override {
    process_node_.reset();
    histogram_tester_.reset();
    Super::TearDown();
  }

 protected:
  void SimulateMemoryPressure() {
    mem_pressure_monitor_.SetAndNotifyMemoryPressure(
        base::MemoryPressureListener::MemoryPressureLevel::
            MEMORY_PRESSURE_LEVEL_CRITICAL);
    task_env().RunUntilIdle();
  }

  ProcessNodeImpl* process_node() { return process_node_.get(); }
  base::HistogramTester* histogram_tester() { return histogram_tester_.get(); }
  MemoryPressureMetrics* metrics() { return metrics_; }

 private:
  MemoryPressureMetrics* metrics_;
  util::test::FakeMemoryPressureMonitor mem_pressure_monitor_;
  performance_manager::TestNodeWrapper<performance_manager::ProcessNodeImpl>
      process_node_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

// Very flaky on Android. http://crbug.com/1069043.
#if defined(OS_ANDROID)
#define MAYBE_TestHistograms DISABLED_TestHistograms
#else
#define MAYBE_TestHistograms TestHistograms
#endif

TEST_F(MemoryPressureMetricsTest, MAYBE_TestHistograms) {
  const int kFakeSystemRamMb = 4096;
  // Pretends that we have one process using half of the RAM.
  process_node()->set_resident_set_kb(kFakeSystemRamMb * 1024 / 2);
  metrics()->set_system_ram_mb_for_testing(kFakeSystemRamMb);

  SimulateMemoryPressure();

  histogram_tester()->ExpectBucketCount(
      "Discarding.OnCriticalPressure.TotalRSS_Mb", kFakeSystemRamMb / 2, 1);

  histogram_tester()->ExpectBucketCount(
      "Discarding.OnCriticalPressure.TotalRSS_PercentOfRAM", 50, 1);
}

}  // namespace metrics
}  // namespace performance_manager
