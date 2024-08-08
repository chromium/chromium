// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/dispatcher/internal/dispatcher_internal.h"

#include <iostream>
#include <tuple>
#include <utility>

#include "base/allocator/dispatcher/subsystem.h"
#include "base/allocator/dispatcher/testing/dispatcher_test.h"
#include "base/allocator/dispatcher/testing/observer_mock.h"
#include "base/allocator/dispatcher/testing/tools.h"
#include "base/dcheck_is_on.h"
#include "partition_alloc/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

#if PA_BUILDFLAG(USE_PARTITION_ALLOC)
#include "partition_alloc/partition_alloc_allocation_data.h"
#endif

using ::base::allocator::dispatcher::AllocationSubsystem;
using ::base::allocator::dispatcher::configuration::kMaximumNumberOfObservers;
using ::base::allocator::dispatcher::testing::CreateTupleOfPointers;
using ::base::allocator::dispatcher::testing::DispatcherTest;
using ::testing::_;
using ::testing::AllOf;
using ::testing::InSequence;
using ::testing::Matcher;
using ::testing::Property;

namespace base::allocator::dispatcher::internal {

namespace {

auto AllocationNotificationMatches(
    Matcher<void*> address_matcher,
    Matcher<size_t> size_matcher,
    Matcher<AllocationSubsystem> subsystem_matcher = _) {
  return AllOf(Property("address", &AllocationNotificationData::address,
                        std::move(address_matcher)),
               Property("size", &AllocationNotificationData::size,
                        std::move(size_matcher)),
               Property("allocation_subsystem",
                        &AllocationNotificationData::allocation_subsystem,
                        std::move(subsystem_matcher)));
}

auto FreeNotificationMatches(
    Matcher<void*> address_matcher,
    Matcher<AllocationSubsystem> subsystem_matcher = _) {
  return AllOf(Property("address", &FreeNotificationData::address,
                        std::move(address_matcher)),
               Property("allocation_subsystem",
                        &FreeNotificationData::allocation_subsystem,
                        std::move(subsystem_matcher)));
}

#if PA_BUILDFLAG(USE_PARTITION_ALLOC)
::partition_alloc::AllocationNotificationData CreatePAAllocationData(
    void* address,
    size_t size,
    partition_alloc::TagViolationReportingMode mte_mode =
        partition_alloc::TagViolationReportingMode::kUndefined) {
  return ::partition_alloc::AllocationNotificationData(address, size, nullptr)
#if PA_BUILDFLAG(HAS_MEMORY_TAGGING)
      .SetMteReportingMode(mte_mode)
#endif
      ;
}

::partition_alloc::FreeNotificationData CreatePAFreeData(
    void* address,
    partition_alloc::TagViolationReportingMode mte_mode =
        partition_alloc::TagViolationReportingMode::kUndefined) {
  return ::partition_alloc::FreeNotificationData(address)
#if PA_BUILDFLAG(HAS_MEMORY_TAGGING)
      .SetMteReportingMode(mte_mode)
#endif
      ;
}
#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC)

struct AllocationEventDispatcherInternalTest : public DispatcherTest {
  static void* GetAllocatedAddress() {
    return reinterpret_cast<void*>(0x12345678);
  }
  static unsigned int GetAllocatedSize() { return 35; }
  static unsigned int GetEstimatedSize() { return 77; }
  static void* GetFreedAddress() {
    return reinterpret_cast<void*>(0x876543210);
  }

#if PA_BUILDFLAG(USE_ALLOCATOR_SHIM)
  AllocatorDispatch* GetNextAllocatorDispatch() { return &allocator_dispatch_; }
  static void* alloc_function(size_t, void*) { return GetAllocatedAddress(); }
  static void* alloc_unchecked_function(size_t, void*) {
    return GetAllocatedAddress();
  }
  static void* alloc_zero_initialized_function(size_t, size_t, void*) {
    return GetAllocatedAddress();
  }
  static void* alloc_aligned_function(size_t, size_t, void*) {
    return GetAllocatedAddress();
  }
  static void* realloc_function(void*, size_t, void*) {
    return GetAllocatedAddress();
  }
  static void* realloc_unchecked_function(void*, size_t, void*) {
    return GetAllocatedAddress();
  }
  static size_t get_size_estimate_function(void*, void*) {
    return GetEstimatedSize();
  }
  static size_t good_size_function(size_t size, void*) { return size; }
  static bool claimed_address_function(void*, void*) {
    return GetEstimatedSize();
  }
  static unsigned batch_malloc_function(size_t,
                                        void**,
                                        unsigned num_requested,
                                        void*) {
    return num_requested;
  }
  static void* aligned_malloc_function(size_t, size_t, void*) {
    return GetAllocatedAddress();
  }
  static void* aligned_malloc_unchecked_function(size_t, size_t, void*) {
    return GetAllocatedAddress();
  }
  static void* aligned_realloc_function(void*, size_t, size_t, void*) {
    return GetAllocatedAddress();
  }
  static void* aligned_realloc_unchecked_function(void*,
                                                  size_t,
                                                  size_t,
                                                  void*) {
    return GetAllocatedAddress();
  }

