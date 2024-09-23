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

  base::flat_map<base::ProcessId, PageState> GetReportedPages() {
    return processes_;
  }

  int report_page_processes_count_ = 0;

 protected:
  void ReportPageProcesses(
      base::flat_map<base::ProcessId, PageState> processes) override {
    processes_ = processes;
    report_page_processes_count_++;
  }

 private:
  base::flat_map<base::ProcessId, PageState> processes_;
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
  testing::MakePageNodeDiscardable(page_node1.get(), task_env());
  AdvanceClock(base::Minutes(30));
  FrameNodeImpl::UpdateCurrentFrame(
      /*previous_frame_node=*/main_frame_node1.get(),
      /*current_frame_node=*/nullptr, graph());
  AdvanceClock(base::Minutes(30));
  // Set page node 1 audible to raise its priority.
  page_node1->SetIsAudible(true);

  auto process_node2 = TestNodeWrapper<TestProcessNodeImpl>::Create(graph());
  process_node2->SetProcessWithPid(kProcessId2, base::Process::Current(),
                                   /* launch_time=*/base::TimeTicks::Now());
  auto page_node2 = CreateNode<performance_manager::PageNodeImpl>();
  auto main_frame_node2 =
      CreateFrameNodeAutoId(process_node2.get(), page_node2.get());
  testing::MakePageNodeDiscardable(page_node2.get(), task_env());
  AdvanceClock(base::Minutes(30));
  FrameNodeImpl::UpdateCurrentFrame(
      /*previous_frame_node=*/main_frame_node2.get(),
      /*current_frame_node=*/nullptr, graph());
  AdvanceClock(base::Minutes(30));

  auto process_node3 = TestNodeWrapper<TestProcessNodeImpl>::Create(graph());
  process_node3->SetProcessWithPid(kProcessId3, base::Process::Current(),
                                   /* launch_time=*/base::TimeTicks::Now());
  auto page_node3 = CreateNode<performance_manager::PageNodeImpl>();
  auto main_frame_node3 =
      CreateFrameNodeAutoId(process_node3.get(), page_node3.get());
  testing::MakePageNodeDiscardable(page_node3.get(), task_env());
  AdvanceClock(base::Minutes(30));
  FrameNodeImpl::UpdateCurrentFrame(
      /*previous_frame_node=*/main_frame_node3.get(),
      /*current_frame_node=*/nullptr, graph());
  AdvanceClock(base::Minutes(30));

  auto process_node4 = TestNodeWrapper<TestProcessNodeImpl>::Create(graph());
  process_node4->SetProcessWithPid(kProcessId4, base::Process::Current(),
                                   /* launch_time=*/base::TimeTicks::Now());
  auto page_node4 = CreateNode<performance_manager::PageNodeImpl>();
  auto main_frame_node4 =
      CreateFrameNodeAutoId(process_node4.get(), page_node4.get());
  testing::MakePageNodeDiscardable(page_node4.get(), task_env());
  AdvanceClock(base::Minutes(30));
  FrameNodeImpl::UpdateCurrentFrame(
      /*previous_frame_node=*/main_frame_node4.get(),
      /*current_frame_node=*/nullptr, graph());
  AdvanceClock(base::Minutes(30));

  auto process_node5 = TestNodeWrapper<TestProcessNodeImpl>::Create(graph());
  process_node5->SetProcessWithPid(kProcessId5, base::Process::Current(),
                                   /* launch_time=*/base::TimeTicks::Now());
  auto page_node5 = CreateNode<performance_manager::PageNodeImpl>();
  auto main_frame_node5 =
      CreateFrameNodeAutoId(process_node5.get(), page_node5.get());
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
  testing::MakePageNodeDiscardable(page_node6.get(), task_env());
  AdvanceClock(base::Minutes(30));
  FrameNodeImpl::UpdateCurrentFrame(
      /*previous_frame_node=*/main_frame_node6.get(),
      /*current_frame_node=*/nullptr, graph());
  AdvanceClock(base::Minutes(30));
  // Set page node 6 focused to raise its priority.
  page_node6->SetIsVisible(true);
  page_node6->SetIsFocused(true);

  // Trigger page node event manually.
  policy()->HandlePageNodeEvents();

  // Processes with descending importance.
  auto processes = policy()->GetReportedPages();
  ASSERT_EQ(processes.size(), 6u);

  ASSERT_TRUE(processes.contains(kProcessId6));
  ASSERT_EQ(processes[kProcessId6].host_protected_page, true);
  ASSERT_EQ(processes[kProcessId6].host_visible_page, true);
  ASSERT_EQ(processes[kProcessId6].host_focused_page, true);

  ASSERT_TRUE(processes.contains(kProcessId5));
  ASSERT_EQ(processes[kProcessId5].host_protected_page, true);
  ASSERT_EQ(processes[kProcessId5].host_visible_page, true);

  // Because page node 1 is audible, it's protected.
  ASSERT_TRUE(processes.contains(kProcessId1));
  ASSERT_EQ(processes[kProcessId1].host_protected_page, true);
  ASSERT_EQ(processes[kProcessId1].host_visible_page, false);
  ASSERT_TRUE(processes.contains(kProcessId4));
  ASSERT_EQ(processes[kProcessId4].host_protected_page, false);
  ASSERT_EQ(processes[kProcessId4].host_visible_page, false);
  ASSERT_TRUE(processes.contains(kProcessId3));
  ASSERT_EQ(processes[kProcessId3].host_protected_page, false);
  ASSERT_EQ(processes[kProcessId3].host_visible_page, false);
  ASSERT_TRUE(processes.contains(kProcessId2));
  ASSERT_EQ(processes[kProcessId2].host_protected_page, false);
  ASSERT_EQ(processes[kProcessId2].host_visible_page, false);
}

