// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "base/mac/mac_util.h"

#import <Cocoa/Cocoa.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/xattr.h>

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
    "/", "/foo", "foo", "/foo/bar.", "foo/bar.", "/foo/bar./bazquux",
    "foo/bar./bazquux", "foo/.app", "//foo",
  };
  for (size_t i = 0; i < std::size(invalid_inputs); i++) {
    out = apple::GetAppBundlePath(FilePath(invalid_inputs[i]));
    EXPECT_TRUE(out.empty()) << "loop: " << i;
  }

  // Some valid inputs; this and |expected_outputs| should be in sync.
  struct {
    const char *in;
    const char *expected_out;
  } valid_inputs[] = {
    { "FooBar.app/", "FooBar.app" },
    { "/FooBar.app", "/FooBar.app" },
    { "/FooBar.app/", "/FooBar.app" },
    { "//FooBar.app", "//FooBar.app" },
    { "/Foo/Bar.app", "/Foo/Bar.app" },
    { "/Foo/Bar.app/", "/Foo/Bar.app" },
    { "/F/B.app", "/F/B.app" },
    { "/F/B.app/", "/F/B.app" },
    { "/Foo/Bar.app/baz", "/Foo/Bar.app" },
    { "/Foo/Bar.app/baz/", "/Foo/Bar.app" },
    { "/Foo/Bar.app/baz/quux.app/quuux", "/Foo/Bar.app" },
    { "/Applications/Google Foo.app/bar/Foo Helper.app/quux/Foo Helper",
        "/Applications/Google Foo.app" },
  };
  for (size_t i = 0; i < std::size(valid_inputs); i++) {
    out = apple::GetAppBundlePath(FilePath(valid_inputs[i].in));
    EXPECT_FALSE(out.empty()) << "loop: " << i;
    EXPECT_STREQ(valid_inputs[i].expected_out,
        out.value().c_str()) << "loop: " << i;
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
  for (size_t i = 0; i < std::size(invalid_inputs); i++) {
    SCOPED_TRACE(testing::Message()
                 << "case #" << i << ", input: " << invalid_inputs[i]);
    out = apple::GetInnermostAppBundlePath(FilePath(invalid_inputs[i]));
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
  for (size_t i = 0; i < std::size(valid_inputs); i++) {
    SCOPED_TRACE(testing::Message()
                 << "case #" << i << ", input " << valid_inputs[i].in);
    out = apple::GetInnermostAppBundlePath(FilePath(valid_inputs[i].in));
    EXPECT_FALSE(out.empty());
    EXPECT_STREQ(valid_inputs[i].expected_out, out.value().c_str());
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
  EXPECT_EQ(18'00'00, ParseOSProductVersionForTesting("18"));
  EXPECT_EQ(18'03'04, ParseOSProductVersionForTesting("18.3.4.3.2.5"));

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
}

TEST_F(MacUtilTest, TestRemoveQuarantineAttribute) {
  ScopedTempDir temp_dir_;
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  FilePath dummy_folder_path = temp_dir_.GetPath().Append("DummyFolder");
  ASSERT_TRUE(base::CreateDirectory(dummy_folder_path));
  const char* quarantine_str = "0000;4b392bb2;Chromium;|org.chromium.Chromium";
  const char* file_path_str = dummy_folder_path.value().c_str();
  EXPECT_EQ(0, setxattr(file_path_str, "com.apple.quarantine",
      quarantine_str, strlen(quarantine_str), 0, 0));
  EXPECT_EQ(static_cast<long>(strlen(quarantine_str)),
            getxattr(file_path_str, "com.apple.quarantine", /*value=*/nullptr,
                     /*size=*/0, /*position=*/0, /*options=*/0));
  EXPECT_TRUE(RemoveQuarantineAttribute(dummy_folder_path));
  EXPECT_EQ(-1,
            getxattr(file_path_str, "com.apple.quarantine", /*value=*/nullptr,
                     /*size=*/0, /*position=*/0, /*options=*/0));
  EXPECT_EQ(ENOATTR, errno);
}

TEST_F(MacUtilTest, TestRemoveQuarantineAttributeTwice) {
  ScopedTempDir temp_dir_;
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  FilePath dummy_folder_path = temp_dir_.GetPath().Append("DummyFolder");
  const char* file_path_str = dummy_folder_path.value().c_str();
  ASSERT_TRUE(base::CreateDirectory(dummy_folder_path));
  EXPECT_EQ(-1,
            getxattr(file_path_str, "com.apple.quarantine", /*value=*/nullptr,
                     /*size=*/0, /*position=*/0, /*options=*/0));
  // No quarantine attribute to begin with, but RemoveQuarantineAttribute still
  // succeeds because in the end the folder still doesn't have the quarantine
  // attribute set.
  EXPECT_TRUE(RemoveQuarantineAttribute(dummy_folder_path));
  EXPECT_TRUE(RemoveQuarantineAttribute(dummy_folder_path));
  EXPECT_EQ(ENOATTR, errno);
}

TEST_F(MacUtilTest, TestRemoveQuarantineAttributeNonExistentPath) {
  ScopedTempDir temp_dir_;
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  FilePath non_existent_path = temp_dir_.GetPath().Append("DummyPath");
  ASSERT_FALSE(PathExists(non_existent_path));
  EXPECT_FALSE(RemoveQuarantineAttribute(non_existent_path));
}

}  // namespace

}  // namespace base::mac
