// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/platform_shared_memory_mapper.h"

#include "base/logging.h"
#include "base/numerics/safe_conversions.h"

#include <sys/mman.h>

namespace base {

// static
bool PlatformSharedMemoryMapper::MapInternal(
    subtle::PlatformSharedMemoryHandle handle,
    bool write_allowed,
    uint64_t offset,
    size_t size,
    void** memory,
    size_t* mapped_size) {
  // IMPORTANT: Even if the mapping is readonly and the mapped data is not
  // changing, the region must ALWAYS be mapped with MAP_SHARED, otherwise with
  // ashmem the mapping is equivalent to a private anonymous mapping.
  *memory = mmap(nullptr, size, PROT_READ | (write_allowed ? PROT_WRITE : 0),
                 MAP_SHARED, handle, checked_cast<off_t>(offset));

  bool mmap_succeeded = *memory && *memory != MAP_FAILED;
  if (!mmap_succeeded) {
    DPLOG(ERROR) << "mmap " << handle << " failed";
    return false;
  }

  *mapped_size = size;
  return true;
}

// static
void PlatformSharedMemoryMapper::UnmapInternal(void* memory, size_t size) {
  if (munmap(memory, size) < 0)
    DPLOG(ERROR) << "munmap";
}

}  // namespace base
