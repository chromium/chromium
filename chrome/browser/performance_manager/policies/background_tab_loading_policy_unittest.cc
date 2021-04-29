// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/background_tab_loading_policy.h"

#include <memory>
#include <vector>

#include "chrome/browser/performance_manager/mechanisms/page_loader.h"
#include "components/performance_manager/graph/graph_impl.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/public/decorators/tab_properties_decorator.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

namespace policies {

// Mock version of a performance_manager::mechanism::PageLoader.
class LenientMockPageLoader
    : public performance_manager::mechanism::PageLoader {
 public:
  LenientMockPageLoader() = default;
  ~LenientMockPageLoader() override = default;
  LenientMockPageLoader(const LenientMockPageLoader& other) = delete;
  LenientMockPageLoader& operator=(const LenientMockPageLoader&) = delete;

  MOCK_METHOD1(LoadPageNode, void(const PageNode* page_node));
};
using MockPageLoader = ::testing::StrictMock<LenientMockPageLoader>;

class BackgroundTabLoadingPolicyTest : public GraphTestHarness {
 public:
  using Super = GraphTestHarness;

  BackgroundTabLoadingPolicyTest() = default;
  ~BackgroundTabLoadingPolicyTest() override = default;
  BackgroundTabLoadingPolicyTest(const BackgroundTabLoadingPolicyTest& other) =
      delete;
  BackgroundTabLoadingPolicyTest& operator=(
      const BackgroundTabLoadingPolicyTest&) = delete;

  void SetUp() override {
    Super::SetUp();

    system_node_ = std::make_unique<TestNodeWrapper<SystemNodeImpl>>(
        TestNodeWrapper<SystemNodeImpl>::Create(graph()));

    // Create the policy.
    auto policy = std::make_unique<BackgroundTabLoadingPolicy>();
    policy_ = policy.get();
    graph()->PassToGraph(std::move(policy));

    // Make the policy use a mock PageLoader.
    auto mock_loader = std::make_unique<MockPageLoader>();
    mock_loader_ = mock_loader.get();
    policy_->SetMockLoaderForTesting(std::move(mock_loader));

    // Set a value explicitly for thresholds that depends on system information.
    policy_->SetMaxSimultaneousLoadsForTesting(4);
    policy_->SetFreeMemoryForTesting(150);
  }

  void TearDown() override {
    graph()->TakeFromGraph(policy_);
    system_node_->reset();
    Super::TearDown();
  }

 protected:
  BackgroundTabLoadingPolicy* policy() { return policy_; }
  MockPageLoader* loader() { return mock_loader_; }

  SystemNodeImpl* system_node() { return system_node_.get()->get(); }

