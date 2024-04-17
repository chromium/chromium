// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/shortcuts/platform_util_mac.h"

#import <AppKit/AppKit.h>

#import "base/apple/foundation_util.h"
#include "base/files/scoped_temp_file.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"

namespace shortcuts {

namespace {
NSImage* GetNSImageForColor(NSColor* color) {
  return [NSImage imageWithSize:NSMakeSize(32, 32)
                        flipped:NO
                 drawingHandler:^(NSRect rect) {
                   [color set];
                   NSRectFill(rect);
                   return YES;
                 }];
}

NSColor* ColorFromImage(NSImage* image) {
  NSBitmapImageRep* bitmap =
      [[NSBitmapImageRep alloc] initWithData:image.TIFFRepresentation];
  NSColor* color = [bitmap colorAtX:0 y:0];
  return color;
}
}  // namespace

TEST(ShortcutsPlatformUtilMacTest, SetIconForFile) {
  base::test::TaskEnvironment task_environment;

  base::ScopedTempFile test_file1;
  ASSERT_TRUE(test_file1.Create());
  base::ScopedTempFile test_file2;
  ASSERT_TRUE(test_file2.Create());

  base::test::TestFuture<bool> result1;
  SetIconForFile(GetNSImageForColor(NSColor.redColor), test_file1.path(),
                 result1.GetCallback());
  base::test::TestFuture<bool> result2;
  SetIconForFile(GetNSImageForColor(NSColor.blueColor), test_file2.path(),
                 result2.GetCallback());

  EXPECT_TRUE(result1.Get());
  EXPECT_TRUE(result2.Get());

  NSImage* image1 = [NSWorkspace.sharedWorkspace
      iconForFile:base::apple::FilePathToNSString(test_file1.path())];
  NSColor* color1 = ColorFromImage(image1);
  EXPECT_EQ(1.0f, color1.redComponent);
  EXPECT_EQ(0.0f, color1.greenComponent);
  EXPECT_EQ(0.0f, color1.blueComponent);
  NSImage* image2 = [NSWorkspace.sharedWorkspace
      iconForFile:base::apple::FilePathToNSString(test_file2.path())];
  NSColor* color2 = ColorFromImage(image2);
  EXPECT_EQ(0.0f, color2.redComponent);
  EXPECT_EQ(0.0f, color2.greenComponent);
  EXPECT_EQ(1.0f, color2.blueComponent);
}

TEST(ShortcutsPlatformUtilMacTest, SetDefaultApplicationToOpenFile) {
  base::test::TaskEnvironment task_environment;

  base::ScopedTempFile test_file;
  ASSERT_TRUE(test_file.Create());
  NSURL* file_url = base::apple::FilePathToNSURL(test_file.path());

  // This test needs an arbitrary (but valid) url for an application. Safari
  // always exists, so is as good as any app to use.
  NSURL* app_url = [NSWorkspace.sharedWorkspace
      URLForApplicationWithBundleIdentifier:@"com.apple.Safari"];

  base::test::TestFuture<NSError*> result;
  SetDefaultApplicationToOpenFile(file_url, app_url, result.GetCallback());
  EXPECT_FALSE(result.Get());

  NSURL* got_app_url =
      [NSWorkspace.sharedWorkspace URLForApplicationToOpenURL:file_url];
  EXPECT_NSEQ(app_url, got_app_url);
}

}  // namespace shortcuts
