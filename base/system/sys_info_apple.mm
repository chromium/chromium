// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/system/sys_info.h"

#include <sys/sysctl.h>

#include "base/strings/stringprintf.h"
#include "base/system/sys_info_internal.h"

namespace base {

namespace internal {

// Queries sysctlbyname() for the given key and returns the 32 bit integer value
// from the system or std::nullopt on failure.
// https://github.com/apple/darwin-xnu/blob/2ff845c2e033bd0ff64b5b6aa6063a1f8f65aa32/bsd/sys/sysctl.h#L1224-L1225
std::optional<int> GetSysctlIntValue(const char* key_name) {
  int value;
  size_t len = sizeof(value);
  if (sysctlbyname(key_name, &value, &len, nullptr, 0) != 0) {
    return std::nullopt;
  }
  DCHECK_EQ(len, sizeof(value));
  return value;
}

}  // namespace internal

// static
int SysInfo::NumberOfEfficientProcessorsImpl() {
  int num_perf_levels =
      internal::GetSysctlIntValue("hw.nperflevels").value_or(1);
  if (num_perf_levels == 1) {
    return 0;
  }
  DCHECK_GE(num_perf_levels, 2);

  // Lower values of perflevel indicate higher-performance core types. See
  // https://developer.apple.com/documentation/kernel/1387446-sysctlbyname/determining_system_capabilities?changes=l__5
  int num_of_efficient_processors =
      internal::GetSysctlIntValue(
          StringPrintf("hw.perflevel%d.logicalcpu", num_perf_levels - 1)
              .c_str())
          .value_or(0);
  DCHECK_GE(num_of_efficient_processors, 0);

  return num_of_efficient_processors;
}

}  // namespace base
