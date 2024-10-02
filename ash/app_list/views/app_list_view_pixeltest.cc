// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/model/search/test_search_result.h"
#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/app_list/views/app_list_bubble_apps_page.h"
#include "ash/app_list/views/app_list_bubble_view.h"
#include "ash/app_list/views/app_list_search_view.h"
#include "ash/app_list/views/apps_container_view.h"
#include "ash/app_list/views/apps_grid_view_test_api.h"
#include "ash/app_list/views/search_box_view.h"
#include "ash/assistant/test/assistant_ash_test_base.h"
#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/assistant/ui/assistant_view_ids.h"
#include "ash/assistant/ui/main_stage/launcher_search_iph_view.h"
#include "ash/public/cpp/style/dark_light_mode_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_navigation_widget.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/pixel/ash_pixel_differ.h"
#include "ash/test/pixel/ash_pixel_test_init_params.h"
#include "ash/test/view_drawn_waiter.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/test/event_generator.h"
#include "ui/events/types/event_type.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/textfield/textfield_test_api.h"

namespace ash {

namespace {

using TestVariantsParam = std::tuple<bool, bool, bool>;

bool IsRtl(TestVariantsParam param) {
  return std::get<0>(param);
}

bool IsDarkMode(TestVariantsParam param) {
  return std::get<1>(param);
}

bool IsTabletMode(TestVariantsParam param) {
  return std::get<2>(param);
}

std::string GenerateTestSuffix(
    const testing::TestParamInfo<TestVariantsParam>& info) {
  std::string suffix;
  suffix.append(IsRtl(info.param) ? "rtl" : "ltr");
  suffix.append("_");
  suffix.append(IsDarkMode(info.param) ? "dark" : "light");
  suffix.append("_");
  suffix.append(IsTabletMode(info.param) ? "tablet" : "clamshell");
  return suffix;
}

void UseFixedPlaceholderTextAndHideCursor(SearchBoxView* search_box_view) {
  ASSERT_TRUE(search_box_view);

  // Use a fixed placeholder text instead of the one picked randomly to
  // avoid the test flakiness.
  search_box_view->UseFixedPlaceholderTextForTest();

  // Hide the search box cursor to avoid the flakiness due to the blinking.
  views::TextfieldTestApi(search_box_view->search_box())
      .SetCursorLayerOpacity(0.f);
}

}  // namespace

class AppListViewPixelRTLTest
    : public AshTestBase,
      public testing::WithParamInterface<std::tuple<bool /*is_rtl=*/>> {
 public:
  std::optional<pixel_test::InitParams> CreatePixelTestInitParams()
      const override {
    pixel_test::InitParams init_params;
    init_params.under_rtl = IsRtl();
    return init_params;
  }

  void ShowAppList() {
    AppListTestHelper* test_helper = GetAppListTestHelper();
    test_helper->ShowAppList();
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

  std::vector<SearchResult::TextItem> BuildKeyboardShortcutTextVector() {
    std::vector<SearchResult::TextItem> keyboard_shortcut_text_vector;
    SearchResult::TextItem shortcut_text_item_1(
        ash::SearchResultTextItemType::kIconifiedText);
    shortcut_text_item_1.SetText(u"ctrl");
    shortcut_text_item_1.SetTextTags({});
    keyboard_shortcut_text_vector.push_back(shortcut_text_item_1);

    SearchResult::TextItem shortcut_text_item_2(
        ash::SearchResultTextItemType::kString);
    shortcut_text_item_2.SetText(u" + ");
    shortcut_text_item_2.SetTextTags({});
    keyboard_shortcut_text_vector.push_back(shortcut_text_item_2);

    SearchResult::TextItem shortcut_text_item_3(
        ash::SearchResultTextItemType::kIconCode);
    shortcut_text_item_3.SetIconCode(
        SearchResultTextItem::IconCode::kKeyboardShortcutSearch);
    keyboard_shortcut_text_vector.push_back(shortcut_text_item_3);

    SearchResult::TextItem shortcut_text_item_4(
        ash::SearchResultTextItemType::kIconCode);
    shortcut_text_item_4.SetIconCode(
        SearchResultTextItem::IconCode::kKeyboardShortcutLeft);
    keyboard_shortcut_text_vector.push_back(shortcut_text_item_4);

    return keyboard_shortcut_text_vector;
  }

  void SetUpKeyboardShortcutResult(SearchModel::SearchResults* results) {
    std::unique_ptr<TestSearchResult> result =
        std::make_unique<TestSearchResult>();
    result->set_display_type(ash::SearchResultDisplayType::kList);
    result->SetAccessibleName(u"Copy and Paste");
    result->SetTitle(u"Copy and Paste");
    result->SetDetails(u"Shortcuts");
    result->set_best_match(true);
    result->SetKeyboardShortcutTextVector(BuildKeyboardShortcutTextVector());
    results->Add(std::move(result));

    // Adding results will schedule Update().
    base::RunLoop().RunUntilIdle();
  }

  bool IsRtl() const { return std::get<0>(GetParam()); }
};

INSTANTIATE_TEST_SUITE_P(RTL,
                         AppListViewPixelRTLTest,
                         testing::Combine(testing::Bool()));

// Verifies Answer Card search results under the clamshell mode.
TEST_P(AppListViewPixelRTLTest, AnswerCardSearchResult) {
  ShowAppList();

  // Press a key to start a search.
  PressAndReleaseKey(ui::VKEY_A);
  // Populate answer card result.
  auto* test_helper = GetAppListTestHelper();
  SearchModel::SearchResults* results = test_helper->GetSearchResults();
  SetUpAnswerCardResult(results, /*init_id=*/1, /*new_result_count=*/1);
  test_helper->GetBubbleAppListSearchView()
      ->OnSearchResultContainerResultsChanged();
  // OnSearchResultContainerResultsChanged will schedule show animations().
  base::RunLoop().RunUntilIdle();

  UseFixedPlaceholderTextAndHideCursor(test_helper->GetSearchBoxView());
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "bubble_launcher_answer_card_search_results", 13,
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
  test_helper->GetBubbleAppListSearchView()
      ->OnSearchResultContainerResultsChanged();
  // OnSearchResultContainerResultsChanged will schedule show animations().
  base::RunLoop().RunUntilIdle();

  UseFixedPlaceholderTextAndHideCursor(test_helper->GetSearchBoxView());
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "bubble_launcher_url_search_results", 12,
      GetAppListTestHelper()->GetBubbleView(),
      GetPrimaryShelf()->navigation_widget()));
}

