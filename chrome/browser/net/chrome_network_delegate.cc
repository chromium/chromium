// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/chrome_network_delegate.h"

#include "base/base_paths.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "build/build_config.h"

#if defined(OS_CHROMEOS)
#include "base/system/sys_info.h"
#include "chrome/browser/download/download_prefs.h"
#endif

#if defined(OS_ANDROID)
#include "base/android/build_info.h"
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

  std::vector<base::FilePath> allowlist;
#if defined(OS_CHROMEOS)
  // Use an allowlist to only allow access to files residing in the list of
  // directories below.
  static const base::FilePath::CharType* const kLocalAccessAllowList[] = {
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
    allowlist.push_back(temp_dir);

  // The actual location of "/home/chronos/user/Xyz" is the Xyz directory under
  // the profile path ("/home/chronos/user' is a hard link to current primary
  // logged in profile.) For the support of multi-profile sessions, we are
  // switching to use explicit "$PROFILE_PATH/Xyz" path and here allow such
  // access.
  if (!profile_path.empty()) {
    const base::FilePath downloads = profile_path.AppendASCII("Downloads");
    allowlist.push_back(downloads);
    allowlist.push_back(profile_path.AppendASCII("MyFiles"));
    const base::FilePath webrtc_logs = profile_path.AppendASCII("WebRTC Logs");
    allowlist.push_back(webrtc_logs);
  }

  // In linux-chromeos, MyFiles dir is at $HOME/Downloads.
  if (!base::SysInfo::IsRunningOnChromeOS())
    allowlist.push_back(DownloadPrefs::GetDefaultDownloadDirectory());

#elif defined(OS_ANDROID)
  // Access to files in external storage is allowed.
  base::FilePath external_storage_path;
  base::PathService::Get(base::DIR_ANDROID_EXTERNAL_STORAGE,
                         &external_storage_path);
  if (external_storage_path.IsParent(path))
    return true;

  std::vector<base::FilePath> all_download_dirs =
      base::android::GetAllPrivateDownloadsDirectories();
  allowlist.insert(allowlist.end(), all_download_dirs.begin(),
                   all_download_dirs.end());

  base::android::BuildInfo* build_info =
      base::android::BuildInfo::GetInstance();
  if (build_info->sdk_int() > base::android::SDK_VERSION_Q) {
    std::vector<base::FilePath> all_external_download_volumes =
        base::android::GetSecondaryStorageDownloadDirectories();
    allowlist.insert(allowlist.end(), all_external_download_volumes.begin(),
                     all_external_download_volumes.end());
  }

  // allowlist of other allowed directories.
  static const base::FilePath::CharType* const kLocalAccessAllowList[] = {
      "/sdcard",
      "/mnt/sdcard",
  };
#endif

  for (const auto* allowlisted_path : kLocalAccessAllowList)
    allowlist.push_back(base::FilePath(allowlisted_path));

  for (const auto& allowlisted_path : allowlist) {
    // base::FilePath::operator== should probably handle trailing separators.
    if (allowlisted_path == path.StripTrailingSeparators() ||
        allowlisted_path.IsParent(path)) {
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
  // Android's allowlist relies on symbolic links (ex. /sdcard is allowed
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
