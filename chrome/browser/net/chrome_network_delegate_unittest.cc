// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/chrome_network_delegate.h"

#include "base/base_paths.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "base/files/file_util.h"
#include "base/system/sys_info.h"
#include "base/test/scoped_running_on_chromeos.h"
#include "base/time/time.h"
#include "chrome/common/chrome_paths.h"
#endif

#if BUILDFLAG(IS_ANDROID)
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
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
  // Chrome OS and Android don't have access to random files.
  EXPECT_FALSE(IsAccessAllowed("/", ""));
  EXPECT_FALSE(IsAccessAllowed("/foo.txt", ""));
  // Empty path should not be allowed.
  EXPECT_FALSE(IsAccessAllowed("", ""));
#else
  // Platforms other than Chrome OS and Android have access to any files.
  EXPECT_TRUE(IsAccessAllowed("/", ""));
  EXPECT_TRUE(IsAccessAllowed("/foo.txt", ""));
#endif

#if BUILDFLAG(IS_CHROMEOS)
  base::FilePath temp_dir;
  ASSERT_TRUE(base::PathService::Get(base::DIR_TEMP, &temp_dir));
  // Chrome OS allows the following directories.
  EXPECT_TRUE(IsAccessAllowed("/home/chronos/user/MyFiles/Downloads", ""));
  EXPECT_TRUE(IsAccessAllowed("/home/chronos/user/MyFiles", ""));
  EXPECT_TRUE(IsAccessAllowed("/home/chronos/user/MyFiles/file.pdf", ""));
  EXPECT_TRUE(IsAccessAllowed("/home/chronos/user/WebRTC Logs", ""));
  EXPECT_TRUE(
      IsAccessAllowed("/home/chronos/user/google-assistant-library/log", ""));
  EXPECT_TRUE(IsAccessAllowed("/home/chronos/user/lacros/lacros.log", ""));
  EXPECT_TRUE(IsAccessAllowed("/home/chronos/user/log", ""));
  EXPECT_TRUE(IsAccessAllowed("/media", ""));
  EXPECT_TRUE(IsAccessAllowed("/opt/oem", ""));
  EXPECT_TRUE(IsAccessAllowed("/usr/share/chromeos-assets", ""));
  EXPECT_TRUE(IsAccessAllowed(temp_dir.AsUTF8Unsafe(), ""));
  EXPECT_TRUE(IsAccessAllowed("/var/log", ""));
  EXPECT_TRUE(IsAccessAllowed("/var/log/lacros/lacros.log", ""));
  // Files under the directories are allowed.
  EXPECT_TRUE(IsAccessAllowed("/var/log/foo.txt", ""));
  // Make sure similar paths are not allowed.
  EXPECT_FALSE(IsAccessAllowed("/home/chronos/user/Downloads", ""));
  EXPECT_FALSE(IsAccessAllowed("/home/chronos/user/log.txt", ""));
  EXPECT_FALSE(IsAccessAllowed("/home/chronos/user", ""));
  EXPECT_FALSE(IsAccessAllowed("/home/chronos", ""));
  EXPECT_FALSE(IsAccessAllowed("/home/chronos/user/lacros/lacros.txt", ""));
  EXPECT_FALSE(IsAccessAllowed("/home/chronos/user/lacros/lacros", ""));
  EXPECT_FALSE(IsAccessAllowed("/home/chronos/user/lacros/*", ""));
  EXPECT_FALSE(IsAccessAllowed("/home/chronos/user/lacros", ""));
  EXPECT_FALSE(
      IsAccessAllowed("/home/chronos/user/lacros/subdir/lacros.log", ""));

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // If profile path is given, the following additional paths are allowed.
  EXPECT_TRUE(IsAccessAllowed("/profile/MyFiles/Downloads", "/profile"));
  EXPECT_TRUE(IsAccessAllowed("/profile/MyFiles", "/profile"));
  EXPECT_TRUE(IsAccessAllowed("/profile/MyFiles/file.pdf", "/profile"));
  EXPECT_FALSE(IsAccessAllowed("/profile/Downloads", "/profile"));

  // $HOME/Downloads is allowed for linux-chromeos, but not on devices.
  base::FilePath downloads_dir;
  base::PathService::Get(chrome::DIR_DEFAULT_DOWNLOADS, &downloads_dir);
  EXPECT_TRUE(IsAccessAllowed(downloads_dir.AsUTF8Unsafe(), ""));
  {
    base::test::ScopedRunningOnChromeOS running_on_chromeos;
    EXPECT_FALSE(IsAccessAllowed(downloads_dir.AsUTF8Unsafe(), ""));
  }
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // System level documents and downloads directories are allowed.
  base::FilePath documents_dir;
  base::PathService::Get(chrome::DIR_USER_DOCUMENTS, &documents_dir);
  EXPECT_TRUE(IsAccessAllowed(documents_dir.AsUTF8Unsafe(), ""));
  base::FilePath downloads_dir;
  base::PathService::Get(chrome::DIR_DEFAULT_DOWNLOADS, &downloads_dir);
  EXPECT_TRUE(IsAccessAllowed(downloads_dir.AsUTF8Unsafe(), ""));
#endif

  // WebRTC logs for the current profile are allowed.
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

#elif BUILDFLAG(IS_ANDROID)
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
