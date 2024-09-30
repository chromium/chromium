// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/background_tab_loading_policy.h"

#include <map>
#include <memory>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/performance_manager/mechanisms/page_loader.h"
#include "components/performance_manager/graph/graph_impl.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/public/persistence/site_data/site_data_reader.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "components/performance_manager/test_support/persistence/test_site_data_reader.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

namespace policies {

using PageNodeData = BackgroundTabLoadingPolicy::PageNodeData;

namespace {

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

class MockBackgroundTabLoadingPolicy : public BackgroundTabLoadingPolicy {
 public:
  explicit MockBackgroundTabLoadingPolicy(
      base::RepeatingClosure all_tabs_loaded_callback)
      : BackgroundTabLoadingPolicy(std::move(all_tabs_loaded_callback)) {}

  void SetSiteDataReaderForPageNode(const PageNode* page_node,
                                    SiteDataReader* site_data_reader) {
    site_data_readers_[page_node] = site_data_reader;
  }

 private:
  SiteDataReader* GetSiteDataReader(const PageNode* page_node) const override {
    auto it = site_data_readers_.find(page_node);
    if (it == site_data_readers_.end())
      return nullptr;
    return it->second;
  }

  std::map<const PageNode*, raw_ptr<SiteDataReader, CtnExperimental>>
      site_data_readers_;
};

}  // namespace

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
    auto policy =
        std::make_unique<MockBackgroundTabLoadingPolicy>(base::BindRepeating(
            &BackgroundTabLoadingPolicyTest::AllTabsLoadedCallback,
            base::Unretained(this)));
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

  int num_all_tabs_loaded_calls() const { return num_all_tabs_loaded_calls_; }

  void AllTabsLoadedCallback() { ++num_all_tabs_loaded_calls_; }

 protected:
  MockBackgroundTabLoadingPolicy* policy() { return policy_; }
  MockPageLoader* loader() { return mock_loader_; }
  SystemNodeImpl* system_node() { return system_node_.get()->get(); }

 private:
  std::unique_ptr<
      performance_manager::TestNodeWrapper<performance_manager::SystemNodeImpl>>
      system_node_;
  raw_ptr<MockBackgroundTabLoadingPolicy, DanglingUntriaged> policy_;
  raw_ptr<MockPageLoader, DanglingUntriaged> mock_loader_;
  int num_all_tabs_loaded_calls_ = 0;
};

TEST_F(BackgroundTabLoadingPolicyTest,
       ScheduleLoadForRestoredTabsWithoutNotificationPermission) {
  std::vector<
      performance_manager::TestNodeWrapper<performance_manager::PageNodeImpl>>
      page_nodes;
  std::vector<PageNodeData> to_load;

  // Create vector of PageNode to restore.
  for (int i = 0; i < 4; i++) {
    page_nodes.push_back(CreateNode<performance_manager::PageNodeImpl>());
    to_load.emplace_back(page_nodes.back().get()->GetWeakPtr());
    EXPECT_CALL(*loader(), LoadPageNode(to_load.back().page_node.get()));

    // Mark the PageNode as a tab as this is a requirement to pass it to
    // ScheduleLoadForRestoredTabs().
    page_nodes.back()->SetType(PageType::kTab);
  }

  EXPECT_EQ(0, num_all_tabs_loaded_calls());
  policy()->ScheduleLoadForRestoredTabs(to_load);
  EXPECT_EQ(0, num_all_tabs_loaded_calls());
  for (auto& page_node : page_nodes) {
    EXPECT_EQ(0, num_all_tabs_loaded_calls());
    page_node->SetLoadingState(PageNode::LoadingState::kLoading);
    page_node->SetLoadingState(PageNode::LoadingState::kLoadedIdle);
  }
  EXPECT_EQ(1, num_all_tabs_loaded_calls());
}

