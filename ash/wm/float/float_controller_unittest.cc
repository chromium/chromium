// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/float/float_controller.h"

#include "ash/accelerators/accelerator_controller_impl.h"
#include "ash/constants/ash_features.h"
#include "ash/display/screen_orientation_controller_test_api.h"
#include "ash/frame/header_view.h"
#include "ash/frame/non_client_frame_view_ash.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/overview/overview_test_util.h"
#include "ash/wm/splitview/split_view_metrics_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_positioning_utils.h"
#include "ash/wm/window_state.h"
#include "ash/wm/work_area_insets.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ui/base/window_state_type.h"
#include "chromeos/ui/frame/immersive/immersive_fullscreen_controller.h"
#include "chromeos/ui/wm/features.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/base/hit_test.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/wm/core/window_util.h"

namespace ash {

class WindowFloatTest : public AshTestBase {
 public:
  WindowFloatTest() = default;

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

  bool IsFloatedWindowTucked() const {
    return !!Shell::Get()->float_controller()->scoped_window_tucker_;
  }

  void SetUp() override {
    // Ensure float feature is enabled.
    scoped_feature_list_.InitAndEnableFeature(
        chromeos::wm::features::kFloatWindow);
    AshTestBase::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Test float/unfloat window.
TEST_F(WindowFloatTest, WindowFloatingSwitch) {
  std::unique_ptr<aura::Window> window_1(CreateTestWindow());
  std::unique_ptr<aura::Window> window_2(CreateTestWindow());

  // Activate 'window_1' and perform floating.
  wm::ActivateWindow(window_1.get());
  PressAndReleaseKey(ui::VKEY_F, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN);
  EXPECT_TRUE(WindowState::Get(window_1.get())->IsFloated());

  // Activate 'window_2' and perform floating.
  wm::ActivateWindow(window_2.get());
  PressAndReleaseKey(ui::VKEY_F, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN);
  EXPECT_TRUE(WindowState::Get(window_2.get())->IsFloated());

  // Only one floated window is allowed so when a different window is floated,
  // the previously floated window will be unfloated.
  EXPECT_FALSE(WindowState::Get(window_1.get())->IsFloated());

  // When try to float the already floated 'window_2', it will unfloat this
  // window.
  wm::ActivateWindow(window_2.get());
  PressAndReleaseKey(ui::VKEY_F, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN);
  EXPECT_FALSE(WindowState::Get(window_2.get())->IsFloated());
}

// Tests that a floated window animates to and from overview.
TEST_F(WindowFloatTest, FloatWindowAnimatesInOverview) {
  std::unique_ptr<aura::Window> floated_window = CreateFloatedWindow();
  std::unique_ptr<aura::Window> maximized_window = CreateTestWindow();

  const WMEvent maximize_event(WM_EVENT_MAXIMIZE);
  WindowState::Get(maximized_window.get())->OnWMEvent(&maximize_event);

  // Activate 'maximized_window'. If the other window was not floated, then it
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
  UpdateDisplay("800x600");
  std::unique_ptr<views::Widget> widget = CreateTestWidget();
  widget->SetBounds(gfx::Rect(0, 0, 200, 200));
  FloatController* controller = Shell::Get()->float_controller();

  // Float Maximized window.
  widget->Maximize();
  PressAndReleaseKey(ui::VKEY_F, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN);
  EXPECT_TRUE(WindowState::Get(widget->GetNativeWindow())->IsFloated());
  gfx::Rect default_float_bounds =
      controller->GetPreferredFloatWindowClamshellBounds(
          widget->GetNativeWindow());
  EXPECT_EQ(widget->GetWindowBoundsInScreen(), default_float_bounds);
  // Unfloat.
  PressAndReleaseKey(ui::VKEY_F, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN);
  EXPECT_FALSE(WindowState::Get(widget->GetNativeWindow())->IsFloated());
  EXPECT_TRUE(widget->IsMaximized());

  // Float Full screen window.
  widget->SetFullscreen(true);
  PressAndReleaseKey(ui::VKEY_F, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN);
  EXPECT_TRUE(WindowState::Get(widget->GetNativeWindow())->IsFloated());
  EXPECT_EQ(widget->GetWindowBoundsInScreen(), default_float_bounds);
  // Unfloat.
  PressAndReleaseKey(ui::VKEY_F, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN);
  EXPECT_FALSE(WindowState::Get(widget->GetNativeWindow())->IsFloated());
  // TODO(crbug.com/1330999): This should return to Fullscreen state after
  // the change.

  // Minimize floated window.
  // Minimized window can't be floated, but when a floated window enter/exit
  // minimized state, it remains floated.
  PressAndReleaseKey(ui::VKEY_F, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN);
  EXPECT_TRUE(WindowState::Get(widget->GetNativeWindow())->IsFloated());
  gfx::Rect curr_bounds = widget->GetWindowBoundsInScreen();
  widget->Minimize();
  widget->Restore();
  EXPECT_EQ(widget->GetWindowBoundsInScreen(), curr_bounds);
  EXPECT_TRUE(WindowState::Get(widget->GetNativeWindow())->IsFloated());

  // Float Snapped window.
  // Create a snap enabled window.
  auto window = CreateAppWindow(default_float_bounds, AppType::BROWSER);
  AcceleratorControllerImpl* acc_controller =
      Shell::Get()->accelerator_controller();

  // Snap Left.
  acc_controller->PerformActionIfEnabled(WINDOW_CYCLE_SNAP_LEFT, {});
  ASSERT_EQ(chromeos::WindowStateType::kPrimarySnapped,
            WindowState::Get(window.get())->GetStateType());
  PressAndReleaseKey(ui::VKEY_F, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN);
  EXPECT_TRUE(WindowState::Get(window.get()));
  EXPECT_EQ(window->bounds(), default_float_bounds);
  // Unfloat.
  PressAndReleaseKey(ui::VKEY_F, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN);
  // Window back to snapped state.
  ASSERT_EQ(chromeos::WindowStateType::kPrimarySnapped,
            WindowState::Get(window.get())->GetStateType());

  // Snap Right.
  acc_controller->PerformActionIfEnabled(WINDOW_CYCLE_SNAP_RIGHT, {});
  ASSERT_EQ(chromeos::WindowStateType::kSecondarySnapped,
            WindowState::Get(window.get())->GetStateType());
  PressAndReleaseKey(ui::VKEY_F, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN);
  EXPECT_TRUE(WindowState::Get(window.get()));
  EXPECT_EQ(window->bounds(), default_float_bounds);
  // Unfloat.
  PressAndReleaseKey(ui::VKEY_F, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN);
  // Window back to snapped state.
  ASSERT_EQ(chromeos::WindowStateType::kSecondarySnapped,
            WindowState::Get(window.get())->GetStateType());
}

using TabletWindowFloatTest = WindowFloatTest;

TEST_F(TabletWindowFloatTest, TabletClamshellTransition) {
  auto window1 = CreateFloatedWindow();
  ASSERT_TRUE(WindowState::Get(window1.get())->IsFloated());

  // Test that on entering tablet mode, we maintain float state.
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  EXPECT_TRUE(WindowState::Get(window1.get())->IsFloated());

  // Create a new floated window in tablet mode. It should unfloat the existing
  // floated window.
  auto window2 = CreateFloatedWindow();
  EXPECT_FALSE(WindowState::Get(window1.get())->IsFloated());
  EXPECT_TRUE(WindowState::Get(window2.get())->IsFloated());

  // Test that on exiting tablet mode, we maintain float state.
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(false);
  EXPECT_TRUE(WindowState::Get(window2.get())->IsFloated());
}

// Tests that a window can be floated in tablet mode, unless its minimum width
// is greater than half the work area.
TEST_F(TabletWindowFloatTest, TabletPositioningLandscape) {
  UpdateDisplay("800x600");

  aura::test::TestWindowDelegate window_delegate;
  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithDelegate(
      &window_delegate, /*id=*/-1, gfx::Rect(300, 300)));
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

TEST_F(TabletWindowFloatTest, Rotation) {
  // Use a display where the width and height are quite different, otherwise it
  // would be hard to tell if portrait mode is using landscape bounds to
  // calculate floating window bounds.
  UpdateDisplay("1800x1000");

  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);

  std::unique_ptr<aura::Window> window = CreateFloatedWindow();
  const gfx::Rect no_rotation_bounds = window->bounds();

  // Set the primary display as the internal display so that the orientation
  // controller can rotate it.
  display::test::ScopedSetInternalDisplayId scoped_set_internal(
      Shell::Get()->display_manager(),
      display::Screen::GetScreen()->GetPrimaryDisplay().id());
  ScreenOrientationControllerTestApi orientation_test_api(
      Shell::Get()->screen_orientation_controller());

  // First rotate to landscape secondary orientation. The float bounds should
  // be the same.
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

// Tests that on drag release, the window sticks to one of the four corners of
// the work area.
TEST_F(TabletWindowFloatTest, DraggingMagnetism) {
  // Use a set display size so we can drag to specific spots.
  UpdateDisplay("1600x1000");

  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);

  std::unique_ptr<aura::Window> window = CreateFloatedWindow();

  // Exiting immersive mode because of float does not seem to trigger a layout
  // like it does in production code. Here we force a layout, otherwise the
  // client view will remain the size of the widget, and dragging it will give
  // us HTCLIENT.
  auto* frame = NonClientFrameViewAsh::Get(window.get());
  frame->Layout();

  const int padding = FloatController::kFloatWindowPaddingDp;
  const int shelf_size = ShelfConfig::Get()->shelf_size();

  // The default location is in the bottom right.
  EXPECT_EQ(gfx::Point(1600 - padding, 1000 - padding - shelf_size),
            window->bounds().bottom_right());

  HeaderView* header_view = frame->GetHeaderView();
  auto* event_generator = GetEventGenerator();
  event_generator->set_current_screen_location(
      header_view->GetBoundsInScreen().CenterPoint());
  event_generator->PressLeftButton();
  event_generator->MoveMouseTo(1590, 10);
  event_generator->ReleaseLeftButton();
  EXPECT_EQ(gfx::Point(1600 - padding, padding), window->bounds().top_right());

  // Move the mouse to somewhere in the top left. Test that on release, it
  // magnetizes to the top left.
  event_generator->set_current_screen_location(
      header_view->GetBoundsInScreen().CenterPoint());
  event_generator->PressLeftButton();
  event_generator->MoveMouseTo(10, 10);
  event_generator->ReleaseLeftButton();
  EXPECT_EQ(gfx::Point(padding, padding), window->bounds().origin());

  // Switch to portrait orientation and move the mouse somewhere in the bottom
  // left. Test that on release, it magentizes to the bottom left.
  UpdateDisplay("1000x1600");
  event_generator->set_current_screen_location(
      header_view->GetBoundsInScreen().CenterPoint());
  event_generator->PressLeftButton();
  event_generator->MoveMouseTo(10, 1590);
  event_generator->ReleaseLeftButton();
  EXPECT_EQ(gfx::Point(padding, 1600 - shelf_size - padding),
            window->bounds().bottom_left());
}

// Tests the functionality of tucking a window in tablet mode. Tucking a window
// is hiding partially offscreen to the side.
TEST_F(TabletWindowFloatTest, TuckedWindow) {
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);

  std::unique_ptr<aura::Window> window = CreateFloatedWindow();

  // Exiting immersive mode because of float does not seem to trigger a layout
  // like it does in production code. Here we force a layout, otherwise the
  // client view will remain the size of the widget, and dragging it will give
  // us HTCLIENT.
  auto* frame = NonClientFrameViewAsh::Get(window.get());
  frame->Layout();

  // Generate a fling to the top left corner. Tests that the window is tucked,
  // and 100 pixels are visible to the user.
  const gfx::Point header_center =
      frame->GetHeaderView()->GetBoundsInScreen().CenterPoint();
  GetEventGenerator()->GestureScrollSequence(
      header_center, header_center - gfx::Vector2d(10, 10),
      base::Milliseconds(10), /*steps=*/2);
  EXPECT_TRUE(IsFloatedWindowTucked());
  EXPECT_EQ(100, window->bounds().right());

  // Tests that after we exit tablet mode, the window is untucked and fully
  // visible, but is still floated.
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(false);
  EXPECT_TRUE(WindowState::Get(window.get())->IsFloated());
  EXPECT_TRUE(screen_util::GetDisplayBoundsInParent(window.get())
                  .Contains(window->bounds()));
}

}  // namespace ash
