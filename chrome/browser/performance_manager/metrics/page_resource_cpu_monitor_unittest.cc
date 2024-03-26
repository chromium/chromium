// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/metrics/page_resource_cpu_monitor.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/check.h"
#include "base/memory/weak_ptr.h"
#include "base/process/kill.h"
#include "base/process/process.h"
#include "base/process/process_handle.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/test/test_waitable_event.h"
#include "base/time/time.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/graph/worker_node_impl.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/performance_manager/public/resource_attribution/resource_contexts.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "components/performance_manager/test_support/mock_graphs.h"
#include "components/performance_manager/test_support/resource_attribution/measurement_delegates.h"
#include "components/performance_manager/test_support/run_in_graph.h"
#include "components/performance_manager/test_support/test_harness_helper.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/process_type.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/navigation_simulator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace performance_manager::metrics {

namespace {

using ::testing::_;
using ::testing::AnyOf;
using ::testing::DoubleEq;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::Optional;

using resource_attribution::ResourceContext;
using ProcessCPUUsageError =
    resource_attribution::CPUMeasurementDelegate::ProcessCPUUsageError;

constexpr base::TimeDelta kTimeBetweenMeasurements = base::Minutes(5);

// A struct holding node pointers for a renderer process containing a single
// page with a single main frame.
struct SinglePageRendererNodes {
  TestNodeWrapper<TestProcessNodeImpl> process_node;
  TestNodeWrapper<PageNodeImpl> page_node;
  TestNodeWrapper<FrameNodeImpl> frame_node;

  // The resource context of `frame_node`.
  ResourceContext resource_context;
};

// Helpers to lookup measurement results from TestNodeWrapper's.

std::optional<double> GetMeasurementResult(
    const PageResourceCPUMonitor::CPUUsageMap& cpu_usage_map,
    const ResourceContext& context) {
  const auto it = cpu_usage_map.find(context);
  if (it == cpu_usage_map.end()) {
    return std::nullopt;
  }
  return it->second;
}

std::optional<double> GetMeasurementResult(
    const PageResourceCPUMonitor::CPUUsageMap& cpu_usage_map,
    const TestNodeWrapper<FrameNodeImpl>& frame_wrapper) {
  return GetMeasurementResult(cpu_usage_map,
                              frame_wrapper->GetResourceContext());
}

std::optional<double> GetMeasurementResult(
    const PageResourceCPUMonitor::CPUUsageMap& cpu_usage_map,
    const TestNodeWrapper<WorkerNodeImpl>& worker_wrapper) {
  return GetMeasurementResult(cpu_usage_map,
                              worker_wrapper->GetResourceContext());
}

}  // namespace

class PageResourceCPUMonitorTest : public GraphTestHarness {
 protected:
  using Super = GraphTestHarness;

  void SetUp() override {
    Super::SetUp();

    mock_graph_ =
        std::make_unique<MockMultiplePagesAndWorkersWithMultipleProcessesGraph>(
            graph());
    mock_utility_process_ =
        CreateNode<ProcessNodeImpl>(content::PROCESS_TYPE_UTILITY);
    mock_utility_process_->SetProcess(base::Process::Current(),
                                      /*launch_time=*/base::TimeTicks::Now());

    // These tests validate specific timing of measurements around process
    // creation and destruction.
    delegate_factory_.SetRequireValidProcesses(true);
    cpu_monitor_.SetCPUMeasurementDelegateFactoryForTesting(graph(),
                                                            &delegate_factory_);
  }

  // Creates a renderer process containing a single page and frame, for simple
  // tracking of CPU usage.
  SinglePageRendererNodes CreateSimpleCPUTrackingRenderer() {
    // CreateNode's default arguments create a renderer process node.
    // TestProcessNodeImpl from mock_graphs.h gets a valid child id.
    auto process_node = CreateNode<TestProcessNodeImpl>();
    auto page_node = CreateNode<PageNodeImpl>();
    auto frame_node =
        CreateFrameNodeAutoId(process_node.get(), page_node.get());
    auto resource_context = frame_node->GetResourceContext();

    // By default simulate 100% CPU usage in the renderer. To override this call
    // SetProcessCPUUsage again before advancing the clock.
    SetProcessCPUUsage(process_node.get(), 1.0);

    return SinglePageRendererNodes{
        .process_node = std::move(process_node),
        .page_node = std::move(page_node),
        .frame_node = std::move(frame_node),
        .resource_context = std::move(resource_context),
    };
  }

