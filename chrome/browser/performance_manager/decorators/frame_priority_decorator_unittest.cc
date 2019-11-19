// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/decorators/frame_priority_decorator.h"

#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/test_support/frame_priority.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {
namespace frame_priority {

namespace {

using testing::_;

class LenientMockFrameNodeObserver : public FrameNode::ObserverDefaultImpl {
 public:
  LenientMockFrameNodeObserver() = default;
  ~LenientMockFrameNodeObserver() override = default;

  MOCK_METHOD2(OnPriorityAndReasonChanged,
               void(const FrameNode*, const PriorityAndReason&));

 private:
  DISALLOW_COPY_AND_ASSIGN(LenientMockFrameNodeObserver);
};

using MockFrameNodeObserver =
    ::testing::StrictMock<LenientMockFrameNodeObserver>;

class FramePriorityDecoratorTest : public GraphTestHarness {
 public:
  FramePriorityDecoratorTest() = default;
  ~FramePriorityDecoratorTest() override = default;
};

}  // namespace

TEST_F(FramePriorityDecoratorTest, VotesForwardedToGraph) {
  FramePriorityDecorator* fpd = new FramePriorityDecorator();
  graph()->PassToGraph(base::WrapUnique(fpd));

  TestNodeWrapper<ProcessNodeImpl> process = CreateNode<ProcessNodeImpl>();
  TestNodeWrapper<PageNodeImpl> page = CreateNode<PageNodeImpl>();
  TestNodeWrapper<FrameNodeImpl> frame =
      graph()->CreateFrameNodeAutoId(process.get(), page.get());

  test::DummyVoter voter;
  voter.SetVotingChannel(fpd->GetVotingChannel());

  MockFrameNodeObserver obs;
  graph()->AddFrameNodeObserver(&obs);

  EXPECT_EQ(base::TaskPriority::LOWEST,
            frame->priority_and_reason().priority());
  EXPECT_EQ(FrameNodeImpl::kDefaultPriorityReason,
            frame->priority_and_reason().reason());

  // Do not expect a notification when an identical vote is submitted.
  voter.EmitVote(frame.get(), frame->priority_and_reason().priority(),
                 frame->priority_and_reason().reason());
  auto& receipt = voter.receipts_[0];
  testing::Mock::VerifyAndClear(&obs);

  // Update the vote with a new priority and expect that to propagate.
  EXPECT_CALL(obs, OnPriorityAndReasonChanged(frame.get(), _));
  receipt.ChangeVote(base::TaskPriority::HIGHEST, test::DummyVoter::kReason);
  testing::Mock::VerifyAndClear(&obs);
  EXPECT_EQ(base::TaskPriority::HIGHEST,
            frame->priority_and_reason().priority());
  EXPECT_EQ(test::DummyVoter::kReason, frame->priority_and_reason().reason());

  // Cancel the existing vote and expect it to go back to the default.
  EXPECT_CALL(obs, OnPriorityAndReasonChanged(frame.get(), _));
  receipt.Reset();
  testing::Mock::VerifyAndClear(&obs);
  EXPECT_EQ(base::TaskPriority::LOWEST,
            frame->priority_and_reason().priority());
  EXPECT_EQ(FrameNodeImpl::kDefaultPriorityReason,
            frame->priority_and_reason().reason());

  graph()->RemoveFrameNodeObserver(&obs);
}

}  // namespace frame_priority
}  // namespace performance_manager
