// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/app_list_view.h"

#include <stddef.h>

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ash/app_list/app_list_test_view_delegate.h"
#include "ash/app_list/model/app_list_test_model.h"
#include "ash/app_list/model/search/test_search_result.h"
#include "ash/app_list/views/app_list_folder_view.h"
#include "ash/app_list/views/app_list_item_view.h"
#include "ash/app_list/views/app_list_main_view.h"
#include "ash/app_list/views/app_list_search_view.h"
#include "ash/app_list/views/apps_container_view.h"
#include "ash/app_list/views/apps_grid_view.h"
#include "ash/app_list/views/apps_grid_view_test_api.h"
#include "ash/app_list/views/contents_view.h"
#include "ash/app_list/views/folder_background_view.h"
#include "ash/app_list/views/folder_header_view.h"
#include "ash/app_list/views/page_switcher.h"
#include "ash/app_list/views/paged_apps_grid_view.h"
#include "ash/app_list/views/recent_apps_view.h"
#include "ash/app_list/views/result_selection_controller.h"
#include "ash/app_list/views/search_box_view.h"
#include "ash/app_list/views/search_result_container_view.h"
#include "ash/app_list/views/search_result_list_view.h"
#include "ash/app_list/views/search_result_page_view.h"
#include "ash/app_list/views/search_result_view.h"
#include "ash/constants/ash_features.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/pagination/pagination_model.h"
#include "ash/search_box/search_box_constants.h"
#include "ash/style/ash_color_provider.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/icu_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chromeos/constants/chromeos_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/presentation_time_recorder.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/view_model.h"

namespace ash {
namespace {

using test::AppListTestModel;
using test::AppListTestViewDelegate;
using test::AppsGridViewTestApi;

constexpr int kInitialItems = 34;

// Constants used for for testing app list layout in fullscreen state:
// The app list grid vertical inset - the height of the view fadeout area.
constexpr int kGridVerticalInset = 16;

// Vertical margin for apps grid view (in addition to the grid insets).
constexpr int kGridVerticalMargin = 8;

// The horizontal spacing between apps grid view and the page switcher.
constexpr int kPageSwitcherSpacing = 8;

// The min margins for contents within the fullscreen launcher.
constexpr int kMinLauncherMargin = 24;

// The min horizontal margin for apps grid in fullscreen launcher.
// In addition to min launcher margin, reserves 32 dip for page
// switcher UI.
constexpr int kMinLauncherGridHorizontalMargin = kMinLauncherMargin + 32;

// The amount of the screen height that should be taken up by the vertical
// margin for the apps container view.
constexpr float kLauncherVerticalMarginRatio = 1.0f / 24.0f;
constexpr float kLauncherVerticalMarginRatioLargeHeight = 1.0f / 16.0f;

// `is_large_height` should be set depending on whether the screen height is
// expected to be greater than 800. This is set so that the correct vertical
// margin ratio is used depending on the expected screen height.
int GetExpectedScreenSize(int row_count,
                          int tile_height,
                          int tile_margins,
                          bool is_large_height) {
  // The vertical margins are calculated as a ratio of the total screen height.

  // Below we solve for the screen_height where vertical AppsContainerView
  // margins are defined as margin_ratio * screen_height.

  // Given the following values:
  // h = screen_height
  // r = margin_ratio

  // The screen_height can be calculated with the following:
  // h = 2 * r * h  + grid_size + space_above_grid + shelf_size

  // Solving for h results in the following, which is used below:
  // h = (grid_size + space_above_grid + shelf_size) / (1 - 2 * r)

  float margin_ratio = kLauncherVerticalMarginRatio;
  if (is_large_height)
    margin_ratio = kLauncherVerticalMarginRatioLargeHeight;

  const float shelf_size = 56;
  const float grid_size =
      row_count * tile_height + (row_count - 1) * tile_margins;
  const float search_box_height = 48;
  const float space_above_grid =
      kGridVerticalMargin + kGridVerticalInset + search_box_height;

  DCHECK_NE(margin_ratio, 0.5f);
  const float screen_height = (grid_size + space_above_grid + shelf_size) /
                              (1.0f - 2.0f * margin_ratio);

  // If the margins would be less than the minimum allowed margin, then add back
  // the necessary amount to account for the minimum margin.
  if (screen_height * margin_ratio < kMinLauncherMargin) {
    return screen_height +
           2 * (kMinLauncherMargin - (screen_height * margin_ratio));
  }

  return screen_height;
}

SearchModel* GetSearchModel() {
  return AppListModelProvider::Get()->search_model();
}

void AddRecentApps(int num_apps) {
  for (int i = 0; i < num_apps; i++) {
    auto result = std::make_unique<TestSearchResult>();
    // Use the same "Item #" convention as AppListTestModel uses. The search
    // result IDs must match app item IDs in the app list data model.
    result->set_result_id(test::AppListTestModel::GetItemName(i));
    result->set_result_type(AppListSearchResultType::kInstalledApp);
    result->set_display_type(SearchResultDisplayType::kRecentApps);
    GetSearchModel()->results()->Add(std::move(result));
  }
}

int GridItemSizeWithMargins(int grid_size, int item_size, int item_count) {
  int margin = (grid_size - item_size * item_count) / (2 * (item_count - 1));
  return item_size + 2 * margin;
}

template <class T>
size_t GetVisibleViews(const std::vector<T*>& tiles) {
  size_t count = 0;
  for (const auto& tile : tiles) {
    if (tile->GetVisible())
      count++;
  }
  return count;
}

// A standard set of checks on a view, e.g., ensuring it is drawn and visible.
void CheckView(views::View* subview) {
  ASSERT_TRUE(subview);
  EXPECT_TRUE(subview->parent());
  EXPECT_TRUE(subview->GetVisible());
  EXPECT_TRUE(subview->IsDrawn());
  EXPECT_FALSE(subview->bounds().IsEmpty());
}

bool IsViewVisibleOnScreen(views::View* view) {
  if (!view->IsDrawn())
    return false;
  if (view->layer() && !view->layer()->IsVisible()) {
    return false;
  }
  if (view->layer() && view->layer()->opacity() == 0.0f)
    return false;

  return display::Screen::GetScreen()
      ->GetPrimaryDisplay()
      .work_area()
      .Intersects(view->GetBoundsInScreen());
}

class AppListViewTest : public views::ViewsTestBase {
 public:
  AppListViewTest() = default;
  AppListViewTest(const AppListViewTest&) = delete;
  AppListViewTest& operator=(const AppListViewTest&) = delete;
  ~AppListViewTest() override = default;

  void SetUp() override {
    views::ViewsTestBase::SetUp();
    zero_duration_mode_ =
        std::make_unique<ui::ScopedAnimationDurationScaleMode>(
            ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);
    ui::PresentationTimeRecorder::SetReportPresentationTimeImmediatelyForTest(
        true);
  }

  void TearDown() override {
    ui::PresentationTimeRecorder::SetReportPresentationTimeImmediatelyForTest(
        false);
    view_->GetWidget()->Close();
    zero_duration_mode_.reset();
    views::ViewsTestBase::TearDown();
  }

 protected:
  void Show() { view_->Show(AppListViewState::kFullscreenAllApps); }

  void Initialize(bool is_tablet_mode) {
    delegate_ = std::make_unique<AppListTestViewDelegate>();
    delegate_->SetIsTabletModeEnabled(is_tablet_mode);
    view_ = new AppListView(delegate_.get());
    view_->InitView(GetContext());
    test_api_ = std::make_unique<AppsGridViewTestApi>(apps_grid_view());
    EXPECT_FALSE(view_->GetWidget()->IsVisible());
  }

  // Switches the launcher to |state| and lays out to ensure all launcher pages
  // are in the correct position. Checks that the state is where it should be
  // and returns false on failure.
  bool SetAppListState(ash::AppListState state) {
    ContentsView* contents_view = view_->app_list_main_view()->contents_view();

    // The default method of changing the state to |kStateSearchResults| is via
    // |ShowSearchResults|
    if (state == ash::AppListState::kStateSearchResults)
      contents_view->ShowSearchResults(true);
    else
      contents_view->SetActiveState(state);

    views::test::RunScheduledLayout(contents_view);
    return IsStateShown(state);
  }

  // Returns true if all of the pages are in their correct position for |state|.
  bool IsStateShown(ash::AppListState state) {
    ContentsView* contents_view = view_->app_list_main_view()->contents_view();
    bool success = true;
    for (int i = 0; i < contents_view->NumLauncherPages(); ++i) {
      const gfx::Rect expected_bounds =
          contents_view->GetPageView(i)->GetPageBoundsForState(
              state, contents_view->GetContentsBounds(),
              contents_view->GetSearchBoxBounds(state));
      const views::View* page_view = contents_view->GetPageView(i);
      if (page_view->bounds() != expected_bounds) {
        ADD_FAILURE() << "Page " << i << " bounds do not match "
                      << "expected: " << expected_bounds.ToString()
                      << " actual: " << page_view->bounds().ToString();
        success = false;
      }

      if (contents_view->GetPageIndexForState(state) == i &&
          !page_view->GetVisible()) {
        ADD_FAILURE() << "Target page not visible " << i;
        success = false;
      }
    }

    if (state != delegate_->GetCurrentAppListPage()) {
      ADD_FAILURE() << "Model state does not match state "
                    << static_cast<int>(state);
      success = false;
    }

    return success;
  }

  // Checks the search box widget is at |expected| in the contents view's
  // coordinate space.
  bool CheckSearchBoxView(const gfx::Rect& expected) {
    ContentsView* contents_view = view_->app_list_main_view()->contents_view();
    // Adjust for the search box view's shadow.
    gfx::Rect expected_with_shadow =
        view_->app_list_main_view()
            ->search_box_view()
            ->GetViewBoundsForSearchBoxContentsBounds(expected);
    gfx::Point point = expected_with_shadow.origin();
    views::View::ConvertPointToScreen(contents_view, &point);

    return gfx::Rect(point, expected_with_shadow.size()) ==
           view_->search_box_view()->GetBoundsInScreen();
  }

