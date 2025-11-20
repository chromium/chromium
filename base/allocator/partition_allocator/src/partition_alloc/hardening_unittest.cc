// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include <cstdint>
#include <string>
#include <vector>

#include "partition_alloc/allocator_config.h"
#include "partition_alloc/build_config.h"
#include "partition_alloc/partition_alloc_config.h"
#include "partition_alloc/partition_alloc_for_testing.h"
#include "partition_alloc/partition_freelist_entry.h"
#include "partition_alloc/partition_page.h"
#include "partition_alloc/partition_root.h"
#include "partition_alloc/slot_start.h"
#include "partition_alloc/use_death_tests.h"
#include "testing/gtest/include/gtest/gtest.h"

// With *SAN, PartitionAlloc is rerouted to malloc().
#if !defined(MEMORY_TOOL_REPLACES_ALLOCATOR)

namespace partition_alloc::internal {
namespace {

#if PA_USE_DEATH_TESTS() && PA_CONFIG(HAS_FREELIST_SHADOW_ENTRY)

TEST(HardeningTest, PartialCorruption) {
  PartitionOptions opts;
  PartitionRoot root(opts);
  root.UncapEmptySlotSpanMemoryForTesting();

  const size_t kAllocSize = 100;
  void* data = root.Alloc(kAllocSize);
  void* data2 = root.Alloc(kAllocSize);
  root.Free(data2);
  root.Free(data);

  // root->bucket->active_slot_span_head->freelist_head points to data, next_
  // points to data2. We can corrupt *data to overwrite the next_ pointer.
  // Even if it looks reasonable (valid encoded pointer), freelist corruption
  // detection will make the code crash, because shadow_ doesn't match
  // encoded_next_.
  FreelistEntry::EmplaceAndInitForTest(
      SlotStart::Checked(data, &root).Untag().value(), data, false);

  EXPECT_DEATH(root.Alloc(kAllocSize), "");
}

TEST(HardeningTest, OffHeapPointerCrashing) {
  PartitionOptions opts;
  PartitionRoot root(opts);
  root.UncapEmptySlotSpanMemoryForTesting();

  const size_t kAllocSize = 100;
  void* data = root.Alloc(kAllocSize);
  void* data2 = root.Alloc(kAllocSize);
  root.Free(data2);
  root.Free(data);

  void* different_superpage = root.Alloc(1 << 20);

  // See "PartialCorruption" above for details. This time, make shadow_
  // consistent.
  FreelistEntry::EmplaceAndInitForTest(
      SlotStart::Checked(data, &root).Untag().value(), different_superpage,
      true);

  // Crashes, because |to_corrupt| is not on the same superpage as data.
  EXPECT_DEATH(root.Alloc(kAllocSize), "");
}

TEST(HardeningTest, MetadataPointerCrashing) {
  PartitionOptions opts;
  PartitionRoot root(opts);
  root.UncapEmptySlotSpanMemoryForTesting();

  const size_t kAllocSize = 100;
  void* data = root.Alloc(kAllocSize);
  void* data2 = root.Alloc(kAllocSize);
  root.Free(data2);
  root.Free(data);

  UntaggedSlotStart slot_start = SlotStart::Checked(data, &root).Untag();
  auto* metadata = SlotSpanMetadata::FromSlotStart(slot_start, &root);

#if PA_CONFIG(MOVE_METADATA_OUT_OF_GIGACAGE)
#if PA_BUILDFLAG(ENABLE_MOVE_METADATA_OUT_OF_GIGACAGE_TRIAL)
  // If the feature is enabled with synthetic trial, a new process for death
  // test might have different configuration from the current process'. It
  // causes EXPECT_DEATH() with unexpected exit code.
  GTEST_SKIP() << "Skipping MetadataPointerCrashing because of PartitionAlloc "
                  "External Metadata trial.";
#endif  // PA_BUILDFLAG(ENABLE_MOVE_METADATA_OUT_OF_GIGACAGE_TRIAL)
  EXPECT_DEATH(
      FreelistEntry::EmplaceAndInitForTest(slot_start.value(), metadata, true),
      "");
#else   // PA_CONFIG(MOVE_METADATA_OUT_OF_GIGACAGE)
  FreelistEntry::EmplaceAndInitForTest(slot_start.value(), metadata, true);

  // Crashes, because |metadata| points inside the metadata area.
  EXPECT_DEATH(root.Alloc(kAllocSize), "");
#endif  // PA_CONFIG(MOVE_METADATA_OUT_OF_GIGACAGE)
}
#endif  // PA_USE_DEATH_TESTS() && PA_CONFIG(HAS_FREELIST_SHADOW_ENTRY)

// Below test also misbehaves on Android; as above, death tests don't
// quite work (crbug.com/1240184), and having free slot bitmaps enabled
// force the expectations below to crash.
#if !PA_BUILDFLAG(IS_ANDROID)

TEST(HardeningTest, SuccessfulCorruption) {
  PartitionOptions opts;
  PartitionRoot root(opts);
  root.UncapEmptySlotSpanMemoryForTesting();

  uintptr_t* zero_vector = reinterpret_cast<uintptr_t*>(
      root.Alloc<AllocFlags::kZeroFill>(100 * sizeof(uintptr_t), ""));
  ASSERT_TRUE(zero_vector);
  // Pointer to the middle of an existing allocation.
  uintptr_t* to_corrupt = zero_vector + 20;

  const size_t kAllocSize = 100;
  void* data = root.Alloc(kAllocSize);
  void* data2 = root.Alloc(kAllocSize);
  root.Free(data2);
  root.Free(data);

  FreelistEntry::EmplaceAndInitForTest(SlotStart::Unchecked(data).value(),
                                       to_corrupt, true);

  // Next allocation is what was in
  // root->bucket->active_slot_span_head->freelist_head, so not the corrupted
  // pointer.
  void* new_data = root.Alloc(kAllocSize);
  ASSERT_EQ(new_data, data);

#if !PA_BUILDFLAG(DCHECKS_ARE_ON)
  // Not crashing, because a zeroed area is a "valid" freelist entry.
  void* new_data2 = root.Alloc(kAllocSize);
  // Now we have a pointer to the middle of an existing allocation.
  EXPECT_EQ(new_data2, to_corrupt);
#else
  // When `SlotStart` enforcement is on, `AllocInternalNoHooks()` will
  // call `SlotStart::ToObject()` and `CHECK()` that it's a slot start.
  EXPECT_DEATH_IF_SUPPORTED(root.Alloc(kAllocSize), "");
#endif  // !PA_BUILDFLAG(DCHECKS_ARE_ON)
}
#endif  // !PA_BUILDFLAG(IS_ANDROID)

#if PA_BUILDFLAG(HAS_64_BIT_POINTERS)
#if PA_USE_DEATH_TESTS() && PA_CONFIG(HAS_FREELIST_SHADOW_ENTRY)
TEST(HardeningTest, ConstructPoolOffsetFromStackPointerCrashing) {
  int num_to_corrupt = 12345;
  int* to_corrupt = &num_to_corrupt;

  PartitionRoot root(PartitionOptions{});
  root.UncapEmptySlotSpanMemoryForTesting();

  const size_t kAllocSize = 100;
  void* data = root.Alloc(kAllocSize);

  EXPECT_DEATH(
      FreelistEntry::EmplaceAndInitForTest(
          SlotStart::Checked(data, &root).Untag().value(), to_corrupt, true),
      "");
}

TEST(HardeningTest, PoolOffsetMetadataPointerCrashing) {
  PartitionRoot root(PartitionOptions{});
  root.UncapEmptySlotSpanMemoryForTesting();

  const size_t kAllocSize = 100;
  void* data = root.Alloc(kAllocSize);
  void* data2 = root.Alloc(kAllocSize);
  root.Free(data2);
  root.Free(data);

  UntaggedSlotStart slot_start = SlotStart::Checked(data, &root).Untag();
  auto* metadata = SlotSpanMetadata::FromSlotStart(slot_start, &root);

#if PA_CONFIG(MOVE_METADATA_OUT_OF_GIGACAGE)
#if PA_BUILDFLAG(ENABLE_MOVE_METADATA_OUT_OF_GIGACAGE_TRIAL)
  // If the feature is enabled with synthetic trial, a new process for death
  // test might have different configuration from the current process'. It
  // causes EXPECT_DEATH() with unexpected exit code.
  GTEST_SKIP() << "Skipping MetadataPointerCrashing because of PartitionAlloc "
                  "External Metadata trial.";
#endif  // PA_BUILDFLAG(ENABLE_MOVE_METADATA_OUT_OF_GIGACAGE_TRIAL)
  EXPECT_DEATH(
      FreelistEntry::EmplaceAndInitForTest(slot_start.value(), metadata, true),
      "");
#else   // PA_CONFIG(MOVE_METADATA_OUT_OF_GIGACAGE)
  FreelistEntry::EmplaceAndInitForTest(slot_start, metadata, true);

  // Crashes, because |metadata| points inside the metadata area.
  EXPECT_DEATH(root.Alloc(kAllocSize), "");
#endif  // !PA_CONFIG(MOVE_METADATA_OUT_OF_GIGACAGE)
}
#endif  // PA_USE_DEATH_TESTS() && PA_CONFIG(HAS_FREELIST_SHADOW_ENTRY)

#if !PA_BUILDFLAG(IS_ANDROID)

TEST(HardeningTest, PoolOffsetSuccessfulCorruption) {
  PartitionRoot root(PartitionOptions{});
  root.UncapEmptySlotSpanMemoryForTesting();

  uintptr_t* zero_vector = reinterpret_cast<uintptr_t*>(
      root.Alloc<AllocFlags::kZeroFill>(100 * sizeof(uintptr_t), ""));
  ASSERT_TRUE(zero_vector);
  // Pointer to the middle of an existing allocation.
  uintptr_t* to_corrupt = zero_vector + 20;

  const size_t kAllocSize = 100;
  void* data = root.Alloc(kAllocSize);
  void* data2 = root.Alloc(kAllocSize);
  root.Free(data2);
  root.Free(data);

  FreelistEntry::EmplaceAndInitForTest(
      SlotStart::Checked(data, &root).Untag().value(), to_corrupt, true);

  // Next allocation is what was in
  // root->bucket->active_slot_span_head->freelist_head, so not the corrupted
  // pointer.
  void* new_data = root.Alloc(kAllocSize);
  ASSERT_EQ(new_data, data);

#if !PA_BUILDFLAG(DCHECKS_ARE_ON)

  // Not crashing, because a zeroed area is a "valid" freelist entry.
  void* new_data2 = root.Alloc(kAllocSize);
  // Now we have a pointer to the middle of an existing allocation.
  EXPECT_EQ(new_data2, to_corrupt);

#else

  // When `SlotStart` enforcement is on, `AllocInternalNoHooks()` will
  // call `SlotStart::ToObject()` and `CHECK()` that it's a slot start.
  EXPECT_DEATH_IF_SUPPORTED(root.Alloc(kAllocSize), "");

#endif  // !PA_BUILDFLAG(DCHECKS_ARE_ON)
}
#endif  // !PA_BUILDFLAG(IS_ANDROID)
#endif  // PA_BUILDFLAG(HAS_64_BIT_POINTERS)
}  // namespace
}  // namespace partition_alloc::internal

#endif  // !defined(MEMORY_TOOL_REPLACES_ALLOCATOR)
