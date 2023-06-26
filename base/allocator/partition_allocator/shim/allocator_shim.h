// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_SHIM_ALLOCATOR_SHIM_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_SHIM_ALLOCATOR_SHIM_H_

#include <stddef.h>
#include <stdint.h>

#include "base/allocator/partition_allocator/partition_alloc_base/types/strong_alias.h"
#include "base/allocator/partition_allocator/partition_alloc_buildflags.h"
#include "base/base_export.h"
#include "build/build_config.h"

#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) && BUILDFLAG(USE_STARSCAN)
#include "base/allocator/partition_allocator/starscan/pcscan.h"
#endif

namespace allocator_shim {

// Allocator Shim API. Allows to:
//  - Configure the behavior of the allocator (what to do on OOM failures).
//  - Install new hooks (AllocatorDispatch) in the allocator chain.

// When this shim layer is enabled, the route of an allocation is as-follows:
//
// [allocator_shim_override_*.h] Intercept malloc() / operator new calls:
//   The override_* headers define the symbols required to intercept calls to
//   malloc() and operator new (if not overridden by specific C++ classes).
//
// [allocator_shim.cc] Routing allocation calls to the shim:
//   The headers above route the calls to the internal ShimMalloc(), ShimFree(),
//   ShimCppNew() etc. methods defined in allocator_shim.cc.
//   These methods will: (1) forward the allocation call to the front of the
//   AllocatorDispatch chain. (2) perform security hardenings (e.g., might
//   call std::new_handler on OOM failure).
//
// [allocator_shim_default_dispatch_to_*.cc] The AllocatorDispatch chain:
//   It is a singly linked list where each element is a struct with function
//   pointers (|malloc_function|, |free_function|, etc). Normally the chain
//   consists of a single AllocatorDispatch element, herein called
//   the "default dispatch", which is statically defined at build time and
//   ultimately routes the calls to the actual allocator defined by the build
//   config (glibc, ...).
//
// It is possible to dynamically insert further AllocatorDispatch stages
// to the front of the chain, for debugging / profiling purposes.
//
// All the functions must be thread safe. The shim does not enforce any
// serialization. This is to route to thread-aware allocators without
// introducing unnecessary perf hits.

struct AllocatorDispatch {
  using AllocFn = void*(const AllocatorDispatch* self,
                        size_t size,
                        void* context);
  using AllocUncheckedFn = void*(const AllocatorDispatch* self,
                                 size_t size,
                                 void* context);
  using AllocZeroInitializedFn = void*(const AllocatorDispatch* self,
                                       size_t n,
                                       size_t size,
                                       void* context);
  using AllocAlignedFn = void*(const AllocatorDispatch* self,
                               size_t alignment,
                               size_t size,
                               void* context);
  using ReallocFn = void*(const AllocatorDispatch* self,
                          void* address,
                          size_t size,
                          void* context);
  using FreeFn = void(const AllocatorDispatch* self,
                      void* address,
                      void* context);
  // Returns the allocated size of user data (not including heap overhead).
  // Can be larger than the requested size.
  using GetSizeEstimateFn = size_t(const AllocatorDispatch* self,
                                   void* address,
                                   void* context);
  using ClaimedAddressFn = bool(const AllocatorDispatch* self,
                                void* address,
                                void* context);
  using BatchMallocFn = unsigned(const AllocatorDispatch* self,
                                 size_t size,
                                 void** results,
                                 unsigned num_requested,
                                 void* context);
  using BatchFreeFn = void(const AllocatorDispatch* self,
                           void** to_be_freed,
                           unsigned num_to_be_freed,
                           void* context);
  using FreeDefiniteSizeFn = void(const AllocatorDispatch* self,
                                  void* ptr,
                                  size_t size,
                                  void* context);
  using TryFreeDefaultFn = void(const AllocatorDispatch* self,
                                void* ptr,
                                void* context);
  using AlignedMallocFn = void*(const AllocatorDispatch* self,
                                size_t size,
                                size_t alignment,
                                void* context);
  using AlignedReallocFn = void*(const AllocatorDispatch* self,
                                 void* address,
                                 size_t size,
                                 size_t alignment,
                                 void* context);
  using AlignedFreeFn = void(const AllocatorDispatch* self,
                             void* address,
                             void* context);

