// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/decorators/process_metrics_decorator.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "components/performance_manager/test_support/mock_graphs.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/global_memory_dump.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/memory_instrumentation.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace performance_manager {

namespace {

constexpr uint32_t kFakeResidentSetKb = 12345;
constexpr uint32_t kFakePrivateFootprintKb = 67890;

// Test version of the |ProcessMetricsDecorator| class.
class LenientTestProcessMetricsDecorator : public ProcessMetricsDecorator {
 public:
  LenientTestProcessMetricsDecorator() = default;
  ~LenientTestProcessMetricsDecorator() override = default;

  // Expose RefreshMetrics for unittesting.
  using ProcessMetricsDecorator::RefreshMetrics;

  // ProcessMetricsDecorator:
  void RequestProcessesMemoryMetrics(
      memory_instrumentation::MemoryInstrumentation::RequestGlobalDumpCallback
          callback) override;

  // Mock method used to set the test expectations.
  MOCK_METHOD0(
      GetMemoryDump,
      absl::optional<memory_instrumentation::mojom::GlobalMemoryDumpPtr>());
};
using TestProcessMetricsDecorator =
    ::testing::StrictMock<LenientTestProcessMetricsDecorator>;

void LenientTestProcessMetricsDecorator::RequestProcessesMemoryMetrics(
    memory_instrumentation::MemoryInstrumentation::RequestGlobalDumpCallback
        callback) {
  absl::optional<memory_instrumentation::mojom::GlobalMemoryDumpPtr>
      global_dump = GetMemoryDump();

  std::move(callback).Run(
      global_dump.has_value(),
      global_dump.has_value()
          ? memory_instrumentation::GlobalMemoryDump::MoveFrom(
                std::move(global_dump.value()))
          : memory_instrumentation::GlobalMemoryDump::MoveFrom(
                memory_instrumentation::mojom::GlobalMemoryDump::New()));
}

class LenientMockSystemNodeObserver
    : public SystemNodeImpl::ObserverDefaultImpl {
 public:
  LenientMockSystemNodeObserver() {}
  ~LenientMockSystemNodeObserver() override {}

  MOCK_METHOD1(OnProcessMemoryMetricsAvailable, void(const SystemNode*));
};
using MockSystemNodeObserver =
    ::testing::StrictMock<LenientMockSystemNodeObserver>;

struct MemoryDumpProcInfo {
  base::ProcessId pid;
  uint32_t resident_set_kb;
  uint32_t private_footprint_kb;
};

// Generate a GlobalMemoryDumpPtr object based on the data contained in
// |proc_info_vec|.
memory_instrumentation::mojom::GlobalMemoryDumpPtr GenerateMemoryDump(
    const std::vector<MemoryDumpProcInfo>& proc_info_vec) {
  memory_instrumentation::mojom::GlobalMemoryDumpPtr global_dump(
      memory_instrumentation::mojom::GlobalMemoryDump::New());

  for (const auto& proc_info : proc_info_vec) {
    memory_instrumentation::mojom::ProcessMemoryDumpPtr pmd =
        memory_instrumentation::mojom::ProcessMemoryDump::New();
    pmd->pid = proc_info.pid;
    memory_instrumentation::mojom::OSMemDumpPtr os_dump =
        memory_instrumentation::mojom::OSMemDump::New();
    os_dump->resident_set_kb = proc_info.resident_set_kb;
    os_dump->private_footprint_kb = proc_info.private_footprint_kb;
    pmd->os_dump = std::move(os_dump);
    global_dump->process_dumps.emplace_back(std::move(pmd));
  }

  return global_dump;
}

}  // namespace

class ProcessMetricsDecoratorTest : public GraphTestHarness {
 public:
  ProcessMetricsDecoratorTest(const ProcessMetricsDecoratorTest&) = delete;
  ProcessMetricsDecoratorTest& operator=(const ProcessMetricsDecoratorTest&) =
      delete;

 protected:
  using Super = GraphTestHarness;