  AllocatorDispatch allocator_dispatch_ = {&alloc_function,
                                           &alloc_unchecked_function,
                                           &alloc_zero_initialized_function,
                                           &alloc_aligned_function,
                                           &realloc_function,
                                           &realloc_unchecked_function,
                                           [](void*, void*) {},
                                           &get_size_estimate_function,
                                           &good_size_function,
                                           &claimed_address_function,
                                           &batch_malloc_function,
                                           [](void**, unsigned, void*) {},
                                           [](void*, size_t, void*) {},
                                           [](void*, void*) {},
                                           &aligned_malloc_function,
                                           &aligned_malloc_unchecked_function,
                                           &aligned_realloc_function,
                                           &aligned_realloc_unchecked_function,
                                           [](void*, void*) {}};
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

#if PA_BUILDFLAG(USE_PARTITION_ALLOC)
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
    EXPECT_CALL(mock, OnAllocation(_)).Times(0);
    EXPECT_CALL(
        mock, OnAllocation(AllocationNotificationMatches(this, sizeof(*this))))
        .Times(1);
    EXPECT_CALL(mock, OnFree(_)).Times(0);
  }

  const auto dispatch_data =
      GetNotificationHooks(CreateTupleOfPointers(observers));

  ::partition_alloc::AllocationNotificationData notification_data =
      CreatePAAllocationData(this, sizeof(*this));

  dispatch_data.GetAllocationObserverHook()(notification_data);
}

TEST_F(AllocationEventDispatcherInternalTest,
       VerifyPartitionAllocatorFreeHooksTriggerCorrectly) {
  std::array<ObserverMock, kMaximumNumberOfObservers> observers;

  for (auto& mock : observers) {
    EXPECT_CALL(mock, OnAllocation(_)).Times(0);
    EXPECT_CALL(mock, OnFree(_)).Times(0);
    EXPECT_CALL(mock, OnFree(FreeNotificationMatches(this))).Times(1);
  }

  const auto dispatch_data =
      GetNotificationHooks(CreateTupleOfPointers(observers));

  dispatch_data.GetFreeObserverHook()(CreatePAFreeData(this));
}
#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC)

#if PA_BUILDFLAG(USE_ALLOCATOR_SHIM)
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
  EXPECT_NE(nullptr, allocator_dispatch->realloc_unchecked_function);
  EXPECT_NE(nullptr, allocator_dispatch->free_function);
  EXPECT_NE(nullptr, allocator_dispatch->batch_malloc_function);
  EXPECT_NE(nullptr, allocator_dispatch->batch_free_function);
  EXPECT_NE(nullptr, allocator_dispatch->free_definite_size_function);
  EXPECT_NE(nullptr, allocator_dispatch->try_free_default_function);
  EXPECT_NE(nullptr, allocator_dispatch->aligned_malloc_function);
  EXPECT_NE(nullptr, allocator_dispatch->aligned_malloc_unchecked_function);
  EXPECT_NE(nullptr, allocator_dispatch->aligned_realloc_function);
  EXPECT_NE(nullptr, allocator_dispatch->aligned_realloc_unchecked_function);
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
    EXPECT_CALL(mock, OnAllocation(_)).Times(0);
    EXPECT_CALL(mock, OnAllocation(AllocationNotificationMatches(
                          GetAllocatedAddress(), GetAllocatedSize(),
                          AllocationSubsystem::kAllocatorShim)))
        .Times(1);
    EXPECT_CALL(mock, OnFree(_)).Times(0);
  }

  auto const dispatch_data =
      GetNotificationHooks(CreateTupleOfPointers(observers));

  auto* const allocator_dispatch = dispatch_data.GetAllocatorDispatch();
  allocator_dispatch->next = GetNextAllocatorDispatch();

  auto* const allocated_address =
      allocator_dispatch->alloc_function(GetAllocatedSize(), nullptr);

  EXPECT_EQ(allocated_address, GetAllocatedAddress());
}

