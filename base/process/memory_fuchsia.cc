// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process/memory.h"

#include "partition_alloc/buildflags.h"

#if PA_BUILDFLAG(USE_ALLOCATOR_SHIM)
#include "partition_alloc/shim/allocator_shim.h"
#endif

#include <stdlib.h>

namespace base {

void EnableTerminationOnOutOfMemory() {
  // Nothing to be done here.
}

void EnableTerminationOnHeapCorruption() {
  // Nothing to be done here.
}

bool UncheckedMalloc(size_t size, void** result) {
#if PA_BUILDFLAG(USE_ALLOCATOR_SHIM)
  *result = allocator_shim::UncheckedAlloc(size);
#else
  *result = malloc(size);
#endif
  return *result != nullptr;
}

void UncheckedFree(void* ptr) {
#if PA_BUILDFLAG(USE_ALLOCATOR_SHIM)
  allocator_shim::UncheckedFree(ptr);
#else
  free(ptr);
#endif
}

}  // namespace base
