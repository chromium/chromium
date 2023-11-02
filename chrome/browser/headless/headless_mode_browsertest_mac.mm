// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/headless/headless_mode_browsertest.h"

#import <Cocoa/Cocoa.h>

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "testing/gtest/include/gtest/gtest.h"

INSTANTIATE_TEST_SUITE_P(HeadlessModeBrowserTestWithStartWindowMode,
                         HeadlessModeBrowserTestWithStartWindowMode,
                         testing::Values(kStartWindowNormal,
                                         kStartWindowMaximized,
                                         kStartWindowFullscreen));

IN_PROC_BROWSER_TEST_P(HeadlessModeBrowserTestWithStartWindowMode,
                       BrowserDesktopWindowVisibility) {
  // On Mac, the Native Headless Chrome browser window exists and is
  // visible, while the underlying platform window is hidden.
  EXPECT_TRUE(browser()->window()->IsVisible());

  gfx::NativeWindow native_window = browser()->window()->GetNativeWindow();
  NSWindow* ns_window = native_window.GetNativeNSWindow();

  EXPECT_FALSE(ns_window.visible);
}

IN_PROC_BROWSER_TEST_F(HeadlessModeBrowserTest,
                       ToggleFullscreenWindowVisibility) {
  gfx::NativeWindow native_window = browser()->window()->GetNativeWindow();
  NSWindow* ns_window = native_window.GetNativeNSWindow();

  // Verify initial state.
  ASSERT_FALSE(browser()->window()->IsFullscreen());
  EXPECT_TRUE(browser()->window()->IsVisible());
  EXPECT_FALSE(ns_window.visible);

  // Verify fullscreen state.
  ToggleFullscreenModeSync(browser());
  ASSERT_TRUE(browser()->window()->IsFullscreen());
  EXPECT_TRUE(browser()->window()->IsVisible());
  EXPECT_FALSE(ns_window.visible);

  // Verify back to normal state.
  ToggleFullscreenModeSync(browser());
  ASSERT_FALSE(browser()->window()->IsFullscreen());
  EXPECT_TRUE(browser()->window()->IsVisible());
  EXPECT_FALSE(ns_window.visible);
}

IN_PROC_BROWSER_TEST_F(HeadlessModeBrowserTest,
                       MinimizedRestoredWindowVisibility) {
  gfx::NativeWindow native_window = browser()->window()->GetNativeWindow();
  NSWindow* ns_window = native_window.GetNativeNSWindow();

  // Verify initial state.
  ASSERT_FALSE(browser()->window()->IsMinimized());
  EXPECT_TRUE(browser()->window()->IsVisible());
  EXPECT_FALSE(ns_window.visible);

  // Verify minimized state.
  browser()->window()->Minimize();
  ASSERT_TRUE(browser()->window()->IsMinimized());
  EXPECT_TRUE(browser()->window()->IsVisible());
  EXPECT_FALSE(ns_window.visible);

  // Verify restored state.
  browser()->window()->Restore();
  ASSERT_FALSE(browser()->window()->IsMinimized());
  EXPECT_TRUE(browser()->window()->IsVisible());
  EXPECT_FALSE(ns_window.visible);
}
