// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>
#include <string>
#include <vector>

#include "partition_alloc/build_config.h"
#include "partition_alloc/partition_alloc_config.h"
#include "partition_alloc/partition_freelist_entry.h"
#include "partition_alloc/partition_page.h"
#include "partition_alloc/partition_root.h"
#include "partition_alloc/use_death_tests.h"
#include "testing/gtest/include/gtest/gtest.h"

// With *SAN, PartitionAlloc is rerouted to malloc().
#if !defined(MEMORY_TOOL_REPLACES_ALLOCATOR)

namespace partition_alloc::internal {
namespace {

#if PA_USE_DEATH_TESTS() && PA_CONFIG(HAS_FREELIST_SHADOW_ENTRY)

TEST(HardeningTest, PartialCorruption) {
  std::string important_data("very important");
  char* to_corrupt = const_cast<char*>(important_data.c_str());

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
  root.get_freelist_dispatcher()->EmplaceAndInitForTest(
      root.ObjectToSlotStart(data), to_corrupt, false);

  EXPECT_DEATH(root.Alloc(kAllocSize), "");
}

TEST(HardeningTest, OffHeapPointerCrashing) {
  std::string important_data("very important");
  char* to_corrupt = const_cast<char*>(important_data.c_str());

  PartitionOptions opts;
  PartitionRoot root(opts);
  root.UncapEmptySlotSpanMemoryForTesting();

  const size_t kAllocSize = 100;
  void* data = root.Alloc(kAllocSize);
  void* data2 = root.Alloc(kAllocSize);
  root.Free(data2);
  root.Free(data);

  // See "PartialCorruption" above for details. This time, make shadow_
  // consistent.
  root.get_freelist_dispatcher()->EmplaceAndInitForTest(
      root.ObjectToSlotStart(data), to_corrupt, true);

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

  uintptr_t slot_start = root.ObjectToSlotStart(data);
  auto* metadata =
      SlotSpanMetadata<MetadataKind::kReadOnly>::FromSlotStart(slot_start);

  root.get_freelist_dispatcher()->EmplaceAndInitForTest(slot_start, metadata,
                                                        true);

  // Crashes, because |metadata| points inside the metadata area.
  EXPECT_DEATH(root.Alloc(kAllocSize), "");
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

  root.get_freelist_dispatcher()->EmplaceAndInitForTest(
      root.ObjectToSlotStartUnchecked(data), to_corrupt, true);

#if PA_BUILDFLAG(USE_FREESLOT_BITMAP)
  // This part crashes with freeslot bitmap because it detects freelist
  // corruptions, which is rather desirable behavior.
  EXPECT_DEATH_IF_SUPPORTED(root.Alloc(kAllocSize), "");
#else
  // Next allocation is what was in
  // root->bucket->active_slot_span_head->freelist_head, so not the corrupted
  // pointer.
  void* new_data = root.Alloc(kAllocSize);
  ASSERT_EQ(new_data, data);

#if !PA_CONFIG(ENFORCE_SLOT_STARTS)
  // Not crashing, because a zeroed area is a "valid" freelist entry.
  void* new_data2 = root.Alloc(kAllocSize);
  // Now we have a pointer to the middle of an existing allocation.
  EXPECT_EQ(new_data2, to_corrupt);
#else
  // When `SlotStart` enforcement is on, `AllocInternalNoHooks()` will
  // call `SlotStartToObject()` and `CHECK()` that it's a slot start.
  EXPECT_DEATH_IF_SUPPORTED(root.Alloc(kAllocSize), "");
#endif  // !PA_CONFIG(ENFORCE_SLOT_STARTS)
#endif  // PA_BUILDFLAG(USE_FREESLOT_BITMAP)
}
#endif  // !PA_BUILDFLAG(IS_ANDROID)

#if PA_BUILDFLAG(USE_FREELIST_DISPATCHER)
#if PA_USE_DEATH_TESTS() && PA_CONFIG(HAS_FREELIST_SHADOW_ENTRY)
TEST(HardeningTest, ConstructPoolOffsetFromStackPointerCrashing) {
  int num_to_corrupt = 12345;
  int* to_corrupt = &num_to_corrupt;

  PartitionOptions opts;
  opts.use_pool_offset_freelists = PartitionOptions::kEnabled;
  PartitionRoot root(opts);
  root.UncapEmptySlotSpanMemoryForTesting();

  const size_t kAllocSize = 100;
  void* data = root.Alloc(kAllocSize);

  EXPECT_DEATH(root.get_freelist_dispatcher()->EmplaceAndInitForTest(
                   root.ObjectToSlotStart(data), to_corrupt, true),
               "");
}

TEST(HardeningTest, PoolOffsetMetadataPointerCrashing) {
  PartitionOptions opts;
  opts.use_pool_offset_freelists = PartitionOptions::kEnabled;
  PartitionRoot root(opts);
  root.UncapEmptySlotSpanMemoryForTesting();

  const size_t kAllocSize = 100;
  void* data = root.Alloc(kAllocSize);
  void* data2 = root.Alloc(kAllocSize);
  root.Free(data2);
  root.Free(data);

  uintptr_t slot_start = root.ObjectToSlotStart(data);
  auto* metadata =
      SlotSpanMetadata<MetadataKind::kReadOnly>::FromSlotStart(slot_start);

  root.get_freelist_dispatcher()->EmplaceAndInitForTest(slot_start, metadata,
                                                        true);

  // Crashes, because |metadata| points inside the metadata area.
  EXPECT_DEATH(root.Alloc(kAllocSize), "");
}
#endif  // PA_USE_DEATH_TESTS() && PA_CONFIG(HAS_FREELIST_SHADOW_ENTRY)

#if !PA_BUILDFLAG(IS_ANDROID)

TEST(HardeningTest, PoolOffsetSuccessfulCorruption) {
  PartitionOptions opts;
  opts.use_pool_offset_freelists = PartitionOptions::kEnabled;
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

  root.get_freelist_dispatcher()->EmplaceAndInitForTest(
      root.ObjectToSlotStart(data), to_corrupt, true);

#if PA_BUILDFLAG(USE_FREESLOT_BITMAP)
  // This part crashes with freeslot bitmap because it detects freelist
  // corruptions, which is rather desirable behavior.
  EXPECT_DEATH_IF_SUPPORTED(root.Alloc(kAllocSize), "");
#else
  // Next allocation is what was in
  // root->bucket->active_slot_span_head->freelist_head, so not the corrupted
  // pointer.
  void* new_data = root.Alloc(kAllocSize);
  ASSERT_EQ(new_data, data);

#if !PA_CONFIG(ENFORCE_SLOT_STARTS)

  // Not crashing, because a zeroed area is a "valid" freelist entry.
  void* new_data2 = root.Alloc(kAllocSize);
  // Now we have a pointer to the middle of an existing allocation.
  EXPECT_EQ(new_data2, to_corrupt);

#else

  // When `SlotStart` enforcement is on, `AllocInternalNoHooks()` will
  // call `SlotStartToObject()` and `CHECK()` that it's a slot start.
  EXPECT_DEATH_IF_SUPPORTED(root.Alloc(kAllocSize), "");

#endif  // !PA_CONFIG(ENFORCE_SLOT_STARTS)

#endif  // PA_BUILDFLAG(USE_FREESLOT_BITMAP)
}
#endif  // !PA_BUILDFLAG(IS_ANDROID)
#endif  // PA_BUILDFLAG(USE_FREELIST_DISPATCHER)
}  // namespace
}  // namespace partition_alloc::internal

#endif  // !defined(MEMORY_TOOL_REPLACES_ALLOCATOR)