TEST_F(AllocationEventDispatcherInternalTest,
       VerifyAllocatorShimHooksTriggerCorrectly_alloc_unchecked_function) {
  std::array<ObserverMock, kMaximumNumberOfObservers> observers;

  for (auto& mock : observers) {
    EXPECT_CALL(mock, OnAllocation(_)).Times(0);
    EXPECT_CALL(mock, OnAllocation(AllocationNotificationMatches(
                          GetAllocatedAddress(), GetAllocatedSize(),
                          AllocationSubsystem::kAllocatorShim)))
        .Times(1);
    EXPECT_CALL(mock, OnFree(_)).Times(0);
  }

  auto const dispatch_data =
      GetNotificationHooks(CreateTupleOfPointers(observers));

  auto* const allocator_dispatch = dispatch_data.GetAllocatorDispatch();
  allocator_dispatch->next = GetNextAllocatorDispatch();

  auto* const allocated_address =
      allocator_dispatch->alloc_unchecked_function(GetAllocatedSize(), nullptr);

  EXPECT_EQ(allocated_address, GetAllocatedAddress());
}

TEST_F(
    AllocationEventDispatcherInternalTest,
    VerifyAllocatorShimHooksTriggerCorrectly_alloc_zero_initialized_function) {
  std::array<ObserverMock, kMaximumNumberOfObservers> observers;
  constexpr int n = 8;

  for (auto& mock : observers) {
    EXPECT_CALL(mock, OnAllocation(_)).Times(0);
    EXPECT_CALL(mock, OnAllocation(AllocationNotificationMatches(
                          GetAllocatedAddress(), n * GetAllocatedSize(),
                          AllocationSubsystem::kAllocatorShim)))
        .Times(1);
    EXPECT_CALL(mock, OnFree(_)).Times(0);
  }

  auto const dispatch_data =
      GetNotificationHooks(CreateTupleOfPointers(observers));

  auto* const allocator_dispatch = dispatch_data.GetAllocatorDispatch();
  allocator_dispatch->next = GetNextAllocatorDispatch();

  auto* const allocated_address =
      allocator_dispatch->alloc_zero_initialized_function(n, GetAllocatedSize(),
                                                          nullptr);

  EXPECT_EQ(allocated_address, GetAllocatedAddress());
}

TEST_F(AllocationEventDispatcherInternalTest,
       VerifyAllocatorShimHooksTriggerCorrectly_alloc_aligned_function) {
  std::array<ObserverMock, kMaximumNumberOfObservers> observers;

  for (auto& mock : observers) {
    EXPECT_CALL(mock, OnAllocation(_)).Times(0);
    EXPECT_CALL(mock, OnAllocation(AllocationNotificationMatches(
                          GetAllocatedAddress(), GetAllocatedSize(),
                          AllocationSubsystem::kAllocatorShim)))
        .Times(1);
    EXPECT_CALL(mock, OnFree(_)).Times(0);
  }

  auto const dispatch_data =
      GetNotificationHooks(CreateTupleOfPointers(observers));

  auto* const allocator_dispatch = dispatch_data.GetAllocatorDispatch();
  allocator_dispatch->next = GetNextAllocatorDispatch();

  auto* const allocated_address = allocator_dispatch->alloc_aligned_function(
      2048, GetAllocatedSize(), nullptr);

  EXPECT_EQ(allocated_address, GetAllocatedAddress());
}

