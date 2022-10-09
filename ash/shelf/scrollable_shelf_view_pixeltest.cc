// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/scrollable_shelf_view.h"
#include "ash/shelf/shelf_menu_model_adapter.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shelf/test/scrollable_shelf_test_base.h"
#include "ash/test/ash_pixel_diff_test_helper.h"
#include "ash/test/ash_pixel_test_init_params.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/submenu_view.h"

namespace ash {

class ScrollableShelfViewPixelRTLTestBase : public ScrollableShelfTestBase {
 public:
  // ScrollableShelfTestBase:
  void SetUp() override {
    ScrollableShelfTestBase::SetUp();
    AddAppShortcutsUntilOverflow(/*use_alternative_color=*/true);
  }
};

class ScrollableShelfViewPixelRTLTest
    : public ScrollableShelfViewPixelRTLTestBase,
      public testing::WithParamInterface<bool /*is_rtl=*/> {
 public:
  void SetUp() override {
    pixel_test::InitParams init_params;
    if (GetParam())
      init_params.under_rtl = true;
    PrepareForPixelDiffTest(/*screenshot_prefix=*/"scrollable_shelf_view_pixel",
                            init_params);

    ScrollableShelfViewPixelRTLTestBase::SetUp();
  }
};

INSTANTIATE_TEST_SUITE_P(RTL, ScrollableShelfViewPixelRTLTest, testing::Bool());

// Verifies the scrollable shelf under overflow.
TEST_P(ScrollableShelfViewPixelRTLTest, Basics) {
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      GetParam() ? "overflow_rtl" : "overflow",
      GetPrimaryShelf()->GetWindow()));
}

class ScrollableShelfViewWithGuestModePixelTest
    : public ScrollableShelfViewPixelRTLTestBase,
      public testing::WithParamInterface<bool /*use_guest_mode=*/> {
 public:
  // ScrollableShelfTestBase:
  void SetUp() override {
    set_start_session(false);
    PrepareForPixelDiffTest(
        /*screenshot_prefix=*/"scrollable_shelf_view_with_guest_mode_pixel",
        pixel_test::InitParams());

    ScrollableShelfTestBase::SetUp();
    if (GetParam())
      SimulateGuestLogin();
    else
      SimulateUserLogin("user@gmail.com");
    StabilizeUIForPixelTest();
  }
};

INSTANTIATE_TEST_SUITE_P(EnableGuestMode,
                         ScrollableShelfViewWithGuestModePixelTest,
                         testing::Bool());

// Verifies the shelf context menu.
TEST_P(ScrollableShelfViewWithGuestModePixelTest, VerifyShelfContextMenu) {
  // Move the mouse to the shelf center then right-click.
  const gfx::Point shelf_center =
      scrollable_shelf_view_->GetBoundsInScreen().CenterPoint();
  GetEventGenerator()->MoveMouseTo(shelf_center);
  GetEventGenerator()->PressRightButton();

  // Verify the shelf context menu and the shelf.
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      GetParam() ? "shelf_context_menu_in_guest_mode" : "shelf_context_menu",
      GetPrimaryShelf()
          ->shelf_widget()
          ->shelf_view_for_testing()
          ->shelf_menu_model_adapter_for_testing()
          ->root_for_testing()
          ->GetSubmenu(),
      GetPrimaryShelf()->GetWindow()));
}

}  // namespace ash
