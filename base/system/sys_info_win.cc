// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "base/system/sys_info.h"

#include <windows.h>

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <bit>
#include <limits>
#include <type_traits>
#include <vector>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/process/process_metrics.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/win/registry.h"
#include "base/win/windows_version.h"
#include "third_party/abseil-cpp/absl/container/inlined_vector.h"

namespace {

// Returns the power efficiency levels of physical cores or empty vector on
// failure. The BYTE value of the element is the relative efficiency rank among
// all physical cores, where 0 is the most efficient, 1 is the second most
// efficient, and so on.
std::vector<BYTE> GetCoreEfficiencyClasses() {
  const DWORD kReservedSize =
      sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX) * 64;
  absl::InlinedVector<BYTE, kReservedSize> buffer;
  buffer.resize(kReservedSize);
  DWORD byte_length = kReservedSize;
  if (!GetLogicalProcessorInformationEx(
          RelationProcessorCore,
          reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(
              buffer.data()),
          &byte_length)) {
    DPCHECK(GetLastError() == ERROR_INSUFFICIENT_BUFFER);
    buffer.resize(byte_length);
    if (!GetLogicalProcessorInformationEx(
            RelationProcessorCore,
            reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(
                buffer.data()),
            &byte_length)) {
      return {};
    }
  }

  std::vector<BYTE> efficiency_classes;
  BYTE* byte_ptr = buffer.data();
  while (byte_ptr < buffer.data() + byte_length) {
    const auto* structure_ptr =
        reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(byte_ptr);
    DCHECK_EQ(structure_ptr->Relationship, RelationProcessorCore);
    DCHECK_LE(&structure_ptr->Processor.EfficiencyClass +
                  sizeof(structure_ptr->Processor.EfficiencyClass),
              buffer.data() + byte_length);
    efficiency_classes.push_back(structure_ptr->Processor.EfficiencyClass);
    DCHECK_GE(
        structure_ptr->Size,
        offsetof(std::remove_pointer_t<decltype(structure_ptr)>, Processor) +
            sizeof(structure_ptr->Processor));
    byte_ptr = byte_ptr + structure_ptr->Size;
  }

  return efficiency_classes;
}

// Returns the physical cores to logical processor mapping masks by using the
// Windows API GetLogicalProcessorInformation(), or an empty vector on failure.
// When succeeded, the vector would be of same size to the number of physical
// cores, while each element is the bitmask of the logical processors that the
// physical core has.
std::vector<uint64_t> GetCoreProcessorMasks() {
  const DWORD kReservedSize = 64;
  absl::InlinedVector<SYSTEM_LOGICAL_PROCESSOR_INFORMATION, kReservedSize>
      buffer;
  buffer.resize(kReservedSize);
  DWORD byte_length = sizeof(buffer[0]) * kReservedSize;
  const BOOL result =
      GetLogicalProcessorInformation(buffer.data(), &byte_length);
  DWORD element_count = byte_length / sizeof(buffer[0]);
  DCHECK_EQ(byte_length % sizeof(buffer[0]), 0u);
  if (!result) {
    DPCHECK(GetLastError() == ERROR_INSUFFICIENT_BUFFER);
    buffer.resize(element_count);
    if (!GetLogicalProcessorInformation(buffer.data(), &byte_length)) {
      return {};
    }
  }

  std::vector<uint64_t> processor_masks;
  for (DWORD i = 0; i < element_count; i++) {
    if (buffer[i].Relationship == RelationProcessorCore) {
      processor_masks.push_back(buffer[i].ProcessorMask);
    }
  }

  return processor_masks;
}

uint64_t AmountOfMemory(DWORDLONG MEMORYSTATUSEX::*memory_field) {
  MEMORYSTATUSEX memory_info;
  memory_info.dwLength = sizeof(memory_info);
  if (!GlobalMemoryStatusEx(&memory_info)) {
    NOTREACHED();
  }

  return memory_info.*memory_field;
}

bool GetDiskSpaceInfo(const base::FilePath& path,
                      int64_t* available_bytes,
                      int64_t* total_bytes) {
  ULARGE_INTEGER available;
  ULARGE_INTEGER total;
  ULARGE_INTEGER free;
  if (!GetDiskFreeSpaceExW(path.value().c_str(), &available, &total, &free))
    return false;

  if (available_bytes) {
    *available_bytes = static_cast<int64_t>(available.QuadPart);
    if (*available_bytes < 0)
      *available_bytes = std::numeric_limits<int64_t>::max();
  }
  if (total_bytes) {
    *total_bytes = static_cast<int64_t>(total.QuadPart);
    if (*total_bytes < 0)
      *total_bytes = std::numeric_limits<int64_t>::max();
  }
  return true;
}

}  // namespace

