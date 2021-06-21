// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/bubble/app_list_bubble_view.h"

#include <memory>

#include "ash/app_list/app_list_bubble_presenter.h"
#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/bubble/app_list_bubble_apps_page.h"
#include "ash/app_list/bubble/app_list_bubble_search_page.h"
#include "ash/app_list/model/app_list_item.h"
#include "ash/app_list/views/search_box_view.h"
#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/icu_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/display.h"
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
  int num_apps_already_added = Shell::Get()
                                   ->app_list_controller()
                                   ->GetModel()
                                   ->top_level_item_list()
                                   ->item_count();
  for (int i = 0; i < num_apps; i++) {
    Shell::Get()->app_list_controller()->GetModel()->AddItem(
        std::make_unique<AppListItem>(
            /*app_id=*/base::NumberToString(i + num_apps_already_added)));
  }
}

AppListBubblePresenter* GetBubblePresenter() {
  return Shell::Get()->app_list_controller()->bubble_presenter_for_test();
}

SearchBoxView* GetSearchBoxView() {
  return GetBubblePresenter()
      ->bubble_view_for_test()
      ->search_box_view_for_test();
}

AppListBubbleAppsPage* GetAppsPage() {
  return GetBubblePresenter()->bubble_view_for_test()->apps_page_for_test();
}

AppListBubbleSearchPage* GetSearchPage() {
  return GetBubblePresenter()->bubble_view_for_test()->search_page_for_test();
}

class AppListBubbleViewTest : public AshTestBase {
 public:
  AppListBubbleViewTest() {
    scoped_features_.InitAndEnableFeature(features::kAppListBubble);
  }
  ~AppListBubbleViewTest() override = default;

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
  AppListBubblePresenter* presenter = GetBubblePresenter();
  presenter->Show(GetPrimaryDisplay().id());

  SearchBoxView* search_box_view = GetSearchBoxView();
  EXPECT_TRUE(search_box_view->search_box()->HasFocus());
  EXPECT_TRUE(search_box_view->is_search_box_active());
}

TEST_F(AppListBubbleViewTest, AppsPageShownByDefault) {
  AppListBubblePresenter* presenter = GetBubblePresenter();
  presenter->Show(GetPrimaryDisplay().id());

  EXPECT_TRUE(GetAppsPage()->GetVisible());
  EXPECT_FALSE(GetSearchPage()->GetVisible());
}

TEST_F(AppListBubbleViewTest, TypingTextShowsSearchPage) {
  AppListBubblePresenter* presenter = GetBubblePresenter();
  presenter->Show(GetPrimaryDisplay().id());

  AppListBubbleAppsPage* apps_page = GetAppsPage();
  AppListBubbleSearchPage* search_page = GetSearchPage();

  // Type some text.
  auto* generator = GetEventGenerator();
  generator->PressKey(ui::VKEY_A, ui::EF_NONE);
  generator->ReleaseKey(ui::VKEY_A, ui::EF_NONE);

  // Search page is shown.
  EXPECT_FALSE(apps_page->GetVisible());
  EXPECT_TRUE(search_page->GetVisible());

  // Backspace to remove the text.
  generator->PressKey(ui::VKEY_BACK, ui::EF_NONE);
  generator->ReleaseKey(ui::VKEY_BACK, ui::EF_NONE);

  // Apps page is shown.
  EXPECT_TRUE(apps_page->GetVisible());
  EXPECT_FALSE(search_page->GetVisible());
}

TEST_F(AppListBubbleViewTest, BubbleSizedForDisplay) {
  UpdateDisplay("800x800");
  AppListBubblePresenter* presenter = GetBubblePresenter();
  presenter->Show(GetPrimaryDisplay().id());

  views::View* client_view = presenter->bubble_view_for_test()->parent();

  // Check that the AppListBubble has the initial default bounds.
  EXPECT_EQ(544, client_view->bounds().width());
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

}  // namespace
}  // namespace ash
