// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/test_util.h"

#include "base/system/sys_info.h"
#include "build/build_config.h"

#if defined(OS_POSIX)
#include <sys/resource.h>
#endif

namespace base {
namespace internal {

bool IsLargeMemoryDevice() {
  // Treat any device with 2GiB or more of physical memory as a "large memory
  // device". We check for slightly less than 2GiB so that devices with a small
  // amount of memory not accessible to the OS still count as "large".
  return base::SysInfo::AmountOfPhysicalMemory() >= 2040LL * 1024 * 1024;
}

bool SetDataLimit(size_t memory_limit) {
#if defined(OS_POSIX)
  // Use RLIMIT_DATA rather than RLIMIT_AS, as with the GigaCage, allocations do
  // not necessarily result in an increase of address space, and on Linux,
  // setting a limit lower than the current usage doesn't return an error, and
  // doesn't crash the process right away.  On the other hand, allocations do
  // impact RLIMIT_DATA.
  struct rlimit limit;
  if (getrlimit(RLIMIT_DATA, &limit) != 0)
    return false;
  if (limit.rlim_cur == RLIM_INFINITY || limit.rlim_cur > memory_limit) {
    limit.rlim_cur = memory_limit;
    if (setrlimit(RLIMIT_DATA, &limit) != 0)
      return false;
  }
  return true;
#else
  return false;
#endif
}

bool ClearDataLimit() {
#if defined(OS_POSIX)
  struct rlimit limit;
  if (getrlimit(RLIMIT_DATA, &limit) != 0)
    return false;
  limit.rlim_cur = limit.rlim_max;
  if (setrlimit(RLIMIT_DATA, &limit) != 0)
    return false;
  return true;
#else
  return false;
#endif
}

}  // namespace internal
}  // namespace base
