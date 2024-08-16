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
#include <wincred.h>

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/win/registry.h"
#include "base/win/win_util.h"
#include "base/win/windows_version.h"
#include "base/win/wmi.h"
#include "chrome/browser/enterprise/signals/signals_common.h"
#include "net/base/network_interfaces.h"

namespace enterprise_signals {

namespace {

constexpr wchar_t kSecureBootRegPath[] =
    L"SYSTEM\\CurrentControlSet\\Control\\SecureBoot\\State";
constexpr wchar_t kSecureBootRegKey[] = L"UEFISecureBootEnabled";

// Possible results of the "System.Volume.BitLockerProtection" shell property.
// These values are undocumented but were directly validated on a Windows 10
// machine. See the comment above the GetDiskEncryption() method.
// The values in this enum should be kep in sync with the analogous definiotion
// in the native app implementation.
enum class BitLockerStatus {
  // Encryption is on, and the volume is unlocked
  kOn = 1,
  // Encryption is off
  kOff = 2,
  // Encryption is in progress
  kEncryptionInProgress = 3,
  // Decryption is in progress
  kDecryptionInProgress = 4,
  // Encryption is on, but temporarily suspended
  kSuspended = 5,
  // Encryption is on, and the volume is locked
  kLocked = 6,
};

// Retrieves the computer serial number from WMI.
std::string GetSerialNumber() {
  base::win::WmiComputerSystemInfo sys_info =
      base::win::WmiComputerSystemInfo::Get();
  return base::WideToUTF8(sys_info.serial_number());
}

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

// Retrieves the state of the screen locking feature from the screen saver
// settings.
std::optional<bool> GetScreenLockStatus() {
  std::optional<bool> status;
  BOOL value = FALSE;
  if (::SystemParametersInfo(SPI_GETSCREENSAVESECURE, 0, &value, 0))
    status = static_cast<bool>(value);
  return status;
}

// Checks if locking is enabled at the currently active power scheme.
std::optional<bool> GetConsoleLockStatus() {
  std::optional<bool> status;
  SYSTEM_POWER_STATUS sps;
  // https://docs.microsoft.com/en-us/windows/desktop/api/winbase/nf-winbase-getsystempowerstatus
  // Retrieves the power status of the system. The status indicates whether the
  // system is running on AC or DC power.
  if (!::GetSystemPowerStatus(&sps))
    return status;

  LPGUID p_active_policy = nullptr;
  // https://docs.microsoft.com/en-us/windows/desktop/api/powersetting/nf-powersetting-powergetactivescheme
  // Retrieves the active power scheme and returns a GUID that identifies the
  // scheme.
  if (::PowerGetActiveScheme(nullptr, &p_active_policy) == ERROR_SUCCESS) {
    constexpr GUID kConsoleLock = {
        0x0E796BDB,
        0x100D,
        0x47D6,
        {0xA2, 0xD5, 0xF7, 0xD2, 0xDA, 0xA5, 0x1F, 0x51}};
    const GUID active_policy = *p_active_policy;
    ::LocalFree(p_active_policy);

    auto const power_read_current_value_func =
        sps.ACLineStatus != 0U ? &PowerReadACValue : &PowerReadDCValue;
    ULONG type;
    DWORD value;
    DWORD value_size = sizeof(value);
    // https://docs.microsoft.com/en-us/windows/desktop/api/powersetting/nf-powersetting-powerreadacvalue
    // Retrieves the AC/DC power value for the specified power setting.
    // NO_SUBGROUP_GUID to retrieve the setting for the default power scheme.
    // LPBYTE case is safe and is needed as the function expects generic byte
    // array buffer regardless of the exact value read as it is a generic
    // interface.
    if (power_read_current_value_func(
            nullptr, &active_policy, &NO_SUBGROUP_GUID, &kConsoleLock, &type,
            reinterpret_cast<LPBYTE>(&value), &value_size) == ERROR_SUCCESS) {
      status = value != 0U;
    }
  }

  return status;
}

// Gets cumulative screen locking policy based on the screen saver and console
// lock status.
SettingValue GetScreenlockSecured() {
  const std::optional<bool> screen_lock_status = GetScreenLockStatus();
  if (screen_lock_status.value_or(false))
    return SettingValue::ENABLED;

  const std::optional<bool> console_lock_status = GetConsoleLockStatus();
  if (console_lock_status.value_or(false))
    return SettingValue::ENABLED;

  if (screen_lock_status.has_value() || console_lock_status.has_value()) {
    return SettingValue::DISABLED;
  }

  return SettingValue::UNKNOWN;
}

// Returns the volume where the Windows OS is installed.
std::optional<std::wstring> GetOsVolume() {
  std::optional<std::wstring> volume;
  base::FilePath windows_dir;
  if (base::PathService::Get(base::DIR_WINDOWS, &windows_dir) &&
      windows_dir.IsAbsolute()) {
    std::vector<std::wstring> components = windows_dir.GetComponents();
    DCHECK(components.size());
    volume = components[0];
  }
  return volume;
}

bool GetPropVariantAsInt64(PROPVARIANT variant, int64_t* out_value) {
  switch (variant.vt) {
    case VT_I2:
      *out_value = variant.iVal;
      return true;
    case VT_UI2:
      *out_value = variant.uiVal;
      return true;
    case VT_I4:
      *out_value = variant.lVal;
      return true;
    case VT_UI4:
      *out_value = variant.ulVal;
      return true;
    case VT_INT:
      *out_value = variant.intVal;
      return true;
    case VT_UINT:
      *out_value = variant.uintVal;
      return true;
  }
  return false;
}

// The ideal solution to check the disk encryption (BitLocker) status is to
// use the WMI APIs (Win32_EncryptableVolume). However, only programs running
// with elevated priledges can access those APIs.
//
// Our alternative solution is based on the value of the undocumented (shell)
// property: "System.Volume.BitLockerProtection". That property is essentially
// an enum containing the current BitLocker status for a given volume. This
// approached was suggested here:
// https://stackoverflow.com/questions/41308245/detect-bitlocker-programmatically-from-c-sharp-without-admin/41310139
//
// Note that the link above doesn't give any explanation / meaning for the
// enum values, it simply says that 1, 3 or 5 means the disk is encrypted.
//
// I directly tested and validated this strategy on a Windows 10 machine.
// The values given in the BitLockerStatus enum contain the relevant values
// for the shell property. I also directly validated them.
SettingValue GetDiskEncrypted() {
  // |volume| has to be a |wstring| because SHCreateItemFromParsingName() only
  // accepts |PCWSTR| which is |wchar_t*|.
  std::optional<std::wstring> volume = GetOsVolume();
  if (!volume.has_value())
    return SettingValue::UNKNOWN;

  PROPERTYKEY key;
  const HRESULT property_key_result =
      PSGetPropertyKeyFromName(L"System.Volume.BitLockerProtection", &key);
  if (FAILED(property_key_result))
    return SettingValue::UNKNOWN;

  Microsoft::WRL::ComPtr<IShellItem2> item;
  const HRESULT create_item_result = SHCreateItemFromParsingName(
      volume.value().c_str(), nullptr, IID_IShellItem2, &item);
  if (FAILED(create_item_result) || !item)
    return SettingValue::UNKNOWN;

  PROPVARIANT prop_status;
  const HRESULT property_result = item->GetProperty(key, &prop_status);
  int64_t status;
  if (FAILED(property_result) || !GetPropVariantAsInt64(prop_status, &status))
    return SettingValue::UNKNOWN;

  // Note that we are not considering BitLockerStatus::Suspended as ENABLED.
  if (status == static_cast<int64_t>(BitLockerStatus::kOn) ||
      status == static_cast<int64_t>(BitLockerStatus::kEncryptionInProgress) ||
      status == static_cast<int64_t>(BitLockerStatus::kLocked)) {
    return SettingValue::ENABLED;
  }

  return SettingValue::DISABLED;
}

std::vector<std::string> GetMacAddresses() {
  std::vector<std::string> mac_addresses;
  ULONG adapter_info_size = 0;
  // Get the right buffer size in case of overflow
  if (::GetAdaptersInfo(nullptr, &adapter_info_size) != ERROR_BUFFER_OVERFLOW ||
      adapter_info_size == 0) {
    return mac_addresses;
  }

  std::vector<byte> adapters(adapter_info_size);
  if (::GetAdaptersInfo(reinterpret_cast<PIP_ADAPTER_INFO>(adapters.data()),
                        &adapter_info_size) != ERROR_SUCCESS) {
    return mac_addresses;
  }

  // The returned value is not an array of IP_ADAPTER_INFO elements but a linked
  // list of such
  PIP_ADAPTER_INFO adapter =
      reinterpret_cast<PIP_ADAPTER_INFO>(adapters.data());
  while (adapter) {
    if (adapter->AddressLength == 6) {
      mac_addresses.push_back(
          base::StringPrintf("%02X-%02X-%02X-%02X-%02X-%02X",
                             static_cast<unsigned int>(adapter->Address[0]),
                             static_cast<unsigned int>(adapter->Address[1]),
                             static_cast<unsigned int>(adapter->Address[2]),
                             static_cast<unsigned int>(adapter->Address[3]),
                             static_cast<unsigned int>(adapter->Address[4]),
                             static_cast<unsigned int>(adapter->Address[5])));
    }
    adapter = adapter->Next;
  }
  return mac_addresses;
}

std::optional<std::string> GetWindowsMachineDomain() {
  if (!base::win::IsEnrolledToDomain())
    return std::nullopt;
  std::string domain;
  ::DSROLE_PRIMARY_DOMAIN_INFO_BASIC* info = nullptr;
  if (::DsRoleGetPrimaryDomainInformation(nullptr,
                                          ::DsRolePrimaryDomainInfoBasic,
                                          (PBYTE*)&info) == ERROR_SUCCESS) {
    if (info->DomainNameFlat)
      domain = base::WideToUTF8(info->DomainNameFlat);
    ::DsRoleFreeMemory(info);
  }
  return domain.empty() ? std::nullopt : std::make_optional(domain);
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

std::string GetSecurityPatchLevel() {
  base::win::OSInfo* gi = base::win::OSInfo::GetInstance();

  return base::NumberToString(gi->version_number().patch);
}

SettingValue GetSecureBootEnabled() {
  base::win::RegKey key;
  auto result = key.Open(HKEY_LOCAL_MACHINE, kSecureBootRegPath,
                         KEY_QUERY_VALUE | KEY_WOW64_64KEY);

  if (result != ERROR_SUCCESS || !key.Valid()) {
    return SettingValue::UNKNOWN;
  }

  DWORD secure_boot_dw;
  result = key.ReadValueDW(kSecureBootRegKey, &secure_boot_dw);

  if (result != ERROR_SUCCESS) {
    return SettingValue::UNKNOWN;
  }

  return secure_boot_dw == 1 ? SettingValue::ENABLED : SettingValue::DISABLED;
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
  device_info.os_version = base::SysInfo::OperatingSystemVersion();
  device_info.security_patch_level = GetSecurityPatchLevel();
  device_info.device_host_name = GetComputerName();
  device_info.device_model = base::SysInfo::HardwareModelName();
  device_info.serial_number = GetSerialNumber();
  device_info.screen_lock_secured = GetScreenlockSecured();
  device_info.disk_encrypted = GetDiskEncrypted();
  device_info.mac_addresses = GetMacAddresses();
  device_info.windows_machine_domain = GetWindowsMachineDomain();
  device_info.windows_user_domain = GetWindowsUserDomain();
  device_info.secure_boot_enabled = GetSecureBootEnabled();

  return device_info;
}

}  // namespace enterprise_signals
