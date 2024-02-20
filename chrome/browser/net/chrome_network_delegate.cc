// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/chrome_network_delegate.h"

#include "base/base_paths.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

#if BUILDFLAG(IS_CHROMEOS)
#include <fnmatch.h>
#include "base/files/file_util.h"
#include "base/system/sys_info.h"
#include "chrome/common/chrome_paths.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ambient_time_of_day_constants.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#include "base/android/path_utils.h"
#endif

namespace {

bool g_access_to_all_files_enabled = false;

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
// Returns true if |allowlist| contains |path| or a parent of |path|.
bool IsPathOnAllowlist(const base::FilePath& path,
                       const std::vector<base::FilePath>& allowlist) {
  for (const auto& allowlisted_path : allowlist) {
    // base::FilePath::operator== should probably handle trailing separators.
    if (allowlisted_path == path.StripTrailingSeparators() ||
        allowlisted_path.IsParent(path)) {
      return true;
    }
  }
  return false;
}
#endif

#if BUILDFLAG(IS_CHROMEOS)
bool IsLacrosLogFile(const base::FilePath& path) {
  return (fnmatch("/home/chronos/user/lacros/lacros*.log", path.value().c_str(),
                  FNM_NOESCAPE) == 0) ||
         (fnmatch("/var/log/lacros/lacros*.log", path.value().c_str(),
                  FNM_NOESCAPE) == 0);
}

// Returns true if access is allowed for |path| for a user with |profile_path).
bool IsAccessAllowedChromeOS(const base::FilePath& path,
                             const base::FilePath& profile_path) {
  // Allow access to DriveFS logs. These reside in
  // $PROFILE_PATH/GCache/v2/<opaque id>/Logs.
  base::FilePath path_within_gcache_v2;
  if (profile_path.Append("GCache/v2")
          .AppendRelativePath(path, &path_within_gcache_v2)) {
    std::vector<std::string> components = path_within_gcache_v2.GetComponents();
    if (components.size() > 1 && components[1] == "Logs") {
      return true;
    }
  }

  if (IsLacrosLogFile(path))
    return true;

  // Use an allowlist to only allow access to files residing in the list of
  // directories below.
  static const base::FilePath::CharType* const kLocalAccessAllowList[] = {
      "/home/chronos/user/MyFiles",
      "/home/chronos/user/WebRTC Logs",
      "/home/chronos/user/google-assistant-library/log",
      "/home/chronos/user/lacros/Crash Reports",
      "/home/chronos/user/log",
      "/home/chronos/user/crostini.icons",
      "/media",
      "/opt/oem",
      "/run/arc/sdcard/write/emulated/0",
      "/usr/share/chromeos-assets",
      "/var/log",
  };
  std::vector<base::FilePath> allowlist;
  for (const auto* allowlisted_path : kLocalAccessAllowList)
    allowlist.emplace_back(allowlisted_path);

  base::FilePath temp_dir;
  if (base::PathService::Get(base::DIR_TEMP, &temp_dir))
    allowlist.push_back(temp_dir);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // The actual location of "/home/chronos/user/Xyz" is the Xyz directory under
  // the profile path ("/home/chronos/user' is a hard link to current primary
  // logged in profile.) For the support of multi-profile sessions, we are
  // switching to use explicit "$PROFILE_PATH/Xyz" path and here allow such
  // access.
  if (!profile_path.empty()) {
    allowlist.push_back(profile_path.AppendASCII("MyFiles"));
    const base::FilePath webrtc_logs = profile_path.AppendASCII("WebRTC Logs");
    allowlist.push_back(webrtc_logs);
  }
  // For developers using the linux-chromeos emulator, the MyFiles dir is at
  // $HOME/Downloads. Ensure developers can access it for manual testing.
  if (!base::SysInfo::IsRunningOnChromeOS()) {
    base::FilePath downloads_dir;
    if (base::PathService::Get(chrome::DIR_DEFAULT_DOWNLOADS, &downloads_dir))
      allowlist.push_back(downloads_dir);
  }
  // /run/imageloader is the root directory for all DLC packages. The "timeofday" package
  // specifically contains assets required for one of ash's screen saver themes.
  allowlist.push_back(
      base::FilePath("/run/imageloader").Append(ash::kTimeOfDayDlcId));
#else
  // Lacros uses the system-level documents directory and downloads directory
  // under /home/chronos/u-<hash>, which are provided via PathService. Since
  // they are system-level, they are not subdirectories of |profile_path|.
  // PathService also provides valid paths for developers using the
  // linux-chromeos emulator.
  base::FilePath documents_dir;
  if (base::PathService::Get(chrome::DIR_USER_DOCUMENTS, &documents_dir))
    allowlist.push_back(documents_dir);

  base::FilePath downloads_dir;
  if (base::PathService::Get(chrome::DIR_DEFAULT_DOWNLOADS, &downloads_dir))
    allowlist.push_back(downloads_dir);

  // Lacros can access WebRTC logs under its browser profile directories.
  if (!profile_path.empty())
    allowlist.push_back(profile_path.AppendASCII("WebRTC Logs"));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  return IsPathOnAllowlist(path, allowlist);
}
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_ANDROID)
// Returns true if access is allowed for |path|.
bool IsAccessAllowedAndroid(const base::FilePath& path) {
  // Access to files in external storage is allowed.
  base::FilePath external_storage_path;
  base::PathService::Get(base::DIR_ANDROID_EXTERNAL_STORAGE,
                         &external_storage_path);
  if (external_storage_path.IsParent(path))
    return true;

  std::vector<base::FilePath> allowlist;
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
  for (const auto* allowlisted_path : kLocalAccessAllowList)
    allowlist.emplace_back(allowlisted_path);

  return IsPathOnAllowlist(path, allowlist);
}
#endif  // BUILDFLAG(IS_ANDROID)

bool IsAccessAllowedInternal(const base::FilePath& path,
                             const base::FilePath& profile_path) {
  if (g_access_to_all_files_enabled)
    return true;

#if BUILDFLAG(IS_CHROMEOS)
  return IsAccessAllowedChromeOS(path, profile_path);
#elif BUILDFLAG(IS_ANDROID)
  return IsAccessAllowedAndroid(path);
#else
  return true;
#endif
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
#if BUILDFLAG(IS_ANDROID)
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
