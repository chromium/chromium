// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/mac/mac_util.h"

#import <Cocoa/Cocoa.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/xattr.h>

#include "base/apple/bridging.h"
#include "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/system/sys_info.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace base::mac {

namespace {

using MacUtilTest = PlatformTest;

TEST_F(MacUtilTest, GetUserDirectoryTest) {
  // Try a few keys, make sure they come back with non-empty paths.
  FilePath caches_dir;
  EXPECT_TRUE(apple::GetUserDirectory(NSCachesDirectory, &caches_dir));
  EXPECT_FALSE(caches_dir.empty());

  FilePath application_support_dir;
  EXPECT_TRUE(apple::GetUserDirectory(NSApplicationSupportDirectory,
                                      &application_support_dir));
  EXPECT_FALSE(application_support_dir.empty());

  FilePath library_dir;
  EXPECT_TRUE(apple::GetUserDirectory(NSLibraryDirectory, &library_dir));
  EXPECT_FALSE(library_dir.empty());
}

TEST_F(MacUtilTest, TestLibraryPath) {
  FilePath library_dir = apple::GetUserLibraryPath();
  // Make sure the string isn't empty.
  EXPECT_FALSE(library_dir.value().empty());
}

TEST_F(MacUtilTest, TestGetAppBundlePath) {
  FilePath out;

  // Make sure it doesn't crash.
  out = apple::GetAppBundlePath(FilePath());
  EXPECT_TRUE(out.empty());

  // Some more invalid inputs.
  const char* const invalid_inputs[] = {
      "/",
      "/foo",
      "foo",
      "/foo/bar.",
      "foo/bar.",
      "/foo/bar./bazquux",
      "foo/bar./bazquux",
      "foo/.app",
      "//foo",
  };
  for (const auto* input : invalid_inputs) {
    SCOPED_TRACE(std::string("input: ") + input);
    out = apple::GetAppBundlePath(FilePath(input));
    EXPECT_TRUE(out.empty());
  }

  // Some valid inputs; this and |expected_outputs| should be in sync.
  struct {
    const char* in;
    const char* expected_out;
  } valid_inputs[] = {
      {"FooBar.app/", "FooBar.app"},
      {"/FooBar.app", "/FooBar.app"},
      {"/FooBar.app/", "/FooBar.app"},
      {"//FooBar.app", "//FooBar.app"},
      {"/Foo/Bar.app", "/Foo/Bar.app"},
      {"/Foo/Bar.app/", "/Foo/Bar.app"},
      {"/F/B.app", "/F/B.app"},
      {"/F/B.app/", "/F/B.app"},
      {"/Foo/Bar.app/baz", "/Foo/Bar.app"},
      {"/Foo/Bar.app/baz/", "/Foo/Bar.app"},
      {"/Foo/Bar.app/baz/quux.app/quuux", "/Foo/Bar.app"},
      {"/Applications/Google Foo.app/bar/Foo Helper.app/quux/Foo Helper",
       "/Applications/Google Foo.app"},
  };
  for (const auto& input : valid_inputs) {
    SCOPED_TRACE(std::string("input: ") + input.in);
    out = apple::GetAppBundlePath(FilePath(input.in));
    EXPECT_FALSE(out.empty());
    EXPECT_STREQ(input.expected_out, out.value().c_str());
  }
}

TEST_F(MacUtilTest, TestGetInnermostAppBundlePath) {
  FilePath out;

  // Make sure it doesn't crash.
  out = apple::GetInnermostAppBundlePath(FilePath());
  EXPECT_TRUE(out.empty());

  // Some more invalid inputs.
  const char* const invalid_inputs[] = {
      "/",
      "/foo",
      "foo",
      "/foo/bar.",
      "foo/bar.",
      "/foo/bar./bazquux",
      "foo/bar./bazquux",
      "foo/.app",
      "//foo",
  };
  for (const auto* input : invalid_inputs) {
    SCOPED_TRACE(std::string("input: ") + input);
    out = apple::GetInnermostAppBundlePath(FilePath(input));
    EXPECT_TRUE(out.empty());
  }

  // Some valid inputs; this and |expected_outputs| should be in sync.
  struct {
    const char* in;
    const char* expected_out;
  } valid_inputs[] = {
      {"FooBar.app/", "FooBar.app"},
      {"/FooBar.app", "/FooBar.app"},
      {"/FooBar.app/", "/FooBar.app"},
      {"//FooBar.app", "//FooBar.app"},
      {"/Foo/Bar.app", "/Foo/Bar.app"},
      {"/Foo/Bar.app/", "/Foo/Bar.app"},
      {"/F/B.app", "/F/B.app"},
      {"/F/B.app/", "/F/B.app"},
      {"/Foo/Bar.app/baz", "/Foo/Bar.app"},
      {"/Foo/Bar.app/baz/", "/Foo/Bar.app"},
      {"/Foo/Bar.app/baz/quux.app/quuux", "/Foo/Bar.app/baz/quux.app"},
      {"/Applications/Google Foo.app/bar/Foo Helper.app/quux/Foo Helper",
       "/Applications/Google Foo.app/bar/Foo Helper.app"},
  };
  for (const auto& input : valid_inputs) {
    SCOPED_TRACE(std::string("input: ") + input.in);
    out = apple::GetInnermostAppBundlePath(FilePath(input.in));
    EXPECT_FALSE(out.empty());
    EXPECT_STREQ(input.expected_out, out.value().c_str());
  }
}

TEST_F(MacUtilTest, MacOSVersion) {
  int32_t major, minor, bugfix;
  base::SysInfo::OperatingSystemVersionNumbers(&major, &minor, &bugfix);

  EXPECT_EQ(major * 1'00'00 + minor * 1'00 + bugfix, MacOSVersion());
  EXPECT_EQ(major, MacOSMajorVersion());
}

TEST_F(MacUtilTest, ParseOSProductVersion) {
  // Various strings in shapes that would be expected to be returned from the
  // API that would need to be parsed.
  EXPECT_EQ(10'06'02, ParseOSProductVersionForTesting("10.6.2"));
  EXPECT_EQ(10'15'00, ParseOSProductVersionForTesting("10.15"));
  EXPECT_EQ(13'05'01, ParseOSProductVersionForTesting("13.5.1"));
  EXPECT_EQ(14'00'00, ParseOSProductVersionForTesting("14.0"));

  // Various strings in shapes that would not be expected, but that should parse
  // without CHECKing.
  EXPECT_EQ(13'04'01, ParseOSProductVersionForTesting("13.4.1 (c)"));
  EXPECT_EQ(14'00'00, ParseOSProductVersionForTesting("14.0.0"));
  EXPECT_EQ(28'00'00, ParseOSProductVersionForTesting("28"));
  EXPECT_EQ(28'03'04, ParseOSProductVersionForTesting("28.3.4.3.2.5"));

  // Various strings in shapes that are so unexpected that they should not
  // parse.
  EXPECT_DEATH_IF_SUPPORTED(ParseOSProductVersionForTesting("Mac OS X 10.0"),
                            "");
  EXPECT_DEATH_IF_SUPPORTED(ParseOSProductVersionForTesting(""), "");
  EXPECT_DEATH_IF_SUPPORTED(ParseOSProductVersionForTesting("  "), "");
  EXPECT_DEATH_IF_SUPPORTED(ParseOSProductVersionForTesting("."), "");
  EXPECT_DEATH_IF_SUPPORTED(ParseOSProductVersionForTesting("10.a.5"), "");
  EXPECT_DEATH_IF_SUPPORTED(ParseOSProductVersionForTesting("१०.१५.७"), "");
  EXPECT_DEATH_IF_SUPPORTED(ParseOSProductVersionForTesting("7.6.1"), "");
  EXPECT_DEATH_IF_SUPPORTED(ParseOSProductVersionForTesting("10.16"), "");
  EXPECT_DEATH_IF_SUPPORTED(ParseOSProductVersionForTesting("16.0"), "");
}

// Note: The `com.apple.quarantine` xattr is not API, and may break in future
// macOS releases, but is used in test code to peek behind the curtain.
constexpr char quarantine_xattr_name[] = "com.apple.quarantine";

// Sample contents of a quarantine xattr. In reality this would refer to an
// entry in the quarantine database, but for the purposes of this test, the
// general shape of this sample is what is important.
constexpr char quarantine_str[] =
    "0000;4b392bb2;Chromium;|org.chromium.Chromium";
constexpr size_t quarantine_str_len = std::size(quarantine_str) - 1;

void VerifyNoQuarantineAttribute(NSURL* url) {
  NSError* error;
  id value;
  EXPECT_TRUE([url getResourceValue:&value
                             forKey:NSURLQuarantinePropertiesKey
                              error:&error]);
  EXPECT_FALSE(value);
  EXPECT_FALSE(error);

  // Verify that the backing xattr is not present. (This is not API and might
  // break.)

  EXPECT_EQ(-1, getxattr(url.fileSystemRepresentation, quarantine_xattr_name,
                         /*value=*/nullptr,
                         /*size=*/0, /*position=*/0, /*options=*/0));
  EXPECT_EQ(ENOATTR, errno);
}

TEST_F(MacUtilTest, TestAddThenRemoveQuarantineAttribute) {
  ScopedTempDir temp_dir_;
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  FilePath example_folder_path = temp_dir_.GetPath().Append("ExampleFolder");
  ASSERT_TRUE(base::CreateDirectory(example_folder_path));
  NSURL* example_folder = apple::FilePathToNSURL(example_folder_path);

  EXPECT_EQ(0, setxattr(example_folder.fileSystemRepresentation,
                        quarantine_xattr_name, quarantine_str,
                        quarantine_str_len, /*position=*/0, /*options=*/0));
  EXPECT_EQ(static_cast<ssize_t>(quarantine_str_len),
            getxattr(example_folder.fileSystemRepresentation,
                     quarantine_xattr_name, /*value=*/nullptr,
                     /*size=*/0, /*position=*/0, /*options=*/0));

  EXPECT_TRUE(RemoveQuarantineAttribute(example_folder_path));
  VerifyNoQuarantineAttribute(example_folder);
}

TEST_F(MacUtilTest, TestAddThenRemoveQuarantineAttributeTwice) {
  ScopedTempDir temp_dir_;
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  FilePath example_folder_path = temp_dir_.GetPath().Append("ExampleFolder");
  ASSERT_TRUE(base::CreateDirectory(example_folder_path));
  NSURL* example_folder = apple::FilePathToNSURL(example_folder_path);

  EXPECT_EQ(0, setxattr(example_folder.fileSystemRepresentation,
                        quarantine_xattr_name, quarantine_str,
                        quarantine_str_len, /*position=*/0, /*options=*/0));
  EXPECT_EQ(static_cast<ssize_t>(quarantine_str_len),
            getxattr(example_folder.fileSystemRepresentation,
                     quarantine_xattr_name, /*value=*/nullptr,
                     /*size=*/0, /*position=*/0, /*options=*/0));

  // RemoveQuarantineAttribute should succeed twice: the first time at removing
  // the attribute, and the second time because there is no attribute.
  EXPECT_TRUE(RemoveQuarantineAttribute(example_folder_path));
  EXPECT_TRUE(RemoveQuarantineAttribute(example_folder_path));
  VerifyNoQuarantineAttribute(example_folder);
}

TEST_F(MacUtilTest, TestRemoveQuarantineAttributeNeverSet) {
  ScopedTempDir temp_dir_;
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  FilePath example_folder_path = temp_dir_.GetPath().Append("ExampleFolder");
  ASSERT_TRUE(base::CreateDirectory(example_folder_path));
  NSURL* example_folder = apple::FilePathToNSURL(example_folder_path);

  VerifyNoQuarantineAttribute(example_folder);

  EXPECT_TRUE(RemoveQuarantineAttribute(example_folder_path));
  VerifyNoQuarantineAttribute(example_folder);
}

TEST_F(MacUtilTest, TestRemoveQuarantineAttributeNonExistentPath) {
  ScopedTempDir temp_dir_;
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  FilePath non_existent_path = temp_dir_.GetPath().Append("ExampleFolder");

  ASSERT_FALSE(PathExists(non_existent_path));
  EXPECT_FALSE(RemoveQuarantineAttribute(non_existent_path));
}

}  // namespace

}  // namespace base::mac
