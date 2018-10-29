// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_tooltip_manager.h"

#include <memory>

#include "ash/public/cpp/shelf_model.h"
#include "ash/shelf/app_list_button.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_tooltip_bubble_base.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shelf/shelf_view_test_api.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ui/events/event_constants.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/widget/widget.h"

namespace ash {

class ShelfTooltipManagerTest : public AshTestBase {
 public:
  ShelfTooltipManagerTest() = default;
  ~ShelfTooltipManagerTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    shelf_view_ = GetPrimaryShelf()->GetShelfViewForTesting();
    tooltip_manager_ = ShelfViewTestAPI(shelf_view_).tooltip_manager();
    tooltip_manager_->set_timer_delay_for_test(0);
  }

  bool IsTimerRunning() { return tooltip_manager_->timer_.IsRunning(); }
  views::Widget* GetTooltip() { return tooltip_manager_->bubble_->GetWidget(); }

 protected:
  ShelfView* shelf_view_;
  ShelfTooltipManager* tooltip_manager_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ShelfTooltipManagerTest);
};

TEST_F(ShelfTooltipManagerTest, ShowTooltip) {
  tooltip_manager_->ShowTooltip(shelf_view_->GetAppListButton());
  EXPECT_TRUE(tooltip_manager_->IsVisible());
  EXPECT_FALSE(IsTimerRunning());
}

TEST_F(ShelfTooltipManagerTest, ShowTooltipWithDelay) {
  // ShowTooltipWithDelay should start the timer instead of showing immediately.
  tooltip_manager_->ShowTooltipWithDelay(shelf_view_->GetAppListButton());
  EXPECT_FALSE(tooltip_manager_->IsVisible());
  EXPECT_TRUE(IsTimerRunning());
  // TODO: Test that the delayed tooltip is shown, without flaky failures.
}

TEST_F(ShelfTooltipManagerTest, DoNotShowForInvalidView) {
  // The manager should not show or start the timer for a null view.
  tooltip_manager_->ShowTooltip(nullptr);
  EXPECT_FALSE(tooltip_manager_->IsVisible());
  tooltip_manager_->ShowTooltipWithDelay(nullptr);
  EXPECT_FALSE(IsTimerRunning());

  // The manager should not show or start the timer for a non-shelf view.
  views::View view;
  tooltip_manager_->ShowTooltip(&view);
  EXPECT_FALSE(tooltip_manager_->IsVisible());
  tooltip_manager_->ShowTooltipWithDelay(&view);
  EXPECT_FALSE(IsTimerRunning());

  // The manager should start the timer for a view on the shelf.
  ShelfModel* model = shelf_view_->model();
  ShelfItem item;
  item.id = ShelfID("foo");
  item.type = TYPE_PINNED_APP;
  const int index = model->Add(item);
  // Note: There's no easy way to correlate shelf a model index/id to its view.
  tooltip_manager_->ShowTooltipWithDelay(
      shelf_view_->child_at(shelf_view_->child_count() - 1));
  EXPECT_TRUE(IsTimerRunning());

  // Removing the view won't stop the timer, but the tooltip shouldn't be shown.
  model->RemoveItemAt(index);
  EXPECT_TRUE(IsTimerRunning());
  RunAllPendingInMessageLoop();
  EXPECT_FALSE(IsTimerRunning());
  EXPECT_FALSE(tooltip_manager_->IsVisible());
}

TEST_F(ShelfTooltipManagerTest, HideWhenShelfIsHidden) {
  tooltip_manager_->ShowTooltip(shelf_view_->GetAppListButton());
  ASSERT_TRUE(tooltip_manager_->IsVisible());

  // Create a full-screen window to hide the shelf.
  std::unique_ptr<views::Widget> widget = CreateTestWidget();
  widget->SetFullscreen(true);

  // Once the shelf is hidden, the tooltip should be invisible.
  ASSERT_EQ(SHELF_HIDDEN, GetPrimaryShelf()->GetVisibilityState());
  EXPECT_FALSE(tooltip_manager_->IsVisible());

  // Do not show the view if the shelf is hidden.
  tooltip_manager_->ShowTooltip(shelf_view_->GetAppListButton());
  EXPECT_FALSE(tooltip_manager_->IsVisible());

  // ShowTooltipWithDelay doesn't even start the timer for the hidden shelf.
  tooltip_manager_->ShowTooltipWithDelay(shelf_view_->GetAppListButton());
  EXPECT_FALSE(IsTimerRunning());
}

