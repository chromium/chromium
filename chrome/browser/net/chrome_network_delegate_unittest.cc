// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/chrome_network_delegate.h"

#include "base/base_paths.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_ANDROID)
#include "base/base_paths_android.h"
#endif

namespace {

// Helper function to make the IsAccessAllowed test concise.
bool IsAccessAllowed(const std::string& path,
                     const std::string& profile_path) {
  return ChromeNetworkDelegate::IsAccessAllowed(
      base::FilePath::FromUTF8Unsafe(path),
      base::FilePath::FromUTF8Unsafe(profile_path));
}

}  // namespace

TEST(ChromeNetworkDelegateStaticTest, IsAccessAllowed) {
#if !defined(OS_CHROMEOS) && !defined(OS_ANDROID)
  // Platforms other than Chrome OS and Android have access to any files.
  EXPECT_TRUE(IsAccessAllowed("/", ""));
  EXPECT_TRUE(IsAccessAllowed("/foo.txt", ""));
#endif

#if defined(OS_CHROMEOS) || defined(OS_ANDROID)
  // Chrome OS and Android don't have access to random files.
  EXPECT_FALSE(IsAccessAllowed("/", ""));
  EXPECT_FALSE(IsAccessAllowed("/foo.txt", ""));
  // Empty path should not be allowed.
  EXPECT_FALSE(IsAccessAllowed("", ""));
#endif

#if defined(OS_CHROMEOS)
  base::FilePath temp_dir;
  ASSERT_TRUE(base::PathService::Get(base::DIR_TEMP, &temp_dir));
  // Chrome OS allows the following directories.
  EXPECT_TRUE(IsAccessAllowed("/home/chronos/user/Downloads", ""));
  EXPECT_TRUE(IsAccessAllowed("/home/chronos/user/MyFiles", ""));
  EXPECT_TRUE(IsAccessAllowed("/home/chronos/user/MyFiles/file.pdf", ""));
  EXPECT_TRUE(IsAccessAllowed("/home/chronos/user/log", ""));
  EXPECT_TRUE(IsAccessAllowed("/home/chronos/user/WebRTC Logs", ""));
  EXPECT_TRUE(IsAccessAllowed("/media", ""));
  EXPECT_TRUE(IsAccessAllowed("/opt/oem", ""));
  EXPECT_TRUE(IsAccessAllowed("/usr/share/chromeos-assets", ""));
  EXPECT_TRUE(IsAccessAllowed(temp_dir.AsUTF8Unsafe(), ""));
  EXPECT_TRUE(IsAccessAllowed("/var/log", ""));
  // Files under the directories are allowed.
  EXPECT_TRUE(IsAccessAllowed("/var/log/foo.txt", ""));
  // Make sure similar paths are not allowed.
  EXPECT_FALSE(IsAccessAllowed("/home/chronos/user/log.txt", ""));
  EXPECT_FALSE(IsAccessAllowed("/home/chronos/user", ""));
  EXPECT_FALSE(IsAccessAllowed("/home/chronos", ""));

  // If profile path is given, the following additional paths are allowed.
  EXPECT_TRUE(IsAccessAllowed("/profile/Downloads", "/profile"));
  EXPECT_TRUE(IsAccessAllowed("/profile/MyFiles", "/profile"));
  EXPECT_TRUE(IsAccessAllowed("/profile/MyFiles/file.pdf", "/profile"));
  EXPECT_TRUE(IsAccessAllowed("/profile/WebRTC Logs", "/profile"));

  // GCache/v2/<opaque ID>/Logs is allowed.
  EXPECT_TRUE(IsAccessAllowed("/profile/GCache/v2/id/Logs", "/profile"));
  EXPECT_TRUE(
      IsAccessAllowed("/profile/GCache/v2/id/Logs/drivefs.txt", "/profile"));
  EXPECT_FALSE(
      IsAccessAllowed("/profile/GCache/v2/id/logs/drivefs.txt", "/profile"));
  EXPECT_FALSE(
      IsAccessAllowed("/profile/GCache/v2/id/something_else", "/profile"));
  EXPECT_FALSE(IsAccessAllowed("/profile/GCache/v2/id", "/profile"));
  EXPECT_FALSE(IsAccessAllowed("/profile/GCache/v2", "/profile"));
  EXPECT_FALSE(IsAccessAllowed("/home/chronos/user/GCache/v2/id/Logs", ""));

#elif defined(OS_ANDROID)
  // Android allows the following directories.
  EXPECT_TRUE(IsAccessAllowed("/sdcard", ""));
  EXPECT_TRUE(IsAccessAllowed("/mnt/sdcard", ""));
  // Files under the directories are allowed.
  EXPECT_TRUE(IsAccessAllowed("/sdcard/foo.txt", ""));
  // Make sure similar paths are not allowed.
  EXPECT_FALSE(IsAccessAllowed("/mnt/sdcard.txt", ""));
  EXPECT_FALSE(IsAccessAllowed("/mnt", ""));

  // Files in external storage are allowed.
  base::FilePath external_storage_path;
  base::PathService::Get(base::DIR_ANDROID_EXTERNAL_STORAGE,
                         &external_storage_path);
  EXPECT_TRUE(IsAccessAllowed(
      external_storage_path.AppendASCII("foo.txt").AsUTF8Unsafe(), ""));
  // The external storage root itself is not allowed.
  EXPECT_FALSE(IsAccessAllowed(external_storage_path.AsUTF8Unsafe(), ""));
#endif
}
