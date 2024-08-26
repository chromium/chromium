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

#include <optional>
#include <string_view>

#include "base/apple/scoped_mach_port.h"
#include "base/check_op.h"
#include "base/debug/stack_trace.h"
#include "base/feature_list.h"
#include "base/mac/mac_util.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/posix/sysctl.h"
#include "base/process/process_metrics.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/system/sys_info_internal.h"

namespace base {

namespace {

// Whether this process has CPU security mitigations enabled.
bool g_is_cpu_security_mitigation_enabled = false;

// Whether NumberOfProcessors() was called. Used to detect when the CPU security
// mitigations state changes after a call to NumberOfProcessors().
bool g_is_cpu_security_mitigation_enabled_read = false;

}  // namespace

namespace internal {

std::optional<int> NumberOfPhysicalProcessors() {
  return GetSysctlIntValue("hw.physicalcpu_max");
}

std::optional<int> NumberOfProcessorsWhenCpuSecurityMitigationEnabled() {
  g_is_cpu_security_mitigation_enabled_read = true;

  if (!g_is_cpu_security_mitigation_enabled ||
      !FeatureList::IsEnabled(kNumberOfCoresWithCpuSecurityMitigation)) {
    return std::nullopt;
  }
  return NumberOfPhysicalProcessors();
}

}  // namespace internal

BASE_FEATURE(kNumberOfCoresWithCpuSecurityMitigation,
             "NumberOfCoresWithCpuSecurityMitigation",
             FEATURE_ENABLED_BY_DEFAULT);

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
      NSProcessInfo.processInfo.operatingSystemVersion;
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
uint64_t SysInfo::AmountOfPhysicalMemoryImpl() {
  struct host_basic_info hostinfo;
  mach_msg_type_number_t count = HOST_BASIC_INFO_COUNT;
  base::apple::ScopedMachSendRight host(mach_host_self());
  int result = host_info(host.get(), HOST_BASIC_INFO,
                         reinterpret_cast<host_info_t>(&hostinfo), &count);
  if (result != KERN_SUCCESS) {
    NOTREACHED();
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
  return StringSysctlByName("machdep.cpu.brand_string").value_or(std::string{});
}

// static
std::string SysInfo::HardwareModelName() {
  // Note that there is lots of code out there that uses "hw.model", but that is
  // deprecated in favor of "hw.product" as used here. See the sysctl.h file for
  // more info.
  return StringSysctl({CTL_HW, HW_PRODUCT}).value_or(std::string{});
}

// static
std::optional<SysInfo::HardwareModelNameSplit>
SysInfo::SplitHardwareModelNameDoNotUse(std::string_view name) {
  size_t number_loc = name.find_first_of("0123456789");
  if (number_loc == std::string::npos) {
    return std::nullopt;
  }
  size_t comma_loc = name.find(',', number_loc);
  if (comma_loc == std::string::npos) {
    return std::nullopt;
  }

  HardwareModelNameSplit split;
  if (!StringToInt(name.substr(0u, comma_loc).substr(number_loc),
                   &split.model) ||
      !StringToInt(name.substr(comma_loc + 1), &split.variant)) {
    return std::nullopt;
  }
  split.category = name.substr(0u, number_loc);
  return split;
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
void SysInfo::SetCpuSecurityMitigationsEnabled() {
  // Setting `g_is_cpu_security_mitigation_enabled_read` after it has been read
  // is disallowed because it could indicate that some code got a number of
  // processor computed without all the required state.
  CHECK(!g_is_cpu_security_mitigation_enabled_read);

  g_is_cpu_security_mitigation_enabled = true;
}

// static
void SysInfo::ResetCpuSecurityMitigationsEnabledForTesting() {
  g_is_cpu_security_mitigation_enabled_read = false;
  g_is_cpu_security_mitigation_enabled = false;
}

}  // namespace base
