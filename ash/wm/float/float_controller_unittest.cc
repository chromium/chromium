// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/float/float_controller.h"

#include "ash/accelerators/accelerator_controller_impl.h"
#include "ash/accessibility/magnifier/docked_magnifier_controller.h"
#include "ash/app_list/app_list_controller_impl.h"
#include "ash/constants/ash_features.h"
#include "ash/display/screen_orientation_controller_test_api.h"
#include "ash/frame/non_client_frame_view_ash.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/cursor_manager_chromeos.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desk_mini_view.h"
#include "ash/wm/desks/desks_test_api.h"
#include "ash/wm/desks/desks_test_util.h"
#include "ash/wm/desks/overview_desk_bar_view.h"
#include "ash/wm/float/float_test_api.h"
#include "ash/wm/float/tablet_mode_float_window_resizer.h"
#include "ash/wm/float/tablet_mode_tuck_education.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_item.h"
#include "ash/wm/overview/overview_test_util.h"
#include "ash/wm/scoped_window_tucker.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/splitview/split_view_metrics_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/test/test_non_client_frame_view_ash.h"
#include "ash/wm/window_positioning_utils.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ash/wm/work_area_insets.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/scoped_observation.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/simple_test_clock.h"
#include "base/time/clock.h"
#include "chromeos/ui/base/app_types.h"
#include "chromeos/ui/base/window_properties.h"
#include "chromeos/ui/base/window_state_type.h"
#include "chromeos/ui/frame/caption_buttons/snap_controller.h"
#include "chromeos/ui/frame/header_view.h"
#include "chromeos/ui/frame/immersive/immersive_fullscreen_controller.h"
#include "chromeos/ui/wm/constants.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window_observer.h"
#include "ui/base/hit_test.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/display/display_switches.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/test/test_widget_observer.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/wm/core/window_util.h"

namespace ash {

namespace {

// Gets the header view for `window` so it can be dragged.
chromeos::HeaderView* GetHeaderView(aura::Window* window) {
  // Exiting immersive mode because of float does not seem to trigger a layout
  // like it does in production code. Here we force a layout, otherwise the
  // client view will remain the size of the widget, and dragging it will give
  // us HTCLIENT.
  auto* frame = NonClientFrameViewAsh::Get(window);
  DCHECK(frame);
  views::test::RunScheduledLayout(frame);
  return frame->GetHeaderView();
}

// Checks if `window` is being visibly animating. That means windows that are
// animated with tween zero are excluded because those jump to the target at the
// end of the animation.
bool IsVisiblyAnimating(aura::Window* window) {
  DCHECK(window);
  ui::LayerAnimator* animator = window->layer()->GetAnimator();
  return animator->is_animating() && animator->tween_type() != gfx::Tween::ZERO;
}

// Counts the amount of times a window's layer has be recreated. Used to ensure
// we are not using the cross-fade animation while dragging.
class WindowLayerRecreatedCounter : public aura::WindowObserver {
 public:
  explicit WindowLayerRecreatedCounter(aura::Window* window) {
    window_observation_.Observe(window);
  }
  WindowLayerRecreatedCounter(const WindowLayerRecreatedCounter&) = delete;
  WindowLayerRecreatedCounter& operator=(const WindowLayerRecreatedCounter&) =
      delete;
  ~WindowLayerRecreatedCounter() override = default;

  int recreated_count() const { return recreated_count_; }

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override {
    window_observation_.Reset();
  }
  void OnWindowLayerRecreated(aura::Window* window) override {
    ++recreated_count_;
  }

 private:
  int recreated_count_ = 0;

  base::ScopedObservation<aura::Window, aura::WindowObserver>
      window_observation_{this};
};

}  // namespace

class WindowFloatTest : public AshTestBase {
 public:
  WindowFloatTest() = default;
  explicit WindowFloatTest(base::test::TaskEnvironment::TimeSource time)
      : AshTestBase(time) {}
  WindowFloatTest(const WindowFloatTest&) = delete;
  WindowFloatTest& operator=(const WindowFloatTest&) = delete;
  ~WindowFloatTest() override = default;

