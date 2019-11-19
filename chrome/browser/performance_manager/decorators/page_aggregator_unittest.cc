// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/decorators/page_aggregator.h"

#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/graph_impl_operations.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/mojom/coordination_unit.mojom.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

using performance_manager::mojom::InterventionPolicy;

namespace {

class PageAggregatorTest : public GraphTestHarness {
 public:
  void SetUp() override {
    GraphTestHarness::SetUp();
    graph()->PassToGraph(std::make_unique<PageAggregator>());
  }
};

void ExpectInitialOriginTrialFreezingPolicyWorks(
    TestGraphImpl* mock_graph,
    InterventionPolicy f0_policy,
    InterventionPolicy f1_policy,
    InterventionPolicy f0_policy_aggregated,
    InterventionPolicy f0f1_policy_aggregated) {
  TestNodeWrapper<ProcessNodeImpl> process =
      TestNodeWrapper<ProcessNodeImpl>::Create(mock_graph);
  TestNodeWrapper<PageNodeImpl> page =
      TestNodeWrapper<PageNodeImpl>::Create(mock_graph);

  // Check the initial values before any frames are added.
  EXPECT_EQ(InterventionPolicy::kDefault, page->origin_trial_freeze_policy());

  // Create an initial frame. Expect the policy to be |f0_policy_aggregated|
  // when it is made current.
  TestNodeWrapper<FrameNodeImpl> f0 =
      mock_graph->CreateFrameNodeAutoId(process.get(), page.get());
  f0->SetOriginTrialFreezePolicy(f0_policy);
  EXPECT_EQ(InterventionPolicy::kDefault, page->origin_trial_freeze_policy());
  f0->SetIsCurrent(true);
  EXPECT_EQ(f0_policy_aggregated, page->origin_trial_freeze_policy());

  // Create a second frame and expect the page policy to be
  // |f0f1_policy_aggregated| when it is made current.
  TestNodeWrapper<FrameNodeImpl> f1 =
      mock_graph->CreateFrameNodeAutoId(process.get(), page.get(), f0.get(), 1);
  f1->SetOriginTrialFreezePolicy(f1_policy);
  f1->SetIsCurrent(true);
  EXPECT_EQ(f0f1_policy_aggregated, page->origin_trial_freeze_policy());

  // Make the second frame non-current. Expect the page policy to go back to
  // |f0_policy_aggregated|.
  f1->SetIsCurrent(false);
  EXPECT_EQ(f0_policy_aggregated, page->origin_trial_freeze_policy());
  f1->SetIsCurrent(true);
  EXPECT_EQ(f0f1_policy_aggregated, page->origin_trial_freeze_policy());

  // Remove the second frame. Expect the page policy to go back to
  // |f0_policy_aggregated|.
  f1.reset();
  EXPECT_EQ(f0_policy_aggregated, page->origin_trial_freeze_policy());
}

}  // namespace

