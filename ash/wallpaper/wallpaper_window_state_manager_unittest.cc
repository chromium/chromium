// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wallpaper/wallpaper_window_state_manager.h"

#include <memory>

#include "ash/test/ash_test_base.h"
#include "ash/wm/window_state.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "ui/aura/window.h"

namespace ash {
namespace {

constexpr char kTestAccount[] = "user@test.com";

std::string GetUserIdHash(const std::string& user_id) {
  return user_id + "-hash";
}

class WallpaperWindowStateManagerTest : public AshTestBase {
 public:
  WallpaperWindowStateManagerTest()
      : window_state_manager_(std::make_unique<WallpaperWindowStateManager>()) {
  }

  ~WallpaperWindowStateManagerTest() override = default;

 protected:
  std::unique_ptr<WallpaperWindowStateManager> window_state_manager_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WallpaperWindowStateManagerTest);
};

TEST_F(WallpaperWindowStateManagerTest, HideAndRestoreWindows) {
  SimulateUserLogin(kTestAccount);
  std::unique_ptr<aura::Window> wallpaper_picker_window(
      CreateTestWindowInShellWithId(0));
  std::unique_ptr<aura::Window> window1(CreateTestWindowInShellWithId(1));
  std::unique_ptr<aura::Window> window2(CreateTestWindowInShellWithId(2));
  std::unique_ptr<aura::Window> window3(CreateTestWindowInShellWithId(3));
  std::unique_ptr<aura::Window> window4(CreateTestWindowInShellWithId(4));

  WindowState* wallpaper_picker_window_state =
      WindowState::Get(wallpaper_picker_window.get());
  WindowState* window1_state = WindowState::Get(window1.get());
  WindowState* window2_state = WindowState::Get(window2.get());
  WindowState* window3_state = WindowState::Get(window3.get());
  WindowState* window4_state = WindowState::Get(window4.get());

  // Window 1 starts maximized and window 3 starts minimized.
  window1_state->Maximize();
  window3_state->Minimize();
  EXPECT_FALSE(wallpaper_picker_window_state->IsMinimized());
  EXPECT_FALSE(window1_state->IsMinimized());
  EXPECT_FALSE(window2_state->IsMinimized());
  EXPECT_TRUE(window3_state->IsMinimized());
  EXPECT_FALSE(window4_state->IsMinimized());

  // Activates the wallpaper picker window and call the minimize function.
  wallpaper_picker_window_state->Activate();
  EXPECT_TRUE(wallpaper_picker_window_state->IsActive());
  window_state_manager_->MinimizeInactiveWindows(GetUserIdHash(kTestAccount));

  // All windows except the wallpaper picker should be minimized.
  EXPECT_FALSE(wallpaper_picker_window_state->IsMinimized());
  EXPECT_TRUE(window1_state->IsMinimized());
  EXPECT_TRUE(window2_state->IsMinimized());
  EXPECT_TRUE(window3_state->IsMinimized());
  EXPECT_TRUE(window4_state->IsMinimized());

  // Activates window 4 and then minimizes it.
  window4_state->Activate();
  window4_state->Minimize();

  // Destroy wallpaper picker window and call the restore function.
  wallpaper_picker_window.reset();
  window_state_manager_->RestoreMinimizedWindows(GetUserIdHash(kTestAccount));

  // Window 1 should be restored to maximized.
  EXPECT_TRUE(window1_state->IsMaximized());
  // Window 2 should be restored and is no longer minimized.
  EXPECT_FALSE(window2_state->IsMinimized());
  // Window 3 should remain minimized because it was minimized before wallpaper
  // picker was open.
  EXPECT_TRUE(window3_state->IsMinimized());
  // Window 4 should remain minimized since user interacted with it (i.e.
  // explicitly minimized it) while wallpaper picker was open.
  EXPECT_TRUE(window4_state->IsMinimized());
}

// Test for multiple calls to |MinimizeInactiveWindows| before calling
// |RestoreMinimizedWindows|:
// 1. If none of the windows changed their states, the following calls are
//    no-op.
// 2. If some windows are unminimized by user, the following call will minimize
//    the unminimized windows again.
TEST_F(WallpaperWindowStateManagerTest, HideAndManualUnminimizeWindows) {
  SimulateUserLogin(kTestAccount);
  std::unique_ptr<aura::Window> wallpaper_picker_window(
      CreateTestWindowInShellWithId(0));
  std::unique_ptr<aura::Window> window1(CreateTestWindowInShellWithId(1));

  WindowState* wallpaper_picker_window_state =
      WindowState::Get(wallpaper_picker_window.get());
  WindowState* window1_state = WindowState::Get(window1.get());

  // Activates the wallpaper picker window and call the minimize function.
  wallpaper_picker_window_state->Activate();
  EXPECT_TRUE(wallpaper_picker_window_state->IsActive());
  window_state_manager_->MinimizeInactiveWindows(GetUserIdHash(kTestAccount));

  // All windows except the wallpaper picker should be minimized.
  EXPECT_FALSE(wallpaper_picker_window_state->IsMinimized());
  EXPECT_TRUE(window1_state->IsMinimized());

  // Calling minimize function again should be an no-op if window state didn't
  // change.
  window_state_manager_->MinimizeInactiveWindows(GetUserIdHash(kTestAccount));
  EXPECT_FALSE(wallpaper_picker_window_state->IsMinimized());
  EXPECT_TRUE(window1_state->IsMinimized());

  // Manually unminimize window 1.
  window1_state->Unminimize();
  EXPECT_FALSE(window1_state->IsMinimized());

  // Call the minimize function and verify window 1 should be minimized again.
  wallpaper_picker_window_state->Activate();
  window_state_manager_->MinimizeInactiveWindows(GetUserIdHash(kTestAccount));
  EXPECT_FALSE(wallpaper_picker_window_state->IsMinimized());
  EXPECT_TRUE(window1_state->IsMinimized());

  // Destroy wallpaper picker window and call the restore function.
  wallpaper_picker_window.reset();
  window_state_manager_->RestoreMinimizedWindows(GetUserIdHash(kTestAccount));

  // Windows 1 should no longer be minimized.
  EXPECT_FALSE(window1_state->IsMinimized());
}

// Test that invisible windows (e.g. those belonging to an inactive user) should
// not be affected by |MinimizeInactiveWindows| or |RestoreMinimizedWindows|.
TEST_F(WallpaperWindowStateManagerTest, IgnoreInvisibleWindows) {
  SimulateUserLogin(kTestAccount);
  std::unique_ptr<aura::Window> wallpaper_picker_window(
      CreateTestWindowInShellWithId(0));
  std::unique_ptr<aura::Window> window1(CreateTestWindowInShellWithId(1));
  std::unique_ptr<aura::Window> window2(CreateTestWindowInShellWithId(2));

  WindowState* wallpaper_picker_window_state =
      WindowState::Get(wallpaper_picker_window.get());
  WindowState* window1_state = WindowState::Get(window1.get());
  WindowState* window2_state = WindowState::Get(window2.get());

  window2->Hide();
  EXPECT_FALSE(window2->IsVisible());

  // Activates the wallpaper picker window and call the minimize function.
  wallpaper_picker_window_state->Activate();
  EXPECT_TRUE(wallpaper_picker_window_state->IsActive());
  window_state_manager_->MinimizeInactiveWindows(GetUserIdHash(kTestAccount));

  // The wallpaper picker window should not be minimized, and window 1 should be
  // minimized.
  EXPECT_FALSE(wallpaper_picker_window_state->IsMinimized());
  EXPECT_TRUE(window1_state->IsMinimized());
  // Window 2 should stay unminimized and invisible.
  EXPECT_FALSE(window2_state->IsMinimized());
  EXPECT_FALSE(window2->IsVisible());

  // Destroy wallpaper picker window and call the restore function.
  wallpaper_picker_window.reset();
  window_state_manager_->RestoreMinimizedWindows(GetUserIdHash(kTestAccount));

  // Windows 1 should no longer be minimized.
  EXPECT_FALSE(window1_state->IsMinimized());
  // Window 2 should stay unminimized and invisible.
  EXPECT_FALSE(window2_state->IsMinimized());
  EXPECT_FALSE(window2->IsVisible());
}

}  // namespace
}  // namespace ash
