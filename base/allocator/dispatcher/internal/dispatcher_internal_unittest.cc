// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/dispatcher/internal/dispatcher_internal.h"
#include "base/allocator/dispatcher/testing/dispatcher_test.h"
#include "base/allocator/dispatcher/testing/observer_mock.h"
#include "base/allocator/dispatcher/testing/tools.h"
#include "base/allocator/partition_allocator/src/partition_alloc/partition_alloc_buildflags.h"
#include "base/dcheck_is_on.h"
#include "testing/gtest/include/gtest/gtest.h"

#include <tuple>
#include <utility>

using ::base::allocator::dispatcher::configuration::kMaximumNumberOfObservers;
using ::base::allocator::dispatcher::testing::CreateTupleOfPointers;
using ::base::allocator::dispatcher::testing::DispatcherTest;
using ::testing::_;
using ::testing::InSequence;

namespace base::allocator::dispatcher::internal {

namespace {

struct AllocationEventDispatcherInternalTest : public DispatcherTest {
  static void* GetAllocatedAddress() {
    return reinterpret_cast<void*>(0x12345678);
  }
  static unsigned int GetAllocatedSize() { return 35; }
  static unsigned int GetEstimatedSize() { return 77; }
  static void* GetFreedAddress() {
    return reinterpret_cast<void*>(0x876543210);
  }

#if BUILDFLAG(USE_ALLOCATOR_SHIM)
  AllocatorDispatch* GetNextAllocatorDispatch() { return &allocator_dispatch_; }
  static void* alloc_function(const AllocatorDispatch*, size_t, void*) {
    return GetAllocatedAddress();
  }
  static void* alloc_unchecked_function(const AllocatorDispatch*,
                                        size_t,
                                        void*) {
    return GetAllocatedAddress();
  }
  static void* alloc_zero_initialized_function(const AllocatorDispatch*,
                                               size_t,
                                               size_t,
                                               void*) {
    return GetAllocatedAddress();
  }
  static void* alloc_aligned_function(const AllocatorDispatch*,
                                      size_t,
                                      size_t,
                                      void*) {
    return GetAllocatedAddress();
  }
  static void* realloc_function(const AllocatorDispatch*,
                                void*,
                                size_t,
                                void*) {
    return GetAllocatedAddress();
  }
  static size_t get_size_estimate_function(const AllocatorDispatch*,
                                           void*,
                                           void*) {
    return GetEstimatedSize();
  }
  static bool claimed_address_function(const AllocatorDispatch*, void*, void*) {
    return GetEstimatedSize();
  }
  static unsigned batch_malloc_function(const AllocatorDispatch*,
                                        size_t,
                                        void**,
                                        unsigned num_requested,
                                        void*) {
    return num_requested;
  }
  static void* aligned_malloc_function(const AllocatorDispatch*,
                                       size_t,
                                       size_t,
                                       void*) {
    return GetAllocatedAddress();
  }
  static void* aligned_realloc_function(const AllocatorDispatch*,
                                        void*,
                                        size_t,
                                        size_t,
                                        void*) {
    return GetAllocatedAddress();
  }