TEST_F(BackgroundTabLoadingPolicyTest,
       ScheduleLoadForRestoredTabsWithNotificationPermission) {
  std::vector<
      performance_manager::TestNodeWrapper<performance_manager::PageNodeImpl>>
      page_nodes;
  std::vector<PageNodeData> to_load;

  // Create vector of PageNode to restore.
  for (int i = 0; i < 4; i++) {
    page_nodes.push_back(CreateNode<performance_manager::PageNodeImpl>());
    to_load.emplace_back(page_nodes.back().get()->GetWeakPtr(), GURL(),
                         blink::mojom::PermissionStatus::GRANTED);
    EXPECT_CALL(*loader(), LoadPageNode(to_load.back().page_node.get()));

    // Mark the PageNode as a tab as this is a requirement to pass it to
    // ScheduleLoadForRestoredTabs().
    page_nodes.back()->SetType(PageType::kTab);
  }

  EXPECT_EQ(0, num_all_tabs_loaded_calls());
  policy()->ScheduleLoadForRestoredTabs(to_load);
  EXPECT_EQ(0, num_all_tabs_loaded_calls());
  for (auto& page_node : page_nodes) {
    EXPECT_EQ(0, num_all_tabs_loaded_calls());
    page_node->SetLoadingState(PageNode::LoadingState::kLoading);
    page_node->SetLoadingState(PageNode::LoadingState::kLoadedIdle);
  }
  EXPECT_EQ(1, num_all_tabs_loaded_calls());
}

TEST_F(BackgroundTabLoadingPolicyTest, AllLoadingSlotsUsed) {
  // Create 4 PageNode to restore.
  std::vector<
      performance_manager::TestNodeWrapper<performance_manager::PageNodeImpl>>
      page_nodes;
  std::vector<PageNodeData> to_load;

  // Create vector of PageNode to restore.
  for (int i = 0; i < 4; i++) {
    page_nodes.push_back(CreateNode<performance_manager::PageNodeImpl>());
    to_load.emplace_back(page_nodes.back().get()->GetWeakPtr());

    // Mark the PageNode as a tab as this is a requirement to pass it to
    // ScheduleLoadForRestoredTabs().
    page_nodes.back()->SetType(PageType::kTab);
  }
  EXPECT_CALL(*loader(), LoadPageNode(to_load[0].page_node.get()));
  EXPECT_CALL(*loader(), LoadPageNode(to_load[1].page_node.get()));

  // Use 2 loading slots, which means only 2 of the PageNodes should immediately
  // be scheduled to load.
  policy()->SetMaxSimultaneousLoadsForTesting(2);

  policy()->ScheduleLoadForRestoredTabs(to_load);
  task_env().RunUntilIdle();
  ::testing::Mock::VerifyAndClear(loader());
  EXPECT_EQ(0, num_all_tabs_loaded_calls());

  // The 3rd page should start loading when the 1st page finishes loading.
  page_nodes[0]->SetLoadingState(PageNode::LoadingState::kLoading);
  EXPECT_CALL(*loader(), LoadPageNode(to_load[2].page_node.get()));
  page_nodes[0]->SetLoadingState(PageNode::LoadingState::kLoadedIdle);
  ::testing::Mock::VerifyAndClear(loader());
  EXPECT_EQ(0, num_all_tabs_loaded_calls());

  // The 4th page should start loading when the 2nd page finishes loading.
  page_nodes[1]->SetLoadingState(PageNode::LoadingState::kLoading);
  EXPECT_CALL(*loader(), LoadPageNode(to_load[3].page_node.get()));
  page_nodes[1]->SetLoadingState(PageNode::LoadingState::kLoadedIdle);
  ::testing::Mock::VerifyAndClear(loader());
  EXPECT_EQ(0, num_all_tabs_loaded_calls());

  // The "all tabs loaded" callback should be loaded after the 3rd and 4th pages
  // finish loading.
  page_nodes[2]->SetLoadingState(PageNode::LoadingState::kLoading);
  page_nodes[2]->SetLoadingState(PageNode::LoadingState::kLoadedIdle);
  page_nodes[3]->SetLoadingState(PageNode::LoadingState::kLoading);
  page_nodes[3]->SetLoadingState(PageNode::LoadingState::kLoadedIdle);
  EXPECT_EQ(1, num_all_tabs_loaded_calls());
}

