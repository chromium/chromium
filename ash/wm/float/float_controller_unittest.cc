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
#include "ash/wm/desks/desks_bar_view.h"
#include "ash/wm/desks/desks_test_api.h"
#include "ash/wm/desks/desks_test_util.h"
#include "ash/wm/float/tablet_mode_float_window_resizer.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_item.h"
#include "ash/wm/overview/overview_test_util.h"
#include "ash/wm/splitview/split_view_metrics_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_positioning_utils.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ash/wm/work_area_insets.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ui/base/window_state_type.h"
#include "chromeos/ui/frame/header_view.h"
#include "chromeos/ui/frame/immersive/immersive_fullscreen_controller.h"
#include "chromeos/ui/wm/constants.h"
#include "chromeos/ui/wm/features.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/test/test_window_delegate.h"
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
#include "ui/wm/core/window_util.h"

namespace ash {

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
    DCHECK(WindowState::Get(floated_window.get())->IsFloated());
    return floated_window;
  }

  void SetUp() override {
    // Enable float feature and desks close all feature.
    scoped_feature_list_.InitWithFeatures(
        {chromeos::wm::features::kFloatWindow}, {features::kDesksCloseAll});
    AshTestBase::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
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
      FloatController::GetPreferredFloatWindowClamshellBounds(window.get());
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
  default_float_bounds =
      FloatController::GetPreferredFloatWindowClamshellBounds(window.get());
  EXPECT_EQ(default_float_bounds, window->GetBoundsInScreen());
  // Unfloat.
  PressAndReleaseKey(ui::VKEY_F, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN);
  EXPECT_FALSE(window_state->IsFloated());
  EXPECT_TRUE(window_state->IsFullscreen());
  // Unfullscreen.
  window_state->OnWMEvent(&toggle_fullscreen_event);
  EXPECT_TRUE(window_state->IsFloated());
  EXPECT_FALSE(window_state->IsFullscreen());

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
  acc_controller->PerformActionIfEnabled(WINDOW_CYCLE_SNAP_LEFT, {});
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
  acc_controller->PerformActionIfEnabled(WINDOW_CYCLE_SNAP_RIGHT, {});
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
  auto* frame = NonClientFrameViewAsh::Get(window.get());
  chromeos::HeaderView* header_view = frame->GetHeaderView();
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
  ASSERT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());
  RemoveDesk(desk_2, DeskCloseType::kCloseAllWindowsAndWait);
  ASSERT_TRUE(desk_2->is_desk_being_removed());
  // During desk removal, float window should be hidden.
  EXPECT_FALSE(window->IsVisible());
  // When `desk_2` is restored the floated window should remain floated and
  // shown.
  views::LabelButton* dismiss_button =
      DesksTestApi::GetCloseAllUndoToastDismissButton();
  AshTestBase::LeftClickOn(dismiss_button);
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
  auto* overview_controller = Shell::Get()->overview_controller();
  EnterOverview();
  auto* overview_session = overview_controller->overview_session();
  // The window should exist on the grid of the first display.
  auto* overview_item =
      overview_session->GetOverviewItemForWindow(window_1.get());
  auto* grid =
      overview_session->GetGridWithRootWindow(Shell::GetPrimaryRootWindow());
  EXPECT_EQ(1u, grid->size());
  // Get position of `desk_2`'s desk mini view on the secondary display.
  const auto* desks_bar_view = grid->desks_bar_view();
  auto* desk_2_mini_view = desks_bar_view->mini_views()[1];
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
  auto* overview_controller = Shell::Get()->overview_controller();
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
  EXPECT_EQ(1u, grid1->size());
  EXPECT_EQ(grid1, overview_item->overview_grid());
  EXPECT_EQ(0u, grid2->size());

  // Get position of `desk_2`'s desk mini view on the secondary display.
  const auto* desks_bar_view = grid2->desks_bar_view();
  auto* desk_2_mini_view = desks_bar_view->mini_views()[1];
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
  auto* overview_controller = Shell::Get()->overview_controller();
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
  ASSERT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());
  RemoveDesk(desk_1, DeskCloseType::kCombineDesks);
  ASSERT_EQ(desks_controller->desks().size(), 1u);
  // Floated window should be appended to overview items.
  const std::vector<std::unique_ptr<OverviewItem>>& overview_items =
      GetOverviewItemsForRoot(0);
  ASSERT_EQ(overview_items.size(), 1u);
  EXPECT_EQ(window.get(), overview_items[0]->GetWindow());
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
  EXPECT_EQ(Shell::Get()->float_controller()->floated_window_counter_, 3);
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
  auto* overview_session =
      Shell::Get()->overview_controller()->overview_session();
  // The window should exist on the grid of the first display.
  auto* overview_item =
      overview_session->GetOverviewItemForWindow(window_1.get());
  auto* grid =
      overview_session->GetGridWithRootWindow(Shell::GetPrimaryRootWindow());
  EXPECT_EQ(1u, grid->size());
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
  EXPECT_EQ(float_controller->floated_window_move_to_another_desk_counter_, 1);
  // Move to `desk_2` and remove `desk_2` by combine 2 desks.
  // Check total counts, it should count 2.
  ActivateDesk(desk_2);
  RemoveDesk(desk_2, DeskCloseType::kCombineDesks);
  EXPECT_EQ(float_controller->floated_window_move_to_another_desk_counter_, 2);
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
    // Drag to a point outside of `kScreenEdgeInsetForSnap` from the edge of the
    // screen to avoid snapping.
    gfx::Rect area = WorkAreaInsets::ForWindow(window)->user_work_area_bounds();
    area.Inset(kScreenEdgeInsetForSnap + 5);

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

 private:
  // Called when the tablet state changes.
  base::RepeatingCallback<void(display::TabletState)>
      on_tablet_state_changed_callback_;

  absl::optional<display::ScopedDisplayObserver> display_observer_;
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
  aura::test::TestWindowDelegate window_delegate;
  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithDelegate(
      &window_delegate, /*id=*/-1, gfx::Rect(500, 500)));
  window->SetProperty(aura::client::kAppType,
                      static_cast<int>(AppType::BROWSER));
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
            window_delegate.set_minimum_size(tablet_minimum_size);
            return;
          case display::TabletState::kInClamshellMode:
            window_delegate.set_minimum_size(gfx::Size());
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
}

