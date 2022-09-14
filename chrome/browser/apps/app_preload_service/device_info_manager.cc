// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_preload_service/device_info_manager.h"

#include "base/system/sys_info.h"
#include "chrome/browser/apps/user_type_filter.h"
#include "chromeos/version/version_loader.h"
#include "components/version_info/version_info.h"

namespace apps {

DeviceInfoManager::DeviceInfoManager(Profile* profile) : profile_(profile) {}

DeviceInfoManager::~DeviceInfoManager() = default;

std::string DeviceInfoManager::GetBoard() const {
  return base::SysInfo::HardwareModelName();
}

std::string DeviceInfoManager::GetChromeVersion() const {
  return version_info::GetVersionNumber();
}

std::string DeviceInfoManager::GetChromeOsPlatformVersion() const {
  return chromeos::version_loader::GetVersion(
      chromeos::version_loader::VERSION_SHORT);
}

std::string DeviceInfoManager::GetUserType() const {
  return apps::DetermineUserType(profile_);
}

std::ostream& operator<<(std::ostream& os,
                         const DeviceInfoManager& device_info_manager) {
  os << "Device info Manager:" << std::endl;
  os << "- Board: " << device_info_manager.GetBoard() << std::endl;
  os << "- Versions: " << std::endl;
  os << "  - Ash Chrome: " << device_info_manager.GetChromeVersion()
     << std::endl;
  os << "  - Platform: " << device_info_manager.GetChromeOsPlatformVersion()
     << std::endl;
  os << "- User Type: " << device_info_manager.GetUserType() << std::endl;
  return os;
}

}  // namespace apps
