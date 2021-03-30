// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/signals/device_info_fetcher.h"

#include "build/build_config.h"

#if defined(OS_MAC)
#include "chrome/browser/enterprise/signals/device_info_fetcher_mac.h"
#elif defined(OS_WIN)
#include "chrome/browser/enterprise/signals/device_info_fetcher_win.h"
#elif defined(OS_LINUX) || defined(OS_CHROMEOS)
#include "chrome/browser/enterprise/signals/device_info_fetcher_linux.h"
#endif

namespace enterprise_signals {

namespace {

// Stub implementation of DeviceInfoFetcher.
class StubDeviceFetcher : public DeviceInfoFetcher {
 public:
  StubDeviceFetcher() = default;
  ~StubDeviceFetcher() override = default;

  StubDeviceFetcher(const StubDeviceFetcher&) = delete;
  StubDeviceFetcher& operator=(const StubDeviceFetcher&) = delete;

  DeviceInfo Fetch() override {
    DeviceInfo device_info;
    device_info.os_name = "stubOS";
    device_info.os_version = "0.0.0.0";
    device_info.device_host_name = "midnightshift";
    device_info.device_model = "topshot";
    device_info.serial_number = "twirlchange";
    device_info.screen_lock_secured = DeviceInfo::SettingValue::ENABLED;
    device_info.disk_encrypted = DeviceInfo::SettingValue::DISABLED;
    device_info.mac_addresses.push_back("00:00:00:00:00:00");
    return device_info;
  }
};

}  // namespace

DeviceInfo::DeviceInfo() = default;
DeviceInfo::~DeviceInfo() = default;
DeviceInfo::DeviceInfo(const DeviceInfo&) = default;
DeviceInfo::DeviceInfo(DeviceInfo&&) = default;

DeviceInfoFetcher::DeviceInfoFetcher() = default;
DeviceInfoFetcher::~DeviceInfoFetcher() = default;

std::unique_ptr<DeviceInfoFetcher> DeviceInfoFetcher::CreateInstance() {
// TODO(pastarmovj): Instead of the if-defs implement the CreateInstance
// function in the platform specific classes.
#if defined(OS_MAC)
  return std::make_unique<DeviceInfoFetcherMac>();
#elif defined(OS_WIN)
  return std::make_unique<DeviceInfoFetcherWin>();
#elif defined(OS_LINUX) || defined(OS_CHROMEOS)
  return std::make_unique<DeviceInfoFetcherLinux>();
#else
  return std::make_unique<StubDeviceFetcher>();
#endif
}

}  // namespace enterprise_signals