namespace base {

// static
int SysInfo::NumberOfProcessors() {
  return win::OSInfo::GetInstance()->processors();
}

// static
int SysInfo::NumberOfEfficientProcessorsImpl() {
  std::vector<BYTE> efficiency_classes = GetCoreEfficiencyClasses();
  if (efficiency_classes.empty())
    return 0;

  auto [min_efficiency_class_it, max_efficiency_class_it] =
      std::minmax_element(efficiency_classes.begin(), efficiency_classes.end());
  if (*min_efficiency_class_it == *max_efficiency_class_it)
    return 0;

  std::vector<uint64_t> processor_masks = GetCoreProcessorMasks();
  if (processor_masks.empty())
    return 0;

  DCHECK_EQ(efficiency_classes.size(), processor_masks.size());
  int num_of_efficient_processors = 0;
  for (size_t i = 0; i < efficiency_classes.size(); i++) {
    if (efficiency_classes[i] == *min_efficiency_class_it) {
      num_of_efficient_processors += std::popcount(processor_masks[i]);
    }
  }

  return num_of_efficient_processors;
}

// static
uint64_t SysInfo::AmountOfPhysicalMemoryImpl() {
  return AmountOfMemory(&MEMORYSTATUSEX::ullTotalPhys);
}

// static
uint64_t SysInfo::AmountOfAvailablePhysicalMemoryImpl() {
  SystemMemoryInfoKB info;
  if (!GetSystemMemoryInfo(&info))
    return 0;
  return checked_cast<uint64_t>(info.avail_phys) * 1024;
}

// static
uint64_t SysInfo::AmountOfVirtualMemory() {
  return AmountOfMemory(&MEMORYSTATUSEX::ullTotalVirtual);
}

// static
int64_t SysInfo::AmountOfFreeDiskSpace(const FilePath& path) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  int64_t available;
  if (!GetDiskSpaceInfo(path, &available, nullptr))
    return -1;
  return available;
}

// static
int64_t SysInfo::AmountOfTotalDiskSpace(const FilePath& path) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  int64_t total;
  if (!GetDiskSpaceInfo(path, nullptr, &total))
    return -1;
  return total;
}

std::string SysInfo::OperatingSystemName() {
  return "Windows NT";
}

// static
std::string SysInfo::OperatingSystemVersion() {
  win::OSInfo* os_info = win::OSInfo::GetInstance();
  win::OSInfo::VersionNumber version_number = os_info->version_number();
  std::string version(StringPrintf("%d.%d.%d", version_number.major,
                                   version_number.minor, version_number.build));
  win::OSInfo::ServicePack service_pack = os_info->service_pack();
  if (service_pack.major != 0) {
    version += StringPrintf(" SP%d", service_pack.major);
    if (service_pack.minor != 0)
      version += StringPrintf(".%d", service_pack.minor);
  }
  return version;
}

// TODO: Implement OperatingSystemVersionComplete, which would include
// patchlevel/service pack number.
// See chrome/browser/feedback/feedback_util.h, FeedbackUtil::SetOSVersion.

// static
std::string SysInfo::OperatingSystemArchitecture() {
  win::OSInfo::WindowsArchitecture arch = win::OSInfo::GetArchitecture();
  switch (arch) {
    case win::OSInfo::X86_ARCHITECTURE:
      return "x86";
    case win::OSInfo::X64_ARCHITECTURE:
      return "x86_64";
    case win::OSInfo::IA64_ARCHITECTURE:
      return "ia64";
    case win::OSInfo::ARM64_ARCHITECTURE:
      return "arm64";
    default:
      return "";
  }
}

// static
std::string SysInfo::CPUModelName() {
  return win::OSInfo::GetInstance()->processor_model_name();
}

// static
size_t SysInfo::VMAllocationGranularity() {
  return win::OSInfo::GetInstance()->allocation_granularity();
}

// static
void SysInfo::OperatingSystemVersionNumbers(int32_t* major_version,
                                            int32_t* minor_version,
                                            int32_t* bugfix_version) {
  win::OSInfo* os_info = win::OSInfo::GetInstance();
  *major_version = static_cast<int32_t>(os_info->version_number().major);
  *minor_version = static_cast<int32_t>(os_info->version_number().minor);
  *bugfix_version = 0;
}

// static
std::string ReadHardwareInfoFromRegistry(const wchar_t* reg_value_name) {
  // On some systems or VMs, the system information and some of the below
  // locations may be missing info. Attempt to find the info from the below
  // registry keys in the order provided.
  static const wchar_t* const kSystemInfoRegKeyPaths[] = {
      L"HARDWARE\\DESCRIPTION\\System\\BIOS",
      L"SYSTEM\\CurrentControlSet\\Control\\SystemInformation",
      L"SYSTEM\\HardwareConfig\\Current",
  };

  std::wstring value;
  for (const wchar_t* system_info_reg_key_path : kSystemInfoRegKeyPaths) {
    base::win::RegKey system_information_key;
    if (system_information_key.Open(HKEY_LOCAL_MACHINE,
                                    system_info_reg_key_path,
                                    KEY_READ) == ERROR_SUCCESS) {
      if ((system_information_key.ReadValue(reg_value_name, &value) ==
           ERROR_SUCCESS) &&
          !value.empty()) {
        break;
      }
    }
  }

  return base::SysWideToUTF8(value);
}

// static
SysInfo::HardwareInfo SysInfo::GetHardwareInfoSync() {
  HardwareInfo info = {ReadHardwareInfoFromRegistry(L"SystemManufacturer"),
                       SysInfo::HardwareModelName()};
  return info;
}

// static
std::string SysInfo::HardwareModelName() {
  return ReadHardwareInfoFromRegistry(L"SystemProductName");
}

}  // namespace base
