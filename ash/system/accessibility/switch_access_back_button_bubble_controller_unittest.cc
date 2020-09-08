// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/switch_access_back_button_bubble_controller.h"

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/accessibility/switch_access_back_button_bubble_controller.h"
#include "ash/system/accessibility/switch_access_back_button_view.h"
#include "ash/system/accessibility/switch_access_menu_bubble_controller.h"
#include "ash/test/ash_test_base.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

namespace ash {

class SwitchAccessBackButtonBubbleControllerTest : public AshTestBase {
 public:
  SwitchAccessBackButtonBubbleControllerTest() = default;
  ~SwitchAccessBackButtonBubbleControllerTest() override = default;

  SwitchAccessBackButtonBubbleControllerTest(
      const SwitchAccessBackButtonBubbleControllerTest&) = delete;
  SwitchAccessBackButtonBubbleControllerTest& operator=(
      const SwitchAccessBackButtonBubbleControllerTest&) = delete;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    Shell::Get()->accessibility_controller()->switch_access().SetEnabled(true);
  }

  SwitchAccessBackButtonBubbleController* GetBubbleController() {
    return Shell::Get()
        ->accessibility_controller()
        ->GetSwitchAccessBubbleControllerForTest()
        ->back_button_controller_.get();
  }

  void ShowBackButton(const gfx::Rect& anchor_rect) {
    GetBubbleController()->ShowBackButton(anchor_rect, /*show_focus_ring=*/true,
                                          /*for_menu=*/false);
  }

  gfx::Rect GetBackButtonBounds() {
    SwitchAccessBackButtonBubbleController* bubble_controller =
        GetBubbleController();
    if (bubble_controller && bubble_controller->back_button_view_) {
      return bubble_controller->back_button_view_->GetBoundsInScreen();
    }
    return gfx::Rect();
  }
};

TEST_F(SwitchAccessBackButtonBubbleControllerTest, AdjustAnchorRect) {
  gfx::Rect display_bounds = display::Screen::GetScreen()
                                 ->GetDisplayNearestPoint(gfx::Point(100, 100))
                                 .bounds();

  // When there's space for the button, the bottom left corner of the button
  // should be the upper right corner of the anchor rect.
  gfx::Rect anchor_rect(100, 100, 50, 50);
  ShowBackButton(anchor_rect);
  gfx::Rect button_bounds = GetBackButtonBounds();
  EXPECT_EQ(anchor_rect.top_right(), button_bounds.bottom_left());

  // When the anchor rect is aligned with the top edge of the screen, the back
  // button should also align with the top edge of the screen.
  anchor_rect = gfx::Rect(100, 0, 50, 50);
  ShowBackButton(anchor_rect);
  button_bounds = GetBackButtonBounds();
  EXPECT_EQ(anchor_rect.right(), button_bounds.x());
  EXPECT_EQ(display_bounds.y(), button_bounds.y());

  // When the anchor rect is aligned with the right edge of the screen, the back
  // button should also align with the right edge of the screen.
  anchor_rect = gfx::Rect(display_bounds.right() - 50, 100, 50, 50);
  ShowBackButton(anchor_rect);
  button_bounds = GetBackButtonBounds();
  EXPECT_EQ(display_bounds.right(), button_bounds.right());
  EXPECT_EQ(anchor_rect.y(), button_bounds.bottom());

  // When the anchor rect is shorter than the back button, the back button
  // should still display entirely onscreen.
  anchor_rect =
      gfx::Rect(display_bounds.right() - 10, display_bounds.y() - 10, 10, 10);
  ShowBackButton(anchor_rect);
  button_bounds = GetBackButtonBounds();
  EXPECT_EQ(display_bounds.top_right(), button_bounds.top_right());
}

}  // namespace ash
