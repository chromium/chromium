// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/app_list_bubble_search_page.h"

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/model/search/test_search_result.h"
#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/views/test/ax_event_counter.h"

namespace ash {

namespace {

int kDefaultSearchItems = 5;

class AppListBubbleSearchPageTest : public AshTestBase {
 public:
  AppListBubbleSearchPageTest()
      : AshTestBase((base::test::TaskEnvironment::TimeSource::MOCK_TIME)) {
    scoped_feature_list_.InitAndEnableFeature(features::kProductivityLauncher);
  }
  ~AppListBubbleSearchPageTest() override = default;

  void SetUpSearchResults(SearchModel::SearchResults* results,
                          int init_id,
                          int new_result_count) {
    for (int i = 0; i < new_result_count; ++i) {
      std::unique_ptr<TestSearchResult> result =
          std::make_unique<TestSearchResult>();
      result->set_display_type(ash::SearchResultDisplayType::kList);
      result->set_title(
          base::UTF8ToUTF16(base::StringPrintf("Result %d", init_id + i)));
      result->set_display_score(100);
      result->set_details(u"Detail");
      results->Add(std::move(result));
    }
    // Adding results will schedule Update().
    base::RunLoop().RunUntilIdle();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(AppListBubbleSearchPageTest, ResultContainerIsVisible) {
  auto* test_helper = GetAppListTestHelper();
  test_helper->ShowAppList();

  // Press a key to start a search.
  PressAndReleaseKey(ui::VKEY_A);

  // The single result container is visible.
  std::vector<SearchResultContainerView*> result_containers =
      test_helper->GetBubbleSearchPage()->result_container_views_for_test();
  ASSERT_EQ(result_containers.size(), 1u);
  EXPECT_TRUE(result_containers[0]->GetVisible());
}

TEST_F(AppListBubbleSearchPageTest, SearchResultA11y) {
  auto* test_helper = GetAppListTestHelper();
  test_helper->ShowAppList();

  // Press a key to start a search.
  PressAndReleaseKey(ui::VKEY_A);

  SearchModel::SearchResults* results = test_helper->GetSearchResults();

  // Create |kDefaultSearchItems| new search results for us to cycle through.
  SetUpSearchResults(results, 1, kDefaultSearchItems);
  test_helper->GetBubbleSearchPage()->OnSearchResultContainerResultsChanged();

  // The single result container is visible.
  std::vector<SearchResultContainerView*> result_containers =
      test_helper->GetBubbleSearchPage()->result_container_views_for_test();
  ASSERT_EQ(result_containers.size(), 1u);
  EXPECT_TRUE(result_containers[0]->GetVisible());

  views::test::AXEventCounter ax_counter(views::AXEventManager::Get());

  // Pressing down should not generate a selection accessibility event because
  // A11Y announcements are delayed since the results list just changed.
  PressAndReleaseKey(ui::VKEY_DOWN);
  EXPECT_EQ(0, ax_counter.GetCount(ax::mojom::Event::kSelection));
  // Advance time to fire the timer to stop ignoring A11Y announcements.
  task_environment()->FastForwardBy(base::Milliseconds(5000));

  // A selection event is generated when the timer fires.
  EXPECT_EQ(1, ax_counter.GetCount(ax::mojom::Event::kSelection));

  // Successive up/down key presses should generate additional selection events.
  PressAndReleaseKey(ui::VKEY_DOWN);
  EXPECT_EQ(2, ax_counter.GetCount(ax::mojom::Event::kSelection));
  PressAndReleaseKey(ui::VKEY_DOWN);
  EXPECT_EQ(3, ax_counter.GetCount(ax::mojom::Event::kSelection));
  PressAndReleaseKey(ui::VKEY_DOWN);
  EXPECT_EQ(4, ax_counter.GetCount(ax::mojom::Event::kSelection));
  PressAndReleaseKey(ui::VKEY_UP);
  EXPECT_EQ(5, ax_counter.GetCount(ax::mojom::Event::kSelection));
}

TEST_F(AppListBubbleSearchPageTest, SearchPageA11y) {
  auto* test_helper = GetAppListTestHelper();
  test_helper->ShowAppList();

  // Press a key to start a search.
  PressAndReleaseKey(ui::VKEY_A);

  SearchModel::SearchResults* results = test_helper->GetSearchResults();

  // Delete all results and verify the bubble search page's A11yNodeData.
  Shell::Get()->app_list_controller()->GetSearchModel()->DeleteAllResults();
  test_helper->GetBubbleSearchPage()->OnSearchResultContainerResultsChanged();

  // The single result container is visible.
  std::vector<SearchResultContainerView*> result_containers =
      test_helper->GetBubbleSearchPage()->result_container_views_for_test();
  ASSERT_EQ(result_containers.size(), 1u);
  EXPECT_TRUE(result_containers[0]->GetVisible());
  EXPECT_TRUE(test_helper->GetBubbleSearchPage()->GetVisible());

  ui::AXNodeData data;
  test_helper->GetBubbleSearchPage()->GetAccessibleNodeData(&data);
  EXPECT_EQ("Displaying 0 results for a",
            data.GetStringAttribute(ax::mojom::StringAttribute::kValue));
  // Create a single search result and and verify A11yNodeData.
  SetUpSearchResults(results, 1, 1);
  test_helper->GetBubbleSearchPage()->OnSearchResultContainerResultsChanged();
  test_helper->GetBubbleSearchPage()->GetAccessibleNodeData(&data);
  EXPECT_EQ("Displaying 1 result for a",
            data.GetStringAttribute(ax::mojom::StringAttribute::kValue));

  // Create new search results and and and verify A11yNodeData.
  SetUpSearchResults(results, 2, kDefaultSearchItems - 1);
  test_helper->GetBubbleSearchPage()->OnSearchResultContainerResultsChanged();
  ui::AXNodeData data2;
  test_helper->GetBubbleSearchPage()->GetAccessibleNodeData(&data);
  EXPECT_EQ("Displaying 5 results for a",
            data.GetStringAttribute(ax::mojom::StringAttribute::kValue));
}

}  // namespace
}  // namespace ash
