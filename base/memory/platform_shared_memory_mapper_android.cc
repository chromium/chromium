// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/platform_shared_memory_mapper.h"

#include <sys/mman.h>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"

namespace base {

std::optional<span<uint8_t>> PlatformSharedMemoryMapper::Map(
    subtle::PlatformSharedMemoryHandle handle,
    bool write_allowed,
    uint64_t offset,
    size_t size) {
  // IMPORTANT: Even if the mapping is readonly and the mapped data is not
  // changing, the region should be mapped with MAP_SHARED.
  //
  // With ashmem doing a mmap(MAP_PRIVATE) would be equivalent anonymous
  // mapping, not a shared one.
  //
  // Historically, memfd had the opposite and confusing behavior: attempting to
  // mmap(MAP_SHARED) a sealed, read-only region would fail, while
  // mmap(MAP_PRIVATE) would correctly create a read-only shared mapping.
  //
  // This inconsistency was resolved in Linux kernel 6.7, which now allows
  // MAP_SHARED to work as well. This fix may also be cherry-picked to
  // older LTS kernels.
  //
  // See the upstream patches:
  //    https://lkml.kernel.org/r/cover.1697116581.git.lstoakes@gmail.com
  //
  // For old devices PlatformSharedMemoryMapper only receives ashmem regions.
  // New devices shipping with Android 17 do not have ashmem, and memfd should
  // be used instead. These new systems are based on 6.12+ kernels (in other
  // words, MAP_SHARED works for them). Devices updating to Android 17
  // (potentially with kernels as old as 6.6) continue using ashmem.
  //
  // Therefore, it is unnecessary to dynamically check the memory region type
  // (ashmem vs. memfd), as MAP_SHARED is the correct flag for all scenarios
  // this code handles.
  void* address =
      mmap(nullptr, size, PROT_READ | (write_allowed ? PROT_WRITE : 0),
           MAP_SHARED, handle, checked_cast<off_t>(offset));

  if (address == MAP_FAILED) {
    DPLOG(ERROR) << "mmap " << handle << " failed";
    return std::nullopt;
  }

  return UNSAFE_TODO(span(static_cast<uint8_t*>(address), size));
}

void PlatformSharedMemoryMapper::Unmap(span<uint8_t> mapping) {
  if (munmap(mapping.data(), mapping.size()) < 0) {
    DPLOG(ERROR) << "munmap";
  }
}

}  // namespace base
