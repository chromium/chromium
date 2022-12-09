// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/system/sys_info.h"

#import <Foundation/Foundation.h>
#include <mach/mach_host.h>
#include <mach/mach_init.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/sysctl.h>
#include <sys/types.h>

#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_mach_port.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/process/process_metrics.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info_internal.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {

namespace {

bool g_is_cpu_security_mitigation_enabled = false;

// Queries sysctlbyname() for the given key and returns the 32 bit integer value
// from the system or absl::nullopt on failure.
// https://github.com/apple/darwin-xnu/blob/2ff845c2e033bd0ff64b5b6aa6063a1f8f65aa32/bsd/sys/sysctl.h#L1224-L1225
absl::optional<int> GetSysctlIntValue(const char* key_name) {
  int value;
  size_t len = sizeof(value);
  if (sysctlbyname(key_name, &value, &len, nullptr, 0) != 0)
    return absl::nullopt;
  DCHECK_EQ(len, sizeof(value));
  return value;
}

// Queries sysctlbyname() for the given key and returns the value from the
// system or the empty string on failure.
std::string GetSysctlStringValue(const char* key_name) {
  char value[256];
  size_t len = sizeof(value);
  if (sysctlbyname(key_name, &value, &len, nullptr, 0) != 0)
    return std::string();
  DCHECK_GE(len, 1u);
  DCHECK_LE(len, sizeof(value));
  DCHECK_EQ('\0', value[len - 1]);
  return std::string(value, len - 1);
}

}  // namespace

namespace internal {

absl::optional<int> NumberOfPhysicalProcessors() {
  return GetSysctlIntValue("hw.physicalcpu_max");
}

absl::optional<int> NumberOfProcessorsWhenCpuSecurityMitigationEnabled() {
  if (!g_is_cpu_security_mitigation_enabled ||
      !FeatureList::IsEnabled(kNumberOfCoresWithCpuSecurityMitigation)) {
    return absl::nullopt;
  }
  return NumberOfPhysicalProcessors();
}

}  // namespace internal

BASE_FEATURE(kNumberOfCoresWithCpuSecurityMitigation,
             "NumberOfCoresWithCpuSecurityMitigation",
             FEATURE_DISABLED_BY_DEFAULT);

// static
std::string SysInfo::OperatingSystemName() {
  return "Mac OS X";
}

// static
std::string SysInfo::OperatingSystemVersion() {
  int32_t major, minor, bugfix;
  OperatingSystemVersionNumbers(&major, &minor, &bugfix);
  return base::StringPrintf("%d.%d.%d", major, minor, bugfix);
}

// static
void SysInfo::OperatingSystemVersionNumbers(int32_t* major_version,
                                            int32_t* minor_version,
                                            int32_t* bugfix_version) {
  NSOperatingSystemVersion version =
      [[NSProcessInfo processInfo] operatingSystemVersion];
  *major_version = saturated_cast<int32_t>(version.majorVersion);
  *minor_version = saturated_cast<int32_t>(version.minorVersion);
  *bugfix_version = saturated_cast<int32_t>(version.patchVersion);
}

// static
std::string SysInfo::OperatingSystemArchitecture() {
  switch (mac::GetCPUType()) {
    case mac::CPUType::kIntel:
      return "x86_64";
    case mac::CPUType::kTranslatedIntel:
    case mac::CPUType::kArm:
      return "arm64";
  }
}

// static
int SysInfo::NumberOfEfficientProcessorsImpl() {
  int num_perf_levels = GetSysctlIntValue("hw.nperflevels").value_or(1);
  if (num_perf_levels == 1)
    return 0;
  DCHECK_GE(num_perf_levels, 2);

  // Lower values of perflevel indicate higher-performance core types. See
  // https://developer.apple.com/documentation/kernel/1387446-sysctlbyname/determining_system_capabilities?changes=l__5
  int num_of_efficient_processors =
      GetSysctlIntValue(
          StringPrintf("hw.perflevel%d.logicalcpu", num_perf_levels - 1)
              .c_str())
          .value_or(0);
  DCHECK_GE(num_of_efficient_processors, 0);

  return num_of_efficient_processors;
}

// static
uint64_t SysInfo::AmountOfPhysicalMemoryImpl() {
  struct host_basic_info hostinfo;
  mach_msg_type_number_t count = HOST_BASIC_INFO_COUNT;
  base::mac::ScopedMachSendRight host(mach_host_self());
  int result = host_info(host.get(), HOST_BASIC_INFO,
                         reinterpret_cast<host_info_t>(&hostinfo), &count);
  if (result != KERN_SUCCESS) {
    NOTREACHED();
    return 0;
  }
  DCHECK_EQ(HOST_BASIC_INFO_COUNT, count);
  return hostinfo.max_mem;
}

// static
uint64_t SysInfo::AmountOfAvailablePhysicalMemoryImpl() {
  SystemMemoryInfoKB info;
  if (!GetSystemMemoryInfo(&info))
    return 0;
  // We should add inactive file-backed memory also but there is no such
  // information from Mac OS unfortunately.
  return checked_cast<uint64_t>(info.free + info.speculative) * 1024;
}

// static
std::string SysInfo::CPUModelName() {
  return GetSysctlStringValue("machdep.cpu.brand_string");
}

// static
std::string SysInfo::HardwareModelName() {
  return GetSysctlStringValue("hw.model");
}

// static
SysInfo::HardwareInfo SysInfo::GetHardwareInfoSync() {
  HardwareInfo info;
  info.manufacturer = "Apple Inc.";
  info.model = HardwareModelName();
  DCHECK(IsStringUTF8(info.manufacturer));
  DCHECK(IsStringUTF8(info.model));
  return info;
}

// static
void SysInfo::SetIsCpuSecurityMitigationsEnabled(bool is_enabled) {
  g_is_cpu_security_mitigation_enabled = is_enabled;
}

}  // namespace base
