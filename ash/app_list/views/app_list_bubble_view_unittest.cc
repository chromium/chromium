// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/app_list_bubble_view.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/app_list/app_list_bubble_presenter.h"
#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/model/app_list_test_model.h"
#include "ash/app_list/model/search/test_search_result.h"
#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/app_list/test_app_list_client.h"
#include "ash/app_list/views/app_list_bubble_apps_page.h"
#include "ash/app_list/views/app_list_bubble_search_page.h"
#include "ash/app_list/views/app_list_folder_view.h"
#include "ash/app_list/views/assistant/app_list_bubble_assistant_page.h"
#include "ash/app_list/views/continue_section_view.h"
#include "ash/app_list/views/continue_task_view.h"
#include "ash/app_list/views/recent_apps_view.h"
#include "ash/app_list/views/scrollable_apps_grid_view.h"
#include "ash/app_list/views/search_box_view.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/style/color_provider.h"
#include "ash/shell.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/test/ash_test_base.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/layer.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

using views::Widget;

namespace ash {
namespace {

void AddSearchResult(const std::string& id, const std::u16string& title) {
  auto search_result = std::make_unique<TestSearchResult>();
  search_result->set_result_id(id);
  search_result->set_display_type(SearchResultDisplayType::kList);
  search_result->set_title(title);
  Shell::Get()->app_list_controller()->GetSearchModel()->results()->Add(
      std::move(search_result));
}

void AddRecentApps(int num_apps) {
  auto* search_model = Shell::Get()->app_list_controller()->GetSearchModel();
  for (int i = 0; i < num_apps; i++) {
    auto result = std::make_unique<TestSearchResult>();
    // Use the same "Item #" convention as AppListTestModel uses. The search
    // result IDs must match app item IDs in the app list data model.
    result->set_result_id(base::StringPrintf("Item %d", i));
    result->set_result_type(AppListSearchResultType::kInstalledApp);
    // TODO(crbug.com/1216662): Replace with a real display type after the ML
    // team gives us a way to query directly for recent apps.
    result->set_display_type(SearchResultDisplayType::kChip);
    search_model->results()->Add(std::move(result));
  }
}

void AddContinueSuggestionResult(int num_suggestions) {
  auto* search_model = Shell::Get()->app_list_controller()->GetSearchModel();
  for (int i = 0; i < num_suggestions; i++) {
    auto result = std::make_unique<TestSearchResult>();
    result->set_result_id(base::NumberToString(i));
    result->set_result_type(AppListSearchResultType::kFileChip);
    // TODO(crbug.com/1216662): Replace with a real display type after the ML
    // team gives us a way to query directly for recent apps.
    result->set_display_type(SearchResultDisplayType::kChip);
    search_model->results()->Add(std::move(result));
  }
}

AppListBubblePresenter* GetBubblePresenter() {
  return Shell::Get()->app_list_controller()->bubble_presenter_for_test();
}

views::View* GetSeparator() {
  return GetBubblePresenter()->bubble_view_for_test()->separator_for_test();
}

// Simulates the Assistant being enabled.
void SimulateAssistantEnabled() {
  Shell::Get()
      ->app_list_controller()
      ->GetSearchModel()
      ->search_box()
      ->SetShowAssistantButton(true);
}

class AppListBubbleViewTest : public AshTestBase {
 public:
  AppListBubbleViewTest() {
    scoped_features_.InitAndEnableFeature(features::kProductivityLauncher);
  }
  ~AppListBubbleViewTest() override = default;

  // testing::Test:
  void SetUp() override {
    AshTestBase::SetUp();
    auto model = std::make_unique<test::AppListTestModel>();
    app_list_test_model_ = model.get();
    Shell::Get()->app_list_controller()->SetAppListModelForTest(
        std::move(model));
  }

  // Shows the app list on the primary display.
  void ShowAppList() { GetAppListTestHelper()->ShowAppList(); }

  void AddAppItems(int num_items) {
    app_list_test_model_->PopulateApps(num_items);
  }

  void AddFolderWithApps(int count) {
    app_list_test_model_->CreateAndPopulateFolderWithApps(count);
  }

  void LeftClickOn(views::View* view) {
    GetEventGenerator()->MoveMouseTo(view->GetBoundsInScreen().CenterPoint());
    GetEventGenerator()->ClickLeftButton();
  }

  SearchBoxView* GetSearchBoxView() {
    return GetAppListTestHelper()->GetBubbleSearchBoxView();
  }