// Verifies keyboard shortcut results under the clamshell mode.
TEST_P(AppListViewPixelRTLTest, KeyboardShortcutSearchResult) {
  ShowAppList();

  // Press a key to start a search.
  PressAndReleaseKey(ui::VKEY_Y);
  // Populate answer card result.
  auto* test_helper = GetAppListTestHelper();
  SearchModel::SearchResults* results = test_helper->GetSearchResults();
  SetUpKeyboardShortcutResult(results);
  test_helper->GetBubbleAppListSearchView()
      ->OnSearchResultContainerResultsChanged();
  // OnSearchResultContainerResultsChanged will schedule show animations().
  base::RunLoop().RunUntilIdle();

  UseFixedPlaceholderTextAndHideCursor(test_helper->GetSearchBoxView());
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "bubble_launcher_ks_search_results", /*revision_number=*/4,
      GetAppListTestHelper()->GetBubbleView()));
}

// Verifies the app list view under the clamshell mode.
TEST_P(AppListViewPixelRTLTest, Basics) {
  GetAppListTestHelper()->AddAppItemsWithColorAndName(
      /*num_apps=*/2, AppListTestHelper::IconColorType::kAlternativeColor,
      /*set_name=*/true);
  ShowAppList();
  UseFixedPlaceholderTextAndHideCursor(
      GetAppListTestHelper()->GetSearchBoxView());
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "bubble_launcher_basics", 12, GetAppListTestHelper()->GetBubbleView(),
      GetPrimaryShelf()->navigation_widget()));
}

// Verifies that the app list gradient zones work as expected.
TEST_P(AppListViewPixelRTLTest, GradientZone) {
  GetAppListTestHelper()->AddAppItemsWithColorAndName(
      /*num_apps=*/22, AppListTestHelper::IconColorType::kAlternativeColor,
      /*set_name=*/true);
  ShowAppList();
  UseFixedPlaceholderTextAndHideCursor(
      GetAppListTestHelper()->GetSearchBoxView());
  views::ScrollView* scroll_view =
      GetAppListTestHelper()->GetBubbleAppsPage()->scroll_view();

  // Scroll the bubble app list so that some app list icons are beneath the
  // gradient zones.
  scroll_view->ScrollToPosition(scroll_view->vertical_scroll_bar(),
                                /*position=*/20);

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "bubble_launcher_gradient_zone", 12,
      GetAppListTestHelper()->GetBubbleView(),
      GetPrimaryShelf()->navigation_widget()));
}

