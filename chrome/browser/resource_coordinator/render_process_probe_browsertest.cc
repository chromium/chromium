// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <string>

#include "base/feature_list.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/resource_coordinator/render_process_probe.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/test/test_utils.h"
#include "url/gurl.h"

namespace resource_coordinator {
namespace {

class TestingRenderProcessProbe : public RenderProcessProbeImpl {
 public:
  // Make these types public for testing.
  using RenderProcessProbeImpl::RenderProcessInfo;
  using RenderProcessProbeImpl::RenderProcessInfoMap;

  TestingRenderProcessProbe() = default;
  ~TestingRenderProcessProbe() override = default;

  void DispatchMetricsOnUIThread(
      mojom::ProcessResourceMeasurementBatchPtr batch) override {
    last_measurement_batch_ = std::move(batch);

    current_run_loop_->QuitWhenIdle();
  }

  // Returns |true| if all of the elements in |*render_process_info_map_|
  // are up-to-date with |current_gather_cycle_|.
  bool AllMeasurementsAreAtCurrentCycle() const {
    for (auto& render_process_info_map_entry : render_process_info_map_) {
      auto& render_process_info = render_process_info_map_entry.second;
      if (render_process_info.last_gather_cycle_active !=
              current_gather_cycle_ ||
          render_process_info.cpu_usage == kUninitializedCPUTime) {
        return false;
      }
    }
    return true;
  }

  const mojom::ProcessResourceMeasurementBatchPtr& last_measurement_batch()
      const {
    return last_measurement_batch_;
  }

  const RenderProcessInfoMap& render_process_info_map() const {
    return render_process_info_map_;
  }

  size_t current_gather_cycle() const { return current_gather_cycle_; }

  void WaitForGather() {
    base::RunLoop run_loop;
    current_run_loop_ = &run_loop;

    run_loop.Run();

    current_run_loop_ = nullptr;
  }

  void StartSingleGatherAndWait() {
    StartSingleGather();
    WaitForGather();
  }

 private:
  base::RunLoop* current_run_loop_ = nullptr;

  mojom::ProcessResourceMeasurementBatchPtr last_measurement_batch_;

  DISALLOW_COPY_AND_ASSIGN(TestingRenderProcessProbe);
};

}  // namespace

class RenderProcessProbeBrowserTest : public InProcessBrowserTest {
 public:
  RenderProcessProbeBrowserTest() = default;
  ~RenderProcessProbeBrowserTest() override = default;

  static bool AtLeastOneMemoryMeasurementIsNonZero(
      const mojom::ProcessResourceMeasurementBatchPtr& batch) {
    for (const auto& measurement : batch->measurements) {
      if (measurement->private_footprint_kb > 0)
        return true;
    }

    return false;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(RenderProcessProbeBrowserTest);
};

IN_PROC_BROWSER_TEST_F(RenderProcessProbeBrowserTest,
                       TrackAndMeasureActiveRenderProcesses) {
#if defined(OS_WIN)
  // TODO(https://crbug.com/833430): Spare-RPH-related failures when run with
  // --site-per-process on Windows.
  if (content::AreAllSitesIsolatedForTesting())
    return;
#endif
  TestingRenderProcessProbe probe;

  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(0u, probe.current_gather_cycle());

  // A tab is already open when the test begins.
  probe.StartSingleGatherAndWait();
  EXPECT_EQ(1u, probe.current_gather_cycle());
  size_t initial_size = probe.render_process_info_map().size();
  EXPECT_LE(1u, initial_size);
  EXPECT_GE(2u, initial_size);  // If a spare RenderProcessHost is present.
  EXPECT_TRUE(probe.AllMeasurementsAreAtCurrentCycle());
  EXPECT_EQ(initial_size, probe.last_measurement_batch()->measurements.size());

  // The CPU measurement is cumulative since the start of process, but it could
  // be zero due to the measurement granularity of the OS.
  std::map<uint32_t, base::TimeDelta> cpu_usage_map;
  for (const auto& measurement : probe.last_measurement_batch()->measurements) {
    EXPECT_LE(base::TimeDelta(), measurement->cpu_usage);
    EXPECT_TRUE(
        cpu_usage_map
            .insert(std::make_pair(measurement->pid, measurement->cpu_usage))
            .second);
  }

  // There is an inherent race in memory measurement that may cause failures
  // which will result in zero private footprint returns. To work around this,
  // assert that there is at least one non-zero measurement.
  AtLeastOneMemoryMeasurementIsNonZero(probe.last_measurement_batch());

  // Open a second tab and complete a navigation.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), embedded_test_server()->GetURL("/title1.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB |
          ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  probe.StartSingleGatherAndWait();
  EXPECT_EQ(2u, probe.current_gather_cycle());
  EXPECT_EQ(initial_size + 1u, probe.render_process_info_map().size());
  EXPECT_EQ(initial_size + 1u,
            probe.last_measurement_batch()->measurements.size());
  EXPECT_TRUE(probe.AllMeasurementsAreAtCurrentCycle());

  // Verify that the elements in the map are reused across multiple
  // measurement cycles.
  using RenderProcessInfo = TestingRenderProcessProbe::RenderProcessInfo;
  std::map<int, const RenderProcessInfo*> info_map;
  for (const auto& entry : probe.render_process_info_map()) {
    const RenderProcessInfo& info = entry.second;
    EXPECT_TRUE(info_map.insert(std::make_pair(entry.first, &info)).second);
  }

  size_t info_map_size = info_map.size();
  probe.StartSingleGatherAndWait();
  // Verify that CPU usage is monotonically increasing, though the measurement
  // granulatity is such on some OSes that a zero difference is almost certain.
  for (const auto& measurement : probe.last_measurement_batch()->measurements) {
    if (cpu_usage_map.find(measurement->pid) != cpu_usage_map.end()) {
      EXPECT_LE(cpu_usage_map[measurement->pid], measurement->cpu_usage);
    }
  }

  AtLeastOneMemoryMeasurementIsNonZero(probe.last_measurement_batch());

  EXPECT_EQ(info_map_size, info_map.size());
  for (const auto& entry : probe.render_process_info_map()) {
    const int key = entry.first;
    const RenderProcessInfo& info = entry.second;

    EXPECT_EQ(&info, info_map[key]);
  }

  // Kill one of the two open tabs.
  EXPECT_TRUE(browser()
                  ->tab_strip_model()
                  ->GetActiveWebContents()
                  ->GetMainFrame()
                  ->GetProcess()
                  ->FastShutdownIfPossible());
  probe.StartSingleGatherAndWait();
  EXPECT_EQ(4u, probe.current_gather_cycle());
  EXPECT_EQ(initial_size, probe.render_process_info_map().size());
  EXPECT_EQ(initial_size, probe.last_measurement_batch()->measurements.size());
  EXPECT_TRUE(probe.AllMeasurementsAreAtCurrentCycle());
}

}  // namespace resource_coordinator
