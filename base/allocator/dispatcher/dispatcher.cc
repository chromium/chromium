// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/dispatcher/dispatcher.h"

#include "base/allocator/buildflags.h"
#include "base/allocator/dispatcher/internal/dispatch_data.h"
#include "base/allocator/dispatcher/reentry_guard.h"
#include "base/allocator/partition_allocator/partition_alloc.h"
#include "base/allocator/partition_allocator/partition_alloc_buildflags.h"
#include "base/allocator/partition_allocator/shim/allocator_shim.h"
#include "base/check.h"
#include "base/dcheck_is_on.h"
#include "base/no_destructor.h"
#include "base/sampling_heap_profiler/poisson_allocation_sampler.h"

#if DCHECK_IS_ON()
#include <atomic>
#endif

#if BUILDFLAG(USE_ALLOCATOR_SHIM)
namespace base::allocator::dispatcher::allocator_shim_details {
namespace {

using allocator_shim::AllocatorDispatch;

void* AllocFn(const AllocatorDispatch* self, size_t size, void* context) {
  ReentryGuard guard;
  void* address = self->next->alloc_function(self->next, size, context);
  if (LIKELY(guard)) {
    PoissonAllocationSampler::RecordAlloc(
        address, size, PoissonAllocationSampler::kMalloc, nullptr);
  }
  return address;
}

void* AllocUncheckedFn(const AllocatorDispatch* self,
                       size_t size,
                       void* context) {
  ReentryGuard guard;
  void* address =
      self->next->alloc_unchecked_function(self->next, size, context);
  if (LIKELY(guard)) {
    PoissonAllocationSampler::RecordAlloc(
        address, size, PoissonAllocationSampler::kMalloc, nullptr);
  }
  return address;
}

void* AllocZeroInitializedFn(const AllocatorDispatch* self,
                             size_t n,
                             size_t size,
                             void* context) {
  ReentryGuard guard;
  void* address =
      self->next->alloc_zero_initialized_function(self->next, n, size, context);
  if (LIKELY(guard)) {
    PoissonAllocationSampler::RecordAlloc(
        address, n * size, PoissonAllocationSampler::kMalloc, nullptr);
  }
  return address;
}

void* AllocAlignedFn(const AllocatorDispatch* self,
                     size_t alignment,
                     size_t size,
                     void* context) {
  ReentryGuard guard;
  void* address =
      self->next->alloc_aligned_function(self->next, alignment, size, context);
  if (LIKELY(guard)) {
    PoissonAllocationSampler::RecordAlloc(
        address, size, PoissonAllocationSampler::kMalloc, nullptr);
  }
  return address;
}

void* ReallocFn(const AllocatorDispatch* self,
                void* address,
                size_t size,
                void* context) {
  ReentryGuard guard;
  // Note: size == 0 actually performs free.
  PoissonAllocationSampler::RecordFree(address);
  address = self->next->realloc_function(self->next, address, size, context);
  if (LIKELY(guard)) {
    PoissonAllocationSampler::RecordAlloc(
        address, size, PoissonAllocationSampler::kMalloc, nullptr);
  }
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
  ReentryGuard guard;
  unsigned num_allocated = self->next->batch_malloc_function(
      self->next, size, results, num_requested, context);
  if (LIKELY(guard)) {
    for (unsigned i = 0; i < num_allocated; ++i) {
      PoissonAllocationSampler::RecordAlloc(
          results[i], size, PoissonAllocationSampler::kMalloc, nullptr);
    }
  }
  return num_allocated;
}

void BatchFreeFn(const AllocatorDispatch* self,
                 void** to_be_freed,
                 unsigned num_to_be_freed,
                 void* context) {
  for (unsigned i = 0; i < num_to_be_freed; ++i)
    PoissonAllocationSampler::RecordFree(to_be_freed[i]);
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
  ReentryGuard guard;
  void* address =
      self->next->aligned_malloc_function(self->next, size, alignment, context);
  if (LIKELY(guard)) {
    PoissonAllocationSampler::RecordAlloc(
        address, size, PoissonAllocationSampler::kMalloc, nullptr);
  }
  return address;
}

static void* AlignedReallocFn(const AllocatorDispatch* self,
                              void* address,
                              size_t size,
                              size_t alignment,
                              void* context) {
  ReentryGuard guard;
  // Note: size == 0 actually performs free.
  PoissonAllocationSampler::RecordFree(address);
  address = self->next->aligned_realloc_function(self->next, address, size,
                                                 alignment, context);
  if (LIKELY(guard)) {
    PoissonAllocationSampler::RecordAlloc(
        address, size, PoissonAllocationSampler::kMalloc, nullptr);
  }
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

#if BUILDFLAG(USE_PARTITION_ALLOC) && !BUILDFLAG(IS_NACL)
namespace base::allocator::dispatcher::partition_allocator_details {
namespace {

void PartitionAllocHook(void* address, size_t size, const char* type) {
  PoissonAllocationSampler::RecordAlloc(
      address, size, PoissonAllocationSampler::kPartitionAlloc, type);
}

void PartitionFreeHook(void* address) {
  PoissonAllocationSampler::RecordFree(address);
}

}  // namespace
}  // namespace base::allocator::dispatcher::partition_allocator_details
#endif  // BUILDFLAG(USE_PARTITION_ALLOC) && !BUILDFLAG(IS_NACL)

namespace base::allocator::dispatcher {

void InstallStandardAllocatorHooks() {
#if BUILDFLAG(USE_ALLOCATOR_SHIM)
  allocator_shim::InsertAllocatorDispatch(
      &allocator_shim_details::g_allocator_dispatch);
#else
  // If the allocator shim isn't available, then we don't install any hooks.
  // There's no point in printing an error message, since this can regularly
  // happen for tests.
#endif  // BUILDFLAG(USE_ALLOCATOR_SHIM)

#if BUILDFLAG(USE_PARTITION_ALLOC) && !BUILDFLAG(IS_NACL)
  partition_alloc::PartitionAllocHooks::SetObserverHooks(
      &partition_allocator_details::PartitionAllocHook,
      &partition_allocator_details::PartitionFreeHook);
#endif  // BUILDFLAG(USE_PARTITION_ALLOC) && !BUILDFLAG(IS_NACL)
}

void RemoveStandardAllocatorHooksForTesting() {
#if BUILDFLAG(USE_ALLOCATOR_SHIM)
  allocator_shim::RemoveAllocatorDispatchForTesting(
      &allocator_shim_details::g_allocator_dispatch);  // IN-TEST
#endif
#if BUILDFLAG(USE_PARTITION_ALLOC) && !BUILDFLAG(IS_NACL)
  partition_alloc::PartitionAllocHooks::SetObserverHooks(nullptr, nullptr);
#endif
}

}  // namespace base::allocator::dispatcher

namespace base::allocator::dispatcher {

// The private implementation of Dispatcher.
struct Dispatcher::Impl {
  void Initialize(const internal::DispatchData& dispatch_data) {
#if DCHECK_IS_ON()
    DCHECK(!is_initialized_check_flag_.test_and_set());
#endif

    dispatch_data_ = dispatch_data;
    ConnectToEmitters(dispatch_data_);
  }

  void Reset() {
#if DCHECK_IS_ON()
    DCHECK([&]() {
      auto const was_set = is_initialized_check_flag_.test_and_set();
      is_initialized_check_flag_.clear();
      return was_set;
    }());
#endif

    DisconnectFromEmitters(dispatch_data_);
    dispatch_data_ = {};
  }

 private:
  // Connect the hooks to the memory subsystem. In some cases, most notably when
  // we have no observers at all, the hooks will be invalid and must NOT be
  // connected. This way we prevent notifications although no observers are
  // present.
  static void ConnectToEmitters(const internal::DispatchData& dispatch_data) {
#if BUILDFLAG(USE_ALLOCATOR_SHIM)
    if (auto* const allocator_dispatch = dispatch_data.GetAllocatorDispatch()) {
      allocator_shim::InsertAllocatorDispatch(allocator_dispatch);
    }
#endif

#if BUILDFLAG(USE_PARTITION_ALLOC)
    {
      auto* const allocation_hook = dispatch_data.GetAllocationObserverHook();
      auto* const free_hook = dispatch_data.GetFreeObserverHook();
      if (allocation_hook && free_hook) {
        partition_alloc::PartitionAllocHooks::SetObserverHooks(allocation_hook,
                                                               free_hook);
      }
    }
#endif
  }

  static void DisconnectFromEmitters(internal::DispatchData& dispatch_data) {
#if BUILDFLAG(USE_ALLOCATOR_SHIM)
    if (auto* const allocator_dispatch = dispatch_data.GetAllocatorDispatch()) {
      allocator_shim::RemoveAllocatorDispatchForTesting(
          allocator_dispatch);  // IN-TEST
    }
#endif

#if BUILDFLAG(USE_PARTITION_ALLOC)
    partition_alloc::PartitionAllocHooks::SetObserverHooks(nullptr, nullptr);
#endif
  }

  // Information on the hooks.
  internal::DispatchData dispatch_data_;
#if DCHECK_IS_ON()
  // Indicator if the dispatcher has been initialized before.
#if !defined(__cpp_lib_atomic_value_initialization) || \
    __cpp_lib_atomic_value_initialization < 201911L
  std::atomic_flag is_initialized_check_flag_ = ATOMIC_FLAG_INIT;
#else
  std::atomic_flag is_initialized_check_flag_;
#endif
#endif
};

Dispatcher::Dispatcher() : impl_(std::make_unique<Impl>()) {}

Dispatcher::~Dispatcher() = default;

Dispatcher& Dispatcher::GetInstance() {
  static base::NoDestructor<Dispatcher> instance;
  return *instance;
}

void Dispatcher::Initialize(const internal::DispatchData& dispatch_data) {
  impl_->Initialize(dispatch_data);
}

void Dispatcher::ResetForTesting() {
  impl_->Reset();
}
}  // namespace base::allocator::dispatcher
