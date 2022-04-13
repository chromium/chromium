// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MEMORY_PLATFORM_SHARED_MEMORY_MAPPER_H_
#define BASE_MEMORY_PLATFORM_SHARED_MEMORY_MAPPER_H_

#include "base/base_export.h"
#include "base/memory/platform_shared_memory_handle.h"

#include <stdint.h>

namespace base {

// Static class responsible for the platform-specific logic to map a shared
// memory region into the virtual address space of the process.
class BASE_EXPORT PlatformSharedMemoryMapper {
 public:
  PlatformSharedMemoryMapper() = delete;

  static bool Map(subtle::PlatformSharedMemoryHandle handle,
                  bool write_allowed,
                  uint64_t offset,
                  size_t size,
                  void** memory,
                  size_t* mapped_size);

  static void Unmap(void* memory, size_t size);

 private:
  static bool MapInternal(subtle::PlatformSharedMemoryHandle handle,
                          bool write_allowed,
                          uint64_t offset,
                          size_t size,
                          void** memory,
                          size_t* mapped_size);

  static void UnmapInternal(void* memory, size_t size);
};

}  // namespace base

#endif  // BASE_MEMORY_PLATFORM_SHARED_MEMORY_MAPPER_H_