  // Creates a floated application window.
  std::unique_ptr<aura::Window> CreateFloatedWindow() {
    std::unique_ptr<aura::Window> floated_window = CreateAppWindow();
    PressAndReleaseKey(ui::VKEY_F, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN);
    CHECK(WindowState::Get(floated_window.get())->IsFloated());
    return floated_window;
  }
};

// Test float/unfloat window.
TEST_F(WindowFloatTest, WindowFloatingSwitch) {
  // Activate `window_1` and perform floating.
  std::unique_ptr<aura::Window> window_1(CreateFloatedWindow());

  // Activate `window_2` and perform floating.
  std::unique_ptr<aura::Window> window_2(CreateFloatedWindow());

  // Only one floated window is allowed so when a different window is floated,
  // the previously floated window will be unfloated.
  EXPECT_FALSE(WindowState::Get(window_1.get())->IsFloated());

  // When try to float the already floated `window_2`, it will unfloat this
  // window.
  wm::ActivateWindow(window_2.get());
  PressAndReleaseKey(ui::VKEY_F, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN);
  EXPECT_FALSE(WindowState::Get(window_2.get())->IsFloated());
}

// Tests that double clicking on the caption maximizes a floated window.
// Regression test for https://crbug.com/1357049.
TEST_F(WindowFloatTest, DoubleClickOnCaption) {
  std::unique_ptr<aura::Window> window = CreateFloatedWindow();

  // Double click on the caption. The window should be maximized now.
  chromeos::HeaderView* header_view = GetHeaderView(window.get());
  auto* event_generator = GetEventGenerator();
  event_generator->set_current_screen_location(
      header_view->GetBoundsInScreen().CenterPoint());
  event_generator->DoubleClickLeftButton();

  EXPECT_TRUE(WindowState::Get(window.get())->IsMaximized());
}

// Tests that a floated window animates to and from overview.
TEST_F(WindowFloatTest, FloatWindowAnimatesInOverview) {
  std::unique_ptr<aura::Window> floated_window = CreateFloatedWindow();
  std::unique_ptr<aura::Window> maximized_window = CreateAppWindow();

  const WMEvent maximize_event(WM_EVENT_MAXIMIZE);
  WindowState::Get(maximized_window.get())->OnWMEvent(&maximize_event);

  // Activate `maximized_window`. If the other window was not floated, then it
  // would be hidden behind the maximized window and not animate.
  wm::ActivateWindow(maximized_window.get());

  // Enter overview, both windows should animate when entering overview, since
  // both are visible to the user.
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  ToggleOverview();
  EXPECT_TRUE(floated_window->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(maximized_window->layer()->GetAnimator()->is_animating());

  // Both windows should animate when exiting overview as well.
  WaitForOverviewEnterAnimation();
  ToggleOverview();
  EXPECT_TRUE(floated_window->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(maximized_window->layer()->GetAnimator()->is_animating());
}

// Tests that a floated window animates when a state change causes it to
// unfloat. Regression test for b/252505434.
TEST_F(WindowFloatTest, FloatToMaximizeWindowAnimates) {
  std::unique_ptr<aura::Window> window = CreateFloatedWindow();

  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  const WMEvent maximize_event(WM_EVENT_MAXIMIZE);
  WindowState::Get(window.get())->OnWMEvent(&maximize_event);
  // `WindowState::SetBoundsDirectCrossFade` still starts an animation if the
  // source and destination bounds are the same. Therefore, it is not enough to
  // just check if its animating.
  EXPECT_TRUE(window->layer()->GetAnimator()->is_animating());
  EXPECT_NE(window->layer()->transform(),
            window->layer()->GetTargetTransform());
}

// Test when float a window in clamshell mode, window will change to default
// float bounds in certain conditions.
TEST_F(WindowFloatTest, WindowFloatingResize) {
  std::unique_ptr<aura::Window> window = CreateAppWindow(gfx::Rect(200, 200));

  // Float maximized window.
  auto* window_state = WindowState::Get(window.get());
  window_state->Maximize();
  PressAndReleaseKey(ui::VKEY_F, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN);
  EXPECT_TRUE(window_state->IsFloated());
  gfx::Rect default_float_bounds =
      FloatController::GetFloatWindowClamshellBounds(
          window.get(), chromeos::FloatStartLocation::kBottomRight);
  EXPECT_EQ(default_float_bounds, window->GetBoundsInScreen());
  // Unfloat.
  PressAndReleaseKey(ui::VKEY_F, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN);
  EXPECT_FALSE(window_state->IsFloated());
  EXPECT_TRUE(window_state->IsMaximized());

  // Float full screen window.
  const WMEvent toggle_fullscreen_event(WM_EVENT_TOGGLE_FULLSCREEN);
  window_state->OnWMEvent(&toggle_fullscreen_event);
  EXPECT_TRUE(window_state->IsFullscreen());
  PressAndReleaseKey(ui::VKEY_F, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN);
  EXPECT_TRUE(window_state->IsFloated());
  default_float_bounds = FloatController::GetFloatWindowClamshellBounds(
      window.get(), chromeos::FloatStartLocation::kBottomRight);
  EXPECT_EQ(default_float_bounds, window->GetBoundsInScreen());
  // Unfloat.
  PressAndReleaseKey(ui::VKEY_F, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN);
  EXPECT_FALSE(window_state->IsFloated());
  // Unfloat a previously full screened window will restore to previous window
  // state.
  EXPECT_TRUE(window_state->IsMaximized());

  // Float window again.
  PressAndReleaseKey(ui::VKEY_F, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN);
  // Minimize floated window.
  // Minimized window can't be floated, but when a floated window enter/exit
  // minimized state, it remains floated.
  EXPECT_TRUE(window_state->IsFloated());
  const gfx::Rect curr_bounds = window->GetBoundsInScreen();
  window_state->Minimize();
  window_state->Restore();
  EXPECT_EQ(curr_bounds, window->GetBoundsInScreen());
  EXPECT_TRUE(window_state->IsFloated());

  // Float Snapped window.
  // Create a snap enabled window.
  auto window2 = CreateAppWindow(default_float_bounds);
  auto* window_state2 = WindowState::Get(window2.get());
  AcceleratorControllerImpl* acc_controller =
      Shell::Get()->accelerator_controller();

  // Snap Left.
  acc_controller->PerformActionIfEnabled(
      AcceleratorAction::kWindowCycleSnapLeft, {});
  ASSERT_EQ(chromeos::WindowStateType::kPrimarySnapped,
            window_state2->GetStateType());
  PressAndReleaseKey(ui::VKEY_F, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN);
  EXPECT_EQ(default_float_bounds, window2->GetBoundsInScreen());

  // Unfloat.
  PressAndReleaseKey(ui::VKEY_F, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN);
  // Window back to snapped state.
  ASSERT_EQ(chromeos::WindowStateType::kPrimarySnapped,
            window_state2->GetStateType());

  // Snap Right.
  acc_controller->PerformActionIfEnabled(
      AcceleratorAction::kWindowCycleSnapRight, {});
  ASSERT_EQ(chromeos::WindowStateType::kSecondarySnapped,
            WindowState::Get(window2.get())->GetStateType());
  PressAndReleaseKey(ui::VKEY_F, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN);
  EXPECT_EQ(default_float_bounds, window2->GetBoundsInScreen());

  // Unfloat.
  PressAndReleaseKey(ui::VKEY_F, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN);
  // Window back to snapped state.
  EXPECT_EQ(chromeos::WindowStateType::kSecondarySnapped,
            window_state2->GetStateType());
}

// Tests that manually resized floated bounds (by dragging the caption area) are
// restored across desk switches.
TEST_F(WindowFloatTest, RestoreResizeBounds) {
  auto* desks_controller = DesksController::Get();
  NewDesk();
  ASSERT_EQ(2u, desks_controller->desks().size());

  std::unique_ptr<aura::Window> window = CreateFloatedWindow();

  // Drag the floated window away from its default bounds.
  auto* event_generator = GetEventGenerator();
  chromeos::HeaderView* header_view = GetHeaderView(window.get());
  event_generator->set_current_screen_location(
      header_view->GetBoundsInScreen().CenterPoint());
  event_generator->DragMouseTo(gfx::Point(100, 100));
  const gfx::Rect resize_bounds(window->bounds());

  // Switch to desk 2.
  ActivateDesk(desks_controller->desks()[1].get());
  ASSERT_TRUE(WindowState::Get(window.get())->IsFloated());
  ASSERT_FALSE(window->IsVisible());

  // Switch back to desk 1. Test that the new floated bounds are restored.
  ActivateDesk(desks_controller->desks()[0].get());
  ASSERT_TRUE(WindowState::Get(window.get())->IsFloated());
  ASSERT_TRUE(window->IsVisible());
  EXPECT_EQ(resize_bounds, window->bounds());

  // Test that minimizing and unminimizing also restores resized bounds.
  auto* window_state = WindowState::Get(window.get());
  window_state->Minimize();
  window_state->Restore();
  EXPECT_EQ(resize_bounds, window->bounds());
}

// Test that the float acclerator does not work on a non-floatable window.
TEST_F(WindowFloatTest, CantFloatAccelerator) {
  // Test window is NON_APP by default, which cannot be floated.
  auto window = CreateTestWindow();
  EXPECT_EQ(window.get(), window_util::GetActiveWindow());
  PressAndReleaseKey(ui::VKEY_F, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN);
  EXPECT_FALSE(WindowState::Get(window.get())->IsFloated());
}

// Tests that after we drag a floated window to another display and then
// maximize, the window is on the correct display. Regression test for
// https://crbug.com/1360551.
TEST_F(WindowFloatTest, DragToOtherDisplayThenMaximize) {
  UpdateDisplay("1200x800,1201+0-1200x800");

  std::unique_ptr<aura::Window> window = CreateFloatedWindow();
  ASSERT_EQ(Shell::GetAllRootWindows()[0], window->GetRootWindow());

  // Drag the window to the secondary display. Note that the event generator
  // does not update the display associated with the cursor, so we have to
  // manually do it here.
  chromeos::HeaderView* header_view = GetHeaderView(window.get());
  auto* event_generator = GetEventGenerator();
  event_generator->set_current_screen_location(
      header_view->GetBoundsInScreen().CenterPoint());
  const gfx::Point point(1600, 400);
  Shell::Get()->cursor_manager()->SetDisplay(
      display::Screen::GetScreen()->GetDisplayNearestPoint(point));
  event_generator->DragMouseTo(point);

  // Tests that the floated window is on the secondary display and remained
  // floated.
  ASSERT_EQ(Shell::GetAllRootWindows()[1], window->GetRootWindow());
  WindowState* window_state = WindowState::Get(window.get());
  ASSERT_TRUE(window_state->IsFloated());

  // Maximize the window. Test that it stays on the secondary display.
  const WMEvent maximize(WM_EVENT_MAXIMIZE);
  window_state->OnWMEvent(&maximize);
  EXPECT_EQ(Shell::GetAllRootWindows()[1], window->GetRootWindow());
}

// Tests that windows that are floated on non-primary displays are onscreen.
// Regression test for b/261860554.
TEST_F(WindowFloatTest, FloatOnOtherDisplay) {
  UpdateDisplay("1200x800,1201+0-1200x800");

  // Create a window on the secondary display.
  std::unique_ptr<aura::Window> window =
      CreateAppWindow(gfx::Rect(1200, 0, 300, 300));
  ASSERT_EQ(Shell::GetAllRootWindows()[1], window->GetRootWindow());

  // After floating, the bounds of `window` should be full contained by the
  // secondary display bounds.
  PressAndReleaseKey(ui::VKEY_F, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN);
  EXPECT_TRUE(
      gfx::Rect(1200, 0, 1200, 800).Contains(window->GetBoundsInScreen()));
}

// Tests that floated windows that are moved to an external display are still
// visible and the same size. Regression test for http://b/286864430 and
// http://b/297218727.
TEST_F(WindowFloatTest, MoveFloatedWindowToOtherDisplay) {
  // On two displays of the same size, this was never an issue. The issue
  // happened if the destination display was narrower than the source display.
  UpdateDisplay("1200x800,1201+0-600x800");

  // Create a floated window on the primary display. Upon floating, it will
  // automatically reposition to the bottom right corner. For this test, we want
  // to ensure the normal state size is different from the floated size; we do
  // this by initializing the window with a large size.
  auto window = CreateAppWindow(gfx::Rect(1100, 700));
  PressAndReleaseKey(ui::VKEY_F, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN);
  CHECK(WindowState::Get(window.get())->IsFloated());
  CHECK_NE(gfx::Size(1100, 700), window->bounds().size());

  const gfx::Size primary_display_size = window->bounds().size();

  // After moving the window to the secondary display, the bounds of `window`
  // should be partially visible and remain the same size.
  PressAndReleaseKey(ui::VKEY_M, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN);
  ASSERT_EQ(Shell::GetAllRootWindows()[1], window->GetRootWindow());
  EXPECT_TRUE(Shell::GetAllRootWindows()[1]->GetBoundsInScreen().Intersects(
      window->GetBoundsInScreen()));
  EXPECT_EQ(primary_display_size, window->bounds().size());

  // After moving the window back to the primary display, the bounds of `window`
  // should be partially visible and remain the same size.
  PressAndReleaseKey(ui::VKEY_M, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN);
  ASSERT_EQ(Shell::GetAllRootWindows()[0], window->GetRootWindow());
  EXPECT_TRUE(Shell::GetAllRootWindows()[0]->GetBoundsInScreen().Intersects(
      window->GetBoundsInScreen()));
  EXPECT_EQ(primary_display_size, window->bounds().size());
}

TEST_F(WindowFloatTest, FloatWindowBoundsWithZoomDisplay) {
  UpdateDisplay("1600x1000");

  // Create a floated window and position it on the top-right edge of the
  // display.
  std::unique_ptr<aura::Window> window =
      CreateAppWindow(gfx::Rect(1200, 0, 400, 300));
  PressAndReleaseKey(ui::VKEY_F, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN);

  // Use the accelerator to zoom the display up (ctrl + shift + "+") a couple
  // times. The floated window bounds should still be within the work area
  // bounds.
  PressAndReleaseKey(ui::VKEY_OEM_PLUS,
                     ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN);
  PressAndReleaseKey(ui::VKEY_OEM_PLUS,
                     ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN);
  PressAndReleaseKey(ui::VKEY_OEM_PLUS,
                     ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN);

  EXPECT_TRUE(WorkAreaInsets::ForWindow(window.get())
                  ->user_work_area_bounds()
                  .Contains(window->GetBoundsInScreen()));
}

TEST_F(WindowFloatTest, FloatWindowBoundsWithShelfChange) {
  UpdateDisplay("1600x1000");

  // This test makes some assumptions that the shelf starts bottom aligned.
  ASSERT_EQ(ShelfAlignment::kBottom, GetPrimaryShelf()->alignment());

  // Create a floated window and position so it is semi offscreen.
  std::unique_ptr<aura::Window> window = CreateFloatedWindow();
  const gfx::Rect ideal_bounds(1400, 0, 400, 300);
  const SetBoundsWMEvent set_bounds_event(ideal_bounds);
  WindowState::Get(window.get())->OnWMEvent(&set_bounds_event);

  // Changing the shelf alignment should not alter the floated window bounds.
  GetPrimaryShelf()->SetAlignment(ShelfAlignment::kLeft);
  EXPECT_EQ(ideal_bounds, window->GetBoundsInScreen());
}

// Test float window per desk logic.
TEST_F(WindowFloatTest, OneFloatWindowPerDeskLogic) {
  // Test one float window per desk is allowed.
  auto* desks_controller = DesksController::Get();
  NewDesk();
  ASSERT_EQ(2u, desks_controller->desks().size());

  // Test creating floating window on different desk.
  // Float `window_1`.
  std::unique_ptr<aura::Window> window_1(CreateFloatedWindow());
  // Move to desk 2.
  auto* desk_2 = desks_controller->desks()[1].get();
  ActivateDesk(desk_2);
  // `window_1` should not be visible since it's a different desk.
  EXPECT_FALSE(window_1->IsVisible());
  std::unique_ptr<aura::Window> window_2(CreateFloatedWindow());
  // Both `window_1` and `window_2` should be floated since we allow one float
  // window per desk.
  EXPECT_TRUE(desk_2->is_active());
  EXPECT_TRUE(WindowState::Get(window_1.get())->IsFloated());
  EXPECT_TRUE(WindowState::Get(window_2.get())->IsFloated());
}

// Tests that `desks_util::BelongsToActiveDesk()` works as intended.
TEST_F(WindowFloatTest, BelongsToActiveDesk) {
  NewDesk();

  std::unique_ptr<aura::Window> window = CreateFloatedWindow();
  EXPECT_TRUE(desks_util::BelongsToActiveDesk(window.get()));

  ActivateDesk(DesksController::Get()->desks()[1].get());
  EXPECT_FALSE(desks_util::BelongsToActiveDesk(window.get()));
}

// Test Desk removal for floating window.
TEST_F(WindowFloatTest, FloatWindowWithDeskRemoval) {
  auto* desks_controller = DesksController::Get();
  NewDesk();
  auto* desk_1 = desks_controller->desks()[0].get();
  auto* desk_2 = desks_controller->desks()[1].get();
  std::unique_ptr<aura::Window> window_1(CreateFloatedWindow());
  // Move to `desk 2`.
  ActivateDesk(desk_2);
  // Float `window_2` at `desk_2`.
  std::unique_ptr<aura::Window> window_2(CreateFloatedWindow());

  // Verify `window_2` belongs to `desk_2`.
  auto* float_controller = Shell::Get()->float_controller();
  ASSERT_EQ(float_controller->FindDeskOfFloatedWindow(window_2.get()), desk_2);

  // Delete `desk_2` and `window_2` should be un-floated while `window_1` should
  // remain floated. As `window_1` is in the target desk and has higher
  // priority over `window_2` from the removed desk.
  RemoveDesk(desk_2);
  EXPECT_TRUE(WindowState::Get(window_1.get())->IsFloated());
  EXPECT_FALSE(WindowState::Get(window_2.get())->IsFloated());
  EXPECT_TRUE(window_1->IsVisible());
  EXPECT_TRUE(window_2->IsVisible());

  // Recreate `desk_2` without any floated window.
  NewDesk();
  desk_2 = desks_controller->desks()[1].get();
  ActivateDesk(desk_2);
  // Remove `desk_1` will move all windows from `desk_1` to the newly created
  // `desk_2` and the floated window should remain floated.
  RemoveDesk(desk_1);
  EXPECT_TRUE(WindowState::Get(window_1.get())->IsFloated());
}

// Test Close-all desk removal undo.
TEST_F(WindowFloatTest, FloatWindowWithDeskRemovalUndo) {
  // Test close all undo will restore float window.
  auto* desks_controller = DesksController::Get();
  NewDesk();
  auto* desk_2 = desks_controller->desks()[1].get();
  // Move to `desk_2`.
  ActivateDesk(desk_2);
  // Float `window` at `desk_2`.
  std::unique_ptr<aura::Window> window(CreateFloatedWindow());
  // Verify `window` belongs to `desk_2`.
  auto* float_controller = Shell::Get()->float_controller();
  ASSERT_EQ(float_controller->FindDeskOfFloatedWindow(window.get()), desk_2);
  EnterOverview();
  ASSERT_TRUE(OverviewController::Get()->InOverviewSession());
  RemoveDesk(desk_2, DeskCloseType::kCloseAllWindowsAndWait);
  ASSERT_TRUE(desk_2->is_desk_being_removed());
  // During desk removal, float window should be hidden.
  EXPECT_FALSE(window->IsVisible());
  // When `desk_2` is restored the floated window should remain floated and
  // shown.
  views::LabelButton* dismiss_button =
      DesksTestApi::GetCloseAllUndoToastDismissButton();
  LeftClickOn(dismiss_button);
  // Canceling close-all will bring the floated window back to shown.
  EXPECT_TRUE(WindowState::Get(window.get())->IsFloated());
  // Check if `window` still belongs to `desk_2`.
  ASSERT_EQ(float_controller->FindFloatedWindowOfDesk(desk_2), window.get());
  EXPECT_TRUE(window->IsVisible());

  // Commit desk removal should also delete the floated window on that desk.
  auto* window_widget = views::Widget::GetWidgetForNativeView(window.get());
  views::test::TestWidgetObserver observer(window_widget);
  // Release the unique_ptr for `window` here, as in close-all desk,
  // we will delete it below when the desk is removed. but since the unique_ptr
  // tries to delete it again, it will cause a crash in this test.
  window.release();
  RemoveDesk(desk_2, DeskCloseType::kCloseAllWindows);
  ASSERT_EQ(desks_controller->desks().size(), 1u);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(observer.widget_closed());
}

// Test float window is included when building MRU window list.
TEST_F(WindowFloatTest, FloatWindowWithMRUWindowList) {
  auto* desks_controller = DesksController::Get();
  // Float `window_1` at `desk_1`.
  auto* desk_1 = desks_controller->desks()[0].get();
  std::unique_ptr<aura::Window> window_1(CreateFloatedWindow());
  NewDesk();
  auto* desk_2 = desks_controller->desks()[1].get();
  // Move to `desk_2`.
  ActivateDesk(desk_2);
  // Float `window_2` at `desk_2`.
  std::unique_ptr<aura::Window> window_2(CreateFloatedWindow());
  // Verify `window_2` belongs to `desk_2`.
  auto* float_controller = Shell::Get()->float_controller();
  ASSERT_EQ(float_controller->FindDeskOfFloatedWindow(window_2.get()), desk_2);
  // Move to `desk_1`.
  ActivateDesk(desk_1);
  // Calling MruWindowTracker::BuildMruWindowList(kActiveDesk) should return a
  // list that doesn't contain floated windows from inactive desk but contains
  // floated window on active desk.
  EXPECT_FALSE(desk_2->is_active());
  auto active_only_list =
      Shell::Get()->mru_window_tracker()->BuildMruWindowList(kActiveDesk);
  EXPECT_TRUE(base::Contains(active_only_list, window_1.get()));
  EXPECT_FALSE(base::Contains(active_only_list, window_2.get()));
  // Calling MruWindowTracker::BuildMruWindowList(kAllDesks) should return a
  // list that contains all windows
  auto all_desks_mru_list =
      Shell::Get()->mru_window_tracker()->BuildMruWindowList(kAllDesks);
  EXPECT_TRUE(base::Contains(all_desks_mru_list, window_1.get()));
  EXPECT_TRUE(base::Contains(all_desks_mru_list, window_2.get()));
}

// Test moving floating window between desks.
TEST_F(WindowFloatTest, MoveFloatWindowBetweenDesks) {
  auto* desks_controller = DesksController::Get();
  // Float `window_1` at `desk_1`.
  auto* desk_1 = desks_controller->desks()[0].get();
  std::unique_ptr<aura::Window> window_1(CreateFloatedWindow());
  // Verify `window_1` belongs to `desk_1`.
  auto* float_controller = Shell::Get()->float_controller();
  ASSERT_EQ(float_controller->FindDeskOfFloatedWindow(window_1.get()), desk_1);
  NewDesk();
  auto* desk_2 = desks_controller->desks()[1].get();
  // Move to `desk_2`.
  ActivateDesk(desk_2);
  // Float `window_2` at `desk_2`.
  std::unique_ptr<aura::Window> window_2(CreateFloatedWindow());
  // Move back to `desk_1`.
  ActivateDesk(desk_1);
  auto* overview_controller = OverviewController::Get();
  EnterOverview();
  auto* overview_session = overview_controller->overview_session();
  // The window should exist on the grid of the first display.
  auto* overview_item =
      overview_session->GetOverviewItemForWindow(window_1.get());
  auto* grid =
      overview_session->GetGridWithRootWindow(Shell::GetPrimaryRootWindow());
  EXPECT_EQ(1u, grid->GetNumWindows());
  // Get position of `desk_2`'s desk mini view on the secondary display.
  const auto* desks_bar_view = grid->desks_bar_view();
  auto* desk_2_mini_view = desks_bar_view->mini_views()[1].get();
  gfx::Point desk_2_mini_view_center =
      desk_2_mini_view->GetBoundsInScreen().CenterPoint();

  // On overview, drag and drop floated `window_1` to `desk_2`.
  DragItemToPoint(overview_item, desk_2_mini_view_center, GetEventGenerator(),
                  /*by_touch_gestures=*/false,
                  /*drop=*/true);

  // Verify `window_1` belongs to `desk_2`.
  ASSERT_EQ(float_controller->FindDeskOfFloatedWindow(window_1.get()), desk_2);
  // Verify `window_2` is unfloated.
  ASSERT_FALSE(WindowState::Get(window_2.get())->IsFloated());
}

// Test drag floating window between desks on different displays.
TEST_F(WindowFloatTest, MoveFloatWindowBetweenDesksOnDifferentDisplay) {
  UpdateDisplay("1000x400,1000+0-1000x400");
  auto* desks_controller = DesksController::Get();
  // Float `window_1` at `desk_1`.
  auto* desk_1 = desks_controller->desks()[0].get();
  std::unique_ptr<aura::Window> window_1(CreateFloatedWindow());
  // Verify `window_1` belongs to `desk_1`.
  auto* float_controller = Shell::Get()->float_controller();
  ASSERT_EQ(float_controller->FindDeskOfFloatedWindow(window_1.get()), desk_1);
  // Create `desk_2`.
  NewDesk();
  auto* desk_2 = desks_controller->desks()[1].get();
  // Move to `desk_2`.
  ActivateDesk(desk_2);
  // Float `window_2` at `desk_2`.
  std::unique_ptr<aura::Window> window_2(CreateFloatedWindow());
  // Move back to `desk_1`.
  ActivateDesk(desk_1);
  auto* overview_controller = OverviewController::Get();
  EnterOverview();
  auto* overview_session = overview_controller->overview_session();
  // Get root for displays.
  auto roots = Shell::GetAllRootWindows();
  ASSERT_EQ(2u, roots.size());
  aura::Window* primary_root = roots[0];
  aura::Window* secondary_root = roots[1];
  // The window should exist on the grid of the first display.
  auto* overview_item =
      overview_session->GetOverviewItemForWindow(window_1.get());
  auto* grid1 = overview_session->GetGridWithRootWindow(primary_root);
  auto* grid2 = overview_session->GetGridWithRootWindow(secondary_root);
  EXPECT_EQ(1u, grid1->GetNumWindows());
  EXPECT_EQ(grid1, overview_item->overview_grid());
  EXPECT_EQ(0u, grid2->GetNumWindows());

  // Get position of `desk_2`'s desk mini view on the secondary display.
  const auto* desks_bar_view = grid2->desks_bar_view();
  auto* desk_2_mini_view = desks_bar_view->mini_views()[1].get();
  gfx::Point desk_2_mini_view_center =
      desk_2_mini_view->GetBoundsInScreen().CenterPoint();

  // On overview, drag and drop floated `window_1` to `desk_2` on display 2.
  DragItemToPoint(overview_item, desk_2_mini_view_center, GetEventGenerator(),
                  /*by_touch_gestures=*/false,
                  /*drop=*/true);
  // Verify `window_2` is unfloated.
  ASSERT_FALSE(WindowState::Get(window_2.get())->IsFloated());
  // Verify `window_1` belongs to `desk_2`.
  ASSERT_EQ(float_controller->FindDeskOfFloatedWindow(window_1.get()), desk_2);
  // Verify `window_1` belongs to display 2.
  EXPECT_TRUE(secondary_root->Contains(window_1.get()));
}

// Test that floated window are contained within the work area.
TEST_F(WindowFloatTest, FloatWindowWorkAreaConsiderations) {
  UpdateDisplay("1600x1000");

  // Create a window in the top right quadrant.
  std::unique_ptr<aura::Window> window =
      CreateAppWindow(gfx::Rect(1000, 100, 300, 300));

  // We will use the docked magnifier to modify the work area in this test.
  DockedMagnifierController* docked_magnifier_controller =
      Shell::Get()->docked_magnifier_controller();
  docked_magnifier_controller->SetEnabled(true);
  ASSERT_GT(docked_magnifier_controller->GetMagnifierHeightForTesting(), 0);

  // Float `window` and verify that it is underneath the docked magnifier
  // region.
  PressAndReleaseKey(ui::VKEY_F, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN);
  ASSERT_TRUE(WindowState::Get(window.get())->IsFloated());
  EXPECT_GT(window->GetBoundsInScreen().y(),
            docked_magnifier_controller->GetMagnifierHeightForTesting());

  // Enter tablet mode and drag the floated window so it magnitizes to the top
  // right. Verify that it is underneath the docked magnifier region.
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  ASSERT_TRUE(WindowState::Get(window.get())->IsFloated());
  GetEventGenerator()->GestureScrollSequence(
      GetHeaderView(window.get())->GetBoundsInScreen().CenterPoint(),
      gfx::Point(1000, 300), base::Seconds(3),
      /*steps=*/10);
  EXPECT_GT(window->GetBoundsInScreen().y(),
            docked_magnifier_controller->GetMagnifierHeightForTesting());
}

// Tests that if we unminimize a window that was floated and another window has
// since been floated, unminimizing the window would not float it.
TEST_F(WindowFloatTest, UnminimizeWithFloatedWindow) {
  // Create two windows and float the second one and then minimize it.
  auto window1 = CreateAppWindow();
  auto window2 = CreateFloatedWindow();
  WindowState::Get(window2.get())->Minimize();

  ASSERT_EQ(window1.get(), window_util::GetActiveWindow());
  PressAndReleaseKey(ui::VKEY_F, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN);
  ASSERT_TRUE(WindowState::Get(window1.get())->IsFloated());

  // On unminimizing `window2`, we do not float it even if its pre-minimized
  // state is floated, as doing so would unfloat `window1`.
  WindowState::Get(window2.get())->Unminimize();
  EXPECT_TRUE(WindowState::Get(window1.get())->IsFloated());
}

// Test that floated window are not blocking keyboard events when it's on an
// inactive desk.
TEST_F(WindowFloatTest, FloatWindowShouldNotBlockKeyboardEvents) {
  auto* desks_controller = DesksController::Get();
  // Float `window_1` at `desk_1`.
  auto* desk_1 = desks_controller->desks()[0].get();
  std::unique_ptr<aura::Window> window_1(CreateFloatedWindow());
  // Verify `window_1` belongs to `desk_1`.
  auto* float_controller = Shell::Get()->float_controller();
  ASSERT_EQ(float_controller->FindDeskOfFloatedWindow(window_1.get()), desk_1);
  NewDesk();
  auto* desk_2 = desks_controller->desks()[1].get();
  // Move to `desk_2`.
  ActivateDesk(desk_2);
  // Going into overview mode from keyboard shortcut.
  auto* overview_controller = OverviewController::Get();
  ASSERT_FALSE(overview_controller->InOverviewSession());
  PressAndReleaseKey(ui::VKEY_MEDIA_LAUNCH_APP1, ui::EF_NONE);
  // Verify we are in overview mode.
  ASSERT_TRUE(overview_controller->InOverviewSession());
  // Repeat.
  PressAndReleaseKey(ui::VKEY_MEDIA_LAUNCH_APP1, ui::EF_NONE);
  ASSERT_FALSE(overview_controller->InOverviewSession());
  PressAndReleaseKey(ui::VKEY_MEDIA_LAUNCH_APP1, ui::EF_NONE);
  // Verify we are in overview mode.
  ASSERT_TRUE(overview_controller->InOverviewSession());
}

// Test that activate a floated window on an inactive desk will activate that
// desk.
TEST_F(WindowFloatTest, FloatWindowActivationActivatesBelongingDesk) {
  auto* desks_controller = DesksController::Get();
  // Float `window_1` at `desk_1`.
  auto* desk_1 = desks_controller->desks()[0].get();
  std::unique_ptr<aura::Window> window_1(CreateFloatedWindow());
  // Verify `window_1` belongs to `desk_1`.
  auto* float_controller = Shell::Get()->float_controller();
  ASSERT_EQ(float_controller->FindDeskOfFloatedWindow(window_1.get()), desk_1);
  NewDesk();
  auto* desk_2 = desks_controller->desks()[1].get();
  // Move to `desk_2`.
  ActivateDesk(desk_2);
  DeskSwitchAnimationWaiter waiter;
  // Activate `window_1` that belongs to `desk_1`.
  wm::ActivateWindow(window_1.get());
  EXPECT_TRUE(wm::IsActiveWindow(window_1.get()));
  waiter.Wait();
  // Verify `desk_1` is now activated.
  EXPECT_TRUE(desk_1->is_active());
}

// Tests that if a float window was activated before changing desks, it will be
// activated when returning to that desk.
TEST_F(WindowFloatTest, FloatWindowActivatesWhenChangingDesks) {
  auto* desks_controller = DesksController::Get();

  // Create a floated window on desk 1. We expect this window to be active later
  // when we return to desk 1.
  std::unique_ptr<aura::Window> floated_window1 = CreateFloatedWindow();
  ASSERT_TRUE(WindowState::Get(floated_window1.get())->IsActive());

  // Create a new desk with a floated window and a normal window. The normal
  // window should be activated.
  NewDesk();
  ActivateDesk(desks_controller->desks()[1].get());
  std::unique_ptr<aura::Window> floated_window2 = CreateFloatedWindow();
  std::unique_ptr<aura::Window> normal_window = CreateAppWindow();
  ASSERT_TRUE(WindowState::Get(normal_window.get())->IsActive());

  // Switch to desk 1, the first floated window should be active.
  ActivateDesk(desks_controller->desks()[0].get());
  EXPECT_TRUE(WindowState::Get(floated_window1.get())->IsActive());

  // Switch to desk 2, the normal window should be active.
  ActivateDesk(desks_controller->desks()[1].get());
  EXPECT_TRUE(WindowState::Get(normal_window.get())->IsActive());
}

// Test when we combine desks, floated window is updated on overview.
TEST_F(WindowFloatTest, FloatWindowUpdatedOnOverview) {
  auto* desks_controller = DesksController::Get();
  auto* desk_1 = desks_controller->desks()[0].get();
  // Float `window` at `desk_1`.
  std::unique_ptr<aura::Window> window(CreateFloatedWindow());
  // Verify `window` belongs to `desk_1`.
  auto* float_controller = Shell::Get()->float_controller();
  ASSERT_EQ(float_controller->FindDeskOfFloatedWindow(window.get()), desk_1);
  NewDesk();
  ASSERT_EQ(desks_controller->desks().size(), 2u);
  EnterOverview();
  ASSERT_TRUE(OverviewController::Get()->InOverviewSession());
  RemoveDesk(desk_1, DeskCloseType::kCombineDesks);
  ASSERT_EQ(desks_controller->desks().size(), 1u);
  // Floated window should be appended to overview items.
  const std::vector<std::unique_ptr<OverviewItemBase>>& overview_items =
      GetOverviewItemsForRoot(0);
  ASSERT_EQ(overview_items.size(), 1u);
  EXPECT_EQ(window.get(), overview_items[0]->GetWindow());
}

// Tests the floated window is hidden when there is a pinned window.
TEST_F(WindowFloatTest, PinnedWindow) {
  std::unique_ptr<aura::Window> floated_window = CreateFloatedWindow();

  // Create and pin a window. The floated window should be hidden.
  std::unique_ptr<aura::Window> pinned_window = CreateAppWindow();
  wm::ActivateWindow(pinned_window.get());
  window_util::PinWindow(pinned_window.get(), /*trusted=*/false);
  EXPECT_FALSE(floated_window->IsVisible());

  // Unpin the window.
  Shell::Get()->accelerator_controller()->PerformActionIfEnabled(
      AcceleratorAction::kUnpin, {});
  EXPECT_TRUE(floated_window->IsVisible());

  // Trusted pin the window.
  window_util::PinWindow(pinned_window.get(), /*trusted=*/true);
  EXPECT_FALSE(floated_window->IsVisible());

  // Unpin the window by destroying it.
  pinned_window.reset();
  EXPECT_TRUE(floated_window->IsVisible());

  // Try pinning the floated window. It should still be visible.
  window_util::PinWindow(floated_window.get(), /*trusted=*/true);
  EXPECT_TRUE(floated_window->IsVisible());
}

// Tests that there is no crash when trying to float an always on top window.
// Regression test for b/279366443.
TEST_F(WindowFloatTest, AlwaysOnTopWindow) {
  std::unique_ptr<aura::Window> always_on_top_window = CreateAppWindow();
  always_on_top_window->SetProperty(aura::client::kZOrderingKey,
                                    ui::ZOrderLevel::kFloatingWindow);

  PressAndReleaseKey(ui::VKEY_F, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN);
  EXPECT_FALSE(WindowState::Get(always_on_top_window.get())->IsFloated());
}

// Tests that for unresizable windows, floatability depends on its window state
// type.
TEST_F(WindowFloatTest, UnresizableFloatPerWindowState) {
  std::unique_ptr<aura::Window> window = CreateAppWindow(gfx::Rect(600, 600));
  window->SetProperty(aura::client::kResizeBehaviorKey,
                      aura::client::kResizeBehaviorNone);
  auto* const window_state = WindowState::Get(window.get());

  // Unresizable freeform window should enter floating mode.
  WMEvent restore_event(WM_EVENT_NORMAL);
  window_state->OnWMEvent(&restore_event);
  ASSERT_TRUE(window_state->IsNormalStateType());
  PressAndReleaseKey(ui::VKEY_F, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN);
  EXPECT_TRUE(window_state->IsFloated());
  PressAndReleaseKey(ui::VKEY_F, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN);
  ASSERT_FALSE(window_state->IsFloated());

  // Unresizable maximized window should not enter floating mode.
  WMEvent maximize_event(WM_EVENT_MAXIMIZE);
  window_state->OnWMEvent(&maximize_event);
  ASSERT_TRUE(window_state->IsMaximized());
  PressAndReleaseKey(ui::VKEY_F, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN);
  EXPECT_FALSE(window_state->IsFloated());

  // Unresizable fullscreen window should not enter floating mode.
  WMEvent fullscreen_event(WM_EVENT_FULLSCREEN);
  window_state->OnWMEvent(&fullscreen_event);
  ASSERT_TRUE(window_state->IsFullscreen());
  window_state->Maximize();
  PressAndReleaseKey(ui::VKEY_F, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN);
  EXPECT_FALSE(window_state->IsFloated());
}

// Tests that a window sent to all desks can be floated.
TEST_F(WindowFloatTest, FloatAllDesksWindow) {
  // Create two new desks (three total).
  NewDesk();
  NewDesk();
  auto* desks_controller = DesksController::Get();
  ASSERT_EQ(3u, desks_controller->desks().size());

  // Create a floated window and a regular window on the first desk.
  auto first_floated_window = CreateFloatedWindow();
  auto all_desks_window = CreateAppWindow();

  // Assign the regular window to all desks.
  views::Widget::GetWidgetForNativeWindow(all_desks_window.get())
      ->SetVisibleOnAllWorkspaces(true);
  ASSERT_TRUE(
      desks_util::IsWindowVisibleOnAllWorkspaces(all_desks_window.get()));
  ASSERT_EQ(1u, desks_controller->visible_on_all_desks_windows().size());

  // Switch to the second desk and create another floated window.
  ActivateDesk(desks_controller->desks()[1].get());
  ASSERT_TRUE(wm::IsActiveWindow(all_desks_window.get()));
  auto second_floated_window = CreateFloatedWindow();

  // Switch to the third desk and float the `all_desks_window` using the
  // accelerator.
  ActivateDesk(desks_controller->desks()[2].get());
  ASSERT_TRUE(wm::IsActiveWindow(all_desks_window.get()));
  PressAndReleaseKey(ui::VKEY_F, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN);
  EXPECT_TRUE(WindowState::Get(all_desks_window.get())->IsFloated());

  // Switch back to the first and second desks. The other floated windows should
  // no longer be floated, and the `all_desks_window` should still be floated.
  ActivateDesk(desks_controller->desks()[0].get());
  EXPECT_FALSE(WindowState::Get(first_floated_window.get())->IsFloated());
  EXPECT_TRUE(WindowState::Get(all_desks_window.get())->IsFloated());
  ActivateDesk(desks_controller->desks()[1].get());
  EXPECT_FALSE(WindowState::Get(second_floated_window.get())->IsFloated());
  EXPECT_TRUE(WindowState::Get(all_desks_window.get())->IsFloated());
}

// Tests that a floated window can be sent to all desks.
TEST_F(WindowFloatTest, SendFloatedWindowToAllDesks) {
  NewDesk();
  auto* desks_controller = DesksController::Get();
  ASSERT_EQ(2u, desks_controller->desks().size());

  // Create a floated window on both desks.
  auto first_floated_window = CreateFloatedWindow();
  ActivateDesk(desks_controller->desks()[1].get());
  auto all_floated_window = CreateFloatedWindow();

  // Assign the second floated window to all desks.
  views::Widget::GetWidgetForNativeWindow(all_floated_window.get())
      ->SetVisibleOnAllWorkspaces(true);
  ASSERT_EQ(1u, desks_controller->visible_on_all_desks_windows().size());
  ASSERT_TRUE(
      desks_util::IsWindowVisibleOnAllWorkspaces(all_floated_window.get()));
  ASSERT_TRUE(desks_util::BelongsToActiveDesk(all_floated_window.get()));

  // Switch back the first desk and verify `all_floated_window` is active
  // and floated.
  ActivateDesk(desks_controller->desks()[0].get());
  ASSERT_TRUE(desks_util::BelongsToActiveDesk(all_floated_window.get()));
  EXPECT_TRUE(WindowState::Get(all_floated_window.get())->IsFloated());
  // The first window should no longer be floated.
  ASSERT_TRUE(desks_util::BelongsToActiveDesk(first_floated_window.get()));
  EXPECT_FALSE(WindowState::Get(first_floated_window.get())->IsFloated());

  // Send `all_floated_window` to the second desk. It should no longer appear on
  // the first desk, but it should still be floated on the second desk.
  desks_controller->SendToDeskAtIndex(all_floated_window.get(), 1);
  ASSERT_FALSE(desks_util::BelongsToActiveDesk(all_floated_window.get()));
  ActivateDesk(desks_controller->desks()[1].get());
  ASSERT_TRUE(desks_util::BelongsToActiveDesk(all_floated_window.get()));
  EXPECT_TRUE(WindowState::Get(all_floated_window.get())->IsFloated());
}

// A test class that uses a mock time test environment.
class WindowFloatMetricsTest : public WindowFloatTest {
 public:
  WindowFloatMetricsTest()
      : WindowFloatTest(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  WindowFloatMetricsTest(const WindowFloatMetricsTest&) = delete;
  WindowFloatMetricsTest& operator=(const WindowFloatMetricsTest&) = delete;
  ~WindowFloatMetricsTest() override = default;

 protected:
  base::HistogramTester histogram_tester_;
};

// Tests the float window counts.
TEST_F(WindowFloatMetricsTest, FloatWindowCountPerSession) {
  // Float a window, Tests that it counts properly.
  std::unique_ptr<aura::Window> window = CreateFloatedWindow();

  // Unfloat and float the window again, it should count 2.
  PressAndReleaseKey(ui::VKEY_F, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN);
  ASSERT_FALSE(WindowState::Get(window.get())->IsFloated());
  PressAndReleaseKey(ui::VKEY_F, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN);
  ASSERT_TRUE(WindowState::Get(window.get())->IsFloated());
  // Float a new window on a new desk, it should count 3.
  NewDesk();
  std::unique_ptr<aura::Window> window_2 = CreateFloatedWindow();
  // Check total counts.
  EXPECT_EQ(FloatTestApi::GetFloatedWindowCounter(), 3);
}

// Tests the float window moved to another desk counts.
TEST_F(WindowFloatMetricsTest, FloatWindowMovedToAnotherDeskCountPerSession) {
  // Float a window, then move to another desk, tests that it counts properly.
  std::unique_ptr<aura::Window> window_1 = CreateFloatedWindow();
  // Create a new desk.
  NewDesk();
  auto* desks_controller = DesksController::Get();
  auto* desk_2 = desks_controller->desks()[1].get();
  EnterOverview();
  auto* overview_session = OverviewController::Get()->overview_session();
  // The window should exist on the grid of the first display.
  auto* overview_item =
      overview_session->GetOverviewItemForWindow(window_1.get());
  auto* grid =
      overview_session->GetGridWithRootWindow(Shell::GetPrimaryRootWindow());
  EXPECT_EQ(1u, grid->GetNumWindows());
  // Get position of `desk_2`'s desk mini view.
  const auto* desks_bar_view = grid->desks_bar_view();
  gfx::Point desk_2_mini_view_center =
      desks_bar_view->mini_views()[1]->GetBoundsInScreen().CenterPoint();

  // On overview, drag and drop floated `window_1` to `desk_2`.
  DragItemToPoint(overview_item, desk_2_mini_view_center, GetEventGenerator(),
                  /*by_touch_gestures=*/false,
                  /*drop=*/true);

  // Verify `window_1` belongs to `desk_2`.
  auto* float_controller = Shell::Get()->float_controller();
  ASSERT_EQ(float_controller->FindDeskOfFloatedWindow(window_1.get()), desk_2);
  // Check total counts, it should count 1.
  EXPECT_EQ(FloatTestApi::GetFloatedWindowMoveToAnotherDeskCounter(), 1);
  // Move to `desk_2` and remove `desk_2` by combine 2 desks.
  // Check total counts, it should count 2.
  ActivateDesk(desk_2);
  RemoveDesk(desk_2, DeskCloseType::kCombineDesks);
  EXPECT_EQ(FloatTestApi::GetFloatedWindowMoveToAnotherDeskCounter(), 2);
}

// Tests that the float window duration histogram is properly recorded.
TEST_F(WindowFloatMetricsTest, FloatWindowDuration) {
  constexpr char kHistogramName[] = "Ash.Float.FloatWindowDuration";

  auto* desks_controller = DesksController::Get();
  NewDesk();
  ASSERT_EQ(2u, desks_controller->desks().size());

  // Float the window for 3 minutes and then maximize it. Tests that it records
  // properly.
  std::unique_ptr<aura::Window> window = CreateFloatedWindow();
  task_environment()->AdvanceClock(base::Minutes(3));
  task_environment()->RunUntilIdle();
  WindowState::Get(window.get())->Maximize();
  histogram_tester_.ExpectBucketCount(kHistogramName, 3, 1);

  // Float again for 3 hours. Test that it records into a different bucket.
  PressAndReleaseKey(ui::VKEY_F, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN);
  task_environment()->AdvanceClock(base::Hours(3));
  task_environment()->RunUntilIdle();
  WindowState::Get(window.get())->Maximize();
  histogram_tester_.ExpectBucketCount(kHistogramName, 180, 1);

  // Activate desk 2. At this point the floated window is still in floated
  // state, but is hidden, so we treat it as if a float session has ended.
  PressAndReleaseKey(ui::VKEY_F, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN);
  task_environment()->AdvanceClock(base::Minutes(3));
  task_environment()->RunUntilIdle();
  ActivateDesk(desks_controller->desks()[1].get());
  histogram_tester_.ExpectBucketCount(kHistogramName, 3, 2);

  // Activate desk 1. The floated window will be visible again and we start
  // recording the float session. Sending the window to desk 2 should record
  // the float duration.
  ActivateDesk(desks_controller->desks()[0].get());
  task_environment()->AdvanceClock(base::Minutes(3));
  task_environment()->RunUntilIdle();
  desks_controller->SendToDeskAtIndex(window.get(), 1);
  histogram_tester_.ExpectBucketCount(kHistogramName, 3, 3);

  // Activate desk 2. The floated window will be visible again and we start
  // recording the float session. Hiding and reshowing the window should not
  // record, this is to simulate hiding and reshowing while staying on the
  // active desk (i.e. showing the saved desks library).
  ActivateDesk(desks_controller->desks()[1].get());
  window->Hide();
  task_environment()->AdvanceClock(base::Minutes(3));
  task_environment()->RunUntilIdle();
  window->Show();
  histogram_tester_.ExpectBucketCount(kHistogramName, 3, 3);

  // Closing a window should record the float duration.
  window.reset();
  histogram_tester_.ExpectBucketCount(kHistogramName, 3, 4);
}

// Test bounds for a floated unresizable window.
TEST_F(WindowFloatTest, BoundsForUnresizableWindow) {
  std::unique_ptr<aura::Window> window = CreateAppWindow(gfx::Rect(600, 600));
  window->SetProperty(aura::client::kResizeBehaviorKey,
                      aura::client::kResizeBehaviorNone);
  const gfx::Size window_size = window->GetBoundsInScreen().size();

  // Float the window. Test that the size does not change, and that it is
  // roughly in the bottom right.
  PressAndReleaseKey(ui::VKEY_F, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN);
  ASSERT_TRUE(WindowState::Get(window.get())->IsFloated());
  const gfx::Rect work_area_bounds =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area();
  const gfx::Rect window_bounds = window->GetBoundsInScreen();
  EXPECT_EQ(window_size, window_bounds.size());
  EXPECT_NEAR(window_bounds.bottom(), work_area_bounds.bottom(), 10);
  EXPECT_NEAR(window_bounds.right(), work_area_bounds.right(), 10);
}

class TabletWindowFloatTest : public WindowFloatTest,
                              public display::DisplayObserver {
 public:
  TabletWindowFloatTest() = default;
  TabletWindowFloatTest(const TabletWindowFloatTest&) = delete;
  TabletWindowFloatTest& operator=(const TabletWindowFloatTest&) = delete;
  ~TabletWindowFloatTest() override = default;

  // Flings a window in the direction provided by `left` and `up`.
  void FlingWindow(aura::Window* window, bool left, bool up) {
    const gfx::Point start =
        GetHeaderView(window)->GetBoundsInScreen().CenterPoint();
    const gfx::Vector2d offset(left ? -10 : 10, up ? -10 : 10);
    GetEventGenerator()->GestureScrollSequence(
        start, start + offset, base::Milliseconds(10), /*steps=*/1);
  }

  // Drags `window` so that it magnetizes to `corner`.
  void MagnetizeWindow(aura::Window* window,
                       FloatController::MagnetismCorner corner) {
    const gfx::Rect area =
        WorkAreaInsets::ForWindow(window)->user_work_area_bounds();

    gfx::Point end;
    switch (corner) {
      case FloatController::MagnetismCorner::kTopLeft:
        end = area.origin();
        break;
      case FloatController::MagnetismCorner::kBottomLeft:
        end = area.bottom_left();
        break;
      case FloatController::MagnetismCorner::kTopRight:
        end = area.top_right();
        break;
      case FloatController::MagnetismCorner::kBottomRight:
        end = area.bottom_right();
        break;
    }
    GetEventGenerator()->GestureScrollSequence(
        GetHeaderView(window)->GetBoundsInScreen().CenterPoint(), end,
        base::Milliseconds(100), /*steps=*/3);
  }

  // Checks that `window` has been magnetized in `corner`.
  void CheckMagnetized(aura::Window* window,
                       FloatController::MagnetismCorner corner) {
    const gfx::Rect work_area =
        WorkAreaInsets::ForWindow(window)->user_work_area_bounds();
    const int padding = chromeos::wm::kFloatedWindowPaddingDp;
    switch (corner) {
      case FloatController::MagnetismCorner::kTopLeft:
        EXPECT_EQ(gfx::Point(padding, padding), window->bounds().origin());
        return;
      case FloatController::MagnetismCorner::kBottomLeft:
        EXPECT_EQ(gfx::Point(padding, work_area.bottom() - padding),
                  window->bounds().bottom_left());
        return;
      case FloatController::MagnetismCorner::kTopRight:
        EXPECT_EQ(gfx::Point(work_area.right() - padding, padding),
                  window->bounds().top_right());
        return;
      case FloatController::MagnetismCorner::kBottomRight:
        EXPECT_EQ(gfx::Point(work_area.right() - padding,
                             work_area.bottom() - padding),
                  window->bounds().bottom_right());
        return;
    }
  }

  void SetOnTabletStateChangedCallback(
      base::RepeatingCallback<void(display::TabletState)> callback) {
    on_tablet_state_changed_callback_ = callback;
    display_observer_.emplace(this);
  }

  // WindowFloatTest:
  void SetUp() override {
    // This allows us to snap to the bottom in portrait mode.
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        ::switches::kUseFirstDisplayAsInternal);
    WindowFloatTest::SetUp();
  }

  void TearDown() override {
    display_observer_.reset();
    WindowFloatTest::TearDown();
  }

  // display::DisplayObserver:
  void OnDisplayTabletStateChanged(display::TabletState state) override {
    on_tablet_state_changed_callback_.Run(state);
  }

 protected:
  base::UserActionTester user_action_tester_;

 private:
  // Called when the tablet state changes.
  base::RepeatingCallback<void(display::TabletState)>
      on_tablet_state_changed_callback_;

  std::optional<display::ScopedDisplayObserver> display_observer_;
};

// Test class used to keep track of the amount of times the tuck education nudge
// has appeared.
class NudgeCounter : public aura::WindowObserver {
 public:
  NudgeCounter() {
    window_observation_.Observe(Shell::Get()->GetPrimaryRootWindow());
  }
  NudgeCounter(const NudgeCounter&) = delete;
  NudgeCounter& operator=(const NudgeCounter&) = delete;
  ~NudgeCounter() override = default;

  int nudge_count() const { return nudge_count_; }

  // aura::WindowObserver:
  void OnWindowHierarchyChanged(const HierarchyChangeParams& params) override {
    if (params.target->GetName() == "TuckEducationNudgeWidget" &&
        params.new_parent) {
      nudge_count_++;
    }
  }
  void OnWindowDestroying(aura::Window* window) override {
    window_observation_.Reset();
  }

 private:
  int nudge_count_ = 0;

  base::ScopedObservation<aura::Window, aura::WindowObserver>
      window_observation_{this};
};

TEST_F(TabletWindowFloatTest, TabletClamshellTransition) {
  auto window1 = CreateFloatedWindow();

  // Test that on entering tablet mode, we maintain float state.
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  EXPECT_TRUE(WindowState::Get(window1.get())->IsFloated());

  // Create a new floated window in tablet mode. It should unfloat the existing
  // floated window.
  auto window2 = CreateFloatedWindow();
  EXPECT_FALSE(WindowState::Get(window1.get())->IsFloated());

  // Test that on exiting tablet mode, we maintain float state.
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(false);
  EXPECT_TRUE(WindowState::Get(window2.get())->IsFloated());
}

TEST_F(TabletWindowFloatTest, ClamshellToTabletMagnetism) {
  auto window = CreateFloatedWindow();
  window->SetBounds(gfx::Rect(300, 300));

  // Verify that on entering tablet mode, since our window's origin was 0,0, the
  // window is magnetized to the top left.
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  EXPECT_TRUE(WindowState::Get(window.get())->IsFloated());
  CheckMagnetized(window.get(), FloatController::MagnetismCorner::kTopLeft);
}

// Tests that the expected windows are animating duration a tablet <-> clamshell
// transition.
TEST_F(TabletWindowFloatTest, TabletClamshellTransitionAnimation) {
  auto normal_window = CreateAppWindow();
  auto floated_window = CreateFloatedWindow();

  // Both windows are expected to animate, so we wait for them both. We don't
  // know which window would finish first so we wait for the normal window
  // first, and if the floated window is still animating, wait for that as well.
  auto wait_for_windows_finish_animating = [&]() {
    ShellTestApi().WaitForWindowFinishAnimating(normal_window.get());
    if (floated_window->layer()->GetAnimator()->is_animating())
      ShellTestApi().WaitForWindowFinishAnimating(floated_window.get());
  };

  // Activate `normal_window`. `floated_window` should still animate since it is
  // stacked on top and visible.
  wm::ActivateWindow(normal_window.get());

  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Tests that on entering tablet mode, both windows are animating since both
  // are visible before and after the transition.
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  ASSERT_TRUE(IsVisiblyAnimating(normal_window.get()));
  ASSERT_TRUE(IsVisiblyAnimating(floated_window.get()));
  wait_for_windows_finish_animating();

  // Tests that both windows are animating on exit.
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(false);
  ASSERT_TRUE(IsVisiblyAnimating(normal_window.get()));
  ASSERT_TRUE(IsVisiblyAnimating(floated_window.get()));
  wait_for_windows_finish_animating();

  // Activate `floated_window`. `normal_window` should still animate since it
  // is visible behind `floated_window`.
  wm::ActivateWindow(floated_window.get());

  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  ASSERT_TRUE(IsVisiblyAnimating(normal_window.get()));
  ASSERT_TRUE(IsVisiblyAnimating(floated_window.get()));
  wait_for_windows_finish_animating();

  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(false);
  EXPECT_TRUE(IsVisiblyAnimating(normal_window.get()));
  EXPECT_TRUE(IsVisiblyAnimating(floated_window.get()));
}

// Tests that the new minimum size is respected when entering tablet mode.
// Regression test for b/261780362.
TEST_F(TabletWindowFloatTest, MinimumSizeChangeOnTablet) {
  UpdateDisplay("800x600");

  // Create a window in clamshell mode without a minimum size, and larger than
  // its tablet minimum size.
  auto window =
      CreateAppWindow(gfx::Rect(500, 500), chromeos::AppType::SYSTEM_APP,
                      kShellWindowId_DeskContainerA, new TestWidgetDelegateAsh);
  auto* custom_frame = static_cast<TestNonClientFrameViewAsh*>(
      NonClientFrameViewAsh::Get(window.get()));
  wm::ActivateWindow(window.get());
  PressAndReleaseKey(ui::VKEY_F, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN);
  ASSERT_TRUE(WindowState::Get(window.get())->IsFloated());

  // Set a minimum size for tablet that is floatable.
  const gfx::Size tablet_minimum_size(300, 300);

  // Change the minimum size when tablet mode is changed. This mimics the
  // behaviour chrome windows have, when they switch to a more tap friendly mode
  // with larger buttons, which results in a larger minimum size.
  SetOnTabletStateChangedCallback(
      base::BindLambdaForTesting([&](display::TabletState state) {
        switch (state) {
          case display::TabletState::kInTabletMode:
            custom_frame->SetMinimumSize(tablet_minimum_size);
            return;
          case display::TabletState::kInClamshellMode:
            custom_frame->SetMinimumSize(gfx::Size());
            return;
          case display::TabletState::kEnteringTabletMode:
          case display::TabletState::kExitingTabletMode:
            break;
        }
      }));

  // Tests that on entering tablet mode, the window is sized to its minimum
  // width.
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  ASSERT_TRUE(WindowState::Get(window.get())->IsFloated());
  EXPECT_EQ(tablet_minimum_size.width(), window->bounds().width());

  // Alter the minimum size of the window. Tests that the window bounds adjust
  // to match.
  custom_frame->SetMinimumSize(gfx::Size(350, 350));
  ASSERT_TRUE(WindowState::Get(window.get())->IsFloated());
  EXPECT_EQ(350, window->bounds().width());
}

// Tests if a browser can float at several minimum sizes.
TEST_F(TabletWindowFloatTest, CanBrowsersFloat) {
  UpdateDisplay("800x600");

  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);

  const int work_area_width =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area().width();
  const int maximum_float_width =
      (work_area_width - chromeos::wm::kSplitviewDividerShortSideLength) / 2 -
      chromeos::wm::kFloatedWindowPaddingDp * 2;

  aura::test::TestWindowDelegate window_delegate;
  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithDelegate(
      &window_delegate, /*id=*/-1, gfx::Rect(500, 500)));
  window->SetProperty(chromeos::kAppTypeKey, chromeos::AppType::BROWSER);
  wm::ActivateWindow(window.get());

  // Browser windows whose minimum size is greater than the maximum allowed
  // float width can never be floated.
  window_delegate.set_minimum_size(gfx::Size(maximum_float_width + 5, 100));
  PressAndReleaseKey(ui::VKEY_F, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN);
  ASSERT_FALSE(WindowState::Get(window.get())->IsFloated());

  // Browser windows whose minimum size is slightly less than the maximum
  // allowed, will be floated and have some extra padding, but their size should
  // not exceed the maximum allowed.
  window_delegate.set_minimum_size(gfx::Size(maximum_float_width - 20, 100));
  PressAndReleaseKey(ui::VKEY_F, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN);
  ASSERT_TRUE(WindowState::Get(window.get())->IsFloated());
  EXPECT_EQ(maximum_float_width, window->bounds().width());

  // Unfloat the window for the next check.
  PressAndReleaseKey(ui::VKEY_F, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN);
  ASSERT_FALSE(WindowState::Get(window.get())->IsFloated());

  // Browser windows whose minimum size is smaller that the maximum allowed will
  // have extra padding to make the omnibox tappable.
  window_delegate.set_minimum_size(gfx::Size(250, 100));
  PressAndReleaseKey(ui::VKEY_F, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN);
  ASSERT_TRUE(WindowState::Get(window.get())->IsFloated());
  EXPECT_EQ(250 + chromeos::wm::kBrowserExtraPaddingDp,
            window->bounds().width());
}

// Tests that a window can be floated in tablet mode, unless its minimum width
// is greater than half the work area.
TEST_F(TabletWindowFloatTest, TabletPositioningLandscape) {
  UpdateDisplay("800x600");

  aura::test::TestWindowDelegate window_delegate;
  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithDelegate(
      &window_delegate, /*id=*/-1, gfx::Rect(300, 300)));
  window->SetProperty(chromeos::kAppTypeKey, chromeos::AppType::BROWSER);
  wm::ActivateWindow(window.get());

  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);

  PressAndReleaseKey(ui::VKEY_F, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN);
  ASSERT_TRUE(WindowState::Get(window.get())->IsFloated());

  PressAndReleaseKey(ui::VKEY_F, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN);
  ASSERT_FALSE(WindowState::Get(window.get())->IsFloated());

  window_delegate.set_minimum_size(gfx::Size(600, 600));
  PressAndReleaseKey(ui::VKEY_F, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN);
  EXPECT_FALSE(WindowState::Get(window.get())->IsFloated());
}

