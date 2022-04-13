// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/platform_shared_memory_mapper.h"

#include "base/logging.h"

#include <mach/mach_vm.h>
#include "base/mac/mach_logging.h"

namespace base {

// static
bool PlatformSharedMemoryMapper::MapInternal(
    subtle::PlatformSharedMemoryHandle handle,
    bool write_allowed,
    uint64_t offset,
    size_t size,
    void** memory,
    size_t* mapped_size) {
  vm_prot_t vm_prot_write = write_allowed ? VM_PROT_WRITE : 0;
  kern_return_t kr = mach_vm_map(
      mach_task_self(),
      reinterpret_cast<mach_vm_address_t*>(memory),  // Output parameter
      size,
      0,  // Alignment mask
      VM_FLAGS_ANYWHERE, handle, offset,
      FALSE,                         // Copy
      VM_PROT_READ | vm_prot_write,  // Current protection
      VM_PROT_READ | vm_prot_write,  // Maximum protection
      VM_INHERIT_NONE);
  if (kr != KERN_SUCCESS) {
    MACH_DLOG(ERROR, kr) << "mach_vm_map";
    return false;
  }

  *mapped_size = size;
  return true;
}

// static
void PlatformSharedMemoryMapper::UnmapInternal(void* memory, size_t size) {
  kern_return_t kr = mach_vm_deallocate(
      mach_task_self(), reinterpret_cast<mach_vm_address_t>(memory), size);
  MACH_DLOG_IF(ERROR, kr != KERN_SUCCESS, kr) << "mach_vm_deallocate";
}

}  // namespace base
