// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_preload_service/device_info_manager.h"

#include "base/system/sys_info.h"

namespace apps {

DeviceInfoManager::DeviceInfoManager() = default;

DeviceInfoManager::~DeviceInfoManager() = default;

std::string DeviceInfoManager::GetBoard() const {
  return base::SysInfo::HardwareModelName();
}

std::ostream& operator<<(std::ostream& os,
                         const DeviceInfoManager& device_info_manager) {
  os << "Device info Manager:" << std::endl;
  os << "- Board: " << device_info_manager.GetBoard();
  return os;
}

}  // namespace apps
