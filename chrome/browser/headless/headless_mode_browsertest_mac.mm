// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/headless/headless_mode_browsertest.h"

#import <Cocoa/Cocoa.h>

#include "chrome/browser/headless/headless_mode_browsertest_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/ui_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/mac/coordinate_conversion.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/widget/widget.h"

namespace headless {

namespace test {

bool IsPlatformWindowVisible(views::Widget* widget) {
  CHECK(widget);

  gfx::NativeWindow native_window = widget->GetNativeWindow();
  CHECK(native_window);

  NSWindow* ns_window = native_window.GetNativeNSWindow();
  CHECK(ns_window);

  return ns_window.visible;
}

gfx::Rect GetPlatformWindowExpectedBounds(views::Widget* widget) {
  CHECK(widget);

  gfx::NativeWindow native_window = widget->GetNativeWindow();
  CHECK(native_window);

  NSWindow* ns_window = native_window.GetNativeNSWindow();
  CHECK(ns_window);

  gfx::Rect bounds = gfx::ScreenRectFromNSRect([ns_window frame]);

  return bounds;
}

}  // namespace test

namespace {

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
  ui_test_utils::ToggleFullscreenModeAndWait(browser());
  ASSERT_TRUE(browser()->window()->IsFullscreen());
  EXPECT_TRUE(browser()->window()->IsVisible());
  EXPECT_FALSE(ns_window.visible);

  // Verify back to normal state.
  ui_test_utils::ToggleFullscreenModeAndWait(browser());
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

IN_PROC_BROWSER_TEST_F(HeadlessModeBrowserTest,
                       MaximizedRestoredWindowVisibility) {
  gfx::NativeWindow native_window = browser()->window()->GetNativeWindow();
  NSWindow* ns_window = native_window.GetNativeNSWindow();

  // Verify initial state.
  ASSERT_FALSE(browser()->window()->IsMaximized());
  EXPECT_TRUE(browser()->window()->IsVisible());
  EXPECT_FALSE(ns_window.visible);

  // Verify maximized state.
  browser()->window()->Maximize();
  ASSERT_TRUE(browser()->window()->IsMaximized());
  EXPECT_TRUE(browser()->window()->IsVisible());
  EXPECT_FALSE(ns_window.visible);

  // Verify restored state.
  browser()->window()->Restore();
  ASSERT_FALSE(browser()->window()->IsMaximized());
  EXPECT_TRUE(browser()->window()->IsVisible());
  EXPECT_FALSE(ns_window.visible);
}

IN_PROC_BROWSER_TEST_F(HeadlessModeBrowserTestWithWindowSize, LargeWindowSize) {
  gfx::NativeWindow native_window = browser()->window()->GetNativeWindow();
  NSWindow* ns_window = native_window.GetNativeNSWindow();

  // Expect the platform window size to be the same as the requested window
  // size.
  NSRect frame_rect = [ns_window frame];
  EXPECT_EQ(NSWidth(frame_rect), kWindowSize.width());
  EXPECT_EQ(NSHeight(frame_rect), kWindowSize.height());

  // Expect the reported browser window size to be the same as the requested
  // window size.
  gfx::Rect bounds = browser()->window()->GetBounds();
  EXPECT_EQ(bounds.size(), kWindowSize);
}

IN_PROC_BROWSER_TEST_F(HeadlessModeBrowserTestWithWindowSizeAndScale,
                       WindowSizeWithScale) {
  gfx::NativeWindow native_window = browser()->window()->GetNativeWindow();
  NSWindow* ns_window = native_window.GetNativeNSWindow();

  // Expect the platform window size to be the same as the requested window size
  // due to scaling because Mac does not appear to support device scaling at
  // this time.
  NSRect frame_rect = [ns_window frame];
  EXPECT_EQ(NSWidth(frame_rect), kWindowSize.width());
  EXPECT_EQ(NSHeight(frame_rect), kWindowSize.height());

  // Expect the reported browser window size to be the same as the requested
  // window size.
  gfx::Rect bounds = browser()->window()->GetBounds();
  EXPECT_EQ(bounds.size(), kWindowSize);
}

}  // namespace

}  // namespace headless
