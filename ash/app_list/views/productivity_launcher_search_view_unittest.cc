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
#include "ash/app_list/views/result_selection_controller.h"
#include "ash/app_list/views/search_box_view.h"
#include "ash/app_list/views/search_result_list_view.h"
#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/views/controls/button/label_button.h"
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
                          int new_result_count,
                          int display_score,
                          bool best_match,
                          SearchResult::Category category) {
    for (int i = 0; i < new_result_count; ++i) {
      std::unique_ptr<TestSearchResult> result =
          std::make_unique<TestSearchResult>();
      result->set_display_type(ash::SearchResultDisplayType::kList);
      result->set_title(
          base::UTF8ToUTF16(base::StringPrintf("Result %d", init_id + i)));
      result->set_display_score(display_score);
      result->set_details(u"Detail");
      result->set_best_match(best_match);
      result->set_category(category);
      results->Add(std::move(result));
    }
    // Adding results will schedule Update().
    base::RunLoop().RunUntilIdle();
  }

  SearchResultListView::SearchResultListType GetListType(
      SearchResultContainerView* result_container_view) {
    return static_cast<SearchResultListView*>(result_container_view)
        ->list_type_for_test();
  }

  std::u16string GetListLabel(
      SearchResultContainerView* result_container_view) {
    return static_cast<SearchResultListView*>(result_container_view)
        ->title_label_for_test()
        ->GetText();
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

// Tests that key traversal correctly cycles between the list of results and
// search box close button.
TEST_F(ProductivityLauncherSearchViewTest, ResultSelectionCycle) {
  auto* test_helper = GetAppListTestHelper();
  test_helper->ShowAppList();
  EXPECT_FALSE(test_helper->GetProductivityLauncherSearchView()
                   ->CanSelectSearchResults());

  // Press a key to start a search.
  PressAndReleaseKey(ui::VKEY_A);
  SearchModel::SearchResults* results = test_helper->GetSearchResults();

  // Create categorized results and order categories as {kApps, kWeb}.
  std::vector<AppListSearchResultCategory>* ordered_categories =
      test_helper->GetOrderedResultCategories();
  AppListModelProvider::Get()->search_model()->DeleteAllResults();
  ordered_categories->push_back(AppListSearchResultCategory::kApps);
  ordered_categories->push_back(AppListSearchResultCategory::kWeb);
  SetUpSearchResults(results, 1, kDefaultSearchItems, 100, false,
                     SearchResult::Category::kApps);
  SetUpSearchResults(results, 1 + kDefaultSearchItems, kDefaultSearchItems, 1,
                     false, SearchResult::Category::kWeb);
  test_helper->GetProductivityLauncherSearchView()
      ->OnSearchResultContainerResultsChanged();

  // Press VKEY_DOWN and check if the first result view is selected.
  EXPECT_TRUE(test_helper->GetProductivityLauncherSearchView()
                  ->CanSelectSearchResults());
  ResultSelectionController* controller =
      test_helper->GetProductivityLauncherSearchView()
          ->result_selection_controller_for_test();

  // Traverse the first results container.
  for (int i = 0; i < kDefaultSearchItems - 1; ++i) {
    PressAndReleaseKey(ui::VKEY_DOWN);
    ASSERT_TRUE(controller->selected_result()) << i;
    EXPECT_EQ(controller->selected_location_details()->container_index, 1) << i;
    EXPECT_EQ(controller->selected_location_details()->result_index, i + 1);
  }

  // Traverse the second container.
  for (int i = 0; i < kDefaultSearchItems; ++i) {
    PressAndReleaseKey(ui::VKEY_DOWN);
    ASSERT_TRUE(controller->selected_result()) << i;
    EXPECT_EQ(controller->selected_location_details()->container_index, 2) << i;
    EXPECT_EQ(controller->selected_location_details()->result_index, i);
  }

  // Pressing down while the last result is selected moves focus to the close
  // button.
  PressAndReleaseKey(ui::VKEY_DOWN);

  EXPECT_FALSE(controller->selected_result());
  EXPECT_TRUE(
      test_helper->GetBubbleSearchBoxView()->close_button()->HasFocus());

  // Move focus the the search box, and verify result selection is properly set.
  PressAndReleaseKey(ui::VKEY_DOWN);
  EXPECT_TRUE(test_helper->GetBubbleSearchBoxView()->search_box()->HasFocus());

  ASSERT_TRUE(controller->selected_result());
  EXPECT_EQ(controller->selected_location_details()->container_index, 1);
  EXPECT_EQ(controller->selected_location_details()->result_index, 0);

  // Up key should cycle focus to the close button, and then the last search
  // result.
  PressAndReleaseKey(ui::VKEY_UP);
  EXPECT_FALSE(controller->selected_result());
  EXPECT_TRUE(
      test_helper->GetBubbleSearchBoxView()->close_button()->HasFocus());

  PressAndReleaseKey(ui::VKEY_UP);
  EXPECT_TRUE(test_helper->GetBubbleSearchBoxView()->search_box()->HasFocus());

  ASSERT_TRUE(controller->selected_result());
  EXPECT_EQ(controller->selected_location_details()->container_index, 2);
  EXPECT_EQ(controller->selected_location_details()->result_index,
            kDefaultSearchItems - 1);
}

// Tests that result selection controller can change between  within and between
// result containers.
TEST_F(ProductivityLauncherSearchViewTest, ResultSelection) {
  auto* test_helper = GetAppListTestHelper();
  test_helper->ShowAppList();
  EXPECT_FALSE(test_helper->GetProductivityLauncherSearchView()
                   ->CanSelectSearchResults());

  // Press a key to start a search.
  PressAndReleaseKey(ui::VKEY_A);
  SearchModel::SearchResults* results = test_helper->GetSearchResults();

  // Create categorized results and order categories as {kApps, kWeb}.
  std::vector<AppListSearchResultCategory>* ordered_categories =
      test_helper->GetOrderedResultCategories();
  AppListModelProvider::Get()->search_model()->DeleteAllResults();
  ordered_categories->push_back(AppListSearchResultCategory::kApps);
  ordered_categories->push_back(AppListSearchResultCategory::kWeb);
  SetUpSearchResults(results, 1, kDefaultSearchItems, 100, false,
                     SearchResult::Category::kApps);
  SetUpSearchResults(results, 1 + kDefaultSearchItems, kDefaultSearchItems, 1,
                     false, SearchResult::Category::kWeb);
  test_helper->GetProductivityLauncherSearchView()
      ->OnSearchResultContainerResultsChanged();

  // Press VKEY_DOWN and check if the first result view is selected.
  EXPECT_TRUE(test_helper->GetProductivityLauncherSearchView()
                  ->CanSelectSearchResults());
  ResultSelectionController* controller =
      test_helper->GetProductivityLauncherSearchView()
          ->result_selection_controller_for_test();
  // Tests that VKEY_DOWN selects the next result in container 1.
  PressAndReleaseKey(ui::VKEY_DOWN);

  EXPECT_EQ(controller->selected_location_details()->container_index, 1);
  EXPECT_EQ(controller->selected_location_details()->result_index, 1);
  PressAndReleaseKey(ui::VKEY_DOWN);
  EXPECT_EQ(controller->selected_location_details()->container_index, 1);
  EXPECT_EQ(controller->selected_location_details()->result_index, 2);
  // Tests that VKEY_DOWN while selecting the last result of the current
  // container causes the selection controller to select the next container.
  PressAndReleaseKey(ui::VKEY_DOWN);
  EXPECT_EQ(controller->selected_location_details()->container_index, 2);
  EXPECT_EQ(controller->selected_location_details()->result_index, 0);
  PressAndReleaseKey(ui::VKEY_DOWN);
  EXPECT_EQ(controller->selected_location_details()->container_index, 2);
  EXPECT_EQ(controller->selected_location_details()->result_index, 1);
  PressAndReleaseKey(ui::VKEY_DOWN);
  EXPECT_EQ(controller->selected_location_details()->container_index, 2);
  EXPECT_EQ(controller->selected_location_details()->result_index, 2);
  // Tests that VKEY_UP while selecting the first result of the current
  // container causes the selection controller to select the previous container.
  PressAndReleaseKey(ui::VKEY_UP);
  PressAndReleaseKey(ui::VKEY_UP);
  PressAndReleaseKey(ui::VKEY_UP);
  EXPECT_EQ(controller->selected_location_details()->container_index, 1);
  EXPECT_EQ(controller->selected_location_details()->result_index, 2);
}

// Verifies that search result categories are sorted properly.
TEST_F(ProductivityLauncherSearchViewTest, SearchResultCategoricalSort) {
  auto* test_helper = GetAppListTestHelper();
  test_helper->ShowAppList();

  // Press a key to start a search.
  PressAndReleaseKey(ui::VKEY_A);

  SearchModel::SearchResults* results = test_helper->GetSearchResults();

  std::vector<SearchResultContainerView*> result_containers =
      test_helper->GetProductivityLauncherSearchView()
          ->result_container_views_for_test();
  ASSERT_EQ(static_cast<int>(result_containers.size()), kResultContainersCount);

  // Create categorized results and order categories as {kApps, kWeb}.
  std::vector<ash::AppListSearchResultCategory>* ordered_categories =
      test_helper->GetOrderedResultCategories();
  AppListModelProvider::Get()->search_model()->DeleteAllResults();
  ordered_categories->push_back(ash::AppListSearchResultCategory::kApps);
  ordered_categories->push_back(ash::AppListSearchResultCategory::kWeb);
  SetUpSearchResults(results, 1, kDefaultSearchItems, 100, false,
                     SearchResult::Category::kApps);
  SetUpSearchResults(results, 1 + kDefaultSearchItems, kDefaultSearchItems, 1,
                     false, SearchResult::Category::kWeb);
  test_helper->GetProductivityLauncherSearchView()
      ->OnSearchResultContainerResultsChanged();

  // Verify result container visibility.
  EXPECT_FALSE(result_containers[0]->GetVisible());
  EXPECT_TRUE(result_containers[1]->GetVisible());
  EXPECT_TRUE(result_containers[2]->GetVisible());
  EXPECT_FALSE(result_containers[3]->GetVisible());

  // Verify title labels are correctly updated.
  EXPECT_EQ(GetListLabel(result_containers[1]), u"Apps");
  EXPECT_EQ(GetListLabel(result_containers[2]), u"Websites");

  // Verify result container ordering.
  EXPECT_EQ(GetListType(result_containers[1]),
            SearchResultListView::SearchResultListType::kApps);
  EXPECT_EQ(GetListType(result_containers[2]),
            SearchResultListView::SearchResultListType::kWeb);

  // Create categorized results and order categories as {kWeb, kApps}.
  AppListModelProvider::Get()->search_model()->DeleteAllResults();
  ordered_categories->push_back(ash::AppListSearchResultCategory::kWeb);
  ordered_categories->push_back(ash::AppListSearchResultCategory::kApps);
  SetUpSearchResults(results, 1, kDefaultSearchItems, 1, false,
                     SearchResult::Category::kApps);
  SetUpSearchResults(results, 1 + kDefaultSearchItems, kDefaultSearchItems, 100,
                     false, SearchResult::Category::kWeb);
  test_helper->GetProductivityLauncherSearchView()
      ->OnSearchResultContainerResultsChanged();

  // Verify result container visibility.
  EXPECT_FALSE(result_containers[0]->GetVisible());
  EXPECT_TRUE(result_containers[1]->GetVisible());
  EXPECT_TRUE(result_containers[2]->GetVisible());
  EXPECT_FALSE(result_containers[3]->GetVisible());

  // Verify title labels are correctly updated.

  EXPECT_EQ(GetListLabel(result_containers[1]), u"Websites");
  EXPECT_EQ(GetListLabel(result_containers[2]), u"Apps");

  // Verify result container ordering.
  result_containers = test_helper->GetProductivityLauncherSearchView()
                          ->result_container_views_for_test();

  EXPECT_EQ(GetListType(result_containers[1]),
            SearchResultListView::SearchResultListType::kWeb);
  EXPECT_EQ(GetListType(result_containers[2]),
            SearchResultListView::SearchResultListType::kApps);
}

TEST_F(ProductivityLauncherSearchViewTest, SearchResultA11y) {
  auto* test_helper = GetAppListTestHelper();
  test_helper->ShowAppList();

  // Press a key to start a search.
  PressAndReleaseKey(ui::VKEY_A);

  SearchModel::SearchResults* results = test_helper->GetSearchResults();

  // Create |kDefaultSearchItems| new search results for us to cycle through.
  SetUpSearchResults(results, 1, kDefaultSearchItems, 100, true,
                     SearchResult::Category::kApps);
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
  SetUpSearchResults(results, 1, 1, 100, true, SearchResult::Category::kApps);
  search_view->OnSearchResultContainerResultsChanged();
  search_view->GetAccessibleNodeData(&data);
  EXPECT_EQ("Displaying 1 result for a",
            data.GetStringAttribute(ax::mojom::StringAttribute::kValue));

  // Create new search results and and and verify A11yNodeData.
  SetUpSearchResults(results, 2, kDefaultSearchItems - 1, 100, true,
                     SearchResult::Category::kApps);
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
  SetUpSearchResults(results, 1, kDefaultSearchItems, 100, true,
                     SearchResult::Category::kApps);
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
  SetUpSearchResults(search_model_override->results(), 2, 1, 100, true,
                     SearchResult::Category::kApps);
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