// Tests that a window that cannot be floated in tablet mode unfloats after
// entering tablet mode.
TEST_F(TabletWindowFloatTest, FloatWindowUnfloatsEnterTablet) {
  UpdateDisplay("800x600");

  aura::test::TestWindowDelegate window_delegate;
  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithDelegate(
      &window_delegate, /*id=*/-1, gfx::Rect(850, 850)));
  window_delegate.set_minimum_size(gfx::Size(500, 500));
  window->SetProperty(chromeos::kAppTypeKey, chromeos::AppType::BROWSER);
  wm::ActivateWindow(window.get());

  PressAndReleaseKey(ui::VKEY_F, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN);
  ASSERT_TRUE(WindowState::Get(window.get())->IsFloated());

  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  EXPECT_FALSE(WindowState::Get(window.get())->IsFloated());
}

// Tests that a floated window unfloats if a display change makes it no longer a
// valid floating window.
TEST_F(TabletWindowFloatTest, FloatWindowUnfloatsDisplayChange) {
  UpdateDisplay("1800x1000");

  aura::test::TestWindowDelegate window_delegate;
  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithDelegate(
      &window_delegate, /*id=*/-1, gfx::Rect(300, 300)));
  window_delegate.set_minimum_size(gfx::Size(400, 400));
  window->SetProperty(chromeos::kAppTypeKey, chromeos::AppType::BROWSER);
  wm::ActivateWindow(window.get());

  // Enter tablet mode and float `window`.
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  PressAndReleaseKey(ui::VKEY_F, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN);
  ASSERT_TRUE(WindowState::Get(window.get())->IsFloated());

  // If the display width is 700, the minimum width exceeds half the display
  // width.
  UpdateDisplay("700x600");
  EXPECT_FALSE(WindowState::Get(window.get())->IsFloated());
}

