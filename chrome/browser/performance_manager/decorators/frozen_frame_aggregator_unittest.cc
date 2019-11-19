// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/decorators/frozen_frame_aggregator.h"

#include <memory>

#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

namespace {

using LifecycleState = PageNodeImpl::LifecycleState;

class LenientMockProcessNodeObserver : public ProcessNode::ObserverDefaultImpl {
 public:
  LenientMockProcessNodeObserver() = default;
  ~LenientMockProcessNodeObserver() override = default;

  MOCK_METHOD1(OnAllFramesInProcessFrozen, void(const ProcessNode*));

 private:
  DISALLOW_COPY_AND_ASSIGN(LenientMockProcessNodeObserver);
};

using MockProcessNodeObserver =
    ::testing::StrictMock<LenientMockProcessNodeObserver>;

}  // namespace

class FrozenFrameAggregatorTest : public GraphTestHarness {
 protected:
  FrozenFrameAggregatorTest() = default;
  ~FrozenFrameAggregatorTest() override = default;

  void SetUp() override {
    ffa_ = new FrozenFrameAggregator();
    graph()->PassToGraph(base::WrapUnique(ffa_));
    process_node_ = CreateNode<ProcessNodeImpl>();
    page_node_ = CreateNode<PageNodeImpl>();
  }

  template <typename NodeType>
  void ExpectData(NodeType* node,
                  uint32_t current_frame_count,
                  uint32_t frozen_frame_count) {
    auto* data = FrozenFrameAggregator::Data::GetForTesting(node);
    EXPECT_TRUE(data);
    EXPECT_EQ(current_frame_count, data->current_frame_count);
    EXPECT_EQ(frozen_frame_count, data->frozen_frame_count);
  }

  void ExpectPageData(uint32_t current_frame_count,
                      uint32_t frozen_frame_count) {
    ExpectData(page_node_.get(), current_frame_count, frozen_frame_count);
  }

  void ExpectProcessData(uint32_t current_frame_count,
                         uint32_t frozen_frame_count) {
    ExpectData(process_node_.get(), current_frame_count, frozen_frame_count);
  }

  void ExpectRunning() {
    EXPECT_EQ(LifecycleState::kRunning, page_node_.get()->lifecycle_state());
  }

  void ExpectFrozen() {
    EXPECT_EQ(LifecycleState::kFrozen, page_node_.get()->lifecycle_state());
  }

  TestNodeWrapper<FrameNodeImpl> CreateFrame(FrameNodeImpl* parent_frame_node,
                                             int frame_tree_node_id) {
    return CreateFrameNodeAutoId(process_node_.get(), page_node_.get(),
                                 parent_frame_node, frame_tree_node_id);
  }

  FrozenFrameAggregator* ffa_;
  TestNodeWrapper<ProcessNodeImpl> process_node_;
  TestNodeWrapper<PageNodeImpl> page_node_;

 private:
  DISALLOW_COPY_AND_ASSIGN(FrozenFrameAggregatorTest);
};

TEST_F(FrozenFrameAggregatorTest, ProcessAggregation) {
  MockProcessNodeObserver obs;
  graph()->AddProcessNodeObserver(&obs);

  ExpectProcessData(0, 0);

  // Add a main frame.
  auto f0 = CreateFrame(nullptr, 0);
  ExpectProcessData(0, 0);

  // Make the frame current.
  f0.get()->SetIsCurrent(true);
  ExpectProcessData(1, 0);

  // Make the frame frozen and expect a notification.
  EXPECT_CALL(obs, OnAllFramesInProcessFrozen(process_node_.get()));
  f0.get()->SetLifecycleState(LifecycleState::kFrozen);
  testing::Mock::VerifyAndClear(&obs);
  ExpectProcessData(1, 1);

  // Create another process and another page.
  auto proc2 = CreateNode<ProcessNodeImpl>();
  auto page2 = CreateNode<PageNodeImpl>();
  ExpectProcessData(1, 1);

  // Create a child frame for the first page hosted in the second process.
  auto f1 = CreateFrameNodeAutoId(proc2.get(), page_node_.get(), f0.get(), 1);
  ExpectProcessData(1, 1);

  // Immediately make it current.
  f1.get()->SetIsCurrent(true);
  ExpectProcessData(1, 1);

  // Freeze the child frame and expect |proc2| to receive an event, but not
  // |process_node_|.
  EXPECT_CALL(obs, OnAllFramesInProcessFrozen(proc2.get()));
  f1.get()->SetLifecycleState(LifecycleState::kFrozen);
  ExpectProcessData(1, 1);

  // Unfreeze both frames.
  f0.get()->SetLifecycleState(LifecycleState::kRunning);
  ExpectProcessData(1, 0);
  f1.get()->SetLifecycleState(LifecycleState::kRunning);
  ExpectProcessData(1, 0);

  // Create a main frame in the second page, but that's in the first process.
  auto f2 = CreateFrameNodeAutoId(process_node_.get(), page2.get(), nullptr, 2);
  ExpectProcessData(1, 0);

  // Freeze the main frame in the second page.
  f2.get()->SetLifecycleState(LifecycleState::kFrozen);
  ExpectProcessData(1, 0);

  // Make the frozen second main frame current.
  f2.get()->SetIsCurrent(true);
  ExpectProcessData(2, 1);

  // Freeze the child frame of the first page, hosted in the other process.
  EXPECT_CALL(obs, OnAllFramesInProcessFrozen(proc2.get()));
  f1.get()->SetLifecycleState(LifecycleState::kFrozen);
  ExpectProcessData(2, 1);

  // Freeze the main frame of the first page.
  EXPECT_CALL(obs, OnAllFramesInProcessFrozen(process_node_.get()));
  f0.get()->SetLifecycleState(LifecycleState::kFrozen);
  testing::Mock::VerifyAndClear(&obs);
  ExpectProcessData(2, 2);

  // Destroy the child frame in the other process, and then kill that process.
  f1.reset();
  ExpectProcessData(2, 2);
  proc2.reset();
  ExpectProcessData(2, 2);

  // Kill the main frame of the second page.
  f2.reset();
  ExpectProcessData(1, 1);

  // Kill the main frame of the first page.
  f0.reset();
  ExpectProcessData(0, 0);

  graph()->RemoveProcessNodeObserver(&obs);
}

