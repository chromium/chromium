// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/platform_shared_memory_mapper.h"

#include "base/allocator/partition_allocator/page_allocator.h"
#include "base/logging.h"

#include <aclapi.h>

namespace base {

namespace {
// Returns the length of the memory section starting at the supplied address.
size_t GetMemorySectionSize(void* address) {
  MEMORY_BASIC_INFORMATION memory_info;
  if (!::VirtualQuery(address, &memory_info, sizeof(memory_info)))
    return 0;
  return memory_info.RegionSize -
         (static_cast<char*>(address) -
          static_cast<char*>(memory_info.AllocationBase));
}
}  // namespace

// static
bool PlatformSharedMemoryMapper::MapInternal(
    subtle::PlatformSharedMemoryHandle handle,
    bool write_allowed,
    uint64_t offset,
    size_t size,
    void** memory,
    size_t* mapped_size) {
  // Try to map the shared memory. On the first failure, release any reserved
  // address space for a single retry.
  for (int i = 0; i < 2; ++i) {
    *memory = MapViewOfFile(
        handle, FILE_MAP_READ | (write_allowed ? FILE_MAP_WRITE : 0),
        static_cast<DWORD>(offset >> 32), static_cast<DWORD>(offset), size);
    if (*memory)
      break;
    ReleaseReservation();
  }
  if (!*memory) {
    DPLOG(ERROR) << "Failed executing MapViewOfFile";
    return false;
  }

  *mapped_size = GetMemorySectionSize(*memory);
  return true;
}

// static
void PlatformSharedMemoryMapper::UnmapInternal(void* memory, size_t size) {
  if (!UnmapViewOfFile(memory))
    DPLOG(ERROR) << "UnmapViewOfFile";
}

}  // namespace base
