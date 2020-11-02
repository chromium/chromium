// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/unified_system_tray.h"

#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/system/unified/unified_slider_bubble_controller.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/test/ash_test_base.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

namespace ash {

class UnifiedSystemTrayTest : public AshTestBase {
 public:
  UnifiedSystemTrayTest() = default;
  ~UnifiedSystemTrayTest() override = default;

 protected:
  bool IsSliderBubbleShown() {
    return GetPrimaryUnifiedSystemTray()
        ->slider_bubble_controller_->bubble_widget_;
  }

  UnifiedSliderBubbleController::SliderType GetSliderBubbleType() {
    return GetPrimaryUnifiedSystemTray()
        ->slider_bubble_controller_->slider_type_;
  }

  UnifiedSystemTrayBubble* GetUnifiedSystemTrayBubble() {
    return GetPrimaryUnifiedSystemTray()->bubble_.get();
  }

  void UpdateAutoHideStateNow() {
    GetPrimaryShelf()->shelf_layout_manager()->UpdateAutoHideStateNow();
  }

  gfx::Rect GetBubbleViewBounds() {
    auto* bubble =
        GetPrimaryUnifiedSystemTray()->slider_bubble_controller_->bubble_view_;
    return bubble ? bubble->GetBoundsInScreen() : gfx::Rect();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(UnifiedSystemTrayTest);
};

TEST_F(UnifiedSystemTrayTest, ShowVolumeSliderBubble) {
  // The volume popup is not visible initially.
  EXPECT_FALSE(IsSliderBubbleShown());

  // When set to autohide, the shelf shouldn't be shown.
  StatusAreaWidget* status = StatusAreaWidgetTestHelper::GetStatusAreaWidget();
  EXPECT_FALSE(status->ShouldShowShelf());

  // Simulate ARC asking to show the volume view.
  GetPrimaryUnifiedSystemTray()->ShowVolumeSliderBubble();

  // Volume view is now visible.
  EXPECT_TRUE(IsSliderBubbleShown());
  EXPECT_EQ(UnifiedSliderBubbleController::SLIDER_TYPE_VOLUME,
            GetSliderBubbleType());

  // This does not force the shelf to automatically show. Regression tests for
  // crbug.com/729188
  EXPECT_FALSE(status->ShouldShowShelf());
}

TEST_F(UnifiedSystemTrayTest, SliderBubbleMovesOnShelfAutohide) {
  // The slider button should be moved when the autohidden shelf is shown, so
  // as to not overlap. Regression test for crbug.com/1136564
  auto* shelf = GetPrimaryShelf();
  shelf->SetAlignment(ShelfAlignment::kBottom);
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);

  // Create a test widget to make auto-hiding work. Auto-hidden shelf will
  // remain visible if no windows are shown, making it impossible to properly
  // test.
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
  params.bounds = gfx::Rect(0, 0, 200, 200);
  params.context = GetContext();
  views::Widget* widget = new views::Widget;
  widget->Init(std::move(params));
  widget->Show();

  // Start off the mouse nowhere near the shelf; the shelf should be hidden.
  display::Display display = display::Screen::GetScreen()->GetPrimaryDisplay();
  auto center = display.bounds().CenterPoint();
  auto bottom_center = display.bounds().bottom_center();
  bottom_center.set_y(bottom_center.y() - 1);
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(center);
  UpdateAutoHideStateNow();

  GetPrimaryUnifiedSystemTray()->ShowVolumeSliderBubble();

  gfx::Rect before_bounds = GetBubbleViewBounds();

  // Now move the mouse close to the edge, so that the shelf shows, and verify
  // that the volume slider adjusts accordingly.
  generator->MoveMouseTo(bottom_center);
  UpdateAutoHideStateNow();
  gfx::Rect after_bounds = GetBubbleViewBounds();
  EXPECT_NE(after_bounds, before_bounds);

  // Also verify that the shelf and slider bubble would have overlapped, but do
  // not now that we've moved the slider bubble.
  gfx::Rect shelf_bounds = shelf->GetShelfBoundsInScreen();
  EXPECT_TRUE(before_bounds.Intersects(shelf_bounds));
  EXPECT_FALSE(after_bounds.Intersects(shelf_bounds));

  // Move the mouse away and verify that it adjusts back to its original
  // position.
  generator->MoveMouseTo(center);
  UpdateAutoHideStateNow();
  after_bounds = GetBubbleViewBounds();
  EXPECT_EQ(after_bounds, before_bounds);

  // Now fullscreen and restore our window with autohide disabled and verify
  // that the bubble moves down as the shelf disappears and reappears. Disable
  // autohide so that the shelf is initially showing.
  shelf->SetAlignment(ShelfAlignment::kRight);
  after_bounds = GetBubbleViewBounds();
  EXPECT_NE(after_bounds, before_bounds);
  shelf->SetAlignment(ShelfAlignment::kBottom);
  after_bounds = GetBubbleViewBounds();
  EXPECT_EQ(after_bounds, before_bounds);

  // Adjust the alignment of the shelf, and verify that the bubble moves along
  // with it.
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kNever);
  before_bounds = GetBubbleViewBounds();
  widget->SetFullscreen(true);
  after_bounds = GetBubbleViewBounds();
  EXPECT_NE(after_bounds, before_bounds);
  widget->SetFullscreen(false);
  after_bounds = GetBubbleViewBounds();
  EXPECT_EQ(after_bounds, before_bounds);
}

TEST_F(UnifiedSystemTrayTest, ShowBubble_MultipleDisplays_OpenedOnSameDisplay) {
  // Initialize two displays with 800x800 resolution.
  UpdateDisplay("400+400-800x600,1220+400-800x600");
  auto* screen = display::Screen::GetScreen();
  EXPECT_EQ(2, screen->GetNumDisplays());

  // The tray bubble for each display should be opened on the same display.
  // See crbug.com/937420.
  for (int i = 0; i < screen->GetNumDisplays(); ++i) {
    auto* system_tray = GetPrimaryUnifiedSystemTray();
    system_tray->ShowBubble(true /* show_by_click */);
    const gfx::Rect primary_display_bounds = GetPrimaryDisplay().bounds();
    const gfx::Rect tray_bubble_bounds =
        GetPrimaryUnifiedSystemTray()->GetBubbleBoundsInScreen();
    EXPECT_TRUE(primary_display_bounds.Contains(tray_bubble_bounds))
        << "primary display bounds=" << primary_display_bounds.ToString()
        << ", tray bubble bounds=" << tray_bubble_bounds.ToString();

    SwapPrimaryDisplay();
  }
}

}  // namespace ash
