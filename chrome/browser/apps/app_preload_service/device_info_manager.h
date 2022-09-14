// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_PRELOAD_SERVICE_DEVICE_INFO_MANAGER_H_
#define CHROME_BROWSER_APPS_APP_PRELOAD_SERVICE_DEVICE_INFO_MANAGER_H_

#include <ostream>
#include <string>

#include "base/memory/raw_ptr.h"

class Profile;

namespace apps {
// This class is a helper interface to get info about the device the code is
// currently running on.
class DeviceInfoManager {
 public:
  explicit DeviceInfoManager(Profile* profile);
  DeviceInfoManager(const DeviceInfoManager&) = delete;
  DeviceInfoManager& operator=(const DeviceInfoManager&) = delete;
  ~DeviceInfoManager();

  // Returns the board family of the device. e.g. "brya"
  std::string GetBoard() const;

  // Returns the Chrome browser version of the device. e.g. "107.0.5296.0"
  std::string GetChromeVersion() const;

  // Returns the ChromeOS platform version of the device. e.g. "15088.0.0"
  std::string GetChromeOsPlatformVersion() const;

  // Returns the User Type of the profile currently running. e.g. "unmanaged"
  std::string GetUserType() const;

 private:
  base::raw_ptr<Profile> profile_;
};

std::ostream& operator<<(std::ostream& os,
                         const DeviceInfoManager& device_info_manager);

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_PRELOAD_SERVICE_DEVICE_INFO_MANAGER_H_