  ProcessMetricsDecoratorTest() = default;
  ~ProcessMetricsDecoratorTest() override = default;

  void SetUp() override {
    Super::SetUp();
    std::unique_ptr<TestProcessMetricsDecorator> decorator =
        std::make_unique<TestProcessMetricsDecorator>();
    decorator_raw_ = decorator.get();
    mock_graph_ =
        std::make_unique<MockMultiplePagesAndWorkersWithMultipleProcessesGraph>(
            graph());
    EXPECT_FALSE(decorator_raw_->IsTimerRunningForTesting());
    graph()->PassToGraph(std::move(decorator));
    EXPECT_FALSE(decorator_raw_->IsTimerRunningForTesting());
    metrics_interest_token_ =
        ProcessMetricsDecorator::RegisterInterestForProcessMetrics(graph());
    EXPECT_TRUE(decorator_raw_->IsTimerRunningForTesting());
  }

  TestProcessMetricsDecorator* decorator() const { return decorator_raw_; }

  MockMultiplePagesAndWorkersWithMultipleProcessesGraph* mock_graph() {
    return mock_graph_.get();
  }

  void ReleaseMetricsInterestToken() { metrics_interest_token_.reset(); }

 private:
  raw_ptr<TestProcessMetricsDecorator> decorator_raw_;

  std::unique_ptr<MockMultiplePagesAndWorkersWithMultipleProcessesGraph>
      mock_graph_;

  std::unique_ptr<ProcessMetricsDecorator::ScopedMetricsInterestToken>
      metrics_interest_token_;
};

TEST_F(ProcessMetricsDecoratorTest, RefreshTimer) {
  MockSystemNodeObserver sys_node_observer;

  graph()->AddSystemNodeObserver(&sys_node_observer);
  auto memory_dump = absl::make_optional(
      GenerateMemoryDump({{mock_graph()->process->process_id(),
                           kFakeResidentSetKb, kFakePrivateFootprintKb},
                          {mock_graph()->other_process->process_id(),
                           kFakeResidentSetKb, kFakePrivateFootprintKb}}));

  EXPECT_CALL(*decorator(), GetMemoryDump())
      .WillOnce(testing::Return(testing::ByMove(std::move(memory_dump))));

  // There's no data available initially.
  EXPECT_EQ(0U, mock_graph()->process->resident_set_kb());
  EXPECT_EQ(0U, mock_graph()->process->private_footprint_kb());

  EXPECT_CALL(sys_node_observer, OnProcessMemoryMetricsAvailable(testing::_));

  // Advance the timer, this should trigger a refresh of the metrics.
  task_env().FastForwardBy(decorator()->GetTimerDelayForTesting());

  EXPECT_EQ(kFakeResidentSetKb, mock_graph()->process->resident_set_kb());
  EXPECT_EQ(kFakePrivateFootprintKb,
            mock_graph()->process->private_footprint_kb());

  EXPECT_EQ(kFakeResidentSetKb / 3,
            mock_graph()->frame->resident_set_kb_estimate());
  EXPECT_EQ(kFakeResidentSetKb / 3,
            mock_graph()->other_frame->resident_set_kb_estimate());
  EXPECT_EQ(kFakeResidentSetKb / 3,
            mock_graph()->worker->resident_set_kb_estimate());

  EXPECT_EQ(kFakeResidentSetKb, mock_graph()->other_process->resident_set_kb());
  EXPECT_EQ(kFakePrivateFootprintKb,
            mock_graph()->other_process->private_footprint_kb());

  EXPECT_EQ(kFakeResidentSetKb / 2,
            mock_graph()->child_frame->resident_set_kb_estimate());
  EXPECT_EQ(kFakeResidentSetKb / 2,
            mock_graph()->other_worker->resident_set_kb_estimate());

  graph()->RemoveSystemNodeObserver(&sys_node_observer);
}