// Tests all possible combinations of Origin Trial freezing policies for 2
// frames. In this test, the policy of a frame is set before it becomes current.
TEST_F(PageAggregatorTest, InitialOriginTrialFreezingPolicy) {
  auto* mock_graph = graph();

  // Unknown x [Unknown, Default, OptIn, OptOut]

  ExpectInitialOriginTrialFreezingPolicyWorks(
      mock_graph, InterventionPolicy::kUnknown /* f0_policy */,
      InterventionPolicy::kUnknown /* f1_policy */,
      InterventionPolicy::kUnknown /* f0_policy_aggregated */,
      InterventionPolicy::kUnknown /* f0f1_policy_aggregated */);

  ExpectInitialOriginTrialFreezingPolicyWorks(
      mock_graph, InterventionPolicy::kUnknown /* f0_policy */,
      InterventionPolicy::kDefault /* f1_policy */,
      InterventionPolicy::kUnknown /* f0_policy_aggregated */,
      InterventionPolicy::kUnknown /* f0f1_policy_aggregated */);

  ExpectInitialOriginTrialFreezingPolicyWorks(
      mock_graph, InterventionPolicy::kUnknown /* f0_policy */,
      InterventionPolicy::kOptIn /* f1_policy */,
      InterventionPolicy::kUnknown /* f0_policy_aggregated */,
      InterventionPolicy::kUnknown /* f0f1_policy_aggregated */);

  ExpectInitialOriginTrialFreezingPolicyWorks(
      mock_graph, InterventionPolicy::kUnknown /* f0_policy */,
      InterventionPolicy::kOptOut /* f1_policy */,
      InterventionPolicy::kUnknown /* f0_policy_aggregated */,
      InterventionPolicy::kUnknown /* f0f1_policy_aggregated */);

  // Default x [Unknown, Default, OptIn, OptOut]

  ExpectInitialOriginTrialFreezingPolicyWorks(
      mock_graph, InterventionPolicy::kDefault /* f0_policy */,
      InterventionPolicy::kUnknown /* f1_policy */,
      InterventionPolicy::kDefault /* f0_policy_aggregated */,
      InterventionPolicy::kUnknown /* f0f1_policy_aggregated */);

  ExpectInitialOriginTrialFreezingPolicyWorks(
      mock_graph, InterventionPolicy::kDefault /* f0_policy */,
      InterventionPolicy::kDefault /* f1_policy */,
      InterventionPolicy::kDefault /* f0_policy_aggregated */,
      InterventionPolicy::kDefault /* f0f1_policy_aggregated */);

  ExpectInitialOriginTrialFreezingPolicyWorks(
      mock_graph, InterventionPolicy::kDefault /* f0_policy */,
      InterventionPolicy::kOptIn /* f1_policy */,
      InterventionPolicy::kDefault /* f0_policy_aggregated */,
      InterventionPolicy::kOptIn /* f0f1_policy_aggregated */);

  ExpectInitialOriginTrialFreezingPolicyWorks(
      mock_graph, InterventionPolicy::kDefault /* f0_policy */,
      InterventionPolicy::kOptOut /* f1_policy */,
      InterventionPolicy::kDefault /* f0_policy_aggregated */,
      InterventionPolicy::kOptOut /* f0f1_policy_aggregated */);

  // OptIn x [Unknown, Default, OptIn, OptOut]

  ExpectInitialOriginTrialFreezingPolicyWorks(
      mock_graph, InterventionPolicy::kOptIn /* f0_policy */,
      InterventionPolicy::kUnknown /* f1_policy */,
      InterventionPolicy::kOptIn /* f0_policy_aggregated */,
      InterventionPolicy::kUnknown /* f0f1_policy_aggregated */);

  ExpectInitialOriginTrialFreezingPolicyWorks(
      mock_graph, InterventionPolicy::kOptIn /* f0_policy */,
      InterventionPolicy::kDefault /* f1_policy */,
      InterventionPolicy::kOptIn /* f0_policy_aggregated */,
      InterventionPolicy::kOptIn /* f0f1_policy_aggregated */);

  ExpectInitialOriginTrialFreezingPolicyWorks(
      mock_graph, InterventionPolicy::kOptIn /* f0_policy */,
      InterventionPolicy::kOptIn /* f1_policy */,
      InterventionPolicy::kOptIn /* f0_policy_aggregated */,
      InterventionPolicy::kOptIn /* f0f1_policy_aggregated */);

  ExpectInitialOriginTrialFreezingPolicyWorks(
      mock_graph, InterventionPolicy::kOptIn /* f0_policy */,
      InterventionPolicy::kOptOut /* f1_policy */,
      InterventionPolicy::kOptIn /* f0_policy_aggregated */,
      InterventionPolicy::kOptOut /* f0f1_policy_aggregated */);

  // OptOut x [Unknown, Default, OptIn, OptOut]

  ExpectInitialOriginTrialFreezingPolicyWorks(
      mock_graph, InterventionPolicy::kOptOut /* f0_policy */,
      InterventionPolicy::kUnknown /* f1_policy */,
      InterventionPolicy::kOptOut /* f0_policy_aggregated */,
      InterventionPolicy::kUnknown /* f0f1_policy_aggregated */);

  ExpectInitialOriginTrialFreezingPolicyWorks(
      mock_graph, InterventionPolicy::kOptOut /* f0_policy */,
      InterventionPolicy::kDefault /* f1_policy */,
      InterventionPolicy::kOptOut /* f0_policy_aggregated */,
      InterventionPolicy::kOptOut /* f0f1_policy_aggregated */);

  ExpectInitialOriginTrialFreezingPolicyWorks(
      mock_graph, InterventionPolicy::kOptOut /* f0_policy */,
      InterventionPolicy::kOptIn /* f1_policy */,
      InterventionPolicy::kOptOut /* f0_policy_aggregated */,
      InterventionPolicy::kOptOut /* f0f1_policy_aggregated */);

  ExpectInitialOriginTrialFreezingPolicyWorks(
      mock_graph, InterventionPolicy::kOptOut /* f0_policy */,
      InterventionPolicy::kOptOut /* f1_policy */,
      InterventionPolicy::kOptOut /* f0_policy_aggregated */,
      InterventionPolicy::kOptOut /* f0f1_policy_aggregated */);
}