// Tests that windows floated in tablet mode have immersive mode disabled,
// showing their title bars.
TEST_F(TabletWindowFloatTest, ImmersiveMode) {
  // Create a test app window that has a header.
  auto window = CreateAppWindow();
  auto* immersive_controller = chromeos::ImmersiveFullscreenController::Get(
      views::Widget::GetWidgetForNativeView(window.get()));

  // Enter tablet mode and float `window`.
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  EXPECT_TRUE(immersive_controller->IsEnabled());

  PressAndReleaseKey(ui::VKEY_F, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN);
  EXPECT_FALSE(immersive_controller->IsEnabled());

  PressAndReleaseKey(ui::VKEY_F, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN);
  EXPECT_TRUE(immersive_controller->IsEnabled());

  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(false);
  EXPECT_FALSE(immersive_controller->IsEnabled());
}

// Tests that if we minimize a floated window, it is floated upon unminimizing,
// and magnetized in the same corner as before.
TEST_F(TabletWindowFloatTest, UnminimizeFloatWindow) {
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);

  std::unique_ptr<aura::Window> window = CreateFloatedWindow();
  MagnetizeWindow(window.get(), FloatController::MagnetismCorner::kTopLeft);

  auto* window_state = WindowState::Get(window.get());
  window_state->Minimize();
  window_state->Unminimize();
  EXPECT_TRUE(window_state->IsFloated());
  CheckMagnetized(window.get(), FloatController::MagnetismCorner::kTopLeft);
}

