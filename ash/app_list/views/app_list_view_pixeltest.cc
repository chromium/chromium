// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/app_list/views/app_list_bubble_apps_page.h"
#include "ash/app_list/views/search_box_view.h"
#include "ash/shell.h"
#include "ash/test/ash_pixel_diff_test_helper.h"
#include "ash/test/ash_pixel_test_init_params.h"
#include "ash/test/ash_test_base.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/textfield/textfield_test_api.h"

namespace ash {

class AppListViewPixelRTLTest
    : public AshTestBase,
      public testing::WithParamInterface<bool /*is_rtl=*/> {
 public:
  AppListViewPixelRTLTest() {
    PrepareForPixelDiffTest();
    if (GetParam()) {
      pixel_test::InitParams init_params;
      init_params.under_rtl = true;
      SetPixelTestInitParam(init_params);
    }
  }
  AppListViewPixelRTLTest(const AppListViewPixelRTLTest&) = delete;
  AppListViewPixelRTLTest& operator=(const AppListViewPixelRTLTest&) = delete;
  ~AppListViewPixelRTLTest() override = default;

  void ShowAppListAndHideCursor() {
    AppListTestHelper* test_helper = GetAppListTestHelper();
    test_helper->ShowAppList();

    // Use a fixed placeholder text instead of the one picked randomly to
    // avoid the test flakiness.
    test_helper->GetSearchBoxView()->UseFixedPlaceholderTextForTest();

    // Hide the search box cursor to avoid the flakiness due to the
    // blinking.
    views::TextfieldTestApi(test_helper->GetBubbleSearchBoxView()->search_box())
        .SetCursorLayerOpacity(0.f);
  }

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    pixel_test_helper_.InitSkiaGoldPixelDiff("app_list_view_pixel");
  }

  AshPixelDiffTestHelper pixel_test_helper_;
};

INSTANTIATE_TEST_SUITE_P(RTL, AppListViewPixelRTLTest, testing::Bool());

// Verifies the app list view under the clamshell mode.
TEST_P(AppListViewPixelRTLTest, Basics) {
  GetAppListTestHelper()->AddAppItemsWithColorAndName(
      /*num_apps=*/2, AppListTestHelper::IconColorType::kAlternativeColor,
      /*set_name=*/true);
  ShowAppListAndHideCursor();
  EXPECT_TRUE(pixel_test_helper_.ComparePrimaryFullScreen(
      GetParam() ? "bubble_launcher_basics_rtl" : "bubble_launcher_basics"));
}

// Verifies that the app list gradient zones work as expected.
TEST_P(AppListViewPixelRTLTest, GradientZone) {
  GetAppListTestHelper()->AddAppItemsWithColorAndName(
      /*num_apps=*/22, AppListTestHelper::IconColorType::kAlternativeColor,
      /*set_name=*/true);
  ShowAppListAndHideCursor();
  views::ScrollView* scroll_view =
      GetAppListTestHelper()->GetBubbleAppsPage()->scroll_view();

  // Scroll the bubble app list so that some app list icons are beneath the
  // gradient zones.
  scroll_view->ScrollToPosition(scroll_view->vertical_scroll_bar(),
                                /*position=*/20);

  EXPECT_TRUE(pixel_test_helper_.ComparePrimaryFullScreen(
      GetParam() ? "bubble_launcher_gradient_zone_rtl"
                 : "bubble_launcher_gradient_zone"));
}

}  // namespace ash
