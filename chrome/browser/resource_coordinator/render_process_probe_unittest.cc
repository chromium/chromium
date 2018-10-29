// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/render_process_probe.h"
#include "base/process/process.h"
#include "base/process/process_metrics.h"
#include "base/task/post_task.h"
#include "base/test/metrics/histogram_tester.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/global_memory_dump.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace resource_coordinator {

namespace {

class TestingRenderProcessProbe : public RenderProcessProbeImpl {
 public:
  // Expose for testing.
  using RenderProcessProbeImpl::MeasurementOutcome;

  void SetMeasurementResults(
      bool global_success,
      std::unique_ptr<memory_instrumentation::GlobalMemoryDump> dump) {
    global_success_ = global_success;
    dump_ = std::move(dump);
  }

 protected:
  bool global_success_;
  std::unique_ptr<memory_instrumentation::GlobalMemoryDump> dump_;

  // Overridden for testing.
  void RegisterRenderProcesses() override;
  void StartMemoryMeasurement(base::TimeTicks collection_start_time) override;
  base::ProcessId GetProcessId(int host_id,
                               const RenderProcessInfo& info) override;
};

void TestingRenderProcessProbe::RegisterRenderProcesses() {
  // Register 4 renderer processes, all of which refer to this process.
  for (int id = 1; id < 5; ++id) {
    auto& render_process_info = render_process_info_map_[id];
    render_process_info.last_gather_cycle_active = current_gather_cycle_;
    render_process_info.process = base::Process::Current();
    render_process_info.metrics =
        base::ProcessMetrics::CreateCurrentProcessMetrics();
  }
}

void TestingRenderProcessProbe::StartMemoryMeasurement(
    base::TimeTicks collection_start_time) {
  // Post the stored results to the completion function.
  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::IO},
      base::BindOnce(&TestingRenderProcessProbe::
                         ProcessGlobalMemoryDumpAndDispatchOnIOThread,
                     base::Unretained(this), collection_start_time,
                     global_success_, std::move(dump_)));
}

base::ProcessId TestingRenderProcessProbe::GetProcessId(
    int host_id,
    const RenderProcessInfo& /* info */) {
  // Use the host ID for PID for the purposes of this test.
  return static_cast<base::ProcessId>(host_id);
}

class RenderProcessProbeTest : public testing::Test {
 protected:
  void RunGatherCycle(
      bool global_success,
      std::unique_ptr<memory_instrumentation::GlobalMemoryDump> dump);

  std::unique_ptr<memory_instrumentation::GlobalMemoryDump> CreateMemoryDump(
      int first_pid,
      int num_pids);

  content::TestBrowserThreadBundle browser_thread_bundle_;
  TestingRenderProcessProbe probe_;
};

void RenderProcessProbeTest::RunGatherCycle(
    bool global_success,
    std::unique_ptr<memory_instrumentation::GlobalMemoryDump> dump) {
  probe_.SetMeasurementResults(global_success, std::move(dump));
  probe_.StartSingleGather();
  browser_thread_bundle_.RunUntilIdle();
}

std::unique_ptr<memory_instrumentation::GlobalMemoryDump>
RenderProcessProbeTest::CreateMemoryDump(int first_pid, int num_pids) {
  memory_instrumentation::mojom::GlobalMemoryDumpPtr global_dump =
      memory_instrumentation::mojom::GlobalMemoryDump::New();

  for (int pid = first_pid; pid < first_pid + num_pids; ++pid) {
    memory_instrumentation::mojom::ProcessMemoryDumpPtr process_dump =
        memory_instrumentation::mojom::ProcessMemoryDump::New();

    process_dump->os_dump = memory_instrumentation::mojom::OSMemDump::New();
    process_dump->os_dump->private_footprint_kb = pid * 100;

    process_dump->process_type =
        memory_instrumentation::mojom::ProcessType::RENDERER;
    process_dump->pid = pid;

    global_dump->process_dumps.push_back(std::move(process_dump));
  }

  return memory_instrumentation::GlobalMemoryDump::MoveFrom(
      std::move(global_dump));
}

}  // namespace

TEST_F(RenderProcessProbeTest, FailureMetrics) {
  base::HistogramTester tester;

  // Full failure with a null dump.
  RunGatherCycle(false, nullptr);

  tester.ExpectTotalCount("ResourceCoordinator.Measurement.Duration", 1);

  tester.ExpectUniqueSample("ResourceCoordinator.Measurement.TotalProcesses", 4,
                            1);
  tester.ExpectUniqueSample(
      "ResourceCoordinator.Measurement.Memory.Outcome",
      TestingRenderProcessProbe::MeasurementOutcome::kMeasurementFailure, 1);
  tester.ExpectTotalCount(
      "ResourceCoordinator.Measurement.Memory.UnmeasuredProcesses", 0);
  tester.ExpectTotalCount(
      "ResourceCoordinator.Measurement.Memory.ExtraProcesses", 0);
}

TEST_F(RenderProcessProbeTest, PartialSuccessMetrics) {
  base::HistogramTester tester;

  // Partial failure with a full dump.
  RunGatherCycle(false, CreateMemoryDump(1, 4));

  tester.ExpectTotalCount("ResourceCoordinator.Measurement.Duration", 1);

  tester.ExpectUniqueSample("ResourceCoordinator.Measurement.TotalProcesses", 4,
                            1);
  tester.ExpectUniqueSample(
      "ResourceCoordinator.Measurement.Memory.Outcome",
      TestingRenderProcessProbe::MeasurementOutcome::kMeasurementPartialSuccess,
      1);
  tester.ExpectUniqueSample(
      "ResourceCoordinator.Measurement.Memory.UnmeasuredProcesses", 0, 1);
  tester.ExpectUniqueSample(
      "ResourceCoordinator.Measurement.Memory.ExtraProcesses", 0, 1);
}

TEST_F(RenderProcessProbeTest, SuccessMetrics) {
  base::HistogramTester tester;

  // Full success with a skewed dump, missing one process while contributing
  // an extra.
  RunGatherCycle(true, CreateMemoryDump(2, 4));

  tester.ExpectTotalCount("ResourceCoordinator.Measurement.Duration", 1);

  tester.ExpectUniqueSample("ResourceCoordinator.Measurement.TotalProcesses", 4,
                            1);
  tester.ExpectUniqueSample(
      "ResourceCoordinator.Measurement.Memory.Outcome",
      TestingRenderProcessProbe::MeasurementOutcome::kMeasurementSuccess, 1);
  tester.ExpectUniqueSample(
      "ResourceCoordinator.Measurement.Memory.UnmeasuredProcesses", 1, 1);
  tester.ExpectUniqueSample(
      "ResourceCoordinator.Measurement.Memory.ExtraProcesses", 1, 1);
}

}  // namespace resource_coordinator
