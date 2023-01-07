// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/system/sys_info.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <limits>
#include <sstream>

#include "base/check.h"
#include "base/files/file_util.h"
#include "base/lazy_instance.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/process/process_metrics.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info_internal.h"
#include "build/build_config.h"

namespace {

uint64_t AmountOfMemory(int pages_name) {
  long pages = sysconf(pages_name);
  long page_size = sysconf(_SC_PAGESIZE);
  if (pages < 0 || page_size < 0)
    return 0;
  return static_cast<uint64_t>(pages) * static_cast<uint64_t>(page_size);
}

uint64_t AmountOfPhysicalMemory() {
  return AmountOfMemory(_SC_PHYS_PAGES);
}

base::LazyInstance<
    base::internal::LazySysInfoValue<uint64_t, AmountOfPhysicalMemory>>::Leaky
    g_lazy_physical_memory = LAZY_INSTANCE_INITIALIZER;

}  // namespace

namespace base {

// static
uint64_t SysInfo::AmountOfPhysicalMemoryImpl() {
  return g_lazy_physical_memory.Get().value();
}

// static
uint64_t SysInfo::AmountOfAvailablePhysicalMemoryImpl() {
  SystemMemoryInfoKB info;
  if (!GetSystemMemoryInfo(&info))
    return 0;
  return AmountOfAvailablePhysicalMemory(info);
}

// static
uint64_t SysInfo::AmountOfAvailablePhysicalMemory(
    const SystemMemoryInfoKB& info) {
  // See details here:
  // https://git.kernel.org/cgit/linux/kernel/git/torvalds/linux.git/commit/?id=34e431b0ae398fc54ea69ff85ec700722c9da773
  // The fallback logic (when there is no MemAvailable) would be more precise
  // if we had info about zones watermarks (/proc/zoneinfo).
  int res_kb = info.available != 0
                   ? std::max(info.available - info.active_file, 0)
                   : info.free + info.reclaimable + info.inactive_file;
  return checked_cast<uint64_t>(res_kb) * 1024;
}

// static
std::string SysInfo::CPUModelName() {
#if BUILDFLAG(IS_CHROMEOS) && defined(ARCH_CPU_ARMEL)
  const char kCpuModelPrefix[] = "Hardware";
#else
  const char kCpuModelPrefix[] = "model name";
#endif
  std::string contents;
  ReadFileToString(FilePath("/proc/cpuinfo"), &contents);
  DCHECK(!contents.empty());
  if (!contents.empty()) {
    std::istringstream iss(contents);
    std::string line;
    while (std::getline(iss, line)) {
      if (line.compare(0, strlen(kCpuModelPrefix), kCpuModelPrefix) == 0) {
        size_t pos = line.find(": ");
        return line.substr(pos + 2);
      }
    }
  }

#if defined(ARCH_CPU_ARMEL)
  // /proc/cpuinfo does not have a defined ABI and so devices may fall
  // through without a model name.
  // For ARM devices use /sys/devices/socX/soc_id
  //
  // https://www.kernel.org/doc/Documentation/ABI/testing/sysfs-devices-soc:
  // On many of ARM based silicon with SMCCC v1.2+ compliant firmware
  // this will contain the SOC ID appended to the family attribute
  // to ensure there is no conflict in this namespace across various
  // vendors. The format is "jep106:XXYY:ZZZZ" where XX is identity
  // code, YY is continuation code and ZZZZ is the SOC ID.

  const char kSocIdDirectory[] = "/sys/devices/soc%u";
  const char kSocIdFile[] = "/sys/devices/soc%u/soc_id";
  const char kJEP106[] = "jep106";

  // There can be multiple /sys/bus/soc/devices/socX on a system.
  // Iterate through until one with jep106:XXYY:ZZZZ is found.
  for (int soc_instance = 0;; ++soc_instance) {
    if (!PathExists(
            FilePath(base::StringPrintf(kSocIdDirectory, soc_instance))))
      break;

    std::string soc_id;
    ReadFileToString(FilePath(base::StringPrintf(kSocIdFile, soc_instance)),
                     &soc_id);
    if (soc_id.find(kJEP106) == 0)
      return soc_id;
  }
#endif

  return std::string();
}

#if !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_ANDROID)
// static
SysInfo::HardwareInfo SysInfo::GetHardwareInfoSync() {
  static const size_t kMaxStringSize = 100u;
  HardwareInfo info;
  std::string data;
  if (ReadFileToStringWithMaxSize(
          FilePath("/sys/devices/virtual/dmi/id/sys_vendor"), &data,
          kMaxStringSize)) {
    TrimWhitespaceASCII(data, TrimPositions::TRIM_ALL, &info.manufacturer);
  }
  if (ReadFileToStringWithMaxSize(
          FilePath("/sys/devices/virtual/dmi/id/product_name"), &data,
          kMaxStringSize)) {
    TrimWhitespaceASCII(data, TrimPositions::TRIM_ALL, &info.model);
  }
  DCHECK(IsStringUTF8(info.manufacturer));
  DCHECK(IsStringUTF8(info.model));
  return info;
}
#endif

}  // namespace base
