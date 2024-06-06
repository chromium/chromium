// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_tooltip_manager.h"

#include <memory>

#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/test/test_shelf_item_delegate.h"
#include "ash/shelf/home_button.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_bubble.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shelf/shelf_view_test_api.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/collision_detection/collision_detection_utils.h"
#include "ash/wm/desks/desk_button/desk_button.h"
#include "ash/wm/desks/desk_button/desk_button_container.h"
#include "ash/wm/desks/desk_button/desk_switch_button.h"
#include "ash/wm/desks/desks_test_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "ui/events/event_constants.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/widget/widget.h"

namespace ash {

class ShelfTooltipManagerTest : public AshTestBase {
 public:
  ShelfTooltipManagerTest() = default;

  ShelfTooltipManagerTest(const ShelfTooltipManagerTest&) = delete;
  ShelfTooltipManagerTest& operator=(const ShelfTooltipManagerTest&) = delete;

  ~ShelfTooltipManagerTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    shelf_view_ = GetPrimaryShelf()->GetShelfViewForTesting();
    test_api_ = std::make_unique<ShelfViewTestAPI>(shelf_view_);
    test_api_->SetAnimationDuration(base::Milliseconds(1));
    test_api_->AddItem(TYPE_PINNED_APP);
    test_api_->RunMessageLoopUntilAnimationsDone();
    tooltip_manager_ = test_api_->tooltip_manager();
    tooltip_manager_->set_timer_delay_for_test(0);
  }

  bool IsTimerRunning() { return tooltip_manager_->timer_.IsRunning(); }
  views::Widget* GetTooltip() { return tooltip_manager_->bubble_->GetWidget(); }

  void ShowTooltipForFirstAppIcon() {
    EXPECT_GE(shelf_view_->number_of_visible_apps(), 1u);
    tooltip_manager_->ShowTooltip(
        shelf_view_->first_visible_button_for_testing());
  }

 protected:
  raw_ptr<ShelfView, DanglingUntriaged> shelf_view_;
  raw_ptr<ShelfTooltipManager, DanglingUntriaged> tooltip_manager_;
  std::unique_ptr<ShelfViewTestAPI> test_api_;
};

TEST_F(ShelfTooltipManagerTest, ShowTooltip) {
  ShowTooltipForFirstAppIcon();
  EXPECT_TRUE(tooltip_manager_->IsVisible());
  EXPECT_FALSE(IsTimerRunning());
}

TEST_F(ShelfTooltipManagerTest, ShowTooltipWithDelay) {
  // ShowTooltipWithDelay should start the timer instead of showing immediately.
  tooltip_manager_->ShowTooltipWithDelay(
      shelf_view_->first_visible_button_for_testing());
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
  const int index =
      model->Add(item, std::make_unique<TestShelfItemDelegate>(item.id));
  ShelfViewTestAPI(GetPrimaryShelf()->GetShelfViewForTesting())
      .RunMessageLoopUntilAnimationsDone();

  // The index of a ShelfItem in the model should be the same as its index
  // within the |shelf_view_|'s list of children.
  tooltip_manager_->ShowTooltipWithDelay(shelf_view_->children().at(index));
  EXPECT_TRUE(IsTimerRunning());

  // Removing the view won't stop the timer, but the tooltip shouldn't be shown.
  model->RemoveItemAt(index);
  EXPECT_TRUE(IsTimerRunning());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(IsTimerRunning());
  EXPECT_FALSE(tooltip_manager_->IsVisible());
}

TEST_F(ShelfTooltipManagerTest, HideWhenShelfIsHidden) {
  ShowTooltipForFirstAppIcon();
  ASSERT_TRUE(tooltip_manager_->IsVisible());

  // Create a full-screen window to hide the shelf.
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  widget->SetFullscreen(true);

  // Once the shelf is hidden, the tooltip should be invisible.
  ASSERT_EQ(SHELF_HIDDEN, GetPrimaryShelf()->GetVisibilityState());
  EXPECT_FALSE(tooltip_manager_->IsVisible());

  // Do not show the view if the shelf is hidden.
  ShowTooltipForFirstAppIcon();
  EXPECT_FALSE(tooltip_manager_->IsVisible());

  // ShowTooltipWithDelay doesn't even start the timer for the hidden shelf.
  tooltip_manager_->ShowTooltipWithDelay(
      shelf_view_->first_visible_button_for_testing());
  EXPECT_FALSE(IsTimerRunning());
}

