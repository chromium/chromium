// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/dispatcher/dispatcher.h"

#include "base/allocator/dispatcher/internal/dispatch_data.h"
#include "base/check.h"
#include "base/dcheck_is_on.h"
#include "base/no_destructor.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "partition_alloc/buildflags.h"
#include "partition_alloc/shim/allocator_shim.h"

#if DCHECK_IS_ON()
#include <atomic>
#endif

#if PA_BUILDFLAG(USE_PARTITION_ALLOC)
#include "partition_alloc/partition_alloc_hooks.h"  // nogncheck
#endif

namespace base::allocator::dispatcher {

// The private implementation of Dispatcher.
struct Dispatcher::Impl {
  void Initialize(const internal::DispatchData& dispatch_data,
                  bool (*should_defer_connection)()) {
#if DCHECK_IS_ON()
    DCHECK(!is_initialized_check_flag_.test_and_set());
#endif

    base::AutoLock lock(lock_);
    dispatch_data_ = dispatch_data;
    // Observers update the state checked by `should_defer_connection` before
    // calling EnsureConnected(). Checking it under `lock_` guarantees that an
    // observer activated concurrently is either seen as active here, or
    // connects the hooks in EnsureConnected() once `lock_` is released.
    connection_deferred_ = should_defer_connection();
    if (!connection_deferred_) {
      ConnectToEmitters(dispatch_data_);
    }
  }

  void EnsureConnected() {
    base::AutoLock lock(lock_);
    if (connection_deferred_) {
      ConnectToEmitters(dispatch_data_);
      connection_deferred_ = false;
    }
  }

  void Reset() {
#if DCHECK_IS_ON()
    DCHECK([&] {
      auto const was_set = is_initialized_check_flag_.test_and_set();
      is_initialized_check_flag_.clear();
      return was_set;
    }());
#endif

    base::AutoLock lock(lock_);
    if (!connection_deferred_) {
      DisconnectFromEmitters(dispatch_data_);
    }
    connection_deferred_ = false;
    dispatch_data_ = {};
  }

 private:
  // Connect the hooks to the memory subsystem. In some cases, most notably when
  // we have no observers at all, the hooks will be invalid and must NOT be
  // connected. This way we prevent notifications although no observers are
  // present.
  static void ConnectToEmitters(const internal::DispatchData& dispatch_data) {
#if PA_BUILDFLAG(USE_ALLOCATOR_SHIM)
    if (auto* const allocator_dispatch = dispatch_data.GetAllocatorDispatch()) {
      allocator_shim::InsertAllocatorDispatch(allocator_dispatch);
    }
#endif

#if PA_BUILDFLAG(USE_PARTITION_ALLOC)
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
#if PA_BUILDFLAG(USE_ALLOCATOR_SHIM)
    if (auto* const allocator_dispatch = dispatch_data.GetAllocatorDispatch()) {
      allocator_shim::RemoveAllocatorDispatchForTesting(
          allocator_dispatch);  // IN-TEST
    }
#endif

#if PA_BUILDFLAG(USE_PARTITION_ALLOC)
    partition_alloc::PartitionAllocHooks::SetObserverHooks(nullptr, nullptr);
#endif
  }

  base::Lock lock_;
  // Information on the hooks.
  internal::DispatchData dispatch_data_ GUARDED_BY(lock_);
  // Whether Initialize() deferred connecting the hooks to EnsureConnected().
  bool connection_deferred_ GUARDED_BY(lock_) = false;
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

void Dispatcher::Initialize(const internal::DispatchData& dispatch_data,
                            bool (*should_defer_connection)()) {
  impl_->Initialize(dispatch_data, should_defer_connection);
}

void Dispatcher::EnsureConnected() {
  impl_->EnsureConnected();
}

void Dispatcher::ResetForTesting() {
  impl_->Reset();
}
}  // namespace base::allocator::dispatcher
