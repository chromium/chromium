// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/pip/pip_controller.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/collision_detection/collision_detection_utils.h"
#include "ash/wm/scoped_window_tucker.h"
#include "ash/wm/test/fake_window_state.h"
#include "ash/wm/test/test_non_client_frame_view_ash.h"
#include "ash/wm/window_dimmer.h"
#include "ash/wm/window_state.h"
#include "ash/wm/wm_event.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ui/base/window_state_type.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/wm/core/window_util.h"

namespace ash {

using ::chromeos::WindowStateType;

namespace {

constexpr int kPipWidth = 300;
constexpr int kPipHeight = 200;
constexpr gfx::Size kMaxWindowSize(600, 600);
constexpr gfx::Size kMinWindowSize(200, 100);
constexpr gfx::SizeF kWindowAspectRatio(3.f, 2.f);

// Max and default size of the PiP window preserving the aspect ratio.
constexpr gfx::Size kExpectedPipMaxSize(600, 400);
constexpr gfx::Size kExpectedPipDefaultSize(200, 133);

}  // namespace

class PipControllerTest : public AshTestBase {
 public:
  PipControllerTest() : scoped_feature_list_(features::kPipTuck) {}

  PipControllerTest(const PipControllerTest&) = delete;
  PipControllerTest& operator=(const PipControllerTest&) = delete;
  ~PipControllerTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    UpdateDisplay("1000x800");
  }

