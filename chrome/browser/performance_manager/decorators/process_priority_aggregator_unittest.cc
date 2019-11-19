// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/decorators/process_priority_aggregator.h"

#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "components/performance_manager/test_support/mock_graphs.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

namespace {

using PriorityAndReason = frame_priority::PriorityAndReason;

static const char* kReason = FrameNodeImpl::kDefaultPriorityReason;

class ProcessPriorityAggregatorTest : public GraphTestHarness {
 public:
  void SetUp() override {
    ppa_ = new ProcessPriorityAggregator();
    graph()->PassToGraph(base::WrapUnique(ppa_));
  }

  void ExpectPriorityCounts(ProcessNodeImpl* process_node,
                            size_t user_visible_count,
                            size_t user_blocking_count) {
    auto* data = ProcessPriorityAggregator::Data::GetForTesting(process_node);
    EXPECT_EQ(user_visible_count, data->user_visible_count_for_testing());
    EXPECT_EQ(user_blocking_count, data->user_blocking_count_for_testing());
  }

  ProcessPriorityAggregator* ppa_ = nullptr;
};

}  // namespace

TEST_F(ProcessPriorityAggregatorTest, ProcessAggregation) {
  MockMultiplePagesWithMultipleProcessesGraph mock_graph(graph());

  auto& proc1 = mock_graph.process;
  auto& proc2 = mock_graph.other_process;
  auto& frame1_1 = mock_graph.frame;
  auto& frame1_2 = mock_graph.other_frame;
  auto& frame2_1 = mock_graph.child_frame;

  EXPECT_EQ(base::TaskPriority::LOWEST, proc1->priority());
  EXPECT_EQ(base::TaskPriority::LOWEST, proc2->priority());
  ExpectPriorityCounts(proc1.get(), 0, 0);
  ExpectPriorityCounts(proc2.get(), 0, 0);

  frame1_1->SetPriorityAndReason(
      PriorityAndReason(base::TaskPriority::USER_VISIBLE, kReason));
  EXPECT_EQ(base::TaskPriority::USER_VISIBLE, proc1->priority());
  EXPECT_EQ(base::TaskPriority::LOWEST, proc2->priority());
  ExpectPriorityCounts(proc1.get(), 1, 0);
  ExpectPriorityCounts(proc2.get(), 0, 0);

  frame2_1->SetPriorityAndReason(
      PriorityAndReason(base::TaskPriority::USER_VISIBLE, kReason));
  EXPECT_EQ(base::TaskPriority::USER_VISIBLE, proc1->priority());
  EXPECT_EQ(base::TaskPriority::USER_VISIBLE, proc2->priority());
  ExpectPriorityCounts(proc1.get(), 1, 0);
  ExpectPriorityCounts(proc2.get(), 1, 0);

  frame1_2->SetPriorityAndReason(
      PriorityAndReason(base::TaskPriority::USER_BLOCKING, kReason));
  EXPECT_EQ(base::TaskPriority::USER_BLOCKING, proc1->priority());
  EXPECT_EQ(base::TaskPriority::USER_VISIBLE, proc2->priority());
  ExpectPriorityCounts(proc1.get(), 1, 1);
  ExpectPriorityCounts(proc2.get(), 1, 0);

  frame1_2->SetPriorityAndReason(
      PriorityAndReason(base::TaskPriority::USER_VISIBLE, kReason));
  EXPECT_EQ(base::TaskPriority::USER_VISIBLE, proc1->priority());
  EXPECT_EQ(base::TaskPriority::USER_VISIBLE, proc2->priority());
  ExpectPriorityCounts(proc1.get(), 2, 0);
  ExpectPriorityCounts(proc2.get(), 1, 0);

  frame2_1->SetPriorityAndReason(
      PriorityAndReason(base::TaskPriority::LOWEST, kReason));
  EXPECT_EQ(base::TaskPriority::USER_VISIBLE, proc1->priority());
  EXPECT_EQ(base::TaskPriority::LOWEST, proc2->priority());
  ExpectPriorityCounts(proc1.get(), 2, 0);
  ExpectPriorityCounts(proc2.get(), 0, 0);

  frame1_1->SetPriorityAndReason(
      PriorityAndReason(base::TaskPriority::LOWEST, kReason));
  EXPECT_EQ(base::TaskPriority::USER_VISIBLE, proc1->priority());
  EXPECT_EQ(base::TaskPriority::LOWEST, proc2->priority());
  ExpectPriorityCounts(proc1.get(), 1, 0);
  ExpectPriorityCounts(proc2.get(), 0, 0);

  frame1_2->SetPriorityAndReason(
      PriorityAndReason(base::TaskPriority::LOWEST, kReason));
  EXPECT_EQ(base::TaskPriority::LOWEST, proc1->priority());
  EXPECT_EQ(base::TaskPriority::LOWEST, proc2->priority());
  ExpectPriorityCounts(proc1.get(), 0, 0);
  ExpectPriorityCounts(proc2.get(), 0, 0);
}

}  // namespace performance_manager