  void SetProcessId(ProcessNodeImpl* process_node) {
    // Assigns the current process object to the node, including its pid.
    process_node->SetProcess(base::Process::Current(), base::TimeTicks::Now());
  }

  void SetProcessExited(ProcessNodeImpl* process_node) {
    process_node->SetProcessExitStatus(0);
    // After a process exits, GetCumulativeCPUUsage() starts returning an error.
    SetProcessCPUUsageError(process_node,
                            ProcessCPUUsageError::kProcessNotFound);
  }

  void SetProcessCPUUsage(const ProcessNodeImpl* process_node, double usage) {
    delegate_factory_.GetDelegate(process_node).SetCPUUsage(usage);
  }

  void SetProcessCPUUsageError(const ProcessNodeImpl* process_node,
                               std::optional<ProcessCPUUsageError> error) {
    delegate_factory_.GetDelegate(process_node).SetError(error);
  }

  // Factory to return CPUMeasurementDelegates for `cpu_monitor_`. This must be
  // created before `cpu_monitor_` and deleted afterward to ensure that it
  // outlives all delegates it creates.
  resource_attribution::SimulatedCPUMeasurementDelegateFactory
      delegate_factory_;

  // The object under test.
  PageResourceCPUMonitor cpu_monitor_;

  TestNodeWrapper<ProcessNodeImpl> mock_utility_process_;

  std::unique_ptr<MockMultiplePagesAndWorkersWithMultipleProcessesGraph>
      mock_graph_;
};