  void SetTextInSearchBox(const std::u16string& text) {
    views::Textfield* search_box =
        view_->app_list_main_view()->search_box_view()->search_box();
    // Set new text as if it is typed by a user.
    search_box->SetText(std::u16string());
    search_box->InsertText(
        text,
        ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  }

  int ShelfSize() const { return delegate_->GetShelfSize(); }

  // Gets the PaginationModel owned by |view_|.
  ash::PaginationModel* GetPaginationModel() const {
    return view_->GetAppsPaginationModel();
  }

  SearchBoxView* search_box_view() {
    return view_->app_list_main_view()->search_box_view();
  }

  ContentsView* contents_view() {
    return view_->app_list_main_view()->contents_view();
  }

  views::View* scrollable_container() {
    return contents_view()
        ->apps_container_view()
        ->scrollable_container_for_test();
  }

  PagedAppsGridView* apps_grid_view() {
    return contents_view()->apps_container_view()->apps_grid_view();
  }

  PageSwitcher* page_switcher_view() {
    return contents_view()->apps_container_view()->page_switcher();
  }

  RecentAppsView* recent_apps() {
    return contents_view()->apps_container_view()->GetRecentAppsView();
  }

  views::View* assistant_page_view() {
    const int assistant_page_index = contents_view()->GetPageIndexForState(
        ash::AppListState::kStateEmbeddedAssistant);
    return contents_view()->GetPageView(assistant_page_index);
  }

  gfx::Point GetPointBetweenTwoApps() {
    const views::ViewModelT<AppListItemView>* view_model =
        apps_grid_view()->view_model();
    const gfx::Rect bounds_1 = view_model->view_at(0)->GetBoundsInScreen();
    const gfx::Rect bounds_2 = view_model->view_at(1)->GetBoundsInScreen();

    return gfx::Point(bounds_1.right() + (bounds_2.x() - bounds_1.right()) / 2,
                      bounds_1.y());
  }

  int show_wallpaper_context_menu_count() {
    return delegate_->show_wallpaper_context_menu_count();
  }

  void VerifyAppsContainerLayout(const gfx::Size& container_size,
                                 int row_count,
                                 int expected_horizontal_margin,
                                 const gfx::Size& expected_item_size,
                                 bool has_recent_apps) {
    const int column_count = 5;
    ASSERT_EQ(column_count, apps_grid_view()->cols());
    ASSERT_EQ(row_count, apps_grid_view()->GetFirstPageRowsForTesting());

    const float ratio = (container_size.height() > 800)
                            ? kLauncherVerticalMarginRatioLargeHeight
                            : kLauncherVerticalMarginRatio;

    const int expected_vertical_margin = std::max(
        static_cast<int>(container_size.height() * ratio), kMinLauncherMargin);
    const int expected_grid_width =
        container_size.width() - 2 * expected_horizontal_margin;

    // Verify scrollable container bounds.
    const int expected_scrollable_container_top = expected_vertical_margin +
                                                  48 /*search box height*/ +
                                                  kGridVerticalMargin;
    const int expected_scrollable_container_height =
        container_size.height() - expected_scrollable_container_top -
        expected_vertical_margin - ShelfSize();
    EXPECT_EQ(
        gfx::Rect(expected_horizontal_margin, expected_scrollable_container_top,
                  expected_grid_width, expected_scrollable_container_height),
        scrollable_container()->bounds());

    // Verify apps grid bounds.
    gfx::Point grid_origin_in_scrollable_container;
    views::View::ConvertPointToTarget(apps_grid_view(), scrollable_container(),
                                      &grid_origin_in_scrollable_container);
    EXPECT_EQ(gfx::Point(0, kGridVerticalInset),
              grid_origin_in_scrollable_container);

    const int expected_grid_height =
        expected_scrollable_container_height - kGridVerticalInset;
    EXPECT_EQ(gfx::Size(expected_grid_width, expected_grid_height),
              apps_grid_view()->size());

    // Verify page switcher bounds.
    EXPECT_EQ(
        gfx::Rect(expected_grid_width + expected_horizontal_margin +
                      kPageSwitcherSpacing,
                  expected_scrollable_container_top,
                  2 * PageSwitcher::kMaxButtonRadius, expected_grid_height),
        page_switcher_view()->bounds());

    // Verify recent apps view visibility and bounds (when visible).
    EXPECT_EQ(has_recent_apps, recent_apps()->GetVisible());
    if (has_recent_apps) {
      gfx::Point origin_in_scrollable_container;
      views::View::ConvertPointToTarget(recent_apps(), scrollable_container(),
                                        &origin_in_scrollable_container);
      EXPECT_EQ(gfx::Point(0, kGridVerticalInset),
                origin_in_scrollable_container);
      EXPECT_EQ(expected_grid_width, recent_apps()->width());
      EXPECT_EQ(expected_item_size.height(), recent_apps()->height());
    }

    // Horizontal offset between app list item views, which includes tile width
    // and horizontal margin.
    const int horizontal_item_offset = GridItemSizeWithMargins(
        expected_grid_width, expected_item_size.width(), column_count);
    EXPECT_LE(horizontal_item_offset - expected_item_size.width(), 128);

    // Calculate space reserved for separator, which is only shown if suggested
    // content (e.g. recent apps) exists.
    int separator_size = 0;
    if (has_recent_apps) {
      const int separator_margin = expected_item_size.height() > 88 ? 16 : 8;
      separator_size = 2 * separator_margin + 1 /*actual separator height*/;
    }

    // Vertical offset between app list item views, which includes tile height
    // and vertical margin.
    const int vertical_item_offset = GridItemSizeWithMargins(
        expected_grid_height - separator_size, expected_item_size.height(),
        row_count + (has_recent_apps ? 1 : 0));
    EXPECT_GE(vertical_item_offset - expected_item_size.height(), 8);
    EXPECT_LE(vertical_item_offset - expected_item_size.height(), 96);

    // If recent apps are shown, the items on the first page are offset by the
    // recent apps container height, a separator and vertical margin between
    // tiles.
    const int base_vertical_offset =
        has_recent_apps ? vertical_item_offset + separator_size : 0;

    // Verify expected bounds for the first row:
    for (int i = 0; i < column_count; ++i) {
      EXPECT_EQ(gfx::Rect(gfx::Point(i * horizontal_item_offset,
                                     base_vertical_offset),
                          expected_item_size),
                test_api_->GetItemTileRectAtVisualIndex(0, i))
          << "Item " << i << " bounds";
    }

    // Verify expected bounds for the first column:
    for (int j = 1; j < row_count; ++j) {
      EXPECT_EQ(gfx::Rect(gfx::Point(0, base_vertical_offset +
                                            j * vertical_item_offset),
                          expected_item_size),
                test_api_->GetItemTileRectAtVisualIndex(0, j * column_count))
          << "Item " << j * column_count << " bounds";
    }

    // The last item in the page (bottom right):
    EXPECT_EQ(gfx::Rect(gfx::Point((column_count - 1) * horizontal_item_offset,
                                   base_vertical_offset +
                                       (row_count - 1) * vertical_item_offset),
                        expected_item_size),
              test_api_->GetItemTileRectAtVisualIndex(
                  0, row_count * column_count - 1));

    // Verify that search box top is at the expected apps container vertical
    // margin, both in apps, and search results state.
    std::vector<ash::AppListState> available_app_list_states = {
        ash::AppListState::kStateApps, ash::AppListState::kStateSearchResults};
    for (auto app_list_state : available_app_list_states) {
      const gfx::Rect search_box_bounds =
          contents_view()->GetSearchBoxBounds(app_list_state);
      EXPECT_EQ(expected_vertical_margin, search_box_bounds.y())
          << "App list state: " << static_cast<int>(app_list_state);
    }
  }

  // Sets animation durations to zero.
  std::unique_ptr<ui::ScopedAnimationDurationScaleMode> zero_duration_mode_;

  // Needed by AppsContainerView::ContinueContainer.
  AshColorProvider ash_color_provider_;

  raw_ptr<AppListView, DanglingUntriaged> view_ =
      nullptr;  // Owned by native widget.
  std::unique_ptr<AppListTestViewDelegate> delegate_;
  std::unique_ptr<AppsGridViewTestApi> test_api_;

  // Used by AppListFolderView::UpdatePreferredBounds.
  keyboard::KeyboardUIController keyboard_ui_controller_;
};

// Tests app list view layout for different screen sizes.
class AppListViewScalableLayoutTest : public AppListViewTest {
 public:
  AppListViewScalableLayoutTest() {
    scoped_feature_list_.InitWithFeatures(
        {ash::features::kEnableBackgroundBlur}, {});
  }
  ~AppListViewScalableLayoutTest() override = default;

  void SetUp() override {
    // Clear configs created in previous tests.
    AppListConfigProvider::Get().ResetForTesting();
    AppListViewTest::SetUp();
  }