  AllocatorDispatch allocator_dispatch_ = {
      &alloc_function,
      &alloc_unchecked_function,
      &alloc_zero_initialized_function,
      &alloc_aligned_function,
      &realloc_function,
      [](const AllocatorDispatch*, void*, void*) {},
      &get_size_estimate_function,
      &claimed_address_function,
      &batch_malloc_function,
      [](const AllocatorDispatch*, void**, unsigned, void*) {},
      [](const AllocatorDispatch*, void*, size_t, void*) {},
      [](const AllocatorDispatch*, void*, void*) {},
      &aligned_malloc_function,
      &aligned_realloc_function,
      [](const AllocatorDispatch*, void*, void*) {}};
#endif
};

}  // namespace

using ::testing::NaggyMock;
using ::testing::StrictMock;

using ObserverMock = StrictMock<testing::ObserverMock<>>;

#if defined(GTEST_HAS_DEATH_TEST) && GTEST_HAS_DEATH_TEST && DCHECK_IS_ON()
TEST(AllocationEventDispatcherInternalDeathTest,
     VerifyDeathWhenObserverIsNull) {
  testing::ObserverMock<int> observer_1;
  testing::ObserverMock<float> observer_2;
  testing::ObserverMock<size_t>* null_observer = nullptr;
  testing::ObserverMock<double> observer_3;

  const auto observer_ptrs =
      std::make_tuple(&observer_1, &observer_2, null_observer, &observer_3);

  EXPECT_DEATH({ GetNotificationHooks(observer_ptrs); }, "");
}
#endif  // defined(GTEST_HAS_DEATH_TEST) && GTEST_HAS_DEATH_TEST &&
        // DCHECK_IS_ON()

#if BUILDFLAG(USE_PARTITION_ALLOC)
TEST_F(AllocationEventDispatcherInternalTest,
       VerifyPartitionAllocatorHooksAreSet) {
  std::array<ObserverMock, 1> observers;

  const auto dispatch_data =
      GetNotificationHooks(CreateTupleOfPointers(observers));

  EXPECT_NE(nullptr, dispatch_data.GetAllocationObserverHook());
  EXPECT_NE(nullptr, dispatch_data.GetFreeObserverHook());
}

TEST_F(AllocationEventDispatcherInternalTest,
       VerifyPartitionAllocatorHooksAreNullWhenNoObservers) {
  const auto dispatch_data = GetNotificationHooks(std::make_tuple());

  EXPECT_EQ(nullptr, dispatch_data.GetAllocationObserverHook());
  EXPECT_EQ(nullptr, dispatch_data.GetFreeObserverHook());
}

TEST_F(AllocationEventDispatcherInternalTest,
       VerifyPartitionAllocatorAllocationHooksTriggerCorrectly) {
  std::array<ObserverMock, kMaximumNumberOfObservers> observers;

  for (auto& mock : observers) {
    EXPECT_CALL(mock, OnAllocation(_, _, _, _)).Times(0);
    EXPECT_CALL(mock, OnAllocation(this, sizeof(*this), _, _)).Times(1);
    EXPECT_CALL(mock, OnFree(_)).Times(0);
  }

  const auto dispatch_data =
      GetNotificationHooks(CreateTupleOfPointers(observers));

  dispatch_data.GetAllocationObserverHook()(
      partition_alloc::AllocationNotificationData(this, sizeof(*this),
                                                  nullptr));
}

TEST_F(AllocationEventDispatcherInternalTest,
       VerifyPartitionAllocatorFreeHooksTriggerCorrectly) {
  std::array<ObserverMock, kMaximumNumberOfObservers> observers;

  for (auto& mock : observers) {
    EXPECT_CALL(mock, OnAllocation(_, _, _, _)).Times(0);
    EXPECT_CALL(mock, OnFree(_)).Times(0);
    EXPECT_CALL(mock, OnFree(this)).Times(1);
  }

  const auto dispatch_data =
      GetNotificationHooks(CreateTupleOfPointers(observers));

  dispatch_data.GetFreeObserverHook()(
      partition_alloc::FreeNotificationData(this));
}
#endif

#if BUILDFLAG(USE_ALLOCATOR_SHIM)
TEST_F(AllocationEventDispatcherInternalTest, VerifyAllocatorShimDataIsSet) {
  std::array<ObserverMock, 1> observers;

  const auto dispatch_data =
      GetNotificationHooks(CreateTupleOfPointers(observers));
  const auto* const allocator_dispatch = dispatch_data.GetAllocatorDispatch();
  EXPECT_NE(nullptr, allocator_dispatch);
  EXPECT_NE(nullptr, allocator_dispatch->alloc_function);
  EXPECT_NE(nullptr, allocator_dispatch->alloc_unchecked_function);
  EXPECT_NE(nullptr, allocator_dispatch->alloc_zero_initialized_function);
  EXPECT_NE(nullptr, allocator_dispatch->alloc_aligned_function);
  EXPECT_NE(nullptr, allocator_dispatch->realloc_function);
  EXPECT_NE(nullptr, allocator_dispatch->free_function);
  EXPECT_NE(nullptr, allocator_dispatch->get_size_estimate_function);
  EXPECT_NE(nullptr, allocator_dispatch->claimed_address_function);
  EXPECT_NE(nullptr, allocator_dispatch->batch_malloc_function);
  EXPECT_NE(nullptr, allocator_dispatch->batch_free_function);
  EXPECT_NE(nullptr, allocator_dispatch->free_definite_size_function);
  EXPECT_NE(nullptr, allocator_dispatch->try_free_default_function);
  EXPECT_NE(nullptr, allocator_dispatch->aligned_malloc_function);
  EXPECT_NE(nullptr, allocator_dispatch->aligned_realloc_function);
  EXPECT_NE(nullptr, allocator_dispatch->aligned_free_function);
}

TEST_F(AllocationEventDispatcherInternalTest,
       VerifyAllocatorShimDataIsNullWhenNoObservers) {
  const auto dispatch_data = GetNotificationHooks(std::make_tuple());

  EXPECT_EQ(nullptr, dispatch_data.GetAllocatorDispatch());
}

TEST_F(AllocationEventDispatcherInternalTest,
       VerifyAllocatorShimHooksTriggerCorrectly_alloc_function) {
  std::array<ObserverMock, kMaximumNumberOfObservers> observers;

  for (auto& mock : observers) {
    EXPECT_CALL(mock, OnAllocation(_, _, _, _)).Times(0);
    EXPECT_CALL(mock, OnAllocation(GetAllocatedAddress(), GetAllocatedSize(),
                                   AllocationSubsystem::kAllocatorShim, _))
        .Times(1);
    EXPECT_CALL(mock, OnFree(_)).Times(0);
  }

  auto const dispatch_data =
      GetNotificationHooks(CreateTupleOfPointers(observers));

  auto* const allocator_dispatch = dispatch_data.GetAllocatorDispatch();
  allocator_dispatch->next = GetNextAllocatorDispatch();

  auto* const allocated_address = allocator_dispatch->alloc_function(
      allocator_dispatch, GetAllocatedSize(), nullptr);

  EXPECT_EQ(allocated_address, GetAllocatedAddress());
}

TEST_F(AllocationEventDispatcherInternalTest,
       VerifyAllocatorShimHooksTriggerCorrectly_alloc_unchecked_function) {
  std::array<ObserverMock, kMaximumNumberOfObservers> observers;

  for (auto& mock : observers) {
    EXPECT_CALL(mock, OnAllocation(_, _, _, _)).Times(0);
    EXPECT_CALL(mock, OnAllocation(GetAllocatedAddress(), GetAllocatedSize(),
                                   AllocationSubsystem::kAllocatorShim, _))
        .Times(1);
    EXPECT_CALL(mock, OnFree(_)).Times(0);
  }

  auto const dispatch_data =
      GetNotificationHooks(CreateTupleOfPointers(observers));

  auto* const allocator_dispatch = dispatch_data.GetAllocatorDispatch();
  allocator_dispatch->next = GetNextAllocatorDispatch();

  auto* const allocated_address = allocator_dispatch->alloc_unchecked_function(
      allocator_dispatch, GetAllocatedSize(), nullptr);

  EXPECT_EQ(allocated_address, GetAllocatedAddress());
}

TEST_F(
    AllocationEventDispatcherInternalTest,
    VerifyAllocatorShimHooksTriggerCorrectly_alloc_zero_initialized_function) {
  std::array<ObserverMock, kMaximumNumberOfObservers> observers;
  constexpr int n = 8;

  for (auto& mock : observers) {
    EXPECT_CALL(mock, OnAllocation(_, _, _, _)).Times(0);
    EXPECT_CALL(mock,
                OnAllocation(GetAllocatedAddress(), n * GetAllocatedSize(),
                             AllocationSubsystem::kAllocatorShim, _))
        .Times(1);
    EXPECT_CALL(mock, OnFree(_)).Times(0);
  }

  auto const dispatch_data =
      GetNotificationHooks(CreateTupleOfPointers(observers));

  auto* const allocator_dispatch = dispatch_data.GetAllocatorDispatch();
  allocator_dispatch->next = GetNextAllocatorDispatch();

  auto* const allocated_address =
      allocator_dispatch->alloc_zero_initialized_function(
          allocator_dispatch, n, GetAllocatedSize(), nullptr);

  EXPECT_EQ(allocated_address, GetAllocatedAddress());
}

TEST_F(AllocationEventDispatcherInternalTest,
       VerifyAllocatorShimHooksTriggerCorrectly_alloc_aligned_function) {
  std::array<ObserverMock, kMaximumNumberOfObservers> observers;

  for (auto& mock : observers) {
    EXPECT_CALL(mock, OnAllocation(_, _, _, _)).Times(0);
    EXPECT_CALL(mock, OnAllocation(GetAllocatedAddress(), GetAllocatedSize(),
                                   AllocationSubsystem::kAllocatorShim, _))
        .Times(1);
    EXPECT_CALL(mock, OnFree(_)).Times(0);
  }

  auto const dispatch_data =
      GetNotificationHooks(CreateTupleOfPointers(observers));

  auto* const allocator_dispatch = dispatch_data.GetAllocatorDispatch();
  allocator_dispatch->next = GetNextAllocatorDispatch();

  auto* const allocated_address = allocator_dispatch->alloc_aligned_function(
      allocator_dispatch, 2048, GetAllocatedSize(), nullptr);

  EXPECT_EQ(allocated_address, GetAllocatedAddress());
}

TEST_F(AllocationEventDispatcherInternalTest,
       VerifyAllocatorShimHooksTriggerCorrectly_realloc_function) {
  std::array<ObserverMock, kMaximumNumberOfObservers> observers;

  for (auto& mock : observers) {
    InSequence execution_order;

    EXPECT_CALL(mock, OnFree(GetFreedAddress())).Times(1);
    EXPECT_CALL(mock, OnAllocation(GetAllocatedAddress(), GetAllocatedSize(),
                                   AllocationSubsystem::kAllocatorShim, _))
        .Times(1);
  }

  auto const dispatch_data =
      GetNotificationHooks(CreateTupleOfPointers(observers));

  auto* const allocator_dispatch = dispatch_data.GetAllocatorDispatch();
  allocator_dispatch->next = GetNextAllocatorDispatch();

  auto* const allocated_address = allocator_dispatch->realloc_function(
      allocator_dispatch, GetFreedAddress(), GetAllocatedSize(), nullptr);

  EXPECT_EQ(allocated_address, GetAllocatedAddress());
}

TEST_F(AllocationEventDispatcherInternalTest,
       VerifyAllocatorShimHooksTriggerCorrectly_free_function) {
  std::array<ObserverMock, kMaximumNumberOfObservers> observers;

  for (auto& mock : observers) {
    EXPECT_CALL(mock, OnFree(GetFreedAddress())).Times(1);
    EXPECT_CALL(mock, OnAllocation(_, _, _, _)).Times(0);
  }

  auto const dispatch_data =
      GetNotificationHooks(CreateTupleOfPointers(observers));

  auto* const allocator_dispatch = dispatch_data.GetAllocatorDispatch();
  allocator_dispatch->next = GetNextAllocatorDispatch();

  allocator_dispatch->free_function(allocator_dispatch, GetFreedAddress(),
                                    nullptr);
}

TEST_F(AllocationEventDispatcherInternalTest,
       VerifyAllocatorShimHooksTriggerCorrectly_get_size_estimate_function) {
  std::array<ObserverMock, kMaximumNumberOfObservers> observers;

  for (auto& mock : observers) {
    EXPECT_CALL(mock, OnFree(_)).Times(0);
    EXPECT_CALL(mock, OnAllocation(_, _, _, _)).Times(0);
  }

  auto const dispatch_data =
      GetNotificationHooks(CreateTupleOfPointers(observers));

  auto* const allocator_dispatch = dispatch_data.GetAllocatorDispatch();
  allocator_dispatch->next = GetNextAllocatorDispatch();

  auto const estimated_size = allocator_dispatch->get_size_estimate_function(
      allocator_dispatch, GetAllocatedAddress(), nullptr);

  EXPECT_EQ(estimated_size, GetEstimatedSize());
}

TEST_F(AllocationEventDispatcherInternalTest,
       VerifyAllocatorShimHooksTriggerCorrectly_batch_malloc_function) {
  constexpr size_t allocation_batch_size = 10;
  std::array<ObserverMock, kMaximumNumberOfObservers> observers;
  std::array<void*, allocation_batch_size> allocation_batch = {nullptr};

  for (auto& mock : observers) {
    EXPECT_CALL(mock, OnFree(_)).Times(0);
    EXPECT_CALL(mock, OnAllocation(nullptr, GetAllocatedSize(),
                                   AllocationSubsystem::kAllocatorShim, _))
        .Times(allocation_batch_size);
  }

  auto const dispatch_data =
      GetNotificationHooks(CreateTupleOfPointers(observers));

  auto* const allocator_dispatch = dispatch_data.GetAllocatorDispatch();
  EXPECT_NE(allocator_dispatch->batch_malloc_function, nullptr);

  allocator_dispatch->next = GetNextAllocatorDispatch();

  auto const number_allocated = allocator_dispatch->batch_malloc_function(
      allocator_dispatch, GetAllocatedSize(), allocation_batch.data(),
      allocation_batch_size, nullptr);

  EXPECT_EQ(number_allocated, allocation_batch_size);
}

TEST_F(AllocationEventDispatcherInternalTest,
       VerifyAllocatorShimHooksTriggerCorrectly_batch_free_function) {
  constexpr size_t allocation_batch_size = 10;
  std::array<ObserverMock, kMaximumNumberOfObservers> observers;
  std::array<void*, allocation_batch_size> allocation_batch;
  allocation_batch.fill(GetFreedAddress());

  for (auto& mock : observers) {
    EXPECT_CALL(mock, OnFree(GetFreedAddress())).Times(allocation_batch_size);
    EXPECT_CALL(mock, OnAllocation(_, _, _, _)).Times(0);
  }

  auto const dispatch_data =
      GetNotificationHooks(CreateTupleOfPointers(observers));

  auto* const allocator_dispatch = dispatch_data.GetAllocatorDispatch();
  EXPECT_NE(allocator_dispatch->batch_free_function, nullptr);

  allocator_dispatch->next = GetNextAllocatorDispatch();

  allocator_dispatch->batch_free_function(allocator_dispatch,
                                          allocation_batch.data(),
                                          allocation_batch_size, nullptr);
}

TEST_F(AllocationEventDispatcherInternalTest,
       VerifyAllocatorShimHooksTriggerCorrectly_free_definite_size_function) {
  std::array<ObserverMock, kMaximumNumberOfObservers> observers;

  for (auto& mock : observers) {
    EXPECT_CALL(mock, OnFree(GetAllocatedAddress())).Times(1);
    EXPECT_CALL(mock, OnAllocation(_, _, _, _)).Times(0);
  }

  DispatchData const dispatch_data =
      GetNotificationHooks(CreateTupleOfPointers(observers));

  auto* const allocator_dispatch = dispatch_data.GetAllocatorDispatch();
  EXPECT_NE(allocator_dispatch->free_definite_size_function, nullptr);

  allocator_dispatch->next = GetNextAllocatorDispatch();

  allocator_dispatch->free_definite_size_function(
      allocator_dispatch, GetAllocatedAddress(), GetAllocatedSize(), nullptr);
}

TEST_F(AllocationEventDispatcherInternalTest,
       VerifyAllocatorShimHooksTriggerCorrectly_try_free_default_function) {
  std::array<ObserverMock, kMaximumNumberOfObservers> observers;

  for (auto& mock : observers) {
    EXPECT_CALL(mock, OnFree(GetAllocatedAddress())).Times(1);
    EXPECT_CALL(mock, OnAllocation(_, _, _, _)).Times(0);
  }

  DispatchData const dispatch_data =
      GetNotificationHooks(CreateTupleOfPointers(observers));

  auto* const allocator_dispatch = dispatch_data.GetAllocatorDispatch();
  EXPECT_NE(allocator_dispatch->try_free_default_function, nullptr);

  allocator_dispatch->next = GetNextAllocatorDispatch();

  allocator_dispatch->try_free_default_function(allocator_dispatch,
                                                GetAllocatedAddress(), nullptr);
}

TEST_F(AllocationEventDispatcherInternalTest,
       VerifyAllocatorShimHooksTriggerCorrectly_aligned_malloc_function) {
  std::array<ObserverMock, kMaximumNumberOfObservers> observers;

  for (auto& mock : observers) {
    EXPECT_CALL(mock, OnAllocation(_, _, _, _)).Times(0);
    EXPECT_CALL(mock, OnAllocation(GetAllocatedAddress(), GetAllocatedSize(),
                                   AllocationSubsystem::kAllocatorShim, _))
        .Times(1);
    EXPECT_CALL(mock, OnFree(_)).Times(0);
  }

  auto const dispatch_data =
      GetNotificationHooks(CreateTupleOfPointers(observers));

  auto* const allocator_dispatch = dispatch_data.GetAllocatorDispatch();
  allocator_dispatch->next = GetNextAllocatorDispatch();

  auto* const allocated_address = allocator_dispatch->aligned_malloc_function(
      allocator_dispatch, GetAllocatedSize(), 2048, nullptr);

  EXPECT_EQ(allocated_address, GetAllocatedAddress());
}

TEST_F(AllocationEventDispatcherInternalTest,
       VerifyAllocatorShimHooksTriggerCorrectly_aligned_realloc_function) {
  std::array<ObserverMock, kMaximumNumberOfObservers> observers;

  for (auto& mock : observers) {
    InSequence execution_order;

    EXPECT_CALL(mock, OnFree(GetFreedAddress())).Times(1);
    EXPECT_CALL(mock, OnAllocation(GetAllocatedAddress(), GetAllocatedSize(),
                                   AllocationSubsystem::kAllocatorShim, _))
        .Times(1);
  }

  auto const dispatch_data =
      GetNotificationHooks(CreateTupleOfPointers(observers));

  auto* const allocator_dispatch = dispatch_data.GetAllocatorDispatch();
  allocator_dispatch->next = GetNextAllocatorDispatch();

  auto* const allocated_address = allocator_dispatch->aligned_realloc_function(
      allocator_dispatch, GetFreedAddress(), GetAllocatedSize(), 2048, nullptr);

  EXPECT_EQ(allocated_address, GetAllocatedAddress());
}

TEST_F(AllocationEventDispatcherInternalTest,
       VerifyAllocatorShimHooksTriggerCorrectly_aligned_free_function) {
  std::array<ObserverMock, kMaximumNumberOfObservers> observers;

  for (auto& mock : observers) {
    EXPECT_CALL(mock, OnFree(GetFreedAddress())).Times(1);
    EXPECT_CALL(mock, OnAllocation(_, _, _, _)).Times(0);
  }

  auto const dispatch_data =
      GetNotificationHooks(CreateTupleOfPointers(observers));

  auto* const allocator_dispatch = dispatch_data.GetAllocatorDispatch();
  allocator_dispatch->next = GetNextAllocatorDispatch();

  allocator_dispatch->aligned_free_function(allocator_dispatch,
                                            GetFreedAddress(), nullptr);
}

#endif
}  // namespace base::allocator::dispatcher::internal
