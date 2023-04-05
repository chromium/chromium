// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/dispatcher/standard_hooks.h"

#include "base/allocator/buildflags.h"
#include "base/allocator/partition_allocator/partition_alloc.h"
#include "base/allocator/partition_allocator/partition_alloc_buildflags.h"
#include "base/allocator/partition_allocator/shim/allocator_shim.h"
#include "base/sampling_heap_profiler/poisson_allocation_sampler.h"

#if !BUILDFLAG(USE_ALLOCATION_EVENT_DISPATCHER)
#if BUILDFLAG(USE_ALLOCATOR_SHIM)
namespace base::allocator::dispatcher::allocator_shim_details {
namespace {

using allocator_shim::AllocatorDispatch;

void* AllocFn(const AllocatorDispatch* self, size_t size, void* context) {
  void* address = self->next->alloc_function(self->next, size, context);

  PoissonAllocationSampler::RecordAlloc(
      address, size, AllocationSubsystem::kAllocatorShim, nullptr);

  return address;
}

void* AllocUncheckedFn(const AllocatorDispatch* self,
                       size_t size,
                       void* context) {
  void* address =
      self->next->alloc_unchecked_function(self->next, size, context);

  PoissonAllocationSampler::RecordAlloc(
      address, size, AllocationSubsystem::kAllocatorShim, nullptr);

  return address;
}

void* AllocZeroInitializedFn(const AllocatorDispatch* self,
                             size_t n,
                             size_t size,
                             void* context) {
  void* address =
      self->next->alloc_zero_initialized_function(self->next, n, size, context);

  PoissonAllocationSampler::RecordAlloc(
      address, n * size, AllocationSubsystem::kAllocatorShim, nullptr);

  return address;
}

void* AllocAlignedFn(const AllocatorDispatch* self,
                     size_t alignment,
                     size_t size,
                     void* context) {
  void* address =
      self->next->alloc_aligned_function(self->next, alignment, size, context);

  PoissonAllocationSampler::RecordAlloc(
      address, size, AllocationSubsystem::kAllocatorShim, nullptr);

  return address;
}

void* ReallocFn(const AllocatorDispatch* self,
                void* address,
                size_t size,
                void* context) {
  // Note: size == 0 actually performs free.
  PoissonAllocationSampler::RecordFree(address);
  address = self->next->realloc_function(self->next, address, size, context);

  PoissonAllocationSampler::RecordAlloc(
      address, size, AllocationSubsystem::kAllocatorShim, nullptr);

  return address;
}

void FreeFn(const AllocatorDispatch* self, void* address, void* context) {
  // Note: The RecordFree should be called before free_function
  // (here and in other places).
  // That is because we need to remove the recorded allocation sample before
  // free_function, as once the latter is executed the address becomes available
  // and can be allocated by another thread. That would be racy otherwise.
  PoissonAllocationSampler::RecordFree(address);
  self->next->free_function(self->next, address, context);
}

size_t GetSizeEstimateFn(const AllocatorDispatch* self,
                         void* address,
                         void* context) {
  return self->next->get_size_estimate_function(self->next, address, context);
}

bool ClaimedAddressFn(const AllocatorDispatch* self,
                      void* address,
                      void* context) {
  return self->next->claimed_address_function(self->next, address, context);
}

unsigned BatchMallocFn(const AllocatorDispatch* self,
                       size_t size,
                       void** results,
                       unsigned num_requested,
                       void* context) {
  unsigned num_allocated = self->next->batch_malloc_function(
      self->next, size, results, num_requested, context);

  for (unsigned i = 0; i < num_allocated; ++i) {
    PoissonAllocationSampler::RecordAlloc(
        results[i], size, AllocationSubsystem::kAllocatorShim, nullptr);
  }

  return num_allocated;
}

void BatchFreeFn(const AllocatorDispatch* self,
                 void** to_be_freed,
                 unsigned num_to_be_freed,
                 void* context) {
  for (unsigned i = 0; i < num_to_be_freed; ++i) {
    PoissonAllocationSampler::RecordFree(to_be_freed[i]);
  }

  self->next->batch_free_function(self->next, to_be_freed, num_to_be_freed,
                                  context);
}

void FreeDefiniteSizeFn(const AllocatorDispatch* self,
                        void* address,
                        size_t size,
                        void* context) {
  PoissonAllocationSampler::RecordFree(address);
  self->next->free_definite_size_function(self->next, address, size, context);
}

void TryFreeDefaultFn(const AllocatorDispatch* self,
                      void* address,
                      void* context) {
  PoissonAllocationSampler::RecordFree(address);
  self->next->try_free_default_function(self->next, address, context);
}

static void* AlignedMallocFn(const AllocatorDispatch* self,
                             size_t size,
                             size_t alignment,
                             void* context) {
  void* address =
      self->next->aligned_malloc_function(self->next, size, alignment, context);

  PoissonAllocationSampler::RecordAlloc(
      address, size, AllocationSubsystem::kAllocatorShim, nullptr);

  return address;
}

static void* AlignedReallocFn(const AllocatorDispatch* self,
                              void* address,
                              size_t size,
                              size_t alignment,
                              void* context) {
  // Note: size == 0 actually performs free.
  PoissonAllocationSampler::RecordFree(address);
  address = self->next->aligned_realloc_function(self->next, address, size,
                                                 alignment, context);

  PoissonAllocationSampler::RecordAlloc(
      address, size, AllocationSubsystem::kAllocatorShim, nullptr);

  return address;
}

static void AlignedFreeFn(const AllocatorDispatch* self,
                          void* address,
                          void* context) {
  PoissonAllocationSampler::RecordFree(address);
  self->next->aligned_free_function(self->next, address, context);
}

AllocatorDispatch g_allocator_dispatch = {&AllocFn,
                                          &AllocUncheckedFn,
                                          &AllocZeroInitializedFn,
                                          &AllocAlignedFn,
                                          &ReallocFn,
                                          &FreeFn,
                                          &GetSizeEstimateFn,
                                          &ClaimedAddressFn,
                                          &BatchMallocFn,
                                          &BatchFreeFn,
                                          &FreeDefiniteSizeFn,
                                          &TryFreeDefaultFn,
                                          &AlignedMallocFn,
                                          &AlignedReallocFn,
                                          &AlignedFreeFn,
                                          nullptr};

}  // namespace
}  // namespace base::allocator::dispatcher::allocator_shim_details
#endif  // BUILDFLAG(USE_ALLOCATOR_SHIM)