TEST_F(PageResourceCPUMonitorTest, CPUMeasurement) {
  // Create several renderer processes to measure. Put one page and one frame in
  // each renderer, so CPU measurements for the renderer are all assigned to
  // that frame for easy validation.

  // Renderer in existence before StartMonitoring().
  const SinglePageRendererNodes renderer1 = CreateSimpleCPUTrackingRenderer();
  SetProcessId(renderer1.process_node.get());

  // Renderer starts and exits before StartMonitoring().
  const SinglePageRendererNodes early_exit_renderer =
      CreateSimpleCPUTrackingRenderer();
  SetProcessId(early_exit_renderer.process_node.get());
  SetProcessExited(early_exit_renderer.process_node.get());

  // Renderer creation racing with StartMonitoring(). Its pid will not be
  // available until after monitoring starts .
  const SinglePageRendererNodes renderer2 = CreateSimpleCPUTrackingRenderer();
  ASSERT_EQ(renderer2.process_node->GetProcessId(), base::kNullProcessId);

  // `renderer1` begins measurement as soon as StartMonitoring is called.
  // `renderer2` begins measurement when its pid is available.
  cpu_monitor_.StartMonitoring(graph());

  task_env().FastForwardBy(kTimeBetweenMeasurements / 2);
  SetProcessId(renderer2.process_node.get());

  // Renderer created halfway through the measurement interval.
  const SinglePageRendererNodes renderer3 = CreateSimpleCPUTrackingRenderer();
  SetProcessId(renderer3.process_node.get());

  // Renderer creation racing with UpdateCPUMeasurements(). `renderer4`'s pid
  // will become available on the same tick the measurement is taken,
  // `renderer5`'s pid will become available after the measurement.
  const SinglePageRendererNodes renderer4 = CreateSimpleCPUTrackingRenderer();
  const SinglePageRendererNodes renderer5 = CreateSimpleCPUTrackingRenderer();

  // Finish next measurement interval.
  task_env().FastForwardBy(kTimeBetweenMeasurements / 2);
  SetProcessId(renderer4.process_node.get());

  // `renderer1` existed for the entire measurement period.
  // `renderer2` existed for all of it, but was only measured for the last half,
  // after its pid became available.
  // `renderer3` only existed for the last half.
  // `renderer4` existed for the measurement but no time passed so it had 0% CPU
  // usage. (As an optimization the monitor may not include it in the results.)
  // `renderer5` is not measured yet.
  {
    auto measurements = cpu_monitor_.UpdateCPUMeasurements();
    EXPECT_THAT(GetMeasurementResult(measurements,
                                     early_exit_renderer.resource_context),
                Eq(std::nullopt));
    EXPECT_THAT(GetMeasurementResult(measurements, renderer1.resource_context),
                Optional(DoubleEq(1.0)));
    EXPECT_THAT(GetMeasurementResult(measurements, renderer2.resource_context),
                Optional(DoubleEq(0.5)));
    EXPECT_THAT(GetMeasurementResult(measurements, renderer3.resource_context),
                Optional(DoubleEq(0.5)));
    EXPECT_THAT(GetMeasurementResult(measurements, renderer4.resource_context),
                AnyOf(Optional(DoubleEq(0.0)), Eq(std::nullopt)));
    EXPECT_THAT(GetMeasurementResult(measurements, renderer5.resource_context),
                Eq(std::nullopt));
  }

  SetProcessId(renderer5.process_node.get());

  // Finish next measurement interval.
  task_env().FastForwardBy(kTimeBetweenMeasurements);

  // All nodes existed for entire measurement interval.
  {
    auto measurements = cpu_monitor_.UpdateCPUMeasurements();
    EXPECT_THAT(GetMeasurementResult(measurements, renderer1.resource_context),
                Optional(DoubleEq(1.0)));
    EXPECT_THAT(GetMeasurementResult(measurements, renderer2.resource_context),
                Optional(DoubleEq(1.0)));
    EXPECT_THAT(GetMeasurementResult(measurements, renderer3.resource_context),
                Optional(DoubleEq(1.0)));
    EXPECT_THAT(GetMeasurementResult(measurements, renderer4.resource_context),
                Optional(DoubleEq(1.0)));
    EXPECT_THAT(GetMeasurementResult(measurements, renderer5.resource_context),
                Optional(DoubleEq(1.0)));
  }

  // `renderer1` drops to 50% CPU usage for the next period.
  // `renderer2` stays at 100% for the first half, 50% for the last half
  // (average 75%).
  // `renderer3` drops to 0% for a time, returns to 100% for half that time,
  // then drops to 0% again (average 25%).
  // `renderer4` drops to 0% at the end of the period. It should still show 100%
  // since no time passes before measuring.
  SetProcessCPUUsage(renderer1.process_node.get(), 0.5);
  SetProcessCPUUsage(renderer3.process_node.get(), 0.0);
  task_env().FastForwardBy(kTimeBetweenMeasurements / 2);
  SetProcessCPUUsage(renderer2.process_node.get(), 0.5);
  SetProcessCPUUsage(renderer3.process_node.get(), 1.0);
  task_env().FastForwardBy(kTimeBetweenMeasurements / 4);
  SetProcessCPUUsage(renderer3.process_node.get(), 0);

  // Finish next measurement interval.
  task_env().FastForwardBy(kTimeBetweenMeasurements / 4);
  SetProcessCPUUsage(renderer4.process_node.get(), 0);

  {
    auto measurements = cpu_monitor_.UpdateCPUMeasurements();
    EXPECT_THAT(GetMeasurementResult(measurements, renderer1.resource_context),
                Optional(DoubleEq(0.5)));
    EXPECT_THAT(GetMeasurementResult(measurements, renderer2.resource_context),
                Optional(DoubleEq(0.75)));
    EXPECT_THAT(GetMeasurementResult(measurements, renderer3.resource_context),
                Optional(DoubleEq(0.25)));
    EXPECT_THAT(GetMeasurementResult(measurements, renderer4.resource_context),
                Optional(DoubleEq(1.0)));
    EXPECT_THAT(GetMeasurementResult(measurements, renderer5.resource_context),
                Optional(DoubleEq(1.0)));
  }

  // `renderer1` exits at the beginning of the next measurement interval.
  // `renderer2` exits halfway through.
  // `renderer3` and `renderer4` are still using 0% CPU.
  // `renderer5` exits at the end of the interval.
  SetProcessExited(renderer1.process_node.get());
  task_env().FastForwardBy(kTimeBetweenMeasurements / 2);
  SetProcessExited(renderer2.process_node.get());

  // Finish next measurement interval.
  task_env().FastForwardBy(kTimeBetweenMeasurements / 2);
  SetProcessExited(renderer5.process_node.get());

  {
    // TODO(crbug.com/1410503): Capture the final CPU usage correctly, and test
    // that the renderers that have exited return their CPU usage for the time
    // they were alive and 0% for the rest of the measurement interval.
    auto measurements = cpu_monitor_.UpdateCPUMeasurements();
    EXPECT_THAT(GetMeasurementResult(measurements, renderer1.resource_context),
                Eq(std::nullopt));
    EXPECT_THAT(GetMeasurementResult(measurements, renderer2.resource_context),
                Eq(std::nullopt));
    EXPECT_THAT(GetMeasurementResult(measurements, renderer3.resource_context),
                Optional(DoubleEq(0.0)));
    EXPECT_THAT(GetMeasurementResult(measurements, renderer4.resource_context),
                Optional(DoubleEq(0.0)));
    EXPECT_THAT(GetMeasurementResult(measurements, renderer5.resource_context),
                Eq(std::nullopt));
  }

  // `renderer3` exits just before the StopMonitoring call and `renderer4`
  // exits just after. This should not cause any assertion failures.
  SetProcessExited(renderer3.process_node.get());
  cpu_monitor_.StopMonitoring(graph());
  SetProcessExited(renderer4.process_node.get());
}