  AppListBubbleAppsPage* GetAppsPage() {
    return GetAppListTestHelper()->GetBubbleAppsPage();
  }

  RecentAppsView* GetRecentAppsView() {
    return GetAppListTestHelper()->GetBubbleRecentAppsView();
  }

  ScrollableAppsGridView* GetAppsGridView() {
    return GetAppListTestHelper()->GetScrollableAppsGridView();
  }

  AppListBubbleSearchPage* GetSearchPage() {
    return GetAppListTestHelper()->GetBubbleSearchPage();
  }

  AppListBubbleAssistantPage* GetAssistantPage() {
    return GetAppListTestHelper()->GetBubbleAssistantPage();
  }

  views::View* GetFocusedView() {
    return GetAppListTestHelper()
        ->GetBubbleView()
        ->GetFocusManager()
        ->GetFocusedView();
  }

  const char* GetFocusedViewName() {
    auto* view = GetFocusedView();
    return view ? view->GetClassName() : "none";
  }

  base::test::ScopedFeatureList scoped_features_;
  test::AppListTestModel* app_list_test_model_ = nullptr;
};

TEST_F(AppListBubbleViewTest, LayerConfiguration) {
  ShowAppList();

  // Verify that nothing has changed the layer configuration.
  ui::Layer* layer = GetBubblePresenter()->bubble_view_for_test()->layer();
  ASSERT_TRUE(layer);
  EXPECT_FALSE(layer->fills_bounds_opaquely());
  EXPECT_TRUE(layer->is_fast_rounded_corner());
  EXPECT_EQ(layer->background_blur(), ColorProvider::kBackgroundBlurSigma);
  EXPECT_EQ(layer->background_color(),
            AshColorProvider::Get()->GetBaseLayerColor(
                AshColorProvider::BaseLayerType::kTransparent80));
}

// Tests some basic layout coordinates, because we don't have screenshot tests.
// See go/cros-launcher-spec for layout.
TEST_F(AppListBubbleViewTest, Layout) {
  ShowAppList();

  // Check the bounds of the search box search icon.
  auto* search_box_view = GetSearchBoxView();
  auto* search_icon = search_box_view->get_search_icon_for_test();
  gfx::Rect search_icon_bounds =
      search_icon->ConvertRectToWidget(search_icon->GetLocalBounds());
  EXPECT_EQ("16,16 24x24", search_icon_bounds.ToString());

  // Check height of search box view.
  EXPECT_EQ(56, search_box_view->height());

  // The separator is immediately under the search box.
  gfx::Point separator_origin;
  views::View::ConvertPointToWidget(GetSeparator(), &separator_origin);
  EXPECT_EQ(0, separator_origin.x());
  EXPECT_EQ(search_box_view->height(), separator_origin.y());
}

TEST_F(AppListBubbleViewTest, OpeningBubbleFocusesSearchBox) {
  ShowAppList();

  SearchBoxView* search_box_view = GetSearchBoxView();
  EXPECT_TRUE(search_box_view->search_box()->HasFocus());
  EXPECT_TRUE(search_box_view->is_search_box_active());
}

TEST_F(AppListBubbleViewTest, SearchBoxTextUsesPrimaryTextColor) {
  ShowAppList();

  views::Textfield* search_box = GetSearchBoxView()->search_box();
  EXPECT_EQ(search_box->GetTextColor(),
            AshColorProvider::Get()->GetContentLayerColor(
                AshColorProvider::ContentLayerType::kTextColorPrimary));
}

TEST_F(AppListBubbleViewTest, SearchBoxShowsAssistantButton) {
  SimulateAssistantEnabled();
  ShowAppList();

  // By default the assistant button is visible.
  SearchBoxView* view = GetSearchBoxView();
  EXPECT_TRUE(view->assistant_button()->GetVisible());
  EXPECT_FALSE(view->close_button()->GetVisible());

  // Typing text shows the close button instead.
  PressAndReleaseKey(ui::VKEY_A);
  EXPECT_FALSE(view->assistant_button()->GetVisible());
  EXPECT_TRUE(view->close_button()->GetVisible());
}

TEST_F(AppListBubbleViewTest, ClickingAssistantButtonShowsAssistantPage) {
  SimulateAssistantEnabled();
  ShowAppList();

  SearchBoxView* search_box = GetSearchBoxView();
  LeftClickOn(search_box->assistant_button());

  EXPECT_FALSE(search_box->GetVisible());
  EXPECT_FALSE(GetSeparator()->GetVisible());
  EXPECT_FALSE(GetAppsPage()->GetVisible());
  EXPECT_FALSE(GetSearchPage()->GetVisible());
  EXPECT_TRUE(GetAssistantPage()->GetVisible());
}

TEST_F(AppListBubbleViewTest, AssistantPageDoesNotHaveBackground) {
  SimulateAssistantEnabled();
  ShowAppList();
  LeftClickOn(GetSearchBoxView()->assistant_button());

  // Assistant not have a background so the blurred launcher is visible
  // underneath the AppListBubbleAssistantPage view.
  EXPECT_FALSE(GetAssistantPage()->GetBackground());
}

TEST_F(AppListBubbleViewTest, SearchBoxCloseButton) {
  ShowAppList();
  PressAndReleaseKey(ui::VKEY_A);

  // Close button is visible after typing text.
  SearchBoxView* search_box_view = GetSearchBoxView();
  EXPECT_TRUE(search_box_view->close_button()->GetVisible());
  EXPECT_FALSE(search_box_view->search_box()->GetText().empty());

  // Clicking the close button clears the search, but the search box is still
  // focused/active.
  LeftClickOn(search_box_view->close_button());
  EXPECT_FALSE(search_box_view->close_button()->GetVisible());
  EXPECT_TRUE(search_box_view->search_box()->GetText().empty());
  EXPECT_TRUE(search_box_view->search_box()->HasFocus());
  EXPECT_TRUE(search_box_view->is_search_box_active());
}

TEST_F(AppListBubbleViewTest, AppsPageShownByDefault) {
  ShowAppList();

  EXPECT_TRUE(GetAppsPage()->GetVisible());
  EXPECT_FALSE(GetSearchPage()->GetVisible());
  EXPECT_FALSE(GetAssistantPage()->GetVisible());
}

TEST_F(AppListBubbleViewTest, TypingTextShowsSearchPage) {
  ShowAppList();

  AppListBubbleAppsPage* apps_page = GetAppsPage();
  AppListBubbleSearchPage* search_page = GetSearchPage();

  // Type some text.
  PressAndReleaseKey(ui::VKEY_A);

  // Search page is shown.
  EXPECT_FALSE(apps_page->GetVisible());
  EXPECT_TRUE(search_page->GetVisible());

  // Backspace to remove the text.
  PressAndReleaseKey(ui::VKEY_BACK);

  // Apps page is shown.
  EXPECT_TRUE(apps_page->GetVisible());
  EXPECT_FALSE(search_page->GetVisible());
}

TEST_F(AppListBubbleViewTest, TypingTextStartsSearch) {
  TestAppListClient* client = GetAppListTestHelper()->app_list_client();

  ShowAppList();

  PressAndReleaseKey(ui::VKEY_A);
  EXPECT_EQ(client->last_search_query(), u"a");

  PressAndReleaseKey(ui::VKEY_B);
  EXPECT_EQ(client->last_search_query(), u"ab");
}

TEST_F(AppListBubbleViewTest, BackActionsClearSearch) {
  ShowAppList();
  SearchBoxView* search_box_view = GetSearchBoxView();

  PressAndReleaseKey(ui::VKEY_A);
  EXPECT_FALSE(search_box_view->search_box()->GetText().empty());

  PressAndReleaseKey(ui::VKEY_BROWSER_BACK);
  EXPECT_TRUE(search_box_view->search_box()->GetText().empty());
  EXPECT_TRUE(search_box_view->is_search_box_active());

  PressAndReleaseKey(ui::VKEY_A);
  EXPECT_FALSE(search_box_view->search_box()->GetText().empty());

  PressAndReleaseKey(ui::VKEY_ESCAPE);
  EXPECT_TRUE(search_box_view->search_box()->GetText().empty());
  EXPECT_TRUE(search_box_view->is_search_box_active());
}

TEST_F(AppListBubbleViewTest, BackActionsCloseAppList) {
  ShowAppList();
  GetAppListTestHelper()->CheckVisibility(true);

  PressAndReleaseKey(ui::VKEY_BROWSER_BACK);
  GetAppListTestHelper()->CheckVisibility(false);

  ShowAppList();
  GetAppListTestHelper()->CheckVisibility(true);

  PressAndReleaseKey(ui::VKEY_ESCAPE);
  GetAppListTestHelper()->CheckVisibility(false);
}

TEST_F(AppListBubbleViewTest, BackActionsCloseFolder) {
  AddFolderWithApps(3);
  ShowAppList();

  AppListItemView* folder_item =
      GetAppListTestHelper()->GetScrollableAppsGridView()->GetItemViewAt(0);

  LeftClickOn(folder_item);
  EXPECT_TRUE(GetAppListTestHelper()->IsInFolderView());
  EXPECT_TRUE(GetAppListTestHelper()->GetBubbleFolderView()->GetVisible());

  // Folder closed.
  PressAndReleaseKey(ui::VKEY_BROWSER_BACK);
  EXPECT_TRUE(GetBubblePresenter()->IsShowing());
  EXPECT_FALSE(GetAppListTestHelper()->IsInFolderView());
  EXPECT_FALSE(GetAppListTestHelper()->GetBubbleFolderView()->GetVisible());

  LeftClickOn(folder_item);
  EXPECT_TRUE(GetAppListTestHelper()->IsInFolderView());
  EXPECT_TRUE(GetAppListTestHelper()->GetBubbleFolderView()->GetVisible());

  // Folder closed.
  PressAndReleaseKey(ui::VKEY_ESCAPE);
  EXPECT_TRUE(GetBubblePresenter()->IsShowing());
  EXPECT_FALSE(GetAppListTestHelper()->IsInFolderView());
  EXPECT_FALSE(GetAppListTestHelper()->GetBubbleFolderView()->GetVisible());
}

TEST_F(AppListBubbleViewTest, BackActionWithSelectedItemSelectsFolder) {
  AddFolderWithApps(3);
  ShowAppList();

  AppListItemView* folder_item =
      GetAppListTestHelper()->GetScrollableAppsGridView()->GetItemViewAt(0);

  LeftClickOn(folder_item);
  EXPECT_TRUE(GetAppListTestHelper()->IsInFolderView());
  EXPECT_TRUE(GetAppListTestHelper()->GetBubbleFolderView()->GetVisible());

  // Focus on first item in folder
  PressAndReleaseKey(ui::VKEY_TAB);

  // Folder closed.
  PressAndReleaseKey(ui::VKEY_BROWSER_BACK);

  ScrollableAppsGridView* grid_view =
      GetAppListTestHelper()->GetScrollableAppsGridView();
  EXPECT_TRUE(grid_view->has_selected_view());
  EXPECT_TRUE(grid_view->selected_view() == folder_item);
}

TEST_F(AppListBubbleViewTest, CanSelectSearchResults) {
  ShowAppList();

  // Can't select results, search page isn't visible.
  AppListBubbleView* view = GetBubblePresenter()->bubble_view_for_test();
  EXPECT_FALSE(view->CanSelectSearchResults());

  // Typing a key switches to the search page, but we still don't have results.
  PressAndReleaseKey(ui::VKEY_A);
  EXPECT_FALSE(view->CanSelectSearchResults());

  // Search results becoming available allows keyboard selection.
  AddSearchResult("id", u"title");
  base::RunLoop().RunUntilIdle();  // Update search model observers.
  EXPECT_TRUE(view->CanSelectSearchResults());
}

TEST_F(AppListBubbleViewTest, DownArrowMovesFocusToApps) {
  // Add an app, but no "Continue" suggestions.
  AddAppItems(1);
  ShowAppList();

  auto* apps_grid_view = GetAppListTestHelper()->GetScrollableAppsGridView();
  AppListItemView* app_item = apps_grid_view->GetItemViewAt(0);
  SearchBoxView* search_box_view = GetSearchBoxView();
  EXPECT_TRUE(search_box_view->search_box()->HasFocus());

  // Pressing down arrow moves focus into apps.
  PressAndReleaseKey(ui::VKEY_DOWN);
  EXPECT_FALSE(search_box_view->search_box()->HasFocus());
  EXPECT_TRUE(apps_grid_view->IsSelectedView(app_item));
  EXPECT_TRUE(app_item->HasFocus());

  // Pressing up arrow moves focus back to search box.
  PressAndReleaseKey(ui::VKEY_UP);
  EXPECT_TRUE(search_box_view->search_box()->HasFocus());
  EXPECT_FALSE(apps_grid_view->has_selected_view());
  EXPECT_FALSE(app_item->HasFocus());
}

TEST_F(AppListBubbleViewTest, DownArrowSelectsRecentsThenApps) {
  // Create enough apps to require scrolling.
  AddAppItems(50);
  // Create enough recent apps that the recents section will show.
  const int kNumRecentApps = 5;
  AddRecentApps(kNumRecentApps);
  ShowAppList();

  // Pressing down arrow once moves focus into recent apps.
  auto* focus_manager = GetAppsPage()->GetFocusManager();
  PressAndReleaseKey(ui::VKEY_DOWN);
  EXPECT_TRUE(GetRecentAppsView()->Contains(focus_manager->GetFocusedView()));

  // Pressing down arrow again moves focus into the apps grid.
  PressAndReleaseKey(ui::VKEY_DOWN);
  auto* apps_grid = GetAppListTestHelper()->GetScrollableAppsGridView();
  EXPECT_TRUE(apps_grid->Contains(focus_manager->GetFocusedView()));
}

TEST_F(AppListBubbleViewTest, DownArrowFromRecentsSelectsSameColumnInAppsGrid) {
  AddRecentApps(5);
  AddAppItems(5);
  ShowAppList();

  for (int column = 0; column < 5; column++) {
    // Pressing down arrow from an item in recent apps selects the app in the
    // same column in the apps grid.
    AppListItemView* recent_app = GetRecentAppsView()->GetItemViewAt(column);
    recent_app->RequestFocus();
    ASSERT_TRUE(recent_app->HasFocus());

    PressAndReleaseKey(ui::VKEY_DOWN);

    AppListItemView* app = GetAppsGridView()->GetItemViewAt(column);
    EXPECT_TRUE(app->HasFocus()) << "Focus mismatch for column " << column;
  }
}

TEST_F(AppListBubbleViewTest, DownArrowFromRecentsSelectsLastColumnInAppsGrid) {
  AddRecentApps(5);
  AddFolderWithApps(2);
  AddFolderWithApps(3);
  ShowAppList();

  // There are only 2 folders, and hence 2 columns, in the top level apps grid.
  auto* apps_grid_view = GetAppsGridView();
  ASSERT_EQ(2, apps_grid_view->view_model()->view_size());

  // Focus the 5th recent app.
  auto* recent_apps_view = GetRecentAppsView();
  ASSERT_EQ(5, recent_apps_view->GetItemViewCount());
  recent_apps_view->GetItemViewAt(4)->RequestFocus();

  PressAndReleaseKey(ui::VKEY_DOWN);

  // There's no 5th column in the apps grid, so the 2nd item is selected.
  AppListItemView* item = GetAppsGridView()->GetItemViewAt(1);
  EXPECT_TRUE(item->HasFocus());
}

TEST_F(AppListBubbleViewTest, UpArrowFromRecentsSelectsContinueTasks) {
  AddContinueSuggestionResult(4);
  AddRecentApps(5);
  AddAppItems(5);
  ShowAppList();

  ContinueTaskView* last_continue_task =
      GetAppListTestHelper()->GetContinueSectionView()->GetTaskViewAtForTesting(
          3);
  auto* recent_apps_view = GetRecentAppsView();

  // Pressing 'up' from any column in recent apps moves to the last continue
  // task.
  for (int column = 0; column < 5; ++column) {
    recent_apps_view->GetItemViewAt(column)->RequestFocus();

    PressAndReleaseKey(ui::VKEY_UP);

    EXPECT_TRUE(views::IsViewClass<ContinueTaskView>(GetFocusedView()))
        << GetFocusedViewName();
    EXPECT_TRUE(last_continue_task->HasFocus());
  }
}

TEST_F(AppListBubbleViewTest, UpArrowFromAppsGridSelectsSameColumnInRecents) {
  AddRecentApps(5);
  AddAppItems(5);
  ShowAppList();

  for (int column = 0; column < 5; column++) {
    // Pressing up arrow from an item in the apps grid selects the app in the
    // same column in the recents list.
    AppListItemView* app = GetAppsGridView()->GetItemViewAt(column);
    app->RequestFocus();
    ASSERT_TRUE(app->HasFocus());

    PressAndReleaseKey(ui::VKEY_UP);

    EXPECT_TRUE(GetRecentAppsView()->GetItemViewAt(column)->HasFocus())
        << "Focus mismatch for column " << column;
  }
}

TEST_F(AppListBubbleViewTest, UpArrowFromAppsGridSelectsLastColumnInRecents) {
  // Add 4 columns of recents, but 5 columns of apps.
  AddRecentApps(4);
  AddAppItems(5);
  ShowAppList();

  // Select the app in the last column of the apps grid.
  GetAppsGridView()->GetItemViewAt(4)->RequestFocus();

  PressAndReleaseKey(ui::VKEY_UP);

  // The last app in recents is selected.
  EXPECT_TRUE(GetRecentAppsView()->GetItemViewAt(3)->HasFocus());
}

TEST_F(AppListBubbleViewTest,
       UpArrowFromAppsGridWithNoRecentsSelectsContinueTasks) {
  AddContinueSuggestionResult(4);
  // Don't add recents.
  AddAppItems(5);
  ShowAppList();
  GetAppsGridView()->GetItemViewAt(0)->RequestFocus();

  PressAndReleaseKey(ui::VKEY_UP);

  auto* continue_section = GetAppListTestHelper()->GetContinueSectionView();
  auto* focus_manager = GetAppsPage()->GetFocusManager();
  EXPECT_TRUE(continue_section->Contains(focus_manager->GetFocusedView()))
      << GetFocusedViewName();
}

TEST_F(AppListBubbleViewTest, DownArrowMovesFocusToContinueTasks) {
  // Add an app, and some "Continue" suggestions.
  AddAppItems(1);
  // Create enough recent apps that the recents section will show.
  AddContinueSuggestionResult(4);
  ShowAppList();

  auto* apps_grid_view = GetAppListTestHelper()->GetScrollableAppsGridView();

  SearchBoxView* search_box_view = GetSearchBoxView();
  EXPECT_TRUE(search_box_view->search_box()->HasFocus());

  // Pressing down arrow moves focus through the continue tasks. It does not
  // trigger ScrollView scrolling.
  auto* continue_section = GetAppListTestHelper()->GetContinueSectionView();
  auto* focus_manager = GetAppsPage()->GetFocusManager();
  for (int i = 0; i < 4; i++) {
    PressAndReleaseKey(ui::VKEY_DOWN);
    EXPECT_TRUE(continue_section->Contains(focus_manager->GetFocusedView()));
  }

  // Pressing down arrow again moves focus into the apps grid.
  PressAndReleaseKey(ui::VKEY_DOWN);
  EXPECT_TRUE(apps_grid_view->Contains(focus_manager->GetFocusedView()));
}

TEST_F(AppListBubbleViewTest, ClickOnFolderOpensFolder) {
  AddFolderWithApps(3);
  ShowAppList();

  AppListItemView* folder_item = GetAppsGridView()->GetItemViewAt(0);
  LeftClickOn(folder_item);

  // Folder opened.
  EXPECT_TRUE(GetAppListTestHelper()->IsInFolderView());
  EXPECT_TRUE(GetAppListTestHelper()->GetBubbleFolderView()->GetVisible());
}

TEST_F(AppListBubbleViewTest, LargeFolderViewFitsInsideMainBubble) {
  // Create more apps than fit in the default sized folder.
  AddFolderWithApps(30);
  ShowAppList();

  AppListItemView* folder_item =
      GetAppListTestHelper()->GetScrollableAppsGridView()->GetItemViewAt(0);
  LeftClickOn(folder_item);

  // The folder fits inside the bubble.
  gfx::Rect folder_bounds =
      GetAppListTestHelper()->GetBubbleFolderView()->GetBoundsInScreen();
  gfx::Rect bubble_bounds =
      GetBubblePresenter()->bubble_view_for_test()->GetBoundsInScreen();
  EXPECT_TRUE(bubble_bounds.Contains(folder_bounds));

  // The top and bottom of the folder are inset from the bubble top and bottom.
  constexpr int kExpectedInset = 16;
  EXPECT_EQ(folder_bounds.y(), bubble_bounds.y() + kExpectedInset);
  EXPECT_EQ(folder_bounds.bottom(), bubble_bounds.bottom() - kExpectedInset);
}

TEST_F(AppListBubbleViewTest, ClickOutsideFolderClosesFolder) {
  AddFolderWithApps(3);
  ShowAppList();

  AppListItemView* folder_item =
      GetAppListTestHelper()->GetScrollableAppsGridView()->GetItemViewAt(0);
  LeftClickOn(folder_item);

  auto* folder_view = GetAppListTestHelper()->GetBubbleFolderView();
  gfx::Point outside_view =
      folder_view->GetBoundsInScreen().bottom_right() + gfx::Vector2d(10, 10);
  GetEventGenerator()->MoveMouseTo(outside_view);
  GetEventGenerator()->ClickLeftButton();

  // Folder closed.
  EXPECT_FALSE(GetAppListTestHelper()->IsInFolderView());
  EXPECT_FALSE(GetAppListTestHelper()->GetBubbleFolderView()->GetVisible());
}

TEST_F(AppListBubbleViewTest, ReparentDragOutOfFolderClosesFolder) {
  AddFolderWithApps(3);
  ShowAppList();

  AppListItemView* folder_item =
      GetAppListTestHelper()->GetScrollableAppsGridView()->GetItemViewAt(0);
  LeftClickOn(folder_item);

  // Drag the first app from the folder's app grid.
  auto* folder_view = GetAppListTestHelper()->GetBubbleFolderView();
  AppListItemView* app_item = folder_view->items_grid_view()->GetItemViewAt(0);
  auto* generator = GetEventGenerator();
  generator->MoveMouseTo(app_item->GetBoundsInScreen().CenterPoint());
  generator->PressLeftButton();
  app_item->FireMouseDragTimerForTest();

  gfx::Point outside_view =
      folder_view->GetBoundsInScreen().bottom_right() + gfx::Vector2d(10, 10);
  generator->MoveMouseTo(outside_view);
  folder_view->items_grid_view()->FireFolderItemReparentTimerForTest();

  // Folder visually closed.
  EXPECT_FALSE(GetAppListTestHelper()->IsInFolderView());

  // Folder is still "visible" because the drag has not ended.
  EXPECT_TRUE(GetAppListTestHelper()->GetBubbleFolderView()->GetVisible());

  // End the drag.
  generator->ReleaseLeftButton();
  EXPECT_FALSE(GetAppListTestHelper()->GetBubbleFolderView()->GetVisible());
}

TEST_F(AppListBubbleViewTest, DragItemInsideFolderDoesNotSelectItem) {
  AddFolderWithApps(3);
  ShowAppList();

  AppListItemView* folder_item =
      GetAppListTestHelper()->GetScrollableAppsGridView()->GetItemViewAt(0);
  LeftClickOn(folder_item);

  // Drag the first app inside the folder's app grid.
  auto* folder_view = GetAppListTestHelper()->GetBubbleFolderView();
  AppListItemView* first_app = folder_view->items_grid_view()->GetItemViewAt(0);
  auto* generator = GetEventGenerator();
  generator->MoveMouseTo(first_app->GetBoundsInScreen().CenterPoint());
  generator->PressLeftButton();
  first_app->FireMouseDragTimerForTest();
  generator->MoveMouseBy(100, 100);
  generator->ReleaseLeftButton();

  // Nothing is selected or focused.
  EXPECT_FALSE(folder_view->items_grid_view()->has_selected_view());
  EXPECT_FALSE(GetFocusedView()) << GetFocusedViewName();
}

TEST_F(AppListBubbleViewTest, OpenFolderWithMouseDoesNotFocusItem) {
  AddFolderWithApps(3);
  ShowAppList();

  AppListItemView* folder_item = GetAppsGridView()->GetItemViewAt(0);
  LeftClickOn(folder_item);

  AppsGridView* items_grid_view =
      GetAppListTestHelper()->GetBubbleFolderView()->items_grid_view();
  EXPECT_FALSE(items_grid_view->has_selected_view());
  EXPECT_FALSE(GetFocusedView()) << GetFocusedViewName();
}

TEST_F(AppListBubbleViewTest, PressingTabMovesFocusInsideFolder) {
  AddFolderWithApps(3);
  ShowAppList();

  AppListItemView* folder_item = GetAppsGridView()->GetItemViewAt(0);
  LeftClickOn(folder_item);

  PressAndReleaseKey(ui::VKEY_TAB);

  // First item is selected.
  auto* folder_view = GetAppListTestHelper()->GetBubbleFolderView();
  AppsGridView* items_grid_view = folder_view->items_grid_view();
  EXPECT_TRUE(items_grid_view->has_selected_view());
  EXPECT_EQ(items_grid_view->GetItemViewAt(0), GetFocusedView())
      << GetFocusedViewName();

  // Repeatedly pressing tab keeps focus inside the folder view.
  for (int i = 0; i < 10; i++) {
    PressAndReleaseKey(ui::VKEY_TAB);
    EXPECT_TRUE(folder_view->Contains(GetFocusedView()))
        << GetFocusedViewName();
  }
}

TEST_F(AppListBubbleViewTest, OpeningFolderRemovesOtherViewsFromAccessibility) {
  AddContinueSuggestionResult(4);
  AddRecentApps(5);
  AddFolderWithApps(5);
  ShowAppList();

  // Open the folder.
  AppListItemView* folder_item = GetAppsGridView()->GetItemViewAt(0);
  LeftClickOn(folder_item);

  auto* search_box = GetSearchBoxView();
  EXPECT_TRUE(search_box->GetViewAccessibility().IsIgnored());
  EXPECT_TRUE(search_box->GetViewAccessibility().IsLeaf());
  auto* continue_section = GetAppListTestHelper()->GetContinueSectionView();
  EXPECT_TRUE(continue_section->GetViewAccessibility().IsIgnored());
  EXPECT_TRUE(continue_section->GetViewAccessibility().IsLeaf());
  auto* recent_apps = GetRecentAppsView();
  EXPECT_TRUE(recent_apps->GetViewAccessibility().IsIgnored());
  EXPECT_TRUE(recent_apps->GetViewAccessibility().IsLeaf());
  auto* apps_grid = GetAppsGridView();
  EXPECT_TRUE(apps_grid->GetViewAccessibility().IsIgnored());
  EXPECT_TRUE(apps_grid->GetViewAccessibility().IsLeaf());

  // Close the folder.
  PressAndReleaseKey(ui::VKEY_ESCAPE);

  EXPECT_FALSE(search_box->GetViewAccessibility().IsIgnored());
  EXPECT_FALSE(search_box->GetViewAccessibility().IsLeaf());
  EXPECT_FALSE(continue_section->GetViewAccessibility().IsIgnored());
  EXPECT_FALSE(continue_section->GetViewAccessibility().IsLeaf());
  EXPECT_FALSE(recent_apps->GetViewAccessibility().IsIgnored());
  EXPECT_FALSE(recent_apps->GetViewAccessibility().IsLeaf());
  EXPECT_FALSE(apps_grid->GetViewAccessibility().IsIgnored());
  EXPECT_FALSE(apps_grid->GetViewAccessibility().IsLeaf());
}

TEST_F(AppListBubbleViewTest, OpenFolderWithKeyboardFocusesFirstItem) {
  AddFolderWithApps(3);
  ShowAppList();

  AppListItemView* folder_item = GetAppsGridView()->GetItemViewAt(0);
  folder_item->RequestFocus();
  PressAndReleaseKey(ui::VKEY_RETURN);

  // First item is selected and focused.
  AppsGridView* items_grid_view =
      GetAppListTestHelper()->GetBubbleFolderView()->items_grid_view();
  AppListItemView* first_item = items_grid_view->GetItemViewAt(0);
  EXPECT_TRUE(items_grid_view->has_selected_view());
  EXPECT_TRUE(items_grid_view->IsSelectedView(first_item));
  EXPECT_TRUE(first_item->HasFocus()) << GetFocusedViewName();
}

TEST_F(AppListBubbleViewTest, CloseFolderWithNoSelectedItemFocusesSearchBox) {
  AddFolderWithApps(3);
  ShowAppList();

  AppListItemView* folder_item = GetAppsGridView()->GetItemViewAt(0);
  LeftClickOn(folder_item);

  auto* folder_view = GetAppListTestHelper()->GetBubbleFolderView();
  ASSERT_FALSE(folder_view->items_grid_view()->has_selected_view());

  // Close the folder.
  PressAndReleaseKey(ui::VKEY_ESCAPE);

  SearchBoxView* search_box_view = GetSearchBoxView();
  EXPECT_TRUE(search_box_view->search_box()->HasFocus())
      << GetFocusedViewName();
  EXPECT_TRUE(search_box_view->is_search_box_active());
}

TEST_F(AppListBubbleViewTest, CloseFolderWithSelectedItemFocusesFolderItem) {
  AddFolderWithApps(3);
  ShowAppList();

  AppListItemView* folder_item = GetAppsGridView()->GetItemViewAt(0);
  LeftClickOn(folder_item);

  auto* folder_view = GetAppListTestHelper()->GetBubbleFolderView();
  folder_view->items_grid_view()->GetItemViewAt(0)->RequestFocus();
  ASSERT_TRUE(folder_view->items_grid_view()->has_selected_view());

  // Close the folder.
  PressAndReleaseKey(ui::VKEY_ESCAPE);

  // Folder item is selected and focused.
  auto* root_apps_grid_view = GetAppsGridView();
  EXPECT_TRUE(root_apps_grid_view->has_selected_view());
  EXPECT_TRUE(root_apps_grid_view->IsSelectedView(folder_item));
  EXPECT_TRUE(folder_item->HasFocus()) << GetFocusedViewName();
}

}  // namespace
}  // namespace ash
