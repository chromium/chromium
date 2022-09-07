// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/display_util.h"

#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/strings/utf_string_conversions.h"

namespace ash {

using DisplayUtilTest = AshTestBase;

TEST_F(DisplayUtilTest, RotatedDisplay) {
  {
    UpdateDisplay("10+10-500x400,600+10-1000x600/r");
    aura::Window::Windows root_windows = Shell::GetAllRootWindows();
    AshWindowTreeHost* host0 =
        RootWindowController::ForWindow(root_windows[0])->ash_host();
    AshWindowTreeHost* host1 =
        RootWindowController::ForWindow(root_windows[1])->ash_host();
    gfx::Rect rect0 = GetNativeEdgeBounds(host0, gfx::Rect(499, 10, 1, 300));
    gfx::Rect rect1 = GetNativeEdgeBounds(host1, gfx::Rect(500, 10, 1, 300));
    EXPECT_EQ(gfx::Rect(509, 20, 1, 300), rect0);
    EXPECT_EQ(gfx::Rect(1290, 10, 300, 1), rect1);
  }
  {
    UpdateDisplay("10+10-500x400,600+10-1000x600/l");
    aura::Window::Windows root_windows = Shell::GetAllRootWindows();
    AshWindowTreeHost* host0 =
        RootWindowController::ForWindow(root_windows[0])->ash_host();
    AshWindowTreeHost* host1 =
        RootWindowController::ForWindow(root_windows[1])->ash_host();
    gfx::Rect rect0 = GetNativeEdgeBounds(host0, gfx::Rect(499, 10, 1, 300));
    gfx::Rect rect1 = GetNativeEdgeBounds(host1, gfx::Rect(500, 10, 1, 300));
    EXPECT_EQ(gfx::Rect(509, 20, 1, 300), rect0);
    EXPECT_EQ(gfx::Rect(610, 609, 300, 1), rect1);
  }
  {
    UpdateDisplay("10+10-500x400,600+10-1000x600/u");
    aura::Window::Windows root_windows = Shell::GetAllRootWindows();
    AshWindowTreeHost* host0 =
        RootWindowController::ForWindow(root_windows[0])->ash_host();
    AshWindowTreeHost* host1 =
        RootWindowController::ForWindow(root_windows[1])->ash_host();
    gfx::Rect rect0 = GetNativeEdgeBounds(host0, gfx::Rect(499, 10, 1, 300));
    gfx::Rect rect1 = GetNativeEdgeBounds(host1, gfx::Rect(500, 10, 1, 300));
    EXPECT_EQ(gfx::Rect(509, 20, 1, 300), rect0);
    EXPECT_EQ(gfx::Rect(1599, 300, 1, 300), rect1);
  }

  {
    UpdateDisplay("10+10-500x400/r,600+10-1000x600");
    aura::Window::Windows root_windows = Shell::GetAllRootWindows();
    AshWindowTreeHost* host0 =
        RootWindowController::ForWindow(root_windows[0])->ash_host();
    AshWindowTreeHost* host1 =
        RootWindowController::ForWindow(root_windows[1])->ash_host();
    gfx::Rect rect0 = GetNativeEdgeBounds(host0, gfx::Rect(399, 10, 1, 300));
    gfx::Rect rect1 = GetNativeEdgeBounds(host1, gfx::Rect(400, 10, 1, 300));
    EXPECT_EQ(gfx::Rect(200, 409, 300, 1), rect0);
    EXPECT_EQ(gfx::Rect(600, 20, 1, 300), rect1);
  }
  {
    UpdateDisplay("10+10-500x400/l,600+10-1000x600");
    aura::Window::Windows root_windows = Shell::GetAllRootWindows();
    AshWindowTreeHost* host0 =
        RootWindowController::ForWindow(root_windows[0])->ash_host();
    AshWindowTreeHost* host1 =
        RootWindowController::ForWindow(root_windows[1])->ash_host();
    gfx::Rect rect0 = GetNativeEdgeBounds(host0, gfx::Rect(499, 10, 1, 300));
    gfx::Rect rect1 = GetNativeEdgeBounds(host1, gfx::Rect(500, 10, 1, 300));
    EXPECT_EQ(gfx::Rect(20, 10, 300, 1), rect0);
    EXPECT_EQ(gfx::Rect(600, 20, 1, 300), rect1);
  }
  {
    UpdateDisplay("10+10-500x400/u,600+10-1000x600");
    aura::Window::Windows root_windows = Shell::GetAllRootWindows();
    AshWindowTreeHost* host0 =
        RootWindowController::ForWindow(root_windows[0])->ash_host();
    AshWindowTreeHost* host1 =
        RootWindowController::ForWindow(root_windows[1])->ash_host();
    gfx::Rect rect0 = GetNativeEdgeBounds(host0, gfx::Rect(499, 10, 1, 300));
    gfx::Rect rect1 = GetNativeEdgeBounds(host1, gfx::Rect(500, 10, 1, 300));
    EXPECT_EQ(gfx::Rect(10, 100, 1, 300), rect0);
    EXPECT_EQ(gfx::Rect(600, 20, 1, 300), rect1);
  }
}

TEST_F(DisplayUtilTest, ConvertRefreshRateToString16) {
  // Decimal numbers are rounded to two digits.
  EXPECT_EQ(u"65.98", ConvertRefreshRateToString16(65.98379));
  EXPECT_EQ(u"65.99", ConvertRefreshRateToString16(65.98779));

  // Mantissa is removed for whole numbers.
  EXPECT_EQ(u"58", ConvertRefreshRateToString16(58.00000));
  EXPECT_EQ(u"58", ConvertRefreshRateToString16(57.99999));
}

}  // namespace ash
