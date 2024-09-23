// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <AppKit/AppKit.h>

#include "testing/gtest/include/gtest/gtest.h"

TEST(ChromeBrowserApplicationTest, MethodsExist) {
  // BrowserCrApplication implements functionality by overriding a method,
  // -[NSApplication _customizeFileMenuIfNeeded]. While this test can't test for
  // the functionality of that method, it can test to ensure it exists, so that
  // if that changes, a test break will be an early alert.
  EXPECT_TRUE([NSApplication
      instancesRespondToSelector:@selector(_customizeFileMenuIfNeeded)]);
  if (@available(macos 12, *)) {
    // Starting with macOS 12, a part of the -_customizeFileMenuIfNeeded
    // implementation is the method -_customizeQuitMenuItem, so check for that
    // too.
    EXPECT_TRUE([NSApplication
        instancesRespondToSelector:@selector(_customizeQuitMenuItem)]);
  }
}
