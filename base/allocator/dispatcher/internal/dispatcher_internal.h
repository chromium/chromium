// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_DISPATCHER_INTERNAL_DISPATCHER_INTERNAL_H_
#define BASE_ALLOCATOR_DISPATCHER_INTERNAL_DISPATCHER_INTERNAL_H_

#include "base/allocator/buildflags.h"
#include "base/allocator/dispatcher/configuration.h"
#include "base/allocator/dispatcher/internal/dispatch_data.h"
#include "base/allocator/dispatcher/internal/tools.h"
#include "base/allocator/dispatcher/subsystem.h"
#include "base/allocator/partition_allocator/partition_alloc_buildflags.h"
#include "base/check.h"
#include "base/compiler_specific.h"
#include "build/build_config.h"

#if BUILDFLAG(USE_PARTITION_ALLOC)
#include "base/allocator/partition_allocator/partition_alloc.h"
#endif

#if BUILDFLAG(USE_ALLOCATOR_SHIM)
#include "base/allocator/partition_allocator/shim/allocator_shim.h"
#endif

#include <tuple>

namespace base::allocator::dispatcher::internal {

#if BUILDFLAG(USE_ALLOCATOR_SHIM)
using allocator_shim::AllocatorDispatch;
#endif

template <typename CheckObserverPredicate,
          typename... ObserverTypes,
          size_t... Indices>
void inline PerformObserverCheck(const std::tuple<ObserverTypes...>& observers,
                                 std::index_sequence<Indices...>,
                                 CheckObserverPredicate check_observer) {
  ([](bool b) { DCHECK(b); }(check_observer(std::get<Indices>(observers))),
   ...);
}

template <typename... ObserverTypes, size_t... Indices>
ALWAYS_INLINE void PerformAllocationNotification(
    const std::tuple<ObserverTypes...>& observers,
    std::index_sequence<Indices...>,
    void* address,
    size_t size,
    AllocationSubsystem subSystem,
    const char* type_name) {
  ((std::get<Indices>(observers)->OnAllocation(address, size, subSystem,
                                               type_name)),
   ...);
}

template <typename... ObserverTypes, size_t... Indices>
ALWAYS_INLINE void PerformFreeNotification(
    const std::tuple<ObserverTypes...>& observers,
    std::index_sequence<Indices...>,
    void* address) {
  ((std::get<Indices>(observers)->OnFree(address)), ...);
}

// DispatcherImpl provides hooks into the various memory subsystems. These hooks
// are responsible for dispatching any notification to the observers.
// In order to provide as many information on the exact type of the observer and
// prevent any conditional jumps in the hot allocation path, observers are
// stored in a std::tuple. DispatcherImpl performs a CHECK at initialization
// time to ensure they are valid.
template <typename... ObserverTypes>
struct DispatcherImpl {
  using AllObservers = std::index_sequence_for<ObserverTypes...>;

  template <std::enable_if_t<
                internal::LessEqual(sizeof...(ObserverTypes),
                                    configuration::kMaximumNumberOfObservers),
                bool> = true>
  static DispatchData GetNotificationHooks(
      std::tuple<ObserverTypes*...> observers) {
    s_observers = std::move(observers);

    PerformObserverCheck(s_observers, AllObservers{}, IsValidObserver{});

    return CreateDispatchData();
  }

 private:
  static DispatchData CreateDispatchData() {
    return DispatchData()
#if BUILDFLAG(USE_PARTITION_ALLOC)
        .SetAllocationObserverHooks(&PartitionAllocatorAllocationHook,
                                    &PartitionAllocatorFreeHook)
#endif
#if BUILDFLAG(USE_ALLOCATOR_SHIM)
        .SetAllocatorDispatch(&allocator_dispatch_)
#endif
        ;
  }

#if BUILDFLAG(USE_PARTITION_ALLOC)
  static void PartitionAllocatorAllocationHook(void* address,
                                               size_t size,
                                               const char* type_name) {
    DoNotifyAllocation(address, size, AllocationSubsystem::kPartitionAllocator,
                       type_name);
  }

  static void PartitionAllocatorFreeHook(void* address) {
    DoNotifyFree(address);
  }
#endif

#if BUILDFLAG(USE_ALLOCATOR_SHIM)
  static void* AllocFn(const AllocatorDispatch* self,
                       size_t size,
                       void* context) {
    void* const address = self->next->alloc_function(self->next, size, context);

    DoNotifyAllocation(address, size, AllocationSubsystem::kAllocatorShim);

    return address;
  }

  static void* AllocUncheckedFn(const AllocatorDispatch* self,
                                size_t size,
                                void* context) {
    void* const address =
        self->next->alloc_unchecked_function(self->next, size, context);

    DoNotifyAllocation(address, size, AllocationSubsystem::kAllocatorShim);

    return address;
  }

  static void* AllocZeroInitializedFn(const AllocatorDispatch* self,
                                      size_t n,
                                      size_t size,
                                      void* context) {
    void* const address = self->next->alloc_zero_initialized_function(
        self->next, n, size, context);

    DoNotifyAllocation(address, n * size, AllocationSubsystem::kAllocatorShim);

    return address;
  }

  static void* AllocAlignedFn(const AllocatorDispatch* self,
                              size_t alignment,
                              size_t size,
                              void* context) {
    void* const address = self->next->alloc_aligned_function(
        self->next, alignment, size, context);

    DoNotifyAllocation(address, size, AllocationSubsystem::kAllocatorShim);

    return address;
  }