TEST_F(AllocationEventDispatcherInternalTest,
       VerifyAllocatorShimHooksTriggerCorrectly_realloc_function) {
  std::array<ObserverMock, kMaximumNumberOfObservers> observers;

  for (auto& mock : observers) {
    InSequence execution_order;

    EXPECT_CALL(mock,
                OnFree(FreeNotificationMatches(
                    GetFreedAddress(), AllocationSubsystem::kAllocatorShim)))
        .Times(1);
    EXPECT_CALL(mock, OnAllocation(AllocationNotificationMatches(
                          GetAllocatedAddress(), GetAllocatedSize(),
                          AllocationSubsystem::kAllocatorShim)))
        .Times(1);
  }

  auto const dispatch_data =
      GetNotificationHooks(CreateTupleOfPointers(observers));

  auto* const allocator_dispatch = dispatch_data.GetAllocatorDispatch();
  allocator_dispatch->next = GetNextAllocatorDispatch();

  auto* const allocated_address = allocator_dispatch->realloc_function(
      GetFreedAddress(), GetAllocatedSize(), nullptr);

  EXPECT_EQ(allocated_address, GetAllocatedAddress());
}

TEST_F(AllocationEventDispatcherInternalTest,
       VerifyAllocatorShimHooksTriggerCorrectly_realloc_unchecked_function) {
  std::array<ObserverMock, kMaximumNumberOfObservers> observers;

  for (auto& mock : observers) {
    InSequence execution_order;

    EXPECT_CALL(mock,
                OnFree(FreeNotificationMatches(
                    GetFreedAddress(), AllocationSubsystem::kAllocatorShim)))
        .Times(1);
    EXPECT_CALL(mock, OnAllocation(AllocationNotificationMatches(
                          GetAllocatedAddress(), GetAllocatedSize(),
                          AllocationSubsystem::kAllocatorShim)))
        .Times(1);
  }

  auto const dispatch_data =
      GetNotificationHooks(CreateTupleOfPointers(observers));

  auto* const allocator_dispatch = dispatch_data.GetAllocatorDispatch();
  allocator_dispatch->next = GetNextAllocatorDispatch();

  auto* const allocated_address =
      allocator_dispatch->realloc_unchecked_function(
          GetFreedAddress(), GetAllocatedSize(), nullptr);

  EXPECT_EQ(allocated_address, GetAllocatedAddress());
}

TEST_F(AllocationEventDispatcherInternalTest,
       VerifyAllocatorShimHooksTriggerCorrectly_free_function) {
  std::array<ObserverMock, kMaximumNumberOfObservers> observers;

  for (auto& mock : observers) {
    EXPECT_CALL(mock,
                OnFree(FreeNotificationMatches(
                    GetFreedAddress(), AllocationSubsystem::kAllocatorShim)))
        .Times(1);
    EXPECT_CALL(mock, OnAllocation(_)).Times(0);
  }

  auto const dispatch_data =
      GetNotificationHooks(CreateTupleOfPointers(observers));

  auto* const allocator_dispatch = dispatch_data.GetAllocatorDispatch();
  allocator_dispatch->next = GetNextAllocatorDispatch();

  allocator_dispatch->free_function(GetFreedAddress(), nullptr);
}

TEST_F(AllocationEventDispatcherInternalTest,
       VerifyAllocatorShimHooksTriggerCorrectly_batch_malloc_function) {
  constexpr size_t allocation_batch_size = 10;
  std::array<ObserverMock, kMaximumNumberOfObservers> observers;
  std::array<void*, allocation_batch_size> allocation_batch = {nullptr};

  for (auto& mock : observers) {
    EXPECT_CALL(mock, OnFree(_)).Times(0);
    EXPECT_CALL(mock, OnAllocation(AllocationNotificationMatches(
                          nullptr, GetAllocatedSize(),
                          AllocationSubsystem::kAllocatorShim)))
        .Times(allocation_batch_size);
  }

  auto const dispatch_data =
      GetNotificationHooks(CreateTupleOfPointers(observers));

  auto* const allocator_dispatch = dispatch_data.GetAllocatorDispatch();
  EXPECT_NE(allocator_dispatch->batch_malloc_function, nullptr);

  allocator_dispatch->next = GetNextAllocatorDispatch();

  auto const number_allocated = allocator_dispatch->batch_malloc_function(
      GetAllocatedSize(), allocation_batch.data(), allocation_batch_size,
      nullptr);

  EXPECT_EQ(number_allocated, allocation_batch_size);
}

