// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/chrome_network_delegate.h"

#include "base/base_paths.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "build/build_config.h"

#if defined(OS_ANDROID)
#include "base/android/path_utils.h"
#endif

namespace {

bool g_access_to_all_files_enabled = false;

bool IsAccessAllowedInternal(const base::FilePath& path,
                             const base::FilePath& profile_path) {
  if (g_access_to_all_files_enabled)
    return true;

#if !defined(OS_CHROMEOS) && !defined(OS_ANDROID)
  return true;
#else

  std::vector<base::FilePath> whitelist;
#if defined(OS_CHROMEOS)
  // Use a whitelist to only allow access to files residing in the list of
  // directories below.
  static const base::FilePath::CharType* const kLocalAccessWhiteList[] = {
      "/home/chronos/user/Downloads",
      "/home/chronos/user/MyFiles",
      "/home/chronos/user/log",
      "/home/chronos/user/WebRTC Logs",
      "/media",
      "/opt/oem",
      "/run/arc/sdcard/write/emulated/0",
      "/usr/share/chromeos-assets",
      "/var/log",
  };

  base::FilePath temp_dir;
  if (base::PathService::Get(base::DIR_TEMP, &temp_dir))
    whitelist.push_back(temp_dir);

  // The actual location of "/home/chronos/user/Xyz" is the Xyz directory under
  // the profile path ("/home/chronos/user' is a hard link to current primary
  // logged in profile.) For the support of multi-profile sessions, we are
  // switching to use explicit "$PROFILE_PATH/Xyz" path and here whitelist such
  // access.
  if (!profile_path.empty()) {
    const base::FilePath downloads = profile_path.AppendASCII("Downloads");
    whitelist.push_back(downloads);
    whitelist.push_back(profile_path.AppendASCII("MyFiles"));
    const base::FilePath webrtc_logs = profile_path.AppendASCII("WebRTC Logs");
    whitelist.push_back(webrtc_logs);
  }
#elif defined(OS_ANDROID)
  // Access to files in external storage is allowed.
  base::FilePath external_storage_path;
  base::PathService::Get(base::DIR_ANDROID_EXTERNAL_STORAGE,
                         &external_storage_path);
  if (external_storage_path.IsParent(path))
    return true;

  auto all_download_dirs = base::android::GetAllPrivateDownloadsDirectories();
  for (const auto& dir : all_download_dirs)
    whitelist.push_back(dir);

  // Whitelist of other allowed directories.
  static const base::FilePath::CharType* const kLocalAccessWhiteList[] = {
      "/sdcard", "/mnt/sdcard",
  };
#endif

  for (const auto* whitelisted_path : kLocalAccessWhiteList)
    whitelist.push_back(base::FilePath(whitelisted_path));

  for (const auto& whitelisted_path : whitelist) {
    // base::FilePath::operator== should probably handle trailing separators.
    if (whitelisted_path == path.StripTrailingSeparators() ||
        whitelisted_path.IsParent(path)) {
      return true;
    }
  }

#if defined(OS_CHROMEOS)
  // Allow access to DriveFS logs. These reside in
  // $PROFILE_PATH/GCache/v2/<opaque id>/Logs.
  base::FilePath path_within_gcache_v2;
  if (profile_path.Append("GCache/v2")
          .AppendRelativePath(path, &path_within_gcache_v2)) {
    std::vector<std::string> components;
    path_within_gcache_v2.GetComponents(&components);
    if (components.size() > 1 && components[1] == "Logs") {
      return true;
    }
  }
#endif  // defined(OS_CHROMEOS)

  DVLOG(1) << "File access denied - " << path.value().c_str();
  return false;
#endif  // !defined(OS_CHROMEOS) && !defined(OS_ANDROID)
}

}  // namespace

// static
bool ChromeNetworkDelegate::IsAccessAllowed(
    const base::FilePath& path,
    const base::FilePath& profile_path) {
  return IsAccessAllowedInternal(path, profile_path);
}

// static
bool ChromeNetworkDelegate::IsAccessAllowed(
    const base::FilePath& path,
    const base::FilePath& absolute_path,
    const base::FilePath& profile_path) {
#if defined(OS_ANDROID)
  // Android's whitelist relies on symbolic links (ex. /sdcard is whitelisted
  // and commonly a symbolic link), thus do not check absolute paths.
  return IsAccessAllowedInternal(path, profile_path);
#else
  return (IsAccessAllowedInternal(path, profile_path) &&
          IsAccessAllowedInternal(absolute_path, profile_path));
#endif
}

// static
void ChromeNetworkDelegate::EnableAccessToAllFilesForTesting(bool enabled) {
  g_access_to_all_files_enabled = enabled;
}
