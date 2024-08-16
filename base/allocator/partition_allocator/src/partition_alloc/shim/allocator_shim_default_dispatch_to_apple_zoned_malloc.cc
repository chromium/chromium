// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "partition_alloc/shim/allocator_interception_apple.h"
#include "partition_alloc/shim/allocator_shim.h"
#include "partition_alloc/shim/malloc_zone_functions_apple.h"

namespace allocator_shim {
namespace {

void* MallocImpl(size_t size, void* context) {
  MallocZoneFunctions& functions = GetFunctionsForZone(context);
  return functions.malloc(reinterpret_cast<struct _malloc_zone_t*>(context),
                          size);
}

void* CallocImpl(size_t n, size_t size, void* context) {
  MallocZoneFunctions& functions = GetFunctionsForZone(context);
  return functions.calloc(reinterpret_cast<struct _malloc_zone_t*>(context), n,
                          size);
}

void* MemalignImpl(size_t alignment, size_t size, void* context) {
  MallocZoneFunctions& functions = GetFunctionsForZone(context);
  return functions.memalign(reinterpret_cast<struct _malloc_zone_t*>(context),
                            alignment, size);
}

void* ReallocImpl(void* ptr, size_t size, void* context) {
  MallocZoneFunctions& functions = GetFunctionsForZone(context);
  return functions.realloc(reinterpret_cast<struct _malloc_zone_t*>(context),
                           ptr, size);
}

void FreeImpl(void* ptr, void* context) {
  MallocZoneFunctions& functions = GetFunctionsForZone(context);
  functions.free(reinterpret_cast<struct _malloc_zone_t*>(context), ptr);
}

size_t GetSizeEstimateImpl(void* ptr, void* context) {
  MallocZoneFunctions& functions = GetFunctionsForZone(context);
  return functions.size(reinterpret_cast<struct _malloc_zone_t*>(context), ptr);
}

size_t GoodSizeImpl(size_t size, void* context) {
  // Technically, libmalloc will only call good_size() on the default zone for
  // malloc_good_size(), but it doesn't matter that we are calling it on another
  // one.
  MallocZoneFunctions& functions = GetFunctionsForZone(context);
  return functions.good_size(reinterpret_cast<struct _malloc_zone_t*>(context),
                             size);
}

bool ClaimedAddressImpl(void* ptr, void* context) {
  MallocZoneFunctions& functions = GetFunctionsForZone(context);
  if (functions.claimed_address) {
    return functions.claimed_address(
        reinterpret_cast<struct _malloc_zone_t*>(context), ptr);
  }
  // If the fast API 'claimed_address' is not implemented in the specified zone,
  // fall back to 'size' function, which also tells whether the given address
  // belongs to the zone or not although it'd be slow.
  return functions.size(reinterpret_cast<struct _malloc_zone_t*>(context), ptr);
}

unsigned BatchMallocImpl(size_t size,
                         void** results,
                         unsigned num_requested,
                         void* context) {
  MallocZoneFunctions& functions = GetFunctionsForZone(context);
  return functions.batch_malloc(
      reinterpret_cast<struct _malloc_zone_t*>(context), size, results,
      num_requested);
}

void BatchFreeImpl(void** to_be_freed,
                   unsigned num_to_be_freed,
                   void* context) {
  MallocZoneFunctions& functions = GetFunctionsForZone(context);
  functions.batch_free(reinterpret_cast<struct _malloc_zone_t*>(context),
                       to_be_freed, num_to_be_freed);
}

void FreeDefiniteSizeImpl(void* ptr, size_t size, void* context) {
  MallocZoneFunctions& functions = GetFunctionsForZone(context);
  functions.free_definite_size(
      reinterpret_cast<struct _malloc_zone_t*>(context), ptr, size);
}

void TryFreeDefaultImpl(void* ptr, void* context) {
  MallocZoneFunctions& functions = GetFunctionsForZone(context);
  if (functions.try_free_default) {
    return functions.try_free_default(
        reinterpret_cast<struct _malloc_zone_t*>(context), ptr);
  }
  allocator_shim::TryFreeDefaultFallbackToFindZoneAndFree(ptr);
}

}  // namespace

const AllocatorDispatch AllocatorDispatch::default_dispatch = {
    &MallocImpl,           /* alloc_function */
    &MallocImpl,           /* alloc_unchecked_function */
    &CallocImpl,           /* alloc_zero_initialized_function */
    &MemalignImpl,         /* alloc_aligned_function */
    &ReallocImpl,          /* realloc_function */
    &ReallocImpl,          /* realloc_unchecked_function */
    &FreeImpl,             /* free_function */
    &GetSizeEstimateImpl,  /* get_size_estimate_function */
    &GoodSizeImpl,         /* good_size_function */
    &ClaimedAddressImpl,   /* claimed_address_function */
    &BatchMallocImpl,      /* batch_malloc_function */
    &BatchFreeImpl,        /* batch_free_function */
    &FreeDefiniteSizeImpl, /* free_definite_size_function */
    &TryFreeDefaultImpl,   /* try_free_default_function */
    nullptr,               /* aligned_malloc_function */
    nullptr,               /* aligned_malloc_unchecked_function */
    nullptr,               /* aligned_realloc_function */
    nullptr,               /* aligned_realloc_unchecked_function */
    nullptr,               /* aligned_free_function */
    nullptr,               /* next */
};

}  // namespace allocator_shim