  void TearDown() override {
    AppListViewTest::TearDown();
    AppListConfigProvider::Get().ResetForTesting();
  }
  void InitializeAppList() {
    Initialize(/*is_tablet_mode=*/true);
    delegate_->GetTestModel()->PopulateApps(kInitialItems);
    Show();
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests of focus, optionally parameterized by RTL.
class AppListViewFocusTest : public views::ViewsTestBase,
                             public testing::WithParamInterface<bool> {
 public:
  AppListViewFocusTest() = default;

  AppListViewFocusTest(const AppListViewFocusTest&) = delete;
  AppListViewFocusTest& operator=(const AppListViewFocusTest&) = delete;

  ~AppListViewFocusTest() override = default;

  // testing::Test
  void SetUp() override {
    if (testing::UnitTest::GetInstance()->current_test_info()->value_param()) {
      // Setup right to left environment if necessary.
      is_rtl_ = GetParam();
      if (is_rtl_)
        base::i18n::SetICUDefaultLocale("he");
    }

    views::ViewsTestBase::SetUp();

    // Initialize app list view.
    delegate_ = std::make_unique<AppListTestViewDelegate>();
    view_ = new AppListView(delegate_.get());
    view_->InitView(GetContext());
    Show();
    test_api_ = std::make_unique<AppsGridViewTestApi>(apps_grid_view());

    // Add a folder with apps and other app list items.
    const int kItemNumInFolder = 25;
    const int kAppListItemNum =
        SharedAppListConfig::instance().GetMaxNumOfItemsPerPage() + 1;
    AppListTestModel* model = delegate_->GetTestModel();
    AppListFolderItem* folder_item =
        model->CreateAndPopulateFolderWithApps(kItemNumInFolder);
    model->PopulateApps(kAppListItemNum);
    EXPECT_EQ(static_cast<size_t>(kAppListItemNum + 1),
              model->top_level_item_list()->item_count());
    EXPECT_EQ(folder_item->id(),
              model->top_level_item_list()->item_at(0)->id());

    // Disable animation timer.
    view_->GetWidget()->GetLayer()->GetAnimator()->set_disable_timer_for_test(
        true);
  }

  void TearDown() override {
    view_->GetWidget()->Close();
    views::ViewsTestBase::TearDown();
  }

  void SetAppListState(ash::AppListViewState state) {
    ASSERT_NE(state, ash::AppListViewState::kClosed);
    view_->SetState(state);
  }

  SearchResultContainerView* GetSearchResultListView() {
    constexpr int kBestMatchContainerIndex = 1;
    return contents_view()
        ->search_result_page_view()
        ->search_view()
        ->result_container_views_for_test()[kBestMatchContainerIndex];
  }

  void Show() { view_->Show(AppListViewState::kFullscreenAllApps); }

  AppsGridViewTestApi* test_api() { return test_api_.get(); }

  void SimulateKeyPress(ui::KeyboardCode key_code,
                        bool shift_down,
                        bool ctrl_down = false) {
    ui::KeyEvent key_event(ui::EventType::kKeyPressed, key_code,
                           shift_down  ? ui::EF_SHIFT_DOWN
                           : ctrl_down ? ui::EF_CONTROL_DOWN
                                       : ui::EF_NONE);
    view_->GetWidget()->OnKeyEvent(&key_event);
  }

  // Adds test search results.
  void SetUpSearchResults(int list_results_num) {
    SearchModel::SearchResults* results = GetSearchModel()->results();
    results->DeleteAll();
    double display_score = list_results_num;
    for (int i = 0; i < list_results_num; ++i) {
      // Set the display score of the results in decreasing order
      // (so the earlier groups have higher display score, and therefore appear
      // first).
      display_score -= 1;
      std::unique_ptr<TestSearchResult> result =
          std::make_unique<TestSearchResult>();
      result->set_display_type(ash::SearchResultDisplayType::kList);
      result->set_display_score(display_score);
      result->SetTitle(u"Test" + base::NumberToString16(i));
      result->set_result_id("Test" + base::NumberToString(i));
      result->set_best_match(true);
      results->Add(std::move(result));
    }

    // Adding results will schedule Update().
    base::RunLoop().RunUntilIdle();
  }

  void ClearSearchResults() { GetSearchModel()->results()->DeleteAll(); }

  void AddSearchResultWithTitleAndScore(std::string_view title, double score) {
    std::unique_ptr<TestSearchResult> result =
        std::make_unique<TestSearchResult>();
    result->set_display_type(ash::SearchResultDisplayType::kList);
    result->set_display_score(score);
    result->SetTitle(base::ASCIIToUTF16(title));
    result->set_best_match(true);
    result->SetCategory(ash::AppListSearchResultCategory::kWeb);
    GetSearchModel()->results()->Add(std::move(result));
    base::RunLoop().RunUntilIdle();
  }

  int GetOpenFirstSearchResultCount() {
    std::map<size_t, int>& counts = delegate_->open_search_result_counts();
    if (counts.size() == 0)
      return 0;
    return counts[0];
  }

  int GetTotalOpenSearchResultCount() {
    return delegate_->open_search_result_count();
  }

  // Test focus traversal across all the views in |view_list|. The initial focus
  // is expected to be on the first view in |view_list|. The final focus is
  // expected to be on the last view in |view_list| after |view_list.size()-1|
  // key events are pressed.
  void TestFocusTraversal(const std::vector<views::View*>& view_list,
                          ui::KeyboardCode key_code,
                          bool shift_down) {
    EXPECT_EQ(view_list[0], focused_view());
    for (size_t i = 1; i < view_list.size(); ++i) {
      SimulateKeyPress(key_code, shift_down);
      EXPECT_EQ(view_list[i], focused_view());
    }
  }

  // Test the behavior triggered by left and right key when focus is on the
  // |textfield|. Does not insert text.
  void TestLeftAndRightKeyTraversalOnTextfield(views::Textfield* textfield) {
    EXPECT_TRUE(textfield->GetText().empty());
    EXPECT_EQ(textfield, focused_view());

    views::View* next_view =
        view_->GetWidget()->GetFocusManager()->GetNextFocusableView(
            textfield, view_->GetWidget(), false, false);
    views::View* prev_view =
        view_->GetWidget()->GetFocusManager()->GetNextFocusableView(
            textfield, view_->GetWidget(), true, false);

    // Only need to hit left or right key once to move focus outside the
    // textfield when it is empty.
    SimulateKeyPress(ui::VKEY_RIGHT, false);
    EXPECT_EQ(is_rtl_ ? prev_view : next_view, focused_view());

    SimulateKeyPress(ui::VKEY_LEFT, false);
    EXPECT_EQ(textfield, focused_view());

    SimulateKeyPress(ui::VKEY_LEFT, false);
    EXPECT_EQ(is_rtl_ ? next_view : prev_view, focused_view());

    SimulateKeyPress(ui::VKEY_RIGHT, false);
    EXPECT_EQ(textfield, focused_view()) << focused_view()->GetClassName();
  }

  // Test the behavior triggered by left and right key when focus is on the
  // |textfield|. This includes typing text into the field.
  void TestLeftAndRightKeyOnTextfieldWithText(views::Textfield* textfield,
                                              bool text_rtl) {
    // Test initial traversal
    TestLeftAndRightKeyTraversalOnTextfield(textfield);

    // Type something in textfield.
    std::u16string text = text_rtl ? u"اختبار" : u"test";
    textfield->InsertText(
        text,
        ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
    views::View* next_view =
        view_->GetWidget()->GetFocusManager()->GetNextFocusableView(
            textfield, view_->GetWidget(), false, false);
    views::View* prev_view =
        view_->GetWidget()->GetFocusManager()->GetNextFocusableView(
            textfield, view_->GetWidget(), true, false);
    EXPECT_EQ(text.length(), textfield->GetCursorPosition());
    EXPECT_FALSE(textfield->HasSelection());
    EXPECT_EQ(textfield, focused_view());

    const ui::KeyboardCode backward_key =
        text_rtl ? ui::VKEY_RIGHT : ui::VKEY_LEFT;
    const ui::KeyboardCode forward_key =
        text_rtl ? ui::VKEY_LEFT : ui::VKEY_RIGHT;

    // Move cursor backward.
    SimulateKeyPress(backward_key, false);
    EXPECT_EQ(text.length() - 1, textfield->GetCursorPosition());
    EXPECT_EQ(textfield, focused_view());

    // Move cursor forward.
    SimulateKeyPress(forward_key, false);
    EXPECT_EQ(text.length(), textfield->GetCursorPosition());
    EXPECT_EQ(textfield, focused_view());

    // Hit forward key to move focus outside the textfield.
    SimulateKeyPress(forward_key, false);
    EXPECT_EQ((!is_rtl_ && !text_rtl) || (is_rtl_ && text_rtl) ? next_view
                                                               : prev_view,
              focused_view());

    // Hit backward key to move focus back to textfield and select all text.
    SimulateKeyPress(backward_key, false);
    EXPECT_EQ(text, textfield->GetSelectedText());
    EXPECT_EQ(textfield, focused_view());

    // Hit backward key to move cursor to the beginning.
    SimulateKeyPress(backward_key, false);
    EXPECT_EQ(0U, textfield->GetCursorPosition());
    EXPECT_FALSE(textfield->HasSelection());
    EXPECT_EQ(textfield, focused_view());

    // Hit backward key to move focus outside the textfield.
    SimulateKeyPress(backward_key, false);
    EXPECT_EQ((!is_rtl_ && !text_rtl) || (is_rtl_ && text_rtl) ? prev_view
                                                               : next_view,
              focused_view());

    // Hit forward key to move focus back to textfield and select all text.
    SimulateKeyPress(forward_key, false);
    EXPECT_EQ(text, textfield->GetSelectedText());
    EXPECT_EQ(textfield, focused_view());

    // Hit forward key to move cursor to the end.
    SimulateKeyPress(forward_key, false);
    EXPECT_EQ(text.length(), textfield->GetCursorPosition());
    EXPECT_FALSE(textfield->HasSelection());
    EXPECT_EQ(textfield, focused_view());

    // Hitt forward key to move focus outside the textfield.
    SimulateKeyPress(forward_key, false);
    EXPECT_EQ((!is_rtl_ && !text_rtl) || (is_rtl_ && text_rtl) ? next_view
                                                               : prev_view,
              focused_view());

    // Clean up
    textfield->RequestFocus();
    textfield->SetText(u"");
  }

  AppListView* app_list_view() { return view_; }

  AppListMainView* main_view() { return view_->app_list_main_view(); }

  ContentsView* contents_view() {
    return view_->app_list_main_view()->contents_view();
  }

  PagedAppsGridView* apps_grid_view() {
    return main_view()
        ->contents_view()
        ->apps_container_view()
        ->apps_grid_view();
  }

  AppListFolderView* app_list_folder_view() {
    return main_view()
        ->contents_view()
        ->apps_container_view()
        ->app_list_folder_view();
  }

  SearchBoxView* search_box_view() { return main_view()->search_box_view(); }

  AppListItemView* folder_item_view() {
    return apps_grid_view()->view_model()->view_at(0);
  }

  views::View* focused_view() {
    return view_->GetWidget()->GetFocusManager()->GetFocusedView();
  }

 protected:
  bool is_rtl_ = false;
  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  AshColorProvider ash_color_provider_;
  raw_ptr<AppListView, DanglingUntriaged> view_ =
      nullptr;  // Owned by native widget.

  std::unique_ptr<AppListTestViewDelegate> delegate_;
  std::unique_ptr<AppsGridViewTestApi> test_api_;
  // Restores the locale to default when destructor is called.
  base::test::ScopedRestoreICUDefaultLocale restore_locale_;

  // Used by AppListFolderView::UpdatePreferredBounds.
  keyboard::KeyboardUIController keyboard_ui_controller_;
};

INSTANTIATE_TEST_SUITE_P(Rtl, AppListViewFocusTest, testing::Bool());

// Tests that the initial focus is on search box.
TEST_F(AppListViewFocusTest, InitialFocus) {
  EXPECT_EQ(search_box_view()->search_box(), focused_view());
}

// Tests the linear focus traversal in FULLSCREEN_ALL_APPS state.
TEST_P(AppListViewFocusTest, LinearFocusTraversalInFullscreenAllAppsState) {
  Show();

  std::vector<views::View*> forward_view_list;
  forward_view_list.push_back(search_box_view()->search_box());
  const views::ViewModelT<AppListItemView>* view_model =
      apps_grid_view()->view_model();
  for (const auto& entry : view_model->entries())
    forward_view_list.push_back(entry.view);
  forward_view_list.push_back(search_box_view()->search_box());
  std::vector<views::View*> backward_view_list = forward_view_list;
  std::reverse(backward_view_list.begin(), backward_view_list.end());

  // Test traversal triggered by tab.
  TestFocusTraversal(forward_view_list, ui::VKEY_TAB, false);

  // Test traversal triggered by shift+tab.
  TestFocusTraversal(backward_view_list, ui::VKEY_TAB, true);

  // Test traversal triggered by right.
  TestFocusTraversal(is_rtl_ ? backward_view_list : forward_view_list,
                     ui::VKEY_RIGHT, false);

  // Test traversal triggered by left.
  TestFocusTraversal(is_rtl_ ? forward_view_list : backward_view_list,
                     ui::VKEY_LEFT, false);
}

// Tests the linear focus traversal in FULLSCREEN_ALL_APPS state within folder.
TEST_P(AppListViewFocusTest, LinearFocusTraversalInFolder) {
  Show();

  // Open the folder.
  folder_item_view()->RequestFocus();
  SimulateKeyPress(ui::VKEY_RETURN, false);
  EXPECT_TRUE(contents_view()->apps_container_view()->IsInFolderView());

  std::vector<views::View*> forward_view_list;
  const views::ViewModelT<AppListItemView>* view_model =
      app_list_folder_view()->items_grid_view()->view_model();
  for (const auto& entry : view_model->entries())
    forward_view_list.push_back(entry.view);
  forward_view_list.push_back(
      app_list_folder_view()->folder_header_view()->GetFolderNameViewForTest());
  forward_view_list.push_back(search_box_view()->search_box());
  forward_view_list.push_back(view_model->view_at(0));
  std::vector<views::View*> backward_view_list = forward_view_list;
  std::reverse(backward_view_list.begin(), backward_view_list.end());

  // Test traversal triggered by tab.
  TestFocusTraversal(forward_view_list, ui::VKEY_TAB, false);

  // Test traversal triggered by shift+tab.
  TestFocusTraversal(backward_view_list, ui::VKEY_TAB, true);

  // Test traversal triggered by right.
  TestFocusTraversal(is_rtl_ ? backward_view_list : forward_view_list,
                     ui::VKEY_RIGHT, false);

  // Test traversal triggered by left.
  TestFocusTraversal(is_rtl_ ? forward_view_list : backward_view_list,
                     ui::VKEY_LEFT, false);
}

// Tests the vertical focus traversal in FULLSCREEN_ALL_APPS state.
TEST_P(AppListViewFocusTest, VerticalFocusTraversalInFullscreenAllAppsState) {
  Show();

  std::vector<views::View*> forward_view_list;
  forward_view_list.push_back(search_box_view()->search_box());
  const views::ViewModelT<AppListItemView>* view_model =
      apps_grid_view()->view_model();
  for (size_t i = 0; i < view_model->view_size(); i += apps_grid_view()->cols())
    forward_view_list.push_back(view_model->view_at(i));
  forward_view_list.push_back(search_box_view()->search_box());

  // Test traversal triggered by down.
  TestFocusTraversal(forward_view_list, ui::VKEY_DOWN, false);

  std::vector<views::View*> backward_view_list;
  backward_view_list.push_back(search_box_view()->search_box());
  for (size_t i = view_model->view_size() - 1; i < view_model->view_size();
       i -= apps_grid_view()->cols())
    backward_view_list.push_back(view_model->view_at(i));
  backward_view_list.push_back(search_box_view()->search_box());

  // Test traversal triggered by up.
  TestFocusTraversal(backward_view_list, ui::VKEY_UP, false);
}

// Tests the vertical focus traversal in FULLSCREEN_ALL_APPS state in the first
// page within folder.
TEST_F(AppListViewFocusTest, VerticalFocusTraversalInFirstPageOfFolder) {
  Show();

  // Open the folder.
  folder_item_view()->RequestFocus();
  SimulateKeyPress(ui::VKEY_RETURN, false);
  EXPECT_TRUE(contents_view()->apps_container_view()->IsInFolderView());

  std::vector<views::View*> forward_view_list;
  const views::ViewModelT<AppListItemView>* view_model =
      app_list_folder_view()->items_grid_view()->view_model();
  for (size_t i = 0; i < view_model->view_size();
       i += app_list_folder_view()->items_grid_view()->cols()) {
    forward_view_list.push_back(view_model->view_at(i));
  }
  forward_view_list.push_back(
      app_list_folder_view()->folder_header_view()->GetFolderNameViewForTest());
  forward_view_list.push_back(search_box_view()->search_box());
  forward_view_list.push_back(view_model->view_at(0));

  // Test traversal triggered by down.
  TestFocusTraversal(forward_view_list, ui::VKEY_DOWN, false);

  std::vector<views::View*> backward_view_list;
  backward_view_list.push_back(view_model->view_at(0));
  backward_view_list.push_back(search_box_view()->search_box());
  backward_view_list.push_back(
      app_list_folder_view()->folder_header_view()->GetFolderNameViewForTest());
  for (size_t i = view_model->view_size() - 1; i < view_model->view_size();
       i -= app_list_folder_view()->items_grid_view()->cols()) {
    backward_view_list.push_back(view_model->view_at(i));
  }
  backward_view_list.push_back(search_box_view()->search_box());

  // Test traversal triggered by up.
  TestFocusTraversal(backward_view_list, ui::VKEY_UP, false);
}

// Tests that focus changes does not update the search box text.
TEST_F(AppListViewFocusTest, SearchBoxTextDoesNotUpdateOnResultFocus) {
  Show();
  views::Textfield* search_box = search_box_view()->search_box();
  search_box->InsertText(
      u"TestText",
      ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);

  // Set up test results with unique titles
  ClearSearchResults();
  AddSearchResultWithTitleAndScore("TestResult1", 3);
  AddSearchResultWithTitleAndScore("TestResult2", 2);
  AddSearchResultWithTitleAndScore("TestResult3", 1);

  // Change focus to the next result
  SimulateKeyPress(ui::VKEY_TAB, false);

  EXPECT_EQ(search_box->GetText(), u"TestText");
  EXPECT_EQ(search_box_view()->GetSearchBoxGhostTextForTest(),
            "TestResult2 - Websites");

  SimulateKeyPress(ui::VKEY_TAB, true);

  EXPECT_EQ(search_box->GetText(), u"TestText");
  EXPECT_EQ(search_box_view()->GetSearchBoxGhostTextForTest(),
            "TestResult1 - Websites");

  SimulateKeyPress(ui::VKEY_TAB, false);

  // Change focus to the final result
  SimulateKeyPress(ui::VKEY_TAB, false);

  EXPECT_EQ(search_box->GetText(), u"TestText");
  EXPECT_EQ(search_box_view()->GetSearchBoxGhostTextForTest(),
            "TestResult3 - Websites");
}

// Tests that ctrl-A selects all text in the searchbox when the SearchBoxView is
// not focused.
TEST_F(AppListViewFocusTest, CtrlASelectsAllTextInSearchbox) {
  Show();
  search_box_view()->search_box()->InsertText(
      u"test0",
      ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  constexpr int kListResults = 2;
  SetUpSearchResults(kListResults);

  // Move focus to the first search result.
  SimulateKeyPress(ui::VKEY_TAB, false);
  SimulateKeyPress(ui::VKEY_TAB, false);

  // Focus left the searchbox, so the selected range should be at the end of the
  // search text.
  EXPECT_FALSE(search_box_view()->search_box()->HasSelection());
  EXPECT_EQ(gfx::Range(5, 5),
            search_box_view()->search_box()->GetSelectedRange());

  // Press Ctrl-A, everything should be selected and the selected range should
  // include the whole text.
  SimulateKeyPress(ui::VKEY_A, false, true);
  EXPECT_TRUE(search_box_view()->search_box()->HasSelection());
  EXPECT_EQ(gfx::Range(0, 5),
            search_box_view()->search_box()->GetSelectedRange());

  // Advance focus, Focus should leave the searchbox, and the selected range
  // should be at the end of the search text.
  SimulateKeyPress(ui::VKEY_TAB, false);
  EXPECT_FALSE(search_box_view()->search_box()->HasSelection());
  EXPECT_EQ(gfx::Range(5, 5),
            search_box_view()->search_box()->GetSelectedRange());
}

// Tests that the first search result's view is selected after search results
// are updated when the focus is on search box.
TEST_F(AppListViewFocusTest, FirstResultSelectedAfterSearchResultsUpdated) {
  Show();

  // Type something in search box to transition to search state and populate
  // fake list results.
  search_box_view()->search_box()->InsertText(
      u"test",
      ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  const int kListResults = 2;
  SetUpSearchResults(kListResults);

  EXPECT_EQ(search_box_view()->search_box(), focused_view());
  SearchResultContainerView* list_view = GetSearchResultListView();
  EXPECT_TRUE(list_view->GetResultViewAt(0)->selected());

  ResultSelectionController* selection_controller =
      contents_view()
          ->search_result_page_view()
          ->search_view()
          ->result_selection_controller_for_test();

  // Ensures the |ResultSelectionController| selects the correct result.
  EXPECT_EQ(selection_controller->selected_result(),
            list_view->GetResultViewAt(0));

  // Clear up all search results.
  SetUpSearchResults(0);
  EXPECT_EQ(search_box_view()->search_box(), focused_view());
}

// Tests hitting Enter key when focus is on search box.
// There are two behaviors:
// 1. Activate the search box when it is inactive.
// 2. Open the first result when query exists.
TEST_F(AppListViewFocusTest, HittingEnterWhenFocusOnSearchBox) {
  Show();

  // Initially the search box is inactive, hitting Enter to activate it.
  EXPECT_FALSE(search_box_view()->is_search_box_active());
  SimulateKeyPress(ui::VKEY_RETURN, false);
  EXPECT_TRUE(search_box_view()->is_search_box_active());

  // Type something in search box to transition to search state and populate
  // fake list results. Then hit Enter key.
  search_box_view()->search_box()->InsertText(
      u"test",
      ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  const int kListResults = 2;
  SetUpSearchResults(kListResults);
  SimulateKeyPress(ui::VKEY_RETURN, false);
  EXPECT_EQ(1, GetOpenFirstSearchResultCount());
  EXPECT_EQ(1, GetTotalOpenSearchResultCount());

  // Clear up all search results. Then hit Enter key.
  SetUpSearchResults(0);
  SimulateKeyPress(ui::VKEY_RETURN, false);
  EXPECT_EQ(1, GetOpenFirstSearchResultCount());
  EXPECT_EQ(1, GetTotalOpenSearchResultCount());
}

// Tests that search box becomes focused when it is activated.
TEST_F(AppListViewFocusTest, SetFocusOnSearchboxWhenActivated) {
  app_list_view()->Show(AppListViewState::kFullscreenAllApps);

  // Press tab several times to move focus out of the search box.
  SimulateKeyPress(ui::VKEY_TAB, false);
  SimulateKeyPress(ui::VKEY_TAB, false);
  EXPECT_FALSE(search_box_view()->search_box()->HasFocus());

  // Activate the search box.
  search_box_view()->SetSearchBoxActive(true, ui::EventType::kMousePressed);
  EXPECT_TRUE(search_box_view()->search_box()->HasFocus());

  // Deactivate the search box won't move focus away.
  search_box_view()->SetSearchBoxActive(false, ui::EventType::kMousePressed);
  EXPECT_TRUE(search_box_view()->search_box()->HasFocus());
}

// Tests the left and right key when focus is on the textfield.
TEST_P(AppListViewFocusTest, HittingLeftRightWhenFocusOnTextfield) {
  Show();

  // Open the folder.
  folder_item_view()->RequestFocus();
  SimulateKeyPress(ui::VKEY_RETURN, false);

  // Set focus on the folder name.
  views::Textfield* folder_name_view = static_cast<views::Textfield*>(
      app_list_folder_view()->folder_header_view()->GetFolderNameViewForTest());
  folder_name_view->RequestFocus();

  // Test folder name.
  TestLeftAndRightKeyOnTextfieldWithText(folder_name_view, false);
  TestLeftAndRightKeyOnTextfieldWithText(folder_name_view, true);

  // Set focus on the search box.
  search_box_view()->search_box()->RequestFocus();

  // Test search box. Active traversal has been tested at this point. This will
  // specifically test inactive traversal with no search results set up.
  TestLeftAndRightKeyTraversalOnTextfield(search_box_view()->search_box());
}

// Tests that the focus is reset and the folder does not exit after hitting
// enter/escape on folder name.
TEST_P(AppListViewFocusTest, FocusResetAfterHittingEnterOrEscapeOnFolderName) {
  Show();

  // Open the folder.
  folder_item_view()->RequestFocus();
  SimulateKeyPress(ui::VKEY_RETURN, false);
  EXPECT_TRUE(contents_view()->apps_container_view()->IsInFolderView());

  // Set focus on the folder name.
  FolderHeaderView* const folder_header_view =
      app_list_folder_view()->folder_header_view();
  views::View* const folder_name_view =
      folder_header_view->GetFolderNameViewForTest();
  folder_name_view->RequestFocus();

  EXPECT_TRUE(contents_view()->apps_container_view()->IsInFolderView());
  EXPECT_TRUE(folder_header_view->IsFolderNameViewActiveForTest());

  // Hit enter key.
  SimulateKeyPress(ui::VKEY_RETURN, false);

  EXPECT_TRUE(contents_view()->apps_container_view()->IsInFolderView());
  EXPECT_FALSE(folder_header_view->IsFolderNameViewActiveForTest());

  // Reactivate and hit escape key. NOTE: The folder name view will not have
  // focus if Jelly feature is disabled, in that case it's sufficient to refocus
  // the folder name view to "reactivate" it.
  if (folder_name_view->HasFocus()) {
    SimulateKeyPress(ui::VKEY_RETURN, false);
  } else {
    folder_name_view->RequestFocus();
  }

  EXPECT_TRUE(folder_header_view->IsFolderNameViewActiveForTest());

  SimulateKeyPress(ui::VKEY_ESCAPE, false);
  EXPECT_TRUE(contents_view()->apps_container_view()->IsInFolderView());
  EXPECT_FALSE(folder_header_view->IsFolderNameViewActiveForTest());

  // Escape when textfield is not active closes the floder.
  SimulateKeyPress(ui::VKEY_ESCAPE, false);
  EXPECT_FALSE(contents_view()->apps_container_view()->IsInFolderView());
}

// Tests that the selection highlight follows the page change.
TEST_F(AppListViewFocusTest, SelectionHighlightFollowsChangingPage) {
  // Move the focus to the first app in the grid.
  Show();
  const views::ViewModelT<AppListItemView>* view_model =
      apps_grid_view()->view_model();
  AppListItemView* first_item_view = view_model->view_at(0);
  first_item_view->RequestFocus();
  ASSERT_EQ(0, apps_grid_view()->pagination_model()->selected_page());

  // Select the second page.
  apps_grid_view()->pagination_model()->SelectPage(1, false);

  // Test that focus followed to the next page.
  EXPECT_EQ(view_model->view_at(test_api()->TilesPerPageInPagedGrid(0)),
            apps_grid_view()->selected_view());

  // Select the first page.
  apps_grid_view()->pagination_model()->SelectPage(0, false);

  // Test that focus followed.
  EXPECT_EQ(view_model->view_at(test_api()->TilesPerPageInPagedGrid(0) - 1),
            apps_grid_view()->selected_view());
}

// Tests that the selection highlight only shows up inside a folder if the
// selection highlight existed on the folder before it opened.
TEST_F(AppListViewFocusTest, SelectionDoesNotShowInFolderIfNotSelected) {
  // Open a folder without making the view selected.
  Show();
  const gfx::Point folder_item_view_bounds =
      folder_item_view()->bounds().CenterPoint();
  ui::GestureEvent tap(folder_item_view_bounds.x(), folder_item_view_bounds.y(),
                       0, base::TimeTicks(),
                       ui::GestureEventDetails(ui::EventType::kGestureTap));
  folder_item_view()->OnGestureEvent(&tap);
  ASSERT_TRUE(contents_view()->apps_container_view()->IsInFolderView());

  // Test that there is no selected view in the folders grid view, but the first
  // item is focused.
  AppsGridView* items_grid_view = app_list_folder_view()->items_grid_view();
  EXPECT_FALSE(items_grid_view->has_selected_view());
  EXPECT_EQ(items_grid_view->view_model()->view_at(0), focused_view());

  // Hide the folder, expect that the folder is not selected, but is focused.
  app_list_view()->AcceleratorPressed(
      ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));
  EXPECT_FALSE(apps_grid_view()->has_selected_view());
  EXPECT_EQ(folder_item_view(), focused_view());
}

// Tests that the selection highlight only shows on the activated folder if it
// existed within the folder.
TEST_F(AppListViewFocusTest, SelectionGoesIntoFolderIfSelected) {
  // Open a folder without making the view selected.
  Show();

  folder_item_view()->RequestFocus();
  ASSERT_TRUE(apps_grid_view()->IsSelectedView(folder_item_view()));

  // Show the folder.
  const gfx::Point folder_item_view_bounds =
      folder_item_view()->bounds().CenterPoint();
  ui::GestureEvent tap(folder_item_view_bounds.x(), folder_item_view_bounds.y(),
                       0, base::TimeTicks(),
                       ui::GestureEventDetails(ui::EventType::kGestureTap));
  folder_item_view()->OnGestureEvent(&tap);
  ASSERT_TRUE(contents_view()->apps_container_view()->IsInFolderView());

  // Test that the focused view is also selected.
  AppsGridView* items_grid_view = app_list_folder_view()->items_grid_view();
  EXPECT_EQ(items_grid_view->selected_view(), focused_view());
  EXPECT_EQ(items_grid_view->view_model()->view_at(0), focused_view());

  // Hide the folder, expect that the folder is selected and focused.
  app_list_view()->AcceleratorPressed(
      ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));
  EXPECT_TRUE(apps_grid_view()->IsSelectedView(folder_item_view()));
  EXPECT_EQ(folder_item_view(), focused_view());
}

// Tests that in tablet mode, the app list opens in fullscreen by default.
TEST_F(AppListViewTest, ShowFullscreenWhenInTabletMode) {
  Initialize(/*is_tablet_mode=*/true);

  Show();

  ASSERT_EQ(ash::AppListViewState::kFullscreenAllApps, view_->app_list_state());
}

// Tests that typing when in fullscreen changes the state to fullscreen search.
TEST_F(AppListViewTest, TypingFullscreenToFullscreenSearch) {
  Initialize(true /*is_tablet_mode*/);
  Show();

  view_->SetState(AppListViewState::kFullscreenAllApps);
  views::Textfield* search_box =
      view_->app_list_main_view()->search_box_view()->search_box();

  search_box->SetText(std::u16string());
  search_box->InsertText(
      u"https://youtu.be/dQw4w9WgXcQ",
      ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);

  ASSERT_EQ(ash::AppListViewState::kFullscreenSearch, view_->app_list_state());
}

// Tests that in tablet mode, typing changes the state to fullscreen search.
TEST_F(AppListViewTest, TypingTabletModeFullscreenSearch) {
  Initialize(/*is_tablet_mode=*/true);
  Show();
  SetTextInSearchBox(u"cool!");
  EXPECT_EQ(ash::AppListViewState::kFullscreenSearch, view_->app_list_state());

  view_->SetState(AppListViewState::kFullscreenAllApps);
  // The state should also change to fullscreen search if the user enters white
  // space query.
  SetTextInSearchBox(u" ");
  EXPECT_EQ(ash::AppListViewState::kFullscreenSearch, view_->app_list_state());
}

// Tests that pressing escape when in tablet mode keeps app list in fullscreen.
TEST_F(AppListViewTest, EscapeKeyTabletModeStayFullscreen) {
  // Put into fullscreen by using tablet mode.
  Initialize(/*is_tablet_mode=*/true);

  Show();
  view_->AcceleratorPressed(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));

  ASSERT_EQ(ash::AppListViewState::kFullscreenAllApps, view_->app_list_state());
}

// Tests that pressing escape when in fullscreen search changes to fullscreen.
TEST_F(AppListViewTest, EscapeKeyFullscreenSearchToFullscreen) {
  Initialize(/*is_tablet_mode=*/true);
  Show();
  SetTextInSearchBox(u"https://youtu.be/dQw4w9WgXcQ");
  ASSERT_EQ(ash::AppListViewState::kFullscreenSearch, view_->app_list_state());

  view_->AcceleratorPressed(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));
  ASSERT_EQ(ash::AppListViewState::kFullscreenAllApps, view_->app_list_state());
}

// Tests that pressing escape when in sideshelf search changes to fullscreen.
TEST_F(AppListViewTest, EscapeKeySideShelfSearchToFullscreen) {
  // Put into fullscreen using side-shelf.
  Initialize(/*is_tablet_mode=*/true);

  Show();
  SetTextInSearchBox(u"kitty");
  view_->AcceleratorPressed(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));

  ASSERT_EQ(ash::AppListViewState::kFullscreenAllApps, view_->app_list_state());
}

// Tests that in fullscreen, the app list has multiple pages with enough apps.
TEST_F(AppListViewTest, PopulateAppsCreatesAnotherPage) {
  Initialize(/*is_tablet_mode=*/true);
  delegate_->GetTestModel()->PopulateApps(kInitialItems);

  Show();

  ASSERT_EQ(2, GetPaginationModel()->total_pages());
}

// Tests that pressing escape when in tablet search changes to fullscreen.
TEST_F(AppListViewTest, EscapeKeyTabletModeSearchToFullscreen) {
  // Put into fullscreen using tablet mode.
  Initialize(/*is_tablet_mode=*/true);

  Show();
  SetTextInSearchBox(u"yay");
  view_->AcceleratorPressed(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));

