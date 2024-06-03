// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef PARTITION_ALLOC_SHIM_SHIM_ALLOC_FUNCTIONS_H_
#error This header is meant to be included only once by allocator_shim*.cc
#endif

#ifndef PARTITION_ALLOC_SHIM_SHIM_ALLOC_FUNCTIONS_H_
#define PARTITION_ALLOC_SHIM_SHIM_ALLOC_FUNCTIONS_H_

#include <bit>
#include <cerrno>

#include "partition_alloc/build_config.h"
#include "partition_alloc/partition_alloc_base/bits.h"
#include "partition_alloc/partition_alloc_base/compiler_specific.h"
#include "partition_alloc/partition_alloc_base/memory/page_size.h"
#include "partition_alloc/partition_alloc_buildflags.h"
#include "partition_alloc/partition_alloc_check.h"

namespace {

PA_ALWAYS_INLINE size_t GetCachedPageSize() {
  static size_t pagesize = 0;
  if (!pagesize) {
    pagesize = partition_alloc::internal::base::GetPageSize();
  }
  return pagesize;
}

}  // namespace

// The Shim* functions below are the entry-points into the shim-layer and
// are supposed to be invoked by the allocator_shim_override_*
// headers to route the malloc / new symbols through the shim layer.
// They are defined as ALWAYS_INLINE in order to remove a level of indirection
// between the system-defined entry points and the shim implementations.
extern "C" {

// The general pattern for allocations is:
// - Try to allocate, if succeeded return the pointer.
// - If the allocation failed:
//   - Call the std::new_handler if it was a C++ allocation.
//   - Call the std::new_handler if it was a malloc() (or calloc() or similar)
//     AND Setallocator_shim::internal::CallNewHandlerOnMallocFailure(true).
//   - If the std::new_handler is NOT set just return nullptr.
//   - If the std::new_handler is set:
//     - Assume it will abort() if it fails (very likely the new_handler will
//       just suicide printing a message).
//     - Assume it did succeed if it returns, in which case reattempt the alloc.

PA_ALWAYS_INLINE void* ShimCppNew(size_t size) {
  const allocator_shim::AllocatorDispatch* const chain_head =
      allocator_shim::internal::GetChainHead();
  void* ptr;
  do {
    void* context = nullptr;
#if PA_BUILDFLAG(IS_APPLE) && !PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
    context = malloc_default_zone();
#endif
    ptr = chain_head->alloc_function(chain_head, size, context);
  } while (!ptr && allocator_shim::internal::CallNewHandler(size));
  return ptr;
}

PA_ALWAYS_INLINE void* ShimCppNewNoThrow(size_t size) {
  void* context = nullptr;
#if PA_BUILDFLAG(IS_APPLE) && !PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  context = malloc_default_zone();
#endif
  const allocator_shim::AllocatorDispatch* const chain_head =
      allocator_shim::internal::GetChainHead();
  return chain_head->alloc_unchecked_function(chain_head, size, context);
}

PA_ALWAYS_INLINE void* ShimCppAlignedNew(size_t size, size_t alignment) {
  const allocator_shim::AllocatorDispatch* const chain_head =
      allocator_shim::internal::GetChainHead();
  void* ptr;
  do {
    void* context = nullptr;
#if PA_BUILDFLAG(IS_APPLE) && !PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
    context = malloc_default_zone();
#endif
    ptr = chain_head->alloc_aligned_function(chain_head, alignment, size,
                                             context);
  } while (!ptr && allocator_shim::internal::CallNewHandler(size));
  return ptr;
}

PA_ALWAYS_INLINE void ShimCppDelete(void* address) {
  void* context = nullptr;
#if PA_BUILDFLAG(IS_APPLE) && !PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  context = malloc_default_zone();
#endif
  const allocator_shim::AllocatorDispatch* const chain_head =
      allocator_shim::internal::GetChainHead();
  return chain_head->free_function(chain_head, address, context);
}

PA_ALWAYS_INLINE void* ShimMalloc(size_t size, void* context) {
  const allocator_shim::AllocatorDispatch* const chain_head =
      allocator_shim::internal::GetChainHead();
  void* ptr;
  do {
    ptr = chain_head->alloc_function(chain_head, size, context);
  } while (!ptr &&
           allocator_shim::internal::g_call_new_handler_on_malloc_failure &&
           allocator_shim::internal::CallNewHandler(size));
  return ptr;
}

PA_ALWAYS_INLINE void* ShimCalloc(size_t n, size_t size, void* context) {
  const allocator_shim::AllocatorDispatch* const chain_head =
      allocator_shim::internal::GetChainHead();
  void* ptr;
  do {
    ptr = chain_head->alloc_zero_initialized_function(chain_head, n, size,
                                                      context);
  } while (!ptr &&
           allocator_shim::internal::g_call_new_handler_on_malloc_failure &&
           allocator_shim::internal::CallNewHandler(size));
  return ptr;
}

PA_ALWAYS_INLINE void* ShimRealloc(void* address, size_t size, void* context) {
  // realloc(size == 0) means free() and might return a nullptr. We should
  // not call the std::new_handler in that case, though.
  const allocator_shim::AllocatorDispatch* const chain_head =
      allocator_shim::internal::GetChainHead();
  void* ptr;
  do {
    ptr = chain_head->realloc_function(chain_head, address, size, context);
  } while (!ptr && size &&
           allocator_shim::internal::g_call_new_handler_on_malloc_failure &&
           allocator_shim::internal::CallNewHandler(size));
  return ptr;
}

PA_ALWAYS_INLINE void* ShimMemalign(size_t alignment,
                                    size_t size,
                                    void* context) {
  const allocator_shim::AllocatorDispatch* const chain_head =
      allocator_shim::internal::GetChainHead();
  void* ptr;
  do {
    ptr = chain_head->alloc_aligned_function(chain_head, alignment, size,
                                             context);
  } while (!ptr &&
           allocator_shim::internal::g_call_new_handler_on_malloc_failure &&
           allocator_shim::internal::CallNewHandler(size));
  return ptr;
}

PA_ALWAYS_INLINE int ShimPosixMemalign(void** res,
                                       size_t alignment,
                                       size_t size) {
  // posix_memalign is supposed to check the arguments. See tc_posix_memalign()
  // in tc_malloc.cc.
  if (((alignment % sizeof(void*)) != 0) || !std::has_single_bit(alignment)) {
    return EINVAL;
  }
  void* ptr = ShimMemalign(alignment, size, nullptr);
  *res = ptr;
  return ptr ? 0 : ENOMEM;
}

PA_ALWAYS_INLINE void* ShimValloc(size_t size, void* context) {
  return ShimMemalign(GetCachedPageSize(), size, context);
}

PA_ALWAYS_INLINE void* ShimPvalloc(size_t size) {
  // pvalloc(0) should allocate one page, according to its man page.
  if (size == 0) {
    size = GetCachedPageSize();
  } else {
    size = partition_alloc::internal::base::bits::AlignUp(size,
                                                          GetCachedPageSize());
  }
  // The third argument is nullptr because pvalloc is glibc only and does not
  // exist on OSX/BSD systems.
  return ShimMemalign(GetCachedPageSize(), size, nullptr);
}

PA_ALWAYS_INLINE void ShimFree(void* address, void* context) {
  const allocator_shim::AllocatorDispatch* const chain_head =
      allocator_shim::internal::GetChainHead();
  return chain_head->free_function(chain_head, address, context);
}

PA_ALWAYS_INLINE size_t ShimGetSizeEstimate(const void* address,
                                            void* context) {
  const allocator_shim::AllocatorDispatch* const chain_head =
      allocator_shim::internal::GetChainHead();
  return chain_head->get_size_estimate_function(
      chain_head, const_cast<void*>(address), context);
}

PA_ALWAYS_INLINE size_t ShimGoodSize(size_t size, void* context) {
  const allocator_shim::AllocatorDispatch* const chain_head =
      allocator_shim::internal::GetChainHead();
  return chain_head->good_size_function(chain_head, size, context);
}

PA_ALWAYS_INLINE bool ShimClaimedAddress(void* address, void* context) {
  const allocator_shim::AllocatorDispatch* const chain_head =
      allocator_shim::internal::GetChainHead();
  return chain_head->claimed_address_function(chain_head, address, context);
}

PA_ALWAYS_INLINE unsigned ShimBatchMalloc(size_t size,
                                          void** results,
                                          unsigned num_requested,
                                          void* context) {
  const allocator_shim::AllocatorDispatch* const chain_head =
      allocator_shim::internal::GetChainHead();
  return chain_head->batch_malloc_function(chain_head, size, results,
                                           num_requested, context);
}

PA_ALWAYS_INLINE void ShimBatchFree(void** to_be_freed,
                                    unsigned num_to_be_freed,
                                    void* context) {
  const allocator_shim::AllocatorDispatch* const chain_head =
      allocator_shim::internal::GetChainHead();
  return chain_head->batch_free_function(chain_head, to_be_freed,
                                         num_to_be_freed, context);
}

PA_ALWAYS_INLINE void ShimFreeDefiniteSize(void* ptr,
                                           size_t size,
                                           void* context) {
  const allocator_shim::AllocatorDispatch* const chain_head =
      allocator_shim::internal::GetChainHead();
  return chain_head->free_definite_size_function(chain_head, ptr, size,
                                                 context);
}

PA_ALWAYS_INLINE void ShimTryFreeDefault(void* ptr, void* context) {
  const allocator_shim::AllocatorDispatch* const chain_head =
      allocator_shim::internal::GetChainHead();
  return chain_head->try_free_default_function(chain_head, ptr, context);
}

PA_ALWAYS_INLINE void* ShimAlignedMalloc(size_t size,
                                         size_t alignment,
                                         void* context) {
  const allocator_shim::AllocatorDispatch* const chain_head =
      allocator_shim::internal::GetChainHead();
  void* ptr = nullptr;
  do {
    ptr = chain_head->aligned_malloc_function(chain_head, size, alignment,
                                              context);
  } while (!ptr &&
           allocator_shim::internal::g_call_new_handler_on_malloc_failure &&
           allocator_shim::internal::CallNewHandler(size));
  return ptr;
}

PA_ALWAYS_INLINE void* ShimAlignedRealloc(void* address,
                                          size_t size,
                                          size_t alignment,
                                          void* context) {
  // _aligned_realloc(size == 0) means _aligned_free() and might return a
  // nullptr. We should not call the std::new_handler in that case, though.
  const allocator_shim::AllocatorDispatch* const chain_head =
      allocator_shim::internal::GetChainHead();
  void* ptr = nullptr;
  do {
    ptr = chain_head->aligned_realloc_function(chain_head, address, size,
                                               alignment, context);
  } while (!ptr && size &&
           allocator_shim::internal::g_call_new_handler_on_malloc_failure &&
           allocator_shim::internal::CallNewHandler(size));
  return ptr;
}

PA_ALWAYS_INLINE void ShimAlignedFree(void* address, void* context) {
  const allocator_shim::AllocatorDispatch* const chain_head =
      allocator_shim::internal::GetChainHead();
  return chain_head->aligned_free_function(chain_head, address, context);
}

#undef PA_ALWAYS_INLINE

}  // extern "C"

#endif  // PARTITION_ALLOC_SHIM_SHIM_ALLOC_FUNCTIONS_H_
