// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/app_list_bubble_view.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/app_list/app_list_bubble_presenter.h"
#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/model/app_list_item.h"
#include "ash/app_list/model/search/test_search_result.h"
#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/app_list/test_app_list_client.h"
#include "ash/app_list/views/app_list_bubble_apps_page.h"
#include "ash/app_list/views/app_list_bubble_search_page.h"
#include "ash/app_list/views/assistant/app_list_bubble_assistant_page.h"
#include "ash/app_list/views/search_box_view.h"
#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "ash/style/ash_color_provider.h"
#include "ash/test/ash_test_base.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/icu_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/display.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/widget/widget.h"

using views::Widget;

namespace ash {
namespace {

// Distance under which two points are considered "near" each other.
constexpr int kNearDistanceDips = 20;

// The exact position of a bubble relative to its anchor is an implementation
// detail, so tests assert that points are "near" each other. This also makes
// the tests less fragile if padding changes.
testing::AssertionResult IsNear(const gfx::Point& a, const gfx::Point& b) {
  gfx::Vector2d delta = a - b;
  float distance = delta.Length();
  if (distance < float{kNearDistanceDips})
    return testing::AssertionSuccess();

  return testing::AssertionFailure()
         << a.ToString() << " is more than " << kNearDistanceDips
         << " dips away from " << b.ToString();
}

void AddAppItems(int num_apps) {
  auto* controller = Shell::Get()->app_list_controller();
  int num_apps_already_added =
      controller->GetModel()->top_level_item_list()->item_count();
  for (int i = 0; i < num_apps; i++) {
    controller->GetModel()->AddItem(std::make_unique<AppListItem>(
        /*app_id=*/base::NumberToString(i + num_apps_already_added)));
  }
}

void AddSearchResult(const std::string& id, const std::u16string& title) {
  auto search_result = std::make_unique<TestSearchResult>();
  search_result->set_result_id(id);
  search_result->set_display_type(SearchResultDisplayType::kList);
  search_result->set_title(title);
  Shell::Get()->app_list_controller()->GetSearchModel()->results()->Add(
      std::move(search_result));
}

AppListBubblePresenter* GetBubblePresenter() {
  return Shell::Get()->app_list_controller()->bubble_presenter_for_test();
}

gfx::Rect GetShelfBounds() {
  return AshTestBase::GetPrimaryShelf()
      ->shelf_widget()
      ->GetWindowBoundsInScreen();
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
    scoped_features_.InitAndEnableFeature(features::kAppListBubble);
  }
  ~AppListBubbleViewTest() override = default;

  // Shows the app list on the primary display.
  void ShowAppList() { GetBubblePresenter()->Show(GetPrimaryDisplay().id()); }

  void ClickButton(views::Button* button) {
    GetEventGenerator()->MoveMouseTo(button->GetBoundsInScreen().CenterPoint());
    GetEventGenerator()->ClickLeftButton();
  }

  SearchBoxView* GetSearchBoxView() {
    return GetAppListTestHelper()->GetBubbleSearchBoxView();
  }

  AppListBubbleAppsPage* GetAppsPage() {
    return GetAppListTestHelper()->GetBubbleAppsPage();
  }

  AppListBubbleSearchPage* GetSearchPage() {
    return GetAppListTestHelper()->GetBubbleSearchPage();
  }

  AppListBubbleAssistantPage* GetAssistantPage() {
    return GetAppListTestHelper()->GetBubbleAssistantPage();
  }