 protected:
  std::unique_ptr<aura::Window> CreatePipWindow(gfx::Rect bounds) {
    std::unique_ptr<aura::Window> window(CreateTestWindow(bounds));
    WindowState* window_state = WindowState::Get(window.get());
    const WMEvent enter_pip(WM_EVENT_PIP);
    window_state->OnWMEvent(&enter_pip);

    EXPECT_TRUE(window_state->IsPip());
    window.get()->Show();
    return window;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class PipControllerTestAPI {
 public:
  PipControllerTestAPI(PipController* controller) : controller_(controller) {}

  WindowDimmer* dimmer() { return controller_->dimmer_.get(); }

  ScopedWindowTucker* scoped_window_tucker() {
    return controller_->scoped_window_tucker_.get();
  }

 private:
  const raw_ptr<PipController> controller_;
};

TEST_F(PipControllerTest, WMEventPipSetsTargetWindow) {
  const gfx::Rect bounds_in_screen(100, 100, kPipWidth, kPipHeight);
  std::unique_ptr<aura::Window> window(CreatePipWindow(bounds_in_screen));

  // `OnWMEvent` in `CreatePipWindow()` should set the target window to
  // the controller.
  PipController* controller = Shell::Get()->pip_controller();
  EXPECT_EQ(controller->pip_window(), window.get());
}

TEST_F(PipControllerTest, UpdatePipBounds) {
  const gfx::Rect bounds_in_screen(100, 100, kPipWidth, kPipHeight);
  std::unique_ptr<aura::Window> window(CreatePipWindow(bounds_in_screen));

  // `CreatePipWindow()`'s `OnWMEvent()` ends up calling
  // `UpdatePipBounds()` which should move the PiP window to its resting
  // position.
  gfx::Rect expected_bounds = CollisionDetectionUtils::GetRestingPosition(
      WindowState::Get(window.get())->GetDisplay(), bounds_in_screen,
      CollisionDetectionUtils::RelativePriority::kPictureInPicture);
  EXPECT_EQ(expected_bounds, window->bounds());
}

TEST_F(PipControllerTest, TuckCreatesWindowTucker) {
  PipController* controller = Shell::Get()->pip_controller();
  PipControllerTestAPI test_api(controller);

  const gfx::Rect bounds(0, 0, kPipWidth, kPipHeight);
  std::unique_ptr<aura::Window> window(CreatePipWindow(bounds));

  EXPECT_EQ(window.get(), controller->pip_window());

  // Tucking the window should create the `ScopedWindowTucker`.
  controller->TuckWindow(/*left=*/true);
  EXPECT_TRUE(test_api.scoped_window_tucker());

  // Untucking the window should destroy the `ScopedWindowTucker`.
  controller->UntuckWindow();
  EXPECT_FALSE(test_api.scoped_window_tucker());
}

TEST_F(PipControllerTest, TuckAndUntuckChangesWindowBounds) {
  const gfx::Rect bounds(0, 0, kPipWidth, kPipHeight);
  std::unique_ptr<aura::Window> window(CreatePipWindow(bounds));

  PipController* controller = Shell::Get()->pip_controller();
  EXPECT_EQ(window.get(), controller->pip_window());

  // Tuck the PiP window to the left.
  controller->TuckWindow(/*left=*/true);
  EXPECT_TRUE(controller->is_tucked());
  EXPECT_EQ(window->bounds(),
            gfx::Rect(ScopedWindowTucker::kTuckHandleWidth - bounds.width(),
                      kCollisionWindowWorkAreaInsetsDp, kPipWidth, kPipHeight));

  // Untuck from the left.
  controller->UntuckWindow();
  EXPECT_FALSE(controller->is_tucked());
  EXPECT_EQ(window->bounds(),
            gfx::Rect(kCollisionWindowWorkAreaInsetsDp,
                      kCollisionWindowWorkAreaInsetsDp, kPipWidth, kPipHeight));

  // Tuck the PiP window to the right.
  controller->TuckWindow(/*left=*/false);
  EXPECT_TRUE(controller->is_tucked());
  EXPECT_EQ(window->bounds(),
            gfx::Rect(1000 - ScopedWindowTucker::kTuckHandleWidth,
                      kCollisionWindowWorkAreaInsetsDp, kPipWidth, kPipHeight));

  // Untuck from the right.
  controller->UntuckWindow();
  EXPECT_FALSE(controller->is_tucked());
  EXPECT_EQ(window->bounds(),
            gfx::Rect(1000 - kCollisionWindowWorkAreaInsetsDp - bounds.width(),
                      kCollisionWindowWorkAreaInsetsDp, kPipWidth, kPipHeight));
}

TEST_F(PipControllerTest, TuckDimsWindow) {
  PipController* controller = Shell::Get()->pip_controller();
  PipControllerTestAPI test_api(controller);

  const gfx::Rect bounds(0, 0, kPipWidth, kPipHeight);
  std::unique_ptr<aura::Window> window(CreatePipWindow(bounds));

  // Tuck the window and confirm that the dimmer is shown.
  controller->TuckWindow(/*left=*/true);
  EXPECT_NE(test_api.dimmer(), nullptr);
  EXPECT_TRUE(test_api.dimmer()->window()->IsVisible());
  EXPECT_EQ(test_api.dimmer()->window()->layer()->opacity(), 1.f);

  // Untuck the window and confirm that the dimmer's opacity is now 0.
  controller->UntuckWindow();
  EXPECT_EQ(test_api.dimmer()->window()->layer()->opacity(), 0.f);
}

TEST_F(PipControllerTest, TuckHandleIsShownAtCorrectPosition) {
  PipController* controller = Shell::Get()->pip_controller();
  PipControllerTestAPI test_api(controller);

  const gfx::Rect pip_bounds(kCollisionWindowWorkAreaInsetsDp,
                             kCollisionWindowWorkAreaInsetsDp, kPipWidth,
                             kPipHeight);
  std::unique_ptr<aura::Window> window(CreatePipWindow(pip_bounds));

  // Tuck the window and confirm that the tuck handle is shown at the correct
  // position.
  controller->TuckWindow(/*left=*/true);
  EXPECT_TRUE(test_api.scoped_window_tucker());
  EXPECT_TRUE(test_api.scoped_window_tucker()->tuck_handle_widget());
  gfx::Rect tuck_handle_bounds(
      0,
      kCollisionWindowWorkAreaInsetsDp +
          (kPipHeight - ScopedWindowTucker::kTuckHandleHeight) / 2,
      ScopedWindowTucker::kTuckHandleWidth,
      ScopedWindowTucker::kTuckHandleHeight);
  EXPECT_EQ(tuck_handle_bounds, test_api.scoped_window_tucker()
                                    ->tuck_handle_widget()
                                    ->GetWindowBoundsInScreen());

  // Move PiP and confirm that the tuck handle follows.
  gfx::Rect tucked_pip_bounds = window->GetBoundsInScreen();
  tucked_pip_bounds.Offset(0, 100);
  SetBoundsWMEvent event(tucked_pip_bounds, /*animate=*/false);
  WindowState* window_state = WindowState::Get(window.get());
  window_state->OnWMEvent(&event);
  tuck_handle_bounds.Offset(0, 100);
  EXPECT_EQ(tuck_handle_bounds, test_api.scoped_window_tucker()
                                    ->tuck_handle_widget()
                                    ->GetWindowBoundsInScreen());

  // Untuck the window and confirm that the tuck handle is gone.
  controller->UntuckWindow();
  EXPECT_FALSE(test_api.scoped_window_tucker());
}

class PipToggleResizeFeatureTest : public AshTestBase,
                                   public testing::WithParamInterface<bool> {
 public:
  PipToggleResizeFeatureTest()
      : scoped_feature_list_(features::kPipDoubleTapToResize) {}

  PipToggleResizeFeatureTest(const PipToggleResizeFeatureTest&) = delete;
  PipToggleResizeFeatureTest& operator=(const PipToggleResizeFeatureTest&) =
      delete;

  ~PipToggleResizeFeatureTest() override = default;

  std::unique_ptr<aura::Window> CreateAppWindow(
      const gfx::Rect& bounds,
      WindowStateType window_state_type) {
    auto window = AshTestBase::CreateAppWindow(
        bounds, chromeos::AppType::SYSTEM_APP, kShellWindowId_DeskContainerA,
        new TestWidgetDelegateAsh);
    Shell::Get()->pip_controller()->SetPipWindow(window.get());

    auto* custom_frame = static_cast<TestNonClientFrameViewAsh*>(
        NonClientFrameViewAsh::Get(window.get()));

    custom_frame->SetMaximumSize(kMaxWindowSize);
    custom_frame->SetMinimumSize(kMinWindowSize);

    WindowState::Get(window.get())
        ->SetStateObject(std::make_unique<FakeWindowState>(window_state_type));
    window->SetProperty(aura::client::kAspectRatio, kWindowAspectRatio);
    return window;
  }

  gfx::Rect GetFakeWindowStateLastBounds(WindowState* window_state) {
    return static_cast<FakeWindowState*>(
               WindowState::TestApi::GetStateImpl(window_state))
        ->last_requested_bounds();
  }

  void DoubleTapWindowCenter(aura::Window* window) {
    auto* event_generator = GetEventGenerator();
    bool is_mouse_click = GetParam();
    gfx::Point center = window->GetBoundsInScreen().CenterPoint();

    if (is_mouse_click) {
      event_generator->set_current_screen_location(center);
      event_generator->DoubleClickLeftButton();
    } else {  // gesture tap.
      event_generator->GestureTapDownAndUp(center);
      event_generator->GestureTapDownAndUp(center);
    }
  }

  void PressHotKey(aura::Window* window) {
    auto* event_generator = GetEventGenerator();

    event_generator->PressKeyAndModifierKeys(ui::VKEY_X, ui::EF_COMMAND_DOWN);
    event_generator->ReleaseKeyAndModifierKeys(ui::VKEY_X, ui::EF_COMMAND_DOWN);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_P(PipToggleResizeFeatureTest, TestSingleClick) {
  // Tests that single clicking a normal window or a PiP window does not change
  // the bounds.
  for (WindowStateType state_type :
       {WindowStateType::kNormal, WindowStateType::kPip}) {
    std::ostringstream stream;
    stream << "Testing state: " << state_type;
    SCOPED_TRACE(stream.str());

    auto window =
        CreateAppWindow(gfx::Rect(kExpectedPipDefaultSize), state_type);
    auto* event_generator = GetEventGenerator();

    if (GetParam()) {  // Mouse click.
      event_generator->set_current_screen_location(
          window->GetBoundsInScreen().CenterPoint());
      event_generator->ClickLeftButton();
    } else {  // Gesture tap.
      event_generator->GestureTapDownAndUp(
          window->GetBoundsInScreen().CenterPoint());
    }

    EXPECT_FALSE(WindowState::Get(window.get())->bounds_changed_by_user());
  }
}

TEST_P(PipToggleResizeFeatureTest, TestDoubleClickWithPip) {
  auto window = CreateAppWindow(gfx::Rect(kExpectedPipDefaultSize),
                                WindowStateType::kPip);

  DoubleTapWindowCenter(window.get());

  EXPECT_TRUE(WindowState::Get(window.get())->bounds_changed_by_user());
}

TEST_P(PipToggleResizeFeatureTest, TestDoubleClickAtMaxSize) {
  auto window =
      CreateAppWindow(gfx::Rect(kExpectedPipMaxSize), WindowStateType::kPip);

  DoubleTapWindowCenter(window.get());

  auto* window_state = WindowState::Get(window.get());

  EXPECT_TRUE(window_state->bounds_changed_by_user());
  EXPECT_EQ(GetFakeWindowStateLastBounds(window_state),
            gfx::Rect(kExpectedPipDefaultSize));
}

TEST_P(PipToggleResizeFeatureTest, TestDoubleClickAtNonMaxSize) {
  auto window = CreateAppWindow(gfx::Rect(300, 300), WindowStateType::kPip);

  DoubleTapWindowCenter(window.get());

  auto* window_state = WindowState::Get(window.get());

  EXPECT_TRUE(window_state->bounds_changed_by_user());
  // Expect it to go to max size on the first hotkey press.
  EXPECT_EQ(GetFakeWindowStateLastBounds(window_state),
            gfx::Rect(kExpectedPipMaxSize));

  window =
      CreateAppWindow(gfx::Rect(kExpectedPipMaxSize), WindowStateType::kPip);
  window_state = WindowState::Get(window.get());

  // Check when re-double clicking/tapping, if the window goes back to the last
  // user-defined size.
  DoubleTapWindowCenter(window.get());
  EXPECT_TRUE(window_state->bounds_changed_by_user());
  EXPECT_EQ(GetFakeWindowStateLastBounds(window_state), gfx::Rect(300, 300));
}

TEST_P(PipToggleResizeFeatureTest, TestHotkeyWithPipWindow) {
  auto window = CreateAppWindow(gfx::Rect(300, 300), WindowStateType::kPip);

  PressHotKey(window.get());

  auto* window_state = WindowState::Get(window.get());

  EXPECT_TRUE(window_state->bounds_changed_by_user());
}

TEST_P(PipToggleResizeFeatureTest, TestHotKeyToggleTheSizeOfThePipWindow) {
  auto window = CreateAppWindow(gfx::Rect(300, 300), WindowStateType::kPip);

  PressHotKey(window.get());

  auto* window_state = WindowState::Get(window.get());

  EXPECT_TRUE(window_state->bounds_changed_by_user());
  // Expect it to go to max size on the first double-click/tap.
  EXPECT_EQ(GetFakeWindowStateLastBounds(window_state),
            gfx::Rect(kExpectedPipMaxSize));

  window =
      CreateAppWindow(gfx::Rect(kExpectedPipMaxSize), WindowStateType::kPip);
  window_state = WindowState::Get(window.get());

  // Check when re-pressing the hotkey, if the window goes back to the last
  // user-defined size.
  PressHotKey(window.get());
  EXPECT_TRUE(window_state->bounds_changed_by_user());
  EXPECT_EQ(GetFakeWindowStateLastBounds(window_state), gfx::Rect(300, 300));
}

// Assume true is for mouse clicks and false is for gesture taps.
INSTANTIATE_TEST_SUITE_P(All, PipToggleResizeFeatureTest, testing::Bool());

}  // namespace ash