// Regression test for crbug.com/1166745
TEST_F(BackgroundTabLoadingPolicyTest, LoadingStateLoadedBusy) {
  // Create 1 PageNode to load.
  performance_manager::TestNodeWrapper<performance_manager::PageNodeImpl>
      page_node(CreateNode<performance_manager::PageNodeImpl>());

  std::vector<PageNodeData> to_load{
      PageNodeData{page_node.get()->GetWeakPtr()}};

  // Mark the PageNode as a tab as this is a requirement to pass it to
  // ScheduleLoadForRestoredTabs().
  page_node->SetType(PageType::kTab);

  EXPECT_CALL(*loader(), LoadPageNode(page_node.get()));
  policy()->ScheduleLoadForRestoredTabs(to_load);
  task_env().RunUntilIdle();
  ::testing::Mock::VerifyAndClear(loader());

  // Transition to kLoading, to kLoadedBusy, and then back to kLoading. This
  // should not crash.
  page_node->SetLoadingState(PageNode::LoadingState::kLoading);
  page_node->SetLoadingState(PageNode::LoadingState::kLoadedBusy);
  page_node->SetLoadingState(PageNode::LoadingState::kLoading);

  // Simulate load finish.
  page_node->SetLoadingState(PageNode::LoadingState::kLoadedIdle);
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
      nullptr, std::string(), GURL(), performance_manager::PagePropertyFlags{},
      base::TimeTicks::Now() -
          (base::Seconds(1) + policy()->kMaxTimeSinceLastUseToLoad));
  raw_page_node = page_node.get();

  // Simulate that kMinTabsToLoad have loaded.
  policy()->tab_loads_started_ = policy()->kMinTabsToLoad;

  // Test the max time since last use threshold.
  EXPECT_FALSE(policy()->ShouldLoad(raw_page_node));
}

// Regression test for https://crrev.com/c/3909768: Deleting a PageNode with the
// notification permission before it starts loading but before it is scored
// should not decrement the number of tabs scored.
TEST_F(BackgroundTabLoadingPolicyTest, RemoveTabWithNotificationPermission) {
  testing::SimpleTestSiteDataReader site_data_reader_default(
      {.updates_favicon = false, .updates_title = false, .uses_audio = false});
  std::vector<PageNodeData> to_load;

  // Tab without notification permission.
  auto page_node_without_notification_permission =
      CreateNode<performance_manager::PageNodeImpl>(
          nullptr, std::string(), GURL(),
          performance_manager::PagePropertyFlags{},
          base::TimeTicks::Now() - base::Days(1));
  policy()->SetSiteDataReaderForPageNode(
      page_node_without_notification_permission.get(),
      &site_data_reader_default);
  page_node_without_notification_permission->SetType(PageType::kTab);
  to_load.emplace_back(
      page_node_without_notification_permission.get()->GetWeakPtr());

  // Tab with notification permission.
  auto page_node_with_notification_permission =
      CreateNode<performance_manager::PageNodeImpl>(
          nullptr, std::string(), GURL(),
          performance_manager::PagePropertyFlags{},
          base::TimeTicks::Now() - base::Days(1));
  policy()->SetSiteDataReaderForPageNode(
      page_node_with_notification_permission.get(), &site_data_reader_default);
  page_node_with_notification_permission->SetType(PageType::kTab);
  to_load.emplace_back(
      page_node_with_notification_permission.get()->GetWeakPtr(), GURL(),
      blink::mojom::PermissionStatus::GRANTED);

  // Schedule load for restored tabs.
  policy()->ScheduleLoadForRestoredTabs(to_load);

  // Delete the tab with the notification permission.
  page_node_with_notification_permission.reset();

  // The tab without the notification permission should be loaded by the policy
  // (before https://crrev.com/c/3909768, the number of tabs scored would
  // overflow and DCHECKs would fail).
  EXPECT_CALL(*loader(),
              LoadPageNode(page_node_without_notification_permission.get()));
  task_env().RunUntilIdle();
  ::testing::Mock::VerifyAndClear(loader());
}