TEST_F(TabletWindowFloatTest, Rotation) {
  // Use a display where the width and height are quite different, otherwise it
  // would be hard to tell if portrait mode is using landscape bounds to
  // calculate floating window bounds.
  UpdateDisplay("1800x1000");

  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);

  std::unique_ptr<aura::Window> window = CreateFloatedWindow();
  const gfx::Rect no_rotation_bounds = window->bounds();

  // First rotate to landscape secondary orientation. The float bounds should
  // be the same.
  ScreenOrientationControllerTestApi orientation_test_api(
      Shell::Get()->screen_orientation_controller());
  orientation_test_api.SetDisplayRotation(
      display::Display::ROTATE_180, display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(window->bounds(), no_rotation_bounds);

  // Rotate to the two portrait orientations. The float bounds should be
  // similar since landscape bounds are used for portrait float calculations
  // as well, but slightly different since the shelf affects the work area
  // differently.
  const int shelf_size = ShelfConfig::Get()->shelf_size();
  orientation_test_api.SetDisplayRotation(
      display::Display::ROTATE_90, display::Display::RotationSource::ACTIVE);
  EXPECT_NEAR(no_rotation_bounds.width(), window->bounds().width(), shelf_size);
  EXPECT_NEAR(no_rotation_bounds.height(), window->bounds().height(),
              shelf_size);

  orientation_test_api.SetDisplayRotation(
      display::Display::ROTATE_270, display::Display::RotationSource::ACTIVE);
  EXPECT_NEAR(no_rotation_bounds.width(), window->bounds().width(), shelf_size);
  EXPECT_NEAR(no_rotation_bounds.height(), window->bounds().height(),
              shelf_size);
}

