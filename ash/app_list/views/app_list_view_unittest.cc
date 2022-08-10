// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/app_list_view.h"

#include <stddef.h>

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/app_list/app_list_test_view_delegate.h"
#include "ash/app_list/model/app_list_test_model.h"
#include "ash/app_list/model/search/test_search_result.h"
#include "ash/app_list/views/app_list_folder_view.h"
#include "ash/app_list/views/app_list_item_view.h"
#include "ash/app_list/views/app_list_main_view.h"
#include "ash/app_list/views/apps_container_view.h"
#include "ash/app_list/views/apps_grid_view.h"
#include "ash/app_list/views/apps_grid_view_test_api.h"
#include "ash/app_list/views/contents_view.h"
#include "ash/app_list/views/expand_arrow_view.h"
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
#include "ash/app_list/views/search_result_tile_item_list_view.h"
#include "ash/app_list/views/search_result_tile_item_view.h"
#include "ash/app_list/views/search_result_view.h"
#include "ash/app_list/views/suggestion_chip_container_view.h"
#include "ash/constants/ash_features.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/pagination/pagination_model.h"
#include "ash/public/cpp/test/test_app_list_color_provider.h"
#include "ash/search_box/search_box_constants.h"
#include "ash/style/ash_color_provider.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/icu_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
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
#include "ui/views/view_model.h"