// Test changing the Origin Trial Freezing policy of a frame after it becomes
// current.
TEST_F(PageAggregatorTest, OriginTrialFreezingPolicyChanges) {
  auto process = CreateNode<ProcessNodeImpl>();
  auto page = CreateNode<PageNodeImpl>();
  TestNodeWrapper<FrameNodeImpl> frame =
      graph()->CreateFrameNodeAutoId(process.get(), page.get());
  frame->SetIsCurrent(true);

  EXPECT_EQ(InterventionPolicy::kUnknown, page->origin_trial_freeze_policy());
  frame->SetOriginTrialFreezePolicy(InterventionPolicy::kOptIn);
  EXPECT_EQ(InterventionPolicy::kOptIn, page->origin_trial_freeze_policy());
  frame->SetOriginTrialFreezePolicy(InterventionPolicy::kOptOut);
  EXPECT_EQ(InterventionPolicy::kOptOut, page->origin_trial_freeze_policy());
  frame->SetOriginTrialFreezePolicy(InterventionPolicy::kDefault);
  EXPECT_EQ(InterventionPolicy::kDefault, page->origin_trial_freeze_policy());
  frame->SetOriginTrialFreezePolicy(InterventionPolicy::kUnknown);
  EXPECT_EQ(InterventionPolicy::kUnknown, page->origin_trial_freeze_policy());
}

TEST_F(PageAggregatorTest, WebLocksAggregation) {
  // Creates a page containing 2 frames.
  auto process = CreateNode<ProcessNodeImpl>();
  auto page = CreateNode<PageNodeImpl>();
  TestNodeWrapper<FrameNodeImpl> frame_0 =
      graph()->CreateFrameNodeAutoId(process.get(), page.get());
  TestNodeWrapper<FrameNodeImpl> frame_1 =
      graph()->CreateFrameNodeAutoId(process.get(), page.get());

  // By default the page shouldn't hold any WebLock.
  EXPECT_FALSE(page->is_holding_weblock());

  // |frame_0| now holds a WebLock, the corresponding property should be set on
  // the page node.
  frame_0->SetIsHoldingWebLock(true);
  EXPECT_TRUE(page->is_holding_weblock());

  // |frame_1| also holding a WebLock shouldn't affect the page property.
  frame_1->SetIsHoldingWebLock(true);
  EXPECT_TRUE(page->is_holding_weblock());

  // |frame_1| still holds a WebLock after this.
  frame_0->SetIsHoldingWebLock(false);
  EXPECT_TRUE(page->is_holding_weblock());

  // Destroying |frame_1| without explicitly releasing the WebLock it's
  // holding should update the corresponding page property.
  frame_1.reset();
  EXPECT_FALSE(page->is_holding_weblock());
}

TEST_F(PageAggregatorTest, IndexedDBLocksAggregation) {
  // Creates a page containing 2 frames.
  auto process = CreateNode<ProcessNodeImpl>();
  auto page = CreateNode<PageNodeImpl>();
  TestNodeWrapper<FrameNodeImpl> frame_0 =
      graph()->CreateFrameNodeAutoId(process.get(), page.get());
  TestNodeWrapper<FrameNodeImpl> frame_1 =
      graph()->CreateFrameNodeAutoId(process.get(), page.get());

  // By default the page shouldn't hold any IndexedDB lock.
  EXPECT_FALSE(page->is_holding_indexeddb_lock());

  // |frame_0| now holds an IndexedDB lock, the corresponding property should be
  // set on the page node.
  frame_0->SetIsHoldingIndexedDBLock(true);
  EXPECT_TRUE(page->is_holding_indexeddb_lock());

  // |frame_1| also holding an IndexedDB lock shouldn't affect the page
  // property.
  frame_1->SetIsHoldingIndexedDBLock(true);
  EXPECT_TRUE(page->is_holding_indexeddb_lock());

  // |frame_1| still holds an IndexedDB lock after this.
  frame_0->SetIsHoldingIndexedDBLock(false);
  EXPECT_TRUE(page->is_holding_indexeddb_lock());

  // Destroying |frame_1| without explicitly releasing the IndexedDB lock it's
  // holding should update the corresponding page property.
  frame_1.reset();
  EXPECT_FALSE(page->is_holding_indexeddb_lock());
}

}  // namespace performance_manager
