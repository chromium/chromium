// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_shared_memory_util.h"

#include <gtest/gtest.h>

#include <stddef.h>
#include <stdint.h>

#include "base/logging.h"
#include "build/build_config.h"

#if defined(OS_POSIX) && !defined(OS_NACL)
#include <errno.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#endif

#if defined(OS_FUCHSIA)
#include <lib/zx/vmar.h>
#include <zircon/rights.h>
#endif

#if defined(OS_MAC)
#include <mach/mach_vm.h>
#endif

#if defined(OS_WIN)
#include <aclapi.h>
#endif

namespace base {

#if !defined(OS_NACL)

static const size_t kDataSize = 1024;

// Common routine used with Posix file descriptors. Check that shared memory
// file descriptor |fd| does not allow writable mappings. Return true on
// success, false otherwise.
#if defined(OS_POSIX) && !defined(OS_MAC)
static bool CheckReadOnlySharedMemoryFdPosix(int fd) {
// Note that the error on Android is EPERM, unlike other platforms where
// it will be EACCES.
#if defined(OS_ANDROID)
  const int kExpectedErrno = EPERM;
#else
  const int kExpectedErrno = EACCES;
#endif
  errno = 0;
  void* address =
      mmap(nullptr, kDataSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  const bool success = (address != nullptr) && (address != MAP_FAILED);
  if (success) {
    LOG(ERROR) << "mmap() should have failed!";
    munmap(address, kDataSize);  // Cleanup.
    return false;
  }
  if (errno != kExpectedErrno) {
    LOG(ERROR) << "Expected mmap() to return " << kExpectedErrno
               << " but returned " << errno << ": " << strerror(errno) << "\n";
    return false;
  }
  return true;
}
#endif  // OS_POSIX && !defined(OS_MAC)

#if defined(OS_FUCHSIA)
// Fuchsia specific implementation.
bool CheckReadOnlySharedMemoryFuchsiaHandle(zx::unowned_vmo handle) {
  const uint32_t flags = ZX_VM_PERM_READ | ZX_VM_PERM_WRITE;
  uintptr_t addr;
  const zx_status_t status =
      zx::vmar::root_self()->map(flags, 0, *handle, 0U, kDataSize, &addr);
  if (status == ZX_OK) {
    LOG(ERROR) << "zx_vmar_map() should have failed!";
    zx::vmar::root_self()->unmap(addr, kDataSize);
    return false;
  }
  if (status != ZX_ERR_ACCESS_DENIED) {
    LOG(ERROR) << "Expected zx_vmar_map() to return " << ZX_ERR_ACCESS_DENIED
               << " (ZX_ERR_ACCESS_DENIED) but returned " << status << "\n";
    return false;
  }
  return true;
}

#elif defined(OS_MAC)
bool CheckReadOnlySharedMemoryMachPort(mach_port_t memory_object) {
  mach_vm_address_t memory;
  const kern_return_t kr = mach_vm_map(
      mach_task_self(), &memory, kDataSize, 0, VM_FLAGS_ANYWHERE, memory_object,
      0, FALSE, VM_PROT_READ | VM_PROT_WRITE,
      VM_PROT_READ | VM_PROT_WRITE | VM_PROT_IS_MASK, VM_INHERIT_NONE);
  if (kr == KERN_SUCCESS) {
    LOG(ERROR) << "mach_vm_map() should have failed!";
    mach_vm_deallocate(mach_task_self(), memory, kDataSize);  // Cleanup.
    return false;
  }
  return true;
}

#elif defined(OS_WIN)
bool CheckReadOnlySharedMemoryWindowsHandle(HANDLE handle) {
  void* memory =
      MapViewOfFile(handle, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, kDataSize);
  if (memory != nullptr) {
    LOG(ERROR) << "MapViewOfFile() should have failed!";
    UnmapViewOfFile(memory);
    return false;
  }
  return true;
}
#endif

bool CheckReadOnlyPlatformSharedMemoryRegionForTesting(
    subtle::PlatformSharedMemoryRegion region) {
  if (region.GetMode() != subtle::PlatformSharedMemoryRegion::Mode::kReadOnly) {
    LOG(ERROR) << "Expected region mode is "
               << static_cast<int>(
                      subtle::PlatformSharedMemoryRegion::Mode::kReadOnly)
               << " but actual is " << static_cast<int>(region.GetMode());
    return false;
  }

#if defined(OS_MAC)
  return CheckReadOnlySharedMemoryMachPort(region.GetPlatformHandle());
#elif defined(OS_FUCHSIA)
  return CheckReadOnlySharedMemoryFuchsiaHandle(region.GetPlatformHandle());
#elif defined(OS_WIN)
  return CheckReadOnlySharedMemoryWindowsHandle(region.GetPlatformHandle());
#elif defined(OS_ANDROID)
  return CheckReadOnlySharedMemoryFdPosix(region.GetPlatformHandle());
#else
  return CheckReadOnlySharedMemoryFdPosix(region.GetPlatformHandle().fd);
#endif
}

#endif  // !OS_NACL

WritableSharedMemoryMapping MapForTesting(
    subtle::PlatformSharedMemoryRegion* region) {
  return MapAtForTesting(region, 0, region->GetSize());
}

WritableSharedMemoryMapping MapAtForTesting(
    subtle::PlatformSharedMemoryRegion* region,
    off_t offset,
    size_t size) {
  void* memory = nullptr;
  size_t mapped_size = 0;
  if (!region->MapAt(offset, size, &memory, &mapped_size))
    return {};

  return WritableSharedMemoryMapping(memory, size, mapped_size,
                                     region->GetGUID());
}

template <>
std::pair<ReadOnlySharedMemoryRegion, WritableSharedMemoryMapping>
CreateMappedRegion(size_t size) {
  MappedReadOnlyRegion mapped_region = ReadOnlySharedMemoryRegion::Create(size);
  return {std::move(mapped_region.region), std::move(mapped_region.mapping)};
}

}  // namespace base
