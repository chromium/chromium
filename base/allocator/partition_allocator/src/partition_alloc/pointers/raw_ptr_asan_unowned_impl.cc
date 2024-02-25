// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/pointers/raw_ptr_asan_unowned_impl.h"

#include <sanitizer/asan_interface.h>
#include <cstdint>

#include "partition_alloc/partition_alloc_base/compiler_specific.h"

namespace base::internal {

PA_NO_SANITIZE("address")
bool EndOfAliveAllocation(const volatile void* ptr, bool is_adjustable_ptr) {
  uintptr_t address = reinterpret_cast<uintptr_t>(ptr);

  // Normally, we probe the first byte of an object, but in cases of pointer
  // arithmetic, we may be probing subsequent bytes, including the legal
  // "end + 1" position.
  //
  // Alas, ASAN will claim an unmapped page is unpoisoned, so willfully ignore
  // the fist address of a page, since "end + 1" of an object allocated exactly
  // up to a page  boundary will SEGV on probe. This will cause false negatives
  // for pointers that happen to be page aligned, which is undesirable but
  // necessary for now.
  //
  // We minimize the consequences by using the pointer arithmetic flag in
  // higher levels to conditionalize this suppression.
  //
  // TODO(tsepez): this may still fail for a non-accessible but non-null
  // return from, say, malloc(0) which happens to be page-aligned.
  //
  // TODO(tsepez): enforce the pointer arithmetic flag. Until then, we
  // may fail here if a pointer requires the flag but is lacking it.
  return is_adjustable_ptr &&
         ((address & 0x0fff) == 0 ||
          __asan_region_is_poisoned(reinterpret_cast<void*>(address), 1)) &&
         !__asan_region_is_poisoned(reinterpret_cast<void*>(address - 1), 1);
}

bool LikelySmuggledScalar(const volatile void* ptr) {
  intptr_t address = reinterpret_cast<intptr_t>(ptr);
  return address < 0x4000;  // Negative or small positive.
}

}  // namespace base::internal
