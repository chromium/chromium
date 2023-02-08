// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/pointers/raw_ptr_asan_unowned_impl.h"

#include <sanitizer/asan_interface.h>
#include <cstdint>

#include "base/allocator/partition_allocator/partition_alloc_base/compiler_specific.h"

namespace base::internal {

PA_NO_SANITIZE("address")
bool RawPtrAsanUnownedImpl::EndOfAliveAllocation(const volatile void* ptr) {
  uintptr_t address = reinterpret_cast<uintptr_t>(ptr);

  // Alas, ASAN will claim an unmapped page is unpoisoned, so willfully ignore
  // the fist address of a page, since "end + 1" of an object allocated exactly
  // up to a page  boundary will SEGV on probe. This will cause false negatives
  // for pointers that happen to be page aligned, which is undesirable but
  // necessary for now.
  // TODO(tsepez): investigate pointer tracking approaches to avoid this.
  return ((address & 0x0fff) == 0 ||
          __asan_region_is_poisoned(reinterpret_cast<void*>(address), 1)) &&
         !__asan_region_is_poisoned(reinterpret_cast<void*>(address - 1), 1);
}

bool RawPtrAsanUnownedImpl::LikelySmuggledScalar(const volatile void* ptr) {
  intptr_t address = reinterpret_cast<intptr_t>(ptr);
  return address < 0x4000;  // Negative or small positive.
}

}  // namespace base::internal
