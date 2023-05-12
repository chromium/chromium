// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>

#include "base/allocator/partition_allocator/partition_alloc_base/compiler_specific.h"
#include "base/allocator/partition_allocator/partition_alloc_base/numerics/checked_math.h"
#include "base/allocator/partition_allocator/shim/allocator_shim.h"
#include "base/process/memory.h"

#include <dlfcn.h>
#include <malloc.h>

// This translation unit defines a default dispatch for the allocator shim which
// routes allocations to libc functions.
// The code here is strongly inspired from tcmalloc's libc_override_glibc.h.

extern "C" {
void* __libc_malloc(size_t size);
void* __libc_calloc(size_t n, size_t size);
void* __libc_realloc(void* address, size_t size);
void* __libc_memalign(size_t alignment, size_t size);
void __libc_free(void* ptr);
}  // extern "C"

namespace {

using allocator_shim::AllocatorDispatch;

// Strictly speaking, it would make more sense to not subtract amything, but
// other shims limit to something lower than INT_MAX (which is 0x7FFFFFFF on
// most platforms), and tests expect that.
constexpr size_t kMaxAllowedSize = std::numeric_limits<int>::max() - (1 << 12);

void* GlibcMalloc(const AllocatorDispatch*, size_t size, void* context) {
  // Cannot force glibc's malloc() to crash when a large size is requested, do
  // it in the shim instead.
  if (PA_UNLIKELY(size >= kMaxAllowedSize)) {
    base::TerminateBecauseOutOfMemory(size);
  }

  return __libc_malloc(size);
}

void* GlibcUncheckedMalloc(const AllocatorDispatch*,
                           size_t size,
                           void* context) {
  if (PA_UNLIKELY(size >= kMaxAllowedSize)) {
    return nullptr;
  }

  return __libc_malloc(size);
}

void* GlibcCalloc(const AllocatorDispatch*,
                  size_t n,
                  size_t size,
                  void* context) {
  const auto total = partition_alloc::internal::base::CheckMul(n, size);
  if (PA_UNLIKELY(!total.IsValid() || total.ValueOrDie() >= kMaxAllowedSize)) {
    base::TerminateBecauseOutOfMemory(size * n);
  }

  return __libc_calloc(n, size);
}

void* GlibcRealloc(const AllocatorDispatch*,
                   void* address,
                   size_t size,
                   void* context) {
  if (PA_UNLIKELY(size >= kMaxAllowedSize)) {
    base::TerminateBecauseOutOfMemory(size);
  }

  return __libc_realloc(address, size);
}

void* GlibcMemalign(const AllocatorDispatch*,
                    size_t alignment,
                    size_t size,
                    void* context) {
  if (PA_UNLIKELY(size >= kMaxAllowedSize)) {
    base::TerminateBecauseOutOfMemory(size);
  }

  return __libc_memalign(alignment, size);
}

void GlibcFree(const AllocatorDispatch*, void* address, void* context) {
  __libc_free(address);
}

PA_NO_SANITIZE("cfi-icall")
size_t GlibcGetSizeEstimate(const AllocatorDispatch*,
                            void* address,
                            void* context) {
  // glibc does not expose an alias to resolve malloc_usable_size. Dynamically
  // resolve it instead. This should be safe because glibc (and hence dlfcn)
  // does not use malloc_size internally and so there should not be a risk of
  // recursion.
  using MallocUsableSizeFunction = decltype(malloc_usable_size)*;
  static MallocUsableSizeFunction fn_ptr =
      reinterpret_cast<MallocUsableSizeFunction>(
          dlsym(RTLD_NEXT, "malloc_usable_size"));

  return fn_ptr(address);
}

}  // namespace

const AllocatorDispatch AllocatorDispatch::default_dispatch = {
    &GlibcMalloc,          /* alloc_function */
    &GlibcUncheckedMalloc, /* alloc_unchecked_function */
    &GlibcCalloc,          /* alloc_zero_initialized_function */
    &GlibcMemalign,        /* alloc_aligned_function */
    &GlibcRealloc,         /* realloc_function */
    &GlibcFree,            /* free_function */
    &GlibcGetSizeEstimate, /* get_size_estimate_function */
    nullptr,               /* claimed_address */
    nullptr,               /* batch_malloc_function */
    nullptr,               /* batch_free_function */
    nullptr,               /* free_definite_size_function */
    nullptr,               /* try_free_default_function */
    nullptr,               /* aligned_malloc_function */
    nullptr,               /* aligned_realloc_function */
    nullptr,               /* aligned_free_function */
    nullptr,               /* next */
};
