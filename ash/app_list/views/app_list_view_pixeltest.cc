// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/model/search/test_search_result.h"
#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/app_list/views/app_list_bubble_apps_page.h"
#include "ash/app_list/views/app_list_bubble_view.h"
#include "ash/app_list/views/apps_container_view.h"
#include "ash/app_list/views/apps_grid_view_test_api.h"
#include "ash/app_list/views/productivity_launcher_search_view.h"
#include "ash/app_list/views/search_box_view.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_navigation_widget.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/pixel/ash_pixel_differ.h"
#include "ash/test/pixel/ash_pixel_test_init_params.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/textfield/textfield_test_api.h"

namespace ash {

class AppListViewPixelRTLTest
    : public AshTestBase,
      public testing::WithParamInterface<bool /*is_rtl=*/> {
 public:
  // AshTestBase:
  absl::optional<pixel_test::InitParams> CreatePixelTestInitParams()
      const override {
    pixel_test::InitParams init_params;
    init_params.under_rtl = GetParam();
    return init_params;
  }

  void ShowAppList() {
    AppListTestHelper* test_helper = GetAppListTestHelper();
    test_helper->ShowAppList();

    // Use a fixed placeholder text instead of the one picked randomly to
    // avoid the test flakiness.
    test_helper->GetSearchBoxView()->UseFixedPlaceholderTextForTest();
  }

  // Hide the search box cursor to avoid the flakiness due to the
  // blinking.
  void HideCursor() {
    views::TextfieldTestApi(
        GetAppListTestHelper()->GetBubbleSearchBoxView()->search_box())
        .SetCursorLayerOpacity(0.f);
  }

  void SetUpAnswerCardResult(SearchModel::SearchResults* results,
                             int init_id,
                             int new_result_count) {
    std::unique_ptr<TestSearchResult> result =
        std::make_unique<TestSearchResult>();
    result->set_result_id(base::NumberToString(init_id));
    result->set_display_type(ash::SearchResultDisplayType::kAnswerCard);
    result->SetTitle(u"Answer Card Title");
    result->set_display_score(1000);
    result->SetDetails(u"Answer Card Details");
    result->set_best_match(false);
    results->Add(std::move(result));

    // Adding results will schedule Update().
    base::RunLoop().RunUntilIdle();
  }

  void SetUpURLResult(SearchModel::SearchResults* results,
                      int init_id,
                      int new_result_count) {
    auto result = std::make_unique<TestSearchResult>();
    result->set_result_id(base::NumberToString(init_id));
    result->set_display_type(ash::SearchResultDisplayType::kList);

    std::vector<SearchResult::TextItem> title_text_vector;
    SearchResult::TextItem title_text_item_1(
        ash::SearchResultTextItemType::kString);
    title_text_item_1.SetText(u"youtube");
    title_text_item_1.SetTextTags({SearchResult::Tag(
        SearchResult::Tag::NONE, 0, result->details().length())});
    title_text_vector.push_back(title_text_item_1);
    result->SetTitleTextVector(title_text_vector);

    std::vector<SearchResult::TextItem> details_text_vector;
    SearchResult::TextItem details_text_item_1(
        ash::SearchResultTextItemType::kString);
    details_text_item_1.SetText(u"youtube.com");
    details_text_item_1.SetTextTags({SearchResult::Tag(
        SearchResult::Tag::URL, 0, result->details().length())});
    details_text_vector.push_back(details_text_item_1);
    result->SetDetailsTextVector(details_text_vector);

    result->SetAccessibleName(u"Accessible Name");
    result->set_result_id("Test Search Result");
    result->set_best_match(true);
    results->Add(std::move(result));

    // Adding results will schedule Update().
    base::RunLoop().RunUntilIdle();
  }
};

INSTANTIATE_TEST_SUITE_P(RTL, AppListViewPixelRTLTest, testing::Bool());

// Verifies Answer Card search results under the clamshell mode.
TEST_P(AppListViewPixelRTLTest, AnswerCardSearchResult) {
  ShowAppList();

  // Press a key to start a search.
  PressAndReleaseKey(ui::VKEY_A);
  // Populate answer card result.
  auto* test_helper = GetAppListTestHelper();
  SearchModel::SearchResults* results = test_helper->GetSearchResults();
  SetUpAnswerCardResult(results, /*init_id=*/1, /*new_result_count=*/1);
  test_helper->GetProductivityLauncherSearchView()
      ->OnSearchResultContainerResultsChanged();
  // OnSearchResultContainerResultsChanged will schedule show animations().
  base::RunLoop().RunUntilIdle();

  HideCursor();
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "bubble_launcher_answer_card_search_results.rev_0",
      GetAppListTestHelper()->GetBubbleView(),
      GetPrimaryShelf()->navigation_widget()));
}

