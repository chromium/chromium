// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/mac/mac_util.h"

#import <Cocoa/Cocoa.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/xattr.h>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/system/sys_info.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace base::mac {

namespace {

using MacUtilTest = PlatformTest;

TEST_F(MacUtilTest, GetUserDirectoryTest) {
  // Try a few keys, make sure they come back with non-empty paths.
  FilePath caches_dir;
  EXPECT_TRUE(GetUserDirectory(NSCachesDirectory, &caches_dir));
  EXPECT_FALSE(caches_dir.empty());

  FilePath application_support_dir;
  EXPECT_TRUE(GetUserDirectory(NSApplicationSupportDirectory,
                               &application_support_dir));
  EXPECT_FALSE(application_support_dir.empty());

  FilePath library_dir;
  EXPECT_TRUE(GetUserDirectory(NSLibraryDirectory, &library_dir));
  EXPECT_FALSE(library_dir.empty());
}

TEST_F(MacUtilTest, TestLibraryPath) {
  FilePath library_dir = GetUserLibraryPath();
  // Make sure the string isn't empty.
  EXPECT_FALSE(library_dir.value().empty());
}

TEST_F(MacUtilTest, TestGetAppBundlePath) {
  FilePath out;

  // Make sure it doesn't crash.
  out = GetAppBundlePath(FilePath());
  EXPECT_TRUE(out.empty());

  // Some more invalid inputs.
  const char* const invalid_inputs[] = {
    "/", "/foo", "foo", "/foo/bar.", "foo/bar.", "/foo/bar./bazquux",
    "foo/bar./bazquux", "foo/.app", "//foo",
  };
  for (size_t i = 0; i < std::size(invalid_inputs); i++) {
    out = GetAppBundlePath(FilePath(invalid_inputs[i]));
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
    out = GetAppBundlePath(FilePath(valid_inputs[i].in));
    EXPECT_FALSE(out.empty()) << "loop: " << i;
    EXPECT_STREQ(valid_inputs[i].expected_out,
        out.value().c_str()) << "loop: " << i;
  }
}

TEST_F(MacUtilTest, TestGetInnermostAppBundlePath) {
  FilePath out;

  // Make sure it doesn't crash.
  out = GetInnermostAppBundlePath(FilePath());
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
    out = GetInnermostAppBundlePath(FilePath(invalid_inputs[i]));
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
    out = GetInnermostAppBundlePath(FilePath(valid_inputs[i].in));
    EXPECT_FALSE(out.empty());
    EXPECT_STREQ(valid_inputs[i].expected_out, out.value().c_str());
  }
}

TEST_F(MacUtilTest, IsOSEllipsis) {
  int32_t major, minor, bugfix;
  base::SysInfo::OperatingSystemVersionNumbers(&major, &minor, &bugfix);

  // The patterns here are:
  // - FALSE/FALSE/TRUE (it is not the earlier version, it is not "at most" the
  //   earlier version, it is "at least" the earlier version)
  // - TRUE/TRUE/TRUE (it is the same version, it is "at most" the same version,
  //   it is "at least" the same version)
  // - FALSE/TRUE/FALSE (it is not the later version, it is "at most" the later
  //   version, it is not "at least" the later version)

#define TEST_FOR_PAST_OS(V)      \
  EXPECT_FALSE(IsOS##V());       \
  EXPECT_FALSE(IsAtMostOS##V()); \
  EXPECT_TRUE(IsAtLeastOS##V());

#define TEST_FOR_SAME_OS(V)     \
  EXPECT_TRUE(IsOS##V());       \
  EXPECT_TRUE(IsAtMostOS##V()); \
  EXPECT_TRUE(IsAtLeastOS##V());

#define TEST_FOR_FUTURE_OS(V)   \
  EXPECT_FALSE(IsOS##V());      \
  EXPECT_TRUE(IsAtMostOS##V()); \
  EXPECT_FALSE(IsAtLeastOS##V());

  if (major == 10) {
    if (minor == 15) {
      EXPECT_TRUE(IsOS10_15());

      TEST_FOR_FUTURE_OS(11);
      TEST_FOR_FUTURE_OS(12);
      TEST_FOR_FUTURE_OS(13);
      TEST_FOR_FUTURE_OS(14);
    } else {
      // macOS 10.15 was the end of the line.
      FAIL() << "Unexpected 10.x macOS.";
    }
  } else if (major == 11) {
    EXPECT_FALSE(IsOS10_15());

    TEST_FOR_SAME_OS(11);
    TEST_FOR_FUTURE_OS(12);
    TEST_FOR_FUTURE_OS(13);
    TEST_FOR_FUTURE_OS(14);
  } else if (major == 12) {
    EXPECT_FALSE(IsOS10_15());

    TEST_FOR_PAST_OS(11);
    TEST_FOR_SAME_OS(12);
    TEST_FOR_FUTURE_OS(13);
    TEST_FOR_FUTURE_OS(14);
  } else if (major == 13) {
    EXPECT_FALSE(IsOS10_15());

    TEST_FOR_PAST_OS(11);
    TEST_FOR_PAST_OS(12);
    TEST_FOR_SAME_OS(13);
    TEST_FOR_FUTURE_OS(14);
  } else if (major == 14) {
    EXPECT_FALSE(IsOS10_15());

    TEST_FOR_PAST_OS(11);
    TEST_FOR_PAST_OS(12);
    TEST_FOR_PAST_OS(13);
    TEST_FOR_SAME_OS(14);
  } else {
    // The spooky future.
    FAIL() << "Time to update the OS macros!";
  }
}

#undef TEST_FOR_PAST_10_OS
#undef TEST_FOR_PAST_OS
#undef TEST_FOR_SAME_10_OS
#undef TEST_FOR_SAME_OS
#undef TEST_FOR_FUTURE_10_OS
#undef TEST_FOR_FUTURE_OS

TEST_F(MacUtilTest, ParseModelIdentifier) {
  std::string model;
  int32_t major = 1, minor = 2;

  EXPECT_FALSE(ParseModelIdentifier("", &model, &major, &minor));
  EXPECT_EQ(0U, model.length());
  EXPECT_EQ(1, major);
  EXPECT_EQ(2, minor);
  EXPECT_FALSE(ParseModelIdentifier("FooBar", &model, &major, &minor));

  EXPECT_TRUE(ParseModelIdentifier("MacPro4,1", &model, &major, &minor));
  EXPECT_EQ(model, "MacPro");
  EXPECT_EQ(4, major);
  EXPECT_EQ(1, minor);

  EXPECT_TRUE(ParseModelIdentifier("MacBookPro6,2", &model, &major, &minor));
  EXPECT_EQ(model, "MacBookPro");
  EXPECT_EQ(6, major);
  EXPECT_EQ(2, minor);
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