TEST_F(ReportPageProcessesPolicyTest, TestSamePagesAreNotReportedTwice) {
  auto process_node1 = TestNodeWrapper<TestProcessNodeImpl>::Create(graph());
  process_node1->SetProcessWithPid(1, base::Process::Current(),
                                   /* launch_time=*/base::TimeTicks::Now());
  auto page_node1 = CreateNode<performance_manager::PageNodeImpl>();
  auto main_frame_node1 =
      CreateFrameNodeAutoId(process_node1.get(), page_node1.get());
  testing::MakePageNodeDiscardable(page_node1.get(), task_env());
  AdvanceClock(base::Minutes(30));
  FrameNodeImpl::UpdateCurrentFrame(
      /*previous_frame_node=*/main_frame_node1.get(),
      /*current_frame_node=*/nullptr, graph());
  AdvanceClock(base::Minutes(30));

  // The first event should report the page properly.
  policy()->HandlePageNodeEvents();
  ASSERT_EQ(policy()->report_page_processes_count_, 1);

  policy()->HandlePageNodeEvents();
  // The second event should not report anything since no state was changed.
  ASSERT_EQ(policy()->report_page_processes_count_, 1);
}

TEST_F(ReportPageProcessesPolicyTest, TestPageStateChangesCausesNewReport) {
  auto process_node1 = TestNodeWrapper<TestProcessNodeImpl>::Create(graph());
  process_node1->SetProcessWithPid(1, base::Process::Current(),
                                   /* launch_time=*/base::TimeTicks::Now());
  auto page_node1 = CreateNode<performance_manager::PageNodeImpl>();
  auto main_frame_node1 =
      CreateFrameNodeAutoId(process_node1.get(), page_node1.get());
  testing::MakePageNodeDiscardable(page_node1.get(), task_env());
  AdvanceClock(base::Minutes(30));
  FrameNodeImpl::UpdateCurrentFrame(
      /*previous_frame_node=*/main_frame_node1.get(),
      /*current_frame_node=*/nullptr, graph());
  AdvanceClock(base::Minutes(30));

  // The first event should report the page properly.
  policy()->HandlePageNodeEvents();
  ASSERT_EQ(policy()->report_page_processes_count_, 1);

  // Now that the page is audible, the pages should have been reported again.
  page_node1->SetIsAudible(true);
  policy()->HandlePageNodeEvents();
  ASSERT_EQ(policy()->report_page_processes_count_, 2);
}

TEST_F(ReportPageProcessesPolicyTest, TestAddingPageCausesNewReport) {
  auto process_node1 = TestNodeWrapper<TestProcessNodeImpl>::Create(graph());
  process_node1->SetProcessWithPid(1, base::Process::Current(),
                                   /* launch_time=*/base::TimeTicks::Now());
  auto page_node1 = CreateNode<performance_manager::PageNodeImpl>();
  auto main_frame_node1 =
      CreateFrameNodeAutoId(process_node1.get(), page_node1.get());
  testing::MakePageNodeDiscardable(page_node1.get(), task_env());
  AdvanceClock(base::Minutes(30));
  FrameNodeImpl::UpdateCurrentFrame(
      /*previous_frame_node=*/main_frame_node1.get(),
      /*current_frame_node=*/nullptr, graph());
  AdvanceClock(base::Minutes(30));

  // The first event should report the page properly.
  policy()->HandlePageNodeEvents();
  ASSERT_EQ(policy()->report_page_processes_count_, 1);

  auto process_node2 = TestNodeWrapper<TestProcessNodeImpl>::Create(graph());
  process_node2->SetProcessWithPid(2, base::Process::Current(),
                                   /* launch_time=*/base::TimeTicks::Now());
  auto page_node2 = CreateNode<performance_manager::PageNodeImpl>();
  auto main_frame_node2 =
      CreateFrameNodeAutoId(process_node2.get(), page_node2.get());
  testing::MakePageNodeDiscardable(page_node2.get(), task_env());

  // Since there is a new page, the pages should both be reported.
  policy()->HandlePageNodeEvents();
  ASSERT_EQ(policy()->report_page_processes_count_, 2);
  ASSERT_EQ(policy()->GetReportedPages().size(), 2U);
}

