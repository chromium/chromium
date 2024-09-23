// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/tray_background_view.h"

#include "ash/accessibility/accessibility_controller.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shell.h"
#include "ash/system/accessibility/dictation_button_tray.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/system/tray/tray_bubble_view.h"
#include "ash/system/tray/tray_bubble_wrapper.h"
#include "ash/system/tray/tray_utils.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/test/layer_animation_stopped_waiter.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/views/accessibility/view_accessibility.h"

namespace ash {

class TestTrayBackgroundView : public TrayBackgroundView,
                               public ui::SimpleMenuModel::Delegate {
 public:
  explicit TestTrayBackgroundView(Shelf* shelf)
      : TrayBackgroundView(shelf,
                           TrayBackgroundViewCatalogName::kTestCatalogName,
                           RoundedCornerBehavior::kAllRounded) {}

  TestTrayBackgroundView(const TestTrayBackgroundView&) = delete;
  TestTrayBackgroundView& operator=(const TestTrayBackgroundView&) = delete;

  ~TestTrayBackgroundView() override = default;

  // TrayBackgroundView:
  void ClickedOutsideBubble(const ui::LocatedEvent& event) override {}
  void UpdateTrayItemColor(bool is_active) override {}
  std::u16string GetAccessibleNameForTray() override {
    return u"TestTrayBackgroundView";
  }

  void HandleLocaleChange() override {}

  void HideBubbleWithView(const TrayBubbleView* bubble_view) override {
    if (bubble_view == bubble_->GetBubbleView())
      CloseBubble();
  }

  void HideBubble(const TrayBubbleView* bubble_view) override {
    if (bubble_view == bubble_->GetBubbleView()) {
      CloseBubble();
    }
  }

  std::unique_ptr<ui::SimpleMenuModel> CreateContextMenuModel() override {
    return provide_menu_model_ ? std::make_unique<ui::SimpleMenuModel>(this)
                               : nullptr;
  }

  void ShowBubble() override {
    show_bubble_called_ = true;

    auto bubble_view = std::make_unique<TrayBubbleView>(
        CreateInitParamsForTrayBubble(/*tray=*/this));
    bubble_view->SetCanActivate(true);
    bubble_ = std::make_unique<TrayBubbleWrapper>(this,
                                                  /*event_handling=*/false);
    bubble_->ShowBubble(std::move(bubble_view));
    bubble_->GetBubbleWidget()->Activate();
    bubble_->bubble_view()->SetVisible(true);

    SetIsActive(true);
  }

  void CloseBubbleInternal() override {
    bubble_.reset();
    SetIsActive(false);
  }

  // ui::SimpleMenuModel::Delegate:
  void ExecuteCommand(int command_id, int event_flags) override {}

  void set_provide_menu_model(bool provide_menu_model) {
    provide_menu_model_ = provide_menu_model;
  }

  void SetShouldShowMenu(bool should_show_menu) {
    SetContextMenuEnabled(should_show_menu);
  }

  TrayBubbleWrapper* bubble() { return bubble_.get(); }

  TrayBubbleView* bubble_view() { return bubble_->bubble_view(); }

  bool show_bubble_called() const { return show_bubble_called_; }

  std::u16string GetAccessibleNameForBubble() override {
    return u"Sample accessible name";
  }

 private:
  std::unique_ptr<TrayBubbleWrapper> bubble_;
  bool provide_menu_model_ = false;
  bool show_bubble_called_ = false;
};

// A `TrayBackgroundView` whose bubble does not automatically close when the
// lock state changes.
class PersistentBubbleTestTrayBackgroundView : public TestTrayBackgroundView {
 public:
  explicit PersistentBubbleTestTrayBackgroundView(Shelf* shelf)
      : TestTrayBackgroundView(shelf) {
    set_should_close_bubble_on_lock_state_change(false);
  }

  PersistentBubbleTestTrayBackgroundView(
      const PersistentBubbleTestTrayBackgroundView&) = delete;
  PersistentBubbleTestTrayBackgroundView& operator=(
      const PersistentBubbleTestTrayBackgroundView&) = delete;

