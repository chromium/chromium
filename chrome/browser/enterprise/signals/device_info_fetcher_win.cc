// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/enterprise/signals/device_info_fetcher_win.h"

#include <windows.h>

// SECURITY_WIN32 must be defined in order to get
// EXTENDED_NAME_FORMAT enumeration.
#define SECURITY_WIN32 1
#include <security.h>
#undef SECURITY_WIN32

#include <shobjidl.h>

#include <DSRole.h>
#include <iphlpapi.h>
#include <powersetting.h>
#include <propsys.h>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/registry.h"
#include "base/win/win_util.h"
#include "base/win/wincred_shim.h"
#include "base/win/windows_version.h"
#include "components/device_signals/core/common/common_types.h"
#include "components/device_signals/core/common/platform_utils.h"
#include "net/base/network_interfaces.h"

namespace enterprise_signals {

namespace {

// Retrieves the FQDN of the computer and if this fails reverts to the hostname
// as known to the net subsystem.
std::string GetComputerName() {
  DWORD size = 1024;
  std::wstring result_wstr(size, L'\0');

  if (::GetComputerNameExW(ComputerNameDnsFullyQualified, &result_wstr[0],
                           &size)) {
    std::string result;
    if (base::WideToUTF8(result_wstr.data(), size, &result)) {
      return result;
    }
  }

  return net::GetHostName();
}

std::string GetSecurityPatchLevel() {
  base::win::OSInfo* gi = base::win::OSInfo::GetInstance();

  return base::NumberToString(gi->version_number().patch);
}

std::optional<std::string> GetWindowsUserDomain() {
  WCHAR username[CREDUI_MAX_USERNAME_LENGTH + 1] = {};
  DWORD username_length = sizeof(username);
  if (!::GetUserNameExW(::NameSamCompatible, username, &username_length) ||
      username_length <= 0) {
    return std::nullopt;
  }
  // The string corresponds to DOMAIN\USERNAME. If there isn't a domain, the
  // domain name is replaced by the name of the machine, so the function
  // returns nothing in that case.
  std::string username_str = base::WideToUTF8(username);
  std::string domain = username_str.substr(0, username_str.find("\\"));

  return domain == base::ToUpperASCII(GetComputerNameW())
             ? std::nullopt
             : std::make_optional(domain);
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
  device_info.os_version = device_signals::GetOsVersion();
  device_info.security_patch_level = GetSecurityPatchLevel();
  device_info.device_host_name = GetComputerName();
  device_info.device_model = device_signals::GetDeviceModel();
  device_info.serial_number = device_signals::GetSerialNumber();
  device_info.screen_lock_secured = device_signals::GetScreenlockSecured();
  device_info.disk_encrypted = device_signals::GetDiskEncrypted();
  device_info.mac_addresses = device_signals::GetMacAddresses();
  device_info.windows_machine_domain =
      device_signals::GetWindowsMachineDomain();
  device_info.windows_user_domain = GetWindowsUserDomain();
  device_info.secure_boot_enabled = device_signals::GetSecureBootEnabled();

  return device_info;
}

}  // namespace enterprise_signals
