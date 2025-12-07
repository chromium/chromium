// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/signals/device_info_fetcher_win.h"

#include "base/strings/string_number_conversions.h"
#include "base/system/sys_info.h"
#include "base/win/windows_version.h"
#include "components/device_signals/core/browser/browser_utils.h"
#include "components/device_signals/core/common/common_types.h"
#include "components/device_signals/core/common/platform_utils.h"

namespace enterprise_signals {

namespace {

std::string GetSecurityPatchLevel() {
  base::win::OSInfo* gi = base::win::OSInfo::GetInstance();

  return base::NumberToString(gi->version_number().patch);
}

}  // namespace

// static
std::unique_ptr<DeviceInfoFetcher> DeviceInfoFetcher::CreateInstanceInternal() {
  return std::make_unique<DeviceInfoFetcherWin>();
}

DeviceInfoFetcherWin::DeviceInfoFetcherWin() = default;

DeviceInfoFetcherWin::~DeviceInfoFetcherWin() = default;

DeviceInfo DeviceInfoFetcherWin::Fetch() {
  DeviceInfo device_info;
  device_info.os_name = "windows";
  // Using `SysInfo` instead of device_signals function because the clients of
  // this class expects the version to have the format of
  // "<Major>.<Minor>.<Build>", instead of the full version number returned by
  // platform_utils, which also includes <Revision>.
  device_info.os_version = base::SysInfo::OperatingSystemVersion();
  device_info.security_patch_level = GetSecurityPatchLevel();
  device_info.device_host_name = device_signals::GetHostName();
  device_info.device_model = device_signals::GetDeviceModel();
  device_info.serial_number = device_signals::GetSerialNumber();
  device_info.screen_lock_secured = device_signals::GetScreenlockSecured();
  device_info.disk_encrypted = device_signals::GetDiskEncrypted();
  device_info.mac_addresses = device_signals::GetMacAddresses();
  device_info.windows_machine_domain =
      device_signals::GetWindowsMachineDomain();
  device_info.windows_user_domain = device_signals::GetWindowsUserDomain();
  device_info.secure_boot_enabled = device_signals::GetSecureBootEnabled();

  return device_info;
}

}  // namespace enterprise_signals