// Tests that dragged float window follows the touch. Regression test for
// https://crbug.com/1362727.
TEST_F(TabletWindowFloatTest, Dragging) {
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);

  std::unique_ptr<aura::Window> window = CreateFloatedWindow();

  // Start dragging in the center of the header. When moving the touch, the
  // header should move with the touch such that the touch remains in the center
  // of the header.
  chromeos::HeaderView* header_view = GetHeaderView(window.get());
  auto* event_generator = GetEventGenerator();
  event_generator->PressTouch(header_view->GetBoundsInScreen().CenterPoint());

  // Drag to several points. Verify the header is aligned with the new touch
  // point. Also verify that we do not recreate the window while dragging - this
  // would mean we are using a cross-fade animation. See b/272529481 for more
  // details.
  WindowLayerRecreatedCounter recreated_counter(window.get());
  for (const gfx::Point& point :
       {gfx::Point(10, 10), gfx::Point(100, 10), gfx::Point(400, 400)}) {
    event_generator->MoveTouch(point);
    EXPECT_EQ(point, header_view->GetBoundsInScreen().CenterPoint());
  }
  EXPECT_EQ(0, recreated_counter.recreated_count());
}

// Tests that there is no crash when maximizing a dragged floated window.
// Regression test for http://b/254107825.
TEST_F(TabletWindowFloatTest, MaximizeWhileDragging) {
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);

  std::unique_ptr<aura::Window> window = CreateFloatedWindow();

  // Press the accelerator to maximize before releasing touch.
  chromeos::HeaderView* header_view = GetHeaderView(window.get());
  auto* event_generator = GetEventGenerator();
  event_generator->PressTouch(header_view->GetBoundsInScreen().CenterPoint());
  event_generator->MoveTouch(gfx::Point(100, 100));
  PressAndReleaseKey(ui::VKEY_OEM_PLUS, ui::EF_ALT_DOWN);

  EXPECT_TRUE(WindowState::Get(window.get())->IsMaximized());

  // Press the accelerator to minimize before releasing touch.
  event_generator->PressTouch(header_view->GetBoundsInScreen().CenterPoint());
  event_generator->MoveTouch(gfx::Point(100, 100));
  PressAndReleaseKey(ui::VKEY_OEM_MINUS, ui::EF_ALT_DOWN);

  EXPECT_TRUE(WindowState::Get(window.get())->IsMinimized());
}

// Tests that on drag release, the window sticks to one of the four corners of
// the work area.
TEST_F(TabletWindowFloatTest, DraggingMagnetism) {
  // Use a set display size so we can drag to specific spots.
  UpdateDisplay("1600x1000");

  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);

  std::unique_ptr<aura::Window> window = CreateFloatedWindow();

  const int padding = chromeos::wm::kFloatedWindowPaddingDp;
  const int shelf_size = ShelfConfig::Get()->shelf_size();

  // The default location is in the bottom right.
  EXPECT_EQ(gfx::Point(1600 - padding, 1000 - padding - shelf_size),
            window->bounds().bottom_right());
  CheckMagnetized(window.get(), FloatController::MagnetismCorner::kBottomRight);

  // Test no change if we drag it in the bottom right.
  MagnetizeWindow(window.get(), FloatController::MagnetismCorner::kBottomRight);
  CheckMagnetized(window.get(), FloatController::MagnetismCorner::kBottomRight);

  // Move finger somewhere in the top right, but not too right that it falls
  // into the snap region. Test that on release, it magnetizes to the top right
  MagnetizeWindow(window.get(), FloatController::MagnetismCorner::kTopRight);
  CheckMagnetized(window.get(), FloatController::MagnetismCorner::kTopRight);

  // Move finger to somewhere in the top left, but not too left that it falls
  // into the snap region. Test that on release, it magnetizes to the top left.
  MagnetizeWindow(window.get(), FloatController::MagnetismCorner::kTopLeft);
  CheckMagnetized(window.get(), FloatController::MagnetismCorner::kTopLeft);

  // Switch to portrait orientation and move finger somewhere in the bottom
  // left, but not too bottom that it falls into the snap region. Test that on
  // release, it magentizes to the bottom left.
  UpdateDisplay("1000x1600");
  MagnetizeWindow(window.get(), FloatController::MagnetismCorner::kBottomLeft);
  CheckMagnetized(window.get(), FloatController::MagnetismCorner::kBottomLeft);
}

TEST_F(TabletWindowFloatTest, UntuckWindowOnExitTabletMode) {
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  // The window is magnetized to the bottom right by default.
  std::unique_ptr<aura::Window> window = CreateFloatedWindow();

  // Fling to tuck the window in the bottom right.
  auto* float_controller = Shell::Get()->float_controller();
  FlingWindow(window.get(), /*left=*/false, /*up=*/false);
  EXPECT_TRUE(WindowState::Get(window.get())->IsFloated());
  ASSERT_TRUE(float_controller->IsFloatedWindowTuckedForTablet(window.get()));

  // Tests that after we exit tablet mode, the window is untucked and fully
  // visible, but is still floated.
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(false);
  EXPECT_FALSE(float_controller->IsFloatedWindowTuckedForTablet(window.get()));
  EXPECT_TRUE(screen_util::GetDisplayBoundsInParent(window.get())
                  .Contains(window->bounds()));
  EXPECT_TRUE(WindowState::Get(window.get())->IsFloated());
}

TEST_F(TabletWindowFloatTest, UntuckWindowOnActivation) {
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  std::unique_ptr<aura::Window> window = CreateFloatedWindow();

  // Fling to tuck the window in the bottom right.
  auto* float_controller = Shell::Get()->float_controller();
  FlingWindow(window.get(), /*left=*/false, /*up=*/false);
  EXPECT_TRUE(WindowState::Get(window.get())->IsFloated());
  ASSERT_TRUE(float_controller->IsFloatedWindowTuckedForTablet(window.get()));

  // Tests that after we activate the window, the window is untucked and fully
  // visible, but is still floated.
  wm::ActivateWindow(window.get());
  EXPECT_FALSE(float_controller->IsFloatedWindowTuckedForTablet(window.get()));
  EXPECT_TRUE(screen_util::GetDisplayBoundsInParent(window.get())
                  .Contains(window->bounds()));
  EXPECT_TRUE(WindowState::Get(window.get())->IsFloated());
}

// Tests that when we switch desks, a tucked window gets untucked.
TEST_F(TabletWindowFloatTest, UntuckWindowOnDeskChange) {
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);

  auto* desks_controller = DesksController::Get();
  NewDesk();
  ASSERT_EQ(2u, desks_controller->desks().size());
  ActivateDesk(desks_controller->desks()[1].get());

  // Create a floated window on the second desk and fling it.
  std::unique_ptr<aura::Window> window = CreateFloatedWindow();
  const gfx::Rect pre_tucked_bounds = window->bounds();
  FlingWindow(window.get(), /*left=*/false, /*up=*/false);
  ASSERT_TRUE(desks_util::BelongsToActiveDesk(window.get()));
  ASSERT_TRUE(WindowState::Get(window.get())->IsFloated());
  ASSERT_TRUE(Shell::Get()->float_controller()->IsFloatedWindowTuckedForTablet(
      window.get()));

  // Activate desk 1. The window is still floated but is hidden and not tucked.
  ActivateDesk(desks_controller->desks()[0].get());
  EXPECT_TRUE(WindowState::Get(window.get())->IsFloated());
  EXPECT_FALSE(window->IsVisible());
  EXPECT_EQ(pre_tucked_bounds, window->bounds());
  EXPECT_FALSE(Shell::Get()->float_controller()->IsFloatedWindowTuckedForTablet(
      window.get()));
}

// Tests that the tucked window is invisible while it is fully tucked.
TEST_F(TabletWindowFloatTest, TuckedWindowVisibility) {
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  std::unique_ptr<aura::Window> window = CreateFloatedWindow();

  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Fling to tuck the window in the bottom right. Test that the window is
  // invisible once the animation is finished.
  auto* float_controller = Shell::Get()->float_controller();
  FlingWindow(window.get(), /*left=*/false, /*up=*/false);
  EXPECT_TRUE(window->IsVisible());
  ShellTestApi().WaitForWindowFinishAnimating(window.get());
  EXPECT_FALSE(window->IsVisible());
  ASSERT_TRUE(float_controller->IsFloatedWindowTuckedForTablet(window.get()));

  // Tests that there is an overview item created for the tucked window.
  ToggleOverview();
  WaitForOverviewEnterAnimation();
  EXPECT_TRUE(GetOverviewItemForWindow(window.get()));

  // Tests that after we activate the window, the window is visible again as it
  // is getting untucked.
  wm::ActivateWindow(window.get());
  ShellTestApi().WaitForWindowFinishAnimating(window.get());
  EXPECT_TRUE(window->IsVisible());
}