TEST_F(AllocationEventDispatcherInternalTest,
       VerifyAllocatorShimHooksTriggerCorrectly_batch_free_function) {
  constexpr size_t allocation_batch_size = 10;
  std::array<ObserverMock, kMaximumNumberOfObservers> observers;
  std::array<void*, allocation_batch_size> allocation_batch;
  allocation_batch.fill(GetFreedAddress());

  for (auto& mock : observers) {
    EXPECT_CALL(mock,
                OnFree(FreeNotificationMatches(
                    GetFreedAddress(), AllocationSubsystem::kAllocatorShim)))
        .Times(allocation_batch_size);
    EXPECT_CALL(mock, OnAllocation(_)).Times(0);
  }

  auto const dispatch_data =
      GetNotificationHooks(CreateTupleOfPointers(observers));

  auto* const allocator_dispatch = dispatch_data.GetAllocatorDispatch();
  EXPECT_NE(allocator_dispatch->batch_free_function, nullptr);

  allocator_dispatch->next = GetNextAllocatorDispatch();

  allocator_dispatch->batch_free_function(allocation_batch.data(),
                                          allocation_batch_size, nullptr);
}

TEST_F(AllocationEventDispatcherInternalTest,
       VerifyAllocatorShimHooksTriggerCorrectly_free_definite_size_function) {
  std::array<ObserverMock, kMaximumNumberOfObservers> observers;

  for (auto& mock : observers) {
    EXPECT_CALL(
        mock, OnFree(FreeNotificationMatches(
                  GetAllocatedAddress(), AllocationSubsystem::kAllocatorShim)))
        .Times(1);
    EXPECT_CALL(mock, OnAllocation(_)).Times(0);
  }

  DispatchData const dispatch_data =
      GetNotificationHooks(CreateTupleOfPointers(observers));

  auto* const allocator_dispatch = dispatch_data.GetAllocatorDispatch();
  EXPECT_NE(allocator_dispatch->free_definite_size_function, nullptr);

  allocator_dispatch->next = GetNextAllocatorDispatch();

  allocator_dispatch->free_definite_size_function(GetAllocatedAddress(),
                                                  GetAllocatedSize(), nullptr);
}

TEST_F(AllocationEventDispatcherInternalTest,
       VerifyAllocatorShimHooksTriggerCorrectly_try_free_default_function) {
  std::array<ObserverMock, kMaximumNumberOfObservers> observers;

  for (auto& mock : observers) {
    EXPECT_CALL(
        mock, OnFree(FreeNotificationMatches(
                  GetAllocatedAddress(), AllocationSubsystem::kAllocatorShim)))
        .Times(1);
    EXPECT_CALL(mock, OnAllocation(_)).Times(0);
  }

  DispatchData const dispatch_data =
      GetNotificationHooks(CreateTupleOfPointers(observers));

  auto* const allocator_dispatch = dispatch_data.GetAllocatorDispatch();
  EXPECT_NE(allocator_dispatch->try_free_default_function, nullptr);

  allocator_dispatch->next = GetNextAllocatorDispatch();

  allocator_dispatch->try_free_default_function(GetAllocatedAddress(), nullptr);
}

TEST_F(AllocationEventDispatcherInternalTest,
       VerifyAllocatorShimHooksTriggerCorrectly_aligned_malloc_function) {
  std::array<ObserverMock, kMaximumNumberOfObservers> observers;

  for (auto& mock : observers) {
    EXPECT_CALL(mock, OnAllocation(_)).Times(0);
    EXPECT_CALL(mock, OnAllocation(AllocationNotificationMatches(
                          GetAllocatedAddress(), GetAllocatedSize(),
                          AllocationSubsystem::kAllocatorShim)))
        .Times(1);
    EXPECT_CALL(mock, OnFree(_)).Times(0);
  }

  auto const dispatch_data =
      GetNotificationHooks(CreateTupleOfPointers(observers));

  auto* const allocator_dispatch = dispatch_data.GetAllocatorDispatch();
  allocator_dispatch->next = GetNextAllocatorDispatch();

  auto* const allocated_address = allocator_dispatch->aligned_malloc_function(
      GetAllocatedSize(), 2048, nullptr);

  EXPECT_EQ(allocated_address, GetAllocatedAddress());
}

