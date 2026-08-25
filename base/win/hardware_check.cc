// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/hardware_check.h"

#include <windows.h>
#include <winternl.h>

#include <tbs.h>

#include <optional>
#include <string_view>

#include "base/byte_size.h"
#include "base/cpu.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "base/threading/scoped_thread_priority.h"
#include "base/win/delayload_helpers.h"
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
  static const bool is_tbs_availabe = [] {
    SCOPED_MAY_LOAD_LIBRARY_AT_BACKGROUND_PRIORITY();

    // Resolve all delay-loaded imports for tbs.dll on the first call to
    // prevent failed loads being treated as a fatal failure later, which
    // can happen in rare cases due to missing or corrupted DLL file.
    return LoadAllImportsForDllUnchecked("tbs.dll").value_or(false);
  }();

  if (!is_tbs_availabe) {
    return false;
  }
  TPM_DEVICE_INFO tpm_info{};
  TBS_RESULT result = ::Tbsi_GetDeviceInfo(sizeof(tpm_info), &tpm_info);
  return result == TBS_SUCCESS && tpm_info.tpmVersion >= TPM_VERSION_20;
}

}  // namespace

bool HardwareEvaluationResult::IsEligible() const {
  return this->cpu && this->memory && this->disk && this->firmware && this->tpm;
}

HardwareEvaluationResult EvaluateWin11HardwareRequirements() {
  static constexpr ByteSize kMinTotalDiskSpace = GiB(64);
  static constexpr ByteSize kMinTotalPhysicalMemory = GiB(4);

  static const HardwareEvaluationResult eligibility = [] {
    HardwareEvaluationResult result;

    result.cpu = IsWin11SupportedProcessor(
        CPU(), OSInfo::GetInstance()->processor_vendor_name());

    result.memory =
        SysInfo::AmountOfTotalPhysicalMemory() >= kMinTotalPhysicalMemory;

    FilePath system_path;
    if (PathService::Get(DIR_SYSTEM, &system_path)) {
      std::optional<SysInfo::DiskSpaceInfo> disk_info =
          SysInfo::AmountOfDiskSpace(FilePath(system_path.GetComponents()[0]));
      result.disk = disk_info && disk_info->total >= kMinTotalDiskSpace;
    }

    result.firmware = IsUEFISecureBootCapable();

    result.tpm = IsTPM20Supported();

    return result;
  }();

  return eligibility;
}

}  // namespace base::win
