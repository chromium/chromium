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
#import "components/remote_cocoa/app_shim/native_widget_mac_nswindow.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/mac/coordinate_conversion.h"
#include "ui/gfx/native_ui_types.h"
#include "ui/views/widget/widget.h"

namespace headless {

namespace test {

bool IsPlatformWindowVisible(views::Widget* widget) {
  CHECK(widget);

  gfx::NativeWindow native_window = widget->GetNativeWindow();
  CHECK(native_window);

  NSWindow* ns_window = native_window.GetNativeNSWindow();
  CHECK(ns_window);

  return
      [(NativeWidgetMacNSWindow*)ns_window invokeOriginalIsVisibleForTesting];
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

INSTANTIATE_TEST_SUITE_P(
    /*no prefix*/,
    HeadlessModeBrowserTestWithStartWindowMode,
    testing::Values(kStartWindowNormal,
                    kStartWindowMaximized,
                    kStartWindowFullscreen),
    [](const testing::TestParamInfo<
        HeadlessModeBrowserTestWithStartWindowMode::ParamType>& info) {
      switch (info.param) {
        case kStartWindowNormal:
          return "Normal";
        case kStartWindowMaximized:
          return "Maximized";
        case kStartWindowFullscreen:
          return "Fullscreen";
      }
    });

IN_PROC_BROWSER_TEST_P(HeadlessModeBrowserTestWithStartWindowMode,
                       BrowserDesktopWindowVisibility) {
  EXPECT_TRUE(browser()->window()->IsVisible());

  // The Native Window NSWindow exists and pretends to be visible using the
  // method swizzling magic that overrides the relevant NSWindow methods, see
  // components/remote_cocoa/app_shim/native_widget_mac_nswindow_headless.mm.
  gfx::NativeWindow native_window = browser()->window()->GetNativeWindow();
  NSWindow* ns_window = native_window.GetNativeNSWindow();
  EXPECT_TRUE([ns_window isVisible]);

  // However, the underlying platform window is actually always hidden.
  EXPECT_FALSE(
      [(NativeWidgetMacNSWindow*)ns_window invokeOriginalIsVisibleForTesting]);
}

IN_PROC_BROWSER_TEST_F(HeadlessModeBrowserTest,
                       ToggleFullscreenWindowVisibility) {
  gfx::NativeWindow native_window = browser()->window()->GetNativeWindow();
  NSWindow* ns_window = native_window.GetNativeNSWindow();

  // Verify initial state.
  ASSERT_FALSE(browser()->window()->IsFullscreen());
  EXPECT_TRUE(browser()->window()->IsVisible());
  EXPECT_TRUE([ns_window isVisible]);
  EXPECT_FALSE(
      [(NativeWidgetMacNSWindow*)ns_window invokeOriginalIsVisibleForTesting]);

  // Verify fullscreen state.
  ui_test_utils::ToggleFullscreenModeAndWait(browser());
  ASSERT_TRUE(browser()->window()->IsFullscreen());
  EXPECT_TRUE(browser()->window()->IsVisible());
  EXPECT_TRUE([ns_window isVisible]);
  EXPECT_FALSE(
      [(NativeWidgetMacNSWindow*)ns_window invokeOriginalIsVisibleForTesting]);

  // Verify back to normal state.
  ui_test_utils::ToggleFullscreenModeAndWait(browser());
  ASSERT_FALSE(browser()->window()->IsFullscreen());
  EXPECT_TRUE(browser()->window()->IsVisible());
  EXPECT_TRUE([ns_window isVisible]);
  EXPECT_FALSE(
      [(NativeWidgetMacNSWindow*)ns_window invokeOriginalIsVisibleForTesting]);
}

IN_PROC_BROWSER_TEST_F(HeadlessModeBrowserTest,
                       MinimizedRestoredWindowVisibility) {
  gfx::NativeWindow native_window = browser()->window()->GetNativeWindow();
  NSWindow* ns_window = native_window.GetNativeNSWindow();

  // Verify initial state.
  ASSERT_FALSE(browser()->window()->IsMinimized());
  EXPECT_TRUE(browser()->window()->IsVisible());
  EXPECT_TRUE([ns_window isVisible]);
  EXPECT_FALSE(
      [(NativeWidgetMacNSWindow*)ns_window invokeOriginalIsVisibleForTesting]);

  // Verify minimized state.
  browser()->window()->Minimize();
  ASSERT_TRUE(browser()->window()->IsMinimized());
  EXPECT_FALSE(browser()->window()->IsVisible());
  EXPECT_FALSE([ns_window isVisible]);
  EXPECT_FALSE(
      [(NativeWidgetMacNSWindow*)ns_window invokeOriginalIsVisibleForTesting]);

  // Verify restored state.
  browser()->window()->Restore();
  ASSERT_FALSE(browser()->window()->IsMinimized());
  EXPECT_TRUE(browser()->window()->IsVisible());
  EXPECT_TRUE([ns_window isVisible]);
  EXPECT_FALSE(
      [(NativeWidgetMacNSWindow*)ns_window invokeOriginalIsVisibleForTesting]);
}

IN_PROC_BROWSER_TEST_F(HeadlessModeBrowserTest,
                       MaximizedRestoredWindowVisibility) {
  gfx::NativeWindow native_window = browser()->window()->GetNativeWindow();
  NSWindow* ns_window = native_window.GetNativeNSWindow();

  // Verify initial state.
  ASSERT_FALSE(browser()->window()->IsMaximized());
  EXPECT_TRUE(browser()->window()->IsVisible());
  EXPECT_TRUE([ns_window isVisible]);
  EXPECT_FALSE(
      [(NativeWidgetMacNSWindow*)ns_window invokeOriginalIsVisibleForTesting]);

  // Verify maximized state.
  browser()->window()->Maximize();
  ASSERT_TRUE(browser()->window()->IsMaximized());
  EXPECT_TRUE(browser()->window()->IsVisible());
  EXPECT_TRUE([ns_window isVisible]);
  EXPECT_FALSE(
      [(NativeWidgetMacNSWindow*)ns_window invokeOriginalIsVisibleForTesting]);

  // Verify restored state.
  browser()->window()->Restore();
  ASSERT_FALSE(browser()->window()->IsMaximized());
  EXPECT_TRUE(browser()->window()->IsVisible());
  EXPECT_TRUE([ns_window isVisible]);
  EXPECT_FALSE(
      [(NativeWidgetMacNSWindow*)ns_window invokeOriginalIsVisibleForTesting]);
}

}  // namespace
}  // namespace headless
