// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/accelerator_commands.h"

#include <memory>

#include "ash/test/ash_test_base.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ui/aura/window.h"

namespace ash {
namespace accelerators {

using AcceleratorCommandsTest = AshTestBase;

TEST_F(AcceleratorCommandsTest, ToggleMinimized) {
  std::unique_ptr<aura::Window> window1(
      CreateTestWindowInShellWithBounds(gfx::Rect(5, 5, 20, 20)));
  std::unique_ptr<aura::Window> window2(
      CreateTestWindowInShellWithBounds(gfx::Rect(5, 5, 20, 20)));
  WindowState* window_state1 = WindowState::Get(window1.get());
  WindowState* window_state2 = WindowState::Get(window2.get());
  window_state1->Activate();
  window_state2->Activate();

  ToggleMinimized();
  EXPECT_TRUE(window_state2->IsMinimized());
  EXPECT_FALSE(window_state2->IsNormalStateType());
  EXPECT_TRUE(window_state1->IsActive());

  ToggleMinimized();
  EXPECT_TRUE(window_state1->IsMinimized());
  EXPECT_FALSE(window_state1->IsNormalStateType());
  EXPECT_FALSE(window_state1->IsActive());

  // Toggling minimize when there are no active windows should unminimize and
  // activate the last active window.
  ToggleMinimized();
  EXPECT_FALSE(window_state1->IsMinimized());
  EXPECT_TRUE(window_state1->IsNormalStateType());
  EXPECT_TRUE(window_state1->IsActive());
}

TEST_F(AcceleratorCommandsTest, ToggleMaximized) {
  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(gfx::Rect(5, 5, 20, 20)));
  WindowState* window_state = WindowState::Get(window.get());
  window_state->Activate();

  // When not in fullscreen, accelerators::ToggleMaximized toggles Maximized.
  EXPECT_FALSE(window_state->IsMaximized());
  accelerators::ToggleMaximized();
  EXPECT_TRUE(window_state->IsMaximized());
  accelerators::ToggleMaximized();
  EXPECT_FALSE(window_state->IsMaximized());

  // When in fullscreen accelerators::ToggleMaximized gets out of fullscreen.
  EXPECT_FALSE(window_state->IsFullscreen());
  accelerators::ToggleFullscreen();
  EXPECT_TRUE(window_state->IsFullscreen());
  accelerators::ToggleMaximized();
  EXPECT_FALSE(window_state->IsFullscreen());
  EXPECT_FALSE(window_state->IsMaximized());
  accelerators::ToggleMaximized();
  EXPECT_FALSE(window_state->IsFullscreen());
  EXPECT_TRUE(window_state->IsMaximized());
}

TEST_F(AcceleratorCommandsTest, Unpin) {
  std::unique_ptr<aura::Window> window1(
      CreateTestWindowInShellWithBounds(gfx::Rect(5, 5, 20, 20)));
  WindowState* window_state1 = WindowState::Get(window1.get());
  window_state1->Activate();

  window_util::PinWindow(window1.get(), /* trusted */ false);
  EXPECT_TRUE(window_state1->IsPinned());

  UnpinWindow();
  EXPECT_FALSE(window_state1->IsPinned());
}

}  // namespace accelerators
}  // namespace ash