// Tests that a window can be floated in tablet mode, unless its minimum width
// is greater than half the work area.
TEST_F(TabletWindowFloatTest, TabletPositioningLandscape) {
  UpdateDisplay("800x600");

  aura::test::TestWindowDelegate window_delegate;
  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithDelegate(
      &window_delegate, /*id=*/-1, gfx::Rect(300, 300)));
  window->SetProperty(aura::client::kAppType,
                      static_cast<int>(AppType::BROWSER));
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
  window->SetProperty(aura::client::kAppType,
                      static_cast<int>(ash::AppType::BROWSER));
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
  window->SetProperty(aura::client::kAppType,
                      static_cast<int>(ash::AppType::BROWSER));
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

// Tests that if we minimize a floated window, it is floated upon unminimizing.
TEST_F(TabletWindowFloatTest, UnminimizeFloatWindow) {
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);

  std::unique_ptr<aura::Window> window = CreateFloatedWindow();
  auto* window_state = WindowState::Get(window.get());

  window_state->Minimize();
  window_state->Unminimize();
  EXPECT_TRUE(window_state->IsFloated());
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

// TODO(sammiequon): Update dragging to use touch instead of mouse.

// Tests that dragged float window follows the mouse/touch. Regression test for
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
  // point.
  for (const gfx::Point& point :
       {gfx::Point(10, 10), gfx::Point(100, 10), gfx::Point(400, 400)}) {
    event_generator->MoveTouch(point);
    EXPECT_EQ(point, header_view->GetBoundsInScreen().CenterPoint());
  }
}

