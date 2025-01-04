// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/browser_util.h"

#include <string>
#include <string_view>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/values_util.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chromeos/crosapi/cpp/crosapi_constants.h"
#include "components/exo/shell_surface_util.h"
#include "components/version_info/channel.h"
#include "components/version_info/version_info.h"

namespace crosapi::browser_util {

namespace {

// The rootfs lacros-chrome metadata keys.
constexpr char kLacrosMetadataContentKey[] = "content";
constexpr char kLacrosMetadataVersionKey[] = "version";

}  // namespace

base::FilePath GetUserDataDir() {
  if (base::SysInfo::IsRunningOnChromeOS()) {
    // NOTE: On device this function is privacy/security sensitive. The
    // directory must be inside the encrypted user partition.
    return base::FilePath(crosapi::kLacrosUserDataPath);
  }
  // For developers on Linux desktop, put the directory under the developer's
  // specified --user-data-dir.
  base::FilePath base_path;
  base::PathService::Get(chrome::DIR_USER_DATA, &base_path);
  return base_path.Append("lacros");
}

bool IsLacrosEnabled() {
  return false;
}

bool IsAshWebBrowserEnabled() {
  return true;
}

bool IsLacrosWindow(const aura::Window* window) {
  const std::string* app_id = exo::GetShellApplicationId(window);
  if (!app_id)
    return false;
  return base::StartsWith(*app_id, kLacrosAppIdPrefix);
}

base::Version GetRootfsLacrosVersionMayBlock(
    const base::FilePath& version_file_path) {
  if (!base::PathExists(version_file_path)) {
    LOG(WARNING) << "The rootfs lacros-chrome metadata is missing.";
    return {};
  }

  std::string metadata;
  if (!base::ReadFileToString(version_file_path, &metadata)) {
    PLOG(WARNING) << "Failed to read rootfs lacros-chrome metadata.";
    return {};
  }

  std::optional<base::Value> v = base::JSONReader::Read(metadata);
  if (!v || !v->is_dict()) {
    LOG(WARNING) << "Failed to parse rootfs lacros-chrome metadata.";
    return {};
  }

  const base::Value::Dict& dict = v->GetDict();
  const base::Value::Dict* content = dict.FindDict(kLacrosMetadataContentKey);
  if (!content) {
    LOG(WARNING)
        << "Failed to parse rootfs lacros-chrome metadata content key.";
    return {};
  }

  const std::string* version = content->FindString(kLacrosMetadataVersionKey);
  if (!version) {
    LOG(WARNING)
        << "Failed to parse rootfs lacros-chrome metadata version key.";
    return {};
  }

  return base::Version{*version};
}

}  // namespace crosapi::browser_util
