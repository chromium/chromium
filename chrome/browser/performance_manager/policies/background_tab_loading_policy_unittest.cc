// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/background_tab_loading_policy.h"

#include <map>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory_coordinator/test_memory_consumer_registry.h"
#include "base/memory_coordinator/utils.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/to_string.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/performance_manager/mechanisms/page_loader.h"
#include "chrome/browser/performance_manager/policies/policy_features.h"
#include "components/performance_manager/graph/graph_impl.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/persistence/site_data/site_data_reader.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "components/performance_manager/test_support/persistence/test_site_data_reader.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"
#include "url/gurl.h"

namespace performance_manager {

namespace policies {

using PageNodeData = BackgroundTabLoadingPolicy::PageNodeData;

namespace {

using ::testing::_;

// Mock version of a performance_manager::mechanism::PageLoader.
class LenientMockPageLoader
    : public performance_manager::mechanism::PageLoader {
 public:
  LenientMockPageLoader() = default;
  ~LenientMockPageLoader() override = default;
  LenientMockPageLoader(const LenientMockPageLoader& other) = delete;
  LenientMockPageLoader& operator=(const LenientMockPageLoader&) = delete;

  MOCK_METHOD(void, LoadPageNode, (const PageNode* page_node));

  std::vector<const PageNode*> GetPageNodesToLoad(
      const PageNode* page_node) override {
    std::vector<const PageNode*> to_load = split_nodes_map_[page_node];
    if (to_load.empty()) {
      return {page_node};
    } else {
      return to_load;
    }
  }

  void AddTabSplit(std::vector<const PageNode*> split_nodes) {
    // Tab splits are symmetrical: return all pages in `split_nodes` when
    // looking up any of them.
    for (const PageNode* page_node : split_nodes) {
      split_nodes_map_[page_node] = split_nodes;
    }
  }

 private:
  std::map<const PageNode*, std::vector<const PageNode*>> split_nodes_map_;
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

  std::map<const PageNode*, raw_ptr<SiteDataReader>> site_data_readers_;
};

}  // namespace

// Base class listed before `GraphTestHarness` so `ScopedFeatureList` is
// initialized before `BrowserTaskEnvironment` starts background threads and
// destroyed only after `~GraphTestHarness()` stops them.
class BackgroundTabLoadingPolicyFeatureListSetup {
 protected:
  explicit BackgroundTabLoadingPolicyFeatureListSetup(
      bool enable_defer_while_visible_loading = true) {
    if (enable_defer_while_visible_loading) {
      scoped_feature_list_.InitAndEnableFeature(
          features::kDeferBackgroundTabLoadsWhileVisiblePageLoads);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          features::kDeferBackgroundTabLoadsWhileVisiblePageLoads);
    }
  }
  ~BackgroundTabLoadingPolicyFeatureListSetup() = default;

  base::test::ScopedFeatureList scoped_feature_list_;
};

class BackgroundTabLoadingPolicyTest
    : public BackgroundTabLoadingPolicyFeatureListSetup,
      public GraphTestHarness {
 public:
  using Super = GraphTestHarness;

  explicit BackgroundTabLoadingPolicyTest(
      bool enable_defer_while_visible_loading = true)
      : BackgroundTabLoadingPolicyFeatureListSetup(
            enable_defer_while_visible_loading) {}
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
    TakePolicyFromGraph();
    system_node_->reset();
    Super::TearDown();
  }

  void TakePolicyFromGraph() {
    if (policy_) {
      mock_loader_ = nullptr;
      graph()->TakeFromGraph(std::exchange(policy_, nullptr));
    }
  }

  int num_all_tabs_loaded_calls() const { return num_all_tabs_loaded_calls_; }

  void AllTabsLoadedCallback() { ++num_all_tabs_loaded_calls_; }

 protected:
  struct ExpectedTabCountMetrics {
    int scheduled = 0;
    int loaded_by_policy = 0;
    int already_loading = 0;
    int loaded_externally = 0;
    int closed_before_loading = 0;
    int skipped_for_max_tabs = 0;
    int skipped_for_free_memory = 0;
    int skipped_for_tab_age = 0;
    int stopped_for_memory_pressure = 0;
  };

  // Verifies single-restore histogram samples and asserts that outcome counts
  // sum to the number of tabs scheduled.
  static void ExpectSingleRestoreMetrics(
      const base::HistogramTester& tester,
      const ExpectedTabCountMetrics& expected) {
    tester.ExpectUniqueSample("SessionRestore.BackgroundTabLoading.TabCount",
                              expected.scheduled, 1);
    tester.ExpectUniqueSample(
        "SessionRestore.BackgroundTabLoading.TabCount.LoadedByPolicy",
        expected.loaded_by_policy, 1);
    tester.ExpectUniqueSample(
        "SessionRestore.BackgroundTabLoading.TabCount.AlreadyLoading",
        expected.already_loading, 1);
    tester.ExpectUniqueSample(
        "SessionRestore.BackgroundTabLoading.TabCount.LoadedExternally",
        expected.loaded_externally, 1);
    tester.ExpectUniqueSample(
        "SessionRestore.BackgroundTabLoading.TabCount.ClosedBeforeLoading",
        expected.closed_before_loading, 1);
    tester.ExpectUniqueSample(
        "SessionRestore.BackgroundTabLoading.TabCount.SkippedForMaxTabs",
        expected.skipped_for_max_tabs, 1);
    tester.ExpectUniqueSample(
        "SessionRestore.BackgroundTabLoading.TabCount.SkippedForFreeMemory",
        expected.skipped_for_free_memory, 1);
    tester.ExpectUniqueSample(
        "SessionRestore.BackgroundTabLoading.TabCount.SkippedForTabAge",
        expected.skipped_for_tab_age, 1);
    tester.ExpectUniqueSample(
        "SessionRestore.BackgroundTabLoading.TabCount.StoppedForMemoryPressure",
        expected.stopped_for_memory_pressure, 1);

    EXPECT_EQ(
        expected.scheduled,
        tester.GetTotalSum("SessionRestore.BackgroundTabLoading.TabCount"));
    EXPECT_EQ(expected.scheduled,
              tester.GetTotalSumForPrefix(
                  "SessionRestore.BackgroundTabLoading.TabCount."));
  }

  base::TestMemoryConsumerRegistry test_memory_consumer_registry_;
  using LoadDecision = BackgroundTabLoadingPolicy::LoadDecision;
  using PageNodeToLoadData = BackgroundTabLoadingPolicy::PageNodeToLoadData;

  PageNodeToLoadData CreatePageNodeToLoadData(
      const PageNode* page_node,
      bool updates_title_or_favicon_in_bg = false) {
    PageNodeToLoadData data(page_node);
    data.updates_title_or_favicon_in_bg = updates_title_or_favicon_in_bg;
    return data;
  }

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
       ScheduleLoadForRestoredTabsWithNotificationPermission) {
  std::vector<
      performance_manager::TestNodeWrapper<performance_manager::PageNodeImpl>>
      page_nodes;
  std::vector<PageNodeData> to_load;

  // Create vector of PageNodes to restore, each with a different notification
  // permission
  for (int i = 0; i < 3; i++) {
    page_nodes.push_back(CreateNode<performance_manager::PageNodeImpl>());
    to_load.emplace_back(page_nodes.back().get()->GetWeakPtr());

    // Mark the PageNode as a tab as this is a requirement to pass it to
    // ScheduleLoadForRestoredTabs().
    page_nodes.back()->SetType(PageType::kTab);
  }

  to_load[0].notification_permission_status =
      blink::mojom::PermissionStatus::ASK;
  to_load[1].notification_permission_status =
      blink::mojom::PermissionStatus::GRANTED;
  to_load[2].notification_permission_status =
      blink::mojom::PermissionStatus::DENIED;

  // Load one page at a time. After each page starts loading, update its state
  // so its loading slot opens up.
  policy()->SetMaxSimultaneousLoadsForTesting(1);
  auto update_loading_state = [](const PageNode* page_node) {
    auto* page_node_impl = PageNodeImpl::FromNode(page_node);
    page_node_impl->SetLoadingState(PageNode::LoadingState::kLoading);
    page_node_impl->SetLoadingState(PageNode::LoadingState::kLoadedIdle);
  };

  // The page with notification permission should be loaded first.
  ::testing::Expectation first_load =
      EXPECT_CALL(*loader(), LoadPageNode(to_load[1].page_node.get()))
          .WillOnce(update_loading_state);

  // The others may be loaded in any order.
  EXPECT_CALL(*loader(), LoadPageNode(to_load[0].page_node.get()))
      .After(first_load)
      .WillOnce(update_loading_state);
  EXPECT_CALL(*loader(), LoadPageNode(to_load[2].page_node.get()))
      .After(first_load)
      .WillOnce(update_loading_state);

  EXPECT_EQ(0, num_all_tabs_loaded_calls());
  policy()->ScheduleLoadForRestoredTabs(to_load);
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

  // The "all tabs loaded" callback should be invoked after the 3rd and 4th
  // pages finish loading.
  page_nodes[2]->SetLoadingState(PageNode::LoadingState::kLoading);
  page_nodes[2]->SetLoadingState(PageNode::LoadingState::kLoadedIdle);
  page_nodes[3]->SetLoadingState(PageNode::LoadingState::kLoading);
  page_nodes[3]->SetLoadingState(PageNode::LoadingState::kLoadedIdle);
  EXPECT_EQ(1, num_all_tabs_loaded_calls());
}