TEST_F(ShelfTooltipManagerTest, HideWhenShelfIsAutoHideHidden) {
  // Create a visible window so auto-hide behavior can actually hide the shelf.
  std::unique_ptr<views::Widget> widget = CreateTestWidget();
  tooltip_manager_->ShowTooltip(shelf_view_->GetAppListButton());
  ASSERT_TRUE(tooltip_manager_->IsVisible());

  GetPrimaryShelf()->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  GetPrimaryShelf()->UpdateAutoHideState();
  ASSERT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS,
            GetPrimaryShelf()->auto_hide_behavior());
  ASSERT_EQ(SHELF_AUTO_HIDE_HIDDEN, GetPrimaryShelf()->GetAutoHideState());
  EXPECT_FALSE(tooltip_manager_->IsVisible());

  // Do not show the view if the shelf is hidden.
  tooltip_manager_->ShowTooltip(shelf_view_->GetAppListButton());
  EXPECT_FALSE(tooltip_manager_->IsVisible());

  // ShowTooltipWithDelay doesn't even run the timer for the hidden shelf.
  tooltip_manager_->ShowTooltipWithDelay(shelf_view_->GetAppListButton());
  EXPECT_FALSE(IsTimerRunning());

  // Close the window to show the auto-hide shelf; tooltips should now show.
  widget.reset();
  GetPrimaryShelf()->UpdateAutoHideState();
  ASSERT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS,
            GetPrimaryShelf()->auto_hide_behavior());
  ASSERT_EQ(SHELF_AUTO_HIDE_SHOWN, GetPrimaryShelf()->GetAutoHideState());

  // The tooltip should show for an auto-hide-shown shelf.
  tooltip_manager_->ShowTooltip(shelf_view_->GetAppListButton());
  EXPECT_TRUE(tooltip_manager_->IsVisible());

  // ShowTooltipWithDelay should run the timer for an auto-hide-shown shelf.
  tooltip_manager_->ShowTooltipWithDelay(shelf_view_->GetAppListButton());
  EXPECT_TRUE(IsTimerRunning());
}

TEST_F(ShelfTooltipManagerTest, HideForEvents) {
  ui::test::EventGenerator* generator = GetEventGenerator();
  gfx::Rect shelf_bounds = shelf_view_->GetBoundsInScreen();

  // Should hide if the mouse exits the shelf area.
  tooltip_manager_->ShowTooltip(shelf_view_->GetAppListButton());
  ASSERT_TRUE(tooltip_manager_->IsVisible());
  generator->MoveMouseTo(shelf_bounds.CenterPoint());
  generator->SendMouseExit();
  EXPECT_FALSE(tooltip_manager_->IsVisible());

  // Should hide if the mouse is pressed in the shelf area.
  tooltip_manager_->ShowTooltip(shelf_view_->GetAppListButton());
  ASSERT_TRUE(tooltip_manager_->IsVisible());
  generator->MoveMouseTo(shelf_bounds.CenterPoint());
  generator->PressLeftButton();
  EXPECT_FALSE(tooltip_manager_->IsVisible());

  // Should hide for touch events in the shelf.
  tooltip_manager_->ShowTooltip(shelf_view_->GetAppListButton());
  ASSERT_TRUE(tooltip_manager_->IsVisible());
  generator->set_current_location(shelf_bounds.CenterPoint());
  generator->PressTouch();
  EXPECT_FALSE(tooltip_manager_->IsVisible());

  // Should hide for gesture events in the shelf.
  tooltip_manager_->ShowTooltip(shelf_view_->GetAppListButton());
  ASSERT_TRUE(tooltip_manager_->IsVisible());
  generator->GestureTapDownAndUp(shelf_bounds.CenterPoint());
  EXPECT_FALSE(tooltip_manager_->IsVisible());
}

TEST_F(ShelfTooltipManagerTest, HideForExternalEvents) {
  ui::test::EventGenerator* generator = GetEventGenerator();

  // Should hide for touches outside the shelf.
  tooltip_manager_->ShowTooltip(shelf_view_->GetAppListButton());
  ASSERT_TRUE(tooltip_manager_->IsVisible());
  generator->set_current_location(gfx::Point());
  generator->PressTouch();
  EXPECT_FALSE(tooltip_manager_->IsVisible());
  generator->ReleaseTouch();

  // Should hide for touch events on the tooltip.
  tooltip_manager_->ShowTooltip(shelf_view_->GetAppListButton());
  ASSERT_TRUE(tooltip_manager_->IsVisible());
  generator->set_current_location(
      GetTooltip()->GetWindowBoundsInScreen().CenterPoint());
  generator->PressTouch();
  EXPECT_FALSE(tooltip_manager_->IsVisible());
  generator->ReleaseTouch();

  // Should hide for gestures outside the shelf.
  tooltip_manager_->ShowTooltip(shelf_view_->GetAppListButton());
  ASSERT_TRUE(tooltip_manager_->IsVisible());
  generator->GestureTapDownAndUp(gfx::Point());
  EXPECT_FALSE(tooltip_manager_->IsVisible());
}

TEST_F(ShelfTooltipManagerTest, DoNotHideForKeyEvents) {
  ui::test::EventGenerator* generator = GetEventGenerator();

  // Should not hide for key events.
  tooltip_manager_->ShowTooltip(shelf_view_->GetAppListButton());
  ASSERT_TRUE(tooltip_manager_->IsVisible());
  generator->PressKey(ui::VKEY_A, ui::EF_NONE);
  EXPECT_TRUE(tooltip_manager_->IsVisible());
}

}  // namespace ash