TEST_F(BackgroundTabLoadingPolicyTest, ScoreAndScheduleTabLoad) {
  // Use 1 loading slot so only one PageNode loads at a time.
  policy()->SetMaxSimultaneousLoadsForTesting(1);

  testing::SimpleTestSiteDataReader site_data_reader_favicon(
      {.updates_favicon = true, .updates_title = false, .uses_audio = false});
  testing::SimpleTestSiteDataReader site_data_reader_title(
      {.updates_favicon = false, .updates_title = true, .uses_audio = false});
  testing::SimpleTestSiteDataReader site_data_reader_audio(
      {.updates_favicon = false, .updates_title = false, .uses_audio = true});
  testing::SimpleTestSiteDataReader site_data_reader_default(
      {.updates_favicon = false, .updates_title = false, .uses_audio = false});

  // Create PageNodes with decreasing last visibility time (oldest to newest).
  std::vector<
      performance_manager::TestNodeWrapper<performance_manager::PageNodeImpl>>
      page_nodes;
  std::vector<PageNodeData> to_load;

  // Add tabs to restore:

  // Old
  page_nodes.push_back(CreateNode<performance_manager::PageNodeImpl>(
      nullptr, std::string(), GURL(), performance_manager::PagePropertyFlags{},
      base::TimeTicks::Now() - base::Days(30)));
  policy()->SetSiteDataReaderForPageNode(page_nodes.back().get(),
                                         &site_data_reader_default);
  const PageNodeData old(page_nodes.back().get()->GetWeakPtr());
  to_load.push_back(old);

  // Recent
  page_nodes.push_back(CreateNode<performance_manager::PageNodeImpl>(
      nullptr, std::string(), GURL(), performance_manager::PagePropertyFlags{},
      base::TimeTicks::Now() - base::Seconds(1)));
  policy()->SetSiteDataReaderForPageNode(page_nodes.back().get(),
                                         &site_data_reader_default);
  const PageNodeData recent(page_nodes.back().get()->GetWeakPtr());
  to_load.push_back(recent);

  // Slightly older tabs which were observed updating their title or favicon or
  // playing audio in the background
  page_nodes.push_back(CreateNode<performance_manager::PageNodeImpl>(
      nullptr, std::string(), GURL(), performance_manager::PagePropertyFlags{},
      base::TimeTicks::Now() - base::Seconds(2)));
  policy()->SetSiteDataReaderForPageNode(page_nodes.back().get(),
                                         &site_data_reader_title);
  const PageNodeData title(page_nodes.back().get()->GetWeakPtr());
  to_load.push_back(title);

  page_nodes.push_back(CreateNode<performance_manager::PageNodeImpl>(
      nullptr, std::string(), GURL(), performance_manager::PagePropertyFlags{},
      base::TimeTicks::Now() - base::Seconds(3)));
  policy()->SetSiteDataReaderForPageNode(page_nodes.back().get(),
                                         &site_data_reader_favicon);
  const PageNodeData favicon(page_nodes.back().get()->GetWeakPtr());
  to_load.push_back(favicon);

  page_nodes.push_back(CreateNode<performance_manager::PageNodeImpl>(
      nullptr, std::string(), GURL(), performance_manager::PagePropertyFlags{},
      base::TimeTicks::Now() - base::Seconds(4)));
  policy()->SetSiteDataReaderForPageNode(page_nodes.back().get(),
                                         &site_data_reader_audio);
  const PageNodeData audio(page_nodes.back().get()->GetWeakPtr());
  to_load.push_back(audio);

  //  Internal page
  page_nodes.push_back(CreateNode<performance_manager::PageNodeImpl>(
      nullptr, std::string(), GURL(), performance_manager::PagePropertyFlags{},
      base::TimeTicks::Now() - base::Seconds(1)));
  policy()->SetSiteDataReaderForPageNode(page_nodes.back().get(),
                                         &site_data_reader_default);
  const PageNodeData internal(page_nodes.back().get()->GetWeakPtr(),
                              GURL("chrome://newtab"));
  to_load.push_back(internal);

  //  Page with notification permission
  page_nodes.push_back(CreateNode<performance_manager::PageNodeImpl>(
      nullptr, std::string(), GURL(), performance_manager::PagePropertyFlags{},
      base::TimeTicks::Now() - base::Seconds(1)));
  policy()->SetSiteDataReaderForPageNode(page_nodes.back().get(),
                                         &site_data_reader_default);
  const PageNodeData notification(page_nodes.back().get()->GetWeakPtr(), GURL(),
                                  blink::mojom::PermissionStatus::GRANTED);
  to_load.push_back(notification);

  for (auto& page_node : page_nodes) {
    // Mark the PageNode as a tab as this is a requirement to pass it to
    // ScheduleLoadForRestoredTabs().
    page_node->SetType(PageType::kTab);
  }

  // Test that tabs are loaded in the expected order:

  const std::vector<PageNodeData> expected_load_order{
      notification, title, favicon, recent, audio, old, internal};

  // 1st tab starts loading when ScheduleLoadForRestoredTabs is invoked.
  EXPECT_CALL(*loader(), LoadPageNode(expected_load_order[0].page_node.get()));
  policy()->ScheduleLoadForRestoredTabs(to_load);
  task_env().RunUntilIdle();
  ::testing::Mock::VerifyAndClear(loader());

  // Other tabs start loading when the previous tab finishes loading.
  for (size_t i = 1; i < expected_load_order.size(); ++i) {
    PageNodeImpl::FromNode(expected_load_order[i - 1].page_node.get())
        ->SetLoadingState(PageNode::LoadingState::kLoading);
    EXPECT_CALL(*loader(),
                LoadPageNode(expected_load_order[i].page_node.get()));
    PageNodeImpl::FromNode(expected_load_order[i - 1].page_node.get())
        ->SetLoadingState(PageNode::LoadingState::kLoadedIdle);
    ::testing::Mock::VerifyAndClear(loader());
  }
}

TEST_F(BackgroundTabLoadingPolicyTest, OnMemoryPressure) {
  // Multiple PageNodes are necessary to make sure that the policy
  // doesn't immediately kick off loading of all tabs.
  std::vector<
      performance_manager::TestNodeWrapper<performance_manager::PageNodeImpl>>
      page_nodes;
  std::vector<PageNodeData> to_load;

  for (uint32_t i = 0; i < 2; i++) {
    page_nodes.push_back(CreateNode<performance_manager::PageNodeImpl>());
    to_load.emplace_back(page_nodes.back().get()->GetWeakPtr());

    // Mark the PageNode as a tab as this is a requirement to pass it to
    // ScheduleLoadForRestoredTabs().
    page_nodes.back()->SetType(PageType::kTab);
  }
  // Use 1 loading slot so only one PageNode loads at a time.
  policy()->SetMaxSimultaneousLoadsForTesting(1);

  // Test that the score produces the expected loading order
  EXPECT_CALL(*loader(), LoadPageNode(to_load[0].page_node.get()));

  policy()->ScheduleLoadForRestoredTabs(to_load);
  task_env().RunUntilIdle();
  ::testing::Mock::VerifyAndClear(loader());

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
