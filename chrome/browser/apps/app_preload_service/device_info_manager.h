// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_PRELOAD_SERVICE_DEVICE_INFO_MANAGER_H_
#define CHROME_BROWSER_APPS_APP_PRELOAD_SERVICE_DEVICE_INFO_MANAGER_H_

#include <ostream>
#include <string>

namespace apps {
// This class is a helper interface to get info about the device the code is
// currently running on.
class DeviceInfoManager {
 public:
  DeviceInfoManager();
  DeviceInfoManager(const DeviceInfoManager&) = delete;
  DeviceInfoManager& operator=(const DeviceInfoManager&) = delete;
  ~DeviceInfoManager();

  std::string GetBoard() const;
};

std::ostream& operator<<(std::ostream& os,
                         const DeviceInfoManager& device_info_manager);

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_PRELOAD_SERVICE_DEVICE_INFO_MANAGER_H_
