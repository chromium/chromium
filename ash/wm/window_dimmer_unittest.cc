// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_dimmer.h"

#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/test_windows.h"
#include "ui/wm/core/window_util.h"

namespace ash {

using WindowDimmerTest = ash::AshTestBase;

// Verify that a window underneath the window dimmer is not occluded.
TEST_F(WindowDimmerTest, Occlusion) {
  aura::Window* root_window = GetContext();
  aura::Window* bottom_window = aura::test::CreateTestWindow(
      SK_ColorWHITE, 1, root_window->bounds(), root_window);
  bottom_window->TrackOcclusionState();
  WindowDimmer dimmer(root_window);

  EXPECT_EQ(aura::Window::OcclusionState::VISIBLE,
            bottom_window->GetOcclusionState());
  // Sanity check: An opaque window on top of |bottom_window| occludes it.
  aura::test::CreateTestWindow(SK_ColorWHITE, 2, root_window->bounds(),
                               root_window);
  EXPECT_EQ(aura::Window::OcclusionState::OCCLUDED,
            bottom_window->GetOcclusionState());

  // The dimming window should never be activate-able even when it's visible.
  dimmer.window()->Show();
  EXPECT_FALSE(wm::CanActivateWindow(dimmer.window()));
}

}  // namespace ash
