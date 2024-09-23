// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/safety_checks.h"

#include <new>

#include "base/allocator/partition_alloc_features.h"
#include "base/feature_list.h"
#include "partition_alloc/partition_address_space.h"
#include "partition_alloc/tagging.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
using base::internal::is_memory_safety_checked;
using base::internal::MemorySafetyCheck;

// Normal object: should be targeted by no additional |MemorySafetyCheck|.
struct DefaultChecks {
 public:
  char data[16];
};

// Annotated object: should have |base::internal::kAdvancedMemorySafetyChecks|.
struct AdvancedChecks {
  ADVANCED_MEMORY_SAFETY_CHECKS();

 public:
  char data[16];
};

// Annotated object: should have |base::internal::kAdvancedMemorySafetyChecks|.
struct AnotherAdvancedChecks {
  ADVANCED_MEMORY_SAFETY_CHECKS();

 public:
  char data[16];
};

// Annotated and aligned object for testing aligned allocations.
constexpr int kLargeAlignment = 2 * __STDCPP_DEFAULT_NEW_ALIGNMENT__;
struct alignas(kLargeAlignment) AlignedAdvancedChecks {
  ADVANCED_MEMORY_SAFETY_CHECKS();

 public:
  char data[16];
};

struct PrivateInheritanceWithInheritMacro : private AdvancedChecks {
  INHERIT_MEMORY_SAFETY_CHECKS(AdvancedChecks);
};
static_assert(
    is_memory_safety_checked<PrivateInheritanceWithInheritMacro,
                             MemorySafetyCheck::kForcePartitionAlloc>);

struct PrivateInheritanceWithDefaultMacro : private AdvancedChecks {
  DEFAULT_MEMORY_SAFETY_CHECKS();
};
static_assert(
    !is_memory_safety_checked<PrivateInheritanceWithDefaultMacro,
                              MemorySafetyCheck::kForcePartitionAlloc>);

struct MultipleInheritanceWithInheritMacro : AdvancedChecks,
                                             AnotherAdvancedChecks {
  INHERIT_MEMORY_SAFETY_CHECKS(AdvancedChecks);
};
static_assert(
    is_memory_safety_checked<MultipleInheritanceWithInheritMacro,
                             MemorySafetyCheck::kForcePartitionAlloc>);

struct MultipleInheritanceWithDefaultMacro : AdvancedChecks,
                                             AnotherAdvancedChecks {
  DEFAULT_MEMORY_SAFETY_CHECKS();
};
static_assert(
    !is_memory_safety_checked<MultipleInheritanceWithDefaultMacro,
                              MemorySafetyCheck::kForcePartitionAlloc>);

struct AdvancedChecksWithPartialOverwrite {
  ADVANCED_MEMORY_SAFETY_CHECKS(kNone, kForcePartitionAlloc);

 public:
  char data[16];
};
static_assert(
    !is_memory_safety_checked<AdvancedChecksWithPartialOverwrite,
                              MemorySafetyCheck::kForcePartitionAlloc>);

struct InheritanceWithPartialOverwrite : private AdvancedChecks {
  INHERIT_MEMORY_SAFETY_CHECKS(AdvancedChecks, kNone, kForcePartitionAlloc);
};
static_assert(
    !is_memory_safety_checked<InheritanceWithPartialOverwrite,
                              MemorySafetyCheck::kForcePartitionAlloc>);

// The macro may hook memory allocation/deallocation but should forward the
// request to PA or any other allocator via
// |HandleMemorySafetyCheckedOperator***|.
TEST(MemorySafetyCheckTest, AllocatorFunctions) {
  static_assert(
      !is_memory_safety_checked<DefaultChecks,
                                MemorySafetyCheck::kForcePartitionAlloc>);
  static_assert(
      is_memory_safety_checked<AdvancedChecks,
                               MemorySafetyCheck::kForcePartitionAlloc>);
  static_assert(
      is_memory_safety_checked<AlignedAdvancedChecks,
                               MemorySafetyCheck::kForcePartitionAlloc>);

  // void* operator new(std::size_t count);
  auto* ptr1 = new DefaultChecks();
  auto* ptr2 = new AdvancedChecks();
  EXPECT_NE(ptr1, nullptr);
  EXPECT_NE(ptr2, nullptr);

// AdvancedChecks is kForcePartitionAlloc.
#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  EXPECT_TRUE(partition_alloc::IsManagedByPartitionAlloc(
      reinterpret_cast<uintptr_t>(partition_alloc::UntagPtr(ptr2))));