 private:
  std::unique_ptr<
      performance_manager::TestNodeWrapper<performance_manager::SystemNodeImpl>>
      system_node_;
  BackgroundTabLoadingPolicy* policy_;
  MockPageLoader* mock_loader_;
};

TEST_F(BackgroundTabLoadingPolicyTest, ScheduleLoadForRestoredTabs) {
  std::vector<
      performance_manager::TestNodeWrapper<performance_manager::PageNodeImpl>>
      page_nodes;
  std::vector<PageNode*> raw_page_nodes;

  // Create vector of PageNode to restore.
  for (int i = 0; i < 4; i++) {
    page_nodes.push_back(CreateNode<performance_manager::PageNodeImpl>());
    raw_page_nodes.push_back(page_nodes.back().get());
    EXPECT_CALL(*loader(), LoadPageNode(raw_page_nodes.back()));

    // Set |is_tab| property as this is a requirement to pass the PageNode to
    // ScheduleLoadForRestoredTabs().
    TabPropertiesDecorator::SetIsTabForTesting(raw_page_nodes.back(), true);
  }

  policy()->ScheduleLoadForRestoredTabs(raw_page_nodes);
  task_env().RunUntilIdle();
}

TEST_F(BackgroundTabLoadingPolicyTest, AllLoadingSlotsUsed) {
  // Create 4 PageNode to restore.
  std::vector<
      performance_manager::TestNodeWrapper<performance_manager::PageNodeImpl>>
      page_nodes;
  std::vector<PageNode*> raw_page_nodes;

  // Create vector of PageNode to restore.
  for (int i = 0; i < 4; i++) {
    page_nodes.push_back(CreateNode<performance_manager::PageNodeImpl>());
    raw_page_nodes.push_back(page_nodes.back().get());

    // Set |is_tab| property as this is a requirement to pass the PageNode to
    // ScheduleLoadForRestoredTabs().
    TabPropertiesDecorator::SetIsTabForTesting(raw_page_nodes.back(), true);
  }
  PageNodeImpl* page_node_impl = page_nodes[0].get();

  EXPECT_CALL(*loader(), LoadPageNode(raw_page_nodes[0]));
  EXPECT_CALL(*loader(), LoadPageNode(raw_page_nodes[1]));

  // Use 2 loading slots, which means only 2 of the PageNodes should immediately
  // be scheduled to load.
  policy()->SetMaxSimultaneousLoadsForTesting(2);

  policy()->ScheduleLoadForRestoredTabs(raw_page_nodes);
  task_env().RunUntilIdle();
  testing::Mock::VerifyAndClear(loader());

  // Simulate load start of a PageNode that initiated load.
  page_node_impl->SetLoadingState(PageNode::LoadingState::kLoading);

  // The policy should allow one more PageNode to load after a PageNode finishes
  // loading.
  EXPECT_CALL(*loader(), LoadPageNode(raw_page_nodes[2]));

  // Simulate load finish of a PageNode.
  page_node_impl->SetLoadingState(PageNode::LoadingState::kLoadedIdle);
}

TEST_F(BackgroundTabLoadingPolicyTest, ShouldLoad_MaxTabsToRestore) {
  // Create vector of PageNodes to restore.
  std::vector<
      performance_manager::TestNodeWrapper<performance_manager::PageNodeImpl>>
      page_nodes;
  std::vector<PageNode*> raw_page_nodes;

  for (uint32_t i = 0; i < policy()->kMaxTabsToLoad + 1; i++) {
    page_nodes.push_back(CreateNode<performance_manager::PageNodeImpl>());
    raw_page_nodes.push_back(page_nodes.back().get());
  }

  // Test the maximum number of tabs to load threshold.
  for (uint32_t i = 0; i < policy()->kMaxTabsToLoad; i++) {
    EXPECT_TRUE(policy()->ShouldLoad(raw_page_nodes[i]));
    policy()->tab_loads_started_++;
  }
  EXPECT_FALSE(policy()->ShouldLoad(raw_page_nodes[policy()->kMaxTabsToLoad]));
}

TEST_F(BackgroundTabLoadingPolicyTest, ShouldLoad_MinTabsToRestore) {
  // Create vector of PageNodes to restore.
  std::vector<
      performance_manager::TestNodeWrapper<performance_manager::PageNodeImpl>>
      page_nodes;
  std::vector<PageNode*> raw_page_nodes;

  for (uint32_t i = 0; i < policy()->kMinTabsToLoad + 1; i++) {
    page_nodes.push_back(CreateNode<performance_manager::PageNodeImpl>());
    raw_page_nodes.push_back(page_nodes.back().get());
  }

  // When free memory limit is reached.
  const size_t kFreeMemoryLimit = policy()->kDesiredAmountOfFreeMemoryMb - 1;
  policy()->SetFreeMemoryForTesting(kFreeMemoryLimit);

  // Test that the minimum number of tabs to load is respected.
  for (uint32_t i = 0; i < policy()->kMinTabsToLoad; i++) {
    EXPECT_TRUE(policy()->ShouldLoad(raw_page_nodes[i]));
    policy()->tab_loads_started_++;
  }
  EXPECT_FALSE(policy()->ShouldLoad(raw_page_nodes[policy()->kMinTabsToLoad]));
}

TEST_F(BackgroundTabLoadingPolicyTest, ShouldLoad_FreeMemory) {
  // Create a PageNode to restore.
  performance_manager::TestNodeWrapper<performance_manager::PageNodeImpl>
      page_node;
  PageNode* raw_page_node;

  page_node = CreateNode<performance_manager::PageNodeImpl>();
  raw_page_node = page_node.get();

  // Simulate that kMinTabsToLoad have loaded.
  policy()->tab_loads_started_ = policy()->kMinTabsToLoad;

  // Test the free memory constraint.
  const size_t kFreeMemoryLimit = policy()->kDesiredAmountOfFreeMemoryMb;
  policy()->SetFreeMemoryForTesting(kFreeMemoryLimit);
  EXPECT_TRUE(policy()->ShouldLoad(raw_page_node));
  policy()->SetFreeMemoryForTesting(kFreeMemoryLimit - 1);
  EXPECT_FALSE(policy()->ShouldLoad(raw_page_node));
  policy()->SetFreeMemoryForTesting(kFreeMemoryLimit + 1);
  EXPECT_TRUE(policy()->ShouldLoad(raw_page_node));
}

TEST_F(BackgroundTabLoadingPolicyTest, ShouldLoad_OldTab) {
  // Create an old tab.
  performance_manager::TestNodeWrapper<performance_manager::PageNodeImpl>
      page_node;
  PageNode* raw_page_node;

  page_node = CreateNode<performance_manager::PageNodeImpl>(
      WebContentsProxy(), std::string(), GURL(), false, false,
      base::TimeTicks::Now() - (base::TimeDelta::FromSeconds(1) +
                                policy()->kMaxTimeSinceLastUseToLoad));
  raw_page_node = page_node.get();

  // Simulate that kMinTabsToLoad have loaded.
  policy()->tab_loads_started_ = policy()->kMinTabsToLoad;

  // Test the max time since last use threshold.
  EXPECT_FALSE(policy()->ShouldLoad(raw_page_node));
}

TEST_F(BackgroundTabLoadingPolicyTest, ScoreAndScheduleTabLoad) {
  // Use 1 loading slot so only one PageNode loads at a time.
  policy()->SetMaxSimultaneousLoadsForTesting(1);

  // Create PageNodes with decreasing last visibility time (oldest to newest).
  std::vector<
      performance_manager::TestNodeWrapper<performance_manager::PageNodeImpl>>
      page_nodes;
  std::vector<PageNode*> raw_page_nodes;

  // Add a old tab to restore.
  page_nodes.push_back(CreateNode<performance_manager::PageNodeImpl>(
      WebContentsProxy(), std::string(), GURL(), false, false,
      base::TimeTicks::Now() - base::TimeDelta::FromDays(30)));
  raw_page_nodes.push_back(page_nodes.back().get());

  // Add a recent tab to restore.
  page_nodes.push_back(CreateNode<performance_manager::PageNodeImpl>(
      WebContentsProxy(), std::string(), GURL(), false, false,
      base::TimeTicks::Now() - base::TimeDelta::FromSeconds(1)));
  raw_page_nodes.push_back(page_nodes.back().get());

  // Add an internal page to restore.
  page_nodes.push_back(CreateNode<performance_manager::PageNodeImpl>(
      WebContentsProxy(), std::string(), GURL("chrome://newtab"), false, false,
      base::TimeTicks::Now() - base::TimeDelta::FromSeconds(1)));
  raw_page_nodes.push_back(page_nodes.back().get());

  // Set |is_tab| property as this is a requirement to pass the PageNode to
  // ScheduleLoadForRestoredTabs().
  for (auto* page_node : raw_page_nodes) {
    TabPropertiesDecorator::SetIsTabForTesting(page_node, true);
  }

  // Test that the score produces the expected loading order
  EXPECT_CALL(*loader(), LoadPageNode(raw_page_nodes[1]));

  policy()->ScheduleLoadForRestoredTabs(raw_page_nodes);
  task_env().RunUntilIdle();
  testing::Mock::VerifyAndClear(loader());

  PageNodeImpl* page_node_impl = page_nodes[1].get();

  // Simulate load start of a PageNode that initiated load.
  page_node_impl->SetLoadingState(PageNode::LoadingState::kLoading);

  // The policy should allow one more PageNode to load after a PageNode finishes
  // loading.
  EXPECT_CALL(*loader(), LoadPageNode(raw_page_nodes[0]));

  // Simulate load finish of a PageNode.
  page_node_impl->SetLoadingState(PageNode::LoadingState::kLoadedIdle);

  testing::Mock::VerifyAndClear(loader());

  page_node_impl = page_nodes[0].get();

  // Simulate load start of a PageNode that initiated load.
  page_node_impl->SetLoadingState(PageNode::LoadingState::kLoading);

  // The policy should allow one more PageNode to load after a PageNode finishes
  // loading.
  EXPECT_CALL(*loader(), LoadPageNode(raw_page_nodes[2]));

  // Simulate load finish of a PageNode.
  page_node_impl->SetLoadingState(PageNode::LoadingState::kLoadedIdle);
}

TEST_F(BackgroundTabLoadingPolicyTest, OnMemoryPressure) {
  // Multiple PageNodes are necessary to make sure that the policy
  // doesn't immediately kick off loading of all tabs.
  std::vector<
      performance_manager::TestNodeWrapper<performance_manager::PageNodeImpl>>
      page_nodes;
  std::vector<PageNode*> raw_page_nodes;

  for (uint32_t i = 0; i < 2; i++) {
    page_nodes.push_back(CreateNode<performance_manager::PageNodeImpl>());
    raw_page_nodes.push_back(page_nodes.back().get());

    // Set |is_tab| property as this is a requirement to pass the PageNode to
    // ScheduleLoadForRestoredTabs().
    TabPropertiesDecorator::SetIsTabForTesting(raw_page_nodes.back(), true);
  }

  // Use 1 loading slot so only one PageNode loads at a time.
  policy()->SetMaxSimultaneousLoadsForTesting(1);

  // Test that the score produces the expected loading order
  EXPECT_CALL(*loader(), LoadPageNode(raw_page_nodes[0]));

  policy()->ScheduleLoadForRestoredTabs(raw_page_nodes);
  task_env().RunUntilIdle();
  testing::Mock::VerifyAndClear(loader());

  // Simulate memory pressure and expect the tab loader to disable loading.
  system_node()->OnMemoryPressureForTesting(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE);

  PageNodeImpl* page_node_impl = page_nodes[0].get();

  // Simulate load start of a PageNode that initiated load.
  page_node_impl->SetLoadingState(PageNode::LoadingState::kLoading);

  // Simulate load finish of a PageNode and expect the policy to not start
  // another load.
  page_node_impl->SetLoadingState(PageNode::LoadingState::kLoadedIdle);
}

}  // namespace policies

}  // namespace performance_manager