  static void* ReallocFn(const AllocatorDispatch* self,
                         void* address,
                         size_t size,
                         void* context) {
    // Note: size == 0 actually performs free.
    DoNotifyFree(address);
    void* const reallocated_address =
        self->next->realloc_function(self->next, address, size, context);

    DoNotifyAllocation(reallocated_address, size,
                       AllocationSubsystem::kAllocatorShim);

    return reallocated_address;
  }

  static void FreeFn(const AllocatorDispatch* self,
                     void* address,
                     void* context) {
    // Note: The RecordFree should be called before free_function (here and in
    // other places). That is because observers need to handle the allocation
    // being freed before calling free_function, as once the latter is executed
    // the address becomes available and can be allocated by another thread.
    // That would be racy otherwise.
    DoNotifyFree(address);
    self->next->free_function(self->next, address, context);
  }

  static size_t GetSizeEstimateFn(const AllocatorDispatch* self,
                                  void* address,
                                  void* context) {
    return self->next->get_size_estimate_function(self->next, address, context);
  }

  static bool ClaimedAddressFn(const AllocatorDispatch* self,
                               void* address,
                               void* context) {
    return self->next->claimed_address_function(self->next, address, context);
  }

  static unsigned BatchMallocFn(const AllocatorDispatch* self,
                                size_t size,
                                void** results,
                                unsigned num_requested,
                                void* context) {
    unsigned const num_allocated = self->next->batch_malloc_function(
        self->next, size, results, num_requested, context);
    for (unsigned i = 0; i < num_allocated; ++i) {
      DoNotifyAllocation(results[i], size, AllocationSubsystem::kAllocatorShim);
    }
    return num_allocated;
  }

  static void BatchFreeFn(const AllocatorDispatch* self,
                          void** to_be_freed,
                          unsigned num_to_be_freed,
                          void* context) {
    // Note: The code doesn't need to protect from recursions using
    // ReentryGuard, see ReallocFn for details.
    for (unsigned i = 0; i < num_to_be_freed; ++i) {
      DoNotifyFree(to_be_freed[i]);
    }
    self->next->batch_free_function(self->next, to_be_freed, num_to_be_freed,
                                    context);
  }

  static void FreeDefiniteSizeFn(const AllocatorDispatch* self,
                                 void* address,
                                 size_t size,
                                 void* context) {
    DoNotifyFree(address);
    self->next->free_definite_size_function(self->next, address, size, context);
  }

  static void TryFreeDefaultFn(const AllocatorDispatch* self,
                               void* address,
                               void* context) {
    DoNotifyFree(address);
    self->next->try_free_default_function(self->next, address, context);
  }

  static void* AlignedMallocFn(const AllocatorDispatch* self,
                               size_t size,
                               size_t alignment,
                               void* context) {
    void* const address = self->next->aligned_malloc_function(
        self->next, size, alignment, context);

    DoNotifyAllocation(address, size, AllocationSubsystem::kAllocatorShim);

    return address;
  }

  static void* AlignedReallocFn(const AllocatorDispatch* self,
                                void* address,
                                size_t size,
                                size_t alignment,
                                void* context) {
    // Note: size == 0 actually performs free.
    DoNotifyFree(address);
    address = self->next->aligned_realloc_function(self->next, address, size,
                                                   alignment, context);

    DoNotifyAllocation(address, size, AllocationSubsystem::kAllocatorShim);

    return address;
  }

  static void AlignedFreeFn(const AllocatorDispatch* self,
                            void* address,
                            void* context) {
    DoNotifyFree(address);
    self->next->aligned_free_function(self->next, address, context);
  }

  static AllocatorDispatch allocator_dispatch_;
#endif

  static ALWAYS_INLINE void DoNotifyAllocation(
      void* address,
      size_t size,
      AllocationSubsystem subSystem,
      const char* type_name = nullptr) {
    PerformAllocationNotification(s_observers, AllObservers{}, address, size,
                                  subSystem, type_name);
  }

  static ALWAYS_INLINE void DoNotifyFree(void* address) {
    PerformFreeNotification(s_observers, AllObservers{}, address);
  }

  static std::tuple<ObserverTypes*...> s_observers;
};

template <typename... ObserverTypes>
std::tuple<ObserverTypes*...> DispatcherImpl<ObserverTypes...>::s_observers;

#if BUILDFLAG(USE_ALLOCATOR_SHIM)
template <typename... ObserverTypes>
AllocatorDispatch DispatcherImpl<ObserverTypes...>::allocator_dispatch_ = {
    &AllocFn,
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
#endif

// Specialization of DispatcherImpl in case we have no observers to notify. In
// this special case we return a set of null pointers as the Dispatcher must not
// install any hooks at all.
template <>
struct DispatcherImpl<> {
  static DispatchData GetNotificationHooks(std::tuple<> /*observers*/) {
    return DispatchData()
#if BUILDFLAG(USE_PARTITION_ALLOC)
        .SetAllocationObserverHooks(nullptr, nullptr)
#endif
#if BUILDFLAG(USE_ALLOCATOR_SHIM)
        .SetAllocatorDispatch(nullptr)
#endif
        ;
  }
};

// A little utility function that helps using DispatcherImpl by providing
// automated type deduction for templates.
template <typename... ObserverTypes>
inline DispatchData GetNotificationHooks(
    std::tuple<ObserverTypes*...> observers) {
  return DispatcherImpl<ObserverTypes...>::GetNotificationHooks(
      std::move(observers));
}

}  // namespace base::allocator::dispatcher::internal

#endif  // BASE_ALLOCATOR_DISPATCHER_INTERNAL_DISPATCHER_INTERNAL_H_
