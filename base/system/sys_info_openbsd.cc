// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/system/sys_info.h"

#include <stddef.h>
#include <stdint.h>
#include <sys/param.h>
#include <sys/shm.h>
#include <sys/sysctl.h>

#include "base/notreached.h"
#include "base/posix/sysctl.h"

namespace {

uint64_t AmountOfMemory(int pages_name) {
  long pages = sysconf(pages_name);
  long page_size = sysconf(_SC_PAGESIZE);
  if (pages < 0 || page_size < 0)
    return 0;
  return static_cast<uint64_t>(pages) * static_cast<uint64_t>(page_size);
}

}  // namespace

namespace base {

// static
int SysInfo::NumberOfProcessors() {
  int mib[] = {CTL_HW, HW_NCPU};
  int ncpu;
  size_t size = sizeof(ncpu);
  if (sysctl(mib, std::size(mib), &ncpu, &size, NULL, 0) < 0) {
    NOTREACHED();
  }
  return ncpu;
}

// static
uint64_t SysInfo::AmountOfPhysicalMemoryImpl() {
  return AmountOfMemory(_SC_PHYS_PAGES);
}

// static
uint64_t SysInfo::AmountOfAvailablePhysicalMemoryImpl() {
  // We should add inactive file-backed memory also but there is no such
  // information from OpenBSD unfortunately.
  return AmountOfMemory(_SC_AVPHYS_PAGES);
}

// static
uint64_t SysInfo::MaxSharedMemorySize() {
  int mib[] = {CTL_KERN, KERN_SHMINFO, KERN_SHMINFO_SHMMAX};
  size_t limit;
  size_t size = sizeof(limit);
  if (sysctl(mib, std::size(mib), &limit, &size, NULL, 0) < 0) {
    NOTREACHED();
  }
  return static_cast<uint64_t>(limit);
}

// static
std::string SysInfo::CPUModelName() {
  return StringSysctl({CTL_HW, HW_MODEL}).value();
}

}  // namespace base
