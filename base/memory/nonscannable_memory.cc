// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/nonscannable_memory.h"

#include "base/allocator/partition_allocator/src/partition_alloc/partition_alloc_buildflags.h"

#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
#include "base/allocator/partition_allocator/src/partition_alloc/shim/nonscannable_allocator.h"
#else
#include <stdlib.h>
#endif

namespace base {

void* AllocNonScannable(size_t size) {
#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  return allocator_shim::NonScannableAllocator::Instance().Alloc(size);
#else
  return ::malloc(size);
#endif
}

void FreeNonScannable(void* ptr) {
#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  allocator_shim::NonScannableAllocator::Instance().Free(ptr);
#else
  return ::free(ptr);
#endif
}

void* AllocNonQuarantinable(size_t size) {
#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  return allocator_shim::NonQuarantinableAllocator::Instance().Alloc(size);
#else
  return ::malloc(size);
#endif
}

void FreeNonQuarantinable(void* ptr) {
#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  allocator_shim::NonQuarantinableAllocator::Instance().Free(ptr);
#else
  return ::free(ptr);
#endif
}

}  // namespace base
