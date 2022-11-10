// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/scrollable_shelf_view.h"
#include "ash/shelf/shelf_menu_model_adapter.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shelf/test/scrollable_shelf_test_base.h"
#include "ash/test/pixel/ash_pixel_differ.h"
#include "ash/test/pixel/ash_pixel_test_init_params.h"
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
  // ScrollableShelfViewPixelRTLTestBase:
  absl::optional<pixel_test::InitParams> CreatePixelTestInitParams()
      const override {
    pixel_test::InitParams init_params;
    init_params.under_rtl = GetParam();
    return init_params;
  }
};

INSTANTIATE_TEST_SUITE_P(RTL, ScrollableShelfViewPixelRTLTest, testing::Bool());

// Verifies the scrollable shelf under overflow.
TEST_P(ScrollableShelfViewPixelRTLTest, Basics) {
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "overflow.rev_0", GetPrimaryShelf()->GetWindow()));
}

class ScrollableShelfViewWithGuestModePixelTest
    : public ScrollableShelfTestBase,
      public testing::WithParamInterface<bool /*use_guest_mode=*/> {
 public:
  // ScrollableShelfTestBase:
  absl::optional<pixel_test::InitParams> CreatePixelTestInitParams()
      const override {
    return pixel_test::InitParams();
  }

  void SetUp() override {
    set_start_session(false);

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
      "shelf_context_menu.rev_0",
      GetPrimaryShelf()
          ->shelf_widget()
          ->shelf_view_for_testing()
          ->shelf_menu_model_adapter_for_testing()
          ->root_for_testing()
          ->GetSubmenu(),
      GetPrimaryShelf()->GetWindow()));
}

}  // namespace ash
