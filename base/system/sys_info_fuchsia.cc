// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/system/sys_info.h"

#include <zircon/syscalls.h>

#include "base/containers/flat_map.h"
#include "base/files/file_util.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/synchronization/lock.h"
#include "base/threading/scoped_blocking_call.h"
#include "build/build_config.h"

namespace base {

namespace {

struct TotalDiskSpace {
  Lock lock;
  flat_map<FilePath, int64_t> space_map GUARDED_BY(lock);
};

TotalDiskSpace& GetTotalDiskSpace() {
  static NoDestructor<TotalDiskSpace> total_disk_space;
  return *total_disk_space;
}

// Returns the total-disk-space set for the volume containing |path|. If
// |volume_path| is non-null then it receives the path to the relevant volume.
// Returns -1, and does not modify |volume_path|, if no match is found.
int64_t GetAmountOfTotalDiskSpaceAndVolumePath(const FilePath& path,
                                               base::FilePath* volume_path) {
  CHECK(path.IsAbsolute());
  TotalDiskSpace& total_disk_space = GetTotalDiskSpace();

  AutoLock l(total_disk_space.lock);
  int64_t result = -1;
  base::FilePath matched_path;
  for (const auto& path_and_size : total_disk_space.space_map) {
    if (path_and_size.first == path || path_and_size.first.IsParent(path)) {
      // If a deeper path was already matched then ignore this entry.
      if (!matched_path.empty() && !matched_path.IsParent(path_and_size.first))
        continue;
      matched_path = path_and_size.first;
      result = path_and_size.second;
    }
  }

  if (volume_path)
    *volume_path = matched_path;
  return result;
}

}  // namespace

// static
int64_t SysInfo::AmountOfPhysicalMemoryImpl() {
  return zx_system_get_physmem();
}

// static
int64_t SysInfo::AmountOfAvailablePhysicalMemoryImpl() {
  // TODO(https://crbug.com/986608): Implement this.
  NOTIMPLEMENTED_LOG_ONCE();
  return 0;
}

// static
int SysInfo::NumberOfProcessors() {
  return zx_system_get_num_cpus();
}

// static
int64_t SysInfo::AmountOfVirtualMemory() {
  return 0;
}

// static
std::string SysInfo::OperatingSystemName() {
  return "Fuchsia";
}

// static
int64_t SysInfo::AmountOfFreeDiskSpace(const FilePath& path) {
  ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::MAY_BLOCK);
  base::FilePath volume_path;
  const int64_t total_space =
      GetAmountOfTotalDiskSpaceAndVolumePath(path, &volume_path);
  if (total_space < 0)
    return -1;

  // TODO(crbug.com/1148334): Replace this with an efficient implementation.
  const int64_t used_space = ComputeDirectorySize(volume_path);
  if (used_space < total_space)
    return total_space - used_space;

  return 0;
}

// static
int64_t SysInfo::AmountOfTotalDiskSpace(const FilePath& path) {
  if (path.empty())
    return -1;
  return GetAmountOfTotalDiskSpaceAndVolumePath(path, nullptr);
}

// static
void SysInfo::SetAmountOfTotalDiskSpace(const FilePath& path, int64_t bytes) {
  DCHECK(path.IsAbsolute());
  TotalDiskSpace& total_disk_space = GetTotalDiskSpace();
  AutoLock l(total_disk_space.lock);
  if (bytes >= 0)
    total_disk_space.space_map[path] = bytes;
  else
    total_disk_space.space_map.erase(path);
}

// static
std::string SysInfo::OperatingSystemVersion() {
  return zx_system_get_version_string();
}

// static
void SysInfo::OperatingSystemVersionNumbers(int32_t* major_version,
                                            int32_t* minor_version,
                                            int32_t* bugfix_version) {
  // Fuchsia doesn't have OS version numbers.
  *major_version = 0;
  *minor_version = 0;
  *bugfix_version = 0;
}

// static
std::string SysInfo::OperatingSystemArchitecture() {
#if defined(ARCH_CPU_X86_64)
  return "x86_64";
#elif defined(ARCH_CPU_ARM64)
  return "aarch64";
#else
#error Unsupported architecture.
#endif
}

// static
std::string SysInfo::CPUModelName() {
  NOTIMPLEMENTED_LOG_ONCE();
  return std::string();
}

// static
size_t SysInfo::VMAllocationGranularity() {
  return getpagesize();
}

}  // namespace base