// Tests that the floated window is visible when it is untucked.
TEST_F(TabletWindowFloatTest, UntuckedWindowVisibility) {
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  std::unique_ptr<aura::Window> window = CreateFloatedWindow();

  // Long duration for tuck animation, to allow it to be interrupted.
  ui::ScopedAnimationDurationScaleMode test_duration(
      ui::ScopedAnimationDurationScaleMode::SLOW_DURATION);

  // Fling to tuck the window. Press the untuck handle before the window
  // finishes the tuck animation. Test that the window is visible.
  FlingWindow(window.get(), /*left=*/false, /*up=*/false);
  auto* float_controller = Shell::Get()->float_controller();
  views::Widget* tuck_handle_widget =
      float_controller->GetTuckHandleWidget(window.get());
  ASSERT_TRUE(tuck_handle_widget);
  GestureTapOn(tuck_handle_widget->GetContentsView());
  ASSERT_TRUE(window->IsVisible());
  EXPECT_FALSE(float_controller->IsFloatedWindowTuckedForTablet(window.get()));
}

// Tests that the expected window gets activation after tucking a floated
// window, and that on untucking the floated window, it gains activation.
TEST_F(TabletWindowFloatTest, WindowActivationAfterTuckingUntucking) {
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  std::unique_ptr<aura::Window> float_window = CreateFloatedWindow();

  // Fling to tuck the window in the bottom right.
  ASSERT_EQ(float_window.get(), window_util::GetActiveWindow());
  FlingWindow(float_window.get(), /*left=*/false, /*up=*/false);
  auto* float_controller = Shell::Get()->float_controller();
  ASSERT_TRUE(
      float_controller->IsFloatedWindowTuckedForTablet(float_window.get()));

  // There are no other app windows, so the activation goes to the app list.
  EXPECT_EQ(Shell::Get()->app_list_controller()->GetWindow(),
            window_util::GetActiveWindow());

  // Create another window and untuck the floated window.
  std::unique_ptr<aura::Window> window2 = CreateAppWindow();
  views::Widget* tuck_handle_widget =
      float_controller->GetTuckHandleWidget(float_window.get());
  ASSERT_TRUE(tuck_handle_widget);
  GestureTapOn(tuck_handle_widget->GetContentsView());
  ASSERT_FALSE(
      float_controller->IsFloatedWindowTuckedForTablet(float_window.get()));
  ASSERT_EQ(float_window.get(), window_util::GetActiveWindow());

  // Tests that tucking the floated window activates the window underneath.
  FlingWindow(float_window.get(), /*left=*/false, /*up=*/false);
  ASSERT_TRUE(
      float_controller->IsFloatedWindowTuckedForTablet(float_window.get()));
  EXPECT_EQ(window2.get(), window_util::GetActiveWindow());

  // Untuck the floated window and minimize the other window.
  tuck_handle_widget =
      float_controller->GetTuckHandleWidget(float_window.get());
  ASSERT_TRUE(tuck_handle_widget);
  GestureTapOn(tuck_handle_widget->GetContentsView());
  ASSERT_FALSE(
      float_controller->IsFloatedWindowTuckedForTablet(float_window.get()));
  WindowState::Get(window2.get())->Minimize();
  ASSERT_EQ(float_window.get(), window_util::GetActiveWindow());

  // Tests that tucking the floated window activates the app list instead of
  // activating and unminimizing the minimized window.
  FlingWindow(float_window.get(), /*left=*/false, /*up=*/false);
  ASSERT_TRUE(
      float_controller->IsFloatedWindowTuckedForTablet(float_window.get()));
  EXPECT_EQ(Shell::Get()->app_list_controller()->GetWindow(),
            window_util::GetActiveWindow());
}

// Tests the functionality of tucking a window in tablet mode.
TEST_F(TabletWindowFloatTest, TuckWindowLeft) {
  UpdateDisplay("1600x1000");

  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  std::unique_ptr<aura::Window> window = CreateFloatedWindow();

  // Magnetize the window to the top left.
  MagnetizeWindow(window.get(), FloatController::MagnetismCorner::kTopLeft);
  auto* float_controller = Shell::Get()->float_controller();

  // Verify user action initial states.
  EXPECT_EQ(0, user_action_tester_.GetActionCount(kTuckUserAction));
  EXPECT_EQ(0, user_action_tester_.GetActionCount(kUntuckUserAction));

  // Fling the window left and up. Test that it tucks in the top left.
  FlingWindow(window.get(), /*left=*/true, /*up=*/true);
  ASSERT_TRUE(float_controller->IsFloatedWindowTuckedForTablet(window.get()));
  const int padding = chromeos::wm::kFloatedWindowPaddingDp;
  EXPECT_EQ(gfx::Point(0, padding), window->bounds().top_right());
  EXPECT_EQ(1, user_action_tester_.GetActionCount(kTuckUserAction));

  // Test that the tuck handle is aligned with the window.
  views::Widget* tuck_handle_widget =
      float_controller->GetTuckHandleWidget(window.get());
  ASSERT_TRUE(tuck_handle_widget);
  EXPECT_EQ(window->bounds().right_center(),
            tuck_handle_widget->GetWindowBoundsInScreen().left_center());

  // Untuck the window. Test that it magnetizes to the top left.
  GestureTapOn(tuck_handle_widget->GetContentsView());
  ASSERT_FALSE(float_controller->IsFloatedWindowTuckedForTablet(window.get()));
  CheckMagnetized(window.get(), FloatController::MagnetismCorner::kTopLeft);
  EXPECT_EQ(1, user_action_tester_.GetActionCount(kUntuckUserAction));

  // Fling the window left and down. Test that it tucks in the bottom left.
  FlingWindow(window.get(), /*left=*/true, /*up=*/false);
  ASSERT_TRUE(float_controller->IsFloatedWindowTuckedForTablet(window.get()));
  const gfx::Rect work_area = WorkAreaInsets::ForWindow(window->GetRootWindow())
                                  ->user_work_area_bounds();
  EXPECT_EQ(gfx::Point(0, work_area.bottom() - padding),
            window->bounds().bottom_right());
  EXPECT_EQ(2, user_action_tester_.GetActionCount(kTuckUserAction));

  // Untuck the window. Test that it magnetizes to the bottom left.
  tuck_handle_widget = float_controller->GetTuckHandleWidget(window.get());
  GestureTapOn(tuck_handle_widget->GetContentsView());
  ASSERT_FALSE(float_controller->IsFloatedWindowTuckedForTablet(window.get()));
  CheckMagnetized(window.get(), FloatController::MagnetismCorner::kBottomLeft);
  EXPECT_EQ(2, user_action_tester_.GetActionCount(kUntuckUserAction));
}

// Tests the functionality of tucking a window in tablet mode.
TEST_F(TabletWindowFloatTest, TuckWindowRight) {
  UpdateDisplay("1600x1000");

  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  // The window is magnetized to the bottom right by default.
  std::unique_ptr<aura::Window> window = CreateFloatedWindow();
  auto* float_controller = Shell::Get()->float_controller();
  const gfx::Rect work_area = WorkAreaInsets::ForWindow(window->GetRootWindow())
                                  ->user_work_area_bounds();

  // Verify user action initial states.
  EXPECT_EQ(0, user_action_tester_.GetActionCount(kTuckUserAction));
  EXPECT_EQ(0, user_action_tester_.GetActionCount(kUntuckUserAction));

  // Fling the window right and up. Test that it tucks in the top right.
  FlingWindow(window.get(), /*left=*/false, /*up=*/true);
  ASSERT_TRUE(float_controller->IsFloatedWindowTuckedForTablet(window.get()));
  const int padding = chromeos::wm::kFloatedWindowPaddingDp;
  EXPECT_EQ(gfx::Point(work_area.right(), padding), window->bounds().origin());
  EXPECT_EQ(1, user_action_tester_.GetActionCount(kTuckUserAction));

  // Test that the tuck handle is aligned with the window.
  views::Widget* tuck_handle_widget =
      float_controller->GetTuckHandleWidget(window.get());
  ASSERT_TRUE(tuck_handle_widget);
  EXPECT_EQ(window->bounds().left_center(),
            tuck_handle_widget->GetWindowBoundsInScreen().right_center());

  // Untuck the window. Test that it magnetizes to the top right.
  GestureTapOn(tuck_handle_widget->GetContentsView());
  ASSERT_FALSE(float_controller->IsFloatedWindowTuckedForTablet(window.get()));
  CheckMagnetized(window.get(), FloatController::MagnetismCorner::kTopRight);
  EXPECT_EQ(1, user_action_tester_.GetActionCount(kUntuckUserAction));

  // Fling the window right and down. Test that it tucks in the bottom right.
  FlingWindow(window.get(), /*left=*/false, /*up=*/false);
  ASSERT_TRUE(float_controller->IsFloatedWindowTuckedForTablet(window.get()));
  EXPECT_EQ(gfx::Point(work_area.right(), work_area.bottom() - padding),
            window->bounds().bottom_left());
  EXPECT_EQ(2, user_action_tester_.GetActionCount(kTuckUserAction));

  // Untuck the window. Test that it magnetizes to the bottom right.
  tuck_handle_widget = float_controller->GetTuckHandleWidget(window.get());
  GestureTapOn(tuck_handle_widget->GetContentsView());
  ASSERT_FALSE(float_controller->IsFloatedWindowTuckedForTablet(window.get()));
  CheckMagnetized(window.get(), FloatController::MagnetismCorner::kBottomRight);
  EXPECT_EQ(2, user_action_tester_.GetActionCount(kUntuckUserAction));
}

// Tests that the window gets tucked to the closer edge and corner based on the
// fling velocity.
TEST_F(TabletWindowFloatTest, TuckToMagnetismCorner) {
  UpdateDisplay("1600x1000");

  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);

  // Create a floated window in the bottom right.
  std::unique_ptr<aura::Window> window = CreateFloatedWindow();
  CheckMagnetized(window.get(), FloatController::MagnetismCorner::kBottomRight);

  auto* float_controller = Shell::Get()->float_controller();

  // Fling the window left and up. Test that it does not tuck but magnetizes to
  // the top left.
  FlingWindow(window.get(), /*left=*/true, /*up=*/true);
  ASSERT_FALSE(float_controller->IsFloatedWindowTuckedForTablet(window.get()));
  CheckMagnetized(window.get(), FloatController::MagnetismCorner::kTopLeft);

  // Fling the window left and down. Now the window should tuck in the bottom
  // left.
  FlingWindow(window.get(), /*left=*/true, /*up=*/false);
  ASSERT_TRUE(float_controller->IsFloatedWindowTuckedForTablet(window.get()));
  const gfx::Rect work_area = WorkAreaInsets::ForWindow(window->GetRootWindow())
                                  ->user_work_area_bounds();
  const int padding = chromeos::wm::kFloatedWindowPaddingDp;
  EXPECT_EQ(gfx::Point(0, work_area.bottom() - padding),
            window->bounds().bottom_right());
}

// Tests that tapping on a point on the edge but far from the tuck handle does
// not untuck a tucked window. Regression test for b/262573071.
TEST_F(TabletWindowFloatTest, TapOnEdgeDoesNotUntuck) {
  UpdateDisplay("800x600");

  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  // The window is magnetized to the bottom right by default.
  std::unique_ptr<aura::Window> window = CreateFloatedWindow();
  auto* float_controller = Shell::Get()->float_controller();

  // Tuck the window in the bottom right.
  FlingWindow(window.get(), /*left=*/false, /*up=*/false);
  ASSERT_TRUE(float_controller->IsFloatedWindowTuckedForTablet(window.get()));

  // Select a point close to the edge that is not close to the tuck handle.
  const gfx::Point point(799, window->GetBoundsInScreen().y());
  views::Widget* tuck_handle_widget =
      float_controller->GetTuckHandleWidget(window.get());
  ASSERT_FALSE(tuck_handle_widget->GetWindowBoundsInScreen().Contains(point));

  // Tests that we are still tucked after tapping that point.
  GetEventGenerator()->GestureTapAt(point);
  EXPECT_TRUE(float_controller->IsFloatedWindowTuckedForTablet(window.get()));
}

// Tests that the tuck handle tap target is larger than its bounds.
TEST_F(TabletWindowFloatTest, TuckHandleTapTarget) {
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);

  std::unique_ptr<aura::Window> window = CreateFloatedWindow();
  auto* float_controller = Shell::Get()->float_controller();

  // Tuck the window in the bottom right.
  FlingWindow(window.get(), /*left=*/false, /*up=*/false);
  ASSERT_TRUE(float_controller->IsFloatedWindowTuckedForTablet(window.get()));

  // Tap somewhere slightly outside the tuck handle widget. Verify that the
  // tucked window is untucked.
  const gfx::Rect tuck_handle_bounds =
      float_controller->GetTuckHandleWidget(window.get())
          ->GetWindowBoundsInScreen();
  GetEventGenerator()->GestureTapAt(tuck_handle_bounds.origin() -
                                    gfx::Vector2d(5, 5));
  EXPECT_FALSE(float_controller->IsFloatedWindowTuckedForTablet(window.get()));
}

