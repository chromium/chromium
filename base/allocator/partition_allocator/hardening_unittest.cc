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

#if defined(GTEST_HAS_DEATH_TEST) && defined(PA_HAS_FREELIST_HARDENING)

TEST(HardeningTest, PartialCorruption) {
  std::string important_data("very important");
  char* to_corrupt = const_cast<char*>(important_data.c_str());

  PartitionRoot<base::internal::ThreadSafe> root{
      PartitionOptions{PartitionOptions::AlignedAlloc::kAllowed,
                       PartitionOptions::ThreadCache::kDisabled,
                       PartitionOptions::Quarantine::kDisallowed,
                       PartitionOptions::Cookies::kDisallowed,
                       PartitionOptions::RefCount::kDisallowed}};

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

// When DCHECK_IS_ON(), the freelist entries are checked, making this test
// crash earlier.
#if !DCHECK_IS_ON()
TEST(HardeningTest, CorruptionStillCrashing) {
  std::string important_data("very important");
  char* to_corrupt = const_cast<char*>(important_data.c_str());

  PartitionRoot<base::internal::ThreadSafe> root{
      PartitionOptions{PartitionOptions::AlignedAlloc::kAllowed,
                       PartitionOptions::ThreadCache::kDisabled,
                       PartitionOptions::Quarantine::kDisallowed,
                       PartitionOptions::Cookies::kDisallowed,
                       PartitionOptions::RefCount::kDisallowed}};

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

  void* new_data = root.Alloc(kAllocSize, "");
  ASSERT_EQ(new_data, data);

  // This is still crashing, because |*to_corrupt| is not properly formatted as
  // a freelist entry, in particular its second pointer is invalid.
  EXPECT_DEATH(root.Alloc(kAllocSize, ""), "");

  root.Free(new_data);
}
#endif  // !DCHECK_IS_ON()
#endif  // defined(GTEST_HAS_DEATH_TEST) && defined(PA_HAS_FREELIST_HARDENING)

#if !DCHECK_IS_ON()
TEST(HardeningTest, SuccessfulCorruption) {
  std::vector<char> v(100);
  char* to_corrupt = const_cast<char*>(&v[0]);

  PartitionRoot<base::internal::ThreadSafe> root{
      PartitionOptions{PartitionOptions::AlignedAlloc::kAllowed,
                       PartitionOptions::ThreadCache::kDisabled,
                       PartitionOptions::Quarantine::kDisallowed,
                       PartitionOptions::Cookies::kDisallowed,
                       PartitionOptions::RefCount::kDisallowed}};

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
  // Now we have an off-heap pointer returned by a heap allocation.
  EXPECT_EQ(new_data2, to_corrupt);
}
#endif  // !DCHECK_IS_ON()

}  // namespace internal
}  // namespace base

#endif  // !defined(MEMORY_TOOL_REPLACES_ALLOCATOR)
