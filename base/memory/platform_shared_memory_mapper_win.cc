// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "base/memory/platform_shared_memory_mapper.h"

#include "base/logging.h"
#include "partition_alloc/page_allocator.h"

#include <aclapi.h>

namespace base {

namespace {
// Returns the length of the memory section starting at the supplied address.
size_t GetMemorySectionSize(void* address) {
  MEMORY_BASIC_INFORMATION memory_info;
  if (!::VirtualQuery(address, &memory_info, sizeof(memory_info)))
    return 0;
  return memory_info.RegionSize -
         static_cast<size_t>(static_cast<char*>(address) -
                             static_cast<char*>(memory_info.AllocationBase));
}
}  // namespace

std::optional<span<uint8_t>> PlatformSharedMemoryMapper::Map(
    subtle::PlatformSharedMemoryHandle handle,
    bool write_allowed,
    uint64_t offset,
    size_t size) {
  // Try to map the shared memory. On the first failure, release any reserved
  // address space for a single retry.
  void* address;
  for (int i = 0; i < 2; ++i) {
    address = MapViewOfFile(
        handle, FILE_MAP_READ | (write_allowed ? FILE_MAP_WRITE : 0),
        static_cast<DWORD>(offset >> 32), static_cast<DWORD>(offset), size);
    if (address)
      break;
    partition_alloc::ReleaseReservation();
  }
  if (!address) {
    DPLOG(ERROR) << "Failed executing MapViewOfFile";
    return std::nullopt;
  }

  return make_span(static_cast<uint8_t*>(address),
                   GetMemorySectionSize(address));
}

void PlatformSharedMemoryMapper::Unmap(span<uint8_t> mapping) {
  if (!UnmapViewOfFile(mapping.data()))
    DPLOG(ERROR) << "UnmapViewOfFile";
}

}  // namespace base
