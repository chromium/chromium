// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/390223051): Remove C-library calls to fix the errors.
#pragma allow_unsafe_libc_calls
#endif

#include "base/system/sys_info.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <limits>
#include <sstream>
#include <type_traits>

#include "base/byte_count.h"
#include "base/check.h"
#include "base/files/file_util.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/process/process_metrics.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info_internal.h"
#include "build/build_config.h"

namespace {

base::ByteCount AmountOfMemory(int pages_name) {
  long pages = sysconf(pages_name);
  long page_size = sysconf(_SC_PAGESIZE);
  if (pages < 0 || page_size < 0) {
    return base::ByteCount(0);
  }
  return base::ByteCount(page_size) * pages;
}

base::ByteCount AmountOfPhysicalMemory() {
  return AmountOfMemory(_SC_PHYS_PAGES);
}
using LazyPhysicalMemory =
    base::internal::LazySysInfoValue<base::ByteCount, AmountOfPhysicalMemory>;

}  // namespace

namespace base {

// static
ByteCount SysInfo::AmountOfPhysicalMemoryImpl() {
  static_assert(std::is_trivially_destructible<LazyPhysicalMemory>::value);
  static LazyPhysicalMemory physical_memory;
  return physical_memory.value();
}

// static
ByteCount SysInfo::AmountOfAvailablePhysicalMemoryImpl() {
  SystemMemoryInfo info;
  if (!GetSystemMemoryInfo(&info)) {
    return ByteCount(0);
  }
  return AmountOfAvailablePhysicalMemory(info);
}

// static
ByteCount SysInfo::AmountOfAvailablePhysicalMemory(
    const SystemMemoryInfo& info) {
  // See details here:
  // https://git.kernel.org/cgit/linux/kernel/git/torvalds/linux.git/commit/?id=34e431b0ae398fc54ea69ff85ec700722c9da773
  // The fallback logic (when there is no MemAvailable) would be more precise
  // if we had info about zones watermarks (/proc/zoneinfo).
  ByteCount res =
      !info.available.is_zero()
          ? std::max(info.available - info.active_file, ByteCount(0))
          : info.free + info.reclaimable + info.inactive_file;
  return res;
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
            FilePath(base::StringPrintf(kSocIdDirectory, soc_instance)))) {
      break;
    }

    std::string soc_id;
    ReadFileToString(FilePath(base::StringPrintf(kSocIdFile, soc_instance)),
                     &soc_id);
    if (soc_id.find(kJEP106) == 0) {
      return soc_id;
    }
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
