// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_BROWSER_UTIL_H_
#define CHROME_BROWSER_ASH_CROSAPI_BROWSER_UTIL_H_

namespace base {
class FilePath;
class Version;
}  // namespace base

namespace component_updater {
class ComponentUpdateService;
}  // namespace component_updater

namespace version_info {
enum class Channel;
}  // namespace version_info

// These methods are used by ash-chrome.
namespace crosapi::browser_util {

// Returns the user directory for lacros-chrome.
base::FilePath GetUserDataDir();

// Returns true if the Lacros feature is enabled for the primary user.
bool IsLacrosEnabled();

// Gets the version of the rootfs lacros-chrome. By reading the metadata json
// file in the correct format.
base::Version GetRootfsLacrosVersionMayBlock(
    const base::FilePath& version_file_path);

// Returns the currently installed version of lacros-chrome managed by the
// component updater. Will return an empty / invalid version if no lacros
// component is found.
base::Version GetInstalledLacrosComponentVersion(
    const component_updater::ComponentUpdateService* component_update_service);

}  // namespace crosapi::browser_util

#endif  // CHROME_BROWSER_ASH_CROSAPI_BROWSER_UTIL_H_