namespace ash {
namespace {

using test::AppListTestModel;
using test::AppListTestViewDelegate;
using test::AppsGridViewTestApi;

constexpr int kInitialItems = 34;

constexpr int kMaxItemsPerFolderPage = AppListFolderView::kMaxFolderColumns *
                                       AppListFolderView::kMaxPagedFolderRows;

// Constants used for for testing app list layout in fullscreen state:

// The total height of search box and suggestion chips views, including the
// vertical margin between them.
constexpr int kSearchBoxAndSuggestionChipsHeightDefault =
    48 /* search box height */ +
    16 /* margin between search box and suggestion chips */ +
    32 /* suggestion chips container height */;

constexpr int kSearchBoxAndSuggestionChipsHeightDense =
    40 /* search box height */ +
    16 /* margin between search box and suggestion chips */ +
    32 /* suggestion chips container height */;

// The app list grid vertical inset - the height of the view fadeout area.
constexpr int kGridVerticalInset = 16;

// Vertical margin for apps grid view (in addition to the grid insets).
constexpr int kGridVerticalMargin = 8;

// The horizontal spacing between apps grid view and the page switcher.
constexpr int kPageSwitcherSpacing = 8;

// The maximum allowed margin between items in apps item grid.
constexpr int kMaxItemMargin = 96;

// The min margins for contents within the fullscreen productivity launcher.
constexpr int kMinProductivityLauncherMargin = 24;

// The min horizontal margin for apps grid in fullscreen productivity launcher.
// In addition to min productivity launcher margin, reserves 32 dip for page
// switcher UI.
constexpr int kMinProductivityLauncherGridHorizontalMargin =
    kMinProductivityLauncherMargin + 32;

// The amount of the screen height that should be taken up by the vertical
// margin for the apps container view.
constexpr float kProductivityLauncherVerticalMarginRatio = 1.0f / 24.0f;
constexpr float kProductivityLauncherVerticalMarginRatioLargeHeight =
    1.0f / 16.0f;

// `is_large_height` should be set depending on whether the screen height is
// expected to be greater than 800. This is set so that the correct vertical
// margin ratio is used depending on the expected screen height.
int GetExpectedScreenSizeForProductivityLauncher(int row_count,
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

  float margin_ratio = kProductivityLauncherVerticalMarginRatio;
  if (is_large_height)
    margin_ratio = kProductivityLauncherVerticalMarginRatioLargeHeight;

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
  if (screen_height * margin_ratio < kMinProductivityLauncherMargin) {
    return screen_height + 2 * (kMinProductivityLauncherMargin -
                                (screen_height * margin_ratio));
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

// Calculates the apps item grid size with maximum allowed margin between items.
int GetItemGridSizeWithMaxItemMargins(int item_size, int item_count) {
  return item_size * item_count + (item_count - 1) * kMaxItemMargin;
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
  if (view->layer() && !view->layer()->IsDrawn())
    return false;
  if (view->layer() && view->layer()->opacity() == 0.0f)
    return false;

  return display::Screen::GetScreen()
      ->GetPrimaryDisplay()
      .work_area()
      .Intersects(view->GetBoundsInScreen());
}

class TestStartPageSearchResult : public TestSearchResult {
 public:
  TestStartPageSearchResult() {
    set_display_type(ash::SearchResultDisplayType::kChip);
    set_is_recommendation(true);
  }

  TestStartPageSearchResult(const TestStartPageSearchResult&) = delete;
  TestStartPageSearchResult& operator=(const TestStartPageSearchResult&) =
      delete;

  ~TestStartPageSearchResult() override = default;
};

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
  void Show(bool is_side_shelf = false) {
    view_->Show(AppListViewState::kPeeking, is_side_shelf);
  }

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

    contents_view->Layout();
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
  bool CheckSearchBoxWidget(const gfx::Rect& expected) {
    ContentsView* contents_view = view_->app_list_main_view()->contents_view();
    // Adjust for the search box view's shadow.
    gfx::Rect expected_with_shadow =
        view_->app_list_main_view()
            ->search_box_view()
            ->GetViewBoundsForSearchBoxContentsBounds(expected);
    gfx::Point point = expected_with_shadow.origin();
    views::View::ConvertPointToScreen(contents_view, &point);

    return gfx::Rect(point, expected_with_shadow.size()) ==
           view_->search_box_view()->GetWidget()->GetWindowBoundsInScreen();
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

  void VerifyAppsContainerLayoutForProductivityLauncher(
      const gfx::Size& container_size,
      int row_count,
      int expected_horizontal_margin,
      const gfx::Size& expected_item_size,
      bool has_recent_apps) {
    const int column_count = 5;
    ASSERT_EQ(column_count, apps_grid_view()->cols());
    ASSERT_EQ(row_count, apps_grid_view()->GetFirstPageRowsForTesting());

    const float ratio =
        (container_size.height() > 800)
            ? kProductivityLauncherVerticalMarginRatioLargeHeight
            : kProductivityLauncherVerticalMarginRatio;

    const int expected_vertical_margin =
        std::max(static_cast<int>(container_size.height() * ratio),
                 kMinProductivityLauncherMargin);
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
    EXPECT_EQ(gfx::Rect(expected_grid_width + expected_horizontal_margin +
                            kPageSwitcherSpacing,
                        expected_scrollable_container_top,
                        2 * PageSwitcher::kMaxButtonRadiusForRootGrid,
                        expected_grid_height),
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

  // Verifies fullscreen apps container bounds and layout.
  void VerifyAppsContainerLayout(const gfx::Size& container_size,
                                 int column_count,
                                 int row_count,
                                 int expected_horizontal_margin,
                                 int expected_vertical_margin,
                                 int expected_item_size) {
    const int kExpectedGridWidth =
        container_size.width() - 2 * expected_horizontal_margin;

    const int search_box_and_suggestion_chip_height =
        container_size.height() < 600 + ShelfSize()
            ? kSearchBoxAndSuggestionChipsHeightDense
            : kSearchBoxAndSuggestionChipsHeightDefault;

    const int kExpectedGridTop = expected_vertical_margin +
                                 search_box_and_suggestion_chip_height +
                                 kGridVerticalMargin;
    const int kExpectedGridHeight =
        container_size.height() - kExpectedGridTop -
        (expected_vertical_margin - kGridVerticalInset) - ShelfSize();

    EXPECT_EQ(gfx::Rect(0, 0, kExpectedGridWidth, kExpectedGridHeight),
              apps_grid_view()->bounds());
    EXPECT_EQ(gfx::Rect(expected_horizontal_margin, kExpectedGridTop,
                        kExpectedGridWidth, kExpectedGridHeight),
              scrollable_container()->bounds());
    EXPECT_EQ(gfx::Rect(kExpectedGridWidth + expected_horizontal_margin +
                            kPageSwitcherSpacing,
                        kExpectedGridTop,
                        2 * PageSwitcher::kMaxButtonRadiusForRootGrid,
                        kExpectedGridHeight),
              page_switcher_view()->bounds());

    // Horizontal offset between app list item views.
    const int kHorizontalOffset = GridItemSizeWithMargins(
        kExpectedGridWidth, expected_item_size, column_count);

    // Verify expected bounds for the first row:
    for (int i = 0; i < column_count; ++i) {
      EXPECT_EQ(gfx::Rect(i * kHorizontalOffset, kGridVerticalInset,
                          expected_item_size, expected_item_size),
                test_api_->GetItemTileRectAtVisualIndex(0, i))
          << "Item " << i << " bounds";
    }

    // Vertical offset between app list item views.
    const int kVerticalOffset =
        GridItemSizeWithMargins(kExpectedGridHeight - 2 * kGridVerticalInset,
                                expected_item_size, row_count);

    // Verify expected bounds for the first column:
    for (int j = 1; j < row_count; ++j) {
      EXPECT_EQ(gfx::Rect(0, kGridVerticalInset + j * kVerticalOffset,
                          expected_item_size, expected_item_size),
                test_api_->GetItemTileRectAtVisualIndex(0, j * column_count))
          << "Item " << j * column_count << " bounds";
    }

    // The last item in the page (bottom right):
    EXPECT_EQ(gfx::Rect((column_count - 1) * kHorizontalOffset,
                        kGridVerticalInset + (row_count - 1) * kVerticalOffset,
                        expected_item_size, expected_item_size),
              test_api_->GetItemTileRectAtVisualIndex(0, 19));

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

  TestAppListColorProvider app_list_color_provider_;  // Needed by AppListView.

  // Needed by ProductivityLauncher AppsContainerView::ContinueContainer.
  AshColorProvider ash_color_provider_;

  AppListView* view_ = nullptr;  // Owned by native widget.
  std::unique_ptr<AppListTestViewDelegate> delegate_;
  std::unique_ptr<AppsGridViewTestApi> test_api_;

  // Used by AppListFolderView::UpdatePreferredBounds.
  keyboard::KeyboardUIController keyboard_ui_controller_;
};

// Tests for the legacy "peeking" clamshell launcher. These can be deleted when
// ProductivityLauncher is the default.
class AppListViewPeekingTest : public AppListViewTest {
 public:
  AppListViewPeekingTest() {
    feature_list_.InitAndDisableFeature(features::kProductivityLauncher);
  }

  base::test::ScopedFeatureList feature_list_;
};

// Tests for tablet mode. Parameterized by ProductivityLauncher.
class AppListViewTabletTest : public AppListViewTest,
                              public testing::WithParamInterface<bool> {
 public:
  AppListViewTabletTest() {
    const bool enable_productivity_launcher = GetParam();
    feature_list_.InitWithFeatureState(features::kProductivityLauncher,
                                       enable_productivity_launcher);
  }

  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(ProductivityLauncher,
                         AppListViewTabletTest,
                         testing::Bool());

// Tests app list view layout for different screen sizes.
class AppListViewScalableLayoutTest : public AppListViewTest {
 public:
  explicit AppListViewScalableLayoutTest(bool enable_productivity_launcher) {
    if (enable_productivity_launcher) {
      scoped_feature_list_.InitWithFeatures(
          {ash::features::kEnableBackgroundBlur,
           ash::features::kProductivityLauncher},
          {});
    } else {
      scoped_feature_list_.InitWithFeatures(
          {ash::features::kEnableBackgroundBlur},
          {ash::features::kProductivityLauncher});
    }
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

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class ProductivityLauncherAppListViewLayoutTest
    : public AppListViewScalableLayoutTest {
 public:
  ProductivityLauncherAppListViewLayoutTest()
      : AppListViewScalableLayoutTest(/*enable_productivity_launcher=*/true) {}

  void InitializeAppList() {
    Initialize(true /*is_tablet_mode*/);
    delegate_->GetTestModel()->PopulateApps(kInitialItems);
    Show();
  }
};

class LegacyLauncherAppListViewLayoutTest
    : public AppListViewScalableLayoutTest {
 public:
  LegacyLauncherAppListViewLayoutTest()
      : AppListViewScalableLayoutTest(/*enable_productivity_launcher=*/false) {}
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
    // May be null for ProductivityLauncher, which does not use chips.
    suggestions_container_ = contents_view()
                                 ->apps_container_view()
                                 ->suggestion_chip_container_view_for_test();
    expand_arrow_view_ = contents_view()->expand_arrow_view();

    // Add suggestion apps, a folder with apps and other app list items.
    const int kSuggestionAppNum = 3;
    const int kItemNumInFolder = 25;
    const int kAppListItemNum =
        SharedAppListConfig::instance().GetMaxNumOfItemsPerPage() + 1;
    AppListTestModel* model = delegate_->GetTestModel();
    SearchModel* search_model = GetSearchModel();
    for (size_t i = 0; i < kSuggestionAppNum; i++) {
      search_model->results()->Add(
          std::make_unique<TestStartPageSearchResult>());
    }
    AppListFolderItem* folder_item =
        model->CreateAndPopulateFolderWithApps(kItemNumInFolder);
    model->PopulateApps(kAppListItemNum);
    if (suggestions_container_)
      suggestions_container_->Update();
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
    if (state == ash::AppListViewState::kClosed) {
      view_->Dismiss();
      return;
    }
    view_->SetState(state);
  }

  void Show() {
    view_->Show(AppListViewState::kPeeking, /*is_side_shelf=*/false);
  }

  SearchResultTileItemListView* GetSearchResultTileItemListView() {
    return contents_view()
        ->search_result_page_view()
        ->GetSearchResultTileItemListViewForTest();
  }

  SearchResultListView* GetSearchResultListView() {
    return contents_view()
        ->search_result_page_view()
        ->GetSearchResultListViewForTest();
  }

  AppsGridViewTestApi* test_api() { return test_api_.get(); }

  void SimulateKeyPress(ui::KeyboardCode key_code,
                        bool shift_down,
                        bool ctrl_down = false) {
    ui::KeyEvent key_event(ui::ET_KEY_PRESSED, key_code,
                           shift_down
                               ? ui::EF_SHIFT_DOWN
                               : ctrl_down ? ui::EF_CONTROL_DOWN : ui::EF_NONE);
    view_->GetWidget()->OnKeyEvent(&key_event);
  }

  // Add search results for test on focus movement.
  void SetUpSearchResults(int tile_results_num, int list_results_num) {
    std::vector<std::pair<SearchResult::DisplayType, int>> result_types;
    result_types.emplace_back(SearchResultDisplayType::kTile, tile_results_num);
    result_types.emplace_back(SearchResultDisplayType::kList, list_results_num);

    SearchModel::SearchResults* results = GetSearchModel()->results();
    results->DeleteAll();
    double display_score = result_types.size();
    for (const auto& data : result_types) {
      // Set the display score of the results in each group in decreasing order
      // (so the earlier groups have higher display score, and therefore appear
      // first).
      display_score -= 0.5;
      for (int i = 0; i < data.second; ++i) {
        std::unique_ptr<TestSearchResult> result =
            std::make_unique<TestSearchResult>();
        result->set_display_type(data.first);
        result->set_display_score(display_score);
        result->SetTitle(u"Test");
        result->set_best_match(true);
        results->Add(std::move(result));
      }
    }

    // Adding results will schedule Update().
    base::RunLoop().RunUntilIdle();
  }

  // Add search results for test on embedded Assistant UI.
  void SetUpSearchResultsForAssistantUI(int list_results_num,
                                        int index_open_assistant_ui) {
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
      if (i == index_open_assistant_ui)
        result->set_is_omnibox_search(true);

      results->Add(std::move(result));
    }

    // Adding results will schedule Update().
    base::RunLoop().RunUntilIdle();
  }

  void ClearSearchResults() { GetSearchModel()->results()->DeleteAll(); }

  void AddSearchResultWithTitleAndScore(const base::StringPiece& title,
                                        double score) {
    std::unique_ptr<TestSearchResult> result =
        std::make_unique<TestSearchResult>();
    result->set_display_type(ash::SearchResultDisplayType::kList);
    result->set_display_score(score);
    result->SetTitle(ASCIIToUTF16(title));
    result->set_best_match(true);
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

  int GetTotalOpenAssistantUICount() {
    return delegate_->open_assistant_ui_count();
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

  void TestSelectionTraversal(const std::vector<views::View*>& view_list,
                              ui::KeyboardCode key_code,
                              bool shift_down) {
    ResultSelectionController* selection_controller =
        contents_view()
            ->search_result_page_view()
            ->result_selection_controller();
    EXPECT_EQ(view_list[0], selection_controller->selected_result());
    for (size_t i = 1; i < view_list.size(); ++i) {
      SimulateKeyPress(key_code, shift_down);
      EXPECT_EQ(view_list[i], selection_controller->selected_result());
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
    EXPECT_EQ(textfield, focused_view());
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
    views::View* next_view = next_view =
        view_->GetWidget()->GetFocusManager()->GetNextFocusableView(
            textfield, view_->GetWidget(), false, false);
    views::View* prev_view = prev_view =
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

  SearchResultContainerView* suggestions_container() {
    return suggestions_container_;
  }

  std::vector<views::View*> GetAllSuggestions() {
    // ProductivityLauncher does not use suggestion chips.
    DCHECK(!features::IsProductivityLauncherEnabled());
    const auto& children = suggestions_container()->children();
    std::vector<views::View*> suggestions;
    std::copy_if(children.cbegin(), children.cend(),
                 std::back_inserter(suggestions),
                 [](const auto* v) { return v->GetVisible(); });
    return suggestions;
  }

  SearchBoxView* search_box_view() { return main_view()->search_box_view(); }

  AppListItemView* folder_item_view() {
    return apps_grid_view()->view_model()->view_at(0);
  }

  views::View* focused_view() {
    return view_->GetWidget()->GetFocusManager()->GetFocusedView();
  }

  ExpandArrowView* expand_arrow_view() { return expand_arrow_view_; }

 protected:
  bool is_rtl_ = false;
  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  TestAppListColorProvider app_list_color_provider_;  // Needed by AppListView.
  AshColorProvider ash_color_provider_;  // Needed by ProductivityLauncher.
  AppListView* view_ = nullptr;  // Owned by native widget.
  // Owned by view hierarchy. May be null for ProductivityLauncher, which does
  // not use suggestion chips.
  SearchResultContainerView* suggestions_container_ = nullptr;
  ExpandArrowView* expand_arrow_view_ = nullptr;  // Owned by view hierarchy.

  std::unique_ptr<AppListTestViewDelegate> delegate_;
  std::unique_ptr<AppsGridViewTestApi> test_api_;
  // Restores the locale to default when destructor is called.
  base::test::ScopedRestoreICUDefaultLocale restore_locale_;

  // Used by AppListFolderView::UpdatePreferredBounds.
  keyboard::KeyboardUIController keyboard_ui_controller_;
};

INSTANTIATE_TEST_SUITE_P(Rtl, AppListViewFocusTest, testing::Bool());

// Tests for the legacy "peeking" clamshell launcher. These can be deleted when
// ProductivityLauncher is the default.
class AppListViewPeekingFocusTest : public AppListViewFocusTest {
 public:
  AppListViewPeekingFocusTest() {
    feature_list_.InitAndDisableFeature(features::kProductivityLauncher);
  }

  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(Rtl, AppListViewPeekingFocusTest, testing::Bool());

// Tests that the initial focus is on search box.
TEST_F(AppListViewFocusTest, InitialFocus) {
  EXPECT_EQ(search_box_view()->search_box(), focused_view());
}

// Tests the linear focus traversal in PEEKING state.
TEST_P(AppListViewPeekingFocusTest, LinearFocusTraversalInPeekingState) {
  Show();
  SetAppListState(ash::AppListViewState::kPeeking);

  std::vector<views::View*> forward_view_list;
  forward_view_list.push_back(search_box_view()->search_box());
  for (auto* v : GetAllSuggestions())
    forward_view_list.push_back(v);
  forward_view_list.push_back(expand_arrow_view());
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

// Tests the linear focus traversal in FULLSCREEN_ALL_APPS state.
TEST_P(AppListViewFocusTest, LinearFocusTraversalInFullscreenAllAppsState) {
  // TODO(https://crbug.com/1284992): Fix for ProductivityLauncher, which
  // does not use suggestion chips.
  if (features::IsProductivityLauncherEnabled())
    return;

  Show();
  SetAppListState(ash::AppListViewState::kFullscreenAllApps);

  std::vector<views::View*> forward_view_list;
  forward_view_list.push_back(search_box_view()->search_box());
  for (auto* v : GetAllSuggestions())
    forward_view_list.push_back(v);
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

// Tests focus traversal in HALF state with opened search box using |VKEY_TAB|.
TEST_F(AppListViewPeekingFocusTest, TabFocusTraversalInHalfState) {
  Show();

  // Type something in search box to transition to HALF state and populate
  // fake search results.
  search_box_view()->search_box()->InsertText(
      u"test",
      ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  EXPECT_EQ(app_list_view()->app_list_state(), ash::AppListViewState::kHalf);
  constexpr int kTileResults = 3;
  constexpr int kListResults = 2;
  SetUpSearchResults(kTileResults, kListResults);

  std::vector<views::View*> forward_view_list;

  const std::vector<SearchResultTileItemView*>& tile_views =
      GetSearchResultTileItemListView()->tile_views_for_test();
  for (int i = 0; i < kTileResults; ++i)
    forward_view_list.push_back(tile_views[i]);

  SearchResultListView* list_view = GetSearchResultListView();
  for (int i = 0; i < kListResults; ++i)
    forward_view_list.push_back(list_view->GetResultViewAt(i));

  // The selected view will always be a result when using
  // |result_selection_controller|
  forward_view_list.push_back(nullptr);

  std::vector<views::View*> backward_view_list = forward_view_list;
  std::reverse(backward_view_list.begin(), backward_view_list.end());

  // Test traversal triggered by tab.
  EXPECT_TRUE(search_box_view()->search_box()->HasFocus());
  TestSelectionTraversal(forward_view_list, ui::VKEY_TAB, false);
  EXPECT_TRUE(search_box_view()->close_button()->HasFocus());

  // Focus cycles from the close button to the first result.
  TestSelectionTraversal({nullptr, forward_view_list[0]}, ui::VKEY_TAB, false);
  EXPECT_TRUE(search_box_view()->search_box()->HasFocus());

  // The shift+tab key should move focus back to the close button.
  TestSelectionTraversal({forward_view_list[0], nullptr}, ui::VKEY_TAB, true);
  EXPECT_TRUE(search_box_view()->close_button()->HasFocus());

  // Test traversal triggered by shift+tab.
  TestSelectionTraversal(backward_view_list, ui::VKEY_TAB, true);
  EXPECT_TRUE(search_box_view()->search_box()->HasFocus());
}

// Tests return key with search box close button focused (with app list view in
// half state):
// *   search box text is cleared
// *   search box gets focus, but it's not active
// *   subsequent tab keys move focus to app list folder view.
TEST_F(AppListViewPeekingFocusTest, CloseButtonClearsSearchOnEnter) {
  Show();

  // Type something in search box to transition to HALF state and populate
  // fake search results.
  search_box_view()->search_box()->InsertText(
      u"test",
      ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  EXPECT_EQ(app_list_view()->app_list_state(), ash::AppListViewState::kHalf);
  constexpr int kTileResults = 3;
  constexpr int kListResults = 2;
  SetUpSearchResults(kTileResults, kListResults);

  const std::vector<SearchResultTileItemView*>& tile_views =
      GetSearchResultTileItemListView()->tile_views_for_test();
  ASSERT_FALSE(tile_views.empty());
  views::View* first_result_view = tile_views[0];

  // Shift+Tab to focus close button.
  TestSelectionTraversal({first_result_view, nullptr}, ui::VKEY_TAB, true);
  EXPECT_TRUE(search_box_view()->close_button()->HasFocus());

  // Enter - it should clear the search box.
  SimulateKeyPress(ui::VKEY_RETURN, false /*shift_down*/);
  EXPECT_TRUE(search_box_view()->search_box()->HasFocus());
  EXPECT_EQ(std::u16string(), search_box_view()->search_box()->GetText());
  EXPECT_FALSE(search_box_view()->is_search_box_active());
  EXPECT_FALSE(contents_view()->search_result_page_view()->GetVisible());
  ResultSelectionController* selection_controller =
      contents_view()->search_result_page_view()->result_selection_controller();
  EXPECT_EQ(nullptr, selection_controller->selected_result());

  // Tab traversal continues with app list folder items.
  std::vector<views::View*> forward_view_list;
  forward_view_list.push_back(search_box_view()->search_box());
  const views::ViewModelT<AppListItemView>* view_model =
      app_list_folder_view()->items_grid_view()->view_model();
  for (const auto& entry : view_model->entries())
    forward_view_list.push_back(entry.view);
  TestFocusTraversal(forward_view_list, ui::VKEY_TAB, false);
}

// Tests focus traversal in HALF state with opened search box using |VKEY_LEFT|
// and |VKEY_RIGHT|.
TEST_P(AppListViewPeekingFocusTest, LeftRightFocusTraversalInHalfState) {
  Show();

  // Type something in search box to transition to HALF state and populate
  // fake search results.
  // Type something in textfield.
  std::u16string text = is_rtl_ ? u"اختبار" : u"test";
  search_box_view()->search_box()->InsertText(
      text,
      ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  EXPECT_EQ(app_list_view()->app_list_state(), ash::AppListViewState::kHalf);

  constexpr int kTileResults = 6;
  SetUpSearchResults(kTileResults, 0);

  std::vector<views::View*> forward_view_list;
  const std::vector<SearchResultTileItemView*>& tile_views =
      GetSearchResultTileItemListView()->tile_views_for_test();
  forward_view_list.push_back(tile_views[0]);

  for (int i = 1; i < kTileResults; ++i)
    forward_view_list.push_back(tile_views[i]);

  forward_view_list.push_back(tile_views[0]);

  TestSelectionTraversal(forward_view_list,
                         is_rtl_ ? ui::VKEY_LEFT : ui::VKEY_RIGHT, false);

  std::vector<views::View*> backward_view_list = forward_view_list;

  std::reverse(backward_view_list.begin(), backward_view_list.end());

  TestSelectionTraversal(backward_view_list,
                         is_rtl_ ? ui::VKEY_RIGHT : ui::VKEY_LEFT, false);
}

// Tests the linear focus traversal in FULLSCREEN_ALL_APPS state within folder.
TEST_P(AppListViewFocusTest, LinearFocusTraversalInFolder) {
  Show();

  // Transition to FULLSCREEN_ALL_APPS state and open the folder.
  SetAppListState(ash::AppListViewState::kFullscreenAllApps);
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

// Tests the vertical focus traversal by in PEEKING state.
TEST_P(AppListViewPeekingFocusTest, VerticalFocusTraversalInPeekingState) {
  Show();
  SetAppListState(ash::AppListViewState::kPeeking);

  std::vector<views::View*> forward_view_list;
  forward_view_list.push_back(search_box_view()->search_box());
  const std::vector<views::View*> suggestions = GetAllSuggestions();
  forward_view_list.push_back(suggestions[0]);
  forward_view_list.push_back(expand_arrow_view());
  forward_view_list.push_back(search_box_view()->search_box());

  // Test traversal triggered by down.
  TestFocusTraversal(forward_view_list, ui::VKEY_DOWN, false);

  std::vector<views::View*> backward_view_list;
  backward_view_list.push_back(search_box_view()->search_box());
  backward_view_list.push_back(expand_arrow_view());
  backward_view_list.push_back(suggestions.back());
  backward_view_list.push_back(search_box_view()->search_box());

  // Test traversal triggered by up.
  TestFocusTraversal(backward_view_list, ui::VKEY_UP, false);
}

// Tests the vertical focus traversal in FULLSCREEN_ALL_APPS state.
TEST_P(AppListViewFocusTest, VerticalFocusTraversalInFullscreenAllAppsState) {
  // TODO(https://crbug.com/1284992): Fix for ProductivityLauncher, which
  // does not use suggestion chips.
  if (features::IsProductivityLauncherEnabled())
    return;

  Show();
  SetAppListState(ash::AppListViewState::kFullscreenAllApps);

  std::vector<views::View*> forward_view_list;
  forward_view_list.push_back(search_box_view()->search_box());
  const std::vector<views::View*> suggestions = GetAllSuggestions();
  forward_view_list.push_back(suggestions[0]);
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
  // Up key will always move focus to the last suggestion chip from first row
  // apps.
  const int index = suggestions.size() - 1;
  backward_view_list.push_back(suggestions[index]);
  backward_view_list.push_back(search_box_view()->search_box());

  // Test traversal triggered by up.
  TestFocusTraversal(backward_view_list, ui::VKEY_UP, false);
}

// Tests the vertical focus traversal in HALF state with opened search box.
TEST_F(AppListViewPeekingFocusTest, VerticalFocusTraversalInHalfState) {
  Show();

  // Type something in search box to transition to HALF state and populate
  // fake search results.
  search_box_view()->search_box()->InsertText(
      u"test",
      ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  EXPECT_EQ(app_list_view()->app_list_state(), ash::AppListViewState::kHalf);
  constexpr int kTileResults = 3;
  constexpr int kListResults = 2;
  SetUpSearchResults(kTileResults, kListResults);

  std::vector<views::View*> forward_view_list;

  const std::vector<SearchResultTileItemView*>& tile_views =
      GetSearchResultTileItemListView()->tile_views_for_test();
  forward_view_list.push_back(tile_views[0]);

  SearchResultListView* list_view = GetSearchResultListView();
  for (int i = 0; i < kListResults; ++i)
    forward_view_list.push_back(list_view->GetResultViewAt(i));

  contents_view()
      ->search_result_page_view()
      ->result_selection_controller()
      ->ResetSelection(nullptr, false);

  // Test traversal triggered by down.
  TestSelectionTraversal(forward_view_list, ui::VKEY_DOWN, false);

  std::vector<views::View*> backward_view_list;
  for (int i = kListResults - 1; i >= 0; --i)
    backward_view_list.push_back(list_view->GetResultViewAt(i));
  backward_view_list.push_back(tile_views[kTileResults - 1]);

  // Test traversal triggered by up.
  TestSelectionTraversal(backward_view_list, ui::VKEY_UP, false);
}

// Tests the vertical focus traversal in FULLSCREEN_ALL_APPS state in the first
// page within folder.
TEST_F(AppListViewFocusTest, VerticalFocusTraversalInFirstPageOfFolder) {
  Show();

  // Transition to FULLSCREEN_ALL_APPS state and open the folder.
  SetAppListState(ash::AppListViewState::kFullscreenAllApps);
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

// Tests the vertical focus traversal in FULLSCREEN_ALL_APPS state in the second
// page within folder. ProductivityLauncher does not use pages for folders.
TEST_F(AppListViewPeekingFocusTest,
       VerticalFocusTraversalInSecondPageOfFolder) {
  Show();

  // Transition to FULLSCREEN_ALL_APPS state and open the folder.
  SetAppListState(ash::AppListViewState::kFullscreenAllApps);
  folder_item_view()->RequestFocus();
  SimulateKeyPress(ui::VKEY_RETURN, false);
  EXPECT_TRUE(contents_view()->apps_container_view()->IsInFolderView());

  // Select the second page.
  ASSERT_FALSE(features::IsProductivityLauncherEnabled());
  static_cast<PagedAppsGridView*>(app_list_folder_view()->items_grid_view())
      ->pagination_model()
      ->SelectPage(1, false /* animate */);

  std::vector<views::View*> forward_view_list;
  const views::ViewModelT<AppListItemView>* view_model =
      app_list_folder_view()->items_grid_view()->view_model();
  for (size_t i = kMaxItemsPerFolderPage; i < view_model->view_size();
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

// Tests that the focus is set back onto search box after all state transitions
// besides those going to/from an activated folder.
TEST_F(AppListViewPeekingFocusTest, FocusResetAfterStateTransition) {
  Show();

  // Type something in search box to transition to HALF state and populate
  // fake search results.
  search_box_view()->search_box()->InsertText(
      u"test",
      ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  const int kTileResults = 3;
  const int kListResults = 2;
  SetUpSearchResults(kTileResults, kListResults);

  EXPECT_EQ(app_list_view()->app_list_state(), ash::AppListViewState::kHalf);
  EXPECT_EQ(search_box_view()->search_box(), focused_view());

  // Move focus to the first search result, then transition to PEEKING state.
  SimulateKeyPress(ui::VKEY_TAB, false);
  SimulateKeyPress(ui::VKEY_TAB, false);

  SetAppListState(ash::AppListViewState::kPeeking);
  EXPECT_EQ(app_list_view()->app_list_state(), ash::AppListViewState::kPeeking);
  EXPECT_EQ(search_box_view()->search_box(), focused_view());

  // Move focus to the first suggestion app, then transition to
  // FULLSCREEN_ALL_APPS state.
  SimulateKeyPress(ui::VKEY_TAB, false);
  SetAppListState(ash::AppListViewState::kFullscreenAllApps);
  EXPECT_EQ(app_list_view()->app_list_state(),
            ash::AppListViewState::kFullscreenAllApps);
  EXPECT_EQ(search_box_view()->search_box(), focused_view());

  // Move focus to the folder and open it.
  folder_item_view()->RequestFocus();
  SimulateKeyPress(ui::VKEY_RETURN, false);

  //  Test that the first item in the folder is focused.
  EXPECT_TRUE(contents_view()->apps_container_view()->IsInFolderView());
  EXPECT_EQ(app_list_folder_view()->items_grid_view()->view_model()->view_at(0),
            focused_view());

  // Close the folder.
  SimulateKeyPress(ui::VKEY_ESCAPE, false);

  // Test that focus is on the previously activated folder item
  EXPECT_EQ(folder_item_view(), focused_view());

  // Transition to PEEKING state.
  SetAppListState(ash::AppListViewState::kPeeking);

  // Test that the searchbox is focused.
  EXPECT_EQ(app_list_view()->app_list_state(), ash::AppListViewState::kPeeking);
  EXPECT_EQ(search_box_view()->search_box(), focused_view());
}

// Tests that key event which is not handled by focused view will be redirected
// to search box when search box view is active (but not focused).
TEST_F(AppListViewFocusTest, RedirectFocusToSearchBox) {
  // TODO(https://crbug.com/1284992): Fix for ProductivityLauncher, which
  // does not support this behavior and also does not use suggestion chips.
  if (features::IsProductivityLauncherEnabled())
    return;

  Show();

  // Set focus to first suggestion app and type a character.
  GetAllSuggestions()[0]->RequestFocus();
  SimulateKeyPress(ui::VKEY_A, false);
  EXPECT_EQ(search_box_view()->search_box(), focused_view());
  EXPECT_EQ(search_box_view()->search_box()->GetText(), u"a");
  EXPECT_FALSE(search_box_view()->search_box()->HasSelection());

  // Set focus to close button and type a character.
  search_box_view()->close_button()->RequestFocus();
  EXPECT_NE(search_box_view()->search_box(), focused_view());
  SimulateKeyPress(ui::VKEY_B, false);
  EXPECT_EQ(search_box_view()->search_box(), focused_view());
  EXPECT_EQ(search_box_view()->search_box()->GetText(), u"ab");
  EXPECT_FALSE(search_box_view()->search_box()->HasSelection());

  // Set focus to close button and hitting backspace.
  search_box_view()->close_button()->RequestFocus();
  SimulateKeyPress(ui::VKEY_BACK, false);
  EXPECT_EQ(search_box_view()->search_box(), focused_view());
  EXPECT_EQ(search_box_view()->search_box()->GetText(), u"a");
  EXPECT_FALSE(search_box_view()->search_box()->HasSelection());
}

// Tests that focus changes update the search box text.
TEST_F(AppListViewFocusTest, SearchBoxTextUpdatesOnResultFocus) {
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

  EXPECT_EQ(search_box->GetText(), u"TestResult2");

  SimulateKeyPress(ui::VKEY_TAB, true);

  EXPECT_EQ(search_box->GetText(), u"TestResult1");

  SimulateKeyPress(ui::VKEY_TAB, false);

  // Change focus to the final result
  SimulateKeyPress(ui::VKEY_TAB, false);

  EXPECT_EQ(search_box->GetText(), u"TestResult3");
}

// Tests that ctrl-A selects all text in the searchbox when the SearchBoxView is
// not focused.
TEST_F(AppListViewFocusTest, CtrlASelectsAllTextInSearchbox) {
  Show();
  search_box_view()->search_box()->InsertText(
      u"test",
      ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  EXPECT_EQ(app_list_view()->app_list_state(), ash::AppListViewState::kHalf);
  constexpr int kTileResults = 3;
  constexpr int kListResults = 2;
  SetUpSearchResults(kTileResults, kListResults);

  // Move focus to the first search result.
  SimulateKeyPress(ui::VKEY_TAB, false);
  SimulateKeyPress(ui::VKEY_TAB, false);

  // Focus left the searchbox, so the selected range should be at the end of the
  // search text.
  EXPECT_FALSE(search_box_view()->search_box()->HasSelection());
  EXPECT_EQ(gfx::Range(4, 4),
            search_box_view()->search_box()->GetSelectedRange());

  // Press Ctrl-A, everything should be selected and the selected range should
  // include the whole text.
  SimulateKeyPress(ui::VKEY_A, false, true);
  EXPECT_TRUE(search_box_view()->search_box()->HasSelection());
  EXPECT_EQ(gfx::Range(0, 4),
            search_box_view()->search_box()->GetSelectedRange());

  // Advance focus, Focus should leave the searchbox, and the selected range
  // should be at the end of the search text.
  SimulateKeyPress(ui::VKEY_TAB, false);
  EXPECT_FALSE(search_box_view()->search_box()->HasSelection());
  EXPECT_EQ(gfx::Range(4, 4),
            search_box_view()->search_box()->GetSelectedRange());
}

// Tests that the first search result's view is selected after search results
// are updated when the focus is on search box.
// crbug.com/1242053: flaky on chromeos
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_FirstResultSelectedAfterSearchResultsUpdated \
  DISABLED_FirstResultSelectedAfterSearchResultsUpdated
#else
#define MAYBE_FirstResultSelectedAfterSearchResultsUpdated \
  FirstResultSelectedAfterSearchResultsUpdated
#endif
TEST_F(AppListViewFocusTest,
       MAYBE_FirstResultSelectedAfterSearchResultsUpdated) {
  Show();

  // Type something in search box to transition to HALF state and populate
  // fake list results.
  search_box_view()->search_box()->InsertText(
      u"test",
      ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  const int kListResults = 2;
  SetUpSearchResults(0, kListResults);
  SearchResultListView* list_view = GetSearchResultListView();

  EXPECT_EQ(search_box_view()->search_box(), focused_view());
  EXPECT_EQ(list_view->GetResultViewAt(0),
            contents_view()->search_result_page_view()->first_result_view());
  EXPECT_TRUE(list_view->GetResultViewAt(0)->selected());

  // Populate both fake list results and tile results.
  const int kTileResults = 3;
  SetUpSearchResults(kTileResults, kListResults);
  const std::vector<SearchResultTileItemView*>& tile_views =
      GetSearchResultTileItemListView()->tile_views_for_test();
  EXPECT_EQ(search_box_view()->search_box(), focused_view());
  EXPECT_EQ(tile_views[0],
            contents_view()->search_result_page_view()->first_result_view());
  EXPECT_TRUE(tile_views[0]->selected());

  ResultSelectionController* selection_controller =
      contents_view()->search_result_page_view()->result_selection_controller();

  // Ensures the |ResultSelectionController| selects the correct result
  EXPECT_EQ(selection_controller->selected_result(), tile_views[0]);

  // Ensure current highlighted result loses highlight on transition
  SimulateKeyPress(ui::VKEY_TAB, false);
  EXPECT_FALSE(tile_views[0]->selected());

  // Clear up all search results.
  SetUpSearchResults(0, 0);
  EXPECT_EQ(search_box_view()->search_box(), focused_view());
  EXPECT_EQ(nullptr,
            contents_view()->search_result_page_view()->first_result_view());
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

  // Type something in search box to transition to HALF state and populate
  // fake list results. Then hit Enter key.
  search_box_view()->search_box()->InsertText(
      u"test",
      ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  const int kListResults = 2;
  SetUpSearchResults(0, kListResults);
  SimulateKeyPress(ui::VKEY_RETURN, false);
  EXPECT_EQ(1, GetOpenFirstSearchResultCount());
  EXPECT_EQ(1, GetTotalOpenSearchResultCount());

  // Populate both fake list results and tile results. Then hit Enter key.
  const int kTileResults = 3;
  SetUpSearchResults(kTileResults, kListResults);
  SimulateKeyPress(ui::VKEY_RETURN, false);
  EXPECT_EQ(2, GetOpenFirstSearchResultCount());
  EXPECT_EQ(2, GetTotalOpenSearchResultCount());

  // Clear up all search results. Then hit Enter key.
  SetUpSearchResults(0, 0);
  SimulateKeyPress(ui::VKEY_RETURN, false);
  EXPECT_EQ(2, GetOpenFirstSearchResultCount());
  EXPECT_EQ(2, GetTotalOpenSearchResultCount());
}

// Tests that search box becomes focused when it is activated.
TEST_F(AppListViewFocusTest, SetFocusOnSearchboxWhenActivated) {
  Show();

  // Press tab several times to move focus out of the search box.
  SimulateKeyPress(ui::VKEY_TAB, false);
  SimulateKeyPress(ui::VKEY_TAB, false);
  SimulateKeyPress(ui::VKEY_TAB, false);
  EXPECT_FALSE(search_box_view()->search_box()->HasFocus());

  // Activate the search box.
  search_box_view()->SetSearchBoxActive(true, ui::ET_MOUSE_PRESSED);
  EXPECT_TRUE(search_box_view()->search_box()->HasFocus());

  // Deactivate the search box won't move focus away.
  search_box_view()->SetSearchBoxActive(false, ui::ET_MOUSE_PRESSED);
  EXPECT_TRUE(search_box_view()->search_box()->HasFocus());
}

// Tests the left and right key when focus is on the textfield.
TEST_P(AppListViewFocusTest, HittingLeftRightWhenFocusOnTextfield) {
  Show();

  // Transition to FULLSCREEN_ALL_APPS state and open the folder.
  SetAppListState(ash::AppListViewState::kFullscreenAllApps);
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

  // Transition to FULLSCREEN_ALL_APPS state and open the folder.
  SetAppListState(ash::AppListViewState::kFullscreenAllApps);
  folder_item_view()->RequestFocus();
  SimulateKeyPress(ui::VKEY_RETURN, false);
  EXPECT_TRUE(contents_view()->apps_container_view()->IsInFolderView());

  // Set focus on the folder name.
  views::View* folder_name_view =
      app_list_folder_view()->folder_header_view()->GetFolderNameViewForTest();
  folder_name_view->RequestFocus();

  // Hit enter key.
  SimulateKeyPress(ui::VKEY_RETURN, false);
  EXPECT_TRUE(contents_view()->apps_container_view()->IsInFolderView());
  EXPECT_FALSE(folder_name_view->HasFocus());

  // Refocus and hit escape key.
  folder_name_view->RequestFocus();
  SimulateKeyPress(ui::VKEY_ESCAPE, false);
  EXPECT_TRUE(contents_view()->apps_container_view()->IsInFolderView());
  EXPECT_FALSE(folder_name_view->HasFocus());
}

// Tests that the selection highlight follows the page change.
TEST_F(AppListViewFocusTest, SelectionHighlightFollowsChangingPage) {
  // Move the focus to the first app in the grid.
  Show();
  SetAppListState(ash::AppListViewState::kFullscreenAllApps);
  const views::ViewModelT<AppListItemView>* view_model =
      apps_grid_view()->view_model();
  AppListItemView* first_item_view = view_model->view_at(0);
  first_item_view->RequestFocus();
  ASSERT_EQ(0, apps_grid_view()->pagination_model()->selected_page());

  // Select the second page.
  apps_grid_view()->pagination_model()->SelectPage(1, false);

  // Test that focus followed to the next page.
  EXPECT_EQ(view_model->view_at(test_api()->TilesPerPage(0)),
            apps_grid_view()->selected_view());

  // Select the first page.
  apps_grid_view()->pagination_model()->SelectPage(0, false);

  // Test that focus followed.
  EXPECT_EQ(view_model->view_at(test_api()->TilesPerPage(0) - 1),
            apps_grid_view()->selected_view());
}

// Tests that the selection highlight only shows up inside a folder if the
// selection highlight existed on the folder before it opened.
TEST_F(AppListViewFocusTest, SelectionDoesNotShowInFolderIfNotSelected) {
  // Open a folder without making the view selected.
  Show();
  SetAppListState(ash::AppListViewState::kFullscreenAllApps);
  const gfx::Point folder_item_view_bounds =
      folder_item_view()->bounds().CenterPoint();
  ui::GestureEvent tap(folder_item_view_bounds.x(), folder_item_view_bounds.y(),
                       0, base::TimeTicks(),
                       ui::GestureEventDetails(ui::ET_GESTURE_TAP));
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
  SetAppListState(ash::AppListViewState::kFullscreenAllApps);

  folder_item_view()->RequestFocus();
  ASSERT_TRUE(apps_grid_view()->IsSelectedView(folder_item_view()));

  // Show the folder.
  const gfx::Point folder_item_view_bounds =
      folder_item_view()->bounds().CenterPoint();
  ui::GestureEvent tap(folder_item_view_bounds.x(), folder_item_view_bounds.y(),
                       0, base::TimeTicks(),
                       ui::GestureEventDetails(ui::ET_GESTURE_TAP));
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

// Tests that opening the app list opens in peeking mode by default.
TEST_F(AppListViewPeekingTest, ShowPeekingByDefault) {
  Initialize(false /*is_tablet_mode*/);

  Show();

  ASSERT_EQ(ash::AppListViewState::kPeeking, view_->app_list_state());
}

// Tests that in side shelf mode, the app list opens in fullscreen by default
// and verifies that the top rounded corners of the app list background are
// hidden (see https://crbug.com/920082). ProductivityLauncher does not change
// shelf corners.
TEST_F(AppListViewPeekingTest, ShowFullscreenWhenInSideShelfMode) {
  Initialize(false /*is_tablet_mode*/);

  Show(true /*is_side_shelf*/);
  EXPECT_EQ(ash::AppListViewState::kFullscreenAllApps, view_->app_list_state());
  // The rounded corners should be off screen in side shelf.
  gfx::Transform translation;
  translation.Translate(0, -(delegate_->GetShelfSize() / 2));
  // The rounded corners should be off screen in side shelf.
  EXPECT_EQ(translation,
            view_->GetAppListBackgroundShieldForTest()->GetTransform());
}

// Tests that in tablet mode, the app list opens in fullscreen by default.
TEST_F(AppListViewTest, ShowFullscreenWhenInTabletMode) {
  Initialize(true /*is_tablet_mode*/);

  Show();

  ASSERT_EQ(ash::AppListViewState::kFullscreenAllApps, view_->app_list_state());
}

// Tests that setting empty text in the search box does not change the state.
TEST_F(AppListViewPeekingTest, EmptySearchTextStillPeeking) {
  Initialize(false /*is_tablet_mode*/);
  views::Textfield* search_box =
      view_->app_list_main_view()->search_box_view()->search_box();

  Show();
  search_box->SetText(std::u16string());

  ASSERT_EQ(ash::AppListViewState::kPeeking, view_->app_list_state());
}

TEST_F(AppListViewPeekingTest, UpwardMouseWheelScrollTransitionsToFullscreen) {
  base::HistogramTester histogram_tester;

  Initialize(false /*is_tablet_mode*/);
  delegate_->GetTestModel()->PopulateApps(kInitialItems);
  Show();
  EXPECT_EQ(AppListViewState::kPeeking, view_->app_list_state());

  view_->HandleScroll(gfx::Point(0, 0), gfx::Vector2d(0, 30),
                      ui::ET_MOUSEWHEEL);

  EXPECT_EQ(ash::AppListViewState::kFullscreenAllApps, view_->app_list_state());
  // This should use animation instead of drag.
  // TODO(oshima): Test AnimationSmoothness.
  histogram_tester.ExpectTotalCount(
      "Apps.StateTransition.Drag.PresentationTime.ClamshellMode", 0);

  auto grid_bounds = gfx::RectF(apps_grid_view()->bounds());
  views::View::ConvertRectToTarget(apps_grid_view(), view_, &grid_bounds);

  view_->HandleScroll(gfx::ToRoundedPoint(grid_bounds.CenterPoint()),
                      gfx::Vector2d(0, 30), ui::ET_MOUSEWHEEL);
  EXPECT_EQ(ash::AppListViewState::kFullscreenAllApps, view_->app_list_state());

  // Scrolls inside or to the side of the apps grid should not dismiss.
  view_->HandleScroll(
      gfx::ToRoundedPoint(grid_bounds.left_center()) + gfx::Vector2d(-20, 0),
      gfx::Vector2d(0, -30), ui::ET_MOUSEWHEEL);
  ASSERT_EQ(0, delegate_->dismiss_count());

  // Scroll above the app list should dismiss.
  view_->HandleScroll(gfx::Point(0, 0), gfx::Vector2d(0, -30),
                      ui::ET_MOUSEWHEEL);
  ASSERT_EQ(1, delegate_->dismiss_count());
}

TEST_F(AppListViewPeekingTest,
       DownwardMouseWheelScrollDismissesPeekingLauncher) {
  Initialize(false /*is_tablet_mode*/);
  delegate_->GetTestModel()->PopulateApps(kInitialItems);
  Show();

  EXPECT_EQ(ash::AppListViewState::kPeeking, view_->app_list_state());

  EXPECT_EQ(0, delegate_->dismiss_count());
  view_->HandleScroll(gfx::Point(0, 0), gfx::Vector2d(0, -30),
                      ui::ET_MOUSEWHEEL);
  EXPECT_EQ(1, delegate_->dismiss_count());
}

TEST_F(AppListViewPeekingTest, UpwardGestureScrollTransitionsToFullscreen) {
  base::HistogramTester histogram_tester;
  Initialize(false /*is_tablet_mode*/);
  delegate_->GetTestModel()->PopulateApps(kInitialItems);
  Show();
  EXPECT_EQ(AppListViewState::kPeeking, view_->app_list_state());

  view_->HandleScroll(gfx::Point(0, 0), gfx::Vector2d(0, 30), ui::ET_SCROLL);

  EXPECT_EQ(ash::AppListViewState::kFullscreenAllApps, view_->app_list_state());
  // This should use animation instead of drag.
  // TODO(oshima): Test AnimationSmoothness.
  histogram_tester.ExpectTotalCount(
      "Apps.StateTransition.Drag.PresentationTime.ClamshellMode", 0);
}

TEST_F(AppListViewPeekingTest, DownwardGestureScrollDismissesPeekingLauncher) {
  Initialize(false /*is_tablet_mode*/);
  delegate_->GetTestModel()->PopulateApps(kInitialItems);
  Show();

  EXPECT_EQ(ash::AppListViewState::kPeeking, view_->app_list_state());

  EXPECT_EQ(0, delegate_->dismiss_count());
  view_->HandleScroll(gfx::Point(0, 0), gfx::Vector2d(0, -30), ui::ET_SCROLL);
  EXPECT_EQ(1, delegate_->dismiss_count());
}

// Tests that typing text after opening transitions from peeking to half.
TEST_F(AppListViewPeekingTest, TypingPeekingToHalf) {
  Initialize(false /*is_tablet_mode*/);
  views::Textfield* search_box =
      view_->app_list_main_view()->search_box_view()->search_box();

  Show();
  search_box->SetText(std::u16string());
  search_box->InsertText(
      u"nice",
      ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);

  ASSERT_EQ(ash::AppListViewState::kHalf, view_->app_list_state());
}

// Tests that typing when in fullscreen changes the state to fullscreen search.
TEST_F(AppListViewPeekingTest, TypingFullscreenToFullscreenSearch) {
  Initialize(false /*is_tablet_mode*/);
  Show();
  view_->SetState(ash::AppListViewState::kFullscreenAllApps);

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
  Initialize(true /*is_tablet_mode*/);
  views::Textfield* search_box =
      view_->app_list_main_view()->search_box_view()->search_box();

  Show();
  search_box->SetText(std::u16string());
  search_box->InsertText(
      u"cool!",
      ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);

  ASSERT_EQ(ash::AppListViewState::kFullscreenSearch, view_->app_list_state());
}

// Tests that pressing escape when in peeking closes the app list.
TEST_F(AppListViewPeekingTest, EscapeKeyPeekingToClosed) {
  Initialize(false /*is_tablet_mode*/);

  Show();
  view_->AcceleratorPressed(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));

  ASSERT_EQ(1, delegate_->dismiss_count());
}

// Tests that pressing escape when in half screen changes the state to peeking.
TEST_F(AppListViewPeekingTest, EscapeKeyHalfToPeeking) {
  Initialize(false /*is_tablet_mode*/);

  Show();
  SetTextInSearchBox(u"doggie");
  view_->AcceleratorPressed(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));

  ASSERT_EQ(ash::AppListViewState::kPeeking, view_->app_list_state());
}

// Tests that pressing escape when in fullscreen changes the state to closed.
TEST_F(AppListViewPeekingTest, EscapeKeyFullscreenToClosed) {
  Initialize(false /*is_tablet_mode*/);
  view_->SetState(ash::AppListViewState::kFullscreenAllApps);

  Show();
  view_->AcceleratorPressed(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));

  ASSERT_EQ(1, delegate_->dismiss_count());
}

// Tests that pressing escape when in fullscreen side-shelf closes the app list.
TEST_F(AppListViewPeekingTest, EscapeKeySideShelfFullscreenToClosed) {
  // Put into fullscreen by using side-shelf.
  Initialize(false /*is_tablet_mode*/);

  Show(/*is_side_shelf=*/true);
  view_->AcceleratorPressed(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));

  ASSERT_EQ(1, delegate_->dismiss_count());
}

// Tests that pressing escape when in tablet mode keeps app list in fullscreen.
TEST_P(AppListViewTabletTest, EscapeKeyTabletModeStayFullscreen) {
  // Put into fullscreen by using tablet mode.
  Initialize(/*is_tablet_mode=*/true);

  Show();
  view_->AcceleratorPressed(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));

  ASSERT_EQ(ash::AppListViewState::kFullscreenAllApps, view_->app_list_state());
}

// Tests that pressing escape when in fullscreen search changes to fullscreen.
TEST_F(AppListViewPeekingTest, EscapeKeyFullscreenSearchToFullscreen) {
  Initialize(false /*is_tablet_mode*/);
  Show();
  view_->SetState(ash::AppListViewState::kFullscreenAllApps);

  SetTextInSearchBox(u"https://youtu.be/dQw4w9WgXcQ");
  view_->AcceleratorPressed(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));

  ASSERT_EQ(ash::AppListViewState::kFullscreenAllApps, view_->app_list_state());
}

// Tests that pressing escape when in sideshelf search changes to fullscreen.
TEST_F(AppListViewPeekingTest, EscapeKeySideShelfSearchToFullscreen) {
  // Put into fullscreen using side-shelf.
  Initialize(false /*is_tablet_mode*/);

  Show(true /*is_side_shelf*/);
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
  Initialize(true /*is_tablet_mode*/);

  Show();
  SetTextInSearchBox(u"yay");
  view_->AcceleratorPressed(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));

  ASSERT_EQ(ash::AppListViewState::kFullscreenAllApps, view_->app_list_state());
}

// Tests that opening in peeking mode sets the correct height.
TEST_F(AppListViewPeekingTest, OpenInPeekingCorrectHeight) {
  Initialize(false /*is_tablet_mode*/);

  Show();
  view_->SetState(AppListViewState::kPeeking);
  ASSERT_EQ(view_->GetHeightForState(AppListViewState::kPeeking),
            view_->GetCurrentAppListHeight());
}

// Tests that opening in fullscreen mode sets the correct height.
TEST_F(AppListViewPeekingTest, OpenInFullscreenCorrectHeight) {
  Initialize(false /*is_tablet_mode*/);

  Show();
  view_->SetState(ash::AppListViewState::kFullscreenAllApps);
  const int y = view_->GetWidget()->GetWindowBoundsInScreen().y();
  ASSERT_EQ(0, y);
}

// Tests that AppListView::SetState succeeds when the state has been set to
// CLOSED.
TEST_F(AppListViewPeekingTest, SetStateFailsWhenClosing) {
  Initialize(false /*is_tablet_mode*/);
  Show();
  view_->SetState(ash::AppListViewState::kClosed);

  view_->SetState(ash::AppListViewState::kFullscreenAllApps);

  ASSERT_EQ(ash::AppListViewState::kFullscreenAllApps, view_->app_list_state());
}

TEST_F(AppListViewPeekingTest, AppsGridViewVisibilityOnReopening) {
  Initialize(false /*is_tablet_mode*/);
  Show();
  view_->SetState(ash::AppListViewState::kFullscreenAllApps);
  EXPECT_TRUE(IsViewVisibleOnScreen(apps_grid_view()));

  view_->SetState(ash::AppListViewState::kFullscreenSearch);
  SetAppListState(ash::AppListState::kStateSearchResults);
  EXPECT_TRUE(IsViewVisibleOnScreen(apps_grid_view()));

  // Close the app-list and re-show to fullscreen all apps.
  view_->SetState(ash::AppListViewState::kClosed);
  Show();
  view_->SetState(ash::AppListViewState::kFullscreenAllApps);
  EXPECT_TRUE(IsViewVisibleOnScreen(apps_grid_view()));
}

TEST_F(AppListViewPeekingTest, AppsGridViewExpandHintingOnReopening) {
  ui::ScopedAnimationDurationScaleMode non_zero_duration(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  Initialize(false /*is_tablet_mode*/);

  Show();
  view_->SetState(ash::AppListViewState::kPeeking);
  EXPECT_TRUE(
      contents_view()->expand_arrow_view()->IsHintingAnimationRunningForTest());

  view_->SetState(ash::AppListViewState::kClosed);
  EXPECT_FALSE(
      contents_view()->expand_arrow_view()->IsHintingAnimationRunningForTest());

  Show();
  view_->SetState(ash::AppListViewState::kPeeking);
  EXPECT_TRUE(
      contents_view()->expand_arrow_view()->IsHintingAnimationRunningForTest());
}

// Tests that going into a folder view, then setting the AppListState to PEEKING
// hides the folder view.
TEST_F(AppListViewPeekingTest, FolderViewToPeeking) {
  Initialize(false /*is_tablet_mode*/);
  AppListTestModel* model = delegate_->GetTestModel();
  model->PopulateApps(kInitialItems);
  const std::string folder_id =
      model->MergeItems(model->top_level_item_list()->item_at(0)->id(),
                        model->top_level_item_list()->item_at(1)->id());
  model->FindFolderItem(folder_id);
  Show();
  AppsGridViewTestApi test_api(view_->app_list_main_view()
                                   ->contents_view()
                                   ->apps_container_view()
                                   ->apps_grid_view());
  test_api.PressItemAt(0);
  EXPECT_TRUE(view_->app_list_main_view()
                  ->contents_view()
                  ->apps_container_view()
                  ->IsInFolderView());

  view_->SetState(ash::AppListViewState::kPeeking);

  EXPECT_FALSE(view_->app_list_main_view()
                   ->contents_view()
                   ->apps_container_view()
                   ->IsInFolderView());
}

// Tests that a tap or click in an empty region of the AppsGridView closes the
// AppList. ProductivityLauncher does not have this behavior.
TEST_F(AppListViewPeekingTest, TapAndClickWithinAppsGridView) {
  Initialize(false /*is_tablet_mode*/);
  // Populate the AppList with a small number of apps so there is an empty
  // region to click.
  delegate_->GetTestModel()->PopulateApps(6);
  Show();
  view_->SetState(ash::AppListViewState::kFullscreenAllApps);
  EXPECT_EQ(ash::AppListViewState::kFullscreenAllApps, view_->app_list_state());
  AppsGridView* apps_grid_view = view_->app_list_main_view()
                                     ->contents_view()
                                     ->apps_container_view()
                                     ->apps_grid_view();
  AppsGridViewTestApi test_api(apps_grid_view);

  // Get the point of the first empty region (where app #7 would be) and tap on
  // it, the AppList should close.
  const gfx::Point empty_region =
      test_api.GetItemTileRectOnCurrentPageAt(2, 2).CenterPoint();
  ui::GestureEvent tap(empty_region.x(), empty_region.y(), 0, base::TimeTicks(),
                       ui::GestureEventDetails(ui::ET_GESTURE_TAP));
  ui::Event::DispatcherApi tap_dispatcher_api(static_cast<ui::Event*>(&tap));
  tap_dispatcher_api.set_target(view_);
  view_->OnGestureEvent(&tap);
  ASSERT_EQ(1, delegate_->dismiss_count());

  Show();

  // Tap on the same empty region, the AppList should close again.
  ui::MouseEvent mouse_click(ui::ET_MOUSE_PRESSED, empty_region, empty_region,
                             base::TimeTicks(), 0, 0);
  auto mouse_click_dispatcher_api = std::make_unique<ui::Event::DispatcherApi>(
      static_cast<ui::Event*>(&mouse_click));
  mouse_click_dispatcher_api->set_target(view_);
  view_->OnMouseEvent(&mouse_click);
  ui::MouseEvent mouse_release(ui::ET_MOUSE_RELEASED, empty_region,
                               empty_region, base::TimeTicks(), 0, 0);
  mouse_click_dispatcher_api =
      std::make_unique<ui::Event::DispatcherApi>(&mouse_release);
  mouse_click_dispatcher_api->set_target(view_);
  view_->OnMouseEvent(&mouse_release);
  ASSERT_EQ(2, delegate_->dismiss_count());
}

// Tests that search box should not become a rectangle during drag.
TEST_F(AppListViewPeekingTest, SearchBoxCornerRadiusDuringDragging) {
  base::HistogramTester histogram_tester;
  Initialize(false /*is_tablet_mode*/);
  delegate_->GetTestModel()->PopulateApps(kInitialItems);
  Show();
  view_->SetState(ash::AppListViewState::kFullscreenAllApps);
  EXPECT_EQ(ash::AppListViewState::kFullscreenAllApps, view_->app_list_state());
  histogram_tester.ExpectTotalCount(
      "Apps.StateTransition.Drag.PresentationTime.ClamshellMode", 0);

  // Send SCROLL_START and SCROLL_UPDATE events, simulating dragging the
  // launcher.
  base::TimeTicks timestamp = base::TimeTicks::Now();
  gfx::Point start = view_->GetWidget()->GetWindowBoundsInScreen().top_right();
  int delta_y = 1;
  ui::GestureEvent start_event = ui::GestureEvent(
      start.x(), start.y(), ui::EF_NONE, timestamp,
      ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_BEGIN, 0, delta_y));
  view_->OnGestureEvent(&start_event);
  histogram_tester.ExpectTotalCount(
      "Apps.StateTransition.Drag.PresentationTime.ClamshellMode", 0);

  // Drag down the launcher.
  timestamp += base::Milliseconds(25);
  delta_y += 10;
  start.Offset(0, 1);
  ui::GestureEvent update_event = ui::GestureEvent(
      start.x(), start.y(), ui::EF_NONE, timestamp,
      ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_UPDATE, 0, delta_y));
  view_->OnGestureEvent(&update_event);

  EXPECT_TRUE(IsStateShown(ash::AppListState::kStateApps));
  EXPECT_EQ(kSearchBoxBorderCornerRadius,
            search_box_view()->GetSearchBoxBorderCornerRadiusForState(
                ash::AppListState::kStateApps));

  // Search box should keep |kSearchBoxCornerRadiusFullscreen| corner radius
  // during drag.
  EXPECT_TRUE(SetAppListState(ash::AppListState::kStateSearchResults));
  EXPECT_TRUE(view_->is_in_drag());
  EXPECT_EQ(kSearchBoxBorderCornerRadius,
            search_box_view()->GetSearchBoxBorderCornerRadiusForState(
                ash::AppListState::kStateSearchResults));
  histogram_tester.ExpectTotalCount(
      "Apps.StateTransition.Drag.PresentationTime.ClamshellMode", 1);
  histogram_tester.ExpectTotalCount(
      "Apps.StateTransition.Drag.PresentationTime.MaxLatency.ClamshellMode", 0);

  // Ends to drag the launcher.
  EXPECT_TRUE(SetAppListState(ash::AppListState::kStateApps));
  timestamp += base::Milliseconds(25);
  start.Offset(0, 1);
  ui::GestureEvent end_event =
      ui::GestureEvent(start.x(), start.y() + delta_y, ui::EF_NONE, timestamp,
                       ui::GestureEventDetails(ui::ET_GESTURE_END));
  view_->OnGestureEvent(&end_event);

  // Search box should keep |kSearchBoxCornerRadiusFullscreen| corner radius
  // if launcher drag finished.
  EXPECT_FALSE(view_->is_in_drag());
  EXPECT_EQ(kSearchBoxBorderCornerRadius,
            search_box_view()->GetSearchBoxBorderCornerRadiusForState(
                ash::AppListState::kStateApps));
  histogram_tester.ExpectTotalCount(
      "Apps.StateTransition.Drag.PresentationTime.ClamshellMode", 1);
  histogram_tester.ExpectTotalCount(
      "Apps.StateTransition.Drag.PresentationTime.MaxLatency.ClamshellMode", 1);
}

// Tests displaying the app list and performs a standard set of checks on its
// top level views.
TEST_F(AppListViewPeekingTest, DisplayTest) {
  Initialize(/*is_tablet_mode=*/false);
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

// As above above, but tests tablet mode with and without ProductivityLauncher.
TEST_P(AppListViewTabletTest, DisplayTest) {
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
// ProductivityLauncher has tests of animated page transitions in the tests for
// AppListBubbleView and AppListBubbleAppsPage.
TEST_F(AppListViewPeekingTest, PageSwitchingAnimationTest) {
  ui::ScopedAnimationDurationScaleMode non_zero_duration(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  Initialize(/*is_tablet_mode=*/false);
  Show();
  AppListMainView* main_view = view_->app_list_main_view();
  // Checks on the main view.
  EXPECT_NO_FATAL_FAILURE(CheckView(main_view));
  EXPECT_NO_FATAL_FAILURE(CheckView(main_view->contents_view()));

  ContentsView* contents_view = main_view->contents_view();

  contents_view->SetActiveState(ash::AppListState::kStateApps);
  contents_view->Layout();
  EXPECT_TRUE(IsStateShown(ash::AppListState::kStateApps));

  // Change pages. Animation start triggers layout, and updates the page UI.
  contents_view->ShowSearchResults(true);
  EXPECT_TRUE(IsStateShown(ash::AppListState::kStateSearchResults));

  // Change back to the apps container.
  contents_view->SetActiveState(ash::AppListState::kStateApps);
  EXPECT_TRUE(IsStateShown(ash::AppListState::kStateApps));

  // Verify that search results are shown when going back to search results
  // page.
  contents_view->ShowSearchResults(true);
  EXPECT_TRUE(IsStateShown(ash::AppListState::kStateSearchResults));
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
  contents_view->Layout();
  EXPECT_TRUE(
      contents_view->IsStateActive(ash::AppListState::kStateSearchResults));

  EXPECT_TRUE(IsStateShown(ash::AppListState::kStateSearchResults));

  // Hide the search results.
  contents_view->ShowSearchResults(false);
  contents_view->Layout();

  // Check that we return to the page that we were on before the search.
  EXPECT_TRUE(IsStateShown(ash::AppListState::kStateApps));

  view_->Layout();
  EXPECT_TRUE(IsStateShown(ash::AppListState::kStateApps));

  std::u16string search_text = u"test";
  main_view->search_box_view()->search_box()->SetText(std::u16string());
  main_view->search_box_view()->search_box()->InsertText(
      search_text,
      ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  // Check that the current search is using |search_text|.
  EXPECT_EQ(search_text, main_view->search_box_view()->search_box()->GetText());
  EXPECT_EQ(search_text, main_view->search_box_view()->current_query());
  contents_view->Layout();
  EXPECT_TRUE(
      contents_view->IsStateActive(ash::AppListState::kStateSearchResults));
  EXPECT_TRUE(CheckSearchBoxWidget(contents_view->GetSearchBoxBounds(
      ash::AppListState::kStateSearchResults)));

  // Check that typing into the search box triggers the search page.
  EXPECT_TRUE(SetAppListState(ash::AppListState::kStateApps));
  contents_view->Layout();
  EXPECT_TRUE(IsStateShown(ash::AppListState::kStateApps));
  EXPECT_TRUE(CheckSearchBoxWidget(
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
  contents_view->Layout();
  EXPECT_TRUE(IsStateShown(ash::AppListState::kStateSearchResults));
  EXPECT_TRUE(CheckSearchBoxWidget(contents_view->GetSearchBoxBounds(
      ash::AppListState::kStateSearchResults)));
}

// Tests that a context menu can be shown between app icons in tablet mode.
TEST_P(AppListViewTabletTest, ShowContextMenuBetweenAppsInTabletMode) {
  Initialize(true /*is_tablet_mode*/);
  delegate_->GetTestModel()->PopulateApps(kInitialItems);
  Show();

  // Tap between two apps in tablet mode.
  const gfx::Point middle = GetPointBetweenTwoApps();
  ui::GestureEvent tap(middle.x(), middle.y(), 0, base::TimeTicks(),
                       ui::GestureEventDetails(ui::ET_GESTURE_TWO_FINGER_TAP));
  view_->OnGestureEvent(&tap);

  // The wallpaper context menu should show.
  EXPECT_EQ(1, show_wallpaper_context_menu_count());
  EXPECT_TRUE(view_->GetWidget()->IsVisible());

  // Click between two apps in tablet mode.
  ui::MouseEvent click_mouse_event(
      ui::ET_MOUSE_PRESSED, middle, middle, ui::EventTimeForNow(),
      ui::EF_RIGHT_MOUSE_BUTTON, ui::EF_RIGHT_MOUSE_BUTTON);
  view_->OnMouseEvent(&click_mouse_event);
  ui::MouseEvent release_mouse_event(
      ui::ET_MOUSE_RELEASED, middle, middle, ui::EventTimeForNow(),
      ui::EF_RIGHT_MOUSE_BUTTON, ui::EF_RIGHT_MOUSE_BUTTON);
  view_->OnMouseEvent(&release_mouse_event);

  // The wallpaper context menu should show.
  EXPECT_EQ(2, show_wallpaper_context_menu_count());
  EXPECT_TRUE(view_->GetWidget()->IsVisible());
}

// Tests that context menus are not shown between app icons in clamshell mode.
TEST_F(AppListViewPeekingTest, DontShowContextMenuBetweenAppsInClamshellMode) {
  Initialize(false /* disable tablet mode */);
  delegate_->GetTestModel()->PopulateApps(kInitialItems);
  Show();

  // Tap between two apps in clamshell mode.
  const gfx::Point middle = GetPointBetweenTwoApps();
  ui::GestureEvent tap(middle.x(), middle.y(), 0, base::TimeTicks(),
                       ui::GestureEventDetails(ui::ET_GESTURE_TWO_FINGER_TAP));
  view_->OnGestureEvent(&tap);

  // The wallpaper menu should not show.
  EXPECT_EQ(0, show_wallpaper_context_menu_count());
  EXPECT_TRUE(view_->GetWidget()->IsVisible());

  // Right click between two apps in clamshell mode.
  ui::MouseEvent mouse_event(ui::ET_MOUSE_PRESSED, middle, middle,
                             ui::EventTimeForNow(), ui::EF_RIGHT_MOUSE_BUTTON,
                             ui::EF_RIGHT_MOUSE_BUTTON);
  view_->OnMouseEvent(&mouse_event);

  // The wallpaper menu should not show.
  EXPECT_EQ(0, show_wallpaper_context_menu_count());
  EXPECT_TRUE(view_->GetWidget()->IsVisible());
}

// Tests the back action in home launcher.
TEST_F(AppListViewTest, BackAction) {
  // Put into fullscreen using tablet mode.
  Initialize(true /*is_tablet_mode*/);

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
  ui::MouseEvent event(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                       ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                       ui::EF_LEFT_MOUSE_BUTTON);
  apps_container_view->folder_background_view()->OnMouseEvent(&event);
  EXPECT_FALSE(apps_container_view->IsInFolderView());
}

// Tests that, in clamshell mode, the current app list page resets to the
// initial page when app list is closed and re-opened.
TEST_F(AppListViewTest, InitialPageResetClamshellModeTest) {
  Initialize(false /*is_tablet_mode*/);

  AppListTestModel* model = delegate_->GetTestModel();
  const int kAppListItemNum =
      SharedAppListConfig::instance().GetMaxNumOfItemsPerPage() + 1;
  model->PopulateApps(kAppListItemNum);

  Show();
  view_->SetState(ash::AppListViewState::kFullscreenAllApps);

  apps_grid_view()->pagination_model()->SelectPage(1, false /* animate */);

  // Close and re-open the app list to ensure the current page doesn't persist.
  view_->SetState(ash::AppListViewState::kClosed);
  Show();
  view_->SetState(ash::AppListViewState::kFullscreenAllApps);

  EXPECT_EQ(0, apps_grid_view()->pagination_model()->selected_page());
}

// Tests that, in tablet mode, the current app list page doesn't immediately
// reset to the initial page when app list is closed and re-opened.
TEST_P(AppListViewTabletTest, PagePersistanceTabletModeTest) {
  Initialize(true /*is_tablet_mode*/);

  AppListTestModel* model = delegate_->GetTestModel();
  const int kAppListItemNum =
      SharedAppListConfig::instance().GetMaxNumOfItemsPerPage() + 1;
  model->PopulateApps(kAppListItemNum);

  Show();
  EXPECT_EQ(ash::AppListViewState::kFullscreenAllApps, view_->app_list_state());

  apps_grid_view()->pagination_model()->SelectPage(1, false /* animate */);

  // Close and re-open the app list to ensure the current page persists.
  view_->SetState(ash::AppListViewState::kClosed);
  Show();
  EXPECT_EQ(ash::AppListViewState::kFullscreenAllApps, view_->app_list_state());

  // The current page should not be reset for the tablet mode app list.
  EXPECT_EQ(1, apps_grid_view()->pagination_model()->selected_page());
}

// Tests selecting search result to show embedded Assistant UI.
// TODO(https://crbug.com/1280300): Figure out if ProductivityLauncher needs a
// version of this test. ProductivityLauncherSearchView has its own test suite.
TEST_F(AppListViewPeekingFocusTest, ShowEmbeddedAssistantUI) {
  Show();

  // Initially the search box is inactive, hitting Enter to activate it.
  EXPECT_FALSE(search_box_view()->is_search_box_active());
  SimulateKeyPress(ui::VKEY_RETURN, false);
  EXPECT_TRUE(search_box_view()->is_search_box_active());

  // Type something in search box to transition to HALF state and populate
  // fake list results. Then hit Enter key.
  search_box_view()->search_box()->InsertText(
      u"test",
      ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  const int kListResults = 2;
  const int kIndexOpenAssistantUi = 1;
  SetUpSearchResultsForAssistantUI(kListResults, kIndexOpenAssistantUi);
  SimulateKeyPress(ui::VKEY_RETURN, false);
  EXPECT_EQ(1, GetOpenFirstSearchResultCount());
  EXPECT_EQ(1, GetTotalOpenSearchResultCount());
  EXPECT_EQ(0, GetTotalOpenAssistantUICount());

  SearchResultListView* list_view = GetSearchResultListView();
  ui::KeyEvent key_event(ui::ET_KEY_PRESSED, ui::VKEY_RETURN, ui::EF_NONE);
  list_view->GetResultViewAt(kIndexOpenAssistantUi)->OnKeyEvent(&key_event);
  EXPECT_EQ(1, GetOpenFirstSearchResultCount());
  EXPECT_EQ(2, GetTotalOpenSearchResultCount());
  EXPECT_EQ(1, GetTotalOpenAssistantUICount());
}

// Tests that the correct contents is visible in the contents_view upon
// reshowing. See b/142069648 for the details.
TEST_F(AppListViewPeekingTest, AppsGridVisibilityOnResetForShow) {
  Initialize(true /*is_tablet_mode*/);
  Show();

  contents_view()->ShowEmbeddedAssistantUI(true);
  EXPECT_FALSE(contents_view()->apps_container_view()->GetVisible());
  EXPECT_FALSE(contents_view()->search_result_page_view()->GetVisible());
  EXPECT_TRUE(assistant_page_view()->GetVisible());

  view_->OnTabletModeChanged(false);
  Show();
  EXPECT_TRUE(contents_view()->apps_container_view()->GetVisible());
  EXPECT_FALSE(contents_view()->search_result_page_view()->GetVisible());
  EXPECT_FALSE(assistant_page_view()->GetVisible());
}

// Tests that pressing escape in embedded Assistant UI returns to fullscreen
// if the Assistant UI was launched from fullscreen app list.
TEST_F(AppListViewTest, EscapeKeyInEmbeddedAssistantUIReturnsToAppList) {
  Initialize(false /*is_tablet_mode*/);
  Show();

  // First we're in the fullscreen app list
  view_->SetState(ash::AppListViewState::kFullscreenAllApps);
  // Then we go to search by entering text
  SetTextInSearchBox(u"search query");
  // From there we launch the Assistant UI
  contents_view()->ShowEmbeddedAssistantUI(true);

  // We press escape to leave the Assistant UI
  view_->AcceleratorPressed(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));

  // And we should be back in the fullscreen app list
  EXPECT_FALSE(contents_view()->IsShowingSearchResults());
  EXPECT_EQ(ash::AppListViewState::kFullscreenAllApps, view_->app_list_state());
}

// Tests that pressing escape in embedded Assistant UI returns to peeking
// if the Assistant UI was launched from half screen.
TEST_F(AppListViewPeekingTest, EscapeKeyInEmbeddedAssistantUIReturnsToPeeking) {
  Initialize(false /*is_tablet_mode*/);
  Show();

  // Enter half screen search by entering text
  SetTextInSearchBox(u"search query");
  // From there we launch the Assistant UI
  contents_view()->ShowEmbeddedAssistantUI(true);

  // We press escape to leave the Assistant UI
  view_->AcceleratorPressed(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));

  // And we should be back in the peeking state
  EXPECT_FALSE(contents_view()->IsShowingSearchResults());
  EXPECT_EQ(ash::AppListViewState::kPeeking, view_->app_list_state());
}

// Tests that clicking empty region in AppListview when showing Assistant UI
// should go back to peeking state.
TEST_F(AppListViewPeekingTest, ClickOutsideEmbeddedAssistantUIToPeeking) {
  Initialize(false /*is_tablet_mode*/);
  Show();

  // Set search_box_view active.
  ui::KeyEvent key_event(ui::ET_KEY_PRESSED, ui::VKEY_RETURN, ui::EF_NONE);
  view_->GetWidget()->OnKeyEvent(&key_event);

  contents_view()->ShowEmbeddedAssistantUI(true);
  EXPECT_TRUE(contents_view()->IsShowingEmbeddedAssistantUI());

  // Click on the same empty region, the AppList should be peeking state.
  const gfx::Point empty_region = view_->GetBoundsInScreen().origin();
  ui::MouseEvent mouse_click(ui::ET_MOUSE_PRESSED, empty_region, empty_region,
                             base::TimeTicks(), 0, 0);
  auto mouse_click_dispatcher_api =
      std::make_unique<ui::Event::DispatcherApi>(&mouse_click);
  mouse_click_dispatcher_api->set_target(view_);
  view_->OnMouseEvent(&mouse_click);
  ui::MouseEvent mouse_release(ui::ET_MOUSE_RELEASED, empty_region,
                               empty_region, base::TimeTicks(), 0, 0);
  mouse_click_dispatcher_api =
      std::make_unique<ui::Event::DispatcherApi>(&mouse_release);
  mouse_click_dispatcher_api->set_target(view_);
  view_->OnMouseEvent(&mouse_release);
  EXPECT_EQ(ash::AppListViewState::kPeeking, view_->app_list_state());
}

// Tests that expand arrow is not visible when showing embedded Assistant UI.
// ProductivityLauncher does not have an expand arrow.
TEST_F(AppListViewPeekingTest, ExpandArrowNotVisibleInEmbeddedAssistantUI) {
  Initialize(false /*is_tablet_mode*/);
  Show();

  // Set search_box_view active.
  ui::KeyEvent key_event(ui::ET_KEY_PRESSED, ui::VKEY_RETURN, ui::EF_NONE);
  view_->GetWidget()->OnKeyEvent(&key_event);

  contents_view()->ShowEmbeddedAssistantUI(true);
  EXPECT_TRUE(contents_view()->IsShowingEmbeddedAssistantUI());
  EXPECT_EQ(0.0f,
            contents_view()->expand_arrow_view()->layer()->GetTargetOpacity());
}

// Tests the expand arrow view opacity updates correctly when transitioning
// between various app list view states. ProductivityLauncher does not have an
// expand arrow.
TEST_F(AppListViewPeekingTest, ExpandArrowViewVisibilityTest) {
  Initialize(false /*is_tablet_mode*/);
  Show();

  view_->SetState(ash::AppListViewState::kClosed);
  // Expand arrow should not be visible when  app list view state is closed.
  ASSERT_EQ(contents_view()->expand_arrow_view()->layer()->opacity(), 0.0f);
  // Expand arrow view should be visible for peeking launcher.
  view_->SetState(ash::AppListViewState::kPeeking);
  ASSERT_EQ(contents_view()->expand_arrow_view()->layer()->opacity(), 1.0f);

  // Expand arrow view should not be visible for half launcher when showing
  // embedded assistant.
  contents_view()->ShowEmbeddedAssistantUI(true);
  ASSERT_EQ(contents_view()->expand_arrow_view()->layer()->opacity(), 0.0f);
  // Expand arrow should become visible when hiding the assistant view.
  contents_view()->ShowEmbeddedAssistantUI(false);
  EXPECT_TRUE(contents_view()->expand_arrow_view()->GetVisible());
  ASSERT_EQ(contents_view()->expand_arrow_view()->layer()->opacity(), 1.0f);

  // Typing text in the search box should hide the expand arrow view.
  SetTextInSearchBox(u"https://youtu.be/dQw4w9WgXcQ");
  ASSERT_EQ(contents_view()->expand_arrow_view()->layer()->opacity(), 0.0f);
  // Pressing escape should show the expand arrow view again.
  view_->AcceleratorPressed(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));
  ASSERT_EQ(contents_view()->expand_arrow_view()->layer()->opacity(), 1.0f);
}

// Tests the expand arrow view opacity updates correctly when transitioning
// between various app list view states with app list state animations enabled.
// ProductivityLauncher does not have an expand arrow.
TEST_F(AppListViewPeekingTest,
       ExpandArrowViewVisibilityWithStateAnimationsTest) {
  Initialize(false /*is_tablet_mode*/);
  Show();

  ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Expand arrow view should be visible for peeking launcher.
  EXPECT_EQ(1.0f,
            contents_view()->expand_arrow_view()->layer()->GetTargetOpacity());

  // Expand arrow view should not be visible for half launcher when showing
  // embedded assistant.
  contents_view()->ShowEmbeddedAssistantUI(true);
  EXPECT_EQ(0.0f,
            contents_view()->expand_arrow_view()->layer()->GetTargetOpacity());

  // Expand arrow should become visible when hiding the assistant view.
  contents_view()->ShowEmbeddedAssistantUI(false);
  EXPECT_TRUE(contents_view()->expand_arrow_view()->GetVisible());
  EXPECT_EQ(1.0f,
            contents_view()->expand_arrow_view()->layer()->GetTargetOpacity());

  // Typing text in the search box should hide the expand arrow view.
  SetTextInSearchBox(u"https://youtu.be/dQw4w9WgXcQ");
  EXPECT_EQ(0.0f,
            contents_view()->expand_arrow_view()->layer()->GetTargetOpacity());

  // Pressing escape should show the expand arrow view again.
  view_->AcceleratorPressed(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));
  EXPECT_EQ(1.0f,
            contents_view()->expand_arrow_view()->layer()->GetTargetOpacity());
}

// Tests that search box is not visible when showing embedded Assistant UI.
// ProductivityLauncher has tests for this in AppListBubbleViewTest.
TEST_F(AppListViewPeekingTest, SearchBoxViewNotVisibleInEmbeddedAssistantUI) {
  Initialize(false /*is_tablet_mode*/);
  Show();

  EXPECT_TRUE(search_box_view()->GetWidget()->IsVisible());

  contents_view()->ShowEmbeddedAssistantUI(true);

  EXPECT_TRUE(contents_view()->IsShowingEmbeddedAssistantUI());
  EXPECT_FALSE(search_box_view()->GetWidget()->IsVisible());
}

// Tests that the expand arrow cannot be seen when opening the app list with
// side shelf enabled. ProductivityLauncher does not have an expand arrow.
TEST_F(AppListViewPeekingTest, ExpandArrowNotVisibleWithSideShelf) {
  Initialize(false /*is_tablet_mode*/);

  Show(true /*is_side_shelf*/);

  EXPECT_EQ(0.0f, contents_view()->expand_arrow_view()->layer()->opacity());
}

TEST_F(ProductivityLauncherAppListViewLayoutTest, RegularLandscapeScreen) {
  const gfx::Size window_size = gfx::Size(1000, 800);
  GetContext()->SetBounds(gfx::Rect(window_size));

  InitializeAppList();

  const int expected_horizontal_margin =
      kMinProductivityLauncherGridHorizontalMargin;
  const gfx::Size expected_item_size(96, 120);

  {
    SCOPED_TRACE("Only apps grid");
    EXPECT_EQ(4, apps_grid_view()->GetRowsForTesting());
    VerifyAppsContainerLayoutForProductivityLauncher(
        window_size, /*row_count=*/4, expected_horizontal_margin,
        expected_item_size, /*has_recent_apps=*/false);
  }

  AddRecentApps(4);
  contents_view()->ResetForShow();

  {
    SCOPED_TRACE("With recent apps");
    EXPECT_EQ(4, apps_grid_view()->GetRowsForTesting());
    VerifyAppsContainerLayoutForProductivityLauncher(
        window_size, 3 /*row_count*/, expected_horizontal_margin,
        expected_item_size, /*has_recent_apps=*/true);
  }
}

TEST_F(ProductivityLauncherAppListViewLayoutTest,
       RegularLandscapeScreenAtMinPreferredVerticalMargin) {
  const int window_height = GetExpectedScreenSizeForProductivityLauncher(
      /*row_count=*/4, /*tile_height=*/120, /*tile_margins=*/8,
      /*is_large_height=*/false);
  EXPECT_EQ(689, window_height);
  const gfx::Size window_size = gfx::Size(800, window_height);
  GetContext()->SetBounds(gfx::Rect(window_size));

  InitializeAppList();

  const int expected_horizontal_margin =
      kMinProductivityLauncherGridHorizontalMargin;
  const gfx::Size expected_item_size(96, 120);

  {
    SCOPED_TRACE("Only apps grid");
    EXPECT_EQ(4, apps_grid_view()->GetRowsForTesting());
    VerifyAppsContainerLayoutForProductivityLauncher(
        window_size, /*row_count=*/4, expected_horizontal_margin,
        expected_item_size, /*has_recent_apps=*/false);
  }

  AddRecentApps(4);
  contents_view()->ResetForShow();

  {
    SCOPED_TRACE("With recent apps");
    EXPECT_EQ(4, apps_grid_view()->GetRowsForTesting());
    VerifyAppsContainerLayoutForProductivityLauncher(
        window_size, 2 /*row_count*/, expected_horizontal_margin,
        expected_item_size, /*has_recent_apps=*/true);
  }
}

TEST_F(ProductivityLauncherAppListViewLayoutTest,
       RegularLandscapeScreenWithRemovedRows) {
  const int window_height = GetExpectedScreenSizeForProductivityLauncher(
                                /*row_count=*/4, /*tile_height=*/120,
                                /*tile_margins=*/8, /*is_large_height=*/false) -
                            4;
  EXPECT_EQ(685, window_height);
  const gfx::Size window_size = gfx::Size(800, window_height);
  GetContext()->SetBounds(gfx::Rect(window_size));

  InitializeAppList();

  const int expected_horizontal_margin =
      kMinProductivityLauncherGridHorizontalMargin;
  const gfx::Size expected_item_size(96, 120);

  {
    SCOPED_TRACE("Only apps grid");
    EXPECT_EQ(3, apps_grid_view()->GetRowsForTesting());
    VerifyAppsContainerLayoutForProductivityLauncher(
        window_size, 3 /*row_count*/, expected_horizontal_margin,
        expected_item_size, /*has_recent_apps=*/false);
  }

  AddRecentApps(4);
  contents_view()->ResetForShow();

  {
    SCOPED_TRACE("With recent apps");
    EXPECT_EQ(3, apps_grid_view()->GetRowsForTesting());
    VerifyAppsContainerLayoutForProductivityLauncher(
        window_size, 2 /*row_count*/, expected_horizontal_margin,
        expected_item_size, /*has_recent_apps=*/true);
  }
}

TEST_F(ProductivityLauncherAppListViewLayoutTest,
       RegularLandscapeScreenAtMaxPreferredVerticalMargin) {
  const int window_height = GetExpectedScreenSizeForProductivityLauncher(
      /*row_count=*/4, /*tile_height=*/120, /*tile_margins=*/96,
      /*is_large_height=*/true);
  EXPECT_EQ(1024, window_height);
  const gfx::Size window_size = gfx::Size(1100, window_height);
  GetContext()->SetBounds(gfx::Rect(window_size));

  InitializeAppList();

  const int expected_horizontal_margin =
      kMinProductivityLauncherGridHorizontalMargin;
  const gfx::Size expected_item_size(96, 120);

  {
    SCOPED_TRACE("Only apps grid");
    EXPECT_EQ(4, apps_grid_view()->GetRowsForTesting());
    VerifyAppsContainerLayoutForProductivityLauncher(
        window_size, /*row_count=*/4, expected_horizontal_margin,
        expected_item_size, /*has_recent_apps=*/false);
  }

  AddRecentApps(4);
  contents_view()->ResetForShow();

  {
    SCOPED_TRACE("With recent apps");
    EXPECT_EQ(4, apps_grid_view()->GetRowsForTesting());
    VerifyAppsContainerLayoutForProductivityLauncher(
        window_size, 4 /*row_count*/, expected_horizontal_margin,
        expected_item_size, /*has_recent_apps=*/true);
  }
}

TEST_F(ProductivityLauncherAppListViewLayoutTest,
       RegularLandscapeScreenWithAddedRows) {
  const int window_height = GetExpectedScreenSizeForProductivityLauncher(
                                /*row_count=*/4, /*tile_height=*/120,
                                /*tile_margins=*/96, /*is_large_height=*/true) +
                            6;
  EXPECT_EQ(1030, window_height);
  const gfx::Size window_size = gfx::Size(1100, window_height);
  GetContext()->SetBounds(gfx::Rect(window_size));

  InitializeAppList();

  const int expected_horizontal_margin =
      kMinProductivityLauncherGridHorizontalMargin;
  const gfx::Size expected_item_size(96, 120);

  {
    SCOPED_TRACE("Only apps grid");
    EXPECT_EQ(5, apps_grid_view()->GetRowsForTesting());
    VerifyAppsContainerLayoutForProductivityLauncher(
        window_size, /*row_count=*/5, expected_horizontal_margin,
        expected_item_size, /*has_recent_apps=*/false);
  }

  AddRecentApps(4);
  contents_view()->ResetForShow();

  {
    SCOPED_TRACE("With recent apps");
    EXPECT_EQ(5, apps_grid_view()->GetRowsForTesting());
    VerifyAppsContainerLayoutForProductivityLauncher(
        window_size, 4 /*row_count*/, expected_horizontal_margin,
        expected_item_size, /*has_recent_apps=*/true);
  }
}

TEST_F(ProductivityLauncherAppListViewLayoutTest, RegularPortraitScreen) {
  const gfx::Size window_size = gfx::Size(800, 1000);
  GetContext()->SetBounds(gfx::Rect(window_size));

  InitializeAppList();

  const int expected_horizontal_margin =
      kMinProductivityLauncherGridHorizontalMargin;
  const gfx::Size expected_item_size(96, 120);

  {
    SCOPED_TRACE("Only apps grid");
    EXPECT_EQ(5, apps_grid_view()->GetRowsForTesting());
    VerifyAppsContainerLayoutForProductivityLauncher(
        window_size, /*row_count=*/5, expected_horizontal_margin,
        expected_item_size, /*has_recent_apps=*/false);
  }

  AddRecentApps(4);
  contents_view()->ResetForShow();

  {
    SCOPED_TRACE("With recent apps");
    EXPECT_EQ(5, apps_grid_view()->GetRowsForTesting());
    VerifyAppsContainerLayoutForProductivityLauncher(
        window_size, /*row_count=*/4, expected_horizontal_margin,
        expected_item_size, /*has_recent_apps=*/true);
  }
}

TEST_F(ProductivityLauncherAppListViewLayoutTest,
       RegularPortraitScreenAtMinPreferredVerticalMargin) {
  int window_height = GetExpectedScreenSizeForProductivityLauncher(
      /*row_count=*/5, /*tile_height=*/120, /*tile_margins=*/8,
      /*is_large_height=*/true);
  // window_height = 860;
  EXPECT_EQ(868, window_height);
  const gfx::Size window_size = gfx::Size(700, window_height);
  GetContext()->SetBounds(gfx::Rect(window_size));

  InitializeAppList();

  const int expected_horizontal_margin =
      kMinProductivityLauncherGridHorizontalMargin;
  const gfx::Size expected_item_size(96, 120);

  {
    SCOPED_TRACE("Only apps grid");
    EXPECT_EQ(5, apps_grid_view()->GetRowsForTesting());
    VerifyAppsContainerLayoutForProductivityLauncher(
        window_size, /*row_count=*/5, expected_horizontal_margin,
        expected_item_size, /*has_recent_apps=*/false);
  }

  AddRecentApps(4);
  contents_view()->ResetForShow();

  {
    SCOPED_TRACE("With recent apps");
    EXPECT_EQ(5, apps_grid_view()->GetRowsForTesting());
    VerifyAppsContainerLayoutForProductivityLauncher(
        window_size, 3 /*row_count*/, expected_horizontal_margin,
        expected_item_size, /*has_recent_apps=*/true);
  }
}

TEST_F(ProductivityLauncherAppListViewLayoutTest,
       RegularPortraitScreenWithRemovedRows) {
  const int window_height =
      GetExpectedScreenSizeForProductivityLauncher(
          /*row_count=*/5, /*tile_height=*/120, /*tile_margins=*/8,
          /*is_large_height=*/true) -
      8;
  EXPECT_EQ(860, window_height);
  const gfx::Size window_size = gfx::Size(700, window_height);
  GetContext()->SetBounds(gfx::Rect(window_size));

  InitializeAppList();

  const int expected_horizontal_margin =
      kMinProductivityLauncherGridHorizontalMargin;
  const gfx::Size expected_item_size(96, 120);

  {
    SCOPED_TRACE("Only apps grid");
    EXPECT_EQ(4, apps_grid_view()->GetRowsForTesting());
    VerifyAppsContainerLayoutForProductivityLauncher(
        window_size, /*row_count=*/4, expected_horizontal_margin,
        expected_item_size, /*has_recent_apps=*/false);
  }

  AddRecentApps(4);
  contents_view()->ResetForShow();

  {
    SCOPED_TRACE("With recent apps");
    EXPECT_EQ(4, apps_grid_view()->GetRowsForTesting());
    VerifyAppsContainerLayoutForProductivityLauncher(
        window_size, 3 /*row_count*/, expected_horizontal_margin,
        expected_item_size, /*has_recent_apps=*/true);
  }
}

TEST_F(ProductivityLauncherAppListViewLayoutTest,
       RegularPortraitScreenAtMaxPreferredVerticalMargin) {
  const int window_height = GetExpectedScreenSizeForProductivityLauncher(
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
    VerifyAppsContainerLayoutForProductivityLauncher(
        window_size, /*row_count=*/5, expected_horizontal_margin,
        expected_item_size, /*has_recent_apps=*/false);
  }

  AddRecentApps(4);
  contents_view()->ResetForShow();

  {
    SCOPED_TRACE("With recent apps");
    EXPECT_EQ(5, apps_grid_view()->GetRowsForTesting());
    VerifyAppsContainerLayoutForProductivityLauncher(
        window_size, /*row_count=*/4, expected_horizontal_margin,
        expected_item_size, /*has_recent_apps=*/true);
  }
}

TEST_F(ProductivityLauncherAppListViewLayoutTest,
       RegularPortraitScreenWithExtraRows) {
  const int window_height =
      GetExpectedScreenSizeForProductivityLauncher(
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
    VerifyAppsContainerLayoutForProductivityLauncher(
        window_size, /*row_count=*/6, expected_horizontal_margin,
        expected_item_size, /*has_recent_apps=*/false);
  }

  AddRecentApps(4);
  contents_view()->ResetForShow();

  {
    SCOPED_TRACE("With recent apps");
    EXPECT_EQ(6, apps_grid_view()->GetRowsForTesting());
    VerifyAppsContainerLayoutForProductivityLauncher(
        window_size, /*row_count=*/4, expected_horizontal_margin,
        expected_item_size, /*has_recent_apps=*/true);
  }
}

TEST_F(ProductivityLauncherAppListViewLayoutTest, DenseLandscapeScreen) {
  const gfx::Size window_size = gfx::Size(800, 600);
  GetContext()->SetBounds(gfx::Rect(window_size));

  InitializeAppList();

  const int expected_horizontal_margin =
      kMinProductivityLauncherGridHorizontalMargin;
  const gfx::Size expected_item_size(80, 88);

  {
    SCOPED_TRACE("Only apps grid");
    EXPECT_EQ(4, apps_grid_view()->GetRowsForTesting());
    VerifyAppsContainerLayoutForProductivityLauncher(
        window_size, /*row_count=*/4, expected_horizontal_margin,
        expected_item_size, /*has_recent_apps=*/false);
  }

  AddRecentApps(4);
  contents_view()->ResetForShow();

  {
    SCOPED_TRACE("With recent apps");
    EXPECT_EQ(4, apps_grid_view()->GetRowsForTesting());
    VerifyAppsContainerLayoutForProductivityLauncher(
        window_size, 3 /*row_count*/, expected_horizontal_margin,
        expected_item_size, /*has_recent_apps=*/true);
  }
}

TEST_F(ProductivityLauncherAppListViewLayoutTest,
       DenseLandscapeScreenAtMinPreferredVerticalMargin) {
  const int window_height = GetExpectedScreenSizeForProductivityLauncher(
      /*row_count=*/4, /*tile_height=*/88, /*tile_margins=*/8,
      /*is_large_height=*/false);
  EXPECT_EQ(552, window_height);
  const gfx::Size window_size = gfx::Size(800, window_height);
  GetContext()->SetBounds(gfx::Rect(window_size));

  InitializeAppList();

  const int expected_horizontal_margin =
      kMinProductivityLauncherGridHorizontalMargin;
  const gfx::Size expected_item_size(80, 88);

  {
    SCOPED_TRACE("Only apps grid");
    EXPECT_EQ(4, apps_grid_view()->GetRowsForTesting());
    VerifyAppsContainerLayoutForProductivityLauncher(
        window_size, /*row_count=*/4, expected_horizontal_margin,
        expected_item_size, /*has_recent_apps=*/false);
  }

  AddRecentApps(4);
  contents_view()->ResetForShow();

  {
    SCOPED_TRACE("With recent apps");
    EXPECT_EQ(4, apps_grid_view()->GetRowsForTesting());
    VerifyAppsContainerLayoutForProductivityLauncher(
        window_size, 2 /*row_count*/, expected_horizontal_margin,
        expected_item_size, /*has_recent_apps=*/true);
  }
}

TEST_F(ProductivityLauncherAppListViewLayoutTest,
       DenseLandscapeScreenWithRemovedRows) {
  const int window_height =
      GetExpectedScreenSizeForProductivityLauncher(
          /*row_count=*/4, /*tile_height=*/88, /*tile_margins=*/8,
          /*large_height*/ false) -
      4;
  EXPECT_EQ(548, window_height);
  const gfx::Size window_size = gfx::Size(800, window_height);
  GetContext()->SetBounds(gfx::Rect(window_size));

  InitializeAppList();

  const int expected_horizontal_margin =
      kMinProductivityLauncherGridHorizontalMargin;
  const gfx::Size expected_item_size(80, 88);

  {
    SCOPED_TRACE("Only apps grid");
    EXPECT_EQ(3, apps_grid_view()->GetRowsForTesting());
    VerifyAppsContainerLayoutForProductivityLauncher(
        window_size, 3 /*row_count*/, expected_horizontal_margin,
        expected_item_size, /*has_recent_apps=*/false);
  }

  AddRecentApps(4);
  contents_view()->ResetForShow();

  {
    SCOPED_TRACE("With recent apps");
    EXPECT_EQ(3, apps_grid_view()->GetRowsForTesting());
    VerifyAppsContainerLayoutForProductivityLauncher(
        window_size, 2 /*row_count*/, expected_horizontal_margin,
        expected_item_size, /*has_recent_apps=*/true);
  }
}

TEST_F(ProductivityLauncherAppListViewLayoutTest, DensePortraitScreen) {
  const gfx::Size window_size = gfx::Size(600, 800);
  GetContext()->SetBounds(gfx::Rect(window_size));

  InitializeAppList();

  const int expected_horizontal_margin =
      kMinProductivityLauncherGridHorizontalMargin;
  const gfx::Size expected_item_size(80, 88);

  {
    SCOPED_TRACE("Only apps grid");
    EXPECT_EQ(5, apps_grid_view()->GetRowsForTesting());
    VerifyAppsContainerLayoutForProductivityLauncher(
        window_size, /*row_count=*/5, expected_horizontal_margin,
        expected_item_size, /*has_recent_apps=*/false);
  }

  AddRecentApps(4);
  contents_view()->ResetForShow();

  {
    SCOPED_TRACE("With recent apps");
    EXPECT_EQ(5, apps_grid_view()->GetRowsForTesting());
    VerifyAppsContainerLayoutForProductivityLauncher(
        window_size, /*row_count=*/4, expected_horizontal_margin,
        expected_item_size, /*has_recent_apps=*/true);
  }
}

TEST_F(ProductivityLauncherAppListViewLayoutTest,
       DensePortraitScreenAtMinPreferredVerticalMargin) {
  const int window_height = GetExpectedScreenSizeForProductivityLauncher(
      /*row_count=*/5, /*tile_height=*/88, /*tile_margins=*/8,
      /*large_height*/ false);
  EXPECT_EQ(654, window_height);
  const gfx::Size window_size = gfx::Size(600, window_height);
  GetContext()->SetBounds(gfx::Rect(window_size));

  InitializeAppList();

  const int expected_horizontal_margin =
      kMinProductivityLauncherGridHorizontalMargin;
  const gfx::Size expected_item_size(80, 88);

  {
    SCOPED_TRACE("Only apps grid");
    EXPECT_EQ(5, apps_grid_view()->GetRowsForTesting());
    VerifyAppsContainerLayoutForProductivityLauncher(
        window_size, /*row_count=*/5, expected_horizontal_margin,
        expected_item_size, /*has_recent_apps=*/false);
  }

  AddRecentApps(4);
  contents_view()->ResetForShow();

  {
    SCOPED_TRACE("With recent apps");
    EXPECT_EQ(5, apps_grid_view()->GetRowsForTesting());
    VerifyAppsContainerLayoutForProductivityLauncher(
        window_size, 3 /*row_count*/, expected_horizontal_margin,
        expected_item_size, /*has_recent_apps=*/true);
  }
}

TEST_F(ProductivityLauncherAppListViewLayoutTest,
       DensePortraitScreenWithRemovedRows) {
  const int window_height = GetExpectedScreenSizeForProductivityLauncher(
                                /*row_count=*/5, /*tile_height=*/88,
                                /*tile_margins=*/8, /*large_height*/ false) -
                            8;
  EXPECT_EQ(646, window_height);
  const gfx::Size window_size = gfx::Size(540, window_height);
  GetContext()->SetBounds(gfx::Rect(window_size));

  InitializeAppList();

  const int expected_horizontal_margin =
      kMinProductivityLauncherGridHorizontalMargin;
  const gfx::Size expected_item_size(80, 88);

  {
    SCOPED_TRACE("Only apps grid");
    EXPECT_EQ(4, apps_grid_view()->GetRowsForTesting());
    VerifyAppsContainerLayoutForProductivityLauncher(
        window_size, /*row_count=*/4, expected_horizontal_margin,
        expected_item_size, /*has_recent_apps=*/false);
  }

  AddRecentApps(4);
  contents_view()->ResetForShow();

  {
    SCOPED_TRACE("With recent apps");
    EXPECT_EQ(4, apps_grid_view()->GetRowsForTesting());
    VerifyAppsContainerLayoutForProductivityLauncher(
        window_size, 3 /*row_count*/, expected_horizontal_margin,
        expected_item_size, /*has_recent_apps=*/true);
  }
}

TEST_F(ProductivityLauncherAppListViewLayoutTest,
       DensePortraitScreenAtMaxPreferredVerticalMargin) {
  const int window_height = GetExpectedScreenSizeForProductivityLauncher(
      /*row_count=*/5, /*tile_height=*/88, /*tile_margins=*/96,
      /*large_height*/ true);
  EXPECT_EQ(1088, window_height);
  const gfx::Size window_size = gfx::Size(601, window_height);
  GetContext()->SetBounds(gfx::Rect(window_size));

  InitializeAppList();

  const int expected_horizontal_margin =
      kMinProductivityLauncherGridHorizontalMargin;
  const gfx::Size expected_item_size(80, 88);

  {
    SCOPED_TRACE("Only apps grid");
    EXPECT_EQ(5, apps_grid_view()->GetRowsForTesting());
    VerifyAppsContainerLayoutForProductivityLauncher(
        window_size, /*row_count=*/5, expected_horizontal_margin,
        expected_item_size, /*has_recent_apps=*/false);
  }

  AddRecentApps(4);
  contents_view()->ResetForShow();

  {
    SCOPED_TRACE("With recent apps");
    EXPECT_EQ(5, apps_grid_view()->GetRowsForTesting());
    VerifyAppsContainerLayoutForProductivityLauncher(
        window_size, /*row_count=*/4, expected_horizontal_margin,
        expected_item_size, /*has_recent_apps=*/true);
  }
}

TEST_F(ProductivityLauncherAppListViewLayoutTest,
       DensePortraitScreenWithExtraRows) {
  const int window_height = GetExpectedScreenSizeForProductivityLauncher(
                                /*row_count=*/5, /*tile_height=*/88,
                                /*tile_margins=*/96, /*large_height*/ true) +
                            4;
  EXPECT_EQ(1092, window_height);
  const gfx::Size window_size = gfx::Size(601, window_height);
  GetContext()->SetBounds(gfx::Rect(window_size));

  InitializeAppList();

  const int expected_horizontal_margin =
      kMinProductivityLauncherGridHorizontalMargin;
  const gfx::Size expected_item_size(80, 88);

  {
    SCOPED_TRACE("Only apps grid");
    EXPECT_EQ(6, apps_grid_view()->GetRowsForTesting());
    VerifyAppsContainerLayoutForProductivityLauncher(
        window_size, /*row_count=*/6, expected_horizontal_margin,
        expected_item_size, /*has_recent_apps=*/false);
  }

  AddRecentApps(4);
  contents_view()->ResetForShow();

  {
    SCOPED_TRACE("With recent apps");
    EXPECT_EQ(6, apps_grid_view()->GetRowsForTesting());
    VerifyAppsContainerLayoutForProductivityLauncher(
        window_size, /*row_count=*/4, expected_horizontal_margin,
        expected_item_size, /*has_recent_apps=*/true);
  }
}

TEST_F(ProductivityLauncherAppListViewLayoutTest,
       DenseAppsGridPaddingScaledDownToMakeRoomForPageSwitcher) {
  // Select window width so using non-zero horizontal padding would result in
  // lack of space for the page switcher.
  const gfx::Size window_size = gfx::Size(512, 800);
  GetContext()->SetBounds(gfx::Rect(window_size));

  InitializeAppList();

  const int expected_horizontal_margin =
      kMinProductivityLauncherGridHorizontalMargin;
  const gfx::Size expected_item_size(80, 88);
  EXPECT_EQ(5, apps_grid_view()->GetRowsForTesting());
  VerifyAppsContainerLayoutForProductivityLauncher(
      window_size, /*row_count=*/5, expected_horizontal_margin,
      expected_item_size, /*has_recent_apps=*/false);
}

TEST_F(ProductivityLauncherAppListViewLayoutTest,
       DenseAppsGridScaledDownToMakeRoomForPageSwitcher) {
  // Select window width so using default icon width would result in lack of
  // space for the page switcher.
  const gfx::Size window_size = gfx::Size(442, 800);
  GetContext()->SetBounds(gfx::Rect(window_size));

  InitializeAppList();

  const int expected_horizontal_margin =
      kMinProductivityLauncherGridHorizontalMargin;
  const gfx::Size expected_item_size(66, 88);
  EXPECT_EQ(5, apps_grid_view()->GetRowsForTesting());
  VerifyAppsContainerLayoutForProductivityLauncher(
      window_size, /*row_count=*/5, expected_horizontal_margin,
      expected_item_size, /*has_recent_apps=*/false);
}

TEST_F(ProductivityLauncherAppListViewLayoutTest,
       DenseAppsGridWithMaxHorizontalItemMargins) {
  // Select window width that results in apps grid layout with max allowed
  // horizontal margin (128): 2 * 56 (min horizontal margin) + 4 * 128 + 5 * 80
  const gfx::Size window_size = gfx::Size(984, 600);
  GetContext()->SetBounds(gfx::Rect(window_size));

  InitializeAppList();

  const int expected_horizontal_margin =
      kMinProductivityLauncherGridHorizontalMargin;
  const gfx::Size expected_item_size(80, 88);
  EXPECT_EQ(4, apps_grid_view()->GetRowsForTesting());
  VerifyAppsContainerLayoutForProductivityLauncher(
      window_size, /*row_count=*/4, expected_horizontal_margin,
      expected_item_size, /*has_recent_apps=*/false);
}

TEST_F(ProductivityLauncherAppListViewLayoutTest,
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
  VerifyAppsContainerLayoutForProductivityLauncher(
      window_size, /*row_count=*/4, expected_horizontal_margin,
      expected_item_size, /*has_recent_apps=*/false);
}

TEST_F(ProductivityLauncherAppListViewLayoutTest,
       RegularAppsGridWithMaxHorizontalItemMargins) {
  // Select window width that results in apps grid layout with max allowed
  // horizontal margin (128):
  // 2 * 56 (min horizontal margin) + 4 * 128 + 5 * 96
  const gfx::Size window_size = gfx::Size(1104, 1200);
  GetContext()->SetBounds(gfx::Rect(window_size));

  InitializeAppList();

  const int expected_horizontal_margin =
      kMinProductivityLauncherGridHorizontalMargin;
  const gfx::Size expected_item_size(96, 120);
  EXPECT_EQ(5, apps_grid_view()->GetRowsForTesting());
  VerifyAppsContainerLayoutForProductivityLauncher(
      window_size, /*row_count=*/5, expected_horizontal_margin,
      expected_item_size, /*has_recent_apps=*/false);
}

TEST_F(ProductivityLauncherAppListViewLayoutTest,
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
  VerifyAppsContainerLayoutForProductivityLauncher(
      window_size, /*row_count=*/5, expected_horizontal_margin,
      expected_item_size, /*has_recent_apps=*/false);
}

TEST_F(ProductivityLauncherAppListViewLayoutTest, LayoutAfterConfigChange) {
  const gfx::Size window_size = gfx::Size(600, 800);
  GetContext()->SetBounds(gfx::Rect(window_size));

  InitializeAppList();

  const int expected_horizontal_margin =
      kMinProductivityLauncherGridHorizontalMargin;
  const gfx::Size expected_item_size(80, 88);
  EXPECT_EQ(5, apps_grid_view()->GetRowsForTesting());
  VerifyAppsContainerLayoutForProductivityLauncher(
      window_size, /*row_count=*/5, expected_horizontal_margin,
      expected_item_size, /*has_recent_apps=*/false);

  const gfx::Size updated_window_size = gfx::Size(1000, 800);
  GetContext()->SetBounds(gfx::Rect(updated_window_size));
  view_->OnParentWindowBoundsChanged();

  const gfx::Size expected_updated_item_size(96, 120);
  EXPECT_EQ(4, apps_grid_view()->GetRowsForTesting());
  VerifyAppsContainerLayoutForProductivityLauncher(
      updated_window_size, /*row_count=*/4, expected_horizontal_margin,
      expected_updated_item_size, /*has_recent_apps=*/false);
}

TEST_F(ProductivityLauncherAppListViewLayoutTest,
       LayoutAfterConfigChangeWithRecentApps) {
  const gfx::Size window_size = gfx::Size(600, 800);
  GetContext()->SetBounds(gfx::Rect(window_size));

  InitializeAppList();
  AddRecentApps(4);
  contents_view()->ResetForShow();

  const int expected_horizontal_margin =
      kMinProductivityLauncherGridHorizontalMargin;
  const gfx::Size expected_item_size(80, 88);
  EXPECT_EQ(5, apps_grid_view()->GetRowsForTesting());
  VerifyAppsContainerLayoutForProductivityLauncher(
      window_size, /*row_count=*/4, expected_horizontal_margin,
      expected_item_size, /*has_recent_apps=*/true);

  const gfx::Size updated_window_size = gfx::Size(1000, 800);
  GetContext()->SetBounds(gfx::Rect(updated_window_size));
  view_->OnParentWindowBoundsChanged();

  const gfx::Size expected_updated_item_size(96, 120);
  EXPECT_EQ(4, apps_grid_view()->GetRowsForTesting());
  VerifyAppsContainerLayoutForProductivityLauncher(
      updated_window_size, /*row_count=*/3, expected_horizontal_margin,
      expected_updated_item_size, /*has_recent_apps=*/true);
}

// Tests fullscreen apps grid sizing and layout for small screens (width < 960)
// in landscape layout.
TEST_F(LegacyLauncherAppListViewLayoutTest,
       AppListViewLayoutForSmallLandscapeScreen) {
  const gfx::Size window_size = gfx::Size(800, 600);
  gfx::NativeView parent = GetContext();
  parent->SetBounds(gfx::Rect(window_size));

  Initialize(false /*is_tablet_mode*/);
  delegate_->GetTestModel()->PopulateApps(kInitialItems);
  Show();
  view_->SetState(ash::AppListViewState::kFullscreenAllApps);

  const int expected_vertical_margin =
      (window_size.height() - ShelfSize()) / 16;
  VerifyAppsContainerLayout(
      window_size, 5 /*column_count*/, 4 /*row_count*/,
      window_size.width() / 12 /*expected_horizontal_margin*/,
      expected_vertical_margin, 80 /*expected_item_size*/);
}

// Tests fullscreen apps grid sizing and layout for small screens (width < 600)
// in portrait layout.
TEST_F(LegacyLauncherAppListViewLayoutTest,
       AppListViewLayoutForSmallPortraitScreen) {
  const gfx::Size window_size = gfx::Size(500, 800);
  gfx::NativeView parent = GetContext();
  parent->SetBounds(gfx::Rect(window_size));

  Initialize(false /*is_tablet_mode*/);
  delegate_->GetTestModel()->PopulateApps(kInitialItems);
  Show();
  view_->SetState(ash::AppListViewState::kFullscreenAllApps);

  const int expected_vertical_margin =
      (window_size.height() - ShelfSize()) / 16;
  VerifyAppsContainerLayout(window_size, 4 /*column_count*/, 5 /*row_count*/,
                            56 /*expected_horizontal_margin*/,
                            expected_vertical_margin,
                            80 /*expected_item_size*/);
}

// Tests fullscreen apps grid sizing and layout for medium sized screens
// (width < 1200) in lanscape layout.
TEST_F(LegacyLauncherAppListViewLayoutTest,
       AppListViewLayoutForMediumLandscapeScreen) {
  const gfx::Size window_size = gfx::Size(960, 800);
  gfx::NativeView parent = GetContext();
  parent->SetBounds(gfx::Rect(window_size));

  Initialize(false /*is_tablet_mode*/);
  delegate_->GetTestModel()->PopulateApps(kInitialItems);
  Show();
  view_->SetState(ash::AppListViewState::kFullscreenAllApps);

  // Horizontal margin should be set so apps grid doesn't go over the max size.
  const int expected_horizontal_margin =
      (window_size.width() - GetItemGridSizeWithMaxItemMargins(88, 5)) / 2;
  const int expected_vertical_margin =
      (window_size.height() - ShelfSize()) / 16;
  VerifyAppsContainerLayout(window_size, 5 /*column_count*/, 4 /*row_count*/,
                            expected_horizontal_margin,
                            expected_vertical_margin,
                            88 /*expected_item_size*/);
}

// Tests fullscreen apps grid sizing and layout for medium sized screens
// (width < 768) in portrait layout.
TEST_F(LegacyLauncherAppListViewLayoutTest,
       AppListViewLayoutForMediumPortraitScreen) {
  const gfx::Size window_size = gfx::Size(700, 800);
  gfx::NativeView parent = GetContext();
  parent->SetBounds(gfx::Rect(window_size));

  Initialize(false /*is_tablet_mode*/);
  delegate_->GetTestModel()->PopulateApps(kInitialItems);
  Show();
  view_->SetState(ash::AppListViewState::kFullscreenAllApps);

  const int expected_vertical_margin =
      (window_size.height() - ShelfSize()) / 16;
  VerifyAppsContainerLayout(
      window_size, 4 /*column_count*/, 5 /*row_count*/,
      window_size.width() / 12 /*expected_horizontal_margin*/,
      expected_vertical_margin, 88 /*expected_item_size*/);
}

// Tests fullscreen apps grid sizing and layout for large screens
// (width >= 1200) in landscape layout.
TEST_F(LegacyLauncherAppListViewLayoutTest,
       AppListViewLayoutForLargeLandscapeScreen) {
  const gfx::Size window_size = gfx::Size(1200, 960);
  gfx::NativeView parent = GetContext();
  parent->SetBounds(gfx::Rect(window_size));

  Initialize(false /*is_tablet_mode*/);
  delegate_->GetTestModel()->PopulateApps(kInitialItems);
  Show();
  view_->SetState(ash::AppListViewState::kFullscreenAllApps);

  // Horizontal margin should be set so apps grid doesn't go over the max size.
  const int expected_horizontal_margin =
      (window_size.width() - GetItemGridSizeWithMaxItemMargins(120, 5)) / 2;
  const int expected_vertical_margin =
      (window_size.height() - ShelfSize()) / 16;
  VerifyAppsContainerLayout(window_size, 5 /*column_count*/, 4 /*row_count*/,
                            expected_horizontal_margin,
                            expected_vertical_margin,
                            120 /*expected_item_size*/);
}

// Tests fullscreen apps grid sizing and layout for large screens (width >= 768)
// in portrait layout.
TEST_F(LegacyLauncherAppListViewLayoutTest,
       AppListViewLayoutForLargePortraitScreen) {
  const gfx::Size window_size = gfx::Size(800, 1200);
  gfx::NativeView parent = GetContext();
  parent->SetBounds(gfx::Rect(window_size));

  Initialize(false /*is_tablet_mode*/);
  delegate_->GetTestModel()->PopulateApps(kInitialItems);
  Show();
  view_->SetState(ash::AppListViewState::kFullscreenAllApps);

  const int expected_vertical_margin =
      (window_size.height() - ShelfSize()) / 16;
  VerifyAppsContainerLayout(
      window_size, 4 /*column_count*/, 5 /*row_count*/,
      window_size.width() / 12 /*expected_horizontal_margin*/,
      expected_vertical_margin, 120 /*expected_item_size*/);
}

// Tests that apps grid horizontal margin have minimum that ensures the page
// switcher view can fit next to the apps grid.
TEST_F(LegacyLauncherAppListViewLayoutTest,
       EnsurePageSwitcherFitsAppsGridMargin) {
  const gfx::Size window_size = gfx::Size(440, 800);
  gfx::NativeView parent = GetContext();
  parent->SetBounds(gfx::Rect(window_size));

  Initialize(false /*is_tablet_mode*/);
  delegate_->GetTestModel()->PopulateApps(kInitialItems);
  Show();
  view_->SetState(ash::AppListViewState::kFullscreenAllApps);

  const int expected_vertical_margin =
      (window_size.height() - ShelfSize()) / 16;
  // The horizontal margin is selected so the page switcher fits the margin
  // space (note that 440 / 12, which is how the margin is normally calculated
  // is smaller than the width required by page switcher).
  VerifyAppsContainerLayout(window_size, 4 /*column_count*/, 5 /*row_count*/,
                            56 /*expected_horizontal_margin*/,
                            expected_vertical_margin,
                            80 /*expected_item_size*/);
}

// Verifies that the vertical spacing between items in apps grid has an upper
// limit, and that the apps grid is centered in the available space if item
// spacing hits that limit.
TEST_F(LegacyLauncherAppListViewLayoutTest,
       VerticalAppsGridItemSpacingIsBounded) {
  const gfx::Size window_size = gfx::Size(960, 1600);
  gfx::NativeView parent = GetContext();
  parent->SetBounds(gfx::Rect(window_size));

  Initialize(false /*is_tablet_mode*/);
  delegate_->GetTestModel()->PopulateApps(kInitialItems);
  Show();
  view_->SetState(ash::AppListViewState::kFullscreenAllApps);

  // Horizontal margin should be set so apps grid doesn't go over the max size.
  const int expected_horizontal_margin =
      (window_size.width() - GetItemGridSizeWithMaxItemMargins(120, 4)) / 2;
  const int expected_vertical_margin =
      (window_size.height() - ShelfSize() - kGridVerticalInset -
       kSearchBoxAndSuggestionChipsHeightDefault - kGridVerticalMargin -
       GetItemGridSizeWithMaxItemMargins(120, 5)) /
      2;
  VerifyAppsContainerLayout(window_size, 4 /*column_count*/, 5 /*row_count*/,
                            expected_horizontal_margin,
                            expected_vertical_margin,
                            120 /*expected_item_size*/);
}

// Verifies that the vertical apps container margin is big enough to fit the
// apps grid fadeout area.
TEST_F(LegacyLauncherAppListViewLayoutTest,
       VerticalAppsContainerMarginFitFadeoutArea) {
  const gfx::Size window_size(650, 536);
  gfx::NativeView parent = GetContext();
  parent->SetBounds(gfx::Rect(window_size));

  Initialize(false /*is_tablet_mode*/);
  delegate_->GetTestModel()->PopulateApps(kInitialItems);
  Show();
  view_->SetState(ash::AppListViewState::kFullscreenAllApps);

  // The horizontal margin is selected so the page switcher fits the margin
  // space (note that 650 / 12, which is how the margin is normally calculated
  // is smaller than the width required by page switcher).
  VerifyAppsContainerLayout(
      window_size, 5 /*column_count*/, 4 /*row_count*/,
      56 /*expected_horizontal_margin*/,
      kGridVerticalInset + kGridVerticalMargin /*expected_vertical_margin*/,
      80 /*expected_item_size*/);
}

// Tests fullscreen apps grid sizing and layout gets updated to correct bounds
// when app list config changes.
TEST_F(LegacyLauncherAppListViewLayoutTest,
       AppListViewLayoutAfterConfigChange) {
  const gfx::Size window_size = gfx::Size(500, 800);
  gfx::NativeView parent = GetContext();
  parent->SetBounds(gfx::Rect(window_size));

  Initialize(false /*is_tablet_mode*/);
  delegate_->GetTestModel()->PopulateApps(kInitialItems);
  Show();
  view_->SetState(ash::AppListViewState::kFullscreenAllApps);

  int expected_vertical_margin = (window_size.height() - ShelfSize()) / 16;
  VerifyAppsContainerLayout(window_size, 4 /*column_count*/, 5 /*row_count*/,
                            56 /*expected_horizontal_margin*/,
                            expected_vertical_margin,
                            80 /*expected_item_size*/);

  const gfx::Size updated_window_size = gfx::Size(800, 1200);
  parent->SetBounds(gfx::Rect(updated_window_size));
  view_->OnParentWindowBoundsChanged();

  expected_vertical_margin = (updated_window_size.height() - ShelfSize()) / 16;
  VerifyAppsContainerLayout(
      updated_window_size, 4 /*column_count*/, 5 /*row_count*/,
      updated_window_size.width() / 12 /*expected_horizontal_margin*/,
      expected_vertical_margin, 120 /*expected_item_size*/);
}

// Tests that page switching in folder doesn't record AppListPageSwitcherSource
// metric. ProductivityLauncher does not use pages in folders.
TEST_F(AppListViewPeekingFocusTest, PageSwitchingNotRecordingMetric) {
  base::HistogramTester histogram_tester;
  Show();

  histogram_tester.ExpectTotalCount("Apps.AppListPageSwitcherSource", 0);
  // Transition to kFullscreenAllApps state and open the folder.
  SetAppListState(ash::AppListViewState::kFullscreenAllApps);
  folder_item_view()->RequestFocus();
  SimulateKeyPress(ui::VKEY_RETURN, false);
  ASSERT_TRUE(contents_view()->apps_container_view()->IsInFolderView());

  // Create a fling to the left so the folder view changes page.
  constexpr float kFlingVelocityForChangingPage = 850.0f;
  gfx::Point location = app_list_folder_view()->bounds().CenterPoint();
  ui::GestureEventDetails details = ui::GestureEventDetails(
      ui::ET_SCROLL_FLING_START, -kFlingVelocityForChangingPage, 0);
  ui::GestureEvent event = ui::GestureEvent(
      location.x(), location.y(), ui::EF_NONE, base::TimeTicks::Now(), details);
  app_list_folder_view()->items_grid_view()->OnGestureEvent(&event);

  ASSERT_TRUE(event.handled());
  histogram_tester.ExpectTotalCount("Apps.AppListPageSwitcherSource", 0);
}

}  // namespace
}  // namespace ash
