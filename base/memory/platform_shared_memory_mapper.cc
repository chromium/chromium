// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/platform_shared_memory_mapper.h"

#include "base/check_op.h"

namespace base {

// static
bool PlatformSharedMemoryMapper::Map(subtle::PlatformSharedMemoryHandle handle,
                                     bool write_allowed,
                                     uint64_t offset,
                                     size_t size,
                                     void** memory,
                                     size_t* mapped_size) {
  return MapInternal(std::move(handle), write_allowed, offset, size, memory,
                     mapped_size);
}

// static
void PlatformSharedMemoryMapper::Unmap(void* memory, size_t size) {
  return UnmapInternal(memory, size);
}

}  // namespace base
