// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/platform_shared_memory_mapper.h"

#include "base/logging.h"

#include <lib/zx/vmar.h>
#include "base/fuchsia/fuchsia_logging.h"

namespace base {

// static
bool PlatformSharedMemoryMapper::MapInternal(
    subtle::PlatformSharedMemoryHandle handle,
    bool write_allowed,
    uint64_t offset,
    size_t size,
    void** memory,
    size_t* mapped_size) {
  uintptr_t addr;
  zx_vm_option_t options = ZX_VM_REQUIRE_NON_RESIZABLE | ZX_VM_PERM_READ;
  if (write_allowed)
    options |= ZX_VM_PERM_WRITE;
  zx_status_t status = zx::vmar::root_self()->map(options, /*vmar_offset=*/0,
                                                  *handle, offset, size, &addr);
  if (status != ZX_OK) {
    ZX_DLOG(ERROR, status) << "zx_vmar_map";
    return false;
  }

  *memory = reinterpret_cast<void*>(addr);
  *mapped_size = size;
  return true;
}

// static
void PlatformSharedMemoryMapper::UnmapInternal(void* memory, size_t size) {
  uintptr_t addr = reinterpret_cast<uintptr_t>(memory);
  zx_status_t status = zx::vmar::root_self()->unmap(addr, size);
  if (status != ZX_OK)
    ZX_DLOG(ERROR, status) << "zx_vmar_unmap";
}

}  // namespace base
