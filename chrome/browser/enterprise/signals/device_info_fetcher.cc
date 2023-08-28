// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/signals/device_info_fetcher.h"

#include "build/build_config.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/enterprise/signals/device_info_fetcher_mac.h"
#elif BUILDFLAG(IS_WIN)
#include "chrome/browser/enterprise/signals/device_info_fetcher_win.h"
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/enterprise/signals/device_info_fetcher_linux.h"
#endif

namespace enterprise_signals {

namespace {

// When true, will force DeviceInfoFetcher::CreateInstance to return a stubbed
// instance. Used for testing.
bool force_stub_for_testing = false;

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
    device_info.security_patch_level = "security patch level";
    device_info.device_host_name = "midnightshift";
    device_info.device_model = "topshot";
    device_info.serial_number = "twirlchange";
    device_info.secure_boot_enabled = SettingValue::ENABLED;
    device_info.screen_lock_secured = SettingValue::ENABLED;
    device_info.disk_encrypted = SettingValue::DISABLED;
    device_info.mac_addresses.push_back("00:00:00:00:00:00");
    device_info.windows_machine_domain = "MACHINE_DOMAIN";
    device_info.windows_user_domain = "USER_DOMAIN";
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

// static
std::unique_ptr<DeviceInfoFetcher> DeviceInfoFetcher::CreateInstance() {
  if (force_stub_for_testing) {
    return std::make_unique<StubDeviceFetcher>();
  }
  return CreateInstanceInternal();
}

#if !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_WIN) && \
    !(BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS))
// static
std::unique_ptr<DeviceInfoFetcher> DeviceInfoFetcher::CreateInstanceInternal() {
  return std::make_unique<StubDeviceFetcher>();
}
#endif

// static
std::unique_ptr<DeviceInfoFetcher>
DeviceInfoFetcher::CreateStubInstanceForTesting() {
  return std::make_unique<StubDeviceFetcher>();
}

// static
void DeviceInfoFetcher::SetForceStubForTesting(bool should_force) {
  force_stub_for_testing = should_force;
}

}  // namespace enterprise_signals
