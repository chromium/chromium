// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/switch_access/switch_access_back_button_bubble_controller.h"

#include "ash/accessibility/accessibility_controller.h"
#include "ash/shell.h"
#include "ash/system/accessibility/switch_access/switch_access_back_button_view.h"
#include "ash/system/accessibility/switch_access/switch_access_menu_bubble_controller.h"
#include "ash/test/ash_test_base.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/accessibility/view_accessibility.h"

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

  SwitchAccessBackButtonView* GetBackButton() {
    return GetBubbleController()->back_button_view_;
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

  // When there's space for the button, the top edges of the button and the
  // anchor rect's focus ring should be equal.
  gfx::Rect anchor_rect(100, 100, 50, 50);
  ShowBackButton(anchor_rect);
  gfx::Rect button_bounds = GetBackButtonBounds();
  gfx::Point anchor_top_right = anchor_rect.top_right();
  // The focus ring is shown around the anchor rect, so offset to include that.
  anchor_top_right.Offset(
      SwitchAccessBackButtonBubbleController::kFocusRingPaddingDp,
      -SwitchAccessBackButtonBubbleController::kFocusRingPaddingDp);

  EXPECT_EQ(anchor_top_right, button_bounds.origin());

  // When the anchor rect is aligned with the top edge of the screen, the back
  // button should also align with the top edge of the screen.
  anchor_rect = gfx::Rect(100, 0, 50, 50);
  ShowBackButton(anchor_rect);
  button_bounds = GetBackButtonBounds();
  int anchor_right = anchor_rect.right();
  // The focus ring is shown around the anchor rect, so offset to include that.
  anchor_right += SwitchAccessBackButtonBubbleController::kFocusRingPaddingDp;

  EXPECT_EQ(anchor_right, button_bounds.x());
  EXPECT_EQ(display_bounds.y(), button_bounds.y());

  // When the anchor rect is aligned with the right edge of the screen, the back
  // button should also align with the right edge of the screen.
  anchor_rect = gfx::Rect(display_bounds.right() - 50, 100, 50, 50);
  ShowBackButton(anchor_rect);
  button_bounds = GetBackButtonBounds();
  int anchor_y = anchor_rect.y();
  // The focus ring is shown around the anchor rect, so offset to include that.
  anchor_y -= SwitchAccessBackButtonBubbleController::kFocusRingPaddingDp;
  EXPECT_EQ(display_bounds.right(), button_bounds.right());
  EXPECT_EQ(anchor_y, button_bounds.y());

  // When the anchor rect is very small at the bottom of the screen, the back
  // button should still display entirely onscreen.
  anchor_rect = gfx::Rect(display_bounds.right() - 10,
                          display_bounds.bottom() - 10, 10, 10);
  ShowBackButton(anchor_rect);
  EXPECT_TRUE(display_bounds.Contains(GetBackButtonBounds()));
}

TEST_F(SwitchAccessBackButtonBubbleControllerTest,
       SwitchAccessBackButtonViewAccessibleProperties) {
  gfx::Rect anchor_rect(100, 100, 50, 50);
  ShowBackButton(anchor_rect);
  ui::AXNodeData data;

  GetBackButton()->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(ax::mojom::Role::kButton, data.role);
}

}  // namespace ash
