// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/report_page_processes_policy.h"

#include <algorithm>

#include "chrome/browser/performance_manager/test_support/page_discarding_utils.h"
#include "components/performance_manager/test_support/mock_graphs.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager::policies {

class MockReportPageProcessesPolicy : public ReportPageProcessesPolicy {
 public:
  void HandlePageNodeEvents() {
    ReportPageProcessesPolicy::HandlePageNodeEvents();
  }

  std::vector<PageProcess> GetPageProcesses() { return processes_; }

 protected:
  void ReportPageProcesses(std::vector<PageProcess> processes) override {
    processes_ = processes;
  }

 private:
  std::vector<PageProcess> processes_;
};

class ReportPageProcessesPolicyTest
    : public testing::GraphTestHarnessWithMockDiscarder {
 public:
  ReportPageProcessesPolicyTest() = default;
  ~ReportPageProcessesPolicyTest() override = default;
  ReportPageProcessesPolicyTest(const ReportPageProcessesPolicyTest& other) =
      delete;
  ReportPageProcessesPolicyTest& operator=(
      const ReportPageProcessesPolicyTest&) = delete;

  void SetUp() override {
    testing::GraphTestHarnessWithMockDiscarder::SetUp();

    // Create the policy and pass it to the graph.
    auto policy = std::make_unique<MockReportPageProcessesPolicy>();
    policy_ = policy.get();
    graph()->PassToGraph(std::move(policy));
  }

  void TearDown() override {
    graph()->TakeFromGraph(policy_);
    testing::GraphTestHarnessWithMockDiscarder::TearDown();
  }

  MockReportPageProcessesPolicy* policy() { return policy_; }

 private:
  raw_ptr<MockReportPageProcessesPolicy, DanglingUntriaged> policy_;
};