  ~PersistentBubbleTestTrayBackgroundView() override = default;
};

class TrayBackgroundViewTest : public AshTestBase,
                               public ui::LayerAnimationObserver {
 public:
  TrayBackgroundViewTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  TrayBackgroundViewTest(const TrayBackgroundViewTest&) = delete;
  TrayBackgroundViewTest& operator=(const TrayBackgroundViewTest&) = delete;

  ~TrayBackgroundViewTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();

    // Adds this `test_tray_background_view_` to the mock `StatusAreaWidget`. We
    // need to remove the layout manager from the delegate before adding a new
    // child, since there's a DCHECK in the `GridLayout` to assert no more
    // children can be added.
    // Can't use std::make_unique() here, because we need base class type for
    // template method to link successfully without adding test code to
    // status_area_widget.cc.
    test_tray_background_view_ = static_cast<TestTrayBackgroundView*>(
        StatusAreaWidgetTestHelper::GetStatusAreaWidget()->AddTrayButton(
            std::unique_ptr<TrayBackgroundView>(
                new TestTrayBackgroundView(GetPrimaryShelf()))));

    // Same as above but for a `PersistentBubbleTestTrayBackgroundView`.
    std::unique_ptr<PersistentBubbleTestTrayBackgroundView> tmp =
        std::make_unique<PersistentBubbleTestTrayBackgroundView>(
            GetPrimaryShelf());
    persistent_bubble_test_tray_background_view_ =
        static_cast<PersistentBubbleTestTrayBackgroundView*>(
            StatusAreaWidgetTestHelper::GetStatusAreaWidget()->AddTrayButton(
                std::unique_ptr<TrayBackgroundView>(std::move(tmp))));

    // Set Dictation button to be visible.
    AccessibilityController* controller =
        Shell::Get()->accessibility_controller();
    controller->dictation().SetEnabled(true);
  }

  // ui::LayerAnimationObserver:
  void OnLayerAnimationScheduled(
      ui::LayerAnimationSequence* sequence) override {
    num_animations_scheduled_++;
  }
  void OnLayerAnimationEnded(ui::LayerAnimationSequence* sequence) override {}
  void OnLayerAnimationAborted(ui::LayerAnimationSequence* sequence) override {}

  TestTrayBackgroundView* test_tray_background_view() const {
    return test_tray_background_view_;
  }

  PersistentBubbleTestTrayBackgroundView*
  persistent_bubble_test_tray_background_view() const {
    return persistent_bubble_test_tray_background_view_;
  }

  int num_animations_scheduled() const { return num_animations_scheduled_; }

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
  raw_ptr<TestTrayBackgroundView, DanglingUntriaged>
      test_tray_background_view_ = nullptr;
  raw_ptr<PersistentBubbleTestTrayBackgroundView, DanglingUntriaged>
      persistent_bubble_test_tray_background_view_ = nullptr;
  int num_animations_scheduled_ = 0;
};

// Tests that a `TrayBackgroundView` initially starts in a hidden state.
TEST_F(TrayBackgroundViewTest, InitiallyHidden) {
  EXPECT_FALSE(test_tray_background_view()->GetVisible());
  EXPECT_EQ(test_tray_background_view()->layer()->opacity(), 0.0f);
}

TEST_F(TrayBackgroundViewTest, ShowingAnimationAbortedByHideAnimation) {
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Starts showing up animation.
  test_tray_background_view()->SetVisiblePreferred(true);
  EXPECT_TRUE(test_tray_background_view()->GetVisible());
  EXPECT_TRUE(test_tray_background_view()->layer()->GetTargetVisibility());
  EXPECT_TRUE(
      test_tray_background_view()->layer()->GetAnimator()->is_animating());

  // Starts hide animation. The view is visible but the layer's target
  // visibility is false.
  test_tray_background_view()->SetVisiblePreferred(false);
  EXPECT_TRUE(test_tray_background_view()->GetVisible());
  EXPECT_FALSE(test_tray_background_view()->layer()->GetTargetVisibility());
  EXPECT_TRUE(
      test_tray_background_view()->layer()->GetAnimator()->is_animating());

  // Here we wait until the animation is finished and we give it one more second
  // to finish the callbacks in `OnVisibilityAnimationFinished()`.
  StatusAreaWidgetTestHelper::WaitForLayerAnimationEnd(
      test_tray_background_view()->layer());
  task_environment()->FastForwardBy(base::Seconds(1));

  // After the hide animation is finished, test_tray_background_view() is not
  // visible.
  EXPECT_FALSE(test_tray_background_view()->GetVisible());
  EXPECT_FALSE(
      test_tray_background_view()->layer()->GetAnimator()->is_animating());
}