TEST_F(
    AllocationEventDispatcherInternalTest,
    VerifyAllocatorShimHooksTriggerCorrectly_aligned_malloc_unchecked_function) {
  std::array<ObserverMock, kMaximumNumberOfObservers> observers;

  for (auto& mock : observers) {
    EXPECT_CALL(mock, OnAllocation(_)).Times(0);
    EXPECT_CALL(mock, OnAllocation(AllocationNotificationMatches(
                          GetAllocatedAddress(), GetAllocatedSize(),
                          AllocationSubsystem::kAllocatorShim)))
        .Times(1);
    EXPECT_CALL(mock, OnFree(_)).Times(0);
  }

  auto const dispatch_data =
      GetNotificationHooks(CreateTupleOfPointers(observers));

  auto* const allocator_dispatch = dispatch_data.GetAllocatorDispatch();
  allocator_dispatch->next = GetNextAllocatorDispatch();

  auto* const allocated_address =
      allocator_dispatch->aligned_malloc_unchecked_function(GetAllocatedSize(),
                                                            2048, nullptr);

  EXPECT_EQ(allocated_address, GetAllocatedAddress());
}

TEST_F(AllocationEventDispatcherInternalTest,
       VerifyAllocatorShimHooksTriggerCorrectly_aligned_realloc_function) {
  std::array<ObserverMock, kMaximumNumberOfObservers> observers;

  for (auto& mock : observers) {
    InSequence execution_order;

    EXPECT_CALL(mock,
                OnFree(FreeNotificationMatches(
                    GetFreedAddress(), AllocationSubsystem::kAllocatorShim)))
        .Times(1);
    EXPECT_CALL(mock, OnAllocation(AllocationNotificationMatches(
                          GetAllocatedAddress(), GetAllocatedSize(),
                          AllocationSubsystem::kAllocatorShim)))
        .Times(1);
  }

  auto const dispatch_data =
      GetNotificationHooks(CreateTupleOfPointers(observers));

  auto* const allocator_dispatch = dispatch_data.GetAllocatorDispatch();
  allocator_dispatch->next = GetNextAllocatorDispatch();

  auto* const allocated_address = allocator_dispatch->aligned_realloc_function(
      GetFreedAddress(), GetAllocatedSize(), 2048, nullptr);

  EXPECT_EQ(allocated_address, GetAllocatedAddress());
}

TEST_F(
    AllocationEventDispatcherInternalTest,
    VerifyAllocatorShimHooksTriggerCorrectly_aligned_realloc_unchecked_function) {
  std::array<ObserverMock, kMaximumNumberOfObservers> observers;

  for (auto& mock : observers) {
    InSequence execution_order;

    EXPECT_CALL(mock,
                OnFree(FreeNotificationMatches(
                    GetFreedAddress(), AllocationSubsystem::kAllocatorShim)))
        .Times(1);
    EXPECT_CALL(mock, OnAllocation(AllocationNotificationMatches(
                          GetAllocatedAddress(), GetAllocatedSize(),
                          AllocationSubsystem::kAllocatorShim)))
        .Times(1);
  }

  auto const dispatch_data =
      GetNotificationHooks(CreateTupleOfPointers(observers));

  auto* const allocator_dispatch = dispatch_data.GetAllocatorDispatch();
  allocator_dispatch->next = GetNextAllocatorDispatch();

  auto* const allocated_address =
      allocator_dispatch->aligned_realloc_unchecked_function(
          GetFreedAddress(), GetAllocatedSize(), 2048, nullptr);

  EXPECT_EQ(allocated_address, GetAllocatedAddress());
}

TEST_F(AllocationEventDispatcherInternalTest,
       VerifyAllocatorShimHooksTriggerCorrectly_aligned_free_function) {
  std::array<ObserverMock, kMaximumNumberOfObservers> observers;

  for (auto& mock : observers) {
    EXPECT_CALL(mock,
                OnFree(FreeNotificationMatches(
                    GetFreedAddress(), AllocationSubsystem::kAllocatorShim)))
        .Times(1);
    EXPECT_CALL(mock, OnAllocation(_)).Times(0);
  }

  auto const dispatch_data =
      GetNotificationHooks(CreateTupleOfPointers(observers));

  auto* const allocator_dispatch = dispatch_data.GetAllocatorDispatch();
  allocator_dispatch->next = GetNextAllocatorDispatch();

  allocator_dispatch->aligned_free_function(GetFreedAddress(), nullptr);
}
#endif  // PA_BUILDFLAG(USE_ALLOCATOR_SHIM)
}  // namespace base::allocator::dispatcher::internal
