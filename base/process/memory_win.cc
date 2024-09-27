// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process/memory.h"

#include "partition_alloc/buildflags.h"

#if PA_BUILDFLAG(USE_ALLOCATOR_SHIM)
#include "partition_alloc/shim/allocator_shim.h"
#endif  // PA_BUILDFLAG(USE_ALLOCATOR_SHIM)

#include <windows.h>  // Must be in front of other Windows header files.

#include <new.h>
#include <psapi.h>
#include <stddef.h>
#include <stdlib.h>

namespace base {

namespace {

// Return a non-0 value to retry the allocation.
int ReleaseReservationOrTerminate(size_t size) {
  constexpr int kRetryAllocation = 1;
  if (internal::ReleaseAddressSpaceReservation())
    return kRetryAllocation;
  TerminateBecauseOutOfMemory(size);
}

}  // namespace

void EnableTerminationOnHeapCorruption() {
  // Ignore the result code. Supported on XP SP3 and Vista.
  HeapSetInformation(NULL, HeapEnableTerminationOnCorruption, NULL, 0);
}

void EnableTerminationOnOutOfMemory() {
  constexpr int kCallNewHandlerOnAllocationFailure = 1;
  _set_new_handler(&ReleaseReservationOrTerminate);
  _set_new_mode(kCallNewHandlerOnAllocationFailure);
}

bool UncheckedMalloc(size_t size, void** result) {
#if PA_BUILDFLAG(USE_ALLOCATOR_SHIM)
  *result = allocator_shim::UncheckedAlloc(size);
#else
  // malloc_unchecked is required to implement UncheckedMalloc properly.
  // It's provided by allocator_shim_win.cc but since that's not always present,
  // In the case, use regular malloc instead.
  *result = malloc(size);
#endif  // PA_BUILDFLAG(USE_ALLOCATOR_SHIM)
  return *result != NULL;
}

void UncheckedFree(void* ptr) {
#if PA_BUILDFLAG(USE_ALLOCATOR_SHIM)
  allocator_shim::UncheckedFree(ptr);
#else
  free(ptr);
#endif  // PA_BUILDFLAG(USE_ALLOCATOR_SHIM)
}

}  // namespace base