TEST_F(PageResourceCPUMonitorTest, CPUDistribution) {
  // Track CPU usage of the mock utility process to make sure that measuring it
  // doesn't crash. Currently only measurements of renderer processes are
  // stored anywhere, so there are no other expectations to verify.
  SetProcessCPUUsage(mock_utility_process_.get(), 0.7);

  SetProcessCPUUsage(mock_graph_->process.get(), 0.6);
  SetProcessCPUUsage(mock_graph_->other_process.get(), 0.5);

  cpu_monitor_.StartMonitoring(graph());

  {
    // No measurements if no time has passed.
    auto measurements = cpu_monitor_.UpdateCPUMeasurements();
    EXPECT_THAT(measurements, IsEmpty());
  }

  task_env().FastForwardBy(kTimeBetweenMeasurements);

  {
    auto measurements = cpu_monitor_.UpdateCPUMeasurements();

    // `process` splits its 60% CPU usage evenly between `frame`, `other_frame`
    // and `worker`. `other_process` splits its 50% CPU usage evenly between
    // `child_frame` and `other_worker`. See the chart in
    // MockMultiplePagesAndWorkersWithMultipleProcessesGraph.
    EXPECT_THAT(GetMeasurementResult(measurements, mock_graph_->frame),
                Optional(DoubleEq(0.2)));
    EXPECT_THAT(GetMeasurementResult(measurements, mock_graph_->other_frame),
                Optional(DoubleEq(0.2)));
    EXPECT_THAT(GetMeasurementResult(measurements, mock_graph_->worker),
                Optional(DoubleEq(0.2)));
    EXPECT_THAT(GetMeasurementResult(measurements, mock_graph_->child_frame),
                Optional(DoubleEq(0.25)));
    EXPECT_THAT(GetMeasurementResult(measurements, mock_graph_->other_worker),
                Optional(DoubleEq(0.25)));

    // `page` gets its CPU usage from the sum of `frame` and `worker`.
    // `other_page` gets the sum of `other_frame`, `child_frame` and
    // `other_worker`. See the chart in
    // MockMultiplePagesAndWorkersWithMultipleProcessesGraph.
    EXPECT_THAT(PageResourceCPUMonitor::EstimatePageCPUUsage(
                    mock_graph_->page.get(), measurements),
                DoubleEq(0.4));
    EXPECT_THAT(PageResourceCPUMonitor::EstimatePageCPUUsage(
                    mock_graph_->other_page.get(), measurements),
                DoubleEq(0.7));
  }

  // Modify the CPU usage of each process, ensure all frames and workers are
  // updated.
  SetProcessCPUUsage(mock_graph_->process.get(), 0.3);
  SetProcessCPUUsage(mock_graph_->other_process.get(), 0.8);
  task_env().FastForwardBy(kTimeBetweenMeasurements);

  {
    auto measurements = cpu_monitor_.UpdateCPUMeasurements();

    // `process` splits its 30% CPU usage evenly between `frame`, `other_frame`
    // and `worker`. `other_process` splits its 80% CPU usage evenly between
    // `child_frame` and `other_worker`.
    EXPECT_THAT(GetMeasurementResult(measurements, mock_graph_->frame),
                Optional(DoubleEq(0.1)));
    EXPECT_THAT(GetMeasurementResult(measurements, mock_graph_->other_frame),
                Optional(DoubleEq(0.1)));
    EXPECT_THAT(GetMeasurementResult(measurements, mock_graph_->worker),
                Optional(DoubleEq(0.1)));
    EXPECT_THAT(GetMeasurementResult(measurements, mock_graph_->child_frame),
                Optional(DoubleEq(0.4)));
    EXPECT_THAT(GetMeasurementResult(measurements, mock_graph_->other_worker),
                Optional(DoubleEq(0.4)));

    // `page` gets its CPU usage from the sum of `frame` and `worker`.
    // `other_page` gets the sum of `other_frame`, `child_frame` and
    // `other_worker`.
    EXPECT_THAT(PageResourceCPUMonitor::EstimatePageCPUUsage(
                    mock_graph_->page.get(), measurements),
                DoubleEq(0.2));
    EXPECT_THAT(PageResourceCPUMonitor::EstimatePageCPUUsage(
                    mock_graph_->other_page.get(), measurements),
                DoubleEq(0.9));
  }

  // Drop CPU usage of `other_process` to 0%. Only advance part of the normal
  // measurement interval, to be sure that the percentage usage doesn't depend
  // on the length of the interval.
  SetProcessCPUUsage(mock_graph_->other_process.get(), 0.0);
  task_env().FastForwardBy(kTimeBetweenMeasurements / 3);

  {
    auto measurements = cpu_monitor_.UpdateCPUMeasurements();

    // `process` splits its 30% CPU usage evenly between `frame`, `other_frame`
    // and `worker`. `other_process` splits its 0% CPU usage evenly between
    // `child_frame` and `other_worker`.
    EXPECT_THAT(GetMeasurementResult(measurements, mock_graph_->frame),
                Optional(DoubleEq(0.1)));
    EXPECT_THAT(GetMeasurementResult(measurements, mock_graph_->other_frame),
                Optional(DoubleEq(0.1)));
    EXPECT_THAT(GetMeasurementResult(measurements, mock_graph_->worker),
                Optional(DoubleEq(0.1)));
    EXPECT_THAT(GetMeasurementResult(measurements, mock_graph_->child_frame),
                Optional(DoubleEq(0.0)));
    EXPECT_THAT(GetMeasurementResult(measurements, mock_graph_->other_worker),
                Optional(DoubleEq(0.0)));

    // `page` gets its CPU usage from the sum of `frame` and `worker`.
    // `other_page` gets the sum of `other_frame`, `child_frame` and
    // `other_worker`.
    EXPECT_THAT(PageResourceCPUMonitor::EstimatePageCPUUsage(
                    mock_graph_->page.get(), measurements),
                DoubleEq(0.2));
    EXPECT_THAT(PageResourceCPUMonitor::EstimatePageCPUUsage(
                    mock_graph_->other_page.get(), measurements),
                DoubleEq(0.1));
  }

  cpu_monitor_.StopMonitoring(graph());
}