#if BUILDFLAG(USE_PARTITION_ALLOC)
namespace base::allocator::dispatcher::partition_allocator_details {
namespace {

void PartitionAllocHook(void* address, size_t size, const char* type) {
  PoissonAllocationSampler::RecordAlloc(
      address, size, AllocationSubsystem::kPartitionAllocator, type);
}

void PartitionFreeHook(void* address) {
  PoissonAllocationSampler::RecordFree(address);
}

}  // namespace
}  // namespace base::allocator::dispatcher::partition_allocator_details
#endif  // BUILDFLAG(USE_PARTITION_ALLOC)
#endif  // !BUILDFLAG(USE_ALLOCATION_EVENT_DISPATCHER)

namespace base::allocator::dispatcher {

#if !BUILDFLAG(USE_ALLOCATION_EVENT_DISPATCHER)
void InstallStandardAllocatorHooks() {
#if BUILDFLAG(USE_ALLOCATOR_SHIM)
  allocator_shim::InsertAllocatorDispatch(
      &allocator_shim_details::g_allocator_dispatch);
#else
  // If the allocator shim isn't available, then we don't install any hooks.
  // There's no point in printing an error message, since this can regularly
  // happen for tests.
#endif  // BUILDFLAG(USE_ALLOCATOR_SHIM)

#if BUILDFLAG(USE_PARTITION_ALLOC)
  partition_alloc::PartitionAllocHooks::SetObserverHooks(
      &partition_allocator_details::PartitionAllocHook,
      &partition_allocator_details::PartitionFreeHook);
#endif  // BUILDFLAG(USE_PARTITION_ALLOC)
}
#endif  // !BUILDFLAG(USE_ALLOCATION_EVENT_DISPATCHER)

}  // namespace base::allocator::dispatcher
