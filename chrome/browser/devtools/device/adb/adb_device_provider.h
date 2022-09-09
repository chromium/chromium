// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVICE_ADB_ADB_DEVICE_PROVIDER_H_
#define CHROME_BROWSER_DEVTOOLS_DEVICE_ADB_ADB_DEVICE_PROVIDER_H_

#include "chrome/browser/devtools/device/android_device_manager.h"

class AdbDeviceProvider : public AndroidDeviceManager::DeviceProvider {
 public:
  void QueryDevices(SerialsCallback callback) override;

  void QueryDeviceInfo(const std::string& serial,
                       DeviceInfoCallback callback) override;

  void OpenSocket(const std::string& serial,
                  const std::string& socket_name,
                  SocketCallback callback) override;

 private:
  ~AdbDeviceProvider() override;
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVICE_ADB_ADB_DEVICE_PROVIDER_H_
