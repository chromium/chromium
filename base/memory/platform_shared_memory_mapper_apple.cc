// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/platform_shared_memory_mapper.h"

#include <mach/vm_map.h>

#include "base/apple/mach_logging.h"
#include "base/containers/span.h"
#include "base/logging.h"

namespace base {

std::optional<span<uint8_t>> PlatformSharedMemoryMapper::Map(
    subtle::PlatformSharedMemoryHandle handle,
    bool write_allowed,
    uint64_t offset,
    size_t size) {
  vm_prot_t vm_prot_write = write_allowed ? VM_PROT_WRITE : 0;
  vm_address_t address = 0;
  kern_return_t kr = vm_map(mach_task_self(),
                            &address,  // Output parameter
                            size,
                            0,  // Alignment mask
                            VM_FLAGS_ANYWHERE, handle, offset,
                            FALSE,                         // Copy
                            VM_PROT_READ | vm_prot_write,  // Current protection
                            VM_PROT_READ | vm_prot_write,  // Maximum protection
                            VM_INHERIT_NONE);
  if (kr != KERN_SUCCESS) {
    MACH_DLOG(ERROR, kr) << "vm_map";
    return std::nullopt;
  }

  // SAFETY: vm_map() maps a memory segment of `size` bytes. Since
  // `VM_FLAGS_ANYWHERE` is used, the address will be chosen by vm_map() and
  // returned in `address`.
  return UNSAFE_BUFFERS(base::span(reinterpret_cast<uint8_t*>(address), size));
}

void PlatformSharedMemoryMapper::Unmap(span<uint8_t> mapping) {
  kern_return_t kr = vm_deallocate(
      mach_task_self(), reinterpret_cast<vm_address_t>(mapping.data()),
      mapping.size());
  MACH_DLOG_IF(ERROR, kr != KERN_SUCCESS, kr) << "vm_deallocate";
}

}  // namespace base
