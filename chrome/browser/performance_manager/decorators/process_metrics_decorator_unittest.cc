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

using ::testing::_;
using ::testing::ByMove;
using ::testing::Return;

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
  }

  TestProcessMetricsDecorator* decorator() const { return decorator_raw_; }

  MockMultiplePagesAndWorkersWithMultipleProcessesGraph* mock_graph() {
    return mock_graph_.get();
  }

  void ExpectProcessResults(uint64_t resident_set_kb,
                            uint64_t private_footprint_kb) {
    EXPECT_EQ(resident_set_kb, mock_graph()->process->resident_set_kb());
    EXPECT_EQ(private_footprint_kb,
              mock_graph()->process->private_footprint_kb());

    EXPECT_EQ(resident_set_kb / 3,
              mock_graph()->frame->resident_set_kb_estimate());
    EXPECT_EQ(private_footprint_kb / 3,
              mock_graph()->frame->private_footprint_kb_estimate());
    EXPECT_EQ(resident_set_kb / 3,
              mock_graph()->other_frame->resident_set_kb_estimate());
    EXPECT_EQ(private_footprint_kb / 3,
              mock_graph()->other_frame->private_footprint_kb_estimate());
    EXPECT_EQ(resident_set_kb / 3,
              mock_graph()->worker->resident_set_kb_estimate());
    EXPECT_EQ(private_footprint_kb / 3,
              mock_graph()->worker->private_footprint_kb_estimate());
  }

  void ExpectOtherProcessResults(uint64_t resident_set_kb,
                                 uint64_t private_footprint_kb) {
    EXPECT_EQ(resident_set_kb, mock_graph()->other_process->resident_set_kb());
    EXPECT_EQ(private_footprint_kb,
              mock_graph()->other_process->private_footprint_kb());

    EXPECT_EQ(resident_set_kb / 2,
              mock_graph()->child_frame->resident_set_kb_estimate());
    EXPECT_EQ(private_footprint_kb / 2,
              mock_graph()->child_frame->private_footprint_kb_estimate());
    EXPECT_EQ(resident_set_kb / 2,
              mock_graph()->other_worker->resident_set_kb_estimate());
    EXPECT_EQ(private_footprint_kb / 2,
              mock_graph()->other_worker->private_footprint_kb_estimate());
  }

  void ResetResults() {
    mock_graph()->process->set_resident_set_kb(0);
    mock_graph()->process->set_private_footprint_kb(0);
    mock_graph()->frame->SetResidentSetKbEstimate(0);
    mock_graph()->frame->SetPrivateFootprintKbEstimate(0);
    mock_graph()->other_frame->SetResidentSetKbEstimate(0);
    mock_graph()->other_frame->SetPrivateFootprintKbEstimate(0);
    mock_graph()->worker->SetResidentSetKbEstimate(0);
    mock_graph()->worker->SetPrivateFootprintKbEstimate(0);
    mock_graph()->other_process->set_resident_set_kb(0);
    mock_graph()->other_process->set_private_footprint_kb(0);
    mock_graph()->child_frame->SetResidentSetKbEstimate(0);
    mock_graph()->child_frame->SetPrivateFootprintKbEstimate(0);
    mock_graph()->other_worker->SetResidentSetKbEstimate(0);
    mock_graph()->other_worker->SetPrivateFootprintKbEstimate(0);
  }

 private:
  raw_ptr<TestProcessMetricsDecorator> decorator_raw_;

  std::unique_ptr<MockMultiplePagesAndWorkersWithMultipleProcessesGraph>
      mock_graph_;
};