TEST_F(BackgroundTabLoadingPolicyTest, MaxTabsToRestore) {
  // Create more than kMaxTabsToLoad PageNodes to restore.
  std::vector<TestNodeWrapper<PageNodeImpl>> page_nodes;
  std::vector<PageNodeData> to_load;
  for (int i = 0; i < policy()->kMaxTabsToLoad + 2; i++) {
    page_nodes.push_back(CreateNode<PageNodeImpl>());
    to_load.emplace_back(page_nodes.back().get()->GetWeakPtr());

    // Mark the PageNode as a tab as this is a requirement to pass it to
    // ScheduleLoadForRestoredTabs().
    page_nodes.back()->SetType(PageType::kTab);
  }

  // Only kMaxTabsToLoad pages should start loading (in any order).
  absl::flat_hash_set<PageNodeImpl*> loading_pages;
  EXPECT_CALL(*loader(), LoadPageNode(_))
      .Times(policy()->kMaxTabsToLoad)
      .WillRepeatedly([&](const PageNode* page_node) {
        auto* page_node_impl = PageNodeImpl::FromNode(page_node);
        page_node_impl->SetLoadingState(PageNode::LoadingState::kLoading);
        loading_pages.insert(page_node_impl);
      });
  policy()->SetMaxSimultaneousLoadsForTesting(to_load.size());
  policy()->ScheduleLoadForRestoredTabs(to_load);
  EXPECT_EQ(loading_pages.size(), policy()->kMaxTabsToLoad);

  // Find a tab that's not scheduled to load, and load it manually. (Simulates
  // the user switching to it.) This tests that a tab that's loaded outside of
  // session restore doesn't interfere with session restore's loading count.
  PageNodeImpl* manual_page = nullptr;
  for (auto& page_node : page_nodes) {
    if (page_node->GetLoadingState() ==
        PageNode::LoadingState::kLoadingNotStarted) {
      page_node->SetLoadingState(PageNode::LoadingState::kLoading);
      manual_page = page_node.get();
      break;
    }
  }
  ASSERT_TRUE(manual_page);
  EXPECT_FALSE(loading_pages.contains(manual_page));

  // Finish loading the unscheduled page. This shouldn't affect the scheduled
  // pages.
  EXPECT_EQ(0, num_all_tabs_loaded_calls());
  manual_page->SetLoadingState(PageNode::LoadingState::kLoadedIdle);
  EXPECT_EQ(0, num_all_tabs_loaded_calls());

  // The "all tabs loaded" callback should be invoked after all scheduled pages
  // finish loading. If `manual_page` is incorrectly included, the callback will
  // be invoked before the loop finishes.
  for (PageNodeImpl* page_node : loading_pages) {
    EXPECT_EQ(0, num_all_tabs_loaded_calls());
    page_node->SetLoadingState(PageNode::LoadingState::kLoadedIdle);
  }
  EXPECT_EQ(1, num_all_tabs_loaded_calls());

  // Now that all scheduled pages and `manual_page` are loaded, there's one more
  // page in the list that wasn't started. Load it now. This should NOT trigger
  // the "all tabs loaded" callback again.
  PageNodeImpl* final_page = nullptr;
  for (auto& page_node : page_nodes) {
    if (page_node->GetLoadingState() ==
        PageNode::LoadingState::kLoadingNotStarted) {
      page_node->SetLoadingState(PageNode::LoadingState::kLoading);
      final_page = page_node.get();
      break;
    }
  }
  ASSERT_TRUE(final_page);
  EXPECT_NE(final_page, manual_page);
  EXPECT_FALSE(loading_pages.contains(final_page));

  EXPECT_EQ(1, num_all_tabs_loaded_calls());
  final_page->SetLoadingState(PageNode::LoadingState::kLoadedIdle);
  EXPECT_EQ(1, num_all_tabs_loaded_calls());
}

TEST_F(BackgroundTabLoadingPolicyTest, SplitTabsLoadedTogether) {
  // Create 4 PageNodes to restore.
  std::vector<
      performance_manager::TestNodeWrapper<performance_manager::PageNodeImpl>>
      page_nodes;
  std::vector<PageNodeData> to_load;

  for (int i = 0; i < 4; i++) {
    page_nodes.push_back(CreateNode<performance_manager::PageNodeImpl>());
    to_load.emplace_back(page_nodes.back().get()->GetWeakPtr());

    // Mark the PageNode as a tab as this is a requirement to pass it to
    // ScheduleLoadForRestoredTabs().
    page_nodes.back()->SetType(PageType::kTab);
  }

  // Mark tabs 1 and 2 as split and 2 as the having granted permission status so
  // it loads first.
  loader()->AddTabSplit({page_nodes[1].get(), page_nodes[2].get()});
  to_load[2].notification_permission_status =
      blink::mojom::PermissionStatus::GRANTED;

  EXPECT_CALL(*loader(), LoadPageNode(page_nodes[1].get()));
  EXPECT_CALL(*loader(), LoadPageNode(page_nodes[2].get()));

  // Use 2 loading slots, which means only 2 of the PageNodes should immediately
  // be scheduled to load.
  policy()->SetMaxSimultaneousLoadsForTesting(2);

  policy()->ScheduleLoadForRestoredTabs(to_load);
  task_env().RunUntilIdle();
  ::testing::Mock::VerifyAndClear(loader());
  EXPECT_EQ(0, num_all_tabs_loaded_calls());

  // After the first 2 pages are loaded, the next two will be loaded.
  EXPECT_CALL(*loader(), LoadPageNode(page_nodes[0].get()));
  EXPECT_CALL(*loader(), LoadPageNode(page_nodes[3].get()));

  page_nodes[2]->SetLoadingState(PageNode::LoadingState::kLoading);
  page_nodes[2]->SetLoadingState(PageNode::LoadingState::kLoadedIdle);
  page_nodes[1]->SetLoadingState(PageNode::LoadingState::kLoading);
  page_nodes[1]->SetLoadingState(PageNode::LoadingState::kLoadedIdle);

  ::testing::Mock::VerifyAndClear(loader());

  page_nodes[0]->SetLoadingState(PageNode::LoadingState::kLoading);
  page_nodes[0]->SetLoadingState(PageNode::LoadingState::kLoadedIdle);
  page_nodes[3]->SetLoadingState(PageNode::LoadingState::kLoading);
  page_nodes[3]->SetLoadingState(PageNode::LoadingState::kLoadedIdle);

  EXPECT_EQ(1, num_all_tabs_loaded_calls());
}