  AllocFn* const alloc_function;
  AllocUncheckedFn* const alloc_unchecked_function;
  AllocZeroInitializedFn* const alloc_zero_initialized_function;
  AllocAlignedFn* const alloc_aligned_function;
  ReallocFn* const realloc_function;
  FreeFn* const free_function;
  GetSizeEstimateFn* const get_size_estimate_function;
  // claimed_address, batch_malloc, batch_free, free_definite_size and
  // try_free_default are specific to the OSX and iOS allocators.
  ClaimedAddressFn* const claimed_address_function;
  BatchMallocFn* const batch_malloc_function;
  BatchFreeFn* const batch_free_function;
  FreeDefiniteSizeFn* const free_definite_size_function;
  TryFreeDefaultFn* const try_free_default_function;
  // _aligned_malloc, _aligned_realloc, and _aligned_free are specific to the
  // Windows allocator.
  AlignedMallocFn* const aligned_malloc_function;
  AlignedReallocFn* const aligned_realloc_function;
  AlignedFreeFn* const aligned_free_function;

  const AllocatorDispatch* next;

  // |default_dispatch| is statically defined by one (and only one) of the
  // allocator_shim_default_dispatch_to_*.cc files, depending on the build
  // configuration.
  static const AllocatorDispatch default_dispatch;
};

// When true makes malloc behave like new, w.r.t calling the new_handler if
// the allocation fails (see set_new_mode() in Windows).
BASE_EXPORT void SetCallNewHandlerOnMallocFailure(bool value);

// Allocates |size| bytes or returns nullptr. It does NOT call the new_handler,
// regardless of SetCallNewHandlerOnMallocFailure().
BASE_EXPORT void* UncheckedAlloc(size_t size);

// Frees memory allocated with UncheckedAlloc().
BASE_EXPORT void UncheckedFree(void* ptr);

// Inserts |dispatch| in front of the allocator chain. This method is
// thread-safe w.r.t concurrent invocations of InsertAllocatorDispatch().
// The callers have responsibility for inserting a single dispatch no more
// than once.
BASE_EXPORT void InsertAllocatorDispatch(AllocatorDispatch* dispatch);

// Test-only. Rationale: (1) lack of use cases; (2) dealing safely with a
// removal of arbitrary elements from a singly linked list would require a lock
// in malloc(), which we really don't want.
BASE_EXPORT void RemoveAllocatorDispatchForTesting(AllocatorDispatch* dispatch);

#if BUILDFLAG(IS_APPLE)
// The fallback function to be called when try_free_default_function receives a
// pointer which doesn't belong to the allocator.
BASE_EXPORT void TryFreeDefaultFallbackToFindZoneAndFree(void* ptr);
#endif  // BUILDFLAG(IS_APPLE)

#if BUILDFLAG(IS_APPLE)
#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
BASE_EXPORT void InitializeDefaultAllocatorPartitionRoot();
bool IsDefaultAllocatorPartitionRootInitialized();
#endif  // BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
// On macOS, the allocator shim needs to be turned on during runtime.
BASE_EXPORT void InitializeAllocatorShim();
#endif  // BUILDFLAG(IS_APPLE)

#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
BASE_EXPORT void EnablePartitionAllocMemoryReclaimer();

using EnableBrp =
    partition_alloc::internal::base::StrongAlias<class EnableBrpTag, bool>;
using EnableBrpPartitionMemoryReclaimer = partition_alloc::internal::base::
    StrongAlias<class EnableBrpPartitionMemoryReclaimerTag, bool>;
using EnableMemoryTagging =
    partition_alloc::internal::base::StrongAlias<class EnableMemoryTaggingTag,
                                                 bool>;
using SplitMainPartition =
    partition_alloc::internal::base::StrongAlias<class SplitMainPartitionTag,
                                                 bool>;
using UseDedicatedAlignedPartition = partition_alloc::internal::base::
    StrongAlias<class UseDedicatedAlignedPartitionTag, bool>;
enum class AlternateBucketDistribution : uint8_t { kDefault, kDenser };

// If |thread_cache_on_non_quarantinable_partition| is specified, the
// thread-cache will be enabled on the non-quarantinable partition. The
// thread-cache on the main (malloc) partition will be disabled.
BASE_EXPORT void ConfigurePartitions(
    EnableBrp enable_brp,
    EnableBrpPartitionMemoryReclaimer enable_brp_memory_reclaimer,
    EnableMemoryTagging enable_memory_tagging,
    SplitMainPartition split_main_partition,
    UseDedicatedAlignedPartition use_dedicated_aligned_partition,
    size_t ref_count_size,
    AlternateBucketDistribution use_alternate_bucket_distribution);

BASE_EXPORT uint32_t GetMainPartitionRootExtrasSize();

#if BUILDFLAG(USE_STARSCAN)
BASE_EXPORT void EnablePCScan(partition_alloc::internal::PCScan::InitConfig);
#endif
#endif  // BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

}  // namespace allocator_shim

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_SHIM_ALLOCATOR_SHIM_H_
