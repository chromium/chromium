// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/signals/device_info_fetcher_linux.h"

#include <string>

#include "base/files/dir_reader_posix.h"
#include "base/files/file.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "components/device_signals/core/browser/browser_utils.h"
#include "components/device_signals/core/common/common_types.h"
#include "components/device_signals/core/common/platform_utils.h"
#include "net/base/network_interfaces.h"

namespace enterprise_signals {

namespace {

std::string GetOsVersion() {
  const auto& distribution_version = device_signals::GetDistributionVersion();
  if (distribution_version) {
    return distribution_version.value();
  }
  return base::SysInfo::OperatingSystemVersion();
}

std::string GetSecurityPatchLevel() {
  int32_t major, minor, bugfix;
  base::SysInfo::OperatingSystemVersionNumbers(&major, &minor, &bugfix);
  return base::StringPrintf("%d.%d.%d", major, minor, bugfix);
}

}  // namespace

// static
std::unique_ptr<DeviceInfoFetcher> DeviceInfoFetcher::CreateInstanceInternal() {
  return std::make_unique<DeviceInfoFetcherLinux>();
}

DeviceInfoFetcherLinux::DeviceInfoFetcherLinux() = default;

DeviceInfoFetcherLinux::~DeviceInfoFetcherLinux() = default;

DeviceInfo DeviceInfoFetcherLinux::Fetch() {
  DeviceInfo device_info;
  device_info.os_name = "linux";
  device_info.os_version = GetOsVersion();
  device_info.security_patch_level = GetSecurityPatchLevel();
  device_info.device_host_name = device_signals::GetHostName();
  device_info.device_model = device_signals::GetDeviceModel();
  device_info.serial_number = device_signals::GetSerialNumber();
  device_info.screen_lock_secured = device_signals::GetScreenlockSecured();
  device_info.disk_encrypted = device_signals::GetDiskEncrypted();
  device_info.mac_addresses = device_signals::GetMacAddresses();
  return device_info;
}

}  // namespace enterprise_signals