  ASSERT_EQ(ash::AppListViewState::kFullscreenAllApps, view_->app_list_state());
}

TEST_F(AppListViewTest, AppsGridViewVisibilityOnReopening) {
  Initialize(/*is_tablet_mode=*/true);
  Show();
  EXPECT_TRUE(IsViewVisibleOnScreen(apps_grid_view()));

  view_->SetState(ash::AppListViewState::kFullscreenSearch);
  EXPECT_TRUE(IsViewVisibleOnScreen(apps_grid_view()));

  // Close the app-list and re-show to fullscreen all apps.
  view_->SetState(ash::AppListViewState::kClosed);
  Show();
  EXPECT_TRUE(IsViewVisibleOnScreen(apps_grid_view()));
}

// Tests displaying the app list and performs a standard set of checks on its
// top level views.
TEST_F(AppListViewTest, DisplayTest) {
  Initialize(/*is_tablet_mode=*/true);
  EXPECT_EQ(-1, GetPaginationModel()->total_pages());
  delegate_->GetTestModel()->PopulateApps(kInitialItems);

  Show();

  // |view_| bounds equal to the root window's size.
  EXPECT_EQ("800x600", view_->bounds().size().ToString());

  EXPECT_EQ(2, GetPaginationModel()->total_pages());
  EXPECT_EQ(0, GetPaginationModel()->selected_page());

  // Checks on the main view.
  AppListMainView* main_view = view_->app_list_main_view();
  EXPECT_NO_FATAL_FAILURE(CheckView(main_view));
  EXPECT_NO_FATAL_FAILURE(CheckView(main_view->contents_view()));

  ash::AppListState expected = ash::AppListState::kStateApps;
  EXPECT_TRUE(main_view->contents_view()->IsStateActive(expected));
  EXPECT_EQ(expected, delegate_->GetCurrentAppListPage());
}

// Tests switching rapidly between multiple pages of the launcher.
// bubble launcher has tests of animated page transitions in the tests for
// AppListBubbleView and AppListBubbleAppsPage.
TEST_F(AppListViewTest, PageSwitchingAnimationTest) {
  Initialize(/*is_tablet_mode=*/true);
  delegate_->GetTestModel()->PopulateApps(kInitialItems);

  ui::ScopedAnimationDurationScaleMode non_zero_duration(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  Show();

  EXPECT_EQ(2, GetPaginationModel()->total_pages());
  EXPECT_EQ(0, GetPaginationModel()->selected_page());

  AppListMainView* main_view = view_->app_list_main_view();
  // Checks on the main view.
  EXPECT_NO_FATAL_FAILURE(CheckView(main_view));
  EXPECT_NO_FATAL_FAILURE(CheckView(main_view->contents_view()));

  EXPECT_TRUE(
      main_view->contents_view()->IsStateActive(ash::AppListState::kStateApps));

  std::u16string search_text = u"test";
  main_view->search_box_view()->search_box()->SetText(std::u16string());
  main_view->search_box_view()->search_box()->InsertText(
      search_text,
      ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);

  EXPECT_TRUE(main_view->contents_view()->IsStateActive(
      ash::AppListState::kStateSearchResults));

  ui::KeyEvent key_event(ui::EventType::kKeyPressed, ui::VKEY_ESCAPE,
                         ui::EF_NONE);
  main_view->contents_view()->GetWidget()->OnKeyEvent(&key_event);

  EXPECT_TRUE(
      main_view->contents_view()->IsStateActive(ash::AppListState::kStateApps));
}

// Tests that the correct views are displayed for showing search results.
TEST_F(AppListViewTest, DISABLED_SearchResultsTest) {
  Initialize(false /*is_tablet_mode*/);
  // TODO(newcomer): this test needs to be reevaluated for the fullscreen app
  // list (http://crbug.com/759779).
  EXPECT_FALSE(view_->GetWidget()->IsVisible());
  EXPECT_EQ(-1, GetPaginationModel()->total_pages());
  AppListTestModel* model = delegate_->GetTestModel();
  model->PopulateApps(3);

  Show();

  AppListMainView* main_view = view_->app_list_main_view();
  ContentsView* contents_view = main_view->contents_view();
  EXPECT_TRUE(SetAppListState(ash::AppListState::kStateApps));

  // Show the search results.
  contents_view->ShowSearchResults(true);
  views::test::RunScheduledLayout(contents_view);
  EXPECT_TRUE(
      contents_view->IsStateActive(ash::AppListState::kStateSearchResults));

  EXPECT_TRUE(IsStateShown(ash::AppListState::kStateSearchResults));

  // Hide the search results.
  contents_view->ShowSearchResults(false);
  views::test::RunScheduledLayout(contents_view);

  // Check that we return to the page that we were on before the search.
  EXPECT_TRUE(IsStateShown(ash::AppListState::kStateApps));

  views::test::RunScheduledLayout(view_);
  EXPECT_TRUE(IsStateShown(ash::AppListState::kStateApps));

  std::u16string search_text = u"test";
  main_view->search_box_view()->search_box()->SetText(std::u16string());
  main_view->search_box_view()->search_box()->InsertText(
      search_text,
      ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  // Check that the current search is using |search_text|.
  EXPECT_EQ(search_text, main_view->search_box_view()->search_box()->GetText());
  EXPECT_EQ(search_text, main_view->search_box_view()->current_query());
  views::test::RunScheduledLayout(contents_view);
  EXPECT_TRUE(
      contents_view->IsStateActive(ash::AppListState::kStateSearchResults));
  EXPECT_TRUE(CheckSearchBoxView(contents_view->GetSearchBoxBounds(
      ash::AppListState::kStateSearchResults)));

  // Check that typing into the search box triggers the search page.
  EXPECT_TRUE(SetAppListState(ash::AppListState::kStateApps));
  views::test::RunScheduledLayout(contents_view);
  EXPECT_TRUE(IsStateShown(ash::AppListState::kStateApps));
  EXPECT_TRUE(CheckSearchBoxView(
      contents_view->GetSearchBoxBounds(ash::AppListState::kStateApps)));

  std::u16string new_search_text = u"apple";
  main_view->search_box_view()->search_box()->SetText(std::u16string());
  main_view->search_box_view()->search_box()->InsertText(
      new_search_text,
      ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  // Check that the current search is using |new_search_text|.
  EXPECT_EQ(new_search_text,
            main_view->search_box_view()->search_box()->GetText());
  EXPECT_EQ(search_text, main_view->search_box_view()->current_query());
  views::test::RunScheduledLayout(contents_view);
  EXPECT_TRUE(IsStateShown(ash::AppListState::kStateSearchResults));
  EXPECT_TRUE(CheckSearchBoxView(contents_view->GetSearchBoxBounds(
      ash::AppListState::kStateSearchResults)));
}

TEST_F(AppListViewTest, ShowContextMenuBetweenAppsInTabletMode) {
  Initialize(/*is_tablet_mode=*/true);
  delegate_->GetTestModel()->PopulateApps(kInitialItems);
  Show();

  // Tap between two apps in tablet mode.
  const gfx::Point middle = GetPointBetweenTwoApps();
  ui::GestureEvent tap(
      middle.x(), middle.y(), 0, base::TimeTicks(),
      ui::GestureEventDetails(ui::EventType::kGestureTwoFingerTap));
  view_->OnGestureEvent(&tap);

  // The wallpaper context menu should show.
  EXPECT_EQ(1, show_wallpaper_context_menu_count());
  EXPECT_TRUE(view_->GetWidget()->IsVisible());

  // Click between two apps in tablet mode.
  ui::MouseEvent click_mouse_event(
      ui::EventType::kMousePressed, middle, middle, ui::EventTimeForNow(),
      ui::EF_RIGHT_MOUSE_BUTTON, ui::EF_RIGHT_MOUSE_BUTTON);
  view_->OnMouseEvent(&click_mouse_event);
  ui::MouseEvent release_mouse_event(
      ui::EventType::kMouseReleased, middle, middle, ui::EventTimeForNow(),
      ui::EF_RIGHT_MOUSE_BUTTON, ui::EF_RIGHT_MOUSE_BUTTON);
  view_->OnMouseEvent(&release_mouse_event);

  // The wallpaper context menu should show.
  EXPECT_EQ(2, show_wallpaper_context_menu_count());
  EXPECT_TRUE(view_->GetWidget()->IsVisible());
}

// Tests the back action in home launcher.
TEST_F(AppListViewTest, BackAction) {
  // Put into fullscreen using tablet mode.
  Initialize(/*is_tablet_mode=*/true);

  // Populate apps to fill up the first page and add a folder in the second
  // page.
  AppListTestModel* model = delegate_->GetTestModel();
  const int kAppListItemNum =
      SharedAppListConfig::instance().GetMaxNumOfItemsPerPage();
  const int kItemNumInFolder = 5;
  model->PopulateApps(kAppListItemNum);
  model->CreateAndPopulateFolderWithApps(kItemNumInFolder);

  // Show the app list
  Show();
  EXPECT_EQ(ash::AppListViewState::kFullscreenAllApps, view_->app_list_state());
  EXPECT_EQ(2, apps_grid_view()->pagination_model()->total_pages());

  // Select the second page and open the folder.
  apps_grid_view()->pagination_model()->SelectPage(1, false);
  test_api_->PressItemAt(kAppListItemNum);
  EXPECT_TRUE(contents_view()->apps_container_view()->IsInFolderView());
  EXPECT_EQ(1, apps_grid_view()->pagination_model()->selected_page());

  // Back action will first close the folder.
  contents_view()->Back();
  EXPECT_FALSE(contents_view()->apps_container_view()->IsInFolderView());
  EXPECT_EQ(1, apps_grid_view()->pagination_model()->selected_page());

  // Back action will then select the first page.
  contents_view()->Back();
  EXPECT_FALSE(contents_view()->apps_container_view()->IsInFolderView());
  EXPECT_EQ(0, apps_grid_view()->pagination_model()->selected_page());

  // Select the second page and open search results page.
  apps_grid_view()->pagination_model()->SelectPage(1, false);
  search_box_view()->search_box()->InsertText(
      u"A",
      ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  EXPECT_EQ(ash::AppListViewState::kFullscreenSearch, view_->app_list_state());
  EXPECT_EQ(1, apps_grid_view()->pagination_model()->selected_page());

  // Back action will first close the search results page.
  contents_view()->Back();
  EXPECT_EQ(ash::AppListViewState::kFullscreenAllApps, view_->app_list_state());
  EXPECT_EQ(1, apps_grid_view()->pagination_model()->selected_page());

  // Back action will then select the first page.
  contents_view()->Back();
  EXPECT_EQ(ash::AppListViewState::kFullscreenAllApps, view_->app_list_state());
  EXPECT_EQ(0, apps_grid_view()->pagination_model()->selected_page());
}

TEST_F(AppListViewTest, CloseFolderByClickingBackground) {
  Initialize(/*is_tablet_mode=*/false);

  AppListTestModel* model = delegate_->GetTestModel();
  model->CreateAndPopulateFolderWithApps(3);
  EXPECT_EQ(1u, model->top_level_item_list()->item_count());
  EXPECT_EQ(AppListFolderItem::kItemType,
            model->top_level_item_list()->item_at(0)->GetItemType());

  // Open the folder.
  Show();
  test_api_->PressItemAt(0);

  AppsContainerView* apps_container_view =
      contents_view()->apps_container_view();
  EXPECT_TRUE(apps_container_view->IsInFolderView());

  // Simulate mouse press on folder background to close the folder.
  ui::MouseEvent event(ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
                       ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                       ui::EF_LEFT_MOUSE_BUTTON);
  apps_container_view->folder_background_view()->OnMouseEvent(&event);
  EXPECT_FALSE(apps_container_view->IsInFolderView());
}

// Tests selecting search result to show embedded Assistant UI.
TEST_P(AppListViewFocusTest, ShowEmbeddedAssistantUI) {
  Show();

  // Initially the search box is inactive, hitting Enter to activate it.
  EXPECT_FALSE(search_box_view()->is_search_box_active());
  SimulateKeyPress(ui::VKEY_RETURN, false);
  EXPECT_TRUE(search_box_view()->is_search_box_active());

  // Type something in search box to transition to search state and populate
  // fake list results. Then hit Enter key.
  search_box_view()->search_box()->InsertText(
      u"test",
      ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  const int kListResults = 2;

  SetUpSearchResults(kListResults);
  SimulateKeyPress(ui::VKEY_RETURN, false);
  EXPECT_EQ(1, GetOpenFirstSearchResultCount());
  EXPECT_EQ(1, GetTotalOpenSearchResultCount());

  // Type something in search box to transition to re-open search state and
  // populate fake list results. Then hit Enter key.
  search_box_view()->search_box()->InsertText(
      u"test",
      ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  SetUpSearchResults(kListResults);
  SimulateKeyPress(ui::VKEY_DOWN, false);
  SimulateKeyPress(ui::VKEY_RETURN, false);
  EXPECT_EQ(1, GetOpenFirstSearchResultCount());
  EXPECT_EQ(2, GetTotalOpenSearchResultCount());
}

// Tests that pressing escape in embedded Assistant UI returns to fullscreen
// if the Assistant UI was launched from fullscreen app list.
TEST_F(AppListViewTest, EscapeKeyInEmbeddedAssistantUIReturnsToAppList) {
  Initialize(false /*is_tablet_mode*/);
  Show();

  // Enter search view by entering text
  SetTextInSearchBox(u"search query");
  // From there we launch the Assistant UI
  contents_view()->ShowEmbeddedAssistantUI(true);

  // We press escape to leave the Assistant UI
  view_->AcceleratorPressed(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));

  // And we should be back in the fullscreen app list
  EXPECT_FALSE(contents_view()->IsShowingSearchResults());
  EXPECT_EQ(ash::AppListViewState::kFullscreenAllApps, view_->app_list_state());
}

// Tests that search box is not visible when showing embedded Assistant UI.
// ProductivityLauncher has tests for this in AppListBubbleViewTest.
TEST_F(AppListViewTest, SearchBoxViewNotVisibleInEmbeddedAssistantUI) {
  Initialize(/*is_tablet_mode=*/true);
  Show();

  EXPECT_TRUE(search_box_view()->GetVisible());

  contents_view()->ShowEmbeddedAssistantUI(true);

  EXPECT_TRUE(contents_view()->IsShowingEmbeddedAssistantUI());
  EXPECT_FALSE(search_box_view()->GetVisible());
}

TEST_F(AppListViewScalableLayoutTest, RegularLandscapeScreen) {
  const gfx::Size window_size = gfx::Size(1000, 800);
  GetContext()->SetBounds(gfx::Rect(window_size));

  InitializeAppList();

  const int expected_horizontal_margin = kMinLauncherGridHorizontalMargin;
  const gfx::Size expected_item_size(96, 120);

  {
    SCOPED_TRACE("Only apps grid");
    EXPECT_EQ(4, apps_grid_view()->GetRowsForTesting());
    VerifyAppsContainerLayout(window_size, /*row_count=*/4,
                              expected_horizontal_margin, expected_item_size,
                              /*has_recent_apps=*/false);
  }

  AddRecentApps(4);
  contents_view()->ResetForShow();

  {
    SCOPED_TRACE("With recent apps");
    EXPECT_EQ(4, apps_grid_view()->GetRowsForTesting());
    VerifyAppsContainerLayout(window_size, 3 /*row_count*/,
                              expected_horizontal_margin, expected_item_size,
                              /*has_recent_apps=*/true);
  }
}

TEST_F(AppListViewScalableLayoutTest,
       RegularLandscapeScreenAtMinPreferredVerticalMargin) {
  const int window_height = GetExpectedScreenSize(
      /*row_count=*/4, /*tile_height=*/120, /*tile_margins=*/8,
      /*is_large_height=*/false);
  EXPECT_EQ(689, window_height);
  const gfx::Size window_size = gfx::Size(800, window_height);
  GetContext()->SetBounds(gfx::Rect(window_size));

  InitializeAppList();

  const int expected_horizontal_margin = kMinLauncherGridHorizontalMargin;
  const gfx::Size expected_item_size(96, 120);

  {
    SCOPED_TRACE("Only apps grid");
    EXPECT_EQ(4, apps_grid_view()->GetRowsForTesting());
    VerifyAppsContainerLayout(window_size, /*row_count=*/4,
                              expected_horizontal_margin, expected_item_size,
                              /*has_recent_apps=*/false);
  }

  AddRecentApps(4);
  contents_view()->ResetForShow();

  {
    SCOPED_TRACE("With recent apps");
    EXPECT_EQ(4, apps_grid_view()->GetRowsForTesting());
    VerifyAppsContainerLayout(window_size, 2 /*row_count*/,
                              expected_horizontal_margin, expected_item_size,
                              /*has_recent_apps=*/true);
  }
}

TEST_F(AppListViewScalableLayoutTest, RegularLandscapeScreenWithRemovedRows) {
  const int window_height = GetExpectedScreenSize(
                                /*row_count=*/4, /*tile_height=*/120,
                                /*tile_margins=*/8, /*is_large_height=*/false) -
                            4;
  EXPECT_EQ(685, window_height);
  const gfx::Size window_size = gfx::Size(800, window_height);
  GetContext()->SetBounds(gfx::Rect(window_size));

  InitializeAppList();

  const int expected_horizontal_margin = kMinLauncherGridHorizontalMargin;
  const gfx::Size expected_item_size(96, 120);

  {
    SCOPED_TRACE("Only apps grid");
    EXPECT_EQ(3, apps_grid_view()->GetRowsForTesting());
    VerifyAppsContainerLayout(window_size, 3 /*row_count*/,
                              expected_horizontal_margin, expected_item_size,
                              /*has_recent_apps=*/false);
  }

  AddRecentApps(4);
  contents_view()->ResetForShow();

  {
    SCOPED_TRACE("With recent apps");
    EXPECT_EQ(3, apps_grid_view()->GetRowsForTesting());
    VerifyAppsContainerLayout(window_size, 2 /*row_count*/,
                              expected_horizontal_margin, expected_item_size,
                              /*has_recent_apps=*/true);
  }
}

TEST_F(AppListViewScalableLayoutTest,
       RegularLandscapeScreenAtMaxPreferredVerticalMargin) {
  const int window_height = GetExpectedScreenSize(
      /*row_count=*/4, /*tile_height=*/120, /*tile_margins=*/96,
      /*is_large_height=*/true);
  EXPECT_EQ(1024, window_height);
  const gfx::Size window_size = gfx::Size(1100, window_height);
  GetContext()->SetBounds(gfx::Rect(window_size));

  InitializeAppList();

  const int expected_horizontal_margin = kMinLauncherGridHorizontalMargin;
  const gfx::Size expected_item_size(96, 120);

  {
    SCOPED_TRACE("Only apps grid");
    EXPECT_EQ(4, apps_grid_view()->GetRowsForTesting());
    VerifyAppsContainerLayout(window_size, /*row_count=*/4,
                              expected_horizontal_margin, expected_item_size,
                              /*has_recent_apps=*/false);
  }

  AddRecentApps(4);
  contents_view()->ResetForShow();

  {
    SCOPED_TRACE("With recent apps");
    EXPECT_EQ(4, apps_grid_view()->GetRowsForTesting());
    VerifyAppsContainerLayout(window_size, 4 /*row_count*/,
                              expected_horizontal_margin, expected_item_size,
                              /*has_recent_apps=*/true);
  }
}

TEST_F(AppListViewScalableLayoutTest, RegularLandscapeScreenWithAddedRows) {
  const int window_height = GetExpectedScreenSize(
                                /*row_count=*/4, /*tile_height=*/120,
                                /*tile_margins=*/96, /*is_large_height=*/true) +
                            6;
  EXPECT_EQ(1030, window_height);
  const gfx::Size window_size = gfx::Size(1100, window_height);
  GetContext()->SetBounds(gfx::Rect(window_size));

  InitializeAppList();

  const int expected_horizontal_margin = kMinLauncherGridHorizontalMargin;
  const gfx::Size expected_item_size(96, 120);

  {
    SCOPED_TRACE("Only apps grid");
    EXPECT_EQ(5, apps_grid_view()->GetRowsForTesting());
    VerifyAppsContainerLayout(window_size, /*row_count=*/5,
                              expected_horizontal_margin, expected_item_size,
                              /*has_recent_apps=*/false);
  }

  AddRecentApps(4);
  contents_view()->ResetForShow();

  {
    SCOPED_TRACE("With recent apps");
    EXPECT_EQ(5, apps_grid_view()->GetRowsForTesting());
    VerifyAppsContainerLayout(window_size, 4 /*row_count*/,
                              expected_horizontal_margin, expected_item_size,
                              /*has_recent_apps=*/true);
  }
}

TEST_F(AppListViewScalableLayoutTest, RegularPortraitScreen) {
  const gfx::Size window_size = gfx::Size(800, 1000);
  GetContext()->SetBounds(gfx::Rect(window_size));

  InitializeAppList();

  const int expected_horizontal_margin = kMinLauncherGridHorizontalMargin;
  const gfx::Size expected_item_size(96, 120);

  {
    SCOPED_TRACE("Only apps grid");
    EXPECT_EQ(5, apps_grid_view()->GetRowsForTesting());
    VerifyAppsContainerLayout(window_size, /*row_count=*/5,
                              expected_horizontal_margin, expected_item_size,
                              /*has_recent_apps=*/false);
  }

  AddRecentApps(4);
  contents_view()->ResetForShow();

  {
    SCOPED_TRACE("With recent apps");
    EXPECT_EQ(5, apps_grid_view()->GetRowsForTesting());
    VerifyAppsContainerLayout(window_size, /*row_count=*/4,
                              expected_horizontal_margin, expected_item_size,
                              /*has_recent_apps=*/true);
  }
}

TEST_F(AppListViewScalableLayoutTest,
       RegularPortraitScreenAtMinPreferredVerticalMargin) {
  int window_height = GetExpectedScreenSize(
      /*row_count=*/5, /*tile_height=*/120, /*tile_margins=*/8,
      /*is_large_height=*/true);
  // window_height = 860;
  EXPECT_EQ(868, window_height);
  const gfx::Size window_size = gfx::Size(700, window_height);
  GetContext()->SetBounds(gfx::Rect(window_size));

  InitializeAppList();

  const int expected_horizontal_margin = kMinLauncherGridHorizontalMargin;
  const gfx::Size expected_item_size(96, 120);

  {
    SCOPED_TRACE("Only apps grid");
    EXPECT_EQ(5, apps_grid_view()->GetRowsForTesting());
    VerifyAppsContainerLayout(window_size, /*row_count=*/5,
                              expected_horizontal_margin, expected_item_size,
                              /*has_recent_apps=*/false);
  }

  AddRecentApps(4);
  contents_view()->ResetForShow();

  {
    SCOPED_TRACE("With recent apps");
    EXPECT_EQ(5, apps_grid_view()->GetRowsForTesting());
    VerifyAppsContainerLayout(window_size, 3 /*row_count*/,
                              expected_horizontal_margin, expected_item_size,
                              /*has_recent_apps=*/true);
  }
}

TEST_F(AppListViewScalableLayoutTest, RegularPortraitScreenWithRemovedRows) {
  const int window_height =
      GetExpectedScreenSize(
          /*row_count=*/5, /*tile_height=*/120, /*tile_margins=*/8,
          /*is_large_height=*/true) -
      8;
  EXPECT_EQ(860, window_height);
  const gfx::Size window_size = gfx::Size(700, window_height);
  GetContext()->SetBounds(gfx::Rect(window_size));

  InitializeAppList();

  const int expected_horizontal_margin = kMinLauncherGridHorizontalMargin;
  const gfx::Size expected_item_size(96, 120);

  {
    SCOPED_TRACE("Only apps grid");
    EXPECT_EQ(4, apps_grid_view()->GetRowsForTesting());
    VerifyAppsContainerLayout(window_size, /*row_count=*/4,
                              expected_horizontal_margin, expected_item_size,
                              /*has_recent_apps=*/false);
  }

  AddRecentApps(4);
  contents_view()->ResetForShow();

  {
    SCOPED_TRACE("With recent apps");
    EXPECT_EQ(4, apps_grid_view()->GetRowsForTesting());
    VerifyAppsContainerLayout(window_size, 3 /*row_count*/,
                              expected_horizontal_margin, expected_item_size,
                              /*has_recent_apps=*/true);
  }
}

TEST_F(AppListViewScalableLayoutTest,
       RegularPortraitScreenAtMaxPreferredVerticalMargin) {
  const int window_height = GetExpectedScreenSize(
      /*row_count=*/5, /*tile_height=*/120, /*tile_margins=*/96,
      /*is_large_height=*/true);
  EXPECT_EQ(1270, window_height);
  const gfx::Size window_size = gfx::Size(1200, window_height);
  GetContext()->SetBounds(gfx::Rect(window_size));

  InitializeAppList();

  const int expected_horizontal_margin = 104;
  const gfx::Size expected_item_size(96, 120);

  {
    SCOPED_TRACE("Only apps grid");
    EXPECT_EQ(5, apps_grid_view()->GetRowsForTesting());
    VerifyAppsContainerLayout(window_size, /*row_count=*/5,
                              expected_horizontal_margin, expected_item_size,
                              /*has_recent_apps=*/false);
  }

  AddRecentApps(4);
  contents_view()->ResetForShow();

  {
    SCOPED_TRACE("With recent apps");
    EXPECT_EQ(5, apps_grid_view()->GetRowsForTesting());
    VerifyAppsContainerLayout(window_size, /*row_count=*/4,
                              expected_horizontal_margin, expected_item_size,
                              /*has_recent_apps=*/true);
  }
}

TEST_F(AppListViewScalableLayoutTest, RegularPortraitScreenWithExtraRows) {
  const int window_height =
      GetExpectedScreenSize(
          /*row_count=*/5, /*tile_height=*/120, /*tile_margins=*/96,
          /*is_large_height=*/true) +
      4;
  EXPECT_EQ(1274, window_height);
  const gfx::Size window_size = gfx::Size(1200, window_height);
  GetContext()->SetBounds(gfx::Rect(window_size));

  InitializeAppList();

  const int expected_horizontal_margin = 104;
  const gfx::Size expected_item_size(96, 120);

  {
    SCOPED_TRACE("Only apps grid");
    EXPECT_EQ(6, apps_grid_view()->GetRowsForTesting());
    VerifyAppsContainerLayout(window_size, /*row_count=*/6,
                              expected_horizontal_margin, expected_item_size,
                              /*has_recent_apps=*/false);
  }

  AddRecentApps(4);
  contents_view()->ResetForShow();

  {
    SCOPED_TRACE("With recent apps");
    EXPECT_EQ(6, apps_grid_view()->GetRowsForTesting());
    VerifyAppsContainerLayout(window_size, /*row_count=*/4,
                              expected_horizontal_margin, expected_item_size,
                              /*has_recent_apps=*/true);
  }
}

TEST_F(AppListViewScalableLayoutTest, DenseLandscapeScreen) {
  const gfx::Size window_size = gfx::Size(800, 600);
  GetContext()->SetBounds(gfx::Rect(window_size));

  InitializeAppList();

  const int expected_horizontal_margin = kMinLauncherGridHorizontalMargin;
  const gfx::Size expected_item_size(80, 88);

  {
    SCOPED_TRACE("Only apps grid");
    EXPECT_EQ(4, apps_grid_view()->GetRowsForTesting());
    VerifyAppsContainerLayout(window_size, /*row_count=*/4,
                              expected_horizontal_margin, expected_item_size,
                              /*has_recent_apps=*/false);
  }

  AddRecentApps(4);
  contents_view()->ResetForShow();

  {
    SCOPED_TRACE("With recent apps");
    EXPECT_EQ(4, apps_grid_view()->GetRowsForTesting());
    VerifyAppsContainerLayout(window_size, 3 /*row_count*/,
                              expected_horizontal_margin, expected_item_size,
                              /*has_recent_apps=*/true);
  }
}

TEST_F(AppListViewScalableLayoutTest,
       DenseLandscapeScreenAtMinPreferredVerticalMargin) {
  const int window_height = GetExpectedScreenSize(
      /*row_count=*/4, /*tile_height=*/88, /*tile_margins=*/8,
      /*is_large_height=*/false);
  EXPECT_EQ(552, window_height);
  const gfx::Size window_size = gfx::Size(800, window_height);
  GetContext()->SetBounds(gfx::Rect(window_size));

  InitializeAppList();

  const int expected_horizontal_margin = kMinLauncherGridHorizontalMargin;
  const gfx::Size expected_item_size(80, 88);

  {
    SCOPED_TRACE("Only apps grid");
    EXPECT_EQ(4, apps_grid_view()->GetRowsForTesting());
    VerifyAppsContainerLayout(window_size, /*row_count=*/4,
                              expected_horizontal_margin, expected_item_size,
                              /*has_recent_apps=*/false);
  }

  AddRecentApps(4);
  contents_view()->ResetForShow();

  {
    SCOPED_TRACE("With recent apps");
    EXPECT_EQ(4, apps_grid_view()->GetRowsForTesting());
    VerifyAppsContainerLayout(window_size, 2 /*row_count*/,
                              expected_horizontal_margin, expected_item_size,
                              /*has_recent_apps=*/true);
  }
}

TEST_F(AppListViewScalableLayoutTest, DenseLandscapeScreenWithRemovedRows) {
  const int window_height =
      GetExpectedScreenSize(
          /*row_count=*/4, /*tile_height=*/88, /*tile_margins=*/8,
          /*large_height*/ false) -
      4;
  EXPECT_EQ(548, window_height);
  const gfx::Size window_size = gfx::Size(800, window_height);
  GetContext()->SetBounds(gfx::Rect(window_size));

  InitializeAppList();

  const int expected_horizontal_margin = kMinLauncherGridHorizontalMargin;
  const gfx::Size expected_item_size(80, 88);

  {
    SCOPED_TRACE("Only apps grid");
    EXPECT_EQ(3, apps_grid_view()->GetRowsForTesting());
    VerifyAppsContainerLayout(window_size, 3 /*row_count*/,
                              expected_horizontal_margin, expected_item_size,
                              /*has_recent_apps=*/false);
  }

  AddRecentApps(4);
  contents_view()->ResetForShow();

  {
    SCOPED_TRACE("With recent apps");
    EXPECT_EQ(3, apps_grid_view()->GetRowsForTesting());
    VerifyAppsContainerLayout(window_size, 2 /*row_count*/,
                              expected_horizontal_margin, expected_item_size,
                              /*has_recent_apps=*/true);
  }
}

TEST_F(AppListViewScalableLayoutTest, DensePortraitScreen) {
  const gfx::Size window_size = gfx::Size(600, 800);
  GetContext()->SetBounds(gfx::Rect(window_size));

  InitializeAppList();

  const int expected_horizontal_margin = kMinLauncherGridHorizontalMargin;
  const gfx::Size expected_item_size(80, 88);

  {
    SCOPED_TRACE("Only apps grid");
    EXPECT_EQ(5, apps_grid_view()->GetRowsForTesting());
    VerifyAppsContainerLayout(window_size, /*row_count=*/5,
                              expected_horizontal_margin, expected_item_size,
                              /*has_recent_apps=*/false);
  }

  AddRecentApps(4);
  contents_view()->ResetForShow();

  {
    SCOPED_TRACE("With recent apps");
    EXPECT_EQ(5, apps_grid_view()->GetRowsForTesting());
    VerifyAppsContainerLayout(window_size, /*row_count=*/4,
                              expected_horizontal_margin, expected_item_size,
                              /*has_recent_apps=*/true);
  }
}

TEST_F(AppListViewScalableLayoutTest,
       DensePortraitScreenAtMinPreferredVerticalMargin) {
  const int window_height = GetExpectedScreenSize(
      /*row_count=*/5, /*tile_height=*/88, /*tile_margins=*/8,
      /*large_height*/ false);
  EXPECT_EQ(654, window_height);
  const gfx::Size window_size = gfx::Size(600, window_height);
  GetContext()->SetBounds(gfx::Rect(window_size));

  InitializeAppList();

  const int expected_horizontal_margin = kMinLauncherGridHorizontalMargin;
  const gfx::Size expected_item_size(80, 88);

  {
    SCOPED_TRACE("Only apps grid");
    EXPECT_EQ(5, apps_grid_view()->GetRowsForTesting());
    VerifyAppsContainerLayout(window_size, /*row_count=*/5,
                              expected_horizontal_margin, expected_item_size,
                              /*has_recent_apps=*/false);
  }

  AddRecentApps(4);
  contents_view()->ResetForShow();

  {
    SCOPED_TRACE("With recent apps");
    EXPECT_EQ(5, apps_grid_view()->GetRowsForTesting());
    VerifyAppsContainerLayout(window_size, 3 /*row_count*/,
                              expected_horizontal_margin, expected_item_size,
                              /*has_recent_apps=*/true);
  }
}

TEST_F(AppListViewScalableLayoutTest, DensePortraitScreenWithRemovedRows) {
  const int window_height = GetExpectedScreenSize(
                                /*row_count=*/5, /*tile_height=*/88,
                                /*tile_margins=*/8, /*large_height*/ false) -
                            8;
  EXPECT_EQ(646, window_height);
  const gfx::Size window_size = gfx::Size(540, window_height);
  GetContext()->SetBounds(gfx::Rect(window_size));

  InitializeAppList();

  const int expected_horizontal_margin = kMinLauncherGridHorizontalMargin;
  const gfx::Size expected_item_size(80, 88);

  {
    SCOPED_TRACE("Only apps grid");
    EXPECT_EQ(4, apps_grid_view()->GetRowsForTesting());
    VerifyAppsContainerLayout(window_size, /*row_count=*/4,
                              expected_horizontal_margin, expected_item_size,
                              /*has_recent_apps=*/false);
  }

  AddRecentApps(4);
  contents_view()->ResetForShow();

  {
    SCOPED_TRACE("With recent apps");
    EXPECT_EQ(4, apps_grid_view()->GetRowsForTesting());
    VerifyAppsContainerLayout(window_size, 3 /*row_count*/,
                              expected_horizontal_margin, expected_item_size,
                              /*has_recent_apps=*/true);
  }
}

TEST_F(AppListViewScalableLayoutTest,
       DensePortraitScreenAtMaxPreferredVerticalMargin) {
  const int window_height = GetExpectedScreenSize(
      /*row_count=*/5, /*tile_height=*/88, /*tile_margins=*/96,
      /*large_height*/ true);
  EXPECT_EQ(1088, window_height);
  const gfx::Size window_size = gfx::Size(601, window_height);
  GetContext()->SetBounds(gfx::Rect(window_size));

  InitializeAppList();

  const int expected_horizontal_margin = kMinLauncherGridHorizontalMargin;
  const gfx::Size expected_item_size(80, 88);

  {
    SCOPED_TRACE("Only apps grid");
    EXPECT_EQ(5, apps_grid_view()->GetRowsForTesting());
    VerifyAppsContainerLayout(window_size, /*row_count=*/5,
                              expected_horizontal_margin, expected_item_size,
                              /*has_recent_apps=*/false);
  }

  AddRecentApps(4);
  contents_view()->ResetForShow();

  {
    SCOPED_TRACE("With recent apps");
    EXPECT_EQ(5, apps_grid_view()->GetRowsForTesting());
    VerifyAppsContainerLayout(window_size, /*row_count=*/4,
                              expected_horizontal_margin, expected_item_size,
                              /*has_recent_apps=*/true);
  }
}

TEST_F(AppListViewScalableLayoutTest, DensePortraitScreenWithExtraRows) {
  const int window_height = GetExpectedScreenSize(
                                /*row_count=*/5, /*tile_height=*/88,
                                /*tile_margins=*/96, /*large_height*/ true) +
                            4;
  EXPECT_EQ(1092, window_height);
  const gfx::Size window_size = gfx::Size(601, window_height);
  GetContext()->SetBounds(gfx::Rect(window_size));

  InitializeAppList();

  const int expected_horizontal_margin = kMinLauncherGridHorizontalMargin;
  const gfx::Size expected_item_size(80, 88);

  {
    SCOPED_TRACE("Only apps grid");
    EXPECT_EQ(6, apps_grid_view()->GetRowsForTesting());
    VerifyAppsContainerLayout(window_size, /*row_count=*/6,
                              expected_horizontal_margin, expected_item_size,
                              /*has_recent_apps=*/false);
  }

  AddRecentApps(4);
  contents_view()->ResetForShow();

  {
    SCOPED_TRACE("With recent apps");
    EXPECT_EQ(6, apps_grid_view()->GetRowsForTesting());
    VerifyAppsContainerLayout(window_size, /*row_count=*/4,
                              expected_horizontal_margin, expected_item_size,
                              /*has_recent_apps=*/true);
  }
}

TEST_F(AppListViewScalableLayoutTest,
       DenseAppsGridPaddingScaledDownToMakeRoomForPageSwitcher) {
  // Select window width so using non-zero horizontal padding would result in
  // lack of space for the page switcher.
  const gfx::Size window_size = gfx::Size(512, 800);
  GetContext()->SetBounds(gfx::Rect(window_size));

  InitializeAppList();

  const int expected_horizontal_margin = kMinLauncherGridHorizontalMargin;
  const gfx::Size expected_item_size(80, 88);
  EXPECT_EQ(5, apps_grid_view()->GetRowsForTesting());
  VerifyAppsContainerLayout(window_size, /*row_count=*/5,
                            expected_horizontal_margin, expected_item_size,
                            /*has_recent_apps=*/false);
}

TEST_F(AppListViewScalableLayoutTest,
       DenseAppsGridScaledDownToMakeRoomForPageSwitcher) {
  // Select window width so using default icon width would result in lack of
  // space for the page switcher.
  const gfx::Size window_size = gfx::Size(442, 800);
  GetContext()->SetBounds(gfx::Rect(window_size));

  InitializeAppList();

  const int expected_horizontal_margin = kMinLauncherGridHorizontalMargin;
  const gfx::Size expected_item_size(66, 88);
  EXPECT_EQ(5, apps_grid_view()->GetRowsForTesting());
  VerifyAppsContainerLayout(window_size, /*row_count=*/5,
                            expected_horizontal_margin, expected_item_size,
                            /*has_recent_apps=*/false);
}

TEST_F(AppListViewScalableLayoutTest,
       DenseAppsGridWithMaxHorizontalItemMargins) {
  // Select window width that results in apps grid layout with max allowed
  // horizontal margin (128): 2 * 56 (min horizontal margin) + 4 * 128 + 5 * 80
  const gfx::Size window_size = gfx::Size(984, 600);
  GetContext()->SetBounds(gfx::Rect(window_size));

  InitializeAppList();

  const int expected_horizontal_margin = kMinLauncherGridHorizontalMargin;
  const gfx::Size expected_item_size(80, 88);
  EXPECT_EQ(4, apps_grid_view()->GetRowsForTesting());
  VerifyAppsContainerLayout(window_size, /*row_count=*/4,
                            expected_horizontal_margin, expected_item_size,
                            /*has_recent_apps=*/false);
}

TEST_F(AppListViewScalableLayoutTest,
       DenseAppsGridHorizontalItemMarginsBounded) {
  // Select window width that results in apps grid layout with max allowed
  // horizontal margin (128), i.e. larger than
  // 2 * 56 (min horizontal margin) + 4 * 128 + 5 * 80
  const gfx::Size window_size = gfx::Size(1040, 600);
  GetContext()->SetBounds(gfx::Rect(window_size));

  InitializeAppList();

  const int expected_horizontal_margin = 64;
  const gfx::Size expected_item_size(80, 88);
  EXPECT_EQ(4, apps_grid_view()->GetRowsForTesting());
  VerifyAppsContainerLayout(window_size, /*row_count=*/4,
                            expected_horizontal_margin, expected_item_size,
                            /*has_recent_apps=*/false);
}

TEST_F(AppListViewScalableLayoutTest,
       RegularAppsGridWithMaxHorizontalItemMargins) {
  // Select window width that results in apps grid layout with max allowed
  // horizontal margin (128):
  // 2 * 56 (min horizontal margin) + 4 * 128 + 5 * 96
  const gfx::Size window_size = gfx::Size(1104, 1200);
  GetContext()->SetBounds(gfx::Rect(window_size));

  InitializeAppList();

  const int expected_horizontal_margin = kMinLauncherGridHorizontalMargin;
  const gfx::Size expected_item_size(96, 120);
  EXPECT_EQ(5, apps_grid_view()->GetRowsForTesting());
  VerifyAppsContainerLayout(window_size, /*row_count=*/5,
                            expected_horizontal_margin, expected_item_size,
                            /*has_recent_apps=*/false);
}

TEST_F(AppListViewScalableLayoutTest,
       RegularAppsGridHorizontalItemMarginsBounded) {
  // Select window width that results in apps grid layout with max allowed
  // horizontal margin (128), i.e. larger than
  // 2 * 56 (min horizontal margin) + 4 * 128 + 5 * 96
  const gfx::Size window_size = gfx::Size(1116, 1200);
  GetContext()->SetBounds(gfx::Rect(window_size));

  InitializeAppList();

  const int expected_horizontal_margin = 62;
  const gfx::Size expected_item_size(96, 120);
  EXPECT_EQ(5, apps_grid_view()->GetRowsForTesting());
  VerifyAppsContainerLayout(window_size, /*row_count=*/5,
                            expected_horizontal_margin, expected_item_size,
                            /*has_recent_apps=*/false);
}

TEST_F(AppListViewScalableLayoutTest, LayoutAfterConfigChange) {
  const gfx::Size window_size = gfx::Size(600, 800);
  GetContext()->SetBounds(gfx::Rect(window_size));

  InitializeAppList();

  const int expected_horizontal_margin = kMinLauncherGridHorizontalMargin;
  const gfx::Size expected_item_size(80, 88);
  EXPECT_EQ(5, apps_grid_view()->GetRowsForTesting());
  VerifyAppsContainerLayout(window_size, /*row_count=*/5,
                            expected_horizontal_margin, expected_item_size,
                            /*has_recent_apps=*/false);

  const gfx::Size updated_window_size = gfx::Size(1000, 800);
  GetContext()->SetBounds(gfx::Rect(updated_window_size));
  view_->OnParentWindowBoundsChanged();

  const gfx::Size expected_updated_item_size(96, 120);
  EXPECT_EQ(4, apps_grid_view()->GetRowsForTesting());
  VerifyAppsContainerLayout(
      updated_window_size, /*row_count=*/4, expected_horizontal_margin,
      expected_updated_item_size, /*has_recent_apps=*/false);
}

TEST_F(AppListViewScalableLayoutTest, LayoutAfterConfigChangeWithRecentApps) {
  const gfx::Size window_size = gfx::Size(600, 800);
  GetContext()->SetBounds(gfx::Rect(window_size));

  InitializeAppList();
  AddRecentApps(4);
  contents_view()->ResetForShow();

  const int expected_horizontal_margin = kMinLauncherGridHorizontalMargin;
  const gfx::Size expected_item_size(80, 88);
  EXPECT_EQ(5, apps_grid_view()->GetRowsForTesting());
  VerifyAppsContainerLayout(window_size, /*row_count=*/4,
                            expected_horizontal_margin, expected_item_size,
                            /*has_recent_apps=*/true);

  const gfx::Size updated_window_size = gfx::Size(1000, 800);
  GetContext()->SetBounds(gfx::Rect(updated_window_size));
  view_->OnParentWindowBoundsChanged();

  const gfx::Size expected_updated_item_size(96, 120);
  EXPECT_EQ(4, apps_grid_view()->GetRowsForTesting());
  VerifyAppsContainerLayout(
      updated_window_size, /*row_count=*/3, expected_horizontal_margin,
      expected_updated_item_size, /*has_recent_apps=*/true);
}

}  // namespace
}  // namespace ash
