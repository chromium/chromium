// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <stdlib.h>

#include "base/allocator/partition_allocator/src/partition_alloc/buildflags.h"
#include "base/allocator/partition_allocator/src/partition_alloc/partition_address_space.h"

int main() {
  void* ptr = malloc(100);
  if (!ptr) {
    return 1;
  }

#if PA_BUILDFLAG(HAS_64_BIT_POINTERS)
  // Verify that the memory returned by malloc() is NOT managed by
  // PartitionAlloc.
  // This check is only available on 64-bit builds because PartitionAlloc's
  // core address space management (and the IsManagedByPartitionAlloc API)
  // fundamentally relies on 64-bit address space reservations. On 32-bit,
  // the fallback is assumed to be working if malloc/free succeed without
  // crashing.
  if (partition_alloc::IsManagedByPartitionAlloc(
          reinterpret_cast<uintptr_t>(ptr))) {
    return 1;  // Failed: PartitionAlloc is active!
  }
#endif

  free(ptr);
  return 0;  // Success.
}
