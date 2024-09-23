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
#include "ui/gfx/image/image_unittest_util.h"

namespace shortcuts {

TEST(ShortcutsPlatformUtilMacTest, SetIconForFile) {
  base::test::TaskEnvironment task_environment;

  base::ScopedTempFile test_file1;
  ASSERT_TRUE(test_file1.Create());
  base::ScopedTempFile test_file2;
  ASSERT_TRUE(test_file2.Create());

  base::test::TestFuture<bool> result1;
  SetIconForFile(gfx::test::CreateImage(/*size=*/32, SK_ColorRED).ToNSImage(),
                 test_file1.path(), result1.GetCallback());
  base::test::TestFuture<bool> result2;
  SetIconForFile(gfx::test::CreateImage(/*size=*/32, SK_ColorBLUE).ToNSImage(),
                 test_file2.path(), result2.GetCallback());

  EXPECT_TRUE(result1.Get());
  EXPECT_TRUE(result2.Get());

  NSImage* image1 = [NSWorkspace.sharedWorkspace
      iconForFile:base::apple::FilePathToNSString(test_file1.path())];
  SkColor color1 = gfx::test::GetPlatformImageColor(image1, 0, 0);
  gfx::test::CheckColors(SK_ColorRED, color1);

  NSImage* image2 = [NSWorkspace.sharedWorkspace
      iconForFile:base::apple::FilePathToNSString(test_file2.path())];
  SkColor color2 = gfx::test::GetPlatformImageColor(image2, 0, 0);
  gfx::test::CheckColors(SK_ColorBLUE, color2);
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

TEST(ShortcutsPlatformUtilMacTest, SanitizeTitleForFileName) {
  auto sanitize_to_string = [](const std::string& title) {
    std::optional<base::SafeBaseName> name = SanitizeTitleForFileName(title);
    EXPECT_TRUE(name.has_value());
    return name.has_value() ? name->path().value() : "";
  };
  EXPECT_EQ("strip.initial.dots....",
            sanitize_to_string("...strip.initial.dots...."));
  EXPECT_EQ("http:::example.com:foo",
            sanitize_to_string("http://example.com/foo"));
  EXPECT_EQ("Whitespace is allowed!",
            sanitize_to_string("Whitespace is allowed!"));
  EXPECT_EQ(" ... dots ...", sanitize_to_string("... ... dots ..."));
  EXPECT_EQ(std::nullopt, SanitizeTitleForFileName("... ..."));
  EXPECT_EQ(std::nullopt, SanitizeTitleForFileName(""));
  EXPECT_EQ(std::nullopt, SanitizeTitleForFileName("...."));
}

}  // namespace shortcuts