TEST_F(PageResourceCPUMonitorTest, CPUMeasurementError) {
  const SinglePageRendererNodes renderer1 = CreateSimpleCPUTrackingRenderer();
  SetProcessId(renderer1.process_node.get());
  const SinglePageRendererNodes renderer2 = CreateSimpleCPUTrackingRenderer();
  SetProcessId(renderer2.process_node.get());

  cpu_monitor_.StartMonitoring(graph());

  task_env().FastForwardBy(kTimeBetweenMeasurements);

  {
    auto measurements = cpu_monitor_.UpdateCPUMeasurements();
    EXPECT_THAT(GetMeasurementResult(measurements, renderer1.resource_context),
                Optional(DoubleEq(1.0)));
    EXPECT_THAT(GetMeasurementResult(measurements, renderer2.resource_context),
                Optional(DoubleEq(1.0)));
  }

  SetProcessCPUUsage(renderer1.process_node.get(), 0.5);
  SetProcessCPUUsage(renderer2.process_node.get(), 0.5);
  SetProcessCPUUsageError(renderer1.process_node.get(),
                          ProcessCPUUsageError::kSystemError);

  task_env().FastForwardBy(kTimeBetweenMeasurements);

  {
    auto measurements = cpu_monitor_.UpdateCPUMeasurements();
    EXPECT_THAT(GetMeasurementResult(measurements, renderer1.resource_context),
                Eq(std::nullopt));
    EXPECT_THAT(GetMeasurementResult(measurements, renderer2.resource_context),
                Optional(DoubleEq(0.5)));
  }

  cpu_monitor_.StopMonitoring(graph());
}

