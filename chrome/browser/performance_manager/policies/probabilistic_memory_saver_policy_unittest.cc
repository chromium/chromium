// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/probabilistic_memory_saver_policy.h"

#include "chrome/browser/performance_manager/test_support/page_discarding_utils.h"

namespace performance_manager {

class TestEstimator
    : public ProactiveDiscardEvaluator::RevisitProbabilityEstimator {
 public:
  float ComputeRevisitProbability(
      const TabPageDecorator::TabHandle* tab_handle) override {
    auto it = probabilities_.find(tab_handle->page_node());
    CHECK(it != probabilities_.end());
    return it->second;
  }

  void SetProbabilityForPageNode(const PageNode* page_node, float prob) {
    probabilities_[page_node] = prob;
  }

  std::map<const PageNode*, float> probabilities_;
};

class ProbabilisticMemoySaverPolicyTest
    : public testing::GraphTestHarnessWithMockDiscarder {
 public:
  void SetUp() override {
    testing::GraphTestHarnessWithMockDiscarder::SetUp();
    graph()->PassToGraph(
        std::make_unique<performance_manager::TabPageDecorator>());

    // This is usually called when the profile is created. Fake it here since it
    // doesn't happen in tests.
    policies::PageDiscardingHelper::GetFromGraph(graph())
        ->SetNoDiscardPatternsForProfile(
            static_cast<PageNode*>(page_node())->GetBrowserContextID(), {});

    auto policy = std::make_unique<ProbabilisticMemorySaverPolicy>(
        /*simulation_mode=*/false,
        base::BindRepeating(&ProbabilisticMemoySaverPolicyTest::CreateEstimator,
                            base::Unretained(this)));
    policy_ = policy.get();
    graph()->PassToGraph(std::move(policy));
  }

  void TearDown() override {
    estimator_ = nullptr;
    // We get the unique_ptr from the graph to keep it alive long enough to
    // clear our raw_ptr to the policy and thus avoid a dangling raw_ptr.
    std::unique_ptr<GraphOwned> taken_policy = graph()->TakeFromGraph(policy_);
    policy_ = nullptr;
    testing::GraphTestHarnessWithMockDiscarder::TearDown();
  }

  ProbabilisticMemorySaverPolicy* policy() { return policy_; }

  TestEstimator* estimator() { return estimator_; }

  base::TimeDelta GetHeartbeatInterval() {
    return features::kProactiveDiscardingSamplingInterval.Get();
  }

 private:
  std::unique_ptr<ProactiveDiscardEvaluator::RevisitProbabilityEstimator>
  CreateEstimator(Graph* graph) {
    CHECK(!estimator_);

    std::unique_ptr<TestEstimator> estimator =
        std::make_unique<TestEstimator>();
    estimator_ = estimator.get();
    return estimator;
  }

  raw_ptr<TestEstimator> estimator_;
  raw_ptr<ProbabilisticMemorySaverPolicy> policy_;
};

TEST_F(ProbabilisticMemoySaverPolicyTest, DontDiscardIfLikelyToRevisit) {
  page_node()->SetType(PageType::kTab);
  page_node()->SetIsVisible(true);
  page_node()->SetIsVisible(false);
  estimator()->SetProbabilityForPageNode(page_node(), 1.0f);
  task_env().FastForwardBy(GetHeartbeatInterval());
  ::testing::Mock::VerifyAndClearExpectations(discarder());
}

TEST_F(ProbabilisticMemoySaverPolicyTest, DiscardIfUnlikelyToRevisit) {
  page_node()->SetType(PageType::kTab);
  page_node()->SetIsVisible(true);
  page_node()->SetIsVisible(false);
  estimator()->SetProbabilityForPageNode(page_node(), 0.0f);

  EXPECT_CALL(*discarder(), DiscardPageNodeImpl(page_node()))
      .WillOnce(::testing::Return(true));
  task_env().FastForwardBy(GetHeartbeatInterval());
  ::testing::Mock::VerifyAndClearExpectations(discarder());
}

}  // namespace performance_manager
