// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/hardware_check.h"

#include <windows.h>

#include <tbs.h>

#include <string_view>

#include "base/cpu.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "base/win/registry.h"
#include "base/win/windows_version.h"
#include "build/build_config.h"

namespace base::win {

namespace {

bool IsWin11SupportedProcessor(const CPU& cpu_info,
                               std::string_view vendor_name) {
#if defined(ARCH_CPU_X86_FAMILY)
  if (vendor_name == "GenuineIntel") {
    // Windows 11 is supported on Intel 8th Gen and higher models
    // CPU model ID's can be referenced from the following file in
    // the kernel source: arch/x86/include/asm/intel-family.h
    if (cpu_info.family() != 0x06 || cpu_info.model() <= 0x5F ||
        (cpu_info.model() == 0x8E &&
         (cpu_info.stepping() < 9 || cpu_info.stepping() > 12)) ||
        (cpu_info.model() == 0x9E &&
         (cpu_info.stepping() < 10 || cpu_info.stepping() > 13))) {
      return false;
    }
    return true;
  }

  if (vendor_name == "AuthenticAMD") {
    // Windows 11 is supported on AMD Zen+ and higher models
    if (cpu_info.family() < 0x17 ||
        (cpu_info.family() == 0x17 &&
         (cpu_info.model() == 0x1 || cpu_info.model() == 0x11))) {
      return false;
    }
    return true;
  }
#elif defined(ARCH_CPU_ARM_FAMILY)
  if (vendor_name == "Qualcomm Technologies Inc") {
    // Windows 11 is supported on all Qualcomm models with the exception
    // of 1st Gen Compute Platforms due to lack of TPM 2.0
    return true;
  }
#else
#error Unsupported CPU architecture
#endif
  return false;
}

bool IsUEFISecureBootEnabled() {
  static constexpr wchar_t kSecureBootRegPath[] =
      L"SYSTEM\\CurrentControlSet\\Control\\SecureBoot\\State";

  RegKey key;
  auto result =
      key.Open(HKEY_LOCAL_MACHINE, kSecureBootRegPath, KEY_QUERY_VALUE);
  if (result != ERROR_SUCCESS) {
    return false;
  }

  DWORD secure_boot = 0;
  result = key.ReadValueDW(L"UEFISecureBootEnabled", &secure_boot);

  return result == ERROR_SUCCESS && secure_boot == 1;
}

bool IsTPM20Supported() {
  TPM_DEVICE_INFO tpm_info{};

  if (::Tbsi_GetDeviceInfo(sizeof(tpm_info), &tpm_info) != TBS_SUCCESS) {
    return false;
  }

  return tpm_info.tpmVersion >= TPM_VERSION_20;
}

}  // namespace

bool IsWin11UpgradeEligible() {
  static constexpr int64_t kMinTotalDiskSpace = 64 * 1024 * 1024;
  static constexpr uint64_t kMinTotalPhysicalMemory = 4 * 1024 * 1024;

  static const bool is_win11_upgrade_eligible = [] {
    if (!IsWin11SupportedProcessor(
            CPU(), OSInfo::GetInstance()->processor_vendor_name())) {
      return false;
    }

    if (SysInfo::AmountOfPhysicalMemory() < kMinTotalPhysicalMemory) {
      return false;
    }

    FilePath system_path;
    if (PathService::Get(DIR_SYSTEM, &system_path) &&
        SysInfo::AmountOfTotalDiskSpace(
            FilePath(system_path.GetComponents()[0])) < kMinTotalDiskSpace) {
      return false;
    }

    if (!IsUEFISecureBootEnabled()) {
      return false;
    }

    if (!IsTPM20Supported()) {
      return false;
    }

    return true;
  }();

  return is_win11_upgrade_eligible;
}

}  // namespace base::win
