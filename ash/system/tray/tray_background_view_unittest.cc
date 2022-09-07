// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/tray_background_view.h"

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shell.h"
#include "ash/system/accessibility/dictation_button_tray.h"
#include "ash/system/status_area_widget_delegate.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/system/tray/tray_bubble_wrapper.h"
#include "ash/test/ash_test_base.h"
#include "base/test/task_environment.h"
#include "components/user_manager/user_manager.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/scoped_layer_animation_settings.h"

namespace ash {

class TrayBackgroundViewTestView : public TrayBackgroundView,
                                   public ui::SimpleMenuModel::Delegate {
 public:
  explicit TrayBackgroundViewTestView(Shelf* shelf)
      : TrayBackgroundView(shelf), provide_menu_model_(false) {}

  TrayBackgroundViewTestView(const TrayBackgroundViewTestView&) = delete;
  TrayBackgroundViewTestView& operator=(const TrayBackgroundViewTestView&) =
      delete;

  ~TrayBackgroundViewTestView() override = default;

  // TrayBackgroundView:
  void ClickedOutsideBubble() override {}
  std::u16string GetAccessibleNameForTray() override {
    return u"TrayBackgroundViewTestView";
  }
  void HandleLocaleChange() override {}
  void HideBubbleWithView(const TrayBubbleView* bubble_view) override {}
  std::unique_ptr<ui::SimpleMenuModel> CreateContextMenuModel() override {
    return provide_menu_model_ ? std::make_unique<ui::SimpleMenuModel>(this)
                               : nullptr;
  }
  void OnAnyBubbleVisibilityChanged(views::Widget* bubble_widget,
                                    bool visible) override {
    on_bubble_visibility_change_captured_widget_ = bubble_widget;
    on_bubble_visibility_change_captured_visibility_ = visible;
  }

  // ui::SimpleMenuModel::Delegate:
  void ExecuteCommand(int command_id, int event_flags) override {}

  void set_provide_menu_model(bool provide_menu_model) {
    provide_menu_model_ = provide_menu_model;
  }

  void SetShouldShowMenu(bool should_show_menu) {
    SetContextMenuEnabled(should_show_menu);
  }

  views::Widget* on_bubble_visibility_change_captured_widget_ = nullptr;
  bool on_bubble_visibility_change_captured_visibility_ = false;

 private:
  bool provide_menu_model_;
};

class TrayBackgroundViewTest : public AshTestBase {
 public:
  TrayBackgroundViewTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  TrayBackgroundViewTest(const TrayBackgroundViewTest&) = delete;
  TrayBackgroundViewTest& operator=(const TrayBackgroundViewTest&) = delete;

  ~TrayBackgroundViewTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();

    // Adds this `test_view_` to the mock `StatusAreaWidget`. We need to
    // remove the layout manager from the delegate before adding a new
    // child, since there's a DCHECK in the `GridLayout` to assert no more
    // children can be added.
    // Can't use std::make_unique() here, because we need base class type for
    // template method to link successfully without adding test code to
    // status_area_widget.cc.
    test_view_ = static_cast<TrayBackgroundViewTestView*>(
        StatusAreaWidgetTestHelper::GetStatusAreaWidget()->AddTrayButton(
            std::unique_ptr<TrayBackgroundView>(
                new TrayBackgroundViewTestView(GetPrimaryShelf()))));

    // Set Dictation button to be visible.
    AccessibilityControllerImpl* controller =
        Shell::Get()->accessibility_controller();
    controller->dictation().SetEnabled(true);
  }

  TrayBackgroundViewTestView* test_view() const { return test_view_; }

 protected:
  // Here we use dictation tray for testing secondary screen.
  DictationButtonTray* GetPrimaryDictationTray() {
    return StatusAreaWidgetTestHelper::GetStatusAreaWidget()
        ->dictation_button_tray();
  }

  DictationButtonTray* GetSecondaryDictationTray() {
    return StatusAreaWidgetTestHelper::GetSecondaryStatusAreaWidget()
        ->dictation_button_tray();
  }

  bool TriggerAutoHideTimeout(ShelfLayoutManager* layout_manager) {
    if (!layout_manager->auto_hide_timer_.IsRunning())
      return false;

    layout_manager->auto_hide_timer_.FireNow();
    return true;
  }

 private:
  TrayBackgroundViewTestView* test_view_ = nullptr;
};