TEST_F(ReportPageProcessesPolicyTest, TestRemovingPageCausesNewReport) {
  auto process_node1 = TestNodeWrapper<TestProcessNodeImpl>::Create(graph());
  process_node1->SetProcessWithPid(1, base::Process::Current(),
                                   /* launch_time=*/base::TimeTicks::Now());
  auto page_node1 = CreateNode<performance_manager::PageNodeImpl>();
  auto main_frame_node1 =
      CreateFrameNodeAutoId(process_node1.get(), page_node1.get());
  testing::MakePageNodeDiscardable(page_node1.get(), task_env());
  AdvanceClock(base::Minutes(30));
  FrameNodeImpl::UpdateCurrentFrame(
      /*previous_frame_node=*/main_frame_node1.get(),
      /*current_frame_node=*/nullptr, graph());
  AdvanceClock(base::Minutes(30));

  // Creation of the first page should trigger a report.
  policy()->HandlePageNodeEvents();
  ASSERT_EQ(policy()->report_page_processes_count_, 1);
  ASSERT_EQ(policy()->GetReportedPages().size(), 1U);

  {
    auto process_node2 = TestNodeWrapper<TestProcessNodeImpl>::Create(graph());
    process_node2->SetProcessWithPid(2, base::Process::Current(),
                                     /* launch_time=*/base::TimeTicks::Now());
    auto page_node2 = CreateNode<performance_manager::PageNodeImpl>();
    auto main_frame_node2 =
        CreateFrameNodeAutoId(process_node2.get(), page_node2.get());
    testing::MakePageNodeDiscardable(page_node2.get(), task_env());

    // Creation of the second page should create another new report
    policy()->HandlePageNodeEvents();
    ASSERT_EQ(policy()->report_page_processes_count_, 2);
    ASSERT_EQ(policy()->GetReportedPages().size(), 2U);
  }

  // Since a page was removed, another report should be sent.
  policy()->HandlePageNodeEvents();
  ASSERT_EQ(policy()->report_page_processes_count_, 3);
  ASSERT_EQ(policy()->GetReportedPages().size(), 1U);
}

TEST_F(ReportPageProcessesPolicyTest, TestZeroTabsIsReported) {
  {
    auto process_node1 = TestNodeWrapper<TestProcessNodeImpl>::Create(graph());
    process_node1->SetProcessWithPid(1, base::Process::Current(),
                                     /* launch_time=*/base::TimeTicks::Now());
    auto page_node1 = CreateNode<performance_manager::PageNodeImpl>();
    auto main_frame_node1 =
        CreateFrameNodeAutoId(process_node1.get(), page_node1.get());
    testing::MakePageNodeDiscardable(page_node1.get(), task_env());

    policy()->HandlePageNodeEvents();
    ASSERT_EQ(policy()->report_page_processes_count_, 1);
    ASSERT_EQ(policy()->GetReportedPages().size(), 1U);
  }

  // Creation of the first page should trigger a report.
  policy()->HandlePageNodeEvents();
  ASSERT_EQ(policy()->report_page_processes_count_, 2);
  ASSERT_EQ(policy()->GetReportedPages().size(), 0U);
}

