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
  return __asan_region_is_poisoned(reinterpret_cast<void*>(address), 1) &&
         !__asan_region_is_poisoned(reinterpret_cast<void*>(address - 1), 1);
}

bool RawPtrAsanUnownedImpl::LikelySmuggledScalar(const volatile void* ptr) {
  intptr_t address = reinterpret_cast<intptr_t>(ptr);
  return address < 0x4000;  // Negative or small positive.
}

}  // namespace base::internal
