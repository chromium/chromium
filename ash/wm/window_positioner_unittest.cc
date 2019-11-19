// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_positioner.h"

#include <string>

#include "ash/public/cpp/shelf_config.h"
#include "ash/scoped_root_window_for_new_windows.h"
#include "ash/shell.h"
#include "ash/shell/toplevel_window.h"
#include "ash/test/ash_test_base.h"
#include "ash/test_shell_delegate.h"
#include "ash/wm/window_positioner.h"
#include "ash/wm/window_state.h"
#include "base/strings/string_number_conversions.h"
#include "ui/display/screen.h"
#include "ui/views/widget/widget.h"

namespace ash {

class WindowPositionerTest : public AshTestBase {
 public:
  WindowPositionerTest() = default;
  ~WindowPositionerTest() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(WindowPositionerTest);
};

TEST_F(WindowPositionerTest, OpenDefaultWindowOnSecondDisplay) {
  UpdateDisplay("400x400,1400x900");
  aura::Window* second_root_window = Shell::GetAllRootWindows()[1];
  ScopedRootWindowForNewWindows root_for_new_windows(second_root_window);
  shell::ToplevelWindow::CreateParams params;
  params.can_resize = true;
  params.can_maximize = true;
  views::Widget* widget = shell::ToplevelWindow::CreateToplevelWindow(params);
  gfx::Rect bounds = widget->GetWindowBoundsInScreen();

  // The window should be in the 2nd display with the default size.
  EXPECT_EQ("300x300", bounds.size().ToString());
  EXPECT_TRUE(display::Screen::GetScreen()
                  ->GetDisplayNearestWindow(second_root_window)
                  .bounds()
                  .Contains(bounds));
}

// Tests that second window inherits first window's maximized state as well as
// its restore bounds.
TEST_F(WindowPositionerTest, SecondMaximizedWindowHasProperRestoreSize) {
  UpdateDisplay("1400x900");
  const int bottom_inset = 900 - ShelfConfig::Get()->shelf_size();
  shell::ToplevelWindow::CreateParams params;
  params.can_resize = true;
  params.can_maximize = true;
  views::Widget* widget1 = shell::ToplevelWindow::CreateToplevelWindow(params);
  gfx::Rect bounds = widget1->GetWindowBoundsInScreen();

  // The window should have default size.
  EXPECT_FALSE(widget1->IsMaximized());
  EXPECT_EQ("300x300", bounds.size().ToString());
  widget1->Maximize();

  // The window should be maximized.
  bounds = widget1->GetWindowBoundsInScreen();
  EXPECT_TRUE(widget1->IsMaximized());
  EXPECT_EQ(gfx::Rect(0, 0, 1400, bottom_inset).ToString(), bounds.ToString());

  // Create another window
  views::Widget* widget2 = shell::ToplevelWindow::CreateToplevelWindow(params);

  // The second window should be maximized.
  bounds = widget2->GetWindowBoundsInScreen();
  EXPECT_TRUE(widget2->IsMaximized());
  EXPECT_EQ(gfx::Rect(0, 0, 1400, bottom_inset).ToString(), bounds.ToString());

  widget2->Restore();
  // Second window's restored size should be set to default size.
  bounds = widget2->GetWindowBoundsInScreen();
  EXPECT_EQ("300x300", bounds.size().ToString());
}

TEST_F(WindowPositionerTest, IgnoreFullscreenInAutoRearrange) {
  // Set bigger than 1366 so that the new window is opened in normal state.
  UpdateDisplay("1400x800");

  // 1st window mimics fullscreen browser window behavior.
  shell::ToplevelWindow::CreateParams params;
  params.can_resize = true;
  params.can_maximize = true;
  views::Widget* widget1 = shell::ToplevelWindow::CreateToplevelWindow(params);
  WindowState* managed_state = WindowState::Get(widget1->GetNativeWindow());
  EXPECT_TRUE(managed_state->GetWindowPositionManaged());
  EXPECT_EQ("300x300", widget1->GetWindowBoundsInScreen().size().ToString());
  widget1->SetFullscreen(true);
  ASSERT_EQ("1400x800", widget1->GetWindowBoundsInScreen().size().ToString());

  // 2nd window mimics windowed v1 app.
  params.use_saved_placement = false;
  views::Widget* widget2 = shell::ToplevelWindow::CreateToplevelWindow(params);
  WindowState* state_2 = WindowState::Get(widget2->GetNativeWindow());
  EXPECT_TRUE(state_2->GetWindowPositionManaged());
  EXPECT_EQ("300x300", widget2->GetWindowBoundsInScreen().size().ToString());

  // Restores to the original size.
  widget1->SetFullscreen(false);
  ASSERT_EQ("300x300", widget1->GetWindowBoundsInScreen().size().ToString());

  // Closing 2nd widget triggers the rearrange logic but the 1st
  // widget should stay in the current size.
  widget2->CloseNow();
  ASSERT_EQ("300x300", widget1->GetWindowBoundsInScreen().size().ToString());
}

}  // namespace ash