TEST_F(ReportPageProcessesPolicyTest, MarkedPagesAreNotReported) {
  constexpr base::ProcessId kProcessId1 = 1;
  constexpr base::ProcessId kProcessId2 = 2;

  // Creates 2 pages.
  auto process_node1 = TestNodeWrapper<TestProcessNodeImpl>::Create(graph());
  process_node1->SetProcessWithPid(kProcessId1, base::Process::Current(),
                                   /* launch_time=*/base::TimeTicks::Now());
  auto page_node1 = CreateNode<performance_manager::PageNodeImpl>();
  auto main_frame_node1 =
      CreateFrameNodeAutoId(process_node1.get(), page_node1.get());
  testing::MakePageNodeDiscardable(page_node1.get(), task_env());
  AdvanceClock(base::Minutes(30));
  FrameNodeImpl::UpdateCurrentFrame(
      /*previous_frame_node=*/main_frame_node1.get(),
      /*current_frame_node=*/nullptr, graph());
  AdvanceClock(base::Minutes(30));
  // Set page node 1 audible to raise its priority.
  page_node1->SetIsAudible(true);

  auto process_node2 = TestNodeWrapper<TestProcessNodeImpl>::Create(graph());
  process_node2->SetProcessWithPid(kProcessId2, base::Process::Current(),
                                   /* launch_time=*/base::TimeTicks::Now());
  auto page_node2 = CreateNode<performance_manager::PageNodeImpl>();
  auto main_frame_node2 =
      CreateFrameNodeAutoId(process_node2.get(), page_node2.get());
  testing::MakePageNodeDiscardable(page_node2.get(), task_env());
  AdvanceClock(base::Minutes(30));
  FrameNodeImpl::UpdateCurrentFrame(
      /*previous_frame_node=*/main_frame_node2.get(),
      /*current_frame_node=*/nullptr, graph());
  AdvanceClock(base::Minutes(30));

  // Set process 1 as marked
  PageDiscardingHelper::GetFromGraph(graph())
      ->AddDiscardAttemptMarkerForTesting(page_node1.get());

  // Trigger page node event manually.
  policy()->HandlePageNodeEvents();

  // Since page node 1 was marked, only one process should be reported (process
  // 2).
  auto processes = policy()->GetReportedPages();
  ASSERT_EQ(processes.size(), 1u);
  ASSERT_TRUE(processes.contains(kProcessId2));
}

TEST_F(ReportPageProcessesPolicyTest, LastVisibleTimeCurrent) {
  constexpr base::ProcessId kProcessId1 = 6;

  auto process_node1 = TestNodeWrapper<TestProcessNodeImpl>::Create(graph());
  process_node1->SetProcessWithPid(kProcessId1, base::Process::Current(),
                                   /* launch_time=*/base::TimeTicks::Now());
  auto page_node1 = CreateNode<performance_manager::PageNodeImpl>();
  auto main_frame_node1 =
      CreateFrameNodeAutoId(process_node1.get(), page_node1.get());
  testing::MakePageNodeDiscardable(page_node1.get(), task_env());
  AdvanceClock(base::Minutes(30));
  FrameNodeImpl::UpdateCurrentFrame(
      /*previous_frame_node=*/main_frame_node1.get(),
      /*current_frame_node=*/nullptr, graph());
  AdvanceClock(base::Minutes(30));
  // Set page node 6 focused to raise its priority.
  page_node1->SetIsVisible(true);
  page_node1->SetIsFocused(true);

  // Trigger page node event manually.
  policy()->HandlePageNodeEvents();

  // Processes with descending importance.
  auto processes = policy()->GetReportedPages();
  ASSERT_EQ(processes.size(), 1u);

  ASSERT_TRUE(processes.contains(kProcessId1));
  ASSERT_EQ(processes[kProcessId1].last_visible, base::TimeTicks::Now());
}

TEST_F(ReportPageProcessesPolicyTest, LastVisibleTimePast) {
  constexpr base::ProcessId kProcessId1 = 6;

  auto process_node1 = TestNodeWrapper<TestProcessNodeImpl>::Create(graph());
  process_node1->SetProcessWithPid(kProcessId1, base::Process::Current(),
                                   /* launch_time=*/base::TimeTicks::Now());
  auto page_node1 = CreateNode<performance_manager::PageNodeImpl>();
  auto main_frame_node1 =
      CreateFrameNodeAutoId(process_node1.get(), page_node1.get());
  testing::MakePageNodeDiscardable(page_node1.get(), task_env());
  AdvanceClock(base::Minutes(30));
  FrameNodeImpl::UpdateCurrentFrame(
      /*previous_frame_node=*/main_frame_node1.get(),
      /*current_frame_node=*/nullptr, graph());
  AdvanceClock(base::Minutes(30));
  // Set page node 6 focused to raise its priority.
  page_node1->SetIsVisible(true);
  page_node1->SetIsFocused(true);
  AdvanceClock(base::Minutes(30));
  page_node1->SetIsVisible(false);
  page_node1->SetIsFocused(false);
  AdvanceClock(base::Minutes(30));

  // Trigger page node event manually.
  policy()->HandlePageNodeEvents();

  // Processes with descending importance.
  auto processes = policy()->GetReportedPages();
  ASSERT_EQ(processes.size(), 1u);

  ASSERT_TRUE(processes.contains(kProcessId1));
  ASSERT_EQ(processes[kProcessId1].last_visible,
            base::TimeTicks::Now() - base::Minutes(30));
}

}  // namespace performance_manager::policies
