// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <malloc.h>

#include "partition_alloc/build_config.h"
#include "partition_alloc/shim/allocator_shim.h"

// This translation unit defines a default dispatch for the allocator shim which
// routes allocations to the original libc functions when using the link-time
// -Wl,-wrap,malloc approach (see README.md).
// The __real_X functions here are special symbols that the linker will relocate
// against the real "X" undefined symbol, so that __real_malloc becomes the
// equivalent of what an undefined malloc symbol reference would have been.
// This is the counterpart of allocator_shim_override_linker_wrapped_symbols.h,
// which routes the __wrap_X functions into the shim.

extern "C" {
void* __real_malloc(size_t);
void* __real_calloc(size_t, size_t);
void* __real_realloc(void*, size_t);
void* __real_memalign(size_t, size_t);
void __real_free(void*);
size_t __real_malloc_usable_size(void*);
}  // extern "C"

namespace {

using allocator_shim::AllocatorDispatch;

void* RealMalloc(size_t size, void* context) {
  return __real_malloc(size);
}

void* RealCalloc(size_t n, size_t size, void* context) {
  return __real_calloc(n, size);
}

void* RealRealloc(void* address, size_t size, void* context) {
  return __real_realloc(address, size);
}

void* RealMemalign(size_t alignment, size_t size, void* context) {
  return __real_memalign(alignment, size);
}

void RealFree(void* address, void* context) {
  __real_free(address);
}

size_t RealSizeEstimate(void* address, void* context) {
  return __real_malloc_usable_size(address);
}

}  // namespace

const AllocatorDispatch AllocatorDispatch::default_dispatch = {
    &RealMalloc,       /* alloc_function */
    &RealMalloc,       /* alloc_unchecked_function */
    &RealCalloc,       /* alloc_zero_initialized_function */
    &RealMemalign,     /* alloc_aligned_function */
    &RealRealloc,      /* realloc_function */
    &RealRealloc,      /* realloc_unchecked_function */
    &RealFree,         /* free_function */
    &RealSizeEstimate, /* get_size_estimate_function */
    nullptr,           /* good_size_function */
    nullptr,           /* claimed_address */
    nullptr,           /* batch_malloc_function */
    nullptr,           /* batch_free_function */
    nullptr,           /* free_definite_size_function */
    nullptr,           /* try_free_default_function */
    nullptr,           /* aligned_malloc_function */
    nullptr,           /* aligned_malloc_unchecked_function */
    nullptr,           /* aligned_realloc_function */
    nullptr,           /* aligned_realloc_unchecked_function */
    nullptr,           /* aligned_free_function */
    nullptr,           /* next */
};