// Tests that there is no crash when maximizing a dragged floated window.
// Regression test for https://b/254107825.
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

  // Move the mouse somewhere in the top right, but not too right that it falls
  // into the snap region. Test that on release, it magnetizes to the top right
  MagnetizeWindow(window.get(), FloatController::MagnetismCorner::kTopRight);
  CheckMagnetized(window.get(), FloatController::MagnetismCorner::kTopRight);

  // Move the mouse to somewhere in the top left, but not too left that it falls
  // into the snap region. Test that on release, it magnetizes to the top left.
  MagnetizeWindow(window.get(), FloatController::MagnetismCorner::kTopLeft);
  CheckMagnetized(window.get(), FloatController::MagnetismCorner::kTopLeft);

  // Switch to portrait orientation and move the mouse somewhere in the bottom
  // left, but not too bottom that it falls into the snap region. Test that on
  // release, it magentizes to the bottom left.
  UpdateDisplay("1000x1600");
  MagnetizeWindow(window.get(), FloatController::MagnetismCorner::kBottomLeft);
  CheckMagnetized(window.get(), FloatController::MagnetismCorner::kBottomLeft);
}

// Tests that if a floating window is dragged to the edges, it will snap.
TEST_F(TabletWindowFloatTest, DraggingSnapping) {
  // Use a set display size so we can drag to specific spots.
  UpdateDisplay("1600x1000");

  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);

  std::unique_ptr<aura::Window> window = CreateFloatedWindow();

  auto* split_view_controller =
      SplitViewController::Get(Shell::GetPrimaryRootWindow());
  ASSERT_FALSE(split_view_controller->primary_window());
  ASSERT_FALSE(split_view_controller->secondary_window());

  // Move the mouse to towards the right edge. Test that on release, it snaps
  // right.
  chromeos::HeaderView* header_view = GetHeaderView(window.get());
  auto* event_generator = GetEventGenerator();
  event_generator->set_current_screen_location(
      header_view->GetBoundsInScreen().CenterPoint());
  event_generator->DragMouseTo(1580, 500);
  EXPECT_EQ(split_view_controller->secondary_window(), window.get());
  ASSERT_TRUE(WindowState::Get(window.get())->IsSnapped());

  // Float the window so we can drag it again.
  PressAndReleaseKey(ui::VKEY_F, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN);
  ASSERT_TRUE(WindowState::Get(window.get())->IsFloated());

  // Move the mouse to towards the left edge. Test that on release, it snaps
  // left.
  event_generator->set_current_screen_location(
      header_view->GetBoundsInScreen().CenterPoint());
  event_generator->DragMouseTo(20, 500);
  EXPECT_EQ(split_view_controller->primary_window(), window.get());
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
  GetEventGenerator()->GestureTapAt(
      tuck_handle_widget->GetWindowBoundsInScreen().CenterPoint());
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
  GetEventGenerator()->GestureTapAt(
      tuck_handle_widget->GetWindowBoundsInScreen().CenterPoint());
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

  // Fling the window left and up. Test that it tucks in the top left.
  FlingWindow(window.get(), /*left=*/true, /*up=*/true);
  ASSERT_TRUE(float_controller->IsFloatedWindowTuckedForTablet(window.get()));
  const int padding = chromeos::wm::kFloatedWindowPaddingDp;
  EXPECT_EQ(gfx::Point(0, padding), window->bounds().top_right());

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

  // Fling the window left and down. Test that it tucks in the bottom left.
  FlingWindow(window.get(), /*left=*/true, /*up=*/false);
  ASSERT_TRUE(float_controller->IsFloatedWindowTuckedForTablet(window.get()));
  const gfx::Rect work_area = WorkAreaInsets::ForWindow(window->GetRootWindow())
                                  ->user_work_area_bounds();
  EXPECT_EQ(gfx::Point(0, work_area.bottom() - padding),
            window->bounds().bottom_right());

  // Untuck the window. Test that it magnetizes to the bottom left.
  tuck_handle_widget = float_controller->GetTuckHandleWidget(window.get());
  GestureTapOn(tuck_handle_widget->GetContentsView());
  ASSERT_FALSE(float_controller->IsFloatedWindowTuckedForTablet(window.get()));
  CheckMagnetized(window.get(), FloatController::MagnetismCorner::kBottomLeft);
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

  // Fling the window right and up. Test that it tucks in the top right.
  FlingWindow(window.get(), /*left=*/false, /*up=*/true);
  ASSERT_TRUE(float_controller->IsFloatedWindowTuckedForTablet(window.get()));
  const int padding = chromeos::wm::kFloatedWindowPaddingDp;
  EXPECT_EQ(gfx::Point(work_area.right(), padding), window->bounds().origin());

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

  // Fling the window right and down. Test that it tucks in the bottom right.
  FlingWindow(window.get(), /*left=*/false, /*up=*/false);
  ASSERT_TRUE(float_controller->IsFloatedWindowTuckedForTablet(window.get()));
  EXPECT_EQ(gfx::Point(work_area.right(), work_area.bottom() - padding),
            window->bounds().bottom_left());

  // Untuck the window. Test that it magnetizes to the bottom right.
  tuck_handle_widget = float_controller->GetTuckHandleWidget(window.get());
  GestureTapOn(tuck_handle_widget->GetContentsView());
  ASSERT_FALSE(float_controller->IsFloatedWindowTuckedForTablet(window.get()));
  CheckMagnetized(window.get(), FloatController::MagnetismCorner::kBottomRight);
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
TEST_F(TabletWindowFloatTest, ClickOnEdgeDoesNotUntuck) {
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

  // Swipe left on the handle. Test that it untucks and magnetizes to the bottom
  // right.
  GetEventGenerator()->GestureScrollSequence(
      start, start + gfx::Vector2d(-8, 0), base::Milliseconds(100),
      /*steps=*/3);
  EXPECT_FALSE(float_controller->IsFloatedWindowTuckedForTablet(window.get()));
  CheckMagnetized(window.get(), FloatController::MagnetismCorner::kBottomRight);
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

  // Fling the window straight up. Test that it moves to the top right.
  GetEventGenerator()->GestureScrollSequence(
      start, start + gfx::Vector2d(0, -10), base::Milliseconds(10),
      /*steps=*/1);
  ASSERT_FALSE(float_controller->IsFloatedWindowTuckedForTablet(window.get()));
  CheckMagnetized(window.get(), FloatController::MagnetismCorner::kTopRight);
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
  ASSERT_TRUE(split_view_controller->BothSnapped());

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
  ASSERT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());
  ASSERT_TRUE(split_view_controller->InSplitViewMode());

  // Float the window so we can snap it again. Assert that we are still in
  // overview, but no longer in splitview.
  PressAndReleaseKey(ui::VKEY_F, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN);
  ASSERT_TRUE(WindowState::Get(window.get())->IsFloated());
  ASSERT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());
  ASSERT_FALSE(split_view_controller->InSplitViewMode());

  // Create a second window.
  auto other_window = CreateAppWindow();
  wm::ActivateWindow(window.get());

  // Tests that when we snap `window` now, `other_window` will get snapped to
  // the opposite side.
  WindowState::Get(window.get())->OnWMEvent(&snap_left);
  EXPECT_FALSE(Shell::Get()->overview_controller()->InOverviewSession());
  EXPECT_TRUE(split_view_controller->BothSnapped());
  EXPECT_EQ(split_view_controller->primary_window(), window.get());
  EXPECT_EQ(split_view_controller->secondary_window(), other_window.get());
}

}  // namespace ash