TEST_F(ShelfTooltipManagerTest, HideWhenShelfIsAutoHideHidden) {
  // Create a visible window so auto-hide behavior can actually hide the shelf.
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  ShowTooltipForFirstAppIcon();
  ASSERT_TRUE(tooltip_manager_->IsVisible());

  GetPrimaryShelf()->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);
  GetPrimaryShelf()->UpdateAutoHideState();
  ASSERT_EQ(ShelfAutoHideBehavior::kAlways,
            GetPrimaryShelf()->auto_hide_behavior());
  ASSERT_EQ(SHELF_AUTO_HIDE_HIDDEN, GetPrimaryShelf()->GetAutoHideState());
  EXPECT_FALSE(tooltip_manager_->IsVisible());

  // Do not show the view if the shelf is hidden.
  ShowTooltipForFirstAppIcon();
  EXPECT_FALSE(tooltip_manager_->IsVisible());

  // ShowTooltipWithDelay doesn't even run the timer for the hidden shelf.
  tooltip_manager_->ShowTooltipWithDelay(
      shelf_view_->first_visible_button_for_testing());
  EXPECT_FALSE(IsTimerRunning());

  // Close the window to show the auto-hide shelf; tooltips should now show.
  widget.reset();
  GetPrimaryShelf()->UpdateAutoHideState();
  ASSERT_EQ(ShelfAutoHideBehavior::kAlways,
            GetPrimaryShelf()->auto_hide_behavior());
  ASSERT_EQ(SHELF_AUTO_HIDE_SHOWN, GetPrimaryShelf()->GetAutoHideState());

  // The tooltip should show for an auto-hide-shown shelf.
  ShowTooltipForFirstAppIcon();
  EXPECT_TRUE(tooltip_manager_->IsVisible());

  // ShowTooltipWithDelay should run the timer for an auto-hide-shown shelf.
  tooltip_manager_->ShowTooltipWithDelay(
      shelf_view_->first_visible_button_for_testing());
  EXPECT_TRUE(IsTimerRunning());
}

TEST_F(ShelfTooltipManagerTest, HideForEvents) {
  ui::test::EventGenerator* generator = GetEventGenerator();
  gfx::Rect shelf_bounds = shelf_view_->GetBoundsInScreen();

  // Should hide if the mouse exits the shelf area.
  ShowTooltipForFirstAppIcon();
  ASSERT_TRUE(tooltip_manager_->IsVisible());
  generator->MoveMouseTo(shelf_bounds.CenterPoint());
  generator->SendMouseExit();
  EXPECT_FALSE(tooltip_manager_->IsVisible());

  // Should hide if the mouse is pressed in the shelf area.
  ShowTooltipForFirstAppIcon();
  ASSERT_TRUE(tooltip_manager_->IsVisible());
  generator->MoveMouseTo(shelf_bounds.CenterPoint());
  generator->PressLeftButton();
  EXPECT_FALSE(tooltip_manager_->IsVisible());
  generator->ReleaseLeftButton();

  // Should hide for touch events in the shelf.
  ShowTooltipForFirstAppIcon();
  ASSERT_TRUE(tooltip_manager_->IsVisible());
  generator->set_current_screen_location(shelf_bounds.CenterPoint());
  generator->PressTouch();
  EXPECT_FALSE(tooltip_manager_->IsVisible());

  // Should hide for gesture events in the shelf.
  ShowTooltipForFirstAppIcon();
  ASSERT_TRUE(tooltip_manager_->IsVisible());
  generator->GestureTapDownAndUp(shelf_bounds.CenterPoint());
  EXPECT_FALSE(tooltip_manager_->IsVisible());
}

TEST_F(ShelfTooltipManagerTest, HideForExternalEvents) {
  ui::test::EventGenerator* generator = GetEventGenerator();

  // Should hide for touches outside the shelf.
  ShowTooltipForFirstAppIcon();
  ASSERT_TRUE(tooltip_manager_->IsVisible());
  generator->set_current_screen_location(gfx::Point());
  generator->PressTouch();
  EXPECT_FALSE(tooltip_manager_->IsVisible());
  generator->ReleaseTouch();

  // Should hide for touch events on the tooltip.
  ShowTooltipForFirstAppIcon();
  ASSERT_TRUE(tooltip_manager_->IsVisible());
  generator->set_current_screen_location(
      GetTooltip()->GetWindowBoundsInScreen().CenterPoint());
  generator->PressTouch();
  EXPECT_FALSE(tooltip_manager_->IsVisible());
  generator->ReleaseTouch();

  // Should hide for gestures outside the shelf.
  ShowTooltipForFirstAppIcon();
  ASSERT_TRUE(tooltip_manager_->IsVisible());
  generator->GestureTapDownAndUp(gfx::Point());
  EXPECT_FALSE(tooltip_manager_->IsVisible());
}