// Verifies URL results under the clamshell mode.
TEST_P(AppListViewPixelRTLTest, URLSearchResult) {
  ShowAppList();

  // Press a key to start a search.
  PressAndReleaseKey(ui::VKEY_Y);
  // Populate answer card result.
  auto* test_helper = GetAppListTestHelper();
  SearchModel::SearchResults* results = test_helper->GetSearchResults();
  SetUpURLResult(results, /*init_id=*/1, /*new_result_count=*/1);
  test_helper->GetProductivityLauncherSearchView()
      ->OnSearchResultContainerResultsChanged();
  // OnSearchResultContainerResultsChanged will schedule show animations().
  base::RunLoop().RunUntilIdle();

  HideCursor();
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "bubble_launcher_url_search_results.rev_0",
      GetAppListTestHelper()->GetBubbleView(),
      GetPrimaryShelf()->navigation_widget()));
}

// Verifies the app list view under the clamshell mode.
TEST_P(AppListViewPixelRTLTest, Basics) {
  GetAppListTestHelper()->AddAppItemsWithColorAndName(
      /*num_apps=*/2, AppListTestHelper::IconColorType::kAlternativeColor,
      /*set_name=*/true);
  ShowAppList();
  HideCursor();
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "bubble_launcher_basics.rev_0", GetAppListTestHelper()->GetBubbleView(),
      GetPrimaryShelf()->navigation_widget()));
}

// Verifies that the app list gradient zones work as expected.
TEST_P(AppListViewPixelRTLTest, GradientZone) {
  GetAppListTestHelper()->AddAppItemsWithColorAndName(
      /*num_apps=*/22, AppListTestHelper::IconColorType::kAlternativeColor,
      /*set_name=*/true);
  ShowAppList();
  HideCursor();
  views::ScrollView* scroll_view =
      GetAppListTestHelper()->GetBubbleAppsPage()->scroll_view();

  // Scroll the bubble app list so that some app list icons are beneath the
  // gradient zones.
  scroll_view->ScrollToPosition(scroll_view->vertical_scroll_bar(),
                                /*position=*/20);

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "bubble_launcher_gradient_zone.rev_0",
      GetAppListTestHelper()->GetBubbleView(),
      GetPrimaryShelf()->navigation_widget()));
}

class AppListViewTabletPixelTest
    : public AshTestBase,
      public testing::WithParamInterface</*tablet_mode=*/bool> {
 public:
  // AshTestBase:
  absl::optional<pixel_test::InitParams> CreatePixelTestInitParams()
      const override {
    pixel_test::InitParams init_params;
    init_params.under_rtl = GetParam();
    return init_params;
  }

  void SetUp() override {
    AshTestBase::SetUp();

    Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);

    AppListTestHelper* test_helper = GetAppListTestHelper();
    test_helper->GetSearchBoxView()->UseFixedPlaceholderTextForTest();
    test_helper->AddAppItemsWithColorAndName(
        /*num_apps=*/32, AppListTestHelper::IconColorType::kAlternativeColor,
        /*set_name=*/true);
  }
};

INSTANTIATE_TEST_SUITE_P(RTL, AppListViewTabletPixelTest, testing::Bool());

// Verifies the default layout for tablet mode launcher.
TEST_P(AppListViewTabletPixelTest, Basic) {
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "tablet_launcher_basics.rev_0",
      GetAppListTestHelper()->GetAppsContainerView()));
}

// Verifies that the top gradient zone of the tablet mode launcher works
// correctly.
TEST_P(AppListViewTabletPixelTest, TopGradientZone) {
  test::AppsGridViewTestApi test_api(
      GetAppListTestHelper()->GetRootPagedAppsGridView());

  // Drag the first launcher page upwards so that some apps are within the
  // top gradient zone.
  const gfx::Point start_page_drag = test_api.GetViewAtIndex(GridIndex(0, 0))
                                         ->GetIconBoundsInScreen()
                                         .bottom_left();
  auto* generator = GetEventGenerator();
  generator->set_current_screen_location(start_page_drag);
  generator->PressTouch();
  generator->MoveTouchBy(0, -40);

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "tablet_launcher_top_gradient_zone.rev_0",
      GetAppListTestHelper()->GetAppsContainerView()));
}

// Verifies that the bottom gradient zone of the tablet mode launcher works
// correctly.
TEST_P(AppListViewTabletPixelTest, BottomGradientZone) {
  test::AppsGridViewTestApi test_api(
      GetAppListTestHelper()->GetRootPagedAppsGridView());

  // Drag the first launcher page upwards so that some apps are within the
  // bottom gradient zone.
  const gfx::Point start_page_drag = test_api.GetViewAtIndex(GridIndex(0, 0))
                                         ->GetIconBoundsInScreen()
                                         .bottom_left();
  auto* generator = GetEventGenerator();
  generator->set_current_screen_location(start_page_drag);
  generator->PressTouch();
  generator->MoveTouchBy(0, -90);

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "tablet_launcher_bottom_gradient_zone.rev_0",
      GetAppListTestHelper()->GetAppsContainerView()));
}

}  // namespace ash