TEST_F(ProcessMetricsDecoratorTest, RefreshTimer) {
  MockSystemNodeObserver sys_node_observer;

  graph()->AddSystemNodeObserver(&sys_node_observer);

  // There's no data available initially.
  ExpectProcessResults(0, 0);
  ExpectOtherProcessResults(0, 0);

  // The first measurement should be taken immediately.
  EXPECT_CALL(*decorator(), GetMemoryDump())
      .WillOnce(Return(ByMove(GenerateMemoryDump(
          {{mock_graph()->process->process_id(), kFakeResidentSetKb,
            kFakePrivateFootprintKb},
           {mock_graph()->other_process->process_id(), kFakeResidentSetKb,
            kFakePrivateFootprintKb}}))));
  EXPECT_CALL(sys_node_observer, OnProcessMemoryMetricsAvailable(_));

  auto interest_token =
      ProcessMetricsDecorator::RegisterInterestForProcessMetrics(graph());

  ExpectProcessResults(kFakeResidentSetKb, kFakePrivateFootprintKb);
  ExpectOtherProcessResults(kFakeResidentSetKb, kFakePrivateFootprintKb);
  ResetResults();

  // Advance the timer, this should trigger a refresh of the metrics.
  EXPECT_CALL(*decorator(), GetMemoryDump())
      .WillOnce(Return(ByMove(GenerateMemoryDump(
          {{mock_graph()->process->process_id(), kFakeResidentSetKb,
            kFakePrivateFootprintKb},
           {mock_graph()->other_process->process_id(), kFakeResidentSetKb,
            kFakePrivateFootprintKb}}))));
  EXPECT_CALL(sys_node_observer, OnProcessMemoryMetricsAvailable(_));

  task_env().FastForwardBy(decorator()->GetTimerDelayForTesting());

  ExpectProcessResults(kFakeResidentSetKb, kFakePrivateFootprintKb);
  ExpectOtherProcessResults(kFakeResidentSetKb, kFakePrivateFootprintKb);
  ResetResults();

  // Refreshes should stop when there are no tokens left.
  interest_token.reset();
  task_env().FastForwardBy(decorator()->GetTimerDelayForTesting());
  ExpectProcessResults(0, 0);
  ExpectOtherProcessResults(0, 0);

  graph()->RemoveSystemNodeObserver(&sys_node_observer);
}

TEST_F(ProcessMetricsDecoratorTest, PartialRefresh) {
  // Only contains the data for one of the two processes.
  EXPECT_CALL(*decorator(), GetMemoryDump())
      .WillOnce(Return(ByMove(GenerateMemoryDump(
          {{mock_graph()->process->process_id(), kFakeResidentSetKb,
            kFakePrivateFootprintKb}}))));

  auto interest_token =
      ProcessMetricsDecorator::RegisterInterestForProcessMetrics(graph());

  ExpectProcessResults(kFakeResidentSetKb, kFakePrivateFootprintKb);
  ExpectOtherProcessResults(0, 0);

  // Do another partial refresh but this time for the other process. The
  // data attached to |mock_graph()->process| shouldn't change.
  EXPECT_CALL(*decorator(), GetMemoryDump())
      .WillOnce(Return(ByMove(GenerateMemoryDump(
          {{mock_graph()->other_process->process_id(), kFakeResidentSetKb * 2,
            kFakePrivateFootprintKb * 2}}))));

  task_env().FastForwardBy(decorator()->GetTimerDelayForTesting());

  ExpectProcessResults(kFakeResidentSetKb, kFakePrivateFootprintKb);
  ExpectOtherProcessResults(kFakeResidentSetKb * 2,
                            kFakePrivateFootprintKb * 2);
}

TEST_F(ProcessMetricsDecoratorTest, RefreshFailure) {
  EXPECT_CALL(*decorator(), GetMemoryDump())
      .WillOnce(Return(ByMove(absl::nullopt)));

  auto interest_token =
      ProcessMetricsDecorator::RegisterInterestForProcessMetrics(graph());

  ExpectProcessResults(0, 0);
  ExpectOtherProcessResults(0, 0);

  // A failure shouldn't stop the next refresh.
  EXPECT_CALL(*decorator(), GetMemoryDump())
      .WillOnce(Return(ByMove(GenerateMemoryDump(
          {{mock_graph()->process->process_id(), kFakeResidentSetKb,
            kFakePrivateFootprintKb},
           {mock_graph()->other_process->process_id(), kFakeResidentSetKb,
            kFakePrivateFootprintKb}}))));

  task_env().FastForwardBy(decorator()->GetTimerDelayForTesting());

  ExpectProcessResults(kFakeResidentSetKb, kFakePrivateFootprintKb);
  ExpectOtherProcessResults(kFakeResidentSetKb, kFakePrivateFootprintKb);
}

TEST_F(ProcessMetricsDecoratorTest, MetricsInterestTokens) {
  EXPECT_FALSE(decorator()->IsTimerRunningForTesting());

  // The first token created will take a measurement, then start the timer.
  EXPECT_CALL(*decorator(), GetMemoryDump())
      .WillOnce(Return(ByMove(absl::nullopt)));
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

  // Creating another token after all are deleted should take another
  // measurement.
  EXPECT_CALL(*decorator(), GetMemoryDump())
      .WillOnce(Return(ByMove(absl::nullopt)));
  auto metrics_interest_token3 =
      ProcessMetricsDecorator::RegisterInterestForProcessMetrics(graph());
}

}  // namespace performance_manager
