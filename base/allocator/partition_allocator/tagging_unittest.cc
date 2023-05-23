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

#if PA_CONFIG(HAS_MEMORY_TAGGING)
#if BUILDFLAG(IS_ANDROID)
TEST(PartitionAllocMemoryTaggingTest,
     ChangeMemoryTaggingModeForAllThreadsPerProcess) {
  base::CPU cpu;
  // If the underlying platform does not support MTE, skip this test to avoid
  // hiding failures.
  if (!cpu.has_mte()) {
    GTEST_SKIP();
  }

  // The mode should be set to synchronous on startup by AndroidManifest.xml
  // for base_unittests.
  EXPECT_EQ(GetMemoryTaggingModeForCurrentThread(),
            TagViolationReportingMode::kSynchronous);

  // Skip changing to kDisabled, because scudo does not support enabling MTE
  // once it is disabled.
  ChangeMemoryTaggingModeForAllThreadsPerProcess(
      TagViolationReportingMode::kAsynchronous);
  EXPECT_EQ(GetMemoryTaggingModeForCurrentThread(),
            TagViolationReportingMode::kAsynchronous);
  ChangeMemoryTaggingModeForAllThreadsPerProcess(
      TagViolationReportingMode::kSynchronous);
  // End with mode changed back to synchronous.
  EXPECT_EQ(GetMemoryTaggingModeForCurrentThread(),
            TagViolationReportingMode::kSynchronous);
}
#endif  // BUILDFLAG(IS_ANDROID)

TEST(PartitionAllocMemoryTaggingTest, ChangeMemoryTaggingModeForCurrentThread) {
  base::CPU cpu;
  // If the underlying platform does not support MTE, skip this test to avoid
  // hiding failures.
  if (!cpu.has_mte()) {
    GTEST_SKIP();
  }

  TagViolationReportingMode original_mode =
      GetMemoryTaggingModeForCurrentThread();

  ChangeMemoryTaggingModeForCurrentThread(TagViolationReportingMode::kDisabled);
  EXPECT_EQ(GetMemoryTaggingModeForCurrentThread(),
            TagViolationReportingMode::kDisabled);
  ChangeMemoryTaggingModeForCurrentThread(
      TagViolationReportingMode::kSynchronous);
  EXPECT_EQ(GetMemoryTaggingModeForCurrentThread(),
            TagViolationReportingMode::kSynchronous);
  ChangeMemoryTaggingModeForCurrentThread(
      TagViolationReportingMode::kAsynchronous);
  EXPECT_EQ(GetMemoryTaggingModeForCurrentThread(),
            TagViolationReportingMode::kAsynchronous);
  ChangeMemoryTaggingModeForCurrentThread(
      TagViolationReportingMode::kSynchronous);
  EXPECT_EQ(GetMemoryTaggingModeForCurrentThread(),
            TagViolationReportingMode::kSynchronous);
  ChangeMemoryTaggingModeForCurrentThread(TagViolationReportingMode::kDisabled);
  EXPECT_EQ(GetMemoryTaggingModeForCurrentThread(),
            TagViolationReportingMode::kDisabled);
  ChangeMemoryTaggingModeForCurrentThread(
      TagViolationReportingMode::kAsynchronous);
  EXPECT_EQ(GetMemoryTaggingModeForCurrentThread(),
            TagViolationReportingMode::kAsynchronous);

  // Restore mode to original.
  ChangeMemoryTaggingModeForCurrentThread(original_mode);
}
#endif  // PA_CONFIG(HAS_MEMORY_TAGGING)

}  // namespace partition_alloc::internal
