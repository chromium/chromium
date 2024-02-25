// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/metrics/memory_pressure_metrics.h"
#include <memory>

#include "base/memory/memory_pressure_listener.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/graph/system_node_impl.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "components/performance_manager/test_support/mock_graphs.h"
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
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  void TearDown() override {
    histogram_tester_.reset();
    Super::TearDown();
  }

 protected:
  base::HistogramTester* histogram_tester() { return histogram_tester_.get(); }
  MemoryPressureMetrics* metrics() { return metrics_; }

 private:
  raw_ptr<MemoryPressureMetrics> metrics_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

// Very flaky on Android. http://crbug.com/1069043.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_TestHistograms DISABLED_TestHistograms
#else
#define MAYBE_TestHistograms TestHistograms
#endif

TEST_F(MemoryPressureMetricsTest, MAYBE_TestHistograms) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  const int kFakeSystemRamMb = 4096;
  // Pretends that we have one process using half of the RAM.
  mock_graph.process->set_resident_set_kb(kFakeSystemRamMb * 1024 / 2);
  metrics()->set_system_ram_mb_for_testing(kFakeSystemRamMb);

  mock_graph.system->OnMemoryPressureForTesting(
      base::MemoryPressureListener::MemoryPressureLevel::
          MEMORY_PRESSURE_LEVEL_CRITICAL);
}

}  // namespace metrics
}  // namespace performance_manager