TEST_F(ProcessMetricsDecoratorTest, PartialRefresh) {
  // Only contains the data for one of the two processes.
  auto partial_memory_dump = absl::make_optional(
      GenerateMemoryDump({{mock_graph()->process->process_id(),
                           kFakeResidentSetKb, kFakePrivateFootprintKb}}));

  EXPECT_CALL(*decorator(), GetMemoryDump())
      .WillOnce(
          testing::Return(testing::ByMove(std::move(partial_memory_dump))));

  task_env().FastForwardBy(decorator()->GetTimerDelayForTesting());

  EXPECT_EQ(kFakeResidentSetKb, mock_graph()->process->resident_set_kb());
  EXPECT_EQ(kFakePrivateFootprintKb,
            mock_graph()->process->private_footprint_kb());

  // Do another partial refresh but this time for the other process. The data
  // attached to |mock_graph()->process| shouldn't change.
  auto partial_memory_dump2 = absl::make_optional(GenerateMemoryDump(
      {{mock_graph()->other_process->process_id(), kFakeResidentSetKb * 2,
        kFakePrivateFootprintKb * 2}}));
  EXPECT_CALL(*decorator(), GetMemoryDump())
      .WillOnce(
          testing::Return(testing::ByMove(std::move(partial_memory_dump2))));

  task_env().FastForwardBy(decorator()->GetTimerDelayForTesting());

  EXPECT_EQ(kFakeResidentSetKb, mock_graph()->process->resident_set_kb());
  EXPECT_EQ(kFakePrivateFootprintKb,
            mock_graph()->process->private_footprint_kb());

  EXPECT_EQ(kFakeResidentSetKb / 3,
            mock_graph()->frame->resident_set_kb_estimate());
  EXPECT_EQ(kFakeResidentSetKb / 3,
            mock_graph()->other_frame->resident_set_kb_estimate());
  EXPECT_EQ(kFakeResidentSetKb / 3,
            mock_graph()->worker->resident_set_kb_estimate());

  EXPECT_EQ(kFakeResidentSetKb * 2,
            mock_graph()->other_process->resident_set_kb());
  EXPECT_EQ(kFakePrivateFootprintKb * 2,
            mock_graph()->other_process->private_footprint_kb());

  EXPECT_EQ(kFakeResidentSetKb,
            mock_graph()->child_frame->resident_set_kb_estimate());
  EXPECT_EQ(kFakeResidentSetKb,
            mock_graph()->other_worker->resident_set_kb_estimate());
}

TEST_F(ProcessMetricsDecoratorTest, RefreshFailure) {
  EXPECT_CALL(*decorator(), GetMemoryDump())
      .WillOnce(testing::Return(testing::ByMove(absl::nullopt)));

  task_env().FastForwardBy(decorator()->GetTimerDelayForTesting());

  EXPECT_EQ(0U, mock_graph()->process->resident_set_kb());
  EXPECT_EQ(0U, mock_graph()->process->private_footprint_kb());
  EXPECT_EQ(0U, mock_graph()->frame->resident_set_kb_estimate());
  EXPECT_EQ(0U, mock_graph()->child_frame->resident_set_kb_estimate());
}

TEST_F(ProcessMetricsDecoratorTest, MetricsInterestTokens) {
  ReleaseMetricsInterestToken();
  EXPECT_FALSE(decorator()->IsTimerRunningForTesting());
  auto metrics_interest_token1 =
      ProcessMetricsDecorator::RegisterInterestForProcessMetrics(graph());
  EXPECT_TRUE(decorator()->IsTimerRunningForTesting());

  auto metrics_interest_token2 =
      ProcessMetricsDecorator::RegisterInterestForProcessMetrics(graph());
  EXPECT_TRUE(decorator()->IsTimerRunningForTesting());

  metrics_interest_token1.reset();
  EXPECT_TRUE(decorator()->IsTimerRunningForTesting());
  metrics_interest_token2.reset();
  EXPECT_FALSE(decorator()->IsTimerRunningForTesting());
}

}  // namespace performance_manager
