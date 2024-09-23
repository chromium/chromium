// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/dispatcher/dispatcher.h"

#include "base/allocator/dispatcher/configuration.h"
#include "base/allocator/dispatcher/notification_data.h"
#include "base/allocator/dispatcher/testing/dispatcher_test.h"
#include "base/allocator/dispatcher/testing/tools.h"
#include "build/build_config.h"
#include "partition_alloc/buildflags.h"
#include "partition_alloc/partition_root.h"
#include "testing/gtest/include/gtest/gtest.h"

#if PA_BUILDFLAG(USE_PARTITION_ALLOC)
#include "partition_alloc/partition_alloc_for_testing.h"
#endif

#if PA_BUILDFLAG(USE_ALLOCATOR_SHIM)
#include "partition_alloc/shim/allocator_shim.h"
#endif

#include <tuple>

namespace base::allocator::dispatcher {
namespace {
using configuration::kMaximumNumberOfObservers;
using configuration::kMaximumNumberOfOptionalObservers;
using partition_alloc::PartitionOptions;
using partition_alloc::PartitionRoot;
using testing::DispatcherTest;

// A simple observer implementation. Since these tests plug in to Partition
// Allocator and Allocator Shim, implementing an observer with Google Mock
// results in endless recursion.
struct ObserverMock {
  void OnAllocation(const AllocationNotificationData& notification_data) {
    ++on_allocation_calls_;
  }
  void OnFree(const FreeNotificationData& notification_data) {
    ++on_free_calls_;
  }

  void Reset() {
    on_allocation_calls_ = 0;
    on_free_calls_ = 0;
  }

  size_t GetNumberOnAllocationCalls() const { return on_allocation_calls_; }
  size_t GetNumberOnFreeCalls() const { return on_free_calls_; }

 private:
  size_t on_allocation_calls_ = 0;
  size_t on_free_calls_ = 0;
};

struct DispatcherInitializerGuard {
  template <typename... Observers>
  explicit DispatcherInitializerGuard(std::tuple<Observers*...> observers) {
    Dispatcher::GetInstance().Initialize(observers);
  }

  ~DispatcherInitializerGuard() { Dispatcher::GetInstance().ResetForTesting(); }
};

struct BaseAllocatorDispatcherTest : public DispatcherTest {};

template <typename A>
void DoBasicTest(A& allocator) {
  // All we want to verify is that the Dispatcher correctly hooks into the
  // passed allocator. Therefore, we do not perform an exhaustive test but
  // just check some basics.
  std::array<ObserverMock, kMaximumNumberOfObservers> observers;

  {
    DispatcherInitializerGuard const g(
        testing::CreateTupleOfPointers(observers));

    constexpr size_t size_to_allocate = 1024;
    void* const ptr = allocator.Alloc(size_to_allocate);
    allocator.Free(ptr);
  }

  for (const auto& mock : observers) {
    EXPECT_GE(mock.GetNumberOnAllocationCalls(), 1u);
    EXPECT_GE(mock.GetNumberOnFreeCalls(), 1u);
  }
}

TEST_F(BaseAllocatorDispatcherTest, VerifyInitialization) {
  std::array<ObserverMock, kMaximumNumberOfObservers> observers;

  DispatcherInitializerGuard g(testing::CreateTupleOfPointers(observers));
}

#if PA_BUILDFLAG(USE_PARTITION_ALLOC) && \
    !defined(MEMORY_TOOL_REPLACES_ALLOCATOR)
// Don't enable this test when MEMORY_TOOL_REPLACES_ALLOCATOR is defined,
// because it makes PartitionAlloc take a different path that doesn't provide
// notifications to observer hooks.
struct PartitionAllocator {
  void* Alloc(size_t size) { return alloc_.AllocInline(size); }
  void Free(void* data) { alloc_.Free(data); }
  ~PartitionAllocator() {
    // Use |DisallowLeaks| to confirm that there is no memory allocated and
    // not yet freed.
    alloc_.ResetForTesting(::partition_alloc::internal::DisallowLeaks);
  }

 private:
  PartitionRoot alloc_{PartitionOptions{}};
};

TEST_F(BaseAllocatorDispatcherTest, VerifyNotificationUsingPartitionAllocator) {
  PartitionAllocator allocator;
  DoBasicTest(allocator);
}
#endif

#if PA_BUILDFLAG(USE_ALLOCATOR_SHIM)
struct AllocatorShimAllocator {
  void* Alloc(size_t size) { return allocator_shim::UncheckedAlloc(size); }
  void Free(void* data) { allocator_shim::UncheckedFree(data); }
};

#if BUILDFLAG(IS_APPLE) && !PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
// Disable the test when running on any of Apple's OSs without PartitionAlloc
// being the default allocator. In this case, all allocations are routed to
// MallocImpl, which then causes the test to terminate unexpectedly.
#define MAYBE_VerifyNotificationUsingAllocatorShim \
  DISABLED_VerifyNotificationUsingAllocatorShim
#else
#define MAYBE_VerifyNotificationUsingAllocatorShim \
  VerifyNotificationUsingAllocatorShim
#endif

TEST_F(BaseAllocatorDispatcherTest, MAYBE_VerifyNotificationUsingAllocatorShim) {
  AllocatorShimAllocator allocator;
  DoBasicTest(allocator);
}
#endif

}  // namespace
}  // namespace base::allocator::dispatcher