  base::test::ScopedFeatureList scoped_features_;
};

TEST_F(AppListBubbleViewTest, BubbleOpensInBottomLeftForBottomShelf) {
  GetPrimaryShelf()->SetAlignment(ShelfAlignment::kBottom);

  AppListBubblePresenter* presenter = GetBubblePresenter();
  presenter->Show(GetPrimaryDisplay().id());

  Widget* widget = presenter->bubble_widget_for_test();
  EXPECT_TRUE(IsNear(widget->GetWindowBoundsInScreen().bottom_left(),
                     GetPrimaryDisplay().work_area().bottom_left()));
}

TEST_F(AppListBubbleViewTest, BubbleOpensInTopLeftForLeftShelf) {
  GetPrimaryShelf()->SetAlignment(ShelfAlignment::kLeft);

  AppListBubblePresenter* presenter = GetBubblePresenter();
  presenter->Show(GetPrimaryDisplay().id());

  Widget* widget = presenter->bubble_widget_for_test();
  EXPECT_TRUE(IsNear(widget->GetWindowBoundsInScreen().origin(),
                     GetPrimaryDisplay().work_area().origin()));
}

TEST_F(AppListBubbleViewTest, BubbleOpensInTopRightForRightShelf) {
  GetPrimaryShelf()->SetAlignment(ShelfAlignment::kRight);

  AppListBubblePresenter* presenter = GetBubblePresenter();
  presenter->Show(GetPrimaryDisplay().id());

  Widget* widget = presenter->bubble_widget_for_test();
  EXPECT_TRUE(IsNear(widget->GetWindowBoundsInScreen().top_right(),
                     GetPrimaryDisplay().work_area().top_right()));
}

TEST_F(AppListBubbleViewTest, BubbleOpensInBottomRightForBottomShelfRTL) {
  base::test::ScopedRestoreICUDefaultLocale locale("he");
  GetPrimaryShelf()->SetAlignment(ShelfAlignment::kBottom);

  AppListBubblePresenter* presenter = GetBubblePresenter();
  presenter->Show(GetPrimaryDisplay().id());

  Widget* widget = presenter->bubble_widget_for_test();
  EXPECT_TRUE(IsNear(widget->GetWindowBoundsInScreen().bottom_right(),
                     GetPrimaryDisplay().work_area().bottom_right()));
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
  ClickButton(search_box->assistant_button());

  EXPECT_FALSE(search_box->GetVisible());
  EXPECT_FALSE(GetAppsPage()->GetVisible());
  EXPECT_FALSE(GetSearchPage()->GetVisible());
  EXPECT_TRUE(GetAssistantPage()->GetVisible());
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
  ClickButton(search_box_view->close_button());
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

  PressAndReleaseKey(ui::VKEY_A);
  EXPECT_FALSE(search_box_view->search_box()->GetText().empty());

  PressAndReleaseKey(ui::VKEY_ESCAPE);
  EXPECT_TRUE(search_box_view->search_box()->GetText().empty());
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

TEST_F(AppListBubbleViewTest, BubbleSizedForDisplay) {
  UpdateDisplay("800x800");
  AppListBubblePresenter* presenter = GetBubblePresenter();
  presenter->Show(GetPrimaryDisplay().id());

  views::View* client_view = presenter->bubble_view_for_test()->parent();

  // Check that the AppListBubble has the initial default bounds.
  EXPECT_EQ(640, client_view->bounds().width());
  EXPECT_EQ(688, client_view->bounds().height());

  // Check that the space between the top of the AppListBubble and the top of
  // the screen is greater than the shelf size.
  EXPECT_GE(client_view->GetBoundsInScreen().y(),
            ShelfConfig::Get()->shelf_size());

  // Change the display height to be smaller than 800.
  UpdateDisplay("800x600");
  presenter->Dismiss();
  presenter->Show(GetPrimaryDisplay().id());
  client_view = presenter->bubble_view_for_test()->parent();

  // With a smaller display, check that the space between the top of the
  // AppListBubble and the top of the screen is greater than the shelf size.
  EXPECT_GE(client_view->GetBoundsInScreen().y(),
            ShelfConfig::Get()->shelf_size());
}

// Test that the AppListBubbleView scales up with more apps on a larger display.
TEST_F(AppListBubbleViewTest, BubbleSizedForLargeDisplay) {
  UpdateDisplay("2000x2000");
  AppListBubblePresenter* presenter = GetBubblePresenter();
  presenter->Show(GetPrimaryDisplay().id());

  int no_apps_bubble_view_height = presenter->bubble_view_for_test()->height();

  // Add 30 apps to the AppListBubble and reopen.
  presenter->Dismiss();
  AddAppItems(30);
  presenter->Show(GetPrimaryDisplay().id());

  int thirty_apps_bubble_view_height =
      presenter->bubble_view_for_test()->height();

  // The AppListBubbleView should be larger after apps have neen added to it.
  EXPECT_GT(thirty_apps_bubble_view_height, no_apps_bubble_view_height);

  // Add 50 more apps to the AppListBubble and reopen.
  presenter->Dismiss();
  AddAppItems(50);
  presenter->Show(GetPrimaryDisplay().id());

  int eighty_apps_bubble_view_height =
      presenter->bubble_view_for_test()->height();

  // With more apps added, the height of the AppListBubble should increase.
  EXPECT_GT(eighty_apps_bubble_view_height, thirty_apps_bubble_view_height);

  // The AppListBubble height should not be larger than half the display height.
  EXPECT_LE(eighty_apps_bubble_view_height, 1000);

  // The AppListBubble should be contained within the display bounds.
  EXPECT_TRUE(GetPrimaryDisplay().work_area().Contains(
      presenter->bubble_view_for_test()->bounds()));
}

// Tests that the AppListBubbleView is positioned correctly when
// shown with bottom auto-hidden shelf.
TEST_F(AppListBubbleViewTest, BubblePositionWithBottomAutoHideShelf) {
  GetPrimaryShelf()->SetAlignment(ShelfAlignment::kBottom);
  GetPrimaryShelf()->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);

  AppListBubblePresenter* presenter = GetBubblePresenter();
  presenter->Show(GetPrimaryDisplay().id());

  gfx::Point bubble_view_bottom_left = presenter->bubble_widget_for_test()
                                           ->GetWindowBoundsInScreen()
                                           .bottom_left();

  // The bottom of the AppListBubbleView should be near the top of the shelf and
  // not near the bottom side of the display.
  EXPECT_FALSE(IsNear(bubble_view_bottom_left,
                      GetPrimaryDisplay().bounds().bottom_left()));
  EXPECT_TRUE(IsNear(bubble_view_bottom_left, GetShelfBounds().origin()));
}

// Tests that the AppListBubbleView is positioned correctly when shown with left
// auto-hidden shelf.
TEST_F(AppListBubbleViewTest, BubblePositionWithLeftAutoHideShelf) {
  GetPrimaryShelf()->SetAlignment(ShelfAlignment::kLeft);
  GetPrimaryShelf()->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);

