// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/accelerator_commands.h"

#include <memory>

#include "ash/test/ash_test_base.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ui/aura/window.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/screen.h"
#include "ui/display/test/display_manager_test_api.h"

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
  ToggleMaximized();
  EXPECT_TRUE(window_state->IsMaximized());
  ToggleMaximized();
  EXPECT_FALSE(window_state->IsMaximized());

  // When in fullscreen accelerators::ToggleMaximized gets out of fullscreen.
  EXPECT_FALSE(window_state->IsFullscreen());
  ToggleFullscreen();
  EXPECT_TRUE(window_state->IsFullscreen());
  ToggleMaximized();
  EXPECT_FALSE(window_state->IsFullscreen());
  EXPECT_FALSE(window_state->IsMaximized());
  ToggleMaximized();
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

TEST_F(AcceleratorCommandsTest, CycleSwapPrimaryDisplay) {
  display::test::DisplayManagerTestApi(display_manager())
      .SetFirstDisplayAsInternalDisplay();
  UpdateDisplay("800x600,800x600,800x600");

  display::DisplayIdList id_list = display_manager()->GetCurrentDisplayIdList();

  ShiftPrimaryDisplay();
  int64_t primary_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  EXPECT_EQ(id_list[1], primary_id);

  ShiftPrimaryDisplay();
  primary_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  EXPECT_EQ(id_list[2], primary_id);

  ShiftPrimaryDisplay();
  primary_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  EXPECT_EQ(id_list[0], primary_id);
}

TEST_F(AcceleratorCommandsTest, CycleMixedMirrorModeSwapPrimaryDisplay) {
  UpdateDisplay("300x400,400x500,500x600");
  display::DisplayIdList id_list = display_manager()->GetCurrentDisplayIdList();

  // Turn on mixed mirror mode. (Mirror from the first display to the second
  // display)
  display::DisplayIdList dst_ids;
  dst_ids.emplace_back(id_list[1]);
  base::Optional<display::MixedMirrorModeParams> mixed_params(
      base::in_place, id_list[0], dst_ids);

  display_manager()->SetMirrorMode(display::MirrorMode::kMixed, mixed_params);

  EXPECT_TRUE(display_manager()->IsInSoftwareMirrorMode());
  EXPECT_EQ(id_list[0], display_manager()->mirroring_source_id());
  EXPECT_TRUE(display_manager()->mixed_mirror_mode_params());
  EXPECT_EQ(2U, display_manager()->GetNumDisplays());

  ShiftPrimaryDisplay();
  int64_t primary_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  EXPECT_EQ(id_list[2], primary_id);

  ShiftPrimaryDisplay();
  primary_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  EXPECT_EQ(id_list[0], primary_id);

  ShiftPrimaryDisplay();
  primary_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  EXPECT_EQ(id_list[2], primary_id);
}

}  // namespace accelerators
}  // namespace ash
