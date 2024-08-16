// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/system/sys_info.h"

#import <UIKit/UIKit.h>
#include <mach/mach.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/sysctl.h>
#include <sys/types.h>

#include "base/apple/scoped_mach_port.h"
#include "base/check_op.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/posix/sysctl.h"
#include "base/process/process_metrics.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "build/build_config.h"

namespace base {

#if BUILDFLAG(IS_IOS)
namespace {
// Accessor for storage of overridden HardwareModelName.
std::string& GetHardwareModelNameStorage() {
  static base::NoDestructor<std::string> instance;
  return *instance;
}
}  // namespace
#endif

// static
std::string SysInfo::OperatingSystemName() {
  static dispatch_once_t get_system_name_once;
  static std::string* system_name;
  dispatch_once(&get_system_name_once, ^{
    @autoreleasepool {
      system_name =
          new std::string(SysNSStringToUTF8(UIDevice.currentDevice.systemName));
    }
  });
  // Examples of returned value: 'iPhone OS' on iPad 5.1.1
  // and iPhone 5.1.1.
  return *system_name;
}

// static
std::string SysInfo::OperatingSystemVersion() {
  static dispatch_once_t get_system_version_once;
  static std::string* system_version;
  dispatch_once(&get_system_version_once, ^{
    @autoreleasepool {
      system_version = new std::string(
          SysNSStringToUTF8(UIDevice.currentDevice.systemVersion));
    }
  });
  return *system_version;
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
#if defined(ARCH_CPU_X86)
  return "x86";
#elif defined(ARCH_CPU_X86_64)
  return "x86_64";
#elif defined(ARCH_CPU_ARMEL)
  return "arm";
#elif defined(ARCH_CPU_ARM64)
  return "arm64";
#else
#error Unsupported CPU architecture
#endif
}

// static
std::string SysInfo::GetIOSBuildNumber() {
  std::optional<std::string> build_number =
      StringSysctl({CTL_KERN, KERN_OSVERSION});
  return build_number.value();
}

// static
void SysInfo::OverrideHardwareModelName(std::string name) {
  // Normally, HardwareModelName() should not be called before overriding the
  // value, but StartCrashController(), which eventually calls
  // HardwareModelName(), is called before overriding the name.
  CHECK(!name.empty());
  GetHardwareModelNameStorage() = std::move(name);
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
  // information from iOS unfortunately.
  return checked_cast<uint64_t>(info.free + info.speculative) * 1024;
}

// static
std::string SysInfo::CPUModelName() {
  return StringSysctlByName("machdep.cpu.brand_string").value_or(std::string{});
}

// static
std::string SysInfo::HardwareModelName() {
#if TARGET_OS_SIMULATOR
  // On the simulator, "hw.machine" returns "i386" or "x86_64" which doesn't
  // match the expected format, so supply a fake string here.
  const char* model = getenv("SIMULATOR_MODEL_IDENTIFIER");
  if (model == nullptr) {
    switch (UIDevice.currentDevice.userInterfaceIdiom) {
      case UIUserInterfaceIdiomPhone:
        model = "iPhone";
        break;
      case UIUserInterfaceIdiomPad:
        model = "iPad";
        break;
      default:
        model = "Unknown";
        break;
    }
  }
  return base::StringPrintf("iOS Simulator (%s)", model);
#else
  const std::string& override = GetHardwareModelNameStorage();
  if (!override.empty()) {
    return override;
  }
  // Note: This uses "hw.machine" instead of "hw.model" like the Mac code,
  // because "hw.model" doesn't always return the right string on some devices.
  return StringSysctl({CTL_HW, HW_MACHINE}).value_or(std::string{});
#endif
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

}  // namespace base
