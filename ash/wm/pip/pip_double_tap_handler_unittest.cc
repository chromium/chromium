// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/pip/pip_double_tap_handler.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/test/fake_window_state.h"
#include "ash/wm/test/test_non_client_frame_view_ash.h"
#include "ash/wm/window_state.h"
#include "ash/wm/wm_event.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ui/base/window_state_type.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace ash {

using ::chromeos::WindowStateType;

namespace {

constexpr gfx::Size kMaxWindowSize(600, 600);
constexpr gfx::Size kMinWindowSize(200, 100);
constexpr gfx::SizeF kWindowAspectRatio(3.f, 2.f);

// Max and default size of the PiP window preserving the aspect ratio.
constexpr gfx::Size kExpectedPipMaxSize(600, 400);
constexpr gfx::Size kExpectedPipDefaultSize(200, 133);

}  // namespace

class PipDoubleTapHandlerTest : public AshTestBase,
                                public testing::WithParamInterface<bool> {
 public:
  PipDoubleTapHandlerTest()
      : scoped_feature_list_(features::kPipDoubleTapToResize) {}

  PipDoubleTapHandlerTest(const PipDoubleTapHandlerTest&) = delete;
  PipDoubleTapHandlerTest& operator=(const PipDoubleTapHandlerTest&) = delete;

  ~PipDoubleTapHandlerTest() override = default;

  std::unique_ptr<aura::Window> CreateAppWindow(
      const gfx::Rect& bounds,
      WindowStateType window_state_type) {
    auto window = AshTestBase::CreateAppWindow(bounds, AppType::SYSTEM_APP,
                                               kShellWindowId_DeskContainerA,
                                               new TestWidgetDelegateAsh);

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

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_P(PipDoubleTapHandlerTest, TestSingleClick) {
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

TEST_P(PipDoubleTapHandlerTest, TestDoubleClickWithPip) {
  auto window = CreateAppWindow(gfx::Rect(kExpectedPipDefaultSize),
                                WindowStateType::kPip);

  DoubleTapWindowCenter(window.get());

  EXPECT_TRUE(WindowState::Get(window.get())->bounds_changed_by_user());
}

TEST_P(PipDoubleTapHandlerTest, TestDoubleClickAtMaxSize) {
  auto window =
      CreateAppWindow(gfx::Rect(kExpectedPipMaxSize), WindowStateType::kPip);

  DoubleTapWindowCenter(window.get());

  auto* window_state = WindowState::Get(window.get());

  EXPECT_TRUE(window_state->bounds_changed_by_user());
  EXPECT_EQ(GetFakeWindowStateLastBounds(window_state),
            gfx::Rect(kExpectedPipDefaultSize));
}

TEST_P(PipDoubleTapHandlerTest, TestDoubleClickAtNonMaxSize) {
  auto window = CreateAppWindow(gfx::Rect(300, 300), WindowStateType::kPip);

  DoubleTapWindowCenter(window.get());

  auto* window_state = WindowState::Get(window.get());

  EXPECT_TRUE(window_state->bounds_changed_by_user());
  // Expect it to go to max size on the first double-click/tap.
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

// Assume true is for mouse clicks and false is for gesture taps.
INSTANTIATE_TEST_SUITE_P(All, PipDoubleTapHandlerTest, testing::Bool());

}  // namespace ash
