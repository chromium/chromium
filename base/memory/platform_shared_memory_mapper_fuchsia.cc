// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "base/memory/platform_shared_memory_mapper.h"

#include "base/logging.h"

#include <lib/zx/vmar.h>
#include "base/fuchsia/fuchsia_logging.h"

namespace base {

std::optional<span<uint8_t>> PlatformSharedMemoryMapper::Map(
    subtle::PlatformSharedMemoryHandle handle,
    bool write_allowed,
    uint64_t offset,
    size_t size) {
  uintptr_t addr;
  zx_vm_option_t options = ZX_VM_REQUIRE_NON_RESIZABLE | ZX_VM_PERM_READ;
  if (write_allowed)
    options |= ZX_VM_PERM_WRITE;
  zx_status_t status = zx::vmar::root_self()->map(options, /*vmar_offset=*/0,
                                                  *handle, offset, size, &addr);
  if (status != ZX_OK) {
    ZX_DLOG(ERROR, status) << "zx_vmar_map";
    return std::nullopt;
  }

  return make_span(reinterpret_cast<uint8_t*>(addr), size);
}

void PlatformSharedMemoryMapper::Unmap(span<uint8_t> mapping) {
  uintptr_t addr = reinterpret_cast<uintptr_t>(mapping.data());
  zx_status_t status = zx::vmar::root_self()->unmap(addr, mapping.size());
  if (status != ZX_OK)
    ZX_DLOG(ERROR, status) << "zx_vmar_unmap";
}

}  // namespace base
