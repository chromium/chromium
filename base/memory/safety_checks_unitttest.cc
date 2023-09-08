// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <new>
#include "base/allocator/partition_allocator/partition_address_space.h"
#include "base/memory/safety_checks.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
using base::internal::is_memory_safety_checked;
using base::internal::MemorySafetyCheck;

// Normal object: should be targeted by no additional |MemorySafetyCheck|.
struct DefaultChecks {};
static_assert(
    !is_memory_safety_checked<DefaultChecks,
                              MemorySafetyCheck::kForcePartitionAlloc>);

// Annotated object: should have |base::internal::kAdvancedMemorySafetyChecks|.
struct AdvancedChecks {
  ADVANCED_MEMORY_SAFETY_CHECKS();
};
static_assert(
    is_memory_safety_checked<AdvancedChecks,
                             MemorySafetyCheck::kForcePartitionAlloc>);

// Annotated and aligned object for testing aligned allocations.
constexpr int kLargeAlignment = 2 * __STDCPP_DEFAULT_NEW_ALIGNMENT__;
struct alignas(kLargeAlignment) AlignedAdvancedChecks {
  ADVANCED_MEMORY_SAFETY_CHECKS();
};
static_assert(
    is_memory_safety_checked<AlignedAdvancedChecks,
                             MemorySafetyCheck::kForcePartitionAlloc>);

// The macro may hook memory allocation/deallocation but should forward the
// request to PA or any other allocator via
// |HandleMemorySafetyCheckedOperator***|.
TEST(MemorySafetyCheckTest, AllocatorFunctions) {
  // void* operator new(std::size_t count);
  auto* ptr1 = new DefaultChecks();
  auto* ptr2 = new AdvancedChecks();
  EXPECT_NE(ptr1, nullptr);
  EXPECT_NE(ptr2, nullptr);

// AdvancedChecks is kForcePartitionAlloc.
#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  EXPECT_TRUE(partition_alloc::IsManagedByPartitionAlloc(
      reinterpret_cast<uintptr_t>(ptr2)));
#endif  // BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

  // void operator delete(void* ptr);
  delete ptr1;
  delete ptr2;

  // void* operator new(std::size_t count, std::align_val_t alignment)
  ptr1 = new (std::align_val_t(64)) DefaultChecks();
  ptr2 = new (std::align_val_t(64)) AdvancedChecks();
  EXPECT_NE(ptr1, nullptr);
  EXPECT_NE(ptr2, nullptr);

// AdvancedChecks is kForcePartitionAlloc.
#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  EXPECT_TRUE(partition_alloc::IsManagedByPartitionAlloc(
      reinterpret_cast<uintptr_t>(ptr2)));
#endif  // BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

  // void operator delete(void* ptr, std::align_val_t alignment)
  ::operator delete(ptr1, std::align_val_t(64));
  AdvancedChecks::operator delete(ptr2, std::align_val_t(64));

  // void* operator new(std::size_t count, std::align_val_t alignment)
  auto* ptr3 = new AlignedAdvancedChecks();
  EXPECT_NE(ptr3, nullptr);

// AlignedAdvancedChecks is kForcePartitionAlloc.
#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  EXPECT_TRUE(partition_alloc::IsManagedByPartitionAlloc(
      reinterpret_cast<uintptr_t>(ptr3)));
#endif  // BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

  // void operator delete(void* ptr, std::align_val_t alignment)
  delete ptr3;

  // void* operator new(std::size_t, void* ptr)
  char data[32];
  ptr1 = new (data) DefaultChecks();
  ptr2 = new (data) AdvancedChecks();
  ptr3 = new (data) AlignedAdvancedChecks();
}
}  // namespace