TEST_F(ReportPageProcessesPolicyTest, ReportPageProcesses) {
  constexpr base::ProcessId kProcessId1 = 1;
  constexpr base::ProcessId kProcessId2 = 2;
  constexpr base::ProcessId kProcessId3 = 3;
  constexpr base::ProcessId kProcessId4 = 4;
  constexpr base::ProcessId kProcessId5 = 5;
  constexpr base::ProcessId kProcessId6 = 6;

  // Creates 6 pages.
  auto process_node1 = TestNodeWrapper<TestProcessNodeImpl>::Create(graph());
  process_node1->SetProcessWithPid(kProcessId1, base::Process::Current(),
                                   /* launch_time=*/base::TimeTicks::Now());
  auto page_node1 = CreateNode<performance_manager::PageNodeImpl>();
  auto main_frame_node1 =
      CreateFrameNodeAutoId(process_node1.get(), page_node1.get());
  main_frame_node1->SetIsCurrent(true);
  testing::MakePageNodeDiscardable(page_node1.get(), task_env());
  AdvanceClock(base::Minutes(30));
  main_frame_node1->SetIsCurrent(false);
  AdvanceClock(base::Minutes(30));
  // Set page node 1 audible to raise its priority.
  page_node1->SetIsAudible(true);

  auto process_node2 = TestNodeWrapper<TestProcessNodeImpl>::Create(graph());
  process_node2->SetProcessWithPid(kProcessId2, base::Process::Current(),
                                   /* launch_time=*/base::TimeTicks::Now());
  auto page_node2 = CreateNode<performance_manager::PageNodeImpl>();
  auto main_frame_node2 =
      CreateFrameNodeAutoId(process_node2.get(), page_node2.get());
  main_frame_node2->SetIsCurrent(true);
  testing::MakePageNodeDiscardable(page_node2.get(), task_env());
  AdvanceClock(base::Minutes(30));
  main_frame_node2->SetIsCurrent(false);
  AdvanceClock(base::Minutes(30));

  auto process_node3 = TestNodeWrapper<TestProcessNodeImpl>::Create(graph());
  process_node3->SetProcessWithPid(kProcessId3, base::Process::Current(),
                                   /* launch_time=*/base::TimeTicks::Now());
  auto page_node3 = CreateNode<performance_manager::PageNodeImpl>();
  auto main_frame_node3 =
      CreateFrameNodeAutoId(process_node3.get(), page_node3.get());
  main_frame_node3->SetIsCurrent(true);
  testing::MakePageNodeDiscardable(page_node3.get(), task_env());
  AdvanceClock(base::Minutes(30));
  main_frame_node3->SetIsCurrent(false);
  AdvanceClock(base::Minutes(30));

  auto process_node4 = TestNodeWrapper<TestProcessNodeImpl>::Create(graph());
  process_node4->SetProcessWithPid(kProcessId4, base::Process::Current(),
                                   /* launch_time=*/base::TimeTicks::Now());
  auto page_node4 = CreateNode<performance_manager::PageNodeImpl>();
  auto main_frame_node4 =
      CreateFrameNodeAutoId(process_node4.get(), page_node4.get());
  main_frame_node4->SetIsCurrent(true);
  testing::MakePageNodeDiscardable(page_node4.get(), task_env());
  AdvanceClock(base::Minutes(30));
  main_frame_node4->SetIsCurrent(false);
  AdvanceClock(base::Minutes(30));

  auto process_node5 = TestNodeWrapper<TestProcessNodeImpl>::Create(graph());
  process_node5->SetProcessWithPid(kProcessId5, base::Process::Current(),
                                   /* launch_time=*/base::TimeTicks::Now());
  auto page_node5 = CreateNode<performance_manager::PageNodeImpl>();
  auto main_frame_node5 =
      CreateFrameNodeAutoId(process_node5.get(), page_node5.get());
  main_frame_node5->SetIsCurrent(true);
  testing::MakePageNodeDiscardable(page_node5.get(), task_env());
  AdvanceClock(base::Minutes(30));
  page_node5->SetIsVisible(true);
  AdvanceClock(base::Minutes(30));

  auto process_node6 = TestNodeWrapper<TestProcessNodeImpl>::Create(graph());
  process_node6->SetProcessWithPid(kProcessId6, base::Process::Current(),
                                   /* launch_time=*/base::TimeTicks::Now());
  auto page_node6 = CreateNode<performance_manager::PageNodeImpl>();
  auto main_frame_node6 =
      CreateFrameNodeAutoId(process_node6.get(), page_node6.get());
  main_frame_node6->SetIsCurrent(true);
  testing::MakePageNodeDiscardable(page_node6.get(), task_env());
  AdvanceClock(base::Minutes(30));
  main_frame_node6->SetIsCurrent(false);
  AdvanceClock(base::Minutes(30));
  // Set page node 6 focused to raise its priority.
  page_node6->SetIsVisible(true);
  page_node6->SetIsFocused(true);

  // Trigger page node event manually.
  policy()->HandlePageNodeEvents();

  // Processes with descending importance.
  auto processes = policy()->GetPageProcesses();
  ASSERT_EQ(processes.size(), 6u);

  ASSERT_EQ(processes[0].pid, kProcessId6);
  ASSERT_EQ(processes[0].host_protected_page, true);
  ASSERT_EQ(processes[0].host_visible_page, true);
  ASSERT_EQ(processes[0].host_focused_page, true);

  ASSERT_EQ(processes[1].pid, kProcessId5);
  ASSERT_EQ(processes[1].host_protected_page, true);
  ASSERT_EQ(processes[1].host_visible_page, true);

  // Because page node 1 is audible, it's protected.
  ASSERT_EQ(processes[2].pid, kProcessId1);
  ASSERT_EQ(processes[2].host_protected_page, true);
  ASSERT_EQ(processes[2].host_visible_page, false);
  ASSERT_EQ(processes[3].pid, kProcessId4);
  ASSERT_EQ(processes[3].host_protected_page, false);
  ASSERT_EQ(processes[3].host_visible_page, false);
  ASSERT_EQ(processes[4].pid, kProcessId3);
  ASSERT_EQ(processes[4].host_protected_page, false);
  ASSERT_EQ(processes[4].host_visible_page, false);
  ASSERT_EQ(processes[5].pid, kProcessId2);
  ASSERT_EQ(processes[5].host_protected_page, false);
  ASSERT_EQ(processes[5].host_visible_page, false);
}

}  // namespace performance_manager::policies
