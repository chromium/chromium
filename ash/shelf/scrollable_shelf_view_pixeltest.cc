// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/scrollable_shelf_view.h"
#include "ash/shelf/shelf_menu_model_adapter.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shelf/test/shelf_test_base.h"
#include "ash/test/pixel/ash_pixel_differ.h"
#include "ash/test/pixel/ash_pixel_test_init_params.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/submenu_view.h"

namespace ash {

class ScrollableShelfViewPixelRTLTestBase : public ShelfTestBase {
 public:
  // ScrollableShelfTestBase:
  void SetUp() override {
    ShelfTestBase::SetUp();
    AddAppShortcutsUntilOverflow(/*use_alternative_color=*/true);
  }
};

class ScrollableShelfViewPixelRTLTest
    : public ScrollableShelfViewPixelRTLTestBase,
      public testing::WithParamInterface<bool /*is_rtl=*/> {
 public:
  // ScrollableShelfViewPixelRTLTestBase:
  std::optional<pixel_test::InitParams> CreatePixelTestInitParams()
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
      "overflow",
      /*revision_number=*/8, GetPrimaryShelf()->GetWindow()));

  ASSERT_TRUE(scrollable_shelf_view_->right_arrow());
  const gfx::Point right_arrow_center =
      scrollable_shelf_view_->right_arrow()->GetBoundsInScreen().CenterPoint();

  GetEventGenerator()->MoveMouseTo(right_arrow_center);
  GetEventGenerator()->ClickLeftButton();

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "overflow_end",
      /*revision_number=*/8, GetPrimaryShelf()->GetWindow()));
}

TEST_P(ScrollableShelfViewPixelRTLTest, LeftRightShelfAlignment) {
  GetPrimaryShelf()->SetAlignment(ShelfAlignment::kLeft);
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "left_shelf_alignment",
      /*revision_number=*/5, GetPrimaryShelf()->GetWindow()));

  GetPrimaryShelf()->SetAlignment(ShelfAlignment::kRight);
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "right_shelf_alignment",
      /*revision_number=*/5, GetPrimaryShelf()->GetWindow()));
}

class ScrollableShelfViewWithGuestModePixelTest
    : public ShelfTestBase,
      public testing::WithParamInterface<bool /*use_guest_mode=*/> {
 public:
  // ScrollableShelfTestBase:
  std::optional<pixel_test::InitParams> CreatePixelTestInitParams()
      const override {
    return pixel_test::InitParams();
  }

  void SetUp() override {
    set_start_session(false);

    ShelfTestBase::SetUp();
    if (GetParam()) {
      SimulateGuestLogin();
    } else {
      SimulateUserLogin("user@gmail.com");
    }
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
      "shelf_context_menu",
      /*revision_number=*/23,
      GetPrimaryShelf()
          ->shelf_widget()
          ->shelf_view_for_testing()
          ->shelf_menu_model_adapter_for_testing()
          ->root_for_testing()
          ->GetSubmenu(),
      GetPrimaryShelf()->GetWindow()));
}

}  // namespace ash