TEST_F(BackgroundTabLoadingPolicyTest, SplitTabsAllLoaded) {
  // Create 3 PageNodes to restore.
  std::vector<
      performance_manager::TestNodeWrapper<performance_manager::PageNodeImpl>>
      page_nodes;
  std::vector<PageNodeData> to_load;

  for (int i = 0; i < 3; i++) {
    page_nodes.push_back(CreateNode<performance_manager::PageNodeImpl>());
    to_load.emplace_back(page_nodes.back().get()->GetWeakPtr());

    // Mark the PageNode as a tab as this is a requirement to pass it to
    // ScheduleLoadForRestoredTabs().
    page_nodes.back()->SetType(PageType::kTab);
  }

  // Create a 4th tab, and put it in a split with tab 1.
  page_nodes.push_back(CreateNode<performance_manager::PageNodeImpl>());
  page_nodes.back()->SetType(PageType::kTab);
  loader()->AddTabSplit({page_nodes[1].get(), page_nodes.back().get()});

  // All tabs should be loaded, including the 4th, even though it wasn't
  // explicitly requested.
  for (int i = 0; i < 4; i++) {
    EXPECT_CALL(*loader(), LoadPageNode(page_nodes[i].get()));
  }

  policy()->SetMaxSimultaneousLoadsForTesting(4);
  policy()->ScheduleLoadForRestoredTabs(to_load);
  task_env().RunUntilIdle();
  ::testing::Mock::VerifyAndClear(loader());
  EXPECT_EQ(0, num_all_tabs_loaded_calls());

  // The "all tabs loaded" callback should be invoked after all the pages finish
  // loading, not just the explicitly requested ones.
  for (int i = 0; i < 3; i++) {
    page_nodes[i]->SetLoadingState(PageNode::LoadingState::kLoading);
    page_nodes[i]->SetLoadingState(PageNode::LoadingState::kLoadedIdle);
  }
  EXPECT_EQ(0, num_all_tabs_loaded_calls());
  page_nodes.back()->SetLoadingState(PageNode::LoadingState::kLoading);
  page_nodes.back()->SetLoadingState(PageNode::LoadingState::kLoadedIdle);
  EXPECT_EQ(1, num_all_tabs_loaded_calls());
}

// Regression test for https://crbug.com/444491692
TEST_F(BackgroundTabLoadingPolicyTest, SplitTabsAlreadyLoading) {
  // Create 3 PageNodes to restore.
  std::vector<
      performance_manager::TestNodeWrapper<performance_manager::PageNodeImpl>>
      page_nodes;
  std::vector<PageNodeData> to_load;

  for (int i = 0; i < 3; i++) {
    page_nodes.push_back(CreateNode<performance_manager::PageNodeImpl>());
    to_load.emplace_back(page_nodes.back().get()->GetWeakPtr());

    // Mark the PageNode as a tab as this is a requirement to pass it to
    // ScheduleLoadForRestoredTabs().
    page_nodes.back()->SetType(PageType::kTab);
  }

  // Create a 4th tab, and put it in a split with tab 1.
  page_nodes.push_back(CreateNode<performance_manager::PageNodeImpl>());
  page_nodes.back()->SetType(PageType::kTab);
  loader()->AddTabSplit({page_nodes[1].get(), page_nodes.back().get()});

  // Mark the split tab as already loading. It should NOT be loaded again by
  // session restore.
  page_nodes.back()->SetLoadingState(PageNode::LoadingState::kLoading);
  for (int i = 0; i < 3; i++) {
    EXPECT_CALL(*loader(), LoadPageNode(page_nodes[i].get()));
  }

  policy()->SetMaxSimultaneousLoadsForTesting(4);
  policy()->ScheduleLoadForRestoredTabs(to_load);
  task_env().RunUntilIdle();
  ::testing::Mock::VerifyAndClear(loader());
  EXPECT_EQ(0, num_all_tabs_loaded_calls());

  // The "all tabs loaded" callback should be invoked after all scheduled pages
  // finish loading. The split tab isn't counted because it was already loading.
  for (int i = 0; i < 3; i++) {
    page_nodes[i]->SetLoadingState(PageNode::LoadingState::kLoading);
    page_nodes[i]->SetLoadingState(PageNode::LoadingState::kLoadedIdle);
  }
  EXPECT_EQ(1, num_all_tabs_loaded_calls());
  page_nodes.back()->SetLoadingState(PageNode::LoadingState::kLoadedIdle);
  EXPECT_EQ(1, num_all_tabs_loaded_calls());
}

// Regression test for crbug.com/40742293
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
  std::vector<PageNodeToLoadData> page_node_datas;

  for (uint32_t i = 0; i < policy()->kMaxTabsToLoad + 1; i++) {
    page_nodes.push_back(CreateNode<performance_manager::PageNodeImpl>());
    page_node_datas.push_back(
        CreatePageNodeToLoadData(page_nodes.back().get()));
  }

  // Test the maximum number of tabs to load threshold.
  for (uint32_t i = 0; i < policy()->kMaxTabsToLoad; i++) {
    EXPECT_EQ(LoadDecision::kLoad, policy()->ShouldLoad(page_node_datas[i]));
    policy()->tab_loads_started_++;
  }
  EXPECT_EQ(LoadDecision::kSkipForMaxTabs,
            policy()->ShouldLoad(page_node_datas[policy()->kMaxTabsToLoad]));
}

TEST_F(BackgroundTabLoadingPolicyTest, ShouldLoad_MinTabsToRestore) {
  // Create vector of PageNodes to restore.
  std::vector<
      performance_manager::TestNodeWrapper<performance_manager::PageNodeImpl>>
      page_nodes;
  std::vector<PageNodeToLoadData> page_node_datas;

  for (uint32_t i = 0; i < policy()->kMinTabsToLoad + 1; i++) {
    page_nodes.push_back(CreateNode<performance_manager::PageNodeImpl>());
    page_node_datas.push_back(
        CreatePageNodeToLoadData(page_nodes.back().get()));
  }

  // When free memory limit is reached.
  const size_t kFreeMemoryLimit = policy()->kDesiredAmountOfFreeMemoryMb - 1;
  policy()->SetFreeMemoryForTesting(kFreeMemoryLimit);

  // Test that the minimum number of tabs to load is respected.
  for (uint32_t i = 0; i < policy()->kMinTabsToLoad; i++) {
    EXPECT_EQ(LoadDecision::kLoad, policy()->ShouldLoad(page_node_datas[i]));
    policy()->tab_loads_started_++;
  }
  EXPECT_EQ(LoadDecision::kSkipForFreeMemory,
            policy()->ShouldLoad(page_node_datas[policy()->kMinTabsToLoad]));
}

TEST_F(BackgroundTabLoadingPolicyTest, ShouldLoad_FreeMemory) {
  // Create a PageNode to restore.
  performance_manager::TestNodeWrapper<performance_manager::PageNodeImpl>
      page_node = CreateNode<performance_manager::PageNodeImpl>();
  PageNodeToLoadData page_node_data = CreatePageNodeToLoadData(page_node.get());

  // Simulate that kMinTabsToLoad have loaded.
  policy()->tab_loads_started_ = policy()->kMinTabsToLoad;

  // Test the free memory constraint.
  const size_t kFreeMemoryLimit = policy()->kDesiredAmountOfFreeMemoryMb;
  policy()->SetFreeMemoryForTesting(kFreeMemoryLimit);
  EXPECT_EQ(LoadDecision::kLoad, policy()->ShouldLoad(page_node_data));
  policy()->SetFreeMemoryForTesting(kFreeMemoryLimit - 1);
  EXPECT_EQ(LoadDecision::kSkipForFreeMemory,
            policy()->ShouldLoad(page_node_data));
  policy()->SetFreeMemoryForTesting(kFreeMemoryLimit + 1);
  EXPECT_EQ(LoadDecision::kLoad, policy()->ShouldLoad(page_node_data));
}