TEST_F(TrayBackgroundViewTest, ShowingAnimationAbortedByHideAnimation) {
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);

  // Starts showing up animation.
  test_view()->SetVisiblePreferred(true);
  EXPECT_TRUE(test_view()->GetVisible());
  EXPECT_TRUE(test_view()->layer()->GetTargetVisibility());
  EXPECT_TRUE(test_view()->layer()->GetAnimator()->is_animating());

  // Starts hide animation. The view is visible but the layer's target
  // visibility is false.
  test_view()->SetVisiblePreferred(false);
  EXPECT_TRUE(test_view()->GetVisible());
  EXPECT_FALSE(test_view()->layer()->GetTargetVisibility());
  EXPECT_TRUE(test_view()->layer()->GetAnimator()->is_animating());

  // Here we wait until the animation is finished and we give it one more second
  // to finish the callbacks in `OnVisibilityAnimationFinished()`.
  StatusAreaWidgetTestHelper::WaitForLayerAnimationEnd(test_view()->layer());
  task_environment()->FastForwardBy(base::Seconds(1));

  // After the hide animation is finished, test_view() is not visible.
  EXPECT_FALSE(test_view()->GetVisible());
  EXPECT_FALSE(test_view()->layer()->GetAnimator()->is_animating());
}

TEST_F(TrayBackgroundViewTest, HandleSessionChange) {
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);

  // Not showing animation after logging in.
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOGIN_PRIMARY);
  // Gives it a small duration to let the session get changed. This duration is
  // way smaller than the animation duration, so that the animation will not
  // finish when this duration ends. The same for the other places below.
  task_environment()->FastForwardBy(base::Milliseconds(20));

  test_view()->SetVisiblePreferred(false);
  test_view()->SetVisiblePreferred(true);
  task_environment()->FastForwardBy(base::Milliseconds(20));
  EXPECT_TRUE(test_view()->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(test_view()->GetVisible());

  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::ACTIVE);
  task_environment()->FastForwardBy(base::Milliseconds(20));
  EXPECT_FALSE(test_view()->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(test_view()->GetVisible());

  // Enable the animation after session state get changed.
  task_environment()->FastForwardBy(base::Milliseconds(20));
  test_view()->SetVisiblePreferred(false);
  test_view()->SetVisiblePreferred(true);
  task_environment()->FastForwardBy(base::Milliseconds(20));
  EXPECT_TRUE(test_view()->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(test_view()->GetVisible());

  // Not showing animation after unlocking screen.
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOCKED);
  task_environment()->FastForwardBy(base::Milliseconds(20));

  test_view()->SetVisiblePreferred(false);
  test_view()->SetVisiblePreferred(true);
  task_environment()->FastForwardBy(base::Milliseconds(20));
  EXPECT_TRUE(test_view()->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(test_view()->GetVisible());

  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::ACTIVE);
  task_environment()->FastForwardBy(base::Milliseconds(20));
  EXPECT_FALSE(test_view()->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(test_view()->GetVisible());

  // Not showing animation when switching users.
  GetSessionControllerClient()->AddUserSession("a");
  test_view()->SetVisiblePreferred(false);
  test_view()->SetVisiblePreferred(true);
  EXPECT_TRUE(test_view()->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(test_view()->GetVisible());

  // Simulates user switching by changing the order of session_ids.
  Shell::Get()->session_controller()->SetUserSessionOrder({2u, 1u});
  task_environment()->FastForwardBy(base::Milliseconds(20));
  EXPECT_FALSE(test_view()->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(test_view()->GetVisible());
}

// TODO(crbug.com/1314693): Flaky.
TEST_F(TrayBackgroundViewTest, DISABLED_SecondaryDisplay) {
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);

  // Add secondary screen.
  UpdateDisplay("800x600,800x600");

  // Switch the primary and secondary screen.
  SwapPrimaryDisplay();
  task_environment()->FastForwardBy(base::Milliseconds(20));
  EXPECT_FALSE(
      GetPrimaryDictationTray()->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(GetPrimaryDictationTray()->GetVisible());
  EXPECT_FALSE(
      GetSecondaryDictationTray()->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(GetSecondaryDictationTray()->GetVisible());

  // Enable the animation after showing up on the secondary screen.
  task_environment()->FastForwardBy(base::Milliseconds(20));
  GetPrimaryDictationTray()->SetVisiblePreferred(false);
  GetPrimaryDictationTray()->SetVisiblePreferred(true);
  GetSecondaryDictationTray()->SetVisiblePreferred(false);
  GetSecondaryDictationTray()->SetVisiblePreferred(true);
  task_environment()->FastForwardBy(base::Milliseconds(20));
  EXPECT_TRUE(
      GetPrimaryDictationTray()->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(GetPrimaryDictationTray()->GetVisible());
  EXPECT_TRUE(
      GetSecondaryDictationTray()->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(GetSecondaryDictationTray()->GetVisible());
  task_environment()->FastForwardBy(base::Seconds(3));

  // Remove the secondary screen.
  UpdateDisplay("800x600");

  task_environment()->FastForwardBy(base::Milliseconds(20));
  EXPECT_FALSE(
      GetPrimaryDictationTray()->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(GetPrimaryDictationTray()->GetVisible());
}

// Tests that a context menu only appears when a tray provides a menu model and
// the tray should show a context menu.
TEST_F(TrayBackgroundViewTest, ContextMenu) {
  test_view()->SetVisiblePreferred(true);
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(test_view()->GetBoundsInScreen().CenterPoint());
  EXPECT_FALSE(test_view()->IsShowingMenu());

  // The tray should not display a context menu, and no model is provided, so
  // no menu should appear.
  generator->ClickRightButton();
  EXPECT_FALSE(test_view()->IsShowingMenu());

  // The tray should display a context menu, but no model is provided, so no
  // menu should appear.
  test_view()->SetShouldShowMenu(true);
  generator->ClickRightButton();
  EXPECT_FALSE(test_view()->IsShowingMenu());

  // The tray should display a context menu, and a model is provided, so a menu
  // should appear.
  test_view()->set_provide_menu_model(true);
  generator->ClickRightButton();
  EXPECT_TRUE(test_view()->IsShowingMenu());

  // The tray should not display a context menu, so even though a model is
  // provided, no menu should appear.
  test_view()->SetShouldShowMenu(false);
  generator->ClickRightButton();
  EXPECT_FALSE(test_view()->IsShowingMenu());
}

// Tests the auto-hide shelf status when opening and closing a context menu.
TEST_F(TrayBackgroundViewTest, AutoHideShelfWithContextMenu) {
  // Create one window, or the shelf won't auto-hide.
  std::unique_ptr<views::Widget> unused = CreateTestWidget();

  // Set the shelf to auto-hide.
  Shelf* shelf = test_view()->shelf();
  EXPECT_TRUE(shelf);
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);
  ShelfLayoutManager* layout_manager = shelf->shelf_layout_manager();
  EXPECT_TRUE(layout_manager);
  ASSERT_FALSE(TriggerAutoHideTimeout(layout_manager));
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  // Move mouse to display the shelf.
  ui::test::EventGenerator* generator = GetEventGenerator();
  gfx::Rect display_bounds =
      display::Screen::GetScreen()->GetPrimaryDisplay().bounds();
  generator->MoveMouseTo(display_bounds.bottom_center());
  ASSERT_TRUE(TriggerAutoHideTimeout(layout_manager));
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

  // Open tray context menu.
  test_view()->SetVisiblePreferred(true);
  test_view()->set_provide_menu_model(true);
  test_view()->SetShouldShowMenu(true);
  generator->MoveMouseTo(test_view()->GetBoundsInScreen().CenterPoint());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
  EXPECT_FALSE(test_view()->IsShowingMenu());
  generator->ClickRightButton();
  EXPECT_TRUE(test_view()->IsShowingMenu());

  // Close the context menu with the mouse over the shelf. The shelf should
  // remain shown.
  generator->ClickRightButton();
  ASSERT_FALSE(TriggerAutoHideTimeout(layout_manager));
  EXPECT_FALSE(test_view()->IsShowingMenu());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

  // Reopen tray context menu.
  generator->ClickRightButton();
  EXPECT_TRUE(test_view()->IsShowingMenu());

  // Mouse away from the shelf with the context menu still showing. The shelf
  // should remain shown.
  generator->MoveMouseTo(0, 0);
  ASSERT_TRUE(TriggerAutoHideTimeout(layout_manager));
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

  // Close the context menu with the mouse away from the shelf. The shelf
  // should hide.
  generator->ClickRightButton();
  ASSERT_FALSE(TriggerAutoHideTimeout(layout_manager));
  EXPECT_FALSE(test_view()->IsShowingMenu());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());
}

// Loads a bubble inside the tray and shows that. Then verifies that
// OnAnyBubbleVisibilityChanged is called.
TEST_F(TrayBackgroundViewTest, OnAnyBubbleVisibilityChanged) {
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);

  test_view()->SetVisiblePreferred(true);

  TrayBubbleView::InitParams init_params;
  init_params.delegate = test_view()->GetWeakPtr();
  init_params.parent_window =
      Shell::GetContainer(Shell::GetPrimaryRootWindow(),
                          kShellWindowId_AccessibilityBubbleContainer);
  init_params.anchor_mode = TrayBubbleView::AnchorMode::kRect;
  init_params.preferred_width = 200;
  auto bubble_view = std::make_unique<TrayBubbleView>(init_params);
  bubble_view->SetCanActivate(true);
  auto bubble_ =
      std::make_unique<TrayBubbleWrapper>(test_view(), bubble_view.release(),
                                          /*event_handling=*/false);

  bubble_->GetBubbleWidget()->Show();
  bubble_->GetBubbleWidget()->Activate();
  bubble_->bubble_view()->SetVisible(true);

  EXPECT_EQ(bubble_->GetBubbleWidget(),
            test_view()->on_bubble_visibility_change_captured_widget_);
  EXPECT_TRUE(test_view()->on_bubble_visibility_change_captured_visibility_);
}

}  // namespace ash
