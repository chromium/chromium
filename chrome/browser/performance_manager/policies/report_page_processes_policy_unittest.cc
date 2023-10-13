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

  std::vector<base::ProcessId> GetBackgroundProcesses() {
    return background_pids_;
  }

 protected:
  void ReportBackgroundProcesses(std::vector<base::ProcessId> pids) override {
    background_pids_ = pids;
  }

 private:
  std::vector<base::ProcessId> background_pids_;
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

TEST_F(ReportPageProcessesPolicyTest, ReportBackgroundProcesses) {
  constexpr base::ProcessId kProcessId1 = 1;
  constexpr base::ProcessId kProcessId2 = 2;
  constexpr base::ProcessId kProcessId3 = 3;
  constexpr base::ProcessId kProcessId4 = 4;

  // Creates 4 pages.
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

  policy()->HandlePageNodeEvents();

  auto background_pids = policy()->GetBackgroundProcesses();
  // Because page node 1 is audible, it's not in background.
  ASSERT_EQ(background_pids.size(), 3u);
  ASSERT_EQ(
      std::count(background_pids.begin(), background_pids.end(), kProcessId2),
      1);
  ASSERT_EQ(
      std::count(background_pids.begin(), background_pids.end(), kProcessId3),
      1);
  ASSERT_EQ(
      std::count(background_pids.begin(), background_pids.end(), kProcessId4),
      1);
}

}  // namespace performance_manager::policies