// Tests that a `TrayBackgroundView` doesn't get notified of events during its
// hide animation.
TEST_F(TrayBackgroundViewTest, EventsDisabledForHideAnimation) {
  // Initially show the tray. Note that animations complete immediately in this
  // part of the test.
  test_tray_background_view()->SetVisiblePreferred(true);

  // Ensure animations don't complete immediately for the rest of the test.
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Start the tray's hide animation and verify that it can't process events.
  test_tray_background_view()->SetVisiblePreferred(false);
  ASSERT_TRUE(test_tray_background_view()->IsDrawn());
  EXPECT_FALSE(test_tray_background_view()->GetCanProcessEventsWithinSubtree());

  // Interrupt the hide animation with a show animation and verify that the tray
  // can process events again.
  test_tray_background_view()->SetVisiblePreferred(true);
  EXPECT_TRUE(test_tray_background_view()->GetCanProcessEventsWithinSubtree());
}

TEST_F(TrayBackgroundViewTest, HandleSessionChange) {
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Not showing animation after logging in.
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOGIN_PRIMARY);
  // Gives it a small duration to let the session get changed. This duration is
  // way smaller than the animation duration, so that the animation will not
  // finish when this duration ends. The same for the other places below.
  task_environment()->FastForwardBy(base::Milliseconds(20));

  test_tray_background_view()->SetVisiblePreferred(false);
  test_tray_background_view()->SetVisiblePreferred(true);
  task_environment()->FastForwardBy(base::Milliseconds(20));
  EXPECT_TRUE(
      test_tray_background_view()->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(test_tray_background_view()->GetVisible());

  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::ACTIVE);
  task_environment()->FastForwardBy(base::Milliseconds(20));
  EXPECT_FALSE(
      test_tray_background_view()->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(test_tray_background_view()->GetVisible());

  // Enable the animation after session state get changed.
  task_environment()->FastForwardBy(base::Milliseconds(20));
  test_tray_background_view()->SetVisiblePreferred(false);
  test_tray_background_view()->SetVisiblePreferred(true);
  task_environment()->FastForwardBy(base::Milliseconds(20));
  EXPECT_TRUE(
      test_tray_background_view()->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(test_tray_background_view()->GetVisible());

  // Not showing animation after unlocking screen.
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOCKED);
  task_environment()->FastForwardBy(base::Milliseconds(20));

  test_tray_background_view()->SetVisiblePreferred(false);
  test_tray_background_view()->SetVisiblePreferred(true);
  task_environment()->FastForwardBy(base::Milliseconds(20));
  EXPECT_TRUE(
      test_tray_background_view()->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(test_tray_background_view()->GetVisible());

  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::ACTIVE);
  task_environment()->FastForwardBy(base::Milliseconds(20));
  EXPECT_FALSE(
      test_tray_background_view()->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(test_tray_background_view()->GetVisible());

  // Not showing animation when switching users.
  GetSessionControllerClient()->AddUserSession("a");
  test_tray_background_view()->SetVisiblePreferred(false);
  test_tray_background_view()->SetVisiblePreferred(true);
  EXPECT_TRUE(
      test_tray_background_view()->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(test_tray_background_view()->GetVisible());

  // Simulates user switching by changing the order of session_ids.
  Shell::Get()->session_controller()->SetUserSessionOrder({2u, 1u});
  task_environment()->FastForwardBy(base::Milliseconds(20));
  EXPECT_FALSE(
      test_tray_background_view()->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(test_tray_background_view()->GetVisible());
}

// Tests that persistent `TrayBackgroundView` bubbles stay shown across lock
// state changes.
TEST_F(TrayBackgroundViewTest, PersistentBubbleShownAcrossLockStateChanges) {
  // Show the bubble.
  persistent_bubble_test_tray_background_view()->SetVisiblePreferred(true);
  persistent_bubble_test_tray_background_view()->ShowBubble();
  ASSERT_TRUE(persistent_bubble_test_tray_background_view()
                  ->bubble()
                  ->bubble_view()
                  ->IsDrawn());

  // Go to the lock screen.
  GetSessionControllerClient()->LockScreen();

  // Verify that the bubble is still shown.
  EXPECT_TRUE(persistent_bubble_test_tray_background_view()
                  ->bubble()
                  ->bubble_view()
                  ->IsDrawn());

  // Unlock the device.
  GetSessionControllerClient()->UnlockScreen();

  // Verify that the bubble is still shown.
  EXPECT_TRUE(persistent_bubble_test_tray_background_view()
                  ->bubble()
                  ->bubble_view()
                  ->IsDrawn());
}

// Tests that non-persistent `TrayBackgroundView` bubbles are closed when the
// lock state changes.
TEST_F(TrayBackgroundViewTest, NonPersistentBubbleClosedWhenLockStateChanges) {
  // Show the bubble.
  test_tray_background_view()->SetVisiblePreferred(true);
  test_tray_background_view()->ShowBubble();
  ASSERT_TRUE(test_tray_background_view()->bubble()->bubble_view()->IsDrawn());

  // Go to the lock screen.
  GetSessionControllerClient()->LockScreen();

  // Verify that the bubble is closed.
  EXPECT_FALSE(test_tray_background_view()->bubble());

  // Open the bubble on the lock screen.
  test_tray_background_view()->ShowBubble();
  ASSERT_TRUE(test_tray_background_view()->bubble()->bubble_view()->IsDrawn());

  // Unlock the device.
  GetSessionControllerClient()->UnlockScreen();

  // Verify that the bubble is closed.
  EXPECT_FALSE(persistent_bubble_test_tray_background_view()->bubble());
}

TEST_F(TrayBackgroundViewTest, SecondaryDisplay) {
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Add secondary screen.
  UpdateDisplay("800x600,800x600");
  GetPrimaryDictationTray()->layer()->GetAnimator()->AddObserver(this);
  GetSecondaryDictationTray()->layer()->GetAnimator()->AddObserver(this);

  // Switch the primary and secondary screen. This should not cause additional
  // TrayBackgroundView animations to occur.
  SwapPrimaryDisplay();
  task_environment()->RunUntilIdle();
  EXPECT_TRUE(GetPrimaryDictationTray()->GetVisible());
  EXPECT_TRUE(GetSecondaryDictationTray()->GetVisible());
  EXPECT_EQ(num_animations_scheduled(), 0);

  // Enable the animation after showing up on the secondary screen.
  task_environment()->RunUntilIdle();
  ui::LayerAnimationStoppedWaiter animation_waiter;
  GetPrimaryDictationTray()->SetVisiblePreferred(false);
  EXPECT_TRUE(
      GetPrimaryDictationTray()->layer()->GetAnimator()->is_animating());
  animation_waiter.Wait(GetPrimaryDictationTray()->layer());

  GetPrimaryDictationTray()->SetVisiblePreferred(true);
  EXPECT_TRUE(
      GetPrimaryDictationTray()->layer()->GetAnimator()->is_animating());
  animation_waiter.Wait(GetPrimaryDictationTray()->layer());

  GetSecondaryDictationTray()->SetVisiblePreferred(false);
  EXPECT_TRUE(
      GetSecondaryDictationTray()->layer()->GetAnimator()->is_animating());
  animation_waiter.Wait(GetSecondaryDictationTray()->layer());

  GetSecondaryDictationTray()->SetVisiblePreferred(true);
  EXPECT_TRUE(
      GetSecondaryDictationTray()->layer()->GetAnimator()->is_animating());
  animation_waiter.Wait(GetSecondaryDictationTray()->layer());

  EXPECT_TRUE(GetPrimaryDictationTray()->GetVisible());
  EXPECT_TRUE(GetSecondaryDictationTray()->GetVisible());

  // Remove the secondary screen. This should not cause additional
  // TrayBackgroundView animations to occur.
  int num_animations_scheduled_before = num_animations_scheduled();
  UpdateDisplay("800x600");
  task_environment()->RunUntilIdle();
  EXPECT_EQ(num_animations_scheduled(), num_animations_scheduled_before);
  EXPECT_TRUE(GetPrimaryDictationTray()->GetVisible());
}

// Tests that a context menu only appears when a tray provides a menu model and
// the tray should show a context menu.
TEST_F(TrayBackgroundViewTest, ContextMenu) {
  test_tray_background_view()->SetVisiblePreferred(true);
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(
      test_tray_background_view()->GetBoundsInScreen().CenterPoint());
  EXPECT_FALSE(test_tray_background_view()->IsShowingMenu());

  // The tray should not display a context menu, and no model is provided, so
  // no menu should appear.
  generator->ClickRightButton();
  EXPECT_FALSE(test_tray_background_view()->IsShowingMenu());

  // The tray should display a context menu, but no model is provided, so no
  // menu should appear.
  test_tray_background_view()->SetShouldShowMenu(true);
  generator->ClickRightButton();
  EXPECT_FALSE(test_tray_background_view()->IsShowingMenu());

  // The tray should display a context menu, and a model is provided, so a menu
  // should appear.
  test_tray_background_view()->set_provide_menu_model(true);
  generator->ClickRightButton();
  EXPECT_TRUE(test_tray_background_view()->IsShowingMenu());

  // The tray should not display a context menu, so even though a model is
  // provided, no menu should appear.
  test_tray_background_view()->SetShouldShowMenu(false);
  generator->ClickRightButton();
  EXPECT_FALSE(test_tray_background_view()->IsShowingMenu());
}

// Tests the auto-hide shelf status when opening and closing a context menu.
TEST_F(TrayBackgroundViewTest, AutoHideShelfWithContextMenu) {
  // Create one window, or the shelf won't auto-hide.
  std::unique_ptr<views::Widget> unused =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);

  // Set the shelf to auto-hide.
  Shelf* shelf = test_tray_background_view()->shelf();
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
  test_tray_background_view()->SetVisiblePreferred(true);
  test_tray_background_view()->set_provide_menu_model(true);
  test_tray_background_view()->SetShouldShowMenu(true);
  generator->MoveMouseTo(
      test_tray_background_view()->GetBoundsInScreen().CenterPoint());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
  EXPECT_FALSE(test_tray_background_view()->IsShowingMenu());
  generator->ClickRightButton();
  EXPECT_TRUE(test_tray_background_view()->IsShowingMenu());

  // Close the context menu with the mouse over the shelf. The shelf should
  // remain shown.
  generator->ClickRightButton();
  ASSERT_FALSE(TriggerAutoHideTimeout(layout_manager));
  EXPECT_FALSE(test_tray_background_view()->IsShowingMenu());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

  // Reopen tray context menu.
  generator->ClickRightButton();
  EXPECT_TRUE(test_tray_background_view()->IsShowingMenu());

  // Mouse away from the shelf with the context menu still showing. The shelf
  // should remain shown.
  generator->MoveMouseTo(0, 0);
  ASSERT_TRUE(TriggerAutoHideTimeout(layout_manager));
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

  // Close the context menu with the mouse away from the shelf. The shelf
  // should hide.
  generator->ClickRightButton();
  ASSERT_FALSE(TriggerAutoHideTimeout(layout_manager));
  EXPECT_FALSE(test_tray_background_view()->IsShowingMenu());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());
}

// Tests that `TrayBackgroundView::SetPressedCallback()` overrides
// TrayBackgroundView's default press behavior.
TEST_F(TrayBackgroundViewTest, PressedCallbackSet) {
  test_tray_background_view()->SetVisible(true);
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(
      test_tray_background_view()->GetBoundsInScreen().CenterPoint());

  // Set the callback. Pressing the  `TestTrayBackgroundView` should execute the
  // callback instead of `TrayBackgroundView::ShowBubble()`.
  bool pressed = false;
  test_tray_background_view()->SetCallback(base::BindRepeating(
      [](bool& pressed, const ui::Event& event) { pressed = true; },
      std::ref(pressed)));
  generator->ClickLeftButton();

  EXPECT_TRUE(pressed);
  EXPECT_FALSE(test_tray_background_view()->show_bubble_called());
}

// Tests that histograms are still recorded when the TrayBackgroundView has
// custom button press behavior.
TEST_F(TrayBackgroundViewTest, HistogramRecordedPressedCallbackSet) {
  auto histogram_tester = std::make_unique<base::HistogramTester>();
  histogram_tester->ExpectTotalCount(
      "Ash.StatusArea.TrayBackgroundView.Pressed",
      /*count=*/0);

  test_tray_background_view()->SetVisible(true);
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(
      test_tray_background_view()->GetBoundsInScreen().CenterPoint());

  // Set the callback. This should not effect histogram recording.
  test_tray_background_view()->SetCallback(views::Button::PressedCallback());
  generator->ClickLeftButton();

  histogram_tester->ExpectTotalCount(
      "Ash.StatusArea.TrayBackgroundView.Pressed",
      /*count=*/1);
}

// Tests that the `TrayBubbleWrapper` owned by the `TrayBackgroundView` is
// cleaned up and the active state of the `TrayBackgroundView` is updated if the
// bubble widget is destroyed independently (Real life examples would be
// clicking outside a bubble or hitting the escape key).
TEST_F(TrayBackgroundViewTest, CleanUpOnIndependentBubbleDestruction) {
  test_tray_background_view()->SetVisiblePreferred(true);
  test_tray_background_view()->ShowBubble();

  EXPECT_TRUE(test_tray_background_view()->is_active());
  EXPECT_TRUE(
      test_tray_background_view()->bubble()->GetBubbleWidget()->IsVisible());

  // Destroying the bubble's widget independently of the `TrayBackgroundView`
  // should properly clean up `bubble()` in `TrayBackgroundView`.
  test_tray_background_view()->bubble()->GetBubbleWidget()->CloseNow();

  EXPECT_FALSE(test_tray_background_view()->is_active());
  ASSERT_FALSE(test_tray_background_view()->bubble());
}

// Tests that the tray bubble is repositioned when the device scale factor
// changes.
TEST_F(TrayBackgroundViewTest, UpdateScaleFactor) {
  test_tray_background_view()->ShowBubble();
  EXPECT_TRUE(test_tray_background_view()->bubble());

  // Since no zoom factor has been set on the display, it should be 1.
  const display::ManagedDisplayInfo& display =
      display_manager()->GetDisplayInfo(GetPrimaryDisplay().id());
  EXPECT_EQ(
      display_manager()->GetDisplayForId(display.id()).device_scale_factor(),
      1.f);

  const gfx::Rect& tablet_mode_bounds =
      test_tray_background_view()->bubble_view()->GetBoundsInScreen();

  // Set the device scale factor to 2.
  constexpr float zoom_factor = 2.0f;
  display_manager()->UpdateZoomFactor(display.id(), zoom_factor);
  EXPECT_EQ(
      display_manager()->GetDisplayForId(display.id()).device_scale_factor(),
      zoom_factor);

  const gfx::Rect& clamshell_mode_bounds =
      test_tray_background_view()->bubble_view()->GetBoundsInScreen();

  // The bubble should have been repositioned due to the scale factor changing,
  // so the bounds should be different than when there was a scale factor of 1.
  EXPECT_NE(tablet_mode_bounds, clamshell_mode_bounds);

  // Close and reopen the bubble, and verify that the bounds when exiting out of
  // tablet mode is the same as the initial bounds calculation when showing the
  // bubble.
  test_tray_background_view()->CloseBubble();
  test_tray_background_view()->ShowBubble();
  EXPECT_EQ(clamshell_mode_bounds,
            test_tray_background_view()->bubble_view()->GetBoundsInScreen());
}

// Tests that the tray bubble is positioned correctly when the device switches
// from tablet to clamshell mode.
TEST_F(TrayBackgroundViewTest, TabletModeTransitionBase) {
  TabletModeController* tablet_mode_controller =
      Shell::Get()->tablet_mode_controller();

  // With no windows open, switching into tablet mode should show the home
  // launcher with the hotseat extended.
  tablet_mode_controller->SetEnabledForTest(true);
  EXPECT_EQ(HotseatState::kShownHomeLauncher,
            GetPrimaryShelf()->hotseat_widget()->state());

  // Show the bubble.
  test_tray_background_view()->ShowBubble();
  EXPECT_TRUE(test_tray_background_view()->bubble());
  const gfx::Insets& tablet_mode_insets =
      test_tray_background_view()->bubble_view()->GetBorderInsets();

  // Switch back to clamshell mode.
  tablet_mode_controller->SetEnabledForTest(false);
  const gfx::Insets& clamshell_mode_insets =
      test_tray_background_view()->bubble_view()->GetBorderInsets();

  // The bubble should have been repositioned due to switching out of tablet
  // mode, so the bottom inset should be updated with the inset hotseat
  // compensation calculated from `GetBubbleInsetHotseatCompensation` removed.
  EXPECT_LT(clamshell_mode_insets.bottom(), tablet_mode_insets.bottom());

  // Close and reopen the bubble, and verify that the insets when exiting out of
  // tablet mode is the same as the initial inset calculation when showing the
  // bubble.
  test_tray_background_view()->CloseBubble();
  test_tray_background_view()->ShowBubble();
  EXPECT_EQ(clamshell_mode_insets,
            test_tray_background_view()->bubble_view()->GetBorderInsets());
}

// Tests the the tray bubble positions itself correctly even when the shelf
// alignments change when transitioning out of tablet mode. This happens when
// the shelf is aligned to either the left or the right.
TEST_F(TrayBackgroundViewTest, TabletModeTransitionForAlignments) {
  Shelf* shelf = GetPrimaryShelf();
  TabletModeController* tablet_mode_controller =
      Shell::Get()->tablet_mode_controller();

  const struct {
    ShelfAlignment alignment;
  } kTestCases[] = {{ShelfAlignment::kLeft}, {ShelfAlignment::kRight}};
  for (auto& test : kTestCases) {
    shelf->SetAlignment(test.alignment);
    tablet_mode_controller->SetEnabledForTest(true);

    // Show the bubble.
    test_tray_background_view()->ShowBubble();
    EXPECT_TRUE(test_tray_background_view()->bubble());
    const gfx::Rect& tablet_mode_bounds =
        test_tray_background_view()->bubble_view()->GetBoundsInScreen();

    // Switch back to clamshell mode.
    tablet_mode_controller->SetEnabledForTest(false);
    const gfx::Rect& clamshell_mode_bounds =
        test_tray_background_view()->bubble_view()->GetBoundsInScreen();

    // The bubble should have been repositioned due to switching out of tablet
    // mode.
    EXPECT_NE(tablet_mode_bounds, clamshell_mode_bounds);

    // Close and reopen the bubble, and verify that the bounds when exiting out
    // of tablet mode is the same as the initial bounds calculation when showing
    // the bubble.
    test_tray_background_view()->CloseBubble();
    test_tray_background_view()->ShowBubble();
    EXPECT_EQ(clamshell_mode_bounds,
              test_tray_background_view()->bubble_view()->GetBoundsInScreen());
  }
}

TEST_F(TrayBackgroundViewTest, TrayBubbleViewAccessibleProperties) {
  test_tray_background_view()->ShowBubble();
  TrayBubbleView* bubble_view = test_tray_background_view()->bubble_view();
  ASSERT_TRUE(bubble_view->CanActivate());
  bubble_view->InitializeAndShowBubble();
  ui::AXNodeData data;

  bubble_view->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_FALSE(data.HasState(ax::mojom::State::kIgnored));
  EXPECT_EQ(ax::mojom::Role::kWindow, data.role);

  // Test that bubble view is hidden to a11y when `can_activate_` is false.
  bubble_view->SetCanActivate(false);
  // `can_activate_` value is set before showing the bubble.
  bubble_view->InitializeAndShowBubble();
  data = ui::AXNodeData();
  bubble_view->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_TRUE(data.HasState(ax::mojom::State::kIgnored));

  bubble_view->SetCanActivate(true);
  bubble_view->InitializeAndShowBubble();
  data = ui::AXNodeData();
  bubble_view->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_FALSE(data.HasState(ax::mojom::State::kIgnored));

  // Test that bubble view is hidden to a11y when `delegate_` is null.
  bubble_view->ResetDelegate();
  data = ui::AXNodeData();
  bubble_view->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_TRUE(data.HasState(ax::mojom::State::kIgnored));
}

}  // namespace ash
