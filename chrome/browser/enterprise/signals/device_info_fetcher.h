// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_SIGNALS_DEVICE_INFO_FETCHER_H_
#define CHROME_BROWSER_ENTERPRISE_SIGNALS_DEVICE_INFO_FETCHER_H_

#include <memory>
#include <string>
#include <vector>

#include "chrome/browser/enterprise/signals/signals_common.h"

namespace enterprise_signals {

struct DeviceInfo {
  DeviceInfo();
  ~DeviceInfo();
  DeviceInfo(const DeviceInfo&);
  DeviceInfo(DeviceInfo&&);

  std::string os_name;
  std::string os_version;
  std::string device_host_name;
  std::string device_model;
  std::string serial_number;
  SettingValue screen_lock_secured;
  SettingValue disk_encrypted;

  std::vector<std::string> mac_addresses;
};

// Interface used by the chrome.enterprise.reportingPrivate.getDeviceInfo()
// function that fetches info of the device. Each supported platform has its own
// subclass implementation.
class DeviceInfoFetcher {
 public:
  DeviceInfoFetcher();
  virtual ~DeviceInfoFetcher();

  DeviceInfoFetcher(const DeviceInfoFetcher&) = delete;
  DeviceInfoFetcher& operator=(const DeviceInfoFetcher&) = delete;

  // Returns a platform specific instance of DeviceInfoFetcher.
  static std::unique_ptr<DeviceInfoFetcher> CreateInstance();

  // Returns a stub instance so tests can validate attributes independently of
  // the platform.
  static std::unique_ptr<DeviceInfoFetcher> CreateStubInstanceForTesting();

  // Fetches the device information for the current platform.
  virtual DeviceInfo Fetch() = 0;
};

}  // namespace enterprise_signals

#endif  // CHROME_BROWSER_ENTERPRISE_SIGNALS_DEVICE_INFO_FETCHER_H_
