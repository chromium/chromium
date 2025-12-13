// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/hardware_check.h"

#include <windows.h>
#include <winternl.h>

#include <tbs.h>

#include <string_view>

#include "base/cpu.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/scoped_native_library.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "base/threading/scoped_thread_priority.h"
#include "base/win/registry.h"
#include "base/win/windows_version.h"
#include "build/build_config.h"

namespace base::win {

namespace {

// ntstatus.h conflicts with windows.h so define this locally.
#define STATUS_SUCCESS ((NTSTATUS)0x00000000L)
#define SystemSecureBootInformation 0x91

struct SYSTEM_SECUREBOOT_INFORMATION {
  BOOLEAN SecureBootEnabled;
  BOOLEAN SecureBootCapable;
};

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

bool IsUEFISecureBootCapable() {
  SYSTEM_SECUREBOOT_INFORMATION secure_boot_info{};
  if (::NtQuerySystemInformation(
          static_cast<SYSTEM_INFORMATION_CLASS>(SystemSecureBootInformation),
          &secure_boot_info, sizeof(SYSTEM_SECUREBOOT_INFORMATION),
          nullptr) != STATUS_SUCCESS) {
    return false;
  }

  return !!secure_boot_info.SecureBootCapable;
}

bool IsTPM20Supported() {
  SCOPED_MAY_LOAD_LIBRARY_AT_BACKGROUND_PRIORITY();

  // Using dynamic loading instead of using linker support for delay
  // loading to prevent failed loads being treated as a fatal failure which
  // can happen in rare cases due to missing or corrupted DLL file.
  ScopedNativeLibrary tbs_library(LoadSystemLibrary(L"tbs.dll"));
  if (!tbs_library.is_valid()) {
    return false;
  }

  decltype(Tbsi_GetDeviceInfo)* tbsi_get_device_info_proc =
      reinterpret_cast<decltype(Tbsi_GetDeviceInfo)*>(
          tbs_library.GetFunctionPointer("Tbsi_GetDeviceInfo"));
  if (!tbsi_get_device_info_proc) {
    return false;
  }

  TPM_DEVICE_INFO tpm_info{};
  TBS_RESULT result = tbsi_get_device_info_proc(sizeof(tpm_info), &tpm_info);
  return result == TBS_SUCCESS && tpm_info.tpmVersion >= TPM_VERSION_20;
}

}  // namespace

bool HardwareEvaluationResult::IsEligible() const {
  return this->cpu && this->memory && this->disk && this->firmware && this->tpm;
}

HardwareEvaluationResult EvaluateWin11HardwareRequirements() {
  static constexpr int64_t kMinTotalDiskSpace = 64 * 1024 * 1024;
  // TODO(crbug.com/429140103): This was migrated as-is to 4MiB in ByteCount but
  // the legacy code potentially intended 4GiB, needs investigation.
  static constexpr ByteCount kMinTotalPhysicalMemory = MiB(4);

  static const HardwareEvaluationResult evaluate_win11_upgrade_eligibility =
      [] {
        HardwareEvaluationResult result;

        result.cpu = IsWin11SupportedProcessor(
            CPU(), OSInfo::GetInstance()->processor_vendor_name());

        result.memory =
            SysInfo::AmountOfPhysicalMemory() >= kMinTotalPhysicalMemory;

        FilePath system_path;
        result.disk = PathService::Get(DIR_SYSTEM, &system_path) &&
                      SysInfo::AmountOfTotalDiskSpace(
                          FilePath(system_path.GetComponents()[0]))
                              .value_or(-1) >= kMinTotalDiskSpace;

        result.firmware = IsUEFISecureBootCapable();

        result.tpm = IsTPM20Supported();

        return result;
      }();

  return evaluate_win11_upgrade_eligibility;
}

}  // namespace base::win
