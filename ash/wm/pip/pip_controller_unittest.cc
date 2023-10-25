// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/pip/pip_controller.h"

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/collision_detection/collision_detection_utils.h"
#include "ash/wm/window_state.h"
#include "ash/wm/wm_event.h"
#include "base/test/scoped_feature_list.h"
#include "ui/wm/core/window_util.h"

namespace ash {

class PipControllerTest : public AshTestBase {
 public:
  PipControllerTest() : scoped_feature_list_(features::kPipTuck) {}

  PipControllerTest(const PipControllerTest&) = delete;
  PipControllerTest& operator=(const PipControllerTest&) = delete;
  ~PipControllerTest() override = default;

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

TEST_F(PipControllerTest, WMEventPipSetsTargetWindow) {
  const gfx::Rect bounds_in_screen(100, 100, 300, 200);
  std::unique_ptr<aura::Window> window(CreatePipWindow(bounds_in_screen));

  // `OnWMEvent` in `CreatePipWindow()` should set the target window to
  // the controller.
  PipController* controller = Shell::Get()->pip_controller();
  EXPECT_EQ(controller->pip_window(), window.get());
}

TEST_F(PipControllerTest, UpdatePipBounds) {
  const gfx::Rect bounds_in_screen(100, 100, 300, 200);
  std::unique_ptr<aura::Window> window(CreatePipWindow(bounds_in_screen));

  // `CreatePipWindow()`'s `OnWMEvent()` ends up calling
  // `UpdatePipBounds()` which should move the PiP window to its resting
  // position.
  gfx::Rect expected_bounds = CollisionDetectionUtils::GetRestingPosition(
      WindowState::Get(window.get())->GetDisplay(), bounds_in_screen,
      CollisionDetectionUtils::RelativePriority::kPictureInPicture);
  EXPECT_EQ(expected_bounds, window->bounds());
}

}  // namespace ash