TEST_F(FrozenFrameAggregatorTest, PageAggregation) {
  ExpectPageData(0, 0);
  ExpectRunning();

  // Add a non-current frame.
  auto f0 = CreateFrame(nullptr, 0);
  ExpectPageData(0, 0);
  ExpectRunning();

  // Make the frame current.
  f0.get()->SetIsCurrent(true);
  ExpectPageData(1, 0);
  ExpectRunning();

  // Freeze the frame.
  f0.get()->SetLifecycleState(LifecycleState::kFrozen);
  ExpectPageData(1, 1);
  ExpectFrozen();

  // Unfreeze the frame.
  f0.get()->SetLifecycleState(LifecycleState::kRunning);
  ExpectPageData(1, 0);
  ExpectRunning();

  // Add a child frame.
  auto f1 = CreateFrame(f0.get(), 1);
  ExpectPageData(1, 0);
  ExpectRunning();

  // Make it current as well.
  f1.get()->SetIsCurrent(true);
  ExpectPageData(2, 0);
  ExpectRunning();

  // Freeze them both.
  f1.get()->SetLifecycleState(LifecycleState::kFrozen);
  ExpectPageData(2, 1);
  ExpectRunning();
  f0.get()->SetLifecycleState(LifecycleState::kFrozen);
  ExpectPageData(2, 2);
  ExpectFrozen();

  // Unfreeze them both.
  f0.get()->SetLifecycleState(LifecycleState::kRunning);
  ExpectPageData(2, 1);
  ExpectRunning();
  f1.get()->SetLifecycleState(LifecycleState::kRunning);
  ExpectPageData(2, 0);
  ExpectRunning();

  // Create a third frame.
  auto f1a = CreateFrame(f0.get(), 1);
  ExpectPageData(2, 0);
  ExpectRunning();

  // Swap the f1 and f1a.
  f1.get()->SetIsCurrent(false);
  ExpectPageData(1, 0);
  ExpectRunning();
  f1a.get()->SetIsCurrent(true);
  ExpectPageData(2, 0);
  ExpectRunning();

  // Freeze the original frame and swap it back.
  f1.get()->SetLifecycleState(LifecycleState::kFrozen);
  f1a.get()->SetIsCurrent(false);
  ExpectPageData(1, 0);
  ExpectRunning();
  f1.get()->SetIsCurrent(true);
  ExpectPageData(2, 1);
  ExpectRunning();

  // Freeze the non-current frame and expect nothing to change.
  f1a.get()->SetLifecycleState(LifecycleState::kFrozen);
  ExpectPageData(2, 1);
  ExpectRunning();

  // Remove the non-current frame and expect nothing to change.
  f1a.reset();
  ExpectPageData(2, 1);
  ExpectRunning();

  // Remove the frozen child frame and expect a change.
  f1.reset();
  ExpectPageData(1, 0);
  ExpectRunning();

  // Freeze the main frame again.
  f0.get()->SetLifecycleState(LifecycleState::kFrozen);
  ExpectPageData(1, 1);
  ExpectFrozen();

  // Remove the main frame. An empty page is always considered as "running".
  f0.reset();
  ExpectPageData(0, 0);
  ExpectRunning();
}

}  // namespace performance_manager