// Tests that the tuck handle is offscreen in overview mode.
TEST_F(TabletWindowFloatTest, TuckHandleOffscreenInOverview) {
  const gfx::Rect display_bounds =
      display::Screen::GetScreen()->GetPrimaryDisplay().bounds();

  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  std::unique_ptr<aura::Window> window = CreateFloatedWindow();
  auto* float_controller = Shell::Get()->float_controller();

  // Tuck the window in the bottom right.
  FlingWindow(window.get(), /*left=*/false, /*up=*/false);
  ASSERT_TRUE(float_controller->IsFloatedWindowTuckedForTablet(window.get()));

  // The tuck handle widget is normally onscreen.
  views::Widget* tuck_handle_widget =
      float_controller->GetTuckHandleWidget(window.get());
  EXPECT_TRUE(
      display_bounds.Contains(tuck_handle_widget->GetWindowBoundsInScreen()));

  // Tests that on entering overview, the tuck handle is offscreen.
  EnterOverview();
  EXPECT_FALSE(
      display_bounds.Contains(tuck_handle_widget->GetWindowBoundsInScreen()));

  // Tests that on leaving overview, the tuck handle is onscreen again.
  ExitOverview();
  EXPECT_TRUE(
      display_bounds.Contains(tuck_handle_widget->GetWindowBoundsInScreen()));
}

TEST_F(TabletWindowFloatTest, UntuckWindowGestures) {
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  // The window is magnetized to the bottom right by default.
  std::unique_ptr<aura::Window> window = CreateFloatedWindow();
  auto* float_controller = Shell::Get()->float_controller();

  // Tuck the window in the bottom right.
  FlingWindow(window.get(), /*left=*/false, /*up=*/false);
  ASSERT_TRUE(float_controller->IsFloatedWindowTuckedForTablet(window.get()));

  // Swipe up on the handle. Test the window is still tucked.
  views::Widget* tuck_handle_widget =
      float_controller->GetTuckHandleWidget(window.get());
  const gfx::Point start(
      tuck_handle_widget->GetWindowBoundsInScreen().CenterPoint());
  GetEventGenerator()->GestureScrollSequence(
      start, start + gfx::Vector2d(0, -8), base::Milliseconds(100),
      /*steps=*/3);
  ASSERT_TRUE(float_controller->IsFloatedWindowTuckedForTablet(window.get()));
  EXPECT_EQ(0, user_action_tester_.GetActionCount(kUntuckUserAction));

  // Swipe left on the handle. Test that it untucks and magnetizes to the bottom
  // right.
  GetEventGenerator()->GestureScrollSequence(
      start, start + gfx::Vector2d(-8, 0), base::Milliseconds(100),
      /*steps=*/3);
  EXPECT_FALSE(float_controller->IsFloatedWindowTuckedForTablet(window.get()));
  CheckMagnetized(window.get(), FloatController::MagnetismCorner::kBottomRight);
  EXPECT_EQ(1, user_action_tester_.GetActionCount(kUntuckUserAction));
}

// Tests that flinging the window straight up or down does not tuck the window.
TEST_F(TabletWindowFloatTest, FlingVertical) {
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  // The window is magnetized to the bottom right by default.
  std::unique_ptr<aura::Window> window = CreateFloatedWindow();

  // Fling the window straight down. Test that it stays in the bottom right.
  const gfx::Point start =
      GetHeaderView(window.get())->GetBoundsInScreen().CenterPoint();
  GetEventGenerator()->GestureScrollSequence(
      start, start + gfx::Vector2d(0, 10), base::Milliseconds(10), /*steps=*/1);
  auto* float_controller = Shell::Get()->float_controller();
  ASSERT_FALSE(float_controller->IsFloatedWindowTuckedForTablet(window.get()));
  CheckMagnetized(window.get(), FloatController::MagnetismCorner::kBottomRight);
  EXPECT_EQ(0, user_action_tester_.GetActionCount(kTuckUserAction));

  // Fling the window straight up. Test that it moves to the top right.
  GetEventGenerator()->GestureScrollSequence(
      start, start + gfx::Vector2d(0, -10), base::Milliseconds(10),
      /*steps=*/1);
  ASSERT_FALSE(float_controller->IsFloatedWindowTuckedForTablet(window.get()));
  CheckMagnetized(window.get(), FloatController::MagnetismCorner::kTopRight);
  EXPECT_EQ(0, user_action_tester_.GetActionCount(kTuckUserAction));
}

// Tests that the tuck education nudge appears when the window is first floated.
TEST_F(TabletWindowFloatTest, BasicTuckNudge) {
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);

  std::unique_ptr<aura::Window> window = CreateAppWindow();

  // Add observer to check that the tuck education nudge was created.
  views::NamedWidgetShownWaiter widget_waiter(
      views::test::AnyWidgetTestPasskey{}, "TuckEducationNudgeWidget");

  // Float window using accelerator.
  PressAndReleaseKey(ui::VKEY_F, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN);
  ASSERT_TRUE(WindowState::Get(window.get())->IsFloated());

  // If waiter never sees the tuck education nudge, it will hang forever, and
  // the test will fail.
  widget_waiter.WaitIfNeededAndGet();

  // Nudge should dismiss properly after animations end.
  EXPECT_TRUE(window->children().empty());

  // TODO(hewer): Add a callback to check that the nudge has properly dismissed
  // after the bounce animations and timer have ended.
}

TEST_F(TabletWindowFloatTest, EducationPreferences) {
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  base::SimpleTestClock test_clock;
  TabletModeTuckEducation::SetOverrideClockForTesting(&test_clock);

  // Advance clock so we aren't at zero time.
  test_clock.Advance(base::Hours(25));

  NudgeCounter nudge_counter;

  // Float the nudge three times, count should increment each time.
  for (int i = 0; i < 3; i++) {
    std::unique_ptr<aura::Window> window = CreateAppWindow();

    // Float window using accelerator.
    PressAndReleaseKey(ui::VKEY_F, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN);
    ASSERT_TRUE(WindowState::Get(window.get())->IsFloated());

    // Close window and advance time so nudge can be shown again.
    window.reset();
    test_clock.Advance(base::Hours(25));
  }

  EXPECT_EQ(3, nudge_counter.nudge_count());

  // Float the window once more.
  std::unique_ptr<aura::Window> window = CreateAppWindow();
  PressAndReleaseKey(ui::VKEY_F, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN);
  ASSERT_TRUE(WindowState::Get(window.get())->IsFloated());
  window.reset();

  // Counter should not increment as nudge was not shown.
  EXPECT_EQ(3, nudge_counter.nudge_count());

  TabletModeTuckEducation::SetOverrideClockForTesting(nullptr);
}

using TabletWindowFloatSplitviewTest = TabletWindowFloatTest;

// Tests the expected behaviour when a window is floated when there are snapped
// windows on each side.
TEST_F(TabletWindowFloatSplitviewTest, BothSnappedToFloat) {
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);

  // Create two windows and snap one on each side.
  auto left_window = CreateAppWindow();
  const WindowSnapWMEvent snap_left(WM_EVENT_SNAP_PRIMARY);
  WindowState::Get(left_window.get())->OnWMEvent(&snap_left);

  auto right_window = CreateAppWindow();
  const WindowSnapWMEvent snap_right(WM_EVENT_SNAP_SECONDARY);
  WindowState::Get(right_window.get())->OnWMEvent(&snap_right);

  auto* split_view_controller =
      SplitViewController::Get(Shell::GetPrimaryRootWindow());
  ASSERT_EQ(split_view_controller->state(),
            SplitViewController::State::kBothSnapped);

  // Float the left window. Verify that it is floated, the right window becomes
  // maximized and that we are no longer in splitview.
  wm::ActivateWindow(left_window.get());
  PressAndReleaseKey(ui::VKEY_F, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN);
  EXPECT_TRUE(WindowState::Get(left_window.get())->IsFloated());
  EXPECT_TRUE(WindowState::Get(right_window.get())->IsMaximized());
  EXPECT_FALSE(split_view_controller->InSplitViewMode());
}

// Tests the expected behaviour when a window is floated then snapped.
TEST_F(TabletWindowFloatSplitviewTest, FloatToSnapped) {
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);

  std::unique_ptr<aura::Window> window = CreateFloatedWindow();

  auto* split_view_controller =
      SplitViewController::Get(Shell::GetPrimaryRootWindow());

  // If there are no other windows, expect to enter overview. The hotseat will
  // extended and users can pick a second app from there.
  const WindowSnapWMEvent snap_left(WM_EVENT_SNAP_PRIMARY);
  WindowState::Get(window.get())->OnWMEvent(&snap_left);
  ASSERT_TRUE(OverviewController::Get()->InOverviewSession());
  ASSERT_TRUE(split_view_controller->InSplitViewMode());

  // Float the window so we can snap it again. Assert that we are no longer in
  // overview or splitview.
  PressAndReleaseKey(ui::VKEY_F, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN);
  ASSERT_TRUE(WindowState::Get(window.get())->IsFloated());
  ASSERT_FALSE(OverviewController::Get()->InOverviewSession());
  ASSERT_FALSE(split_view_controller->InSplitViewMode());

  // Create a second window.
  auto other_window = CreateAppWindow();
  wm::ActivateWindow(window.get());

  // Tests that when we snap `window` now, `other_window` will get snapped to
  // the opposite side.
  WindowState::Get(window.get())->OnWMEvent(&snap_left);
  EXPECT_FALSE(OverviewController::Get()->InOverviewSession());
  EXPECT_EQ(split_view_controller->state(),
            SplitViewController::State::kBothSnapped);
  EXPECT_EQ(split_view_controller->primary_window(), window.get());
  EXPECT_EQ(split_view_controller->secondary_window(), other_window.get());

  // Tests that in overview mode, with at least one app window in overview, that
  // we also exit splitview and overview when floating the snapped window.
  ToggleOverview();
  wm::ActivateWindow(window.get());
  PressAndReleaseKey(ui::VKEY_F, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN);
  EXPECT_FALSE(OverviewController::Get()->InOverviewSession());
  EXPECT_FALSE(split_view_controller->InSplitViewMode());

  // Tests that when we partial-snap `other_window` now, activating `window`
  // will results in `window` snapped on the opposite side while keeping the
  // partial snap ratio.
  const WindowSnapWMEvent snap_primary_two_third(WM_EVENT_SNAP_PRIMARY,
                                                 chromeos::kTwoThirdSnapRatio);
  WindowState::Get(other_window.get())->OnWMEvent(&snap_primary_two_third);
  ASSERT_TRUE(WindowState::Get(other_window.get())->IsSnapped());

  wm::ActivateWindow(window.get());
  EXPECT_TRUE(WindowState::Get(window.get())->IsSnapped());
  EXPECT_EQ(split_view_controller->state(),
            SplitViewController::State::kBothSnapped);
  EXPECT_EQ(split_view_controller->primary_window(), other_window.get());
  EXPECT_EQ(split_view_controller->secondary_window(), window.get());
  EXPECT_NEAR(chromeos::kOneThirdSnapRatio,
              WindowState::Get(window.get())->snap_ratio().value(),
              /*abs_error=*/0.1);
  EXPECT_NEAR(chromeos::kTwoThirdSnapRatio,
              WindowState::Get(other_window.get())->snap_ratio().value(),
              /*abs_error=*/0.1);
}

// When reset a floated window that's previously snapped, maximize instead of
// snap.
TEST_F(TabletWindowFloatSplitviewTest, ResetFloatToMaximize) {
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);

  // Create two windows and snap one on each side.
  std::unique_ptr<aura::Window> window_1 = CreateAppWindow();
  std::unique_ptr<aura::Window> window_2 = CreateAppWindow();

  auto* split_view_controller =
      SplitViewController::Get(Shell::GetPrimaryRootWindow());

  const WindowSnapWMEvent snap_left(WM_EVENT_SNAP_PRIMARY);
  WindowState::Get(window_1.get())->OnWMEvent(&snap_left);
  const WindowSnapWMEvent snap_right(WM_EVENT_SNAP_SECONDARY);
  WindowState::Get(window_2.get())->OnWMEvent(&snap_right);
  EXPECT_EQ(split_view_controller->state(),
            SplitViewController::State::kBothSnapped);
  EXPECT_EQ(split_view_controller->primary_window(), window_1.get());
  EXPECT_EQ(split_view_controller->secondary_window(), window_2.get());

  // Float `window_1`, `window_2` should be maximized.
  wm::ActivateWindow(window_1.get());
  PressAndReleaseKey(ui::VKEY_F, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN);
  ASSERT_TRUE(WindowState::Get(window_1.get())->IsFloated());
  ASSERT_TRUE(WindowState::Get(window_2.get())->IsMaximized());

  // Float `window_2`, previously floated `window_1` should be maximized instead
  // of restoring back to snapped.
  wm::ActivateWindow(window_2.get());
  PressAndReleaseKey(ui::VKEY_F, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN);
  ASSERT_TRUE(WindowState::Get(window_2.get())->IsFloated());
  ASSERT_TRUE(WindowState::Get(window_1.get())->IsMaximized());
}

// Tests that there is no crash when going from float to always on top in tablet
// mode. Regression test for http://b/317064996.
TEST_F(TabletWindowFloatTest, FloatStateToAlwaysOnTop) {
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);

  std::unique_ptr<aura::Window> window = CreateFloatedWindow();

  // Make the window always on top. It should exit float state.
  window->SetProperty(aura::client::kZOrderingKey,
                      ui::ZOrderLevel::kFloatingWindow);
  EXPECT_FALSE(WindowState::Get(window.get())->IsFloated());
}

}  // namespace ash