class PageResourceCPUMonitorTimingTest
    : public ChromeRenderViewHostTestHarness {
 protected:
  using Super = ChromeRenderViewHostTestHarness;

  void SetUp() override {
    Super::SetUp();
    pm_helper_.SetUp();
    RunInGraph([&](Graph* graph) {
      cpu_monitor_ = std::make_unique<PageResourceCPUMonitor>();
      cpu_monitor_->StartMonitoring(graph);
    });
  }

  void TearDown() override {
    RunInGraph([&](Graph* graph) {
      cpu_monitor_->StopMonitoring(graph);
      cpu_monitor_.reset();
    });
    pm_helper_.TearDown();
    Super::TearDown();
  }

  // Ensure some time passes to measure.
  static void LetTimePass() {
    base::TestWaitableEvent().TimedWait(TestTimeouts::tiny_timeout());
  }

  PerformanceManagerTestHarnessHelper pm_helper_;
  std::unique_ptr<PageResourceCPUMonitor> cpu_monitor_;
};

TEST_F(PageResourceCPUMonitorTimingTest, ProcessLifetime) {
  SetContents(CreateTestWebContents());
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://www.example.com/"));

  base::WeakPtr<FrameNode> frame_node =
      PerformanceManager::GetFrameNodeForRenderFrameHost(main_rfh());
  base::WeakPtr<ProcessNode> process_node =
      PerformanceManager::GetProcessNodeForRenderProcessHost(process());
  const auto frame_context =
      resource_attribution::FrameContext::FromRenderFrameHost(main_rfh())
          .value();

  // Since process() returns a MockRenderProcessHost, ProcessNode is created
  // but has no pid. (Equivalent to the time between OnProcessNodeAdded and
  // OnProcessLifetimeChange.)
  LetTimePass();
  RunInGraph([&] {
    ASSERT_TRUE(process_node);
    EXPECT_EQ(process_node->GetProcessId(), base::kNullProcessId);

    // Process can't be measured yet.
    auto measurements = cpu_monitor_->UpdateCPUMeasurements();
    EXPECT_THAT(GetMeasurementResult(measurements, frame_context),
                Eq(std::nullopt));
  });

  // Assign a real process to the ProcessNode. (Will call
  // OnProcessLifetimeChange.)
  LetTimePass();
  RunInGraph([&] {
    ASSERT_TRUE(process_node);
    ProcessNodeImpl::FromNode(process_node.get())
        ->SetProcess(base::Process::Current(), base::TimeTicks::Now());
    EXPECT_NE(process_node->GetProcessId(), base::kNullProcessId);

    // Process can be measured now.
    auto measurements = cpu_monitor_->UpdateCPUMeasurements();
    EXPECT_THAT(GetMeasurementResult(measurements, frame_context), Optional(_));
  });

  // Simulate that the process died.
  LetTimePass();
  process()->SimulateRenderProcessExit(
      base::TERMINATION_STATUS_NORMAL_TERMINATION, 0);
  RunInGraph([&] {
    // Process is no longer running, so can't be measured.
    // TODO(crbug.com/1410503): Capture the final CPU usage correctly.
    ASSERT_TRUE(process_node);
    EXPECT_FALSE(process_node->GetProcess().IsValid());
    // Depending on the order that observers fire, `frame_node` may or may not
    // have been deleted already. Either way, GetMeasurementResult will return
    // nullopt.
    auto measurements = cpu_monitor_->UpdateCPUMeasurements();
    EXPECT_THAT(GetMeasurementResult(measurements, frame_context),
                Eq(std::nullopt));
  });
}

}  // namespace performance_manager::metrics