#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

  // void operator delete(void* ptr);
  delete ptr1;
  delete ptr2;

  // void* operator new(std::size_t count, std::align_val_t alignment)
  ptr1 = new (std::align_val_t(64)) DefaultChecks();
  ptr2 = new (std::align_val_t(64)) AdvancedChecks();
  EXPECT_NE(ptr1, nullptr);
  EXPECT_NE(ptr2, nullptr);

// AdvancedChecks is kForcePartitionAlloc.
#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  EXPECT_TRUE(partition_alloc::IsManagedByPartitionAlloc(
      reinterpret_cast<uintptr_t>(partition_alloc::UntagPtr(ptr2))));
#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

  // void operator delete(void* ptr, std::align_val_t alignment)
  ::operator delete(ptr1, std::align_val_t(64));
  AdvancedChecks::operator delete(ptr2, std::align_val_t(64));

  // void* operator new(std::size_t count, std::align_val_t alignment)
  auto* ptr3 = new AlignedAdvancedChecks();
  EXPECT_NE(ptr3, nullptr);

// AlignedAdvancedChecks is kForcePartitionAlloc.
#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  EXPECT_TRUE(partition_alloc::IsManagedByPartitionAlloc(
      reinterpret_cast<uintptr_t>(partition_alloc::UntagPtr(ptr3))));
#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

  // void operator delete(void* ptr, std::align_val_t alignment)
  delete ptr3;

  // void* operator new(std::size_t, void* ptr)
  alignas(AlignedAdvancedChecks) char data[32];
  ptr1 = new (data) DefaultChecks();
  ptr2 = new (data) AdvancedChecks();
  ptr3 = new (data) AlignedAdvancedChecks();
}

#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

TEST(MemorySafetyCheckTest, SchedulerLoopQuarantine) {
  // The check is performed only if `kPartitionAllocSchedulerLoopQuarantine` is
  // enabled. `base::ScopedFeatureList` does not work here because the default
  // `PartitionRoot` is configured before running this test.
  if (!base::FeatureList::IsEnabled(
          base::features::kPartitionAllocSchedulerLoopQuarantine)) {
    return;
  }

  static_assert(
      !is_memory_safety_checked<DefaultChecks,
                                MemorySafetyCheck::kSchedulerLoopQuarantine>);
  static_assert(
      is_memory_safety_checked<AdvancedChecks,
                               MemorySafetyCheck::kSchedulerLoopQuarantine>);

  auto* root =
      base::internal::GetPartitionRootForMemorySafetyCheckedAllocation();
  auto& branch = root->GetSchedulerLoopQuarantineBranchForTesting();

  auto* ptr1 = new DefaultChecks();
  EXPECT_NE(ptr1, nullptr);
  delete ptr1;
  EXPECT_FALSE(branch.IsQuarantinedForTesting(ptr1));

  auto* ptr2 = new AdvancedChecks();
  EXPECT_NE(ptr2, nullptr);
  delete ptr2;
  EXPECT_TRUE(branch.IsQuarantinedForTesting(ptr2));

  branch.Purge();
}

TEST(MemorySafetyCheckTest, ZapOnFree) {
  // The check is performed only if `kPartitionAllocZappingByFreeFlags` is
  // enabled. `base::ScopedFeatureList` does not work here because the default
  // `PartitionRoot` is configured before running this test.
  if (!base::FeatureList::IsEnabled(
          base::features::kPartitionAllocZappingByFreeFlags)) {
    return;
  }

  static_assert(
      !is_memory_safety_checked<DefaultChecks, MemorySafetyCheck::kZapOnFree>);
  static_assert(
      is_memory_safety_checked<AdvancedChecks, MemorySafetyCheck::kZapOnFree>);

  {
    // Without kZapOnFree.
    auto* ptr = new DefaultChecks();
    EXPECT_NE(ptr, nullptr);
    delete ptr;
    // *ptr is undefined.
  }

  {
    // With kZapOnFree.
    auto* ptr = new AdvancedChecks();
    EXPECT_NE(ptr, nullptr);
    memset(ptr->data, 'A', sizeof(ptr->data));
    delete ptr;

    // Dereferencing `ptr` is still undefiner behavior, but we can say it is
    // somewhat defined as this test is gated behind
    // `PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)`.
    // I believe behavior here is concrete enough to be tested, but it can be
    // affected by changes in PA. Please disable this test if it flakes.
    EXPECT_NE(ptr->data[0], 'A');
  }
}

#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

}  // namespace
