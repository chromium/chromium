// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/tagging.h"

#include <cstdint>

#include "base/allocator/partition_allocator/page_allocator.h"
#include "base/allocator/partition_allocator/partition_alloc_base/cpu.h"
#include "base/allocator/partition_allocator/partition_alloc_config.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace partition_alloc::internal {

// Check whether we can call the tagging intrinsics safely on all architectures.
TEST(PartitionAllocMemoryTaggingTest, TagMemoryRangeRandomlySafe) {
  ::partition_alloc::internal::InitializeMTESupportIfNeeded();
  uintptr_t buffer =
      AllocPages(PageAllocationGranularity(), PageAllocationGranularity(),
                 PageAccessibilityConfiguration(
                     PageAccessibilityConfiguration::kReadWriteTagged),
                 PageTag::kChromium);
  EXPECT_TRUE(buffer);
  void* bufferp = TagMemoryRangeRandomly(buffer, 4 * kMemTagGranuleSize, 0u);
  EXPECT_TRUE(bufferp);
  int* buffer0 = static_cast<int*>(bufferp);
  *buffer0 = 42;
  EXPECT_EQ(42, *buffer0);
  FreePages(buffer, PageAllocationGranularity());
}

TEST(PartitionAllocMemoryTaggingTest, TagMemoryRangeIncrementSafe) {
  ::partition_alloc::internal::InitializeMTESupportIfNeeded();
  base::CPU cpu;
  uintptr_t buffer =
      AllocPages(PageAllocationGranularity(), PageAllocationGranularity(),
                 PageAccessibilityConfiguration(
                     PageAccessibilityConfiguration::kReadWriteTagged),
                 PageTag::kChromium);
  EXPECT_TRUE(buffer);
  void* bufferp = TagMemoryRangeIncrement(buffer, 4 * kMemTagGranuleSize);
  EXPECT_TRUE(bufferp);
  int* buffer0 = static_cast<int*>(bufferp);
  *buffer0 = 42;
  EXPECT_EQ(42, *buffer0);
  if (cpu.has_mte()) {
    EXPECT_NE(bufferp, reinterpret_cast<void*>(buffer));
  }
  FreePages(buffer, PageAllocationGranularity());
}

#if defined(ARCH_CPU_64_BITS)
// Size / alignment constraints are only enforced on 64-bit architectures.
TEST(PartitionAllocMemoryTaggingTest, TagMemoryRangeBadSz) {
  ::partition_alloc::internal::InitializeMTESupportIfNeeded();
  base::CPU cpu;
  uintptr_t buffer =
      AllocPages(PageAllocationGranularity(), PageAllocationGranularity(),
                 PageAccessibilityConfiguration(
                     PageAccessibilityConfiguration::kReadWriteTagged),
                 PageTag::kChromium);
  EXPECT_TRUE(buffer);
  void* bufferp =
      TagMemoryRangeRandomly(buffer, 4 * kMemTagGranuleSize - 1, 0u);
  if (cpu.has_mte()) {
    EXPECT_FALSE(bufferp);
  }
  FreePages(buffer, PageAllocationGranularity());
}

TEST(PartitionAllocMemoryTaggingTest, TagMemoryRangeRandomlyNoSz) {
  ::partition_alloc::internal::InitializeMTESupportIfNeeded();
  base::CPU cpu;
  uintptr_t buffer =
      AllocPages(PageAllocationGranularity(), PageAllocationGranularity(),
                 PageAccessibilityConfiguration(
                     PageAccessibilityConfiguration::kReadWriteTagged),
                 PageTag::kChromium);
  EXPECT_TRUE(buffer);
  void* bufferp = TagMemoryRangeRandomly(buffer, 0, 0u);
  if (cpu.has_mte()) {
    EXPECT_FALSE(bufferp);
  }
  FreePages(buffer, PageAllocationGranularity());
}

TEST(PartitionAllocMemoryTaggingTest, TagMemoryRangeRandomlyBadAlign) {
  ::partition_alloc::internal::InitializeMTESupportIfNeeded();
  base::CPU cpu;
  uintptr_t buffer =
      AllocPages(PageAllocationGranularity(), PageAllocationGranularity(),
                 PageAccessibilityConfiguration(
                     PageAccessibilityConfiguration::kReadWriteTagged),
                 PageTag::kChromium);
  EXPECT_TRUE(buffer);
  void* bufferp =
      TagMemoryRangeRandomly(buffer - 1, 4 * kMemTagGranuleSize, 0u);
  if (cpu.has_mte()) {
    EXPECT_FALSE(bufferp);
  }
  FreePages(buffer, PageAllocationGranularity());
}

TEST(PartitionAllocMemoryTaggingTest, TagMemoryRangeIncrementBadSz) {
  ::partition_alloc::internal::InitializeMTESupportIfNeeded();
  base::CPU cpu;
  uintptr_t buffer =
      AllocPages(PageAllocationGranularity(), PageAllocationGranularity(),
                 PageAccessibilityConfiguration(
                     PageAccessibilityConfiguration::kReadWriteTagged),
                 PageTag::kChromium);
  EXPECT_TRUE(buffer);
  void* bufferp = TagMemoryRangeIncrement(buffer, 4 * kMemTagGranuleSize - 1);
  if (cpu.has_mte()) {
    EXPECT_FALSE(bufferp);
  }
  FreePages(buffer, PageAllocationGranularity());
}

TEST(PartitionAllocMemoryTaggingTest, TagMemoryRangeIncrementNoSz) {
  ::partition_alloc::internal::InitializeMTESupportIfNeeded();
  base::CPU cpu;
  uintptr_t buffer =
      AllocPages(PageAllocationGranularity(), PageAllocationGranularity(),
                 PageAccessibilityConfiguration(
                     PageAccessibilityConfiguration::kReadWriteTagged),
                 PageTag::kChromium);
  EXPECT_TRUE(buffer);
  void* bufferp = TagMemoryRangeIncrement(buffer, 0);
  if (cpu.has_mte()) {
    EXPECT_FALSE(bufferp);
  }
  FreePages(buffer, PageAllocationGranularity());
}

TEST(PartitionAllocMemoryTaggingTest, TagMemoryRangeIncrementBadAlign) {
  ::partition_alloc::internal::InitializeMTESupportIfNeeded();
  base::CPU cpu;
  uintptr_t buffer =
      AllocPages(PageAllocationGranularity(), PageAllocationGranularity(),
                 PageAccessibilityConfiguration(
                     PageAccessibilityConfiguration::kReadWriteTagged),
                 PageTag::kChromium);
  EXPECT_TRUE(buffer);
  void* bufferp = TagMemoryRangeIncrement(buffer - 1, 4 * kMemTagGranuleSize);
  if (cpu.has_mte()) {
    EXPECT_FALSE(bufferp);
  }
  FreePages(buffer, PageAllocationGranularity());
}
#endif  // defined(ARCH_CPU_64_BITS)

}  // namespace partition_alloc::internal
