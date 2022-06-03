// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/allocator/partition_allocator/partition_alloc_config.h"
#include "base/allocator/partition_allocator/partition_freelist_entry.h"
#include "base/allocator/partition_allocator/partition_root.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

// With *SAN, PartitionAlloc is rerouted to malloc().
#if !defined(MEMORY_TOOL_REPLACES_ALLOCATOR)

namespace base {
namespace internal {

// Death tests misbehave on Android, crbug.com/1240184
#if !defined(OS_ANDROID) && defined(GTEST_HAS_DEATH_TEST) && \
    defined(PA_HAS_FREELIST_HARDENING)

TEST(HardeningTest, PartialCorruption) {
  std::string important_data("very important");
  char* to_corrupt = const_cast<char*>(important_data.c_str());

  PartitionRoot<base::internal::ThreadSafe> root{
      PartitionOptions{PartitionOptions::AlignedAlloc::kAllowed,
                       PartitionOptions::ThreadCache::kDisabled,
                       PartitionOptions::Quarantine::kDisallowed,
                       PartitionOptions::Cookie::kDisallowed,
                       PartitionOptions::BackupRefPtr::kDisabled,
                       PartitionOptions::UseConfigurablePool::kNo,
                       PartitionOptions::LazyCommit::kEnabled}};
  root.UncapEmptySlotSpanMemoryForTesting();

  const size_t kAllocSize = 100;
  void* data = root.Alloc(kAllocSize, "");
  void* data2 = root.Alloc(kAllocSize, "");
  root.Free(data2);
  root.Free(data);

  // root->bucket->active_slot_span_head->freelist_head is data, next is data2.
  // We can corrupt *data to get a new pointer next.

  // However even if the freelist entry looks reasonable (valid encoded
  // pointer), freelist corruption detection will make the code crash.
  *reinterpret_cast<EncodedPartitionFreelistEntry**>(data) =
      PartitionFreelistEntry::Encode(
          reinterpret_cast<PartitionFreelistEntry*>(to_corrupt));
  EXPECT_DEATH(root.Alloc(kAllocSize, ""), "");
}

TEST(HardeningTest, OffHeapPointerCrashing) {
  std::string important_data("very important");
  char* to_corrupt = const_cast<char*>(important_data.c_str());

  PartitionRoot<base::internal::ThreadSafe> root{
      PartitionOptions{PartitionOptions::AlignedAlloc::kAllowed,
                       PartitionOptions::ThreadCache::kDisabled,
                       PartitionOptions::Quarantine::kDisallowed,
                       PartitionOptions::Cookie::kDisallowed,
                       PartitionOptions::BackupRefPtr::kDisabled,
                       PartitionOptions::UseConfigurablePool::kNo,
                       PartitionOptions::LazyCommit::kEnabled}};
  root.UncapEmptySlotSpanMemoryForTesting();

  const size_t kAllocSize = 100;
  void* data = root.Alloc(kAllocSize, "");
  void* data2 = root.Alloc(kAllocSize, "");
  root.Free(data2);
  root.Free(data);

  // See "PartialCorruption" above for details.
  uintptr_t* data_ptr = reinterpret_cast<uintptr_t*>(data);
  *data_ptr = reinterpret_cast<uintptr_t>(PartitionFreelistEntry::Encode(
      reinterpret_cast<PartitionFreelistEntry*>(to_corrupt)));
  // This time, make the second pointer consistent.
  *(data_ptr + 1) = ~(*data_ptr);

  // Crashes, because |to_corrupt| is not on the same superpage as data.
  EXPECT_DEATH(root.Alloc(kAllocSize, ""), "");
}

TEST(HardeningTest, MetadataPointerCrashing) {
  PartitionRoot<base::internal::ThreadSafe> root{
      PartitionOptions{PartitionOptions::AlignedAlloc::kAllowed,
                       PartitionOptions::ThreadCache::kDisabled,
                       PartitionOptions::Quarantine::kDisallowed,
                       PartitionOptions::Cookie::kDisallowed,
                       PartitionOptions::BackupRefPtr::kDisabled,
                       PartitionOptions::UseConfigurablePool::kNo,
                       PartitionOptions::LazyCommit::kEnabled}};
  root.UncapEmptySlotSpanMemoryForTesting();

  const size_t kAllocSize = 100;
  void* data = root.Alloc(kAllocSize, "");
  void* data2 = root.Alloc(kAllocSize, "");
  root.Free(data2);
  root.Free(data);

  uintptr_t data_address = reinterpret_cast<uintptr_t>(data);
  uintptr_t metadata_address =
      data_address & kSuperPageBaseMask + SystemPageSize();

  uintptr_t* data_ptr = reinterpret_cast<uintptr_t*>(data);
  *data_ptr = reinterpret_cast<uintptr_t>(PartitionFreelistEntry::Encode(
      reinterpret_cast<PartitionFreelistEntry*>(metadata_address)));
  // This time, make the second pointer consistent.
  *(data_ptr + 1) = ~(*data_ptr);

  // Crashes, because |metadata_address| points inside the metadata area.
  EXPECT_DEATH(root.Alloc(kAllocSize, ""), "");
}
#endif  // !defined(OS_ANDROID) && defined(GTEST_HAS_DEATH_TEST) &&
        // defined(PA_HAS_FREELIST_HARDENING)

TEST(HardeningTest, SuccessfulCorruption) {
  PartitionRoot<base::internal::ThreadSafe> root{
      PartitionOptions{PartitionOptions::AlignedAlloc::kAllowed,
                       PartitionOptions::ThreadCache::kDisabled,
                       PartitionOptions::Quarantine::kDisallowed,
                       PartitionOptions::Cookie::kDisallowed,
                       PartitionOptions::BackupRefPtr::kDisabled,
                       PartitionOptions::UseConfigurablePool::kNo,
                       PartitionOptions::LazyCommit::kEnabled}};
  root.UncapEmptySlotSpanMemoryForTesting();

  uintptr_t* zero_vector = reinterpret_cast<uintptr_t*>(
      root.AllocFlags(PartitionAllocZeroFill, 100 * sizeof(uintptr_t), ""));
  ASSERT_TRUE(zero_vector);
  // Pointer to the middle of an existing allocation.
  uintptr_t* to_corrupt = zero_vector + 20;

  const size_t kAllocSize = 100;
  void* data = root.Alloc(kAllocSize, "");
  void* data2 = root.Alloc(kAllocSize, "");
  root.Free(data2);
  root.Free(data);

  uintptr_t* data_ptr = reinterpret_cast<uintptr_t*>(data);
  *data_ptr = reinterpret_cast<uintptr_t>(PartitionFreelistEntry::Encode(
      reinterpret_cast<PartitionFreelistEntry*>(to_corrupt)));
  // If we don't have PA_HAS_FREELIST_HARDENING, this is not needed.
  *(data_ptr + 1) = ~(*data_ptr);

  // Next allocation is what was in
  // root->bucket->active_slot_span_head->freelist_head, so not the corrupted
  // pointer.
  void* new_data = root.Alloc(kAllocSize, "");
  ASSERT_EQ(new_data, data);

  // Not crashing, because a zeroed area is a "valid" freelist entry.
  void* new_data2 = root.Alloc(kAllocSize, "");
  // Now we have a pointer to the middle of an existing allocation.
  EXPECT_EQ(new_data2, to_corrupt);
}

}  // namespace internal
}  // namespace base

#endif  // !defined(MEMORY_TOOL_REPLACES_ALLOCATOR)
