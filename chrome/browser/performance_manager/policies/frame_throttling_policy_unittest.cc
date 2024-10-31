// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/frame_throttling_policy.h"

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "components/performance_manager/test_support/mock_graphs.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager::policies {

class FrameThrottlingPolicyTest : public GraphTestHarness {
 public:
  using Super = GraphTestHarness;

  FrameThrottlingPolicyTest() = default;
  ~FrameThrottlingPolicyTest() override = default;
  FrameThrottlingPolicyTest(const FrameThrottlingPolicyTest& other) = delete;
  FrameThrottlingPolicyTest& operator=(const FrameThrottlingPolicyTest&) =
      delete;

  void SetUp() override {
    Super::SetUp();

    auto policy = std::make_unique<FrameThrottlingPolicy>();
    policy_ = policy.get();
    graph()->PassToGraph(std::move(policy));
  }

  FrameThrottlingPolicy* policy() { return policy_; }

 private:
  raw_ptr<FrameThrottlingPolicy> policy_ = nullptr;
};

// Tests that FrameThrottlingPolicy correctly throttle/unthrottle specific frame
// sink when child frame importance state changes
TEST_F(FrameThrottlingPolicyTest, ChangeFrameImportanceState) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kUnimportantFramesPriority);

  // Create a graph with a child frame as only child frames can be unimportant.
  MockSinglePageWithMultipleProcessesGraph mock_graph(graph());
  auto& frame_node = mock_graph.child_frame;

  // Make the frame visible and unimportant.
  frame_node->SetVisibility(FrameNode::Visibility::kVisible);
  frame_node->SetIsImportant(false);

  auto& id = frame_node->GetRenderFrameHostProxy().global_frame_routing_id();
  EXPECT_TRUE(policy()->IsFrameSinkThrottled(id));

  frame_node->SetIsImportant(true);
  EXPECT_FALSE(policy()->IsFrameSinkThrottled(id));
}

}  // namespace performance_manager::policies