  AppListBubblePresenter* presenter = GetBubblePresenter();
  presenter->Show(GetPrimaryDisplay().id());

  gfx::Point bubble_view_origin =
      presenter->bubble_widget_for_test()->GetWindowBoundsInScreen().origin();

  // The left of the AppListBubbleView should be near the right of the shelf and
  // not near the left side of the display.
  EXPECT_FALSE(
      IsNear(bubble_view_origin, GetPrimaryDisplay().bounds().origin()));
  EXPECT_TRUE(IsNear(bubble_view_origin, GetShelfBounds().top_right()));
}

// Tests that the AppListBubbleView is positioned correctly when shown with
// right auto-hidden shelf.
TEST_F(AppListBubbleViewTest, BubblePositionWithRightAutoHideShelf) {
  GetPrimaryShelf()->SetAlignment(ShelfAlignment::kRight);
  GetPrimaryShelf()->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);

  AppListBubblePresenter* presenter = GetBubblePresenter();
  presenter->Show(GetPrimaryDisplay().id());

  gfx::Point bubble_view_top_right = presenter->bubble_widget_for_test()
                                         ->GetWindowBoundsInScreen()
                                         .top_right();

  // The right of the AppListBubbleView should be near the left of the shelf and
  // not near the right side of the display.
  EXPECT_FALSE(
      IsNear(bubble_view_top_right, GetPrimaryDisplay().bounds().top_right()));
  EXPECT_TRUE(IsNear(bubble_view_top_right, GetShelfBounds().origin()));
}

}  // namespace
}  // namespace ash