TEST_F(ShelfTooltipManagerTest, KeyEvents) {
  ui::test::EventGenerator* generator = GetEventGenerator();

  // Should hide when 'Esc' is pressed.
  ShowTooltipForFirstAppIcon();
  ASSERT_TRUE(tooltip_manager_->IsVisible());
  generator->PressKey(ui::VKEY_ESCAPE, ui::EF_NONE);
  EXPECT_FALSE(tooltip_manager_->IsVisible());
}

TEST_F(ShelfTooltipManagerTest, ShelfTooltipDoesNotAffectPipWindow) {
  ShowTooltipForFirstAppIcon();
  EXPECT_TRUE(tooltip_manager_->IsVisible());

  auto display = display::Screen::GetScreen()->GetPrimaryDisplay();
  auto tooltip_bounds = GetTooltip()->GetWindowBoundsInScreen();
  tooltip_bounds.Intersect(CollisionDetectionUtils::GetMovementArea(display));
  EXPECT_FALSE(tooltip_bounds.IsEmpty());
  EXPECT_EQ(tooltip_bounds,
            CollisionDetectionUtils::GetRestingPosition(
                display, tooltip_bounds,
                CollisionDetectionUtils::RelativePriority::kPictureInPicture));
}

TEST_F(ShelfTooltipManagerTest, ShelfTooltipClosesIfScroll) {
  ui::test::EventGenerator* generator = GetEventGenerator();

  ShowTooltipForFirstAppIcon();
  ASSERT_TRUE(tooltip_manager_->IsVisible());
  gfx::Point cursor_position_in_screen =
      display::Screen::GetScreen()->GetCursorScreenPoint();
  generator->ScrollSequence(cursor_position_in_screen, base::TimeDelta(), 0, 3,
                            10, 1);
  EXPECT_FALSE(tooltip_manager_->IsVisible());
}

namespace {

using ::testing::ValuesIn;

class ShelfTooltipManagerDeskButtonTest
    : public ShelfTooltipManagerTest,
      public ::testing::WithParamInterface<ShelfAlignment> {
 public:
  ShelfTooltipManagerDeskButtonTest() = default;
  ShelfTooltipManagerDeskButtonTest(const ShelfTooltipManagerDeskButtonTest&) =
      delete;
  ShelfTooltipManagerDeskButtonTest& operator=(
      const ShelfTooltipManagerDeskButtonTest&) = delete;
  ~ShelfTooltipManagerDeskButtonTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(features::kDeskButton);
    ShelfTooltipManagerTest::SetUp();
    Shelf::ForWindow(Shell::GetPrimaryRootWindow())->SetAlignment(GetParam());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         ShelfTooltipManagerDeskButtonTest,
                         ValuesIn({ShelfAlignment::kBottom,
                                   ShelfAlignment::kLeft,
                                   ShelfAlignment::kRight}));

}  // namespace

// Verifies that tooltips for the desk button and its child views are correctly
// centered directly above their respective views, or if that position is out of
// the display then it verifies that they are still in bounds.
TEST_P(ShelfTooltipManagerDeskButtonTest, TooltipPositioning) {
  NewDesk();
  NewDesk();
  DesksController* desks_controller = DesksController::Get();
  DeskSwitchAnimationWaiter waiter;
  desks_controller->ActivateDesk(desks_controller->desks()[1].get(),
                                 DesksSwitchSource::kDeskButtonSwitchButton);
  waiter.Wait();

  auto validate_tooltip_bounds = [&](views::View* target_view) {
    tooltip_manager_->ShowTooltip(target_view);
    const gfx::Rect tooltip_bounds = GetTooltip()->GetWindowBoundsInScreen();
    const gfx::Rect target_view_bounds = target_view->GetBoundsInScreen();
    const gfx::Rect root_window_bounds =
        Shell::Get()->GetPrimaryRootWindow()->bounds();

    // These exceptions account for out of bounds adjustments for the tooltips
    // on the left and right shelves.
    const bool left_alignment_exception =
        (GetParam() == ShelfAlignment::kLeft && tooltip_bounds.x() == 0 &&
         tooltip_bounds.bottom() == target_view_bounds.y());
    const bool right_alignment_exception =
        (GetParam() == ShelfAlignment::kRight &&
         tooltip_bounds.right() == root_window_bounds.right() &&
         tooltip_bounds.bottom() == target_view_bounds.y());

    if (left_alignment_exception || right_alignment_exception) {
      return;
    }

    ASSERT_EQ(target_view_bounds.top_center(), tooltip_bounds.bottom_center());
  };

  auto* desk_button_container =
      GetPrimaryShelf()->desk_button_widget()->GetDeskButtonContainer();
  validate_tooltip_bounds(desk_button_container->desk_button());
  validate_tooltip_bounds(desk_button_container->prev_desk_button());
  validate_tooltip_bounds(desk_button_container->next_desk_button());
}

}  // namespace ash