TEST_F(BackgroundTabLoadingPolicyTest, ShouldLoad_OldTab) {
  // Create an old tab.
  performance_manager::TestNodeWrapper<performance_manager::PageNodeImpl>
      page_node = CreateNode<performance_manager::PageNodeImpl>(
          nullptr, base::UnguessableToken(), GURL(),
          performance_manager::PagePropertyFlags{},
          base::TimeTicks::Now() -
              (base::Seconds(1) + policy()->kMaxTimeSinceLastUseToLoad));
  PageNodeToLoadData page_node_data = CreatePageNodeToLoadData(page_node.get());

  // Simulate that kMinTabsToLoad have loaded.
  policy()->tab_loads_started_ = policy()->kMinTabsToLoad;

  // Test the max time since last use threshold.
  EXPECT_EQ(LoadDecision::kSkipForTabAge, policy()->ShouldLoad(page_node_data));
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
          nullptr, base::UnguessableToken(), GURL(),
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
          nullptr, base::UnguessableToken(), GURL(),
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
      nullptr, base::UnguessableToken(), GURL(),
      performance_manager::PagePropertyFlags{},
      base::TimeTicks::Now() - base::Days(30)));
  policy()->SetSiteDataReaderForPageNode(page_nodes.back().get(),
                                         &site_data_reader_default);
  const PageNodeData old(page_nodes.back().get()->GetWeakPtr());
  to_load.push_back(old);

  // Recent
  page_nodes.push_back(CreateNode<performance_manager::PageNodeImpl>(
      nullptr, base::UnguessableToken(), GURL(),
      performance_manager::PagePropertyFlags{},
      base::TimeTicks::Now() - base::Seconds(1)));
  policy()->SetSiteDataReaderForPageNode(page_nodes.back().get(),
                                         &site_data_reader_default);
  const PageNodeData recent(page_nodes.back().get()->GetWeakPtr());
  to_load.push_back(recent);

  // Slightly older tabs which were observed updating their title or favicon or
  // playing audio in the background
  page_nodes.push_back(CreateNode<performance_manager::PageNodeImpl>(
      nullptr, base::UnguessableToken(), GURL(),
      performance_manager::PagePropertyFlags{},
      base::TimeTicks::Now() - base::Seconds(2)));
  policy()->SetSiteDataReaderForPageNode(page_nodes.back().get(),
                                         &site_data_reader_title);
  const PageNodeData title(page_nodes.back().get()->GetWeakPtr());
  to_load.push_back(title);

  page_nodes.push_back(CreateNode<performance_manager::PageNodeImpl>(
      nullptr, base::UnguessableToken(), GURL(),
      performance_manager::PagePropertyFlags{},
      base::TimeTicks::Now() - base::Seconds(3)));
  policy()->SetSiteDataReaderForPageNode(page_nodes.back().get(),
                                         &site_data_reader_favicon);
  const PageNodeData favicon(page_nodes.back().get()->GetWeakPtr());
  to_load.push_back(favicon);

  page_nodes.push_back(CreateNode<performance_manager::PageNodeImpl>(
      nullptr, base::UnguessableToken(), GURL(),
      performance_manager::PagePropertyFlags{},
      base::TimeTicks::Now() - base::Seconds(4)));
  policy()->SetSiteDataReaderForPageNode(page_nodes.back().get(),
                                         &site_data_reader_audio);
  const PageNodeData audio(page_nodes.back().get()->GetWeakPtr());
  to_load.push_back(audio);

  //  Internal page
  page_nodes.push_back(CreateNode<performance_manager::PageNodeImpl>(
      nullptr, base::UnguessableToken(), GURL(),
      performance_manager::PagePropertyFlags{},
      base::TimeTicks::Now() - base::Seconds(1)));
  policy()->SetSiteDataReaderForPageNode(page_nodes.back().get(),
                                         &site_data_reader_default);
  const PageNodeData internal(page_nodes.back().get()->GetWeakPtr(),
                              GURL("chrome://newtab"));
  to_load.push_back(internal);

  //  Page with notification permission
  page_nodes.push_back(CreateNode<performance_manager::PageNodeImpl>(
      nullptr, base::UnguessableToken(), GURL(),
      performance_manager::PagePropertyFlags{},
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

TEST_F(BackgroundTabLoadingPolicyTest, OnUpdateMemoryLimit) {
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
  test_memory_consumer_registry_.NotifyUpdateMemoryLimitAndRelease(
      base::MemoryLimit::ModeratePressureThreshold());
  task_env().RunUntilIdle();

  PageNodeImpl* page_node_impl = page_nodes[0].get();

  // Simulate load start of a PageNode that initiated load.
  page_node_impl->SetLoadingState(PageNode::LoadingState::kLoading);

  // Simulate load finish of a PageNode and expect the policy to not start
  // another load.
  page_node_impl->SetLoadingState(PageNode::LoadingState::kLoadedIdle);
}

TEST_F(BackgroundTabLoadingPolicyTest,
       ScheduleLoadForRestoredTabsWithLoadingStatusChanged) {
  testing::SimpleTestSiteDataReader site_data_reader_default(
      {.updates_favicon = false, .updates_title = false, .uses_audio = false});

  performance_manager::TestNodeWrapper<performance_manager::PageNodeImpl>
      page_node = CreateNode<performance_manager::PageNodeImpl>();
  policy()->SetSiteDataReaderForPageNode(page_node.get(),
                                         &site_data_reader_default);
  page_node->SetType(PageType::kTab);
  PageNodeToLoadData page_node_data = CreatePageNodeToLoadData(page_node.get());

  std::vector<PageNodeData> to_load;
  to_load.emplace_back(page_node.get()->GetWeakPtr());
  policy()->ScheduleLoadForRestoredTabs(to_load);
  page_node->SetLoadingState(PageNode::LoadingState::kLoading);
  page_node->SetLoadingState(PageNode::LoadingState::kLoadedIdle);
  EXPECT_FALSE(policy()->has_restored_tabs_to_load_);
}

TEST_F(BackgroundTabLoadingPolicyTest, DeferWhileVisiblePageLoading) {
  auto visible_page = CreateNode<performance_manager::PageNodeImpl>();
  visible_page->SetType(PageType::kTab);
  visible_page->SetIsVisible(true);
  visible_page->SetLoadingState(PageNode::LoadingState::kLoading);

  auto bg_page = CreateNode<performance_manager::PageNodeImpl>();
  bg_page->SetType(PageType::kTab);
  bg_page->SetIsVisible(false);

  policy()->SetMaxSimultaneousLoadsForTesting(4);

  std::vector<PageNodeData> to_load;
  to_load.emplace_back(bg_page.get()->GetWeakPtr());
  policy()->ScheduleLoadForRestoredTabs(to_load);
  task_env().RunUntilIdle();
  ::testing::Mock::VerifyAndClear(loader());
  EXPECT_EQ(0, num_all_tabs_loaded_calls());

  // Background loads remain deferred through `kLoadedBusy` until quiescence.
  visible_page->SetLoadingState(PageNode::LoadingState::kLoadedBusy);
  ::testing::Mock::VerifyAndClear(loader());
  EXPECT_EQ(0, num_all_tabs_loaded_calls());

  // Once the visible page reaches `kLoadedIdle`, the background tab load
  // starts.
  EXPECT_CALL(*loader(), LoadPageNode(bg_page.get()));
  visible_page->SetLoadingState(PageNode::LoadingState::kLoadedIdle);
  ::testing::Mock::VerifyAndClear(loader());

  bg_page->SetLoadingState(PageNode::LoadingState::kLoading);
  bg_page->SetLoadingState(PageNode::LoadingState::kLoadedBusy);
  bg_page->SetLoadingState(PageNode::LoadingState::kLoadedIdle);
  EXPECT_EQ(1, num_all_tabs_loaded_calls());
}

TEST_F(BackgroundTabLoadingPolicyTest,
       DeferWhileAnyOfMultipleVisiblePagesLoading) {
  auto visible_page_1 = CreateNode<performance_manager::PageNodeImpl>();
  visible_page_1->SetType(PageType::kTab);
  visible_page_1->SetIsVisible(true);
  visible_page_1->SetLoadingState(PageNode::LoadingState::kLoading);

  auto visible_page_2 = CreateNode<performance_manager::PageNodeImpl>();
  visible_page_2->SetType(PageType::kTab);
  visible_page_2->SetIsVisible(true);
  visible_page_2->SetLoadingState(PageNode::LoadingState::kLoading);

  auto bg_page = CreateNode<performance_manager::PageNodeImpl>();
  bg_page->SetType(PageType::kTab);
  bg_page->SetIsVisible(false);

  policy()->SetMaxSimultaneousLoadsForTesting(4);

  std::vector<PageNodeData> to_load;
  to_load.emplace_back(bg_page.get()->GetWeakPtr());
  policy()->ScheduleLoadForRestoredTabs(to_load);
  task_env().RunUntilIdle();
  ::testing::Mock::VerifyAndClear(loader());

  // Hiding the first visible page does not start background loads while a
  // second visible page is still loading.
  visible_page_1->SetIsVisible(false);
  task_env().RunUntilIdle();
  ::testing::Mock::VerifyAndClear(loader());

  EXPECT_CALL(*loader(), LoadPageNode(bg_page.get()));
  visible_page_2->SetLoadingState(PageNode::LoadingState::kLoadedBusy);
  visible_page_2->SetLoadingState(PageNode::LoadingState::kLoadedIdle);
  ::testing::Mock::VerifyAndClear(loader());
}

TEST_F(BackgroundTabLoadingPolicyTest,
       ResumeWhenLoadingVisiblePageBecomesHidden) {
  auto visible_page = CreateNode<performance_manager::PageNodeImpl>();
  visible_page->SetType(PageType::kTab);
  visible_page->SetIsVisible(true);
  visible_page->SetLoadingState(PageNode::LoadingState::kLoading);

  auto bg_page = CreateNode<performance_manager::PageNodeImpl>();
  bg_page->SetType(PageType::kTab);
  bg_page->SetIsVisible(false);

  policy()->SetMaxSimultaneousLoadsForTesting(4);

  std::vector<PageNodeData> to_load;
  to_load.emplace_back(bg_page.get()->GetWeakPtr());
  policy()->ScheduleLoadForRestoredTabs(to_load);
  task_env().RunUntilIdle();
  ::testing::Mock::VerifyAndClear(loader());

  // If the loading visible page becomes hidden, background tab loading resumes.
  EXPECT_CALL(*loader(), LoadPageNode(bg_page.get()));
  visible_page->SetIsVisible(false);
  task_env().RunUntilIdle();
  ::testing::Mock::VerifyAndClear(loader());

  bg_page->SetLoadingState(PageNode::LoadingState::kLoading);
  bg_page->SetLoadingState(PageNode::LoadingState::kLoadedBusy);
  bg_page->SetLoadingState(PageNode::LoadingState::kLoadedIdle);
  EXPECT_EQ(1, num_all_tabs_loaded_calls());
}

TEST_F(BackgroundTabLoadingPolicyTest,
       DoNotResumeDuringSynchronousVisibleTabSwitch) {
  auto visible_page = CreateNode<performance_manager::PageNodeImpl>();
  visible_page->SetType(PageType::kTab);
  visible_page->SetIsVisible(true);
  visible_page->SetLoadingState(PageNode::LoadingState::kLoading);

  auto switched_page = CreateNode<performance_manager::PageNodeImpl>();
  switched_page->SetType(PageType::kTab);
  switched_page->SetIsVisible(false);

  auto bg_page = CreateNode<performance_manager::PageNodeImpl>();
  bg_page->SetType(PageType::kTab);
  bg_page->SetIsVisible(false);

  policy()->SetMaxSimultaneousLoadsForTesting(4);

  std::vector<PageNodeData> to_load;
  to_load.emplace_back(switched_page.get()->GetWeakPtr());
  to_load.emplace_back(bg_page.get()->GetWeakPtr());
  policy()->ScheduleLoadForRestoredTabs(to_load);
  task_env().RunUntilIdle();
  ::testing::Mock::VerifyAndClear(loader());

  // Simulate BrowserView::OnActiveTabChanged() hiding the outgoing tab before
  // showing and starting load on the newly selected tab in the same task.
  visible_page->SetIsVisible(false);
  switched_page->SetIsVisible(true);
  switched_page->SetLoadingState(PageNode::LoadingState::kLoading);
  task_env().RunUntilIdle();
  ::testing::Mock::VerifyAndClear(loader());

  // Once the newly selected foreground tab finishes loading, the remaining
  // background tab starts loading even if the old hidden tab is still loading.
  EXPECT_CALL(*loader(), LoadPageNode(bg_page.get()));
  switched_page->SetLoadingState(PageNode::LoadingState::kLoadedBusy);
  switched_page->SetLoadingState(PageNode::LoadingState::kLoadedIdle);
  ::testing::Mock::VerifyAndClear(loader());
}

TEST_F(BackgroundTabLoadingPolicyTest, ResumeAfterVisibleLoadingTimeout) {
  auto visible_page = CreateNode<performance_manager::PageNodeImpl>();
  visible_page->SetType(PageType::kTab);
  visible_page->SetIsVisible(true);
  visible_page->SetLoadingState(PageNode::LoadingState::kLoading);

  auto bg_page = CreateNode<performance_manager::PageNodeImpl>();
  bg_page->SetType(PageType::kTab);
  bg_page->SetIsVisible(false);

  policy()->SetMaxSimultaneousLoadsForTesting(4);

  std::vector<PageNodeData> to_load;
  to_load.emplace_back(bg_page.get()->GetWeakPtr());
  policy()->ScheduleLoadForRestoredTabs(to_load);
  task_env().RunUntilIdle();
  ::testing::Mock::VerifyAndClear(loader());

  // Background tab loads remain deferred prior to the 15s timeout while the
  // visible page stays in `kLoading`.
  task_env().FastForwardBy(base::Seconds(14));
  ::testing::Mock::VerifyAndClear(loader());

  // Reaching the 15s timeout unblocks background tab loads even if the visible
  // page never leaves `kLoading`.
  EXPECT_CALL(*loader(), LoadPageNode(bg_page.get()));
  task_env().FastForwardBy(base::Seconds(1));
  ::testing::Mock::VerifyAndClear(loader());
}

TEST_F(BackgroundTabLoadingPolicyTest,
       VisiblePageStartsLoadingMidRestoreArmsTimeoutImmediately) {
  auto visible_page = CreateNode<performance_manager::PageNodeImpl>();
  visible_page->SetType(PageType::kTab);
  visible_page->SetIsVisible(true);

  auto bg_page1 = CreateNode<performance_manager::PageNodeImpl>();
  bg_page1->SetType(PageType::kTab);
  bg_page1->SetIsVisible(false);

  auto bg_page2 = CreateNode<performance_manager::PageNodeImpl>();
  bg_page2->SetType(PageType::kTab);
  bg_page2->SetIsVisible(false);

  auto bg_page3 = CreateNode<performance_manager::PageNodeImpl>();
  bg_page3->SetType(PageType::kTab);
  bg_page3->SetIsVisible(false);

  // Use 2 loading slots so `bg_page1` and `bg_page2` occupy both slots while
  // `bg_page3` waits in `page_nodes_to_load_`.
  policy()->SetMaxSimultaneousLoadsForTesting(2);

  EXPECT_CALL(*loader(), LoadPageNode(bg_page1.get()));
  EXPECT_CALL(*loader(), LoadPageNode(bg_page2.get()));
  std::vector<PageNodeData> to_load;
  to_load.emplace_back(bg_page1.get()->GetWeakPtr());
  to_load.emplace_back(bg_page2.get()->GetWeakPtr());
  to_load.emplace_back(bg_page3.get()->GetWeakPtr());
  policy()->ScheduleLoadForRestoredTabs(to_load);
  task_env().RunUntilIdle();
  ::testing::Mock::VerifyAndClear(loader());
  bg_page1->SetLoadingState(PageNode::LoadingState::kLoading);
  bg_page2->SetLoadingState(PageNode::LoadingState::kLoading);

  // `visible_page` enters `kLoading` mid-restore while all slots are occupied
  // (so `MaybeLoadSomeTabs()` is not called at `t = 0`). The 15s deferral timer
  // should start immediately.
  visible_page->SetLoadingState(PageNode::LoadingState::kLoading);

  // At `t = 10s`, `bg_page1` and `bg_page2` finish loading (leaving 1 free slot
  // alongside `visible_page`), but `bg_page3` remains deferred because only 10s
  // of the 15s window elapsed.
  task_env().FastForwardBy(base::Seconds(10));
  bg_page1->SetLoadingState(PageNode::LoadingState::kLoadedBusy);
  bg_page1->SetLoadingState(PageNode::LoadingState::kLoadedIdle);
  bg_page2->SetLoadingState(PageNode::LoadingState::kLoadedBusy);
  bg_page2->SetLoadingState(PageNode::LoadingState::kLoadedIdle);
  ::testing::Mock::VerifyAndClear(loader());

  // At `t = 14s`, `bg_page3` is still deferred.
  task_env().FastForwardBy(base::Seconds(4));
  ::testing::Mock::VerifyAndClear(loader());

  // At `t = 15s` from when `visible_page` started loading, the timeout fires
  // and `bg_page3` loads.
  EXPECT_CALL(*loader(), LoadPageNode(bg_page3.get()));
  task_env().FastForwardBy(base::Seconds(1));
  ::testing::Mock::VerifyAndClear(loader());
}

TEST_F(BackgroundTabLoadingPolicyTest,
       TimeoutNotRearmedWhileAnotherVisiblePageRemainsLoading) {
  auto visible_page1 = CreateNode<performance_manager::PageNodeImpl>();
  visible_page1->SetType(PageType::kTab);
  visible_page1->SetIsVisible(true);
  visible_page1->SetLoadingState(PageNode::LoadingState::kLoading);

  auto visible_page2 = CreateNode<performance_manager::PageNodeImpl>();
  visible_page2->SetType(PageType::kTab);
  visible_page2->SetIsVisible(true);
  visible_page2->SetLoadingState(PageNode::LoadingState::kLoading);

  auto bg_page1 = CreateNode<performance_manager::PageNodeImpl>();
  bg_page1->SetType(PageType::kTab);
  bg_page1->SetIsVisible(false);

  auto bg_page2 = CreateNode<performance_manager::PageNodeImpl>();
  bg_page2->SetType(PageType::kTab);
  bg_page2->SetIsVisible(false);

  // 3 slots: 2 occupied by `visible_page1` and `visible_page2`, leaving 1 free
  // slot once the 15s deferral timeout expires.
  policy()->SetMaxSimultaneousLoadsForTesting(3);

  std::vector<PageNodeData> to_load;
  to_load.emplace_back(bg_page1.get()->GetWeakPtr());
  to_load.emplace_back(bg_page2.get()->GetWeakPtr());
  policy()->ScheduleLoadForRestoredTabs(to_load);
  task_env().RunUntilIdle();
  ::testing::Mock::VerifyAndClear(loader());

  // At `t = 15s`, the contiguous visible-loading episode times out and
  // `bg_page1` fills the 3rd loading slot.
  EXPECT_CALL(*loader(), LoadPageNode(bg_page1.get()));
  task_env().FastForwardBy(base::Seconds(15));
  ::testing::Mock::VerifyAndClear(loader());
  bg_page1->SetLoadingState(PageNode::LoadingState::kLoading);

  // When `visible_page1` finishes loading while `visible_page2` remains in
  // `kLoading`, `bg_page2` loads immediately without starting a second 15s
  // deferral within the same contiguous visible-loading episode.
  EXPECT_CALL(*loader(), LoadPageNode(bg_page2.get()));
  visible_page1->SetLoadingState(PageNode::LoadingState::kLoadedBusy);
  visible_page1->SetLoadingState(PageNode::LoadingState::kLoadedIdle);
  ::testing::Mock::VerifyAndClear(loader());
}

TEST_F(BackgroundTabLoadingPolicyTest,
       TimeoutResetsAfterVisiblePageFinishesLoading) {
  auto visible_page1 = CreateNode<performance_manager::PageNodeImpl>();
  visible_page1->SetType(PageType::kTab);
  visible_page1->SetIsVisible(true);
  visible_page1->SetLoadingState(PageNode::LoadingState::kLoading);

  auto visible_page2 = CreateNode<performance_manager::PageNodeImpl>();
  visible_page2->SetType(PageType::kTab);
  visible_page2->SetIsVisible(true);

  auto bg_page1 = CreateNode<performance_manager::PageNodeImpl>();
  bg_page1->SetType(PageType::kTab);
  bg_page1->SetIsVisible(false);

  auto bg_page2 = CreateNode<performance_manager::PageNodeImpl>();
  bg_page2->SetType(PageType::kTab);
  bg_page2->SetIsVisible(false);

  auto bg_page3 = CreateNode<performance_manager::PageNodeImpl>();
  bg_page3->SetType(PageType::kTab);
  bg_page3->SetIsVisible(false);

  policy()->SetMaxSimultaneousLoadsForTesting(2);

  std::vector<PageNodeData> to_load;
  to_load.emplace_back(bg_page1.get()->GetWeakPtr());
  to_load.emplace_back(bg_page2.get()->GetWeakPtr());
  to_load.emplace_back(bg_page3.get()->GetWeakPtr());
  policy()->ScheduleLoadForRestoredTabs(to_load);
  task_env().RunUntilIdle();
  ::testing::Mock::VerifyAndClear(loader());

  // `visible_page1` finishes loading at `t = 10s` (before the 15s timeout),
  // stopping the timer and starting `bg_page1` and `bg_page2`.
  task_env().FastForwardBy(base::Seconds(10));
  EXPECT_CALL(*loader(), LoadPageNode(bg_page1.get()));
  EXPECT_CALL(*loader(), LoadPageNode(bg_page2.get()));
  visible_page1->SetLoadingState(PageNode::LoadingState::kLoadedBusy);
  visible_page1->SetLoadingState(PageNode::LoadingState::kLoadedIdle);
  ::testing::Mock::VerifyAndClear(loader());
  bg_page1->SetLoadingState(PageNode::LoadingState::kLoading);
  bg_page2->SetLoadingState(PageNode::LoadingState::kLoading);

  // `visible_page2` now starts a new visible-loading episode and gets a fresh
  // 15s deferral window when `bg_page1` and `bg_page2` finish.
  visible_page2->SetLoadingState(PageNode::LoadingState::kLoading);
  bg_page1->SetLoadingState(PageNode::LoadingState::kLoadedBusy);
  bg_page1->SetLoadingState(PageNode::LoadingState::kLoadedIdle);
  bg_page2->SetLoadingState(PageNode::LoadingState::kLoadedBusy);
  bg_page2->SetLoadingState(PageNode::LoadingState::kLoadedIdle);
  ::testing::Mock::VerifyAndClear(loader());

  task_env().FastForwardBy(base::Seconds(14));
  ::testing::Mock::VerifyAndClear(loader());

  EXPECT_CALL(*loader(), LoadPageNode(bg_page3.get()));
  task_env().FastForwardBy(base::Seconds(1));
  ::testing::Mock::VerifyAndClear(loader());
}

TEST_F(BackgroundTabLoadingPolicyTest,
       StopLoadingTabsCancelsVisibleLoadingTimeout) {
  auto visible_page = CreateNode<performance_manager::PageNodeImpl>();
  visible_page->SetType(PageType::kTab);
  visible_page->SetIsVisible(true);
  visible_page->SetLoadingState(PageNode::LoadingState::kLoading);

  auto bg_page = CreateNode<performance_manager::PageNodeImpl>();
  bg_page->SetType(PageType::kTab);
  bg_page->SetIsVisible(false);

  policy()->SetMaxSimultaneousLoadsForTesting(4);

  std::vector<PageNodeData> to_load;
  to_load.emplace_back(bg_page.get()->GetWeakPtr());
  policy()->ScheduleLoadForRestoredTabs(to_load);
  task_env().RunUntilIdle();
  ::testing::Mock::VerifyAndClear(loader());

  // Moderate memory pressure invokes `StopLoadingTabs()`, clearing
  // `page_nodes_to_load_` and stopping the deferral timer.
  test_memory_consumer_registry_.NotifyUpdateMemoryLimitAndRelease(
      base::MemoryLimit::ModeratePressureThreshold());
  task_env().RunUntilIdle();

  task_env().FastForwardBy(base::Seconds(15));
  ::testing::Mock::VerifyAndClear(loader());
}

TEST_F(BackgroundTabLoadingPolicyTest,
       RemovingLoadingVisiblePageResumesImmediately) {
  auto visible_page = CreateNode<performance_manager::PageNodeImpl>();
  visible_page->SetType(PageType::kTab);
  visible_page->SetIsVisible(true);
  visible_page->SetLoadingState(PageNode::LoadingState::kLoading);

  auto bg_page = CreateNode<performance_manager::PageNodeImpl>();
  bg_page->SetType(PageType::kTab);
  bg_page->SetIsVisible(false);

  policy()->SetMaxSimultaneousLoadsForTesting(4);

  std::vector<PageNodeData> to_load;
  to_load.emplace_back(bg_page.get()->GetWeakPtr());
  policy()->ScheduleLoadForRestoredTabs(to_load);
  task_env().RunUntilIdle();
  ::testing::Mock::VerifyAndClear(loader());

  // Closing the loading visible page removes it from the graph and immediately
  // unblocks `bg_page` without waiting for the 15s timeout.
  EXPECT_CALL(*loader(), LoadPageNode(bg_page.get()));
  visible_page.reset();
  ::testing::Mock::VerifyAndClear(loader());
}

class BackgroundTabLoadingPolicyFeatureDisabledTest
    : public BackgroundTabLoadingPolicyTest {
 public:
  BackgroundTabLoadingPolicyFeatureDisabledTest()
      : BackgroundTabLoadingPolicyTest(
            /*enable_defer_while_visible_loading=*/false) {}
};

TEST_F(BackgroundTabLoadingPolicyFeatureDisabledTest,
       DeferWhileVisiblePageLoading) {
  auto visible_page = CreateNode<performance_manager::PageNodeImpl>();
  visible_page->SetType(PageType::kTab);
  visible_page->SetIsVisible(true);
  visible_page->SetLoadingState(PageNode::LoadingState::kLoading);

  auto bg_page = CreateNode<performance_manager::PageNodeImpl>();
  bg_page->SetType(PageType::kTab);
  bg_page->SetIsVisible(false);

  policy()->SetMaxSimultaneousLoadsForTesting(4);

  EXPECT_CALL(*loader(), LoadPageNode(bg_page.get()));
  std::vector<PageNodeData> to_load;
  to_load.emplace_back(bg_page.get()->GetWeakPtr());
  policy()->ScheduleLoadForRestoredTabs(to_load);
  task_env().RunUntilIdle();
  ::testing::Mock::VerifyAndClear(loader());
}

TEST_F(BackgroundTabLoadingPolicyTest, Metrics_MaxTabsCapTakesPrecedence) {
  base::HistogramTester histogram_tester;

  // Cap at 4 loaded tabs and also set free memory below the 150 MiB threshold
  // once those 4 start loading, verifying that the max-tabs cap (checked first
  // in `ShouldLoad()`) is recorded rather than free-memory skip.
  policy()->SetMaxLoadedTabCountForTesting(4);
  policy()->SetMaxSimultaneousLoadsForTesting(6);
  policy()->SetFreeMemoryForTesting(50);

  std::vector<TestNodeWrapper<PageNodeImpl>> page_nodes;
  std::vector<PageNodeData> to_load;
  for (int i = 0; i < 6; ++i) {
    page_nodes.push_back(CreateNode<PageNodeImpl>());
    page_nodes.back()->SetType(PageType::kTab);
    to_load.emplace_back(page_nodes.back()->GetWeakPtr());
  }

  std::vector<PageNodeImpl*> loaded_pages;
  EXPECT_CALL(*loader(), LoadPageNode(_))
      .Times(4)
      .WillRepeatedly([&](const PageNode* page_node) {
        auto* impl = PageNodeImpl::FromNode(page_node);
        impl->SetLoadingState(PageNode::LoadingState::kLoading);
        loaded_pages.push_back(impl);
      });

  policy()->ScheduleLoadForRestoredTabs(to_load);
  ASSERT_EQ(4u, loaded_pages.size());

  // Metrics are only recorded once all initiated loads finish.
  histogram_tester.ExpectTotalCount(
      "SessionRestore.BackgroundTabLoading.TabCount", 0);

  for (PageNodeImpl* page : loaded_pages) {
    page->SetLoadingState(PageNode::LoadingState::kLoadedIdle);
  }

  ExpectSingleRestoreMetrics(
      histogram_tester,
      {.scheduled = 6, .loaded_by_policy = 4, .skipped_for_max_tabs = 2});
}

TEST_F(BackgroundTabLoadingPolicyTest, Metrics_FreeMemorySkip) {
  base::HistogramTester histogram_tester;

  // Below the 150 MiB threshold: the first `kMinTabsToLoad` (4) tabs still
  // load, and the remaining 3 tabs are skipped due to low free memory.
  policy()->SetMaxSimultaneousLoadsForTesting(7);
  policy()->SetFreeMemoryForTesting(149);

  std::vector<TestNodeWrapper<PageNodeImpl>> page_nodes;
  std::vector<PageNodeData> to_load;
  for (int i = 0; i < 7; ++i) {
    page_nodes.push_back(CreateNode<PageNodeImpl>());
    page_nodes.back()->SetType(PageType::kTab);
    to_load.emplace_back(page_nodes.back()->GetWeakPtr());
  }

  EXPECT_CALL(*loader(), LoadPageNode(_))
      .Times(4)
      .WillRepeatedly([](const PageNode* page_node) {
        auto* impl = PageNodeImpl::FromNode(page_node);
        impl->SetLoadingState(PageNode::LoadingState::kLoading);
        impl->SetLoadingState(PageNode::LoadingState::kLoadedIdle);
      });

  policy()->ScheduleLoadForRestoredTabs(to_load);

  ExpectSingleRestoreMetrics(
      histogram_tester,
      {.scheduled = 7, .loaded_by_policy = 4, .skipped_for_free_memory = 3});
}

TEST_F(BackgroundTabLoadingPolicyTest, Metrics_TabAgeSkip) {
  base::HistogramTester histogram_tester;

  policy()->SetMaxSimultaneousLoadsForTesting(6);
  const base::TimeTicks now = base::TimeTicks::Now();

  std::vector<TestNodeWrapper<PageNodeImpl>> page_nodes;
  std::vector<PageNodeData> to_load;
  // 4 recent tabs (`kMinTabsToLoad`) and 2 tabs older than 30 days.
  for (int i = 0; i < 4; ++i) {
    page_nodes.push_back(
        CreateNode<PageNodeImpl>(nullptr, base::UnguessableToken(), GURL(),
                                 PagePropertyFlags{}, now - base::Days(1)));
    page_nodes.back()->SetType(PageType::kTab);
    to_load.emplace_back(page_nodes.back()->GetWeakPtr());
  }
  for (int i = 0; i < 2; ++i) {
    page_nodes.push_back(
        CreateNode<PageNodeImpl>(nullptr, base::UnguessableToken(), GURL(),
                                 PagePropertyFlags{}, now - base::Days(31)));
    page_nodes.back()->SetType(PageType::kTab);
    to_load.emplace_back(page_nodes.back()->GetWeakPtr());
  }

  EXPECT_CALL(*loader(), LoadPageNode(_))
      .Times(4)
      .WillRepeatedly([](const PageNode* page_node) {
        auto* impl = PageNodeImpl::FromNode(page_node);
        impl->SetLoadingState(PageNode::LoadingState::kLoading);
        impl->SetLoadingState(PageNode::LoadingState::kLoadedIdle);
      });

  policy()->ScheduleLoadForRestoredTabs(to_load);

  ExpectSingleRestoreMetrics(
      histogram_tester,
      {.scheduled = 6, .loaded_by_policy = 4, .skipped_for_tab_age = 2});
}

TEST_F(BackgroundTabLoadingPolicyTest, Metrics_StopLoadingTabs) {
  base::HistogramTester histogram_tester;

  policy()->SetMaxSimultaneousLoadsForTesting(1);

  std::vector<TestNodeWrapper<PageNodeImpl>> page_nodes;
  std::vector<PageNodeData> to_load;
  for (int i = 0; i < 4; ++i) {
    page_nodes.push_back(CreateNode<PageNodeImpl>());
    page_nodes.back()->SetType(PageType::kTab);
    to_load.emplace_back(page_nodes.back()->GetWeakPtr());
  }

  EXPECT_CALL(*loader(), LoadPageNode(page_nodes[0].get()));
  policy()->ScheduleLoadForRestoredTabs(to_load);
  ::testing::Mock::VerifyAndClear(loader());

  // Moderate memory pressure drops the 3 queued tabs, while `page_nodes[0]` is
  // still in flight so metrics are not yet emitted.
  test_memory_consumer_registry_.NotifyUpdateMemoryLimitAndRelease(
      base::MemoryLimit::ModeratePressureThreshold());
  histogram_tester.ExpectTotalCount(
      "SessionRestore.BackgroundTabLoading.TabCount", 0);

  page_nodes[0]->SetLoadingState(PageNode::LoadingState::kLoading);
  page_nodes[0]->SetLoadingState(PageNode::LoadingState::kLoadedIdle);

  ExpectSingleRestoreMetrics(histogram_tester,
                             {.scheduled = 4,
                              .loaded_by_policy = 1,
                              .stopped_for_memory_pressure = 3});
}

TEST_F(BackgroundTabLoadingPolicyTest,
       Metrics_AlreadyLoadingActivatedAndClosedTabs) {
  base::HistogramTester histogram_tester;

  // 2 loading slots: 1 occupied by `page_nodes[0]` (already loading at schedule
  // time, e.g. active foreground tab) and 1 available for `page_nodes[1]`.
  policy()->SetMaxSimultaneousLoadsForTesting(2);

  std::vector<TestNodeWrapper<PageNodeImpl>> page_nodes;
  std::vector<PageNodeData> to_load;
  for (int i = 0; i < 4; ++i) {
    page_nodes.push_back(CreateNode<PageNodeImpl>());
    page_nodes.back()->SetType(PageType::kTab);
    to_load.emplace_back(page_nodes.back()->GetWeakPtr());
  }

  // `page_nodes[0]` starts loading before `ScheduleLoadForRestoredTabs()`.
  page_nodes[0]->SetLoadingState(PageNode::LoadingState::kLoading);

  EXPECT_CALL(*loader(), LoadPageNode(page_nodes[1].get()));
  policy()->ScheduleLoadForRestoredTabs(to_load);
  ::testing::Mock::VerifyAndClear(loader());

  // User activates `page_nodes[2]` before a loading slot opens.
  page_nodes[2]->SetLoadingState(PageNode::LoadingState::kLoading);

  // User closes `page_nodes[3]` before it starts loading.
  page_nodes[3].reset();

  // Finish policy-loaded and user-activated tabs; `page_nodes[0]` is still
  // loading so the restore wave is not done yet.
  page_nodes[1]->SetLoadingState(PageNode::LoadingState::kLoading);
  page_nodes[1]->SetLoadingState(PageNode::LoadingState::kLoadedIdle);
  page_nodes[2]->SetLoadingState(PageNode::LoadingState::kLoadedIdle);
  histogram_tester.ExpectTotalCount(
      "SessionRestore.BackgroundTabLoading.TabCount", 0);

  // Finishing `page_nodes[0]` completes the restore wave.
  page_nodes[0]->SetLoadingState(PageNode::LoadingState::kLoadedIdle);

  ExpectSingleRestoreMetrics(histogram_tester, {.scheduled = 4,
                                                .loaded_by_policy = 1,
                                                .already_loading = 1,
                                                .loaded_externally = 1,
                                                .closed_before_loading = 1});
}

TEST_F(BackgroundTabLoadingPolicyTest, Metrics_TwoScheduleCallsOverlapping) {
  base::HistogramTester histogram_tester;

  policy()->SetMaxSimultaneousLoadsForTesting(1);

  std::vector<TestNodeWrapper<PageNodeImpl>> page_nodes;
  for (int i = 0; i < 3; ++i) {
    page_nodes.push_back(CreateNode<PageNodeImpl>());
    page_nodes.back()->SetType(PageType::kTab);
  }

  // First `ScheduleLoadForRestoredTabs()` call starts loading `page_nodes[0]`.
  EXPECT_CALL(*loader(), LoadPageNode(page_nodes[0].get()));
  policy()->ScheduleLoadForRestoredTabs(
      {PageNodeData(page_nodes[0]->GetWeakPtr()),
       PageNodeData(page_nodes[1]->GetWeakPtr())});
  ::testing::Mock::VerifyAndClear(loader());
  page_nodes[0]->SetLoadingState(PageNode::LoadingState::kLoading);

  // Second `ScheduleLoadForRestoredTabs()` call arrives before the first batch
  // finishes; both calls belong to the same restore wave.
  policy()->ScheduleLoadForRestoredTabs(
      {PageNodeData(page_nodes[2]->GetWeakPtr())});
  histogram_tester.ExpectTotalCount(
      "SessionRestore.BackgroundTabLoading.TabCount", 0);

  // Close `page_nodes[1]` while queued; finishing `page_nodes[0]` then loads
  // `page_nodes[2]`.
  page_nodes[1].reset();
  EXPECT_CALL(*loader(), LoadPageNode(page_nodes[2].get()));
  page_nodes[0]->SetLoadingState(PageNode::LoadingState::kLoadedIdle);
  ::testing::Mock::VerifyAndClear(loader());

  histogram_tester.ExpectTotalCount(
      "SessionRestore.BackgroundTabLoading.TabCount", 0);

  page_nodes[2]->SetLoadingState(PageNode::LoadingState::kLoading);
  page_nodes[2]->SetLoadingState(PageNode::LoadingState::kLoadedIdle);

  ExpectSingleRestoreMetrics(
      histogram_tester,
      {.scheduled = 3, .loaded_by_policy = 2, .closed_before_loading = 1});
}

TEST_F(BackgroundTabLoadingPolicyTest, Metrics_TwoScheduleCallsSequential) {
  base::HistogramTester histogram_tester;

  policy()->SetMaxSimultaneousLoadsForTesting(2);

  std::vector<TestNodeWrapper<PageNodeImpl>> page_nodes;
  for (int i = 0; i < 3; ++i) {
    page_nodes.push_back(CreateNode<PageNodeImpl>());
    page_nodes.back()->SetType(PageType::kTab);
  }

  // First restore wave schedules 2 tabs and completes.
  EXPECT_CALL(*loader(), LoadPageNode(_))
      .Times(2)
      .WillRepeatedly([](const PageNode* page_node) {
        auto* impl = PageNodeImpl::FromNode(page_node);
        impl->SetLoadingState(PageNode::LoadingState::kLoading);
        impl->SetLoadingState(PageNode::LoadingState::kLoadedIdle);
      });
  policy()->ScheduleLoadForRestoredTabs(
      {PageNodeData(page_nodes[0]->GetWeakPtr()),
       PageNodeData(page_nodes[1]->GetWeakPtr())});
  ::testing::Mock::VerifyAndClear(loader());

  histogram_tester.ExpectTotalCount(
      "SessionRestore.BackgroundTabLoading.TabCount", 1);

  // Second restore wave starts after the first wave completed and emits a
  // second sample.
  EXPECT_CALL(*loader(), LoadPageNode(page_nodes[2].get()))
      .WillOnce([](const PageNode* page_node) {
        auto* impl = PageNodeImpl::FromNode(page_node);
        impl->SetLoadingState(PageNode::LoadingState::kLoading);
        impl->SetLoadingState(PageNode::LoadingState::kLoadedIdle);
      });
  policy()->ScheduleLoadForRestoredTabs(
      {PageNodeData(page_nodes[2]->GetWeakPtr())});
  ::testing::Mock::VerifyAndClear(loader());

  histogram_tester.ExpectTotalCount(
      "SessionRestore.BackgroundTabLoading.TabCount", 2);
  EXPECT_EQ(3, histogram_tester.GetTotalSum(
                   "SessionRestore.BackgroundTabLoading.TabCount"));
  EXPECT_EQ(3,
            histogram_tester.GetTotalSum(
                "SessionRestore.BackgroundTabLoading.TabCount.LoadedByPolicy"));
  EXPECT_EQ(3, histogram_tester.GetTotalSumForPrefix(
                   "SessionRestore.BackgroundTabLoading.TabCount."));
}

TEST_F(BackgroundTabLoadingPolicyTest, Metrics_RecordedOnGraphTeardown) {
  base::HistogramTester histogram_tester;

  policy()->SetMaxSimultaneousLoadsForTesting(1);

  std::vector<TestNodeWrapper<PageNodeImpl>> page_nodes;
  std::vector<PageNodeData> to_load;
  for (int i = 0; i < 3; ++i) {
    page_nodes.push_back(CreateNode<PageNodeImpl>());
    page_nodes.back()->SetType(PageType::kTab);
    to_load.emplace_back(page_nodes.back()->GetWeakPtr());
  }

  EXPECT_CALL(*loader(), LoadPageNode(page_nodes[0].get()));
  policy()->ScheduleLoadForRestoredTabs(to_load);
  ::testing::Mock::VerifyAndClear(loader());
  page_nodes[0]->SetLoadingState(PageNode::LoadingState::kLoading);

  // Removing the policy from the graph while a restore wave is in progress
  // records the wave once, counting still-queued tabs as closed before loading.
  TakePolicyFromGraph();

  ExpectSingleRestoreMetrics(
      histogram_tester,
      {.scheduled = 3, .loaded_by_policy = 1, .closed_before_loading = 2});
}

}  // namespace policies

}  // namespace performance_manager
