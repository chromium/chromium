// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/productivity_launcher_search_view.h"

#include <utility>

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/app_list_model_provider.h"
#include "ash/app_list/model/app_list_test_model.h"
#include "ash/app_list/model/search/test_search_result.h"
#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/app_list/views/app_list_bubble_search_page.h"
#include "ash/app_list/views/search_box_view.h"
#include "ash/app_list/views/search_result_list_view.h"
#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/test/ax_event_counter.h"

namespace ash {

namespace {

int kDefaultSearchItems = 3;
const int kResultContainersCount =
    static_cast<int>(SearchResultListView::SearchResultListType::kMaxValue);

class ProductivityLauncherSearchViewTest : public AshTestBase {
 public:
  ProductivityLauncherSearchViewTest()
      : AshTestBase((base::test::TaskEnvironment::TimeSource::MOCK_TIME)) {
    scoped_feature_list_.InitAndEnableFeature(features::kProductivityLauncher);
  }
  ~ProductivityLauncherSearchViewTest() override = default;

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
      result->set_best_match(true);
      results->Add(std::move(result));
    }
    // Adding results will schedule Update().
    base::RunLoop().RunUntilIdle();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ProductivityLauncherSearchViewTest, ResultContainerIsVisible) {
  auto* test_helper = GetAppListTestHelper();
  test_helper->ShowAppList();

  // Press a key to start a search.
  PressAndReleaseKey(ui::VKEY_A);

  // Check result container visibility.
  std::vector<SearchResultContainerView*> result_containers =
      test_helper->GetProductivityLauncherSearchView()
          ->result_container_views_for_test();
  ASSERT_EQ(static_cast<int>(result_containers.size()), kResultContainersCount);
  EXPECT_TRUE(result_containers[0]->GetVisible());
}

TEST_F(ProductivityLauncherSearchViewTest, SearchResultA11y) {
  auto* test_helper = GetAppListTestHelper();
  test_helper->ShowAppList();

  // Press a key to start a search.
  PressAndReleaseKey(ui::VKEY_A);

  SearchModel::SearchResults* results = test_helper->GetSearchResults();

  // Create |kDefaultSearchItems| new search results for us to cycle through.
  SetUpSearchResults(results, 1, kDefaultSearchItems);
  test_helper->GetProductivityLauncherSearchView()
      ->OnSearchResultContainerResultsChanged();

  // Check result container visibility.
  std::vector<SearchResultContainerView*> result_containers =
      test_helper->GetProductivityLauncherSearchView()
          ->result_container_views_for_test();
  ASSERT_EQ(static_cast<int>(result_containers.size()), kResultContainersCount);
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
  PressAndReleaseKey(ui::VKEY_UP);
  EXPECT_EQ(3, ax_counter.GetCount(ax::mojom::Event::kSelection));
  PressAndReleaseKey(ui::VKEY_DOWN);
  EXPECT_EQ(4, ax_counter.GetCount(ax::mojom::Event::kSelection));
  PressAndReleaseKey(ui::VKEY_DOWN);
  EXPECT_EQ(5, ax_counter.GetCount(ax::mojom::Event::kSelection));
}

TEST_F(ProductivityLauncherSearchViewTest, SearchPageA11y) {
  auto* test_helper = GetAppListTestHelper();
  test_helper->ShowAppList();

  // Press a key to start a search.
  PressAndReleaseKey(ui::VKEY_A);

  SearchModel::SearchResults* results = test_helper->GetSearchResults();

  // Delete all results and verify the bubble search page's A11yNodeData.

  AppListModelProvider::Get()->search_model()->DeleteAllResults();
  auto* search_view = test_helper->GetProductivityLauncherSearchView();
  search_view->OnSearchResultContainerResultsChanged();

  // Check result container visibility.
  std::vector<SearchResultContainerView*> result_containers =
      search_view->result_container_views_for_test();
  ASSERT_EQ(static_cast<int>(result_containers.size()), kResultContainersCount);
  EXPECT_TRUE(result_containers[0]->GetVisible());
  EXPECT_TRUE(search_view->GetVisible());

  ui::AXNodeData data;
  search_view->GetAccessibleNodeData(&data);
  EXPECT_EQ("Displaying 0 results for a",
            data.GetStringAttribute(ax::mojom::StringAttribute::kValue));
  // Create a single search result and and verify A11yNodeData.
  SetUpSearchResults(results, 1, 1);
  search_view->OnSearchResultContainerResultsChanged();
  search_view->GetAccessibleNodeData(&data);
  EXPECT_EQ("Displaying 1 result for a",
            data.GetStringAttribute(ax::mojom::StringAttribute::kValue));

  // Create new search results and and and verify A11yNodeData.
  SetUpSearchResults(results, 2, kDefaultSearchItems - 1);
  search_view->OnSearchResultContainerResultsChanged();
  ui::AXNodeData data2;
  search_view->GetAccessibleNodeData(&data);
  EXPECT_EQ("Displaying 3 results for a",
            data.GetStringAttribute(ax::mojom::StringAttribute::kValue));
}

TEST_F(ProductivityLauncherSearchViewTest, SearchClearedOnModelUpdate) {
  auto* test_helper = GetAppListTestHelper();
  test_helper->ShowAppList();

  // Press a key to start a search.
  PressAndReleaseKey(ui::VKEY_A);

  SearchModel::SearchResults* results = test_helper->GetSearchResults();
  // Create |kDefaultSearchItems| new search results for us to cycle through.
  SetUpSearchResults(results, 1, kDefaultSearchItems);
  test_helper->GetProductivityLauncherSearchView()
      ->OnSearchResultContainerResultsChanged();

  // Check result container visibility.
  std::vector<SearchResultContainerView*> result_containers =
      test_helper->GetProductivityLauncherSearchView()
          ->result_container_views_for_test();
  ASSERT_EQ(static_cast<int>(result_containers.size()), kResultContainersCount);
  EXPECT_TRUE(result_containers[0]->GetVisible());

  // Update the app list and search model, and verify the results page gets
  // hidden.
  auto app_list_model_override = std::make_unique<test::AppListTestModel>();
  auto search_model_override = std::make_unique<SearchModel>();
  Shell::Get()->app_list_controller()->SetActiveModel(
      /*profile_id=*/1, app_list_model_override.get(),
      search_model_override.get());

  EXPECT_FALSE(test_helper->GetBubbleSearchPage()->GetVisible());
  EXPECT_EQ(u"",
            test_helper->GetBubbleSearchBoxView()->search_box()->GetText());

  // Press a key to start a search.
  PressAndReleaseKey(ui::VKEY_A);
  SetUpSearchResults(search_model_override->results(), 2, 1);
  test_helper->GetProductivityLauncherSearchView()
      ->OnSearchResultContainerResultsChanged();

  result_containers = test_helper->GetProductivityLauncherSearchView()
                          ->result_container_views_for_test();
  ASSERT_EQ(static_cast<int>(result_containers.size()), kResultContainersCount);
  EXPECT_TRUE(result_containers[0]->GetVisible());
  EXPECT_EQ(1, result_containers[0]->num_results());
  EXPECT_EQ(u"Result 2",
            result_containers[0]->GetResultViewAt(0)->result()->title());

  Shell::Get()->app_list_controller()->ClearActiveModel();
}

}  // namespace
}  // namespace ash
