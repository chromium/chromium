// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/headless/headless_mode_browsertest.h"

#import <Cocoa/Cocoa.h>

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

IN_PROC_BROWSER_TEST_F(HeadlessModeBrowserTest,
                       BrowserDesktopWindowVisibility) {
  // On Mac, the Native Headless Chrome browser window exists and is
  // visible, while the underlying platform window is hidden.
  EXPECT_TRUE(browser()->window()->IsVisible());

  gfx::NativeWindow native_window = browser()->window()->GetNativeWindow();
  NSWindow* ns_window = native_window.GetNativeNSWindow();

  EXPECT_FALSE(ns_window.visible);
}