class AppListViewLauncherSearchIphTest
    : public AssistantAshTestBase,
      public testing::WithParamInterface<TestVariantsParam> {
 public:
  std::optional<pixel_test::InitParams> CreatePixelTestInitParams()
      const override {
    pixel_test::InitParams init_params;
    init_params.under_rtl = IsRtl(GetParam());
    return init_params;
  }

  void SetUp() override {
    AssistantAshTestBase::SetUp();

    DarkLightModeController::Get()->SetDarkModeEnabledForTest(
        IsDarkMode(GetParam()));

    Shell::Get()->tablet_mode_controller()->SetEnabledForTest(
        IsTabletMode(GetParam()));

    // Set a testing text so that the pixel test can compare.
    LauncherSearchIphView::SetChipTextForTesting(u"chip");

    GetAppListTestHelper()->search_model()->SetWouldTriggerLauncherSearchIph(
        true);
  }
};

INSTANTIATE_TEST_SUITE_P(RTL,
                         AppListViewLauncherSearchIphTest,
                         testing::Combine(testing::Bool(),
                                          testing::Bool(),
                                          testing::Bool()),
                         &GenerateTestSuffix);

TEST_P(AppListViewLauncherSearchIphTest, Basic) {
  GetAppListTestHelper()->ShowAppList();
  raw_ptr<SearchBoxView> search_box_view =
      GetAppListTestHelper()->GetSearchBoxView();
  ASSERT_TRUE(search_box_view);
  LeftClickOn(search_box_view->assistant_button());

  auto* const iph_view = search_box_view->GetIphView();
  ASSERT_TRUE(iph_view);
  ViewDrawnWaiter().Wait(iph_view);

  UseFixedPlaceholderTextAndHideCursor(search_box_view);
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "launcher_search_iph", /*revision_number=*/4, search_box_view));
}

class AppListViewTabletPixelTest
    : public AshTestBase,
      public testing::WithParamInterface<std::tuple</*rtl=*/bool>> {
 public:
  // AshTestBase:
  std::optional<pixel_test::InitParams> CreatePixelTestInitParams()
      const override {
    pixel_test::InitParams init_params;
    init_params.under_rtl = IsRtl();
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

 protected:
  bool IsRtl() const { return std::get<0>(GetParam()); }
};

INSTANTIATE_TEST_SUITE_P(RTL,
                         AppListViewTabletPixelTest,
                         testing::Combine(testing::Bool()));

// Verifies the default layout for tablet mode launcher.
TEST_P(AppListViewTabletPixelTest, Basic) {
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "tablet_launcher_basics", 13,
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
      "tablet_launcher_top_gradient_zone", 11,
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
      "tablet_launcher_bottom_gradient_zone", 13,
      GetAppListTestHelper()->GetAppsContainerView()));
}

TEST_P(AppListViewTabletPixelTest, SearchBoxViewActive) {
  raw_ptr<SearchBoxView> search_box_view =
      GetAppListTestHelper()->GetSearchBoxView();
  search_box_view->SetSearchBoxActive(true, ui::EventType::kUnknown);

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "search_box_view_active", 7, search_box_view));
}

class AppListViewAssistantZeroStateTest
    : public AssistantAshTestBase,
      public testing::WithParamInterface<TestVariantsParam> {
 public:
  std::optional<pixel_test::InitParams> CreatePixelTestInitParams()
      const override {
    pixel_test::InitParams init_params;
    init_params.under_rtl = IsRtl(GetParam());
    return init_params;
  }

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {feature_engagement::kIPHLauncherSearchHelpUiFeature}, {});

    AssistantAshTestBase::SetUp();
    DarkLightModeController::Get()->SetDarkModeEnabledForTest(
        IsDarkMode(GetParam()));
    Shell::Get()->tablet_mode_controller()->SetEnabledForTest(
        IsTabletMode(GetParam()));

    // Set a testing text so that the pixel test can compare.
    LauncherSearchIphView::SetChipTextForTesting(u"chip");
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(RTL,
                         AppListViewAssistantZeroStateTest,
                         testing::Combine(/*IsRtl=*/testing::Bool(),
                                          /*IsDarkMode=*/testing::Bool(),
                                          /*IsTabletMode=*/testing::Bool()),
                         &GenerateTestSuffix);

TEST_P(AppListViewAssistantZeroStateTest, Basic) {
  ShowAssistantUi();

  auto* const assistant_page_view = page_view();
  ViewDrawnWaiter().Wait(
      assistant_page_view->GetViewByID(AssistantViewID::kZeroStateView));

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "app_list_view_assistant_zero_state", 9,
      assistant_page_view->GetViewByID(AssistantViewID::kZeroStateView)));
}

}  // namespace ash
