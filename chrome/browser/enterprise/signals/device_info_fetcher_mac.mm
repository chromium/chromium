// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/signals/device_info_fetcher_mac.h"

#include "base/system/sys_info.h"
#include "components/device_signals/core/browser/browser_utils.h"
#include "components/device_signals/core/common/platform_utils.h"
#include "net/base/network_interfaces.h"

namespace enterprise_signals {

namespace {

std::string GetOsVersion() {
  return base::SysInfo::OperatingSystemVersion();
}

}  // namespace

// static
std::unique_ptr<DeviceInfoFetcher> DeviceInfoFetcher::CreateInstanceInternal() {
  return std::make_unique<DeviceInfoFetcherMac>();
}

DeviceInfoFetcherMac::DeviceInfoFetcherMac() = default;

DeviceInfoFetcherMac::~DeviceInfoFetcherMac() = default;

DeviceInfo DeviceInfoFetcherMac::Fetch() {
  DeviceInfo device_info;
  device_info.os_name = "macOS";
  device_info.os_version = GetOsVersion();
  device_info.device_host_name = device_signals::GetHostName();
  device_info.device_model = device_signals::GetDeviceModel();
  device_info.serial_number = device_signals::GetSerialNumber();
  device_info.screen_lock_secured = device_signals::GetScreenlockSecured();
  device_info.disk_encrypted = device_signals::GetDiskEncrypted();
  device_info.mac_addresses = device_signals::GetMacAddresses();
  return device_info;
}

}  // namespace enterprise_signals
