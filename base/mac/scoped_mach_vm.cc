// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/mac/scoped_mach_vm.h"

#include "base/mac/mach_logging.h"

namespace base {
namespace mac {

void ScopedMachVM::reset(vm_address_t address, vm_size_t size) {
  DCHECK_EQ(address % PAGE_SIZE, 0u);
  DCHECK_EQ(size % PAGE_SIZE, 0u);
  reset_unaligned(address, size);
}

void ScopedMachVM::reset_unaligned(vm_address_t address, vm_size_t size) {
  if (size_) {
    if (address_ < address) {
      kern_return_t kr = vm_deallocate(mach_task_self(), address_,
                                       std::min(size_, address - address_));
      MACH_DCHECK(kr == KERN_SUCCESS, kr) << "vm_deallocate";
    }
    if (address_ + size_ > address + size) {
      vm_address_t deallocate_start = std::max(address_, address + size);
      kern_return_t kr = vm_deallocate(mach_task_self(), deallocate_start,
                                       address_ + size_ - deallocate_start);
      MACH_DCHECK(kr == KERN_SUCCESS, kr) << "vm_deallocate";
    }
  }

  address_ = address;
  size_ = size;
}

}  // namespace mac
}  // namespace base
