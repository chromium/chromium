// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/partition_alloc.h"

#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <cstddef>
#include <limits>
#include <memory>
#include <vector>

#include "base/allocator/buildflags.h"
#include "base/allocator/partition_allocator/address_space_randomization.h"
#include "base/allocator/partition_allocator/page_allocator_constants.h"
#include "base/allocator/partition_allocator/partition_address_space.h"
#include "base/allocator/partition_allocator/partition_alloc_constants.h"
#include "base/allocator/partition_allocator/partition_alloc_features.h"
#include "base/allocator/partition_allocator/partition_cookie.h"
#include "base/allocator/partition_allocator/partition_freelist_entry.h"
#include "base/allocator/partition_allocator/partition_page.h"
#include "base/allocator/partition_allocator/partition_ref_count.h"
#include "base/allocator/partition_allocator/partition_root.h"
#include "base/allocator/partition_allocator/reservation_offset_table.h"
#include "base/bits.h"
#include "base/cxx17_backports.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "base/system/sys_info.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "build/chromecast_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_POSIX)
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/time.h>
#endif  // defined(OS_POSIX)

#if !defined(MEMORY_TOOL_REPLACES_ALLOCATOR)

namespace {

bool IsLargeMemoryDevice() {
  // Treat any device with 4GiB or more of physical memory as a "large memory
  // device". We check for slightly less than GiB so that devices with a small
  // amount of memory not accessible to the OS still count as "large".
  //
  // Set to 4GiB, since we have 2GiB Android devices where tests flakily fail
  // (e.g. Nexus 5X, crbug.com/1191195).
  return base::SysInfo::AmountOfPhysicalMemory() >= 4000LL * 1024 * 1024;
}

bool SetAddressSpaceLimit() {
#if !defined(ARCH_CPU_64_BITS) || !defined(OS_POSIX)
  // 32 bits => address space is limited already.
  return true;
#elif defined(OS_POSIX) && !defined(OS_APPLE)
  // macOS will accept, but not enforce, |RLIMIT_AS| changes. See
  // https://crbug.com/435269 and rdar://17576114.
  //
  // Note: This number must be not less than 6 GB, because with
  // sanitizer_coverage_flags=edge, it reserves > 5 GB of address space. See
  // https://crbug.com/674665.
  const size_t kAddressSpaceLimit = static_cast<size_t>(6144) * 1024 * 1024;
  struct rlimit limit;
  if (getrlimit(RLIMIT_DATA, &limit) != 0)
    return false;
  if (limit.rlim_cur == RLIM_INFINITY || limit.rlim_cur > kAddressSpaceLimit) {
    limit.rlim_cur = kAddressSpaceLimit;
    if (setrlimit(RLIMIT_DATA, &limit) != 0)
      return false;
  }
  return true;
#else
  return false;
#endif
}

bool ClearAddressSpaceLimit() {
#if !defined(ARCH_CPU_64_BITS) || !defined(OS_POSIX)
  return true;
#elif defined(OS_POSIX)
  struct rlimit limit;
  if (getrlimit(RLIMIT_DATA, &limit) != 0)
    return false;
  limit.rlim_cur = limit.rlim_max;
  if (setrlimit(RLIMIT_DATA, &limit) != 0)
    return false;
  return true;
#else
  return false;
#endif
}

const size_t kTestSizes[] = {
    1,
    17,
    100,
    base::SystemPageSize(),
    base::SystemPageSize() + 1,
    base::PartitionRoot<base::internal::ThreadSafe>::GetDirectMapSlotSize(100),
    1 << 20,
    1 << 21,
};
constexpr size_t kTestSizesCount = base::size(kTestSizes);

void AllocateRandomly(base::PartitionRoot<base::internal::ThreadSafe>* root,
                      size_t count,
                      int flags) {
  std::vector<void*> allocations(count, nullptr);
  for (size_t i = 0; i < count; ++i) {
    const size_t size = kTestSizes[base::RandGenerator(kTestSizesCount)];
    allocations[i] = root->AllocFlags(flags, size, nullptr);
    EXPECT_NE(nullptr, allocations[i]) << " size: " << size << " i: " << i;
  }

  for (size_t i = 0; i < count; ++i) {
    if (allocations[i])
      root->Free(allocations[i]);
  }
}

void HandleOOM(size_t unused_size) {
  LOG(FATAL) << "Out of memory";
}

}  // namespace

namespace base {

// NOTE: Though this test actually excercises interfaces inside the ::base
// namespace, the unittest is inside the ::base::internal spaces because a
// portion of the test expectations require inspecting objects and behavior
// in the ::base::internal namespace. An alternate formulation would be to
// explicitly add using statements for each inspected type but this felt more
// readable.
namespace internal {

using SlotSpan = SlotSpanMetadata<ThreadSafe>;

const size_t kTestAllocSize = 16;
#if !DCHECK_IS_ON()
const size_t kPointerOffset = kPartitionRefCountOffsetAdjustment;
const size_t kExtraAllocSize = kInSlotRefCountBufferSize;
#else
const size_t kPointerOffset = kCookieSize + kPartitionRefCountOffsetAdjustment;
const size_t kExtraAllocSize = kCookieSize * 2 + kInSlotRefCountBufferSize;
#endif
const size_t kRealAllocSize =
    bits::AlignUp(kTestAllocSize + kExtraAllocSize, kAlignment);

const char* type_name = nullptr;

class ScopedPageAllocation {
 public:
  ScopedPageAllocation(
      PartitionAllocator<base::internal::ThreadSafe>& allocator,
      base::CheckedNumeric<size_t> npages)
      : allocator_(allocator),
        npages_(npages),
        ptr_(reinterpret_cast<char*>(allocator_.root()->Alloc(
            (npages * SystemPageSize() - kExtraAllocSize).ValueOrDie(),
            type_name))) {}

  ~ScopedPageAllocation() { allocator_.root()->Free(ptr_); }

  void TouchAllPages() {
    memset(ptr_, 'A',
           ((npages_ * SystemPageSize()) - kExtraAllocSize).ValueOrDie());
  }

  void* PageAtIndex(size_t index) {
    return ptr_ - kPointerOffset + (SystemPageSize() * index);
  }

 private:
  PartitionAllocator<base::internal::ThreadSafe>& allocator_;
  const base::CheckedNumeric<size_t> npages_;
  char* ptr_;
};

class PartitionAllocTest : public testing::Test {
 protected:
  PartitionAllocTest() = default;

  ~PartitionAllocTest() override = default;

  void SetUp() override {
    PartitionAllocGlobalInit(HandleOOM);
    allocator.init({PartitionOptions::AlignedAlloc::kDisallowed,
                    PartitionOptions::ThreadCache::kDisabled,
                    PartitionOptions::Quarantine::kDisallowed,
                    PartitionOptions::Cookies::kAllowed,
                    PartitionOptions::RefCount::kAllowed});
    aligned_allocator.init({PartitionOptions::AlignedAlloc::kAllowed,
                            PartitionOptions::ThreadCache::kDisabled,
                            PartitionOptions::Quarantine::kDisallowed,
                            PartitionOptions::Cookies::kDisallowed,
                            PartitionOptions::RefCount::kDisallowed});
    test_bucket_index_ = SizeToIndex(kRealAllocSize);
  }

  size_t SizeToIndex(size_t size) {
    return PartitionRoot<base::internal::ThreadSafe>::SizeToBucketIndex(size);
  }

  void TearDown() override {
    allocator.root()->PurgeMemory(PartitionPurgeDecommitEmptySlotSpans |
                                  PartitionPurgeDiscardUnusedSystemPages);
    PartitionAllocGlobalUninitForTesting();
  }

  size_t GetNumPagesPerSlotSpan(size_t size) {
    size_t real_size = size + kExtraAllocSize;
    size_t bucket_index = SizeToIndex(real_size);
    PartitionRoot<ThreadSafe>::Bucket* bucket =
        &allocator.root()->buckets[bucket_index];
    // TODO(tasak): make get_pages_per_slot_span() available at
    // partition_alloc_unittest.cc. Is it allowable to make the code from
    // partition_bucet.cc to partition_bucket.h?
    return (bucket->num_system_pages_per_slot_span +
            (NumSystemPagesPerPartitionPage() - 1)) /
           NumSystemPagesPerPartitionPage();
  }

  SlotSpan* GetFullSlotSpan(size_t size) {
    size_t real_size = size + kExtraAllocSize;
    size_t bucket_index = SizeToIndex(real_size);
    PartitionRoot<ThreadSafe>::Bucket* bucket =
        &allocator.root()->buckets[bucket_index];
    size_t num_slots =
        (bucket->num_system_pages_per_slot_span * SystemPageSize()) /
        bucket->slot_size;
    void* first = nullptr;
    void* last = nullptr;
    size_t i;
    for (i = 0; i < num_slots; ++i) {
      void* ptr = allocator.root()->Alloc(size, type_name);
      EXPECT_TRUE(ptr);
      if (!i)
        first = allocator.root()->AdjustPointerForExtrasSubtract(ptr);
      else if (i == num_slots - 1)
        last = allocator.root()->AdjustPointerForExtrasSubtract(ptr);
    }
    EXPECT_EQ(SlotSpan::FromSlotStartPtr(first),
              SlotSpan::FromSlotStartPtr(last));
    if (bucket->num_system_pages_per_slot_span ==
        NumSystemPagesPerPartitionPage())
      EXPECT_EQ(reinterpret_cast<size_t>(first) & PartitionPageBaseMask(),
                reinterpret_cast<size_t>(last) & PartitionPageBaseMask());
    EXPECT_EQ(num_slots,
              static_cast<size_t>(
                  bucket->active_slot_spans_head->num_allocated_slots));
    EXPECT_EQ(nullptr, bucket->active_slot_spans_head->freelist_head);
    EXPECT_TRUE(bucket->is_valid());
    EXPECT_TRUE(bucket->active_slot_spans_head !=
                SlotSpan::get_sentinel_slot_span());
    return bucket->active_slot_spans_head;
  }

  void CycleFreeCache(size_t size) {
    for (size_t i = 0; i < kMaxFreeableSpans; ++i) {
      void* ptr = allocator.root()->Alloc(size, type_name);
      auto* slot_span = SlotSpan::FromSlotStartPtr(
          allocator.root()->AdjustPointerForExtrasSubtract(ptr));
      auto* bucket = slot_span->bucket;
      EXPECT_EQ(1, bucket->active_slot_spans_head->num_allocated_slots);
      allocator.root()->Free(ptr);
      EXPECT_EQ(0, bucket->active_slot_spans_head->num_allocated_slots);
      EXPECT_NE(-1, bucket->active_slot_spans_head->empty_cache_index);
    }
  }

  enum ReturnNullTestMode {
    kPartitionAllocFlags,
    kPartitionReallocFlags,
    kPartitionRootTryRealloc,
  };

  void DoReturnNullTest(size_t alloc_size, ReturnNullTestMode mode) {
    // TODO(crbug.com/678782): Where necessary and possible, disable the
    // platform's OOM-killing behavior. OOM-killing makes this test flaky on
    // low-memory devices.
    if (!IsLargeMemoryDevice()) {
      LOG(WARNING)
          << "Skipping test on this device because of crbug.com/678782";
      LOG(FATAL) << "DoReturnNullTest";
    }

    ASSERT_TRUE(SetAddressSpaceLimit());

    // Work out the number of allocations for 6 GB of memory.
    const int num_allocations = (6 * 1024 * 1024) / (alloc_size / 1024);

    void** ptrs = reinterpret_cast<void**>(
        allocator.root()->Alloc(num_allocations * sizeof(void*), type_name));
    int i;

    for (i = 0; i < num_allocations; ++i) {
      switch (mode) {
        case kPartitionAllocFlags: {
          ptrs[i] = allocator.root()->AllocFlags(PartitionAllocReturnNull,
                                                 alloc_size, type_name);
          break;
        }
        case kPartitionReallocFlags: {
          ptrs[i] = allocator.root()->AllocFlags(PartitionAllocReturnNull, 1,
                                                 type_name);
          ptrs[i] = allocator.root()->ReallocFlags(
              PartitionAllocReturnNull, ptrs[i], alloc_size, type_name);
          break;
        }
        case kPartitionRootTryRealloc: {
          ptrs[i] = allocator.root()->AllocFlags(PartitionAllocReturnNull, 1,
                                                 type_name);
          ptrs[i] =
              allocator.root()->TryRealloc(ptrs[i], alloc_size, type_name);
        }
      }

      if (!i)
        EXPECT_TRUE(ptrs[0]);
      if (!ptrs[i]) {
        ptrs[i] = allocator.root()->AllocFlags(PartitionAllocReturnNull,
                                               alloc_size, type_name);
        EXPECT_FALSE(ptrs[i]);
        break;
      }
    }

    // We shouldn't succeed in allocating all 6 GB of memory. If we do, then
    // we're not actually testing anything here.
    EXPECT_LT(i, num_allocations);

    // Free, reallocate and free again each block we allocated. We do this to
    // check that freeing memory also works correctly after a failed allocation.
    for (--i; i >= 0; --i) {
      allocator.root()->Free(ptrs[i]);
      ptrs[i] = allocator.root()->AllocFlags(PartitionAllocReturnNull,
                                             alloc_size, type_name);
      EXPECT_TRUE(ptrs[i]);
      allocator.root()->Free(ptrs[i]);
    }

    allocator.root()->Free(ptrs);

    EXPECT_TRUE(ClearAddressSpaceLimit());
    LOG(FATAL) << "DoReturnNullTest";
  }

  void RunRefCountReallocSubtest(size_t orig_size, size_t new_size);

  base::test::ScopedFeatureList scoped_feature_list;
  PartitionAllocator<base::internal::ThreadSafe> allocator;
  PartitionAllocator<base::internal::ThreadSafe> aligned_allocator;
  size_t test_bucket_index_;
};

class PartitionAllocDeathTest : public PartitionAllocTest {};

namespace {

void FreeFullSlotSpan(PartitionRoot<base::internal::ThreadSafe>* root,
                      SlotSpan* slot_span) {
  size_t size = slot_span->bucket->slot_size;
  size_t num_slots =
      (slot_span->bucket->num_system_pages_per_slot_span * SystemPageSize()) /
      size;
  EXPECT_EQ(num_slots,
            static_cast<size_t>(std::abs(slot_span->num_allocated_slots)));
  char* ptr = reinterpret_cast<char*>(SlotSpan::ToSlotSpanStartPtr(slot_span));
  size_t i;
  for (i = 0; i < num_slots; ++i) {
    root->Free(ptr + kPointerOffset);
    ptr += size;
  }
}

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
bool CheckPageInCore(void* ptr, bool in_core) {
  unsigned char ret = 0;
  EXPECT_EQ(0, mincore(ptr, SystemPageSize(), &ret));
  return in_core == (ret & 1);
}

#define CHECK_PAGE_IN_CORE(ptr, in_core) \
  EXPECT_TRUE(CheckPageInCore(ptr, in_core))
#else
#define CHECK_PAGE_IN_CORE(ptr, in_core) (void)(0)
#endif  // defined(OS_LINUX) || defined(OS_CHROMEOS)

class MockPartitionStatsDumper : public PartitionStatsDumper {
 public:
  MockPartitionStatsDumper() = default;

  void PartitionDumpTotals(const char* partition_name,
                           const PartitionMemoryStats* stats) override {
    EXPECT_GE(stats->total_mmapped_bytes, stats->total_resident_bytes);
    EXPECT_EQ(total_resident_bytes, stats->total_resident_bytes);
    EXPECT_EQ(total_active_bytes, stats->total_active_bytes);
    EXPECT_EQ(total_decommittable_bytes, stats->total_decommittable_bytes);
    EXPECT_EQ(total_discardable_bytes, stats->total_discardable_bytes);
  }

  void PartitionsDumpBucketStats(
      const char* partition_name,
      const PartitionBucketMemoryStats* stats) override {
    (void)partition_name;
    EXPECT_TRUE(stats->is_valid);
    EXPECT_EQ(0u, stats->bucket_slot_size & sizeof(void*));
    bucket_stats.push_back(*stats);
    total_resident_bytes += stats->resident_bytes;
    total_active_bytes += stats->active_bytes;
    total_decommittable_bytes += stats->decommittable_bytes;
    total_discardable_bytes += stats->discardable_bytes;
  }

  bool IsMemoryAllocationRecorded() {
    return total_resident_bytes != 0 && total_active_bytes != 0;
  }

  const PartitionBucketMemoryStats* GetBucketStats(size_t bucket_size) {
    for (auto& stat : bucket_stats) {
      if (stat.bucket_slot_size == bucket_size)
        return &stat;
    }
    return nullptr;
  }

 private:
  size_t total_resident_bytes = 0;
  size_t total_active_bytes = 0;
  size_t total_decommittable_bytes = 0;
  size_t total_discardable_bytes = 0;

  std::vector<PartitionBucketMemoryStats> bucket_stats;
};

}  // namespace

// Check that the most basic of allocate / free pairs work.
TEST_F(PartitionAllocTest, Basic) {
  PartitionRoot<ThreadSafe>::Bucket* bucket =
      &allocator.root()->buckets[test_bucket_index_];
  auto* seed_slot_span = SlotSpan::get_sentinel_slot_span();

  EXPECT_FALSE(bucket->empty_slot_spans_head);
  EXPECT_FALSE(bucket->decommitted_slot_spans_head);
  EXPECT_EQ(seed_slot_span, bucket->active_slot_spans_head);
  EXPECT_EQ(nullptr, bucket->active_slot_spans_head->next_slot_span);

  void* ptr = allocator.root()->Alloc(kTestAllocSize, type_name);
  EXPECT_TRUE(ptr);
  EXPECT_EQ(kPointerOffset,
            reinterpret_cast<size_t>(ptr) & PartitionPageOffsetMask());
  // Check that the offset appears to include a guard page.
  EXPECT_EQ(PartitionPageSize() + kPointerOffset,
            reinterpret_cast<size_t>(ptr) & kSuperPageOffsetMask);

  allocator.root()->Free(ptr);
  // Expect that the last active slot span gets noticed as empty but doesn't get
  // decommitted.
  EXPECT_TRUE(bucket->empty_slot_spans_head);
  EXPECT_FALSE(bucket->decommitted_slot_spans_head);
}

// Test multiple allocations, and freelist handling.
TEST_F(PartitionAllocTest, MultiAlloc) {
  char* ptr1 = reinterpret_cast<char*>(
      allocator.root()->Alloc(kTestAllocSize, type_name));
  char* ptr2 = reinterpret_cast<char*>(
      allocator.root()->Alloc(kTestAllocSize, type_name));
  EXPECT_TRUE(ptr1);
  EXPECT_TRUE(ptr2);
  ptrdiff_t diff = ptr2 - ptr1;
  EXPECT_EQ(static_cast<ptrdiff_t>(kRealAllocSize), diff);

  // Check that we re-use the just-freed slot.
  allocator.root()->Free(ptr2);
  ptr2 = reinterpret_cast<char*>(
      allocator.root()->Alloc(kTestAllocSize, type_name));
  EXPECT_TRUE(ptr2);
  diff = ptr2 - ptr1;
  EXPECT_EQ(static_cast<ptrdiff_t>(kRealAllocSize), diff);
  allocator.root()->Free(ptr1);
  ptr1 = reinterpret_cast<char*>(
      allocator.root()->Alloc(kTestAllocSize, type_name));
  EXPECT_TRUE(ptr1);
  diff = ptr2 - ptr1;
  EXPECT_EQ(static_cast<ptrdiff_t>(kRealAllocSize), diff);

  char* ptr3 = reinterpret_cast<char*>(
      allocator.root()->Alloc(kTestAllocSize, type_name));
  EXPECT_TRUE(ptr3);
  diff = ptr3 - ptr1;
  EXPECT_EQ(static_cast<ptrdiff_t>(kRealAllocSize * 2), diff);

  allocator.root()->Free(ptr1);
  allocator.root()->Free(ptr2);
  allocator.root()->Free(ptr3);
}

// Test a bucket with multiple slot spans.
TEST_F(PartitionAllocTest, MultiSlotSpans) {
  PartitionRoot<ThreadSafe>::Bucket* bucket =
      &allocator.root()->buckets[test_bucket_index_];

  auto* slot_span = GetFullSlotSpan(kTestAllocSize);
  FreeFullSlotSpan(allocator.root(), slot_span);
  EXPECT_TRUE(bucket->empty_slot_spans_head);
  EXPECT_EQ(SlotSpan::get_sentinel_slot_span(), bucket->active_slot_spans_head);
  EXPECT_EQ(nullptr, slot_span->next_slot_span);
  EXPECT_EQ(0, slot_span->num_allocated_slots);

  slot_span = GetFullSlotSpan(kTestAllocSize);
  auto* slot_span2 = GetFullSlotSpan(kTestAllocSize);

  EXPECT_EQ(slot_span2, bucket->active_slot_spans_head);
  EXPECT_EQ(nullptr, slot_span2->next_slot_span);
  EXPECT_EQ(
      reinterpret_cast<uintptr_t>(SlotSpan::ToSlotSpanStartPtr(slot_span)) &
          kSuperPageBaseMask,
      reinterpret_cast<uintptr_t>(SlotSpan::ToSlotSpanStartPtr(slot_span2)) &
          kSuperPageBaseMask);

  // Fully free the non-current slot span. This will leave us with no current
  // active slot span because one is empty and the other is full.
  FreeFullSlotSpan(allocator.root(), slot_span);
  EXPECT_EQ(0, slot_span->num_allocated_slots);
  EXPECT_TRUE(bucket->empty_slot_spans_head);
  EXPECT_EQ(SlotSpanMetadata<ThreadSafe>::get_sentinel_slot_span(),
            bucket->active_slot_spans_head);

  // Allocate a new slot span, it should pull from the freelist.
  slot_span = GetFullSlotSpan(kTestAllocSize);
  EXPECT_FALSE(bucket->empty_slot_spans_head);
  EXPECT_EQ(slot_span, bucket->active_slot_spans_head);

  FreeFullSlotSpan(allocator.root(), slot_span);
  FreeFullSlotSpan(allocator.root(), slot_span2);
  EXPECT_EQ(0, slot_span->num_allocated_slots);
  EXPECT_EQ(0, slot_span2->num_allocated_slots);
  EXPECT_EQ(0, slot_span2->num_unprovisioned_slots);
  EXPECT_NE(-1, slot_span2->empty_cache_index);
}

// Test some finer aspects of internal slot span transitions.
TEST_F(PartitionAllocTest, SlotSpanTransitions) {
  PartitionRoot<ThreadSafe>::Bucket* bucket =
      &allocator.root()->buckets[test_bucket_index_];

  auto* slot_span1 = GetFullSlotSpan(kTestAllocSize);
  EXPECT_EQ(slot_span1, bucket->active_slot_spans_head);
  EXPECT_EQ(nullptr, slot_span1->next_slot_span);
  auto* slot_span2 = GetFullSlotSpan(kTestAllocSize);
  EXPECT_EQ(slot_span2, bucket->active_slot_spans_head);
  EXPECT_EQ(nullptr, slot_span2->next_slot_span);

  // Bounce slot_span1 back into the non-full list then fill it up again.
  char* ptr =
      reinterpret_cast<char*>(SlotSpan::ToSlotSpanStartPtr(slot_span1)) +
      kPointerOffset;
  allocator.root()->Free(ptr);
  EXPECT_EQ(slot_span1, bucket->active_slot_spans_head);
  (void)allocator.root()->Alloc(kTestAllocSize, type_name);
  EXPECT_EQ(slot_span1, bucket->active_slot_spans_head);
  EXPECT_EQ(slot_span2, bucket->active_slot_spans_head->next_slot_span);

  // Allocating another slot span at this point should cause us to scan over
  // slot_span1 (which is both full and NOT our current slot span), and evict it
  // from the freelist. Older code had a O(n^2) condition due to failure to do
  // this.
  auto* slot_span3 = GetFullSlotSpan(kTestAllocSize);
  EXPECT_EQ(slot_span3, bucket->active_slot_spans_head);
  EXPECT_EQ(nullptr, slot_span3->next_slot_span);

  // Work out a pointer into slot_span2 and free it.
  ptr = reinterpret_cast<char*>(SlotSpan::ToSlotSpanStartPtr(slot_span2)) +
        kPointerOffset;
  allocator.root()->Free(ptr);
  // Trying to allocate at this time should cause us to cycle around to
  // slot_span2 and find the recently freed slot.
  char* new_ptr = reinterpret_cast<char*>(
      allocator.root()->Alloc(kTestAllocSize, type_name));
  EXPECT_EQ(ptr, new_ptr);
  EXPECT_EQ(slot_span2, bucket->active_slot_spans_head);
  EXPECT_EQ(slot_span3, slot_span2->next_slot_span);

  // Work out a pointer into slot_span1 and free it. This should pull the slot
  // span back into the list of available slot spans.
  ptr = reinterpret_cast<char*>(SlotSpan::ToSlotSpanStartPtr(slot_span1)) +
        kPointerOffset;
  allocator.root()->Free(ptr);
  // This allocation should be satisfied by slot_span1.
  new_ptr = reinterpret_cast<char*>(
      allocator.root()->Alloc(kTestAllocSize, type_name));
  EXPECT_EQ(ptr, new_ptr);
  EXPECT_EQ(slot_span1, bucket->active_slot_spans_head);
  EXPECT_EQ(slot_span2, slot_span1->next_slot_span);

  FreeFullSlotSpan(allocator.root(), slot_span3);
  FreeFullSlotSpan(allocator.root(), slot_span2);
  FreeFullSlotSpan(allocator.root(), slot_span1);

  // Allocating whilst in this state exposed a bug, so keep the test.
  ptr = reinterpret_cast<char*>(
      allocator.root()->Alloc(kTestAllocSize, type_name));
  allocator.root()->Free(ptr);
}

// Test some corner cases relating to slot span transitions in the internal
// free slot span list metadata bucket.
TEST_F(PartitionAllocTest, FreeSlotSpanListSlotSpanTransitions) {
  PartitionRoot<ThreadSafe>::Bucket* bucket =
      &allocator.root()->buckets[test_bucket_index_];

  size_t num_to_fill_free_list_slot_span =
      PartitionPageSize() / (sizeof(SlotSpan) + kExtraAllocSize);
  // The +1 is because we need to account for the fact that the current slot
  // span never gets thrown on the freelist.
  ++num_to_fill_free_list_slot_span;
  auto slot_spans =
      std::make_unique<SlotSpan*[]>(num_to_fill_free_list_slot_span);

  size_t i;
  for (i = 0; i < num_to_fill_free_list_slot_span; ++i) {
    slot_spans[i] = GetFullSlotSpan(kTestAllocSize);
  }
  EXPECT_EQ(slot_spans[num_to_fill_free_list_slot_span - 1],
            bucket->active_slot_spans_head);
  for (i = 0; i < num_to_fill_free_list_slot_span; ++i)
    FreeFullSlotSpan(allocator.root(), slot_spans[i]);
  EXPECT_EQ(SlotSpan::get_sentinel_slot_span(), bucket->active_slot_spans_head);
  EXPECT_TRUE(bucket->empty_slot_spans_head);

  // Allocate / free in a different bucket size so we get control of a
  // different free slot span list. We need two slot spans because one will be
  // the last active slot span and not get freed.
  auto* slot_span1 = GetFullSlotSpan(kTestAllocSize * 2);
  auto* slot_span2 = GetFullSlotSpan(kTestAllocSize * 2);
  FreeFullSlotSpan(allocator.root(), slot_span1);
  FreeFullSlotSpan(allocator.root(), slot_span2);

  for (i = 0; i < num_to_fill_free_list_slot_span; ++i) {
    slot_spans[i] = GetFullSlotSpan(kTestAllocSize);
  }
  EXPECT_EQ(slot_spans[num_to_fill_free_list_slot_span - 1],
            bucket->active_slot_spans_head);

  for (i = 0; i < num_to_fill_free_list_slot_span; ++i)
    FreeFullSlotSpan(allocator.root(), slot_spans[i]);
  EXPECT_EQ(SlotSpan::get_sentinel_slot_span(), bucket->active_slot_spans_head);
  EXPECT_TRUE(bucket->empty_slot_spans_head);
}

// Test a large series of allocations that cross more than one underlying
// super page.
TEST_F(PartitionAllocTest, MultiPageAllocs) {
  size_t num_pages_per_slot_span = GetNumPagesPerSlotSpan(kTestAllocSize);
  // 1 super page has 2 guard partition pages.
  size_t num_slot_spans_needed =
      (NumPartitionPagesPerSuperPage() - 2) / num_pages_per_slot_span;

  // We need one more slot span in order to cross super page boundary.
  ++num_slot_spans_needed;

  EXPECT_GT(num_slot_spans_needed, 1u);
  auto slot_spans = std::make_unique<SlotSpan*[]>(num_slot_spans_needed);
  uintptr_t first_super_page_base = 0;
  size_t i;
  for (i = 0; i < num_slot_spans_needed; ++i) {
    slot_spans[i] = GetFullSlotSpan(kTestAllocSize);
    void* storage_ptr = SlotSpan::ToSlotSpanStartPtr(slot_spans[i]);
    if (!i)
      first_super_page_base =
          reinterpret_cast<uintptr_t>(storage_ptr) & kSuperPageBaseMask;
    if (i == num_slot_spans_needed - 1) {
      uintptr_t second_super_page_base =
          reinterpret_cast<uintptr_t>(storage_ptr) & kSuperPageBaseMask;
      uintptr_t second_super_page_offset =
          reinterpret_cast<uintptr_t>(storage_ptr) & kSuperPageOffsetMask;
      EXPECT_FALSE(second_super_page_base == first_super_page_base);
      // Check that we allocated a guard page for the second page.
      EXPECT_EQ(PartitionPageSize(), second_super_page_offset);
    }
  }
  for (i = 0; i < num_slot_spans_needed; ++i)
    FreeFullSlotSpan(allocator.root(), slot_spans[i]);
}

// Test the generic allocation functions that can handle arbitrary sizes and
// reallocing etc.
TEST_F(PartitionAllocTest, Alloc) {
  void* ptr = allocator.root()->Alloc(1, type_name);
  EXPECT_TRUE(ptr);
  allocator.root()->Free(ptr);
  ptr = allocator.root()->Alloc(kMaxBucketed + 1, type_name);
  EXPECT_TRUE(ptr);
  allocator.root()->Free(ptr);

  // To make both alloc(x + 1) and alloc(x + kSmallestBucket) to allocate from
  // the same bucket, bits::AlignUp(1 + x + kExtraAllocSize, base::kAlignment)
  // == bits::AlignUp(kSmallestBucket + x + kExtraAllocSize, base::kAlignment),
  // because slot_size is multiples of base::kAlignment.
  // So (x + kExtraAllocSize) must be multiples of base::kAlignment.
  // x = bits::AlignUp(kExtraAllocSize, base::kAlignment) - kExtraAllocSize;
  size_t base_size =
      bits::AlignUp(kExtraAllocSize, base::kAlignment) - kExtraAllocSize;
  ptr = allocator.root()->Alloc(base_size + 1, type_name);
  EXPECT_TRUE(ptr);
  void* orig_ptr = ptr;
  char* char_ptr = static_cast<char*>(ptr);
  *char_ptr = 'A';

  // Change the size of the realloc, remaining inside the same bucket.
  void* new_ptr = allocator.root()->Realloc(ptr, base_size + 2, type_name);
  EXPECT_EQ(ptr, new_ptr);
  new_ptr = allocator.root()->Realloc(ptr, base_size + 1, type_name);
  EXPECT_EQ(ptr, new_ptr);
  new_ptr =
      allocator.root()->Realloc(ptr, base_size + kSmallestBucket, type_name);
  EXPECT_EQ(ptr, new_ptr);

  // Change the size of the realloc, switching buckets.
  new_ptr = allocator.root()->Realloc(ptr, base_size + kSmallestBucket + 1,
                                      type_name);
  EXPECT_NE(new_ptr, ptr);
  // Check that the realloc copied correctly.
  char* new_char_ptr = static_cast<char*>(new_ptr);
  EXPECT_EQ(*new_char_ptr, 'A');
#if EXPENSIVE_DCHECKS_ARE_ON()
  // Subtle: this checks for an old bug where we copied too much from the
  // source of the realloc. The condition can be detected by a trashing of
  // the uninitialized value in the space of the upsized allocation.
  EXPECT_EQ(kUninitializedByte,
            static_cast<unsigned char>(*(new_char_ptr + kSmallestBucket)));
#endif
  *new_char_ptr = 'B';
  // The realloc moved. To check that the old allocation was freed, we can
  // do an alloc of the old allocation size and check that the old allocation
  // address is at the head of the freelist and reused.
  void* reused_ptr = allocator.root()->Alloc(base_size + 1, type_name);
  EXPECT_EQ(reused_ptr, orig_ptr);
  allocator.root()->Free(reused_ptr);

  // Downsize the realloc.
  ptr = new_ptr;
  new_ptr = allocator.root()->Realloc(ptr, base_size + 1, type_name);
  EXPECT_EQ(new_ptr, orig_ptr);
  new_char_ptr = static_cast<char*>(new_ptr);
  EXPECT_EQ(*new_char_ptr, 'B');
  *new_char_ptr = 'C';

  // Upsize the realloc to outside the partition.
  ptr = new_ptr;
  new_ptr = allocator.root()->Realloc(ptr, kMaxBucketed + 1, type_name);
  EXPECT_NE(new_ptr, ptr);
  new_char_ptr = static_cast<char*>(new_ptr);
  EXPECT_EQ(*new_char_ptr, 'C');
  *new_char_ptr = 'D';

  // Upsize and downsize the realloc, remaining outside the partition.
  ptr = new_ptr;
  new_ptr = allocator.root()->Realloc(ptr, kMaxBucketed * 10, type_name);
  new_char_ptr = static_cast<char*>(new_ptr);
  EXPECT_EQ(*new_char_ptr, 'D');
  *new_char_ptr = 'E';
  ptr = new_ptr;
  new_ptr = allocator.root()->Realloc(ptr, kMaxBucketed * 2, type_name);
  new_char_ptr = static_cast<char*>(new_ptr);
  EXPECT_EQ(*new_char_ptr, 'E');
  *new_char_ptr = 'F';

  // Downsize the realloc to inside the partition.
  ptr = new_ptr;
  new_ptr = allocator.root()->Realloc(ptr, base_size + 1, type_name);
  EXPECT_NE(new_ptr, ptr);
  EXPECT_EQ(new_ptr, orig_ptr);
  new_char_ptr = static_cast<char*>(new_ptr);
  EXPECT_EQ(*new_char_ptr, 'F');

  allocator.root()->Free(new_ptr);
}

// Test the generic allocation functions can handle some specific sizes of
// interest.
TEST_F(PartitionAllocTest, AllocSizes) {
  {
    void* ptr = allocator.root()->Alloc(0, type_name);
    EXPECT_TRUE(ptr);
    allocator.root()->Free(ptr);
  }

  {
    // PartitionPageSize() is interesting because it results in just one
    // allocation per page, which tripped up some corner cases.
    const size_t size = PartitionPageSize() - kExtraAllocSize;
    void* ptr = allocator.root()->Alloc(size, type_name);
    EXPECT_TRUE(ptr);
    void* ptr2 = allocator.root()->Alloc(size, type_name);
    EXPECT_TRUE(ptr2);
    allocator.root()->Free(ptr);
    // Should be freeable at this point.
    auto* slot_span = SlotSpan::FromSlotStartPtr(
        allocator.root()->AdjustPointerForExtrasSubtract(ptr));
    EXPECT_NE(-1, slot_span->empty_cache_index);
    allocator.root()->Free(ptr2);
  }

  {
    const size_t size =
        (((PartitionPageSize() * kMaxPartitionPagesPerRegularSlotSpan) -
          SystemPageSize()) /
         2) -
        kExtraAllocSize;
    void* ptr = allocator.root()->Alloc(size, type_name);
    EXPECT_TRUE(ptr);
    memset(ptr, 'A', size);
    void* ptr2 = allocator.root()->Alloc(size, type_name);
    EXPECT_TRUE(ptr2);
    void* ptr3 = allocator.root()->Alloc(size, type_name);
    EXPECT_TRUE(ptr3);
    void* ptr4 = allocator.root()->Alloc(size, type_name);
    EXPECT_TRUE(ptr4);

    auto* slot_span =
        SlotSpanMetadata<base::internal::ThreadSafe>::FromSlotStartPtr(
            allocator.root()->AdjustPointerForExtrasSubtract(ptr));
    auto* slot_span2 = SlotSpan::FromSlotStartPtr(
        allocator.root()->AdjustPointerForExtrasSubtract(ptr3));
    EXPECT_NE(slot_span, slot_span2);

    allocator.root()->Free(ptr);
    allocator.root()->Free(ptr3);
    allocator.root()->Free(ptr2);
    // Should be freeable at this point.
    EXPECT_NE(-1, slot_span->empty_cache_index);
    EXPECT_EQ(0, slot_span->num_allocated_slots);
    EXPECT_EQ(0, slot_span->num_unprovisioned_slots);
    void* new_ptr_1 = allocator.root()->Alloc(size, type_name);
    EXPECT_EQ(ptr2, new_ptr_1);
    void* new_ptr_2 = allocator.root()->Alloc(size, type_name);
    EXPECT_EQ(ptr3, new_ptr_2);

    allocator.root()->Free(new_ptr_1);
    allocator.root()->Free(new_ptr_2);
    allocator.root()->Free(ptr4);

#if EXPENSIVE_DCHECKS_ARE_ON()
    // |SlotSpanMetadata::Free| must poison the slot's contents with
    // |kFreedByte|.
    EXPECT_EQ(kFreedByte,
              *(reinterpret_cast<unsigned char*>(new_ptr_1) + (size - 1)));
#endif
  }

  // Can we allocate a massive (512MB) size?
  // Allocate 512MB, but +1, to test for cookie writing alignment issues.
  // Test this only if the device has enough memory or it might fail due
  // to OOM.
  if (IsLargeMemoryDevice()) {
    void* ptr = allocator.root()->Alloc(512 * 1024 * 1024 + 1, type_name);
    allocator.root()->Free(ptr);
  }

  {
    // Check a more reasonable, but still direct mapped, size.
    // Chop a system page and a byte off to test for rounding errors.
    size_t size = 20 * 1024 * 1024;
    size -= SystemPageSize();
    size -= 1;
    void* ptr = allocator.root()->Alloc(size, type_name);
    char* char_ptr = reinterpret_cast<char*>(ptr);
    *(char_ptr + (size - 1)) = 'A';
    allocator.root()->Free(ptr);

    // Can we free null?
    allocator.root()->Free(nullptr);

    // Do we correctly get a null for a failed allocation?
    EXPECT_EQ(nullptr,
              allocator.root()->AllocFlags(PartitionAllocReturnNull,
                                           3u * 1024 * 1024 * 1024, type_name));
  }
}

// Test that we can fetch the real allocated size after an allocation.
TEST_F(PartitionAllocTest, AllocGetSizeAndStart) {
  void* ptr;
#if BUILDFLAG(USE_BACKUP_REF_PTR)
  void* slot_start;
#endif  // BUILDFLAG(USE_BACKUP_REF_PTR)
  size_t requested_size, actual_capacity, predicted_capacity;

  // Allocate something small.
  requested_size = 511 - kExtraAllocSize;
  predicted_capacity =
      allocator.root()->AllocationCapacityFromRequestedSize(requested_size);
  ptr = allocator.root()->Alloc(requested_size, type_name);
  EXPECT_TRUE(ptr);
#if BUILDFLAG(USE_BACKUP_REF_PTR)
  slot_start = reinterpret_cast<char*>(ptr) - allocator.root()->extras_offset;
#endif  // BUILDFLAG(USE_BACKUP_REF_PTR)
  actual_capacity = allocator.root()->AllocationCapacityFromPtr(ptr);
  EXPECT_EQ(predicted_capacity, actual_capacity);
  EXPECT_LT(requested_size, actual_capacity);
#if BUILDFLAG(USE_BACKUP_REF_PTR)
  for (size_t offset = 0; offset < requested_size; ++offset) {
    EXPECT_EQ(PartitionAllocGetSlotStart(static_cast<char*>(ptr) + offset),
              slot_start);
  }
#endif  // BUILDFLAG(USE_BACKUP_REF_PTR)
  allocator.root()->Free(ptr);

  // Allocate a size that should be a perfect match for a bucket, because it
  // is an exact power of 2.
  requested_size = (256 * 1024) - kExtraAllocSize;
  predicted_capacity =
      allocator.root()->AllocationCapacityFromRequestedSize(requested_size);
  ptr = allocator.root()->Alloc(requested_size, type_name);
  EXPECT_TRUE(ptr);
#if BUILDFLAG(USE_BACKUP_REF_PTR)
  slot_start = reinterpret_cast<char*>(ptr) - allocator.root()->extras_offset;
#endif  // BUILDFLAG(USE_BACKUP_REF_PTR)
  actual_capacity = allocator.root()->AllocationCapacityFromPtr(ptr);
  EXPECT_EQ(predicted_capacity, actual_capacity);
  EXPECT_EQ(requested_size, actual_capacity);
#if BUILDFLAG(USE_BACKUP_REF_PTR)
  for (size_t offset = 0; offset < requested_size; offset += 877) {
    EXPECT_EQ(PartitionAllocGetSlotStart(static_cast<char*>(ptr) + offset),
              slot_start);
  }
#endif  // BUILDFLAG(USE_BACKUP_REF_PTR)
  allocator.root()->Free(ptr);

  // Allocate a size that is a system page smaller than a bucket.
  // AllocationCapacityFromPtr() should return a larger size than we asked for
  // now.
  size_t num = 64;
  while (num * SystemPageSize() >= 1024 * 1024) {
    num /= 2;
  }
  requested_size = num * SystemPageSize() - SystemPageSize() - kExtraAllocSize;
  predicted_capacity =
      allocator.root()->AllocationCapacityFromRequestedSize(requested_size);
  ptr = allocator.root()->Alloc(requested_size, type_name);
  EXPECT_TRUE(ptr);
#if BUILDFLAG(USE_BACKUP_REF_PTR)
  slot_start = reinterpret_cast<char*>(ptr) - allocator.root()->extras_offset;
#endif  // BUILDFLAG(USE_BACKUP_REF_PTR)
  actual_capacity = allocator.root()->AllocationCapacityFromPtr(ptr);
  EXPECT_EQ(predicted_capacity, actual_capacity);
  EXPECT_EQ(requested_size + SystemPageSize(), actual_capacity);
#if BUILDFLAG(USE_BACKUP_REF_PTR)
  for (size_t offset = 0; offset < requested_size; offset += 4999) {
    EXPECT_EQ(PartitionAllocGetSlotStart(static_cast<char*>(ptr) + offset),
              slot_start);
  }
#endif  // BUILDFLAG(USE_BACKUP_REF_PTR)

  // Allocate the maximum allowed bucketed size.
  requested_size = kMaxBucketed - kExtraAllocSize;
  predicted_capacity =
      allocator.root()->AllocationCapacityFromRequestedSize(requested_size);
  ptr = allocator.root()->Alloc(requested_size, type_name);
  EXPECT_TRUE(ptr);
#if BUILDFLAG(USE_BACKUP_REF_PTR)
  slot_start = reinterpret_cast<char*>(ptr) - allocator.root()->extras_offset;
#endif  // BUILDFLAG(USE_BACKUP_REF_PTR)
  actual_capacity = allocator.root()->AllocationCapacityFromPtr(ptr);
  EXPECT_EQ(predicted_capacity, actual_capacity);
  EXPECT_EQ(requested_size, actual_capacity);
#if BUILDFLAG(USE_BACKUP_REF_PTR)
  for (size_t offset = 0; offset < requested_size; offset += 4999) {
    EXPECT_EQ(PartitionAllocGetSlotStart(static_cast<char*>(ptr) + offset),
              slot_start);
  }
#endif  // BUILDFLAG(USE_BACKUP_REF_PTR)

  // Check that we can write at the end of the reported size too.
  char* char_ptr = reinterpret_cast<char*>(ptr);
  *(char_ptr + (actual_capacity - 1)) = 'A';
  allocator.root()->Free(ptr);

  // Allocate something very large, and uneven.
  if (IsLargeMemoryDevice()) {
    requested_size = 512 * 1024 * 1024 - 1;
    predicted_capacity =
        allocator.root()->AllocationCapacityFromRequestedSize(requested_size);
    ptr = allocator.root()->Alloc(requested_size, type_name);
    EXPECT_TRUE(ptr);
#if BUILDFLAG(USE_BACKUP_REF_PTR)
    slot_start = reinterpret_cast<char*>(ptr) - allocator.root()->extras_offset;
#endif  // BUILDFLAG(USE_BACKUP_REF_PTR)
    actual_capacity = allocator.root()->AllocationCapacityFromPtr(ptr);
    EXPECT_EQ(predicted_capacity, actual_capacity);
    EXPECT_LT(requested_size, actual_capacity);
#if BUILDFLAG(USE_BACKUP_REF_PTR)
    for (size_t offset = 0; offset < requested_size; offset += 16111) {
      EXPECT_EQ(PartitionAllocGetSlotStart(static_cast<char*>(ptr) + offset),
                slot_start);
    }
#endif  // BUILDFLAG(USE_BACKUP_REF_PTR)
    allocator.root()->Free(ptr);
  }

  // Too large allocation.
  requested_size = MaxDirectMapped() + 1;
  predicted_capacity =
      allocator.root()->AllocationCapacityFromRequestedSize(requested_size);
  EXPECT_EQ(requested_size, predicted_capacity);
}

#if BUILDFLAG(USE_BACKUP_REF_PTR)
TEST_F(PartitionAllocTest, GetSlotStartMultiplePages) {
  const size_t real_size = 80;
  const size_t requested_size = real_size - kExtraAllocSize;
  // Double check we don't end up with 0 or negative size.
  EXPECT_GT(requested_size, 0u);
  EXPECT_LE(requested_size, real_size);
  PartitionBucket<ThreadSafe>* bucket =
      allocator.root()->buckets + SizeToIndex(real_size);
  // Make sure the test is testing multiple partition pages case.
  EXPECT_GT(bucket->num_system_pages_per_slot_span,
            PartitionPageSize() / SystemPageSize());
  size_t num_slots =
      (bucket->num_system_pages_per_slot_span * SystemPageSize()) / real_size;
  std::vector<void*> ptrs;
  for (size_t i = 0; i < num_slots; ++i) {
    ptrs.push_back(allocator.root()->Alloc(requested_size, type_name));
  }
  for (size_t i = 0; i < num_slots; ++i) {
    char* ptr = static_cast<char*>(ptrs[i]);
    EXPECT_EQ(allocator.root()->AllocationCapacityFromPtr(ptr), requested_size);
    char* slot_start =
        reinterpret_cast<char*>(ptr) - allocator.root()->extras_offset;
    for (size_t offset = 0; offset < requested_size; offset += 13) {
      EXPECT_EQ(PartitionAllocGetSlotStart(static_cast<char*>(ptr) + offset),
                slot_start);
    }
    allocator.root()->Free(ptr);
  }
}
#endif  // BUILDFLAG(USE_BACKUP_REF_PTR)

// Test the realloc() contract.
TEST_F(PartitionAllocTest, Realloc) {
  // realloc(0, size) should be equivalent to malloc().
  void* ptr = allocator.root()->Realloc(nullptr, kTestAllocSize, type_name);
  memset(ptr, 'A', kTestAllocSize);
  auto* slot_span = SlotSpan::FromSlotStartPtr(
      allocator.root()->AdjustPointerForExtrasSubtract(ptr));
  // realloc(ptr, 0) should be equivalent to free().
  void* ptr2 = allocator.root()->Realloc(ptr, 0, type_name);
  EXPECT_EQ(nullptr, ptr2);
  EXPECT_EQ(allocator.root()->AdjustPointerForExtrasSubtract(ptr),
            slot_span->freelist_head);

  // Test that growing an allocation with realloc() copies everything from the
  // old allocation.
  size_t size = SystemPageSize() - kExtraAllocSize;
  // Confirm size fills the entire slot.
  ASSERT_EQ(size, allocator.root()->AllocationCapacityFromRequestedSize(size));
  ptr = allocator.root()->Alloc(size, type_name);
  memset(ptr, 'A', size);
  ptr2 = allocator.root()->Realloc(ptr, size + 1, type_name);
  EXPECT_NE(ptr, ptr2);
  char* char_ptr2 = static_cast<char*>(ptr2);
  EXPECT_EQ('A', char_ptr2[0]);
  EXPECT_EQ('A', char_ptr2[size - 1]);
#if EXPENSIVE_DCHECKS_ARE_ON()
  EXPECT_EQ(kUninitializedByte, static_cast<unsigned char>(char_ptr2[size]));
#endif

  // Test that shrinking an allocation with realloc() also copies everything
  // from the old allocation. Use |size - 1| to test what happens to the extra
  // space before the cookie.
  ptr = allocator.root()->Realloc(ptr2, size - 1, type_name);
  EXPECT_NE(ptr2, ptr);
  char* char_ptr = static_cast<char*>(ptr);
  EXPECT_EQ('A', char_ptr[0]);
  EXPECT_EQ('A', char_ptr[size - 2]);
#if EXPENSIVE_DCHECKS_ARE_ON()
  EXPECT_EQ(kUninitializedByte, static_cast<unsigned char>(char_ptr[size - 1]));
#endif

  allocator.root()->Free(ptr);

  // Single-slot slot spans...
  // Test that growing an allocation with realloc() copies everything from the
  // old allocation.
  size = 200000;
  // Confirm size doesn't fill the entire slot.
  ASSERT_LT(size, allocator.root()->AllocationCapacityFromRequestedSize(size));
  ptr = allocator.root()->Alloc(size, type_name);
  memset(ptr, 'A', size);
  ptr2 = allocator.root()->Realloc(ptr, size * 2, type_name);
  EXPECT_NE(ptr, ptr2);
  char_ptr2 = static_cast<char*>(ptr2);
  EXPECT_EQ('A', char_ptr2[0]);
  EXPECT_EQ('A', char_ptr2[size - 1]);
#if EXPENSIVE_DCHECKS_ARE_ON()
  EXPECT_EQ(kUninitializedByte, static_cast<unsigned char>(char_ptr2[size]));
#endif

  // Test that shrinking an allocation with realloc() also copies everything
  // from the old allocation.
  ptr = allocator.root()->Realloc(ptr2, size / 2, type_name);
  EXPECT_NE(ptr2, ptr);
  char_ptr = static_cast<char*>(ptr);
  EXPECT_EQ('A', char_ptr[0]);
  EXPECT_EQ('A', char_ptr[size / 2 - 1]);
#if DCHECK_IS_ON()
  // For single-slot slot spans, the cookie is always placed immediately after
  // the allocation.
  EXPECT_EQ(kCookieValue[0], static_cast<unsigned char>(char_ptr[size / 2]));
#endif

  allocator.root()->Free(ptr);

  // Test that shrinking a direct mapped allocation happens in-place.
  // Pick a large size so that Realloc doesn't think it's worthwhile to
  // downsize even if one less super page is used (due to high granularity on
  // 64-bit systems).
  size = 10 * kSuperPageSize + SystemPageSize() - 42;
  ptr = allocator.root()->Alloc(size, type_name);
  size_t actual_capacity = allocator.root()->AllocationCapacityFromPtr(ptr);
  ptr2 = allocator.root()->Realloc(ptr, size - SystemPageSize(), type_name);
  EXPECT_EQ(ptr, ptr2);
  EXPECT_EQ(actual_capacity - SystemPageSize(),
            allocator.root()->AllocationCapacityFromPtr(ptr2));
  void* ptr3 =
      allocator.root()->Realloc(ptr2, size - 32 * SystemPageSize(), type_name);
  EXPECT_EQ(ptr2, ptr3);
  EXPECT_EQ(actual_capacity - 32 * SystemPageSize(),
            allocator.root()->AllocationCapacityFromPtr(ptr3));

  // Test that a previously in-place shrunk direct mapped allocation can be
  // expanded up again up to its original size.
  ptr = allocator.root()->Realloc(ptr3, size, type_name);
  EXPECT_EQ(ptr3, ptr);
  EXPECT_EQ(actual_capacity, allocator.root()->AllocationCapacityFromPtr(ptr));

  // Test that the allocation can be expanded in place up to its capacity.
  ptr2 = allocator.root()->Realloc(ptr, actual_capacity, type_name);
  EXPECT_EQ(ptr, ptr2);
  EXPECT_EQ(actual_capacity, allocator.root()->AllocationCapacityFromPtr(ptr2));

  // Test that a direct mapped allocation is performed not in-place when the
  // new size is small enough.
  ptr3 = allocator.root()->Realloc(ptr2, SystemPageSize(), type_name);
  EXPECT_NE(ptr2, ptr3);

  allocator.root()->Free(ptr3);
}

TEST_F(PartitionAllocTest, ReallocDirectMapAligned) {
  size_t alignments[] = {
      PartitionPageSize(),
      2 * PartitionPageSize(),
      kMaxSupportedAlignment / 2,
      kMaxSupportedAlignment,
  };

  for (size_t alignment : alignments) {
    // Test that shrinking a direct mapped allocation happens in-place.
    // Pick a large size so that Realloc doesn't think it's worthwhile to
    // downsize even if one less super page is used (due to high granularity on
    // 64-bit systems), even if the alignment padding is taken out.
    size_t size = 10 * kSuperPageSize + SystemPageSize() - 42;
    void* ptr =
        allocator.root()->AllocFlagsInternal(0, size, alignment, type_name);
    size_t actual_capacity = allocator.root()->AllocationCapacityFromPtr(ptr);
    void* ptr2 =
        allocator.root()->Realloc(ptr, size - SystemPageSize(), type_name);
    EXPECT_EQ(ptr, ptr2);
    EXPECT_EQ(actual_capacity - SystemPageSize(),
              allocator.root()->AllocationCapacityFromPtr(ptr2));
    void* ptr3 = allocator.root()->Realloc(ptr2, size - 32 * SystemPageSize(),
                                           type_name);
    EXPECT_EQ(ptr2, ptr3);
    EXPECT_EQ(actual_capacity - 32 * SystemPageSize(),
              allocator.root()->AllocationCapacityFromPtr(ptr3));

    // Test that a previously in-place shrunk direct mapped allocation can be
    // expanded up again up to its original size.
    ptr = allocator.root()->Realloc(ptr3, size, type_name);
    EXPECT_EQ(ptr3, ptr);
    EXPECT_EQ(actual_capacity,
              allocator.root()->AllocationCapacityFromPtr(ptr));

    // Test that the allocation can be expanded in place up to its capacity.
    ptr2 = allocator.root()->Realloc(ptr, actual_capacity, type_name);
    EXPECT_EQ(ptr, ptr2);
    EXPECT_EQ(actual_capacity,
              allocator.root()->AllocationCapacityFromPtr(ptr2));

    // Test that a direct mapped allocation is performed not in-place when the
    // new size is small enough.
    ptr3 = allocator.root()->Realloc(ptr2, SystemPageSize(), type_name);
    EXPECT_NE(ptr2, ptr3);

    allocator.root()->Free(ptr3);
  }
}

TEST_F(PartitionAllocTest, ReallocDirectMapAlignedRelocate) {
  // Pick size such that the alignment will put it cross the super page
  // boundary.
  size_t size = 2 * kSuperPageSize - kMaxSupportedAlignment + SystemPageSize();
  void* ptr = allocator.root()->AllocFlagsInternal(
      0, size, kMaxSupportedAlignment, type_name);
  // Reallocating with the same size will actually relocate, because without a
  // need for alignment we can downsize the reservation significantly.
  void* ptr2 = allocator.root()->Realloc(ptr, size, type_name);
  EXPECT_NE(ptr, ptr2);
  allocator.root()->Free(ptr2);

  // Again pick size such that the alignment will put it cross the super page
  // boundary, but this time make it so large that Realloc doesn't fing it worth
  // shrinking.
  size = 10 * kSuperPageSize - kMaxSupportedAlignment + SystemPageSize();
  ptr = allocator.root()->AllocFlagsInternal(0, size, kMaxSupportedAlignment,
                                             type_name);
  ptr2 = allocator.root()->Realloc(ptr, size, type_name);
  EXPECT_EQ(ptr, ptr2);
  allocator.root()->Free(ptr2);
}

// Tests the handing out of freelists for partial slot spans.
#if defined(OS_MAC) && defined(ARCH_CPU_ARM64)
// Bulk-disabled on mac-arm64 for bot stabilization: https://crbug.com/1154345
#define MAYBE_PartialPageFreelists DISABLED_PartialPageFreelists
#else
#define MAYBE_PartialPageFreelists PartialPageFreelists
#endif
TEST_F(PartitionAllocTest, MAYBE_PartialPageFreelists) {
  size_t big_size = SystemPageSize() - kExtraAllocSize;
  size_t bucket_index = SizeToIndex(big_size + kExtraAllocSize);
  PartitionRoot<ThreadSafe>::Bucket* bucket =
      &allocator.root()->buckets[bucket_index];
  EXPECT_EQ(nullptr, bucket->empty_slot_spans_head);

  void* ptr = allocator.root()->Alloc(big_size, type_name);
  EXPECT_TRUE(ptr);

  auto* slot_span = SlotSpan::FromSlotStartPtr(
      allocator.root()->AdjustPointerForExtrasSubtract(ptr));
  size_t total_slots =
      (slot_span->bucket->num_system_pages_per_slot_span * SystemPageSize()) /
      (big_size + kExtraAllocSize);
  EXPECT_EQ(4u, total_slots);
  // The freelist should have one entry, because we were able to exactly fit
  // one object slot and one freelist pointer (the null that the head points
  // to) into a system page.
  EXPECT_FALSE(slot_span->freelist_head);
  EXPECT_EQ(1, slot_span->num_allocated_slots);
  EXPECT_EQ(3, slot_span->num_unprovisioned_slots);

  void* ptr2 = allocator.root()->Alloc(big_size, type_name);
  EXPECT_TRUE(ptr2);
  EXPECT_FALSE(slot_span->freelist_head);
  EXPECT_EQ(2, slot_span->num_allocated_slots);
  EXPECT_EQ(2, slot_span->num_unprovisioned_slots);

  void* ptr3 = allocator.root()->Alloc(big_size, type_name);
  EXPECT_TRUE(ptr3);
  EXPECT_FALSE(slot_span->freelist_head);
  EXPECT_EQ(3, slot_span->num_allocated_slots);
  EXPECT_EQ(1, slot_span->num_unprovisioned_slots);

  void* ptr4 = allocator.root()->Alloc(big_size, type_name);
  EXPECT_TRUE(ptr4);
  EXPECT_FALSE(slot_span->freelist_head);
  EXPECT_EQ(4, slot_span->num_allocated_slots);
  EXPECT_EQ(0, slot_span->num_unprovisioned_slots);

  void* ptr5 = allocator.root()->Alloc(big_size, type_name);
  EXPECT_TRUE(ptr5);

  auto* slot_span2 = SlotSpan::FromSlotStartPtr(
      allocator.root()->AdjustPointerForExtrasSubtract(ptr5));
  EXPECT_EQ(1, slot_span2->num_allocated_slots);

  // Churn things a little whilst there's a partial slot span freelist.
  allocator.root()->Free(ptr);
  ptr = allocator.root()->Alloc(big_size, type_name);
  void* ptr6 = allocator.root()->Alloc(big_size, type_name);

  allocator.root()->Free(ptr);
  allocator.root()->Free(ptr2);
  allocator.root()->Free(ptr3);
  allocator.root()->Free(ptr4);
  allocator.root()->Free(ptr5);
  allocator.root()->Free(ptr6);
  EXPECT_NE(-1, slot_span->empty_cache_index);
  EXPECT_NE(-1, slot_span2->empty_cache_index);
  EXPECT_TRUE(slot_span2->freelist_head);
  EXPECT_EQ(0, slot_span2->num_allocated_slots);

  // Size that doesn't divide the system page size.
  size_t non_dividing_size = 2100 - kExtraAllocSize;
  bucket_index = SizeToIndex(non_dividing_size + kExtraAllocSize);
  bucket = &allocator.root()->buckets[bucket_index];
  EXPECT_EQ(nullptr, bucket->empty_slot_spans_head);

  ptr = allocator.root()->Alloc(non_dividing_size, type_name);
  EXPECT_TRUE(ptr);

  slot_span = SlotSpan::FromSlotStartPtr(
      allocator.root()->AdjustPointerForExtrasSubtract(ptr));
  total_slots =
      (slot_span->bucket->num_system_pages_per_slot_span * SystemPageSize()) /
      bucket->slot_size;
  const size_t expected_slots = kNumBucketsPerOrderBits == 3 ? 16u : 24u;
  EXPECT_EQ(expected_slots, total_slots);
  EXPECT_FALSE(slot_span->freelist_head);
  EXPECT_EQ(1, slot_span->num_allocated_slots);
  EXPECT_EQ(expected_slots - 1, slot_span->num_unprovisioned_slots);

  ptr2 = allocator.root()->Alloc(non_dividing_size, type_name);
  EXPECT_TRUE(ptr2);
  EXPECT_TRUE(slot_span->freelist_head);
  EXPECT_EQ(2, slot_span->num_allocated_slots);
  EXPECT_EQ(expected_slots - 3, slot_span->num_unprovisioned_slots);

  ptr3 = allocator.root()->Alloc(non_dividing_size, type_name);
  EXPECT_TRUE(ptr3);
  EXPECT_FALSE(slot_span->freelist_head);
  EXPECT_EQ(3, slot_span->num_allocated_slots);
  EXPECT_EQ(expected_slots - 3, slot_span->num_unprovisioned_slots);

  allocator.root()->Free(ptr);
  allocator.root()->Free(ptr2);
  allocator.root()->Free(ptr3);
  EXPECT_NE(-1, slot_span->empty_cache_index);
  EXPECT_TRUE(slot_span2->freelist_head);
  EXPECT_EQ(0, slot_span2->num_allocated_slots);

  // And test a couple of sizes that do not cross SystemPageSize() with a
  // single allocation.
  size_t medium_size = (SystemPageSize() / 2) - kExtraAllocSize;
  bucket_index = SizeToIndex(medium_size + kExtraAllocSize);
  bucket = &allocator.root()->buckets[bucket_index];
  EXPECT_EQ(nullptr, bucket->empty_slot_spans_head);

  ptr = allocator.root()->Alloc(medium_size, type_name);
  EXPECT_TRUE(ptr);
  slot_span = SlotSpan::FromSlotStartPtr(
      allocator.root()->AdjustPointerForExtrasSubtract(ptr));
  EXPECT_EQ(1, slot_span->num_allocated_slots);
  total_slots =
      (slot_span->bucket->num_system_pages_per_slot_span * SystemPageSize()) /
      (medium_size + kExtraAllocSize);
  size_t first_slot_span_slots =
      SystemPageSize() / (medium_size + kExtraAllocSize);
  EXPECT_EQ(2u, first_slot_span_slots);
  EXPECT_EQ(total_slots - first_slot_span_slots,
            slot_span->num_unprovisioned_slots);

  allocator.root()->Free(ptr);

  size_t small_size = (SystemPageSize() / 4) - kExtraAllocSize;
  bucket_index = SizeToIndex(small_size + kExtraAllocSize);
  bucket = &allocator.root()->buckets[bucket_index];
  EXPECT_EQ(nullptr, bucket->empty_slot_spans_head);

  ptr = allocator.root()->Alloc(small_size, type_name);
  EXPECT_TRUE(ptr);
  slot_span = SlotSpan::FromSlotStartPtr(
      allocator.root()->AdjustPointerForExtrasSubtract(ptr));
  EXPECT_EQ(1, slot_span->num_allocated_slots);
  total_slots =
      (slot_span->bucket->num_system_pages_per_slot_span * SystemPageSize()) /
      (small_size + kExtraAllocSize);
  first_slot_span_slots = SystemPageSize() / (small_size + kExtraAllocSize);
  EXPECT_EQ(total_slots - first_slot_span_slots,
            slot_span->num_unprovisioned_slots);

  allocator.root()->Free(ptr);
  EXPECT_TRUE(slot_span->freelist_head);
  EXPECT_EQ(0, slot_span->num_allocated_slots);

  static_assert(kExtraAllocSize < 64, "");
  size_t very_small_size =
      (kExtraAllocSize <= 32) ? (32 - kExtraAllocSize) : (64 - kExtraAllocSize);
  size_t very_small_adjusted_size =
      allocator.root()->AdjustSize0IfNeeded(very_small_size);
  bucket_index = SizeToIndex(very_small_adjusted_size + kExtraAllocSize);
  bucket = &allocator.root()->buckets[bucket_index];
  EXPECT_EQ(nullptr, bucket->empty_slot_spans_head);

  ptr = allocator.root()->Alloc(very_small_size, type_name);
  EXPECT_TRUE(ptr);
  slot_span = SlotSpan::FromSlotStartPtr(
      allocator.root()->AdjustPointerForExtrasSubtract(ptr));
  EXPECT_EQ(1, slot_span->num_allocated_slots);
  size_t very_small_actual_size = allocator.root()->GetUsableSize(ptr);
  total_slots =
      (slot_span->bucket->num_system_pages_per_slot_span * SystemPageSize()) /
      (very_small_actual_size + kExtraAllocSize);
  first_slot_span_slots =
      SystemPageSize() / (very_small_actual_size + kExtraAllocSize);
  EXPECT_EQ(total_slots - first_slot_span_slots,
            slot_span->num_unprovisioned_slots);

  allocator.root()->Free(ptr);
  EXPECT_TRUE(slot_span->freelist_head);
  EXPECT_EQ(0, slot_span->num_allocated_slots);

  // And try an allocation size (against the generic allocator) that is
  // larger than a system page.
  size_t page_and_a_half_size =
      (SystemPageSize() + (SystemPageSize() / 2)) - kExtraAllocSize;
  ptr = allocator.root()->Alloc(page_and_a_half_size, type_name);
  EXPECT_TRUE(ptr);
  slot_span = SlotSpan::FromSlotStartPtr(
      allocator.root()->AdjustPointerForExtrasSubtract(ptr));
  EXPECT_EQ(1, slot_span->num_allocated_slots);
  // Only the first slot was provisioned, and that's the one that was just
  // allocated so the free list is empty.
  EXPECT_TRUE(!slot_span->freelist_head);
  total_slots =
      (slot_span->bucket->num_system_pages_per_slot_span * SystemPageSize()) /
      (page_and_a_half_size + kExtraAllocSize);
  EXPECT_EQ(total_slots - 1, slot_span->num_unprovisioned_slots);
  ptr2 = allocator.root()->Alloc(page_and_a_half_size, type_name);
  EXPECT_TRUE(ptr);
  slot_span = SlotSpan::FromSlotStartPtr(
      allocator.root()->AdjustPointerForExtrasSubtract(ptr));
  EXPECT_EQ(2, slot_span->num_allocated_slots);
  // As above, only one slot was provisioned.
  EXPECT_TRUE(!slot_span->freelist_head);
  EXPECT_EQ(total_slots - 2, slot_span->num_unprovisioned_slots);
  allocator.root()->Free(ptr);
  allocator.root()->Free(ptr2);

  // And then make sure than exactly the page size only faults one page.
  size_t page_size = SystemPageSize() - kExtraAllocSize;
  ptr = allocator.root()->Alloc(page_size, type_name);
  EXPECT_TRUE(ptr);
  slot_span = SlotSpan::FromSlotStartPtr(
      allocator.root()->AdjustPointerForExtrasSubtract(ptr));
  EXPECT_EQ(1, slot_span->num_allocated_slots);
  EXPECT_TRUE(slot_span->freelist_head);
  total_slots =
      (slot_span->bucket->num_system_pages_per_slot_span * SystemPageSize()) /
      (page_size + kExtraAllocSize);
  EXPECT_EQ(total_slots - 2, slot_span->num_unprovisioned_slots);
  allocator.root()->Free(ptr);
}

// Test some of the fragmentation-resistant properties of the allocator.
TEST_F(PartitionAllocTest, SlotSpanRefilling) {
  PartitionRoot<ThreadSafe>::Bucket* bucket =
      &allocator.root()->buckets[test_bucket_index_];

  // Grab two full slot spans and a non-full slot span.
  auto* slot_span1 = GetFullSlotSpan(kTestAllocSize);
  auto* slot_span2 = GetFullSlotSpan(kTestAllocSize);
  void* ptr = allocator.root()->Alloc(kTestAllocSize, type_name);
  EXPECT_TRUE(ptr);
  EXPECT_NE(slot_span1, bucket->active_slot_spans_head);
  EXPECT_NE(slot_span2, bucket->active_slot_spans_head);
  auto* slot_span = SlotSpan::FromSlotStartPtr(
      allocator.root()->AdjustPointerForExtrasSubtract(ptr));
  EXPECT_EQ(1, slot_span->num_allocated_slots);

  // Work out a pointer into slot_span2 and free it; and then slot_span1 and
  // free it.
  char* ptr2 =
      reinterpret_cast<char*>(SlotSpan::ToSlotSpanStartPtr(slot_span1)) +
      kPointerOffset;
  allocator.root()->Free(ptr2);
  ptr2 = reinterpret_cast<char*>(SlotSpan::ToSlotSpanStartPtr(slot_span2)) +
         kPointerOffset;
  allocator.root()->Free(ptr2);

  // If we perform two allocations from the same bucket now, we expect to
  // refill both the nearly full slot spans.
  (void)allocator.root()->Alloc(kTestAllocSize, type_name);
  (void)allocator.root()->Alloc(kTestAllocSize, type_name);
  EXPECT_EQ(1, slot_span->num_allocated_slots);

  FreeFullSlotSpan(allocator.root(), slot_span2);
  FreeFullSlotSpan(allocator.root(), slot_span1);
  allocator.root()->Free(ptr);
}

// Basic tests to ensure that allocations work for partial page buckets.
TEST_F(PartitionAllocTest, PartialPages) {
  // Find a size that is backed by a partial partition page.
  size_t size = sizeof(void*);
  size_t bucket_index;

  PartitionRoot<ThreadSafe>::Bucket* bucket = nullptr;
  while (size < 1000u) {
    bucket_index = SizeToIndex(size + kExtraAllocSize);
    bucket = &allocator.root()->buckets[bucket_index];
    if (bucket->num_system_pages_per_slot_span %
        NumSystemPagesPerPartitionPage())
      break;
    size += sizeof(void*);
  }
  EXPECT_LT(size, 1000u);

  auto* slot_span1 = GetFullSlotSpan(size);
  auto* slot_span2 = GetFullSlotSpan(size);
  FreeFullSlotSpan(allocator.root(), slot_span2);
  FreeFullSlotSpan(allocator.root(), slot_span1);
}

// Test correct handling if our mapping collides with another.
TEST_F(PartitionAllocTest, MappingCollision) {
  size_t num_pages_per_slot_span = GetNumPagesPerSlotSpan(kTestAllocSize);
  // The -2 is because the first and last partition pages in a super page are
  // guard pages.
  size_t num_slot_span_needed =
      (NumPartitionPagesPerSuperPage() - 2) / num_pages_per_slot_span;
  size_t num_partition_pages_needed =
      num_slot_span_needed * num_pages_per_slot_span;

  auto first_super_page_pages =
      std::make_unique<SlotSpan*[]>(num_partition_pages_needed);
  auto second_super_page_pages =
      std::make_unique<SlotSpan*[]>(num_partition_pages_needed);

  size_t i;
  for (i = 0; i < num_partition_pages_needed; ++i)
    first_super_page_pages[i] = GetFullSlotSpan(kTestAllocSize);

  char* page_base = reinterpret_cast<char*>(
      SlotSpan::ToSlotSpanStartPtr(first_super_page_pages[0]));
  EXPECT_EQ(PartitionPageSize(),
            reinterpret_cast<uintptr_t>(page_base) & kSuperPageOffsetMask);
  page_base -= PartitionPageSize();
  // Map a single system page either side of the mapping for our allocations,
  // with the goal of tripping up alignment of the next mapping.
  void* map1 = AllocPages(
      page_base - PageAllocationGranularity(), PageAllocationGranularity(),
      PageAllocationGranularity(), PageInaccessible, PageTag::kPartitionAlloc);
  EXPECT_TRUE(map1);
  void* map2 = AllocPages(
      page_base + kSuperPageSize, PageAllocationGranularity(),
      PageAllocationGranularity(), PageInaccessible, PageTag::kPartitionAlloc);
  EXPECT_TRUE(map2);

  for (i = 0; i < num_partition_pages_needed; ++i)
    second_super_page_pages[i] = GetFullSlotSpan(kTestAllocSize);

  FreePages(map1, PageAllocationGranularity());
  FreePages(map2, PageAllocationGranularity());

  page_base = reinterpret_cast<char*>(
      SlotSpan::ToSlotSpanStartPtr(second_super_page_pages[0]));
  EXPECT_EQ(PartitionPageSize(),
            reinterpret_cast<uintptr_t>(page_base) & kSuperPageOffsetMask);
  page_base -= PartitionPageSize();
  // Map a single system page either side of the mapping for our allocations,
  // with the goal of tripping up alignment of the next mapping.
  map1 = AllocPages(page_base - PageAllocationGranularity(),
                    PageAllocationGranularity(), PageAllocationGranularity(),
                    PageReadWrite, PageTag::kPartitionAlloc);
  EXPECT_TRUE(map1);
  map2 = AllocPages(page_base + kSuperPageSize, PageAllocationGranularity(),
                    PageAllocationGranularity(), PageReadWrite,
                    PageTag::kPartitionAlloc);
  EXPECT_TRUE(map2);
  EXPECT_TRUE(TrySetSystemPagesAccess(map1, PageAllocationGranularity(),
                                      PageInaccessible));
  EXPECT_TRUE(TrySetSystemPagesAccess(map2, PageAllocationGranularity(),
                                      PageInaccessible));

  auto* slot_span_in_third_super_page = GetFullSlotSpan(kTestAllocSize);
  FreePages(map1, PageAllocationGranularity());
  FreePages(map2, PageAllocationGranularity());

  EXPECT_EQ(0u, reinterpret_cast<uintptr_t>(SlotSpan::ToSlotSpanStartPtr(
                    slot_span_in_third_super_page)) &
                    PartitionPageOffsetMask());

  // And make sure we really did get a page in a new superpage.
  EXPECT_NE(reinterpret_cast<uintptr_t>(
                SlotSpan::ToSlotSpanStartPtr(first_super_page_pages[0])) &
                kSuperPageBaseMask,
            reinterpret_cast<uintptr_t>(
                SlotSpan::ToSlotSpanStartPtr(slot_span_in_third_super_page)) &
                kSuperPageBaseMask);
  EXPECT_NE(reinterpret_cast<uintptr_t>(
                SlotSpan::ToSlotSpanStartPtr(second_super_page_pages[0])) &
                kSuperPageBaseMask,
            reinterpret_cast<uintptr_t>(
                SlotSpan::ToSlotSpanStartPtr(slot_span_in_third_super_page)) &
                kSuperPageBaseMask);

  FreeFullSlotSpan(allocator.root(), slot_span_in_third_super_page);
  for (i = 0; i < num_partition_pages_needed; ++i) {
    FreeFullSlotSpan(allocator.root(), first_super_page_pages[i]);
    FreeFullSlotSpan(allocator.root(), second_super_page_pages[i]);
  }
}

// Tests that slot spans in the free slot span cache do get freed as
// appropriate.
TEST_F(PartitionAllocTest, FreeCache) {
  EXPECT_EQ(0U, allocator.root()->get_total_size_of_committed_pages());

  size_t big_size = 1000 - kExtraAllocSize;
  size_t bucket_index = SizeToIndex(big_size + kExtraAllocSize);
  PartitionBucket<base::internal::ThreadSafe>* bucket =
      &allocator.root()->buckets[bucket_index];

  void* ptr = allocator.root()->Alloc(big_size, type_name);
  EXPECT_TRUE(ptr);
  auto* slot_span = SlotSpan::FromSlotStartPtr(
      allocator.root()->AdjustPointerForExtrasSubtract(ptr));
  EXPECT_EQ(nullptr, bucket->empty_slot_spans_head);
  EXPECT_EQ(1, slot_span->num_allocated_slots);
  // Lazy commit commits only needed pages.
  size_t expected_committed_size = allocator.root()->use_lazy_commit
                                       ? SystemPageSize()
                                       : PartitionPageSize();
  EXPECT_EQ(expected_committed_size,
            allocator.root()->get_total_size_of_committed_pages());
  allocator.root()->Free(ptr);
  EXPECT_EQ(0, slot_span->num_allocated_slots);
  EXPECT_NE(-1, slot_span->empty_cache_index);
  EXPECT_TRUE(slot_span->freelist_head);

  CycleFreeCache(kTestAllocSize);

  // Flushing the cache should have really freed the unused slot spans.
  EXPECT_FALSE(slot_span->freelist_head);
  EXPECT_EQ(-1, slot_span->empty_cache_index);
  EXPECT_EQ(0, slot_span->num_allocated_slots);
  size_t num_system_pages_per_slot_span = allocator.root()
                                              ->buckets[test_bucket_index_]
                                              .num_system_pages_per_slot_span;
  size_t expected_size =
      allocator.root()->use_lazy_commit
          ? SystemPageSize()
          : num_system_pages_per_slot_span * SystemPageSize();
  EXPECT_EQ(expected_size,
            allocator.root()->get_total_size_of_committed_pages());

  // Check that an allocation works ok whilst in this state (a free'd slot span
  // as the active slot spans head).
  ptr = allocator.root()->Alloc(big_size, type_name);
  EXPECT_FALSE(bucket->empty_slot_spans_head);
  allocator.root()->Free(ptr);

  // Also check that a slot span that is bouncing immediately between empty and
  // used does not get freed.
  for (size_t i = 0; i < kMaxFreeableSpans * 2; ++i) {
    ptr = allocator.root()->Alloc(big_size, type_name);
    EXPECT_TRUE(slot_span->freelist_head);
    allocator.root()->Free(ptr);
    EXPECT_TRUE(slot_span->freelist_head);
  }
  EXPECT_EQ(expected_committed_size,
            allocator.root()->get_total_size_of_committed_pages());
}

// Tests for a bug we had with losing references to free slot spans.
TEST_F(PartitionAllocTest, LostFreeSlotSpansBug) {
  size_t size = PartitionPageSize() - kExtraAllocSize;

  void* ptr = allocator.root()->Alloc(size, type_name);
  EXPECT_TRUE(ptr);
  void* ptr2 = allocator.root()->Alloc(size, type_name);
  EXPECT_TRUE(ptr2);

  SlotSpanMetadata<base::internal::ThreadSafe>* slot_span =
      SlotSpanMetadata<base::internal::ThreadSafe>::FromSlotStartPtr(
          allocator.root()->AdjustPointerForExtrasSubtract(ptr));
  SlotSpanMetadata<base::internal::ThreadSafe>* slot_span2 =
      SlotSpanMetadata<base::internal::ThreadSafe>::FromSlotStartPtr(
          allocator.root()->AdjustPointerForExtrasSubtract(ptr2));
  PartitionBucket<base::internal::ThreadSafe>* bucket = slot_span->bucket;

  EXPECT_EQ(nullptr, bucket->empty_slot_spans_head);
  EXPECT_EQ(-1, slot_span->num_allocated_slots);
  EXPECT_EQ(1, slot_span2->num_allocated_slots);

  allocator.root()->Free(ptr);
  allocator.root()->Free(ptr2);

  EXPECT_TRUE(bucket->empty_slot_spans_head);
  EXPECT_TRUE(bucket->empty_slot_spans_head->next_slot_span);
  EXPECT_EQ(0, slot_span->num_allocated_slots);
  EXPECT_EQ(0, slot_span2->num_allocated_slots);
  EXPECT_TRUE(slot_span->freelist_head);
  EXPECT_TRUE(slot_span2->freelist_head);

  CycleFreeCache(kTestAllocSize);

  EXPECT_FALSE(slot_span->freelist_head);
  EXPECT_FALSE(slot_span2->freelist_head);

  EXPECT_TRUE(bucket->empty_slot_spans_head);
  EXPECT_TRUE(bucket->empty_slot_spans_head->next_slot_span);
  EXPECT_EQ(
      SlotSpanMetadata<base::internal::ThreadSafe>::get_sentinel_slot_span(),
      bucket->active_slot_spans_head);

  // At this moment, we have two decommitted slot spans, on the empty list.
  ptr = allocator.root()->Alloc(size, type_name);
  EXPECT_TRUE(ptr);
  allocator.root()->Free(ptr);

  EXPECT_EQ(
      SlotSpanMetadata<base::internal::ThreadSafe>::get_sentinel_slot_span(),
      bucket->active_slot_spans_head);
  EXPECT_TRUE(bucket->empty_slot_spans_head);
  EXPECT_TRUE(bucket->decommitted_slot_spans_head);

  CycleFreeCache(kTestAllocSize);

  // We're now set up to trigger a historical bug by scanning over the active
  // slot spans list. The current code gets into a different state, but we'll
  // keep the test as being an interesting corner case.
  ptr = allocator.root()->Alloc(size, type_name);
  EXPECT_TRUE(ptr);
  allocator.root()->Free(ptr);

  EXPECT_TRUE(bucket->is_valid());
  EXPECT_TRUE(bucket->empty_slot_spans_head);
  EXPECT_TRUE(bucket->decommitted_slot_spans_head);
}

// Death tests misbehave on Android, http://crbug.com/643760.
#if defined(GTEST_HAS_DEATH_TEST) && !defined(OS_ANDROID)

// Unit tests that check if an allocation fails in "return null" mode,
// repeating it doesn't crash, and still returns null. The tests need to
// stress memory subsystem limits to do so, hence they try to allocate
// 6 GB of memory, each with a different per-allocation block sizes.
//
// On 64-bit systems we need to restrict the address space to force allocation
// failure, so these tests run only on POSIX systems that provide setrlimit(),
// and use it to limit address space to 6GB.
//
// Disable these tests on Android because, due to the allocation-heavy behavior,
// they tend to get OOM-killed rather than pass.
// TODO(https://crbug.com/779645): Fuchsia currently sets OS_POSIX, but does
// not provide a working setrlimit().
//
// Disable these test on Windows, since they run slower, so tend to timout and
// cause flake.
#if !defined(OS_WIN) &&            \
    (!defined(ARCH_CPU_64_BITS) || \
     (defined(OS_POSIX) && !(defined(OS_APPLE) || defined(OS_ANDROID))))

// The following four tests wrap a called function in an expect death statement
// to perform their test, because they are non-hermetic. Specifically they are
// going to attempt to exhaust the allocatable memory, which leaves the
// allocator in a bad global state.
// Performing them as death tests causes them to be forked into their own
// process, so they won't pollute other tests.
//
// These tests are *very* slow when DCHECK_IS_ON(), because they memset() many
// GiB of data (see crbug.com/1168168).
// TODO(lizeb): make these tests faster.
TEST_F(PartitionAllocDeathTest, RepeatedAllocReturnNullDirect) {
  // A direct-mapped allocation size.
  EXPECT_DEATH(DoReturnNullTest(32 * 1024 * 1024, kPartitionAllocFlags),
               "DoReturnNullTest");
}

// Repeating above test with Realloc
TEST_F(PartitionAllocDeathTest, RepeatedReallocReturnNullDirect) {
  EXPECT_DEATH(DoReturnNullTest(32 * 1024 * 1024, kPartitionReallocFlags),
               "DoReturnNullTest");
}

// Repeating above test with TryRealloc
TEST_F(PartitionAllocDeathTest, RepeatedTryReallocReturnNullDirect) {
  EXPECT_DEATH(DoReturnNullTest(32 * 1024 * 1024, kPartitionRootTryRealloc),
               "DoReturnNullTest");
}

// See crbug.com/1187404 to re-enable the tests below.
// Test "return null" with a 512 kB block size.
TEST_F(PartitionAllocDeathTest, DISABLED_RepeatedAllocReturnNull) {
  // A single-slot but non-direct-mapped allocation size.
  EXPECT_DEATH(DoReturnNullTest(512 * 1024, kPartitionAllocFlags),
               "DoReturnNullTest");
}

// Repeating above test with Realloc.
TEST_F(PartitionAllocDeathTest, DISABLED_RepeatedReallocReturnNull) {
  EXPECT_DEATH(DoReturnNullTest(512 * 1024, kPartitionReallocFlags),
               "DoReturnNullTest");
}

// Repeating above test with TryRealloc.
TEST_F(PartitionAllocDeathTest, DISABLED_RepeatedTryReallocReturnNull) {
  EXPECT_DEATH(DoReturnNullTest(512 * 1024, kPartitionRootTryRealloc),
               "DoReturnNullTest");
}

#endif  // !defined(ARCH_CPU_64_BITS) || (defined(OS_POSIX) &&
        // !(defined(OS_APPLE) || defined(OS_ANDROID)))

// Make sure that malloc(-1) dies.
// In the past, we had an integer overflow that would alias malloc(-1) to
// malloc(0), which is not good.
TEST_F(PartitionAllocDeathTest, LargeAllocs) {
  // Largest alloc.
  EXPECT_DEATH(allocator.root()->Alloc(static_cast<size_t>(-1), type_name), "");
  // And the smallest allocation we expect to die.
  EXPECT_DEATH(allocator.root()->Alloc(MaxDirectMapped() + 1, type_name), "");
}

// Check that our immediate double-free detection works.
TEST_F(PartitionAllocDeathTest, ImmediateDoubleFree) {
  void* ptr = allocator.root()->Alloc(kTestAllocSize, type_name);
  EXPECT_TRUE(ptr);
  allocator.root()->Free(ptr);
  EXPECT_DEATH(allocator.root()->Free(ptr), "");
}

// Check that our refcount-based double-free detection works.
TEST_F(PartitionAllocDeathTest, RefcountDoubleFree) {
  void* ptr = allocator.root()->Alloc(kTestAllocSize, type_name);
  EXPECT_TRUE(ptr);
  void* ptr2 = allocator.root()->Alloc(kTestAllocSize, type_name);
  EXPECT_TRUE(ptr2);
  allocator.root()->Free(ptr);
  allocator.root()->Free(ptr2);
  // This is not an immediate double-free so our immediate detection won't
  // fire. However, it does take the "refcount" of the to -1, which is illegal
  // and should be trapped.
  EXPECT_DEATH(allocator.root()->Free(ptr), "");
}

// Check that guard pages are present where expected.
TEST_F(PartitionAllocDeathTest, DirectMapGuardPages) {
  const size_t kSizes[] = {
      kMaxBucketed + kExtraAllocSize + 1, kMaxBucketed + SystemPageSize(),
      kMaxBucketed + PartitionPageSize(),
      bits::AlignUp(kMaxBucketed + kSuperPageSize, kSuperPageSize) -
          PartitionRoot<ThreadSafe>::GetDirectMapMetadataAndGuardPagesSize()};
  for (size_t size : kSizes) {
    size -= kExtraAllocSize;
    EXPECT_GT(size, kMaxBucketed)
        << "allocation not large enough for direct allocation";
    void* ptr = allocator.root()->Alloc(size, type_name);

    EXPECT_TRUE(ptr);
    char* char_ptr = reinterpret_cast<char*>(ptr) - kPointerOffset;

    EXPECT_DEATH(*(char_ptr - 1) = 'A', "");
    EXPECT_DEATH(*(char_ptr + bits::AlignUp(size, SystemPageSize())) = 'A', "");

    allocator.root()->Free(ptr);
  }
}

// These tests rely on precise layout. We handle cookies, not reference counts.
#if !BUILDFLAG(USE_BACKUP_REF_PTR) && defined(PA_HAS_FREELIST_HARDENING)

namespace {

// Used to adjust the returned pointer. This is necessary to make the following
// death tests pass with DCHECK_IS_ON(), as they rely on the precise knowledge
// of the allocator layout. Since the offset is 0 for non-DCHECK() builds, this
// provides the benefit of more testing coverage.
constexpr size_t kAllocationOffset =
#if DCHECK_IS_ON()
    kCookieSize;
#else
    0;
#endif
}  // namespace

TEST_F(PartitionAllocDeathTest, UseAfterFreeDetection) {
  void* data = allocator.root()->Alloc(100, "");
  allocator.root()->Free(data);

  // use after free, not crashing here, but the next allocation should crash,
  // since we corrupted the freelist.
  //
  // When there is a cookie, must offset the UaF since the freelist entry is
  // stored over the cookie area, not the allocated data.
  void* data_before_cookie = reinterpret_cast<char*>(data) - kAllocationOffset;
  memset(data_before_cookie, 0x42, 100);
  EXPECT_DEATH(allocator.root()->Alloc(100, ""), "");
}

TEST_F(PartitionAllocDeathTest, FreelistCorruption) {
  const size_t alloc_size = 2 * sizeof(void*);
  void** fake_freelist_entry =
      static_cast<void**>(allocator.root()->Alloc(alloc_size, ""));
  fake_freelist_entry[0] = nullptr;
  fake_freelist_entry[1] = nullptr;

  void** uaf_data =
      static_cast<void**>(allocator.root()->Alloc(alloc_size, ""));
  allocator.root()->Free(uaf_data);
  void** uaf_data_before_cookie = reinterpret_cast<void**>(
      reinterpret_cast<char*>(uaf_data) - kAllocationOffset);
  // Try to confuse the allocator. This is still easy to circumvent willingly,
  // "just" need to set uaf_data[1] to ~uaf_data[0].
  uaf_data_before_cookie[0] = fake_freelist_entry;
  EXPECT_DEATH(allocator.root()->Alloc(alloc_size, ""), "");
}

// With DCHECK_IS_ON(), cookies already handle off-by-one detection.
#if !DCHECK_IS_ON()
TEST_F(PartitionAllocDeathTest, OffByOneDetection) {
  const size_t alloc_size = 2 * sizeof(void*);
  char* array = static_cast<char*>(allocator.root()->Alloc(alloc_size, ""));
  array[alloc_size] = 'A';
  // Crash at the next allocation. This assumes that we are touching a new,
  // non-randomized slot span, where the next slot to be handed over to the
  // application directly follows the current one.
  EXPECT_DEATH(allocator.root()->Alloc(alloc_size, ""), "");
}

TEST_F(PartitionAllocDeathTest, OffByOneDetectionWithRealisticData) {
  const size_t alloc_size = 2 * sizeof(void*);
  void** array = static_cast<void**>(allocator.root()->Alloc(alloc_size, ""));
  char valid;
  array[2] = &valid;
  // Crash at the next allocation. This assumes that we are touching a new,
  // non-randomized slot span, where the next slot to be handed over to the
  // application directly follows the current one.
  EXPECT_DEATH(allocator.root()->Alloc(alloc_size, ""), "");
}
#endif  // !DCHECK_IS_ON()

#endif  // !BUILDFLAG(USE_BACKUP_REF_PTR) && defined(PA_HAS_FREELIST_HARDENING)

#endif  // !defined(OS_ANDROID) && !defined(OS_IOS)

// Tests that |PartitionDumpStats| and |PartitionDumpStats| run without
// crashing and return non-zero values when memory is allocated.
TEST_F(PartitionAllocTest, DumpMemoryStats) {
  {
    void* ptr = allocator.root()->Alloc(kTestAllocSize, type_name);
    MockPartitionStatsDumper mock_stats_dumper;
    allocator.root()->DumpStats("mock_allocator", false /* detailed dump */,
                                &mock_stats_dumper);
    EXPECT_TRUE(mock_stats_dumper.IsMemoryAllocationRecorded());
    allocator.root()->Free(ptr);
  }

  // This series of tests checks the active -> empty -> decommitted states.
  {
    {
      void* ptr = allocator.root()->Alloc(2048 - kExtraAllocSize, type_name);
      MockPartitionStatsDumper dumper;
      allocator.root()->DumpStats("mock_allocator", false /* detailed dump */,
                                  &dumper);
      EXPECT_TRUE(dumper.IsMemoryAllocationRecorded());

      const PartitionBucketMemoryStats* stats = dumper.GetBucketStats(2048);
      EXPECT_TRUE(stats);
      EXPECT_TRUE(stats->is_valid);
      EXPECT_EQ(2048u, stats->bucket_slot_size);
      EXPECT_EQ(2048u, stats->active_bytes);
      EXPECT_EQ(SystemPageSize(), stats->resident_bytes);
      EXPECT_EQ(0u, stats->decommittable_bytes);
      EXPECT_EQ(0u, stats->discardable_bytes);
      EXPECT_EQ(0u, stats->num_full_slot_spans);
      EXPECT_EQ(1u, stats->num_active_slot_spans);
      EXPECT_EQ(0u, stats->num_empty_slot_spans);
      EXPECT_EQ(0u, stats->num_decommitted_slot_spans);
      allocator.root()->Free(ptr);
    }

    {
      MockPartitionStatsDumper dumper;
      allocator.root()->DumpStats("mock_allocator", false /* detailed dump */,
                                  &dumper);
      EXPECT_FALSE(dumper.IsMemoryAllocationRecorded());

      const PartitionBucketMemoryStats* stats = dumper.GetBucketStats(2048);
      EXPECT_TRUE(stats);
      EXPECT_TRUE(stats->is_valid);
      EXPECT_EQ(2048u, stats->bucket_slot_size);
      EXPECT_EQ(0u, stats->active_bytes);
      EXPECT_EQ(SystemPageSize(), stats->resident_bytes);
      EXPECT_EQ(SystemPageSize(), stats->decommittable_bytes);
      EXPECT_EQ(0u, stats->discardable_bytes);
      EXPECT_EQ(0u, stats->num_full_slot_spans);
      EXPECT_EQ(0u, stats->num_active_slot_spans);
      EXPECT_EQ(1u, stats->num_empty_slot_spans);
      EXPECT_EQ(0u, stats->num_decommitted_slot_spans);
    }

    // TODO(crbug.com/722911): Commenting this out causes this test to fail when
    // run singly (--gtest_filter=PartitionAllocTest.DumpMemoryStats), but not
    // when run with the others (--gtest_filter=PartitionAllocTest.*).
    CycleFreeCache(kTestAllocSize);

    {
      MockPartitionStatsDumper dumper;
      allocator.root()->DumpStats("mock_allocator", false /* detailed dump */,
                                  &dumper);
      EXPECT_FALSE(dumper.IsMemoryAllocationRecorded());

      const PartitionBucketMemoryStats* stats = dumper.GetBucketStats(2048);
      EXPECT_TRUE(stats);
      EXPECT_TRUE(stats->is_valid);
      EXPECT_EQ(2048u, stats->bucket_slot_size);
      EXPECT_EQ(0u, stats->active_bytes);
      EXPECT_EQ(0u, stats->resident_bytes);
      EXPECT_EQ(0u, stats->decommittable_bytes);
      EXPECT_EQ(0u, stats->discardable_bytes);
      EXPECT_EQ(0u, stats->num_full_slot_spans);
      EXPECT_EQ(0u, stats->num_active_slot_spans);
      EXPECT_EQ(0u, stats->num_empty_slot_spans);
      EXPECT_EQ(1u, stats->num_decommitted_slot_spans);
    }
  }

  // This test checks for correct empty slot span list accounting.
  {
    size_t size = PartitionPageSize() - kExtraAllocSize;
    void* ptr1 = allocator.root()->Alloc(size, type_name);
    void* ptr2 = allocator.root()->Alloc(size, type_name);
    allocator.root()->Free(ptr1);
    allocator.root()->Free(ptr2);

    CycleFreeCache(kTestAllocSize);

    ptr1 = allocator.root()->Alloc(size, type_name);

    {
      MockPartitionStatsDumper dumper;
      allocator.root()->DumpStats("mock_allocator", false /* detailed dump */,
                                  &dumper);
      EXPECT_TRUE(dumper.IsMemoryAllocationRecorded());

      const PartitionBucketMemoryStats* stats =
          dumper.GetBucketStats(PartitionPageSize());
      EXPECT_TRUE(stats);
      EXPECT_TRUE(stats->is_valid);
      EXPECT_EQ(PartitionPageSize(), stats->bucket_slot_size);
      EXPECT_EQ(PartitionPageSize(), stats->active_bytes);
      EXPECT_EQ(PartitionPageSize(), stats->resident_bytes);
      EXPECT_EQ(0u, stats->decommittable_bytes);
      EXPECT_EQ(0u, stats->discardable_bytes);
      EXPECT_EQ(1u, stats->num_full_slot_spans);
      EXPECT_EQ(0u, stats->num_active_slot_spans);
      EXPECT_EQ(0u, stats->num_empty_slot_spans);
      EXPECT_EQ(1u, stats->num_decommitted_slot_spans);
    }
    allocator.root()->Free(ptr1);
  }

  // This test checks for correct direct mapped accounting.
  {
    size_t size_smaller = kMaxBucketed + 1;
    size_t size_bigger = (kMaxBucketed * 2) + 1;
    size_t real_size_smaller =
        (size_smaller + SystemPageOffsetMask()) & SystemPageBaseMask();
    size_t real_size_bigger =
        (size_bigger + SystemPageOffsetMask()) & SystemPageBaseMask();
    void* ptr = allocator.root()->Alloc(size_smaller, type_name);
    void* ptr2 = allocator.root()->Alloc(size_bigger, type_name);

    {
      MockPartitionStatsDumper dumper;
      allocator.root()->DumpStats("mock_allocator", false /* detailed dump */,
                                  &dumper);
      EXPECT_TRUE(dumper.IsMemoryAllocationRecorded());

      const PartitionBucketMemoryStats* stats =
          dumper.GetBucketStats(real_size_smaller);
      EXPECT_TRUE(stats);
      EXPECT_TRUE(stats->is_valid);
      EXPECT_TRUE(stats->is_direct_map);
      EXPECT_EQ(real_size_smaller, stats->bucket_slot_size);
      EXPECT_EQ(real_size_smaller, stats->active_bytes);
      EXPECT_EQ(real_size_smaller, stats->resident_bytes);
      EXPECT_EQ(0u, stats->decommittable_bytes);
      EXPECT_EQ(0u, stats->discardable_bytes);
      EXPECT_EQ(1u, stats->num_full_slot_spans);
      EXPECT_EQ(0u, stats->num_active_slot_spans);
      EXPECT_EQ(0u, stats->num_empty_slot_spans);
      EXPECT_EQ(0u, stats->num_decommitted_slot_spans);

      stats = dumper.GetBucketStats(real_size_bigger);
      EXPECT_TRUE(stats);
      EXPECT_TRUE(stats->is_valid);
      EXPECT_TRUE(stats->is_direct_map);
      EXPECT_EQ(real_size_bigger, stats->bucket_slot_size);
      EXPECT_EQ(real_size_bigger, stats->active_bytes);
      EXPECT_EQ(real_size_bigger, stats->resident_bytes);
      EXPECT_EQ(0u, stats->decommittable_bytes);
      EXPECT_EQ(0u, stats->discardable_bytes);
      EXPECT_EQ(1u, stats->num_full_slot_spans);
      EXPECT_EQ(0u, stats->num_active_slot_spans);
      EXPECT_EQ(0u, stats->num_empty_slot_spans);
      EXPECT_EQ(0u, stats->num_decommitted_slot_spans);
    }

    allocator.root()->Free(ptr2);
    allocator.root()->Free(ptr);

    // Whilst we're here, allocate again and free with different ordering to
    // give a workout to our linked list code.
    ptr = allocator.root()->Alloc(size_smaller, type_name);
    ptr2 = allocator.root()->Alloc(size_bigger, type_name);
    allocator.root()->Free(ptr);
    allocator.root()->Free(ptr2);
  }

  // This test checks large-but-not-quite-direct allocations.
  {
    const size_t requested_size = 16 * SystemPageSize();
    void* ptr = allocator.root()->Alloc(requested_size + 1, type_name);

    {
      MockPartitionStatsDumper dumper;
      allocator.root()->DumpStats("mock_allocator", false /* detailed dump */,
                                  &dumper);
      EXPECT_TRUE(dumper.IsMemoryAllocationRecorded());

      size_t slot_size =
          requested_size + (requested_size / kNumBucketsPerOrder);
      const PartitionBucketMemoryStats* stats =
          dumper.GetBucketStats(slot_size);
      ASSERT_TRUE(stats);
      EXPECT_TRUE(stats->is_valid);
      EXPECT_FALSE(stats->is_direct_map);
      EXPECT_EQ(slot_size, stats->bucket_slot_size);
      EXPECT_EQ(requested_size + 1 + kExtraAllocSize, stats->active_bytes);
      EXPECT_EQ(slot_size, stats->resident_bytes);
      EXPECT_EQ(0u, stats->decommittable_bytes);
      EXPECT_EQ(3 * SystemPageSize(), stats->discardable_bytes);
      EXPECT_EQ(1u, stats->num_full_slot_spans);
      EXPECT_EQ(0u, stats->num_active_slot_spans);
      EXPECT_EQ(0u, stats->num_empty_slot_spans);
      EXPECT_EQ(0u, stats->num_decommitted_slot_spans);
    }

    allocator.root()->Free(ptr);

    {
      MockPartitionStatsDumper dumper;
      allocator.root()->DumpStats("mock_allocator", false /* detailed dump */,
                                  &dumper);
      EXPECT_FALSE(dumper.IsMemoryAllocationRecorded());

      size_t slot_size =
          requested_size + (requested_size / kNumBucketsPerOrder);
      const PartitionBucketMemoryStats* stats =
          dumper.GetBucketStats(slot_size);
      EXPECT_TRUE(stats);
      EXPECT_TRUE(stats->is_valid);
      EXPECT_FALSE(stats->is_direct_map);
      EXPECT_EQ(slot_size, stats->bucket_slot_size);
      EXPECT_EQ(0u, stats->active_bytes);
      EXPECT_EQ(slot_size, stats->resident_bytes);
      EXPECT_EQ(slot_size, stats->decommittable_bytes);
      EXPECT_EQ(0u, stats->num_full_slot_spans);
      EXPECT_EQ(0u, stats->num_active_slot_spans);
      EXPECT_EQ(1u, stats->num_empty_slot_spans);
      EXPECT_EQ(0u, stats->num_decommitted_slot_spans);
    }

    void* ptr2 = allocator.root()->Alloc(requested_size + SystemPageSize() + 1,
                                         type_name);
    EXPECT_EQ(ptr, ptr2);

    {
      MockPartitionStatsDumper dumper;
      allocator.root()->DumpStats("mock_allocator", false /* detailed dump */,
                                  &dumper);
      EXPECT_TRUE(dumper.IsMemoryAllocationRecorded());

      size_t slot_size =
          requested_size + (requested_size / kNumBucketsPerOrder);
      const PartitionBucketMemoryStats* stats =
          dumper.GetBucketStats(slot_size);
      EXPECT_TRUE(stats);
      EXPECT_TRUE(stats->is_valid);
      EXPECT_FALSE(stats->is_direct_map);
      EXPECT_EQ(slot_size, stats->bucket_slot_size);
      EXPECT_EQ(requested_size + SystemPageSize() + 1 + kExtraAllocSize,
                stats->active_bytes);
      EXPECT_EQ(slot_size, stats->resident_bytes);
      EXPECT_EQ(0u, stats->decommittable_bytes);
      EXPECT_EQ(2 * SystemPageSize(), stats->discardable_bytes);
      EXPECT_EQ(1u, stats->num_full_slot_spans);
      EXPECT_EQ(0u, stats->num_active_slot_spans);
      EXPECT_EQ(0u, stats->num_empty_slot_spans);
      EXPECT_EQ(0u, stats->num_decommitted_slot_spans);
    }

    allocator.root()->Free(ptr2);
  }
}

// Tests the API to purge freeable memory.
TEST_F(PartitionAllocTest, Purge) {
  char* ptr = reinterpret_cast<char*>(
      allocator.root()->Alloc(2048 - kExtraAllocSize, type_name));
  allocator.root()->Free(ptr);
  {
    MockPartitionStatsDumper dumper;
    allocator.root()->DumpStats("mock_allocator", false /* detailed dump */,
                                &dumper);
    EXPECT_FALSE(dumper.IsMemoryAllocationRecorded());

    const PartitionBucketMemoryStats* stats = dumper.GetBucketStats(2048);
    EXPECT_TRUE(stats);
    EXPECT_TRUE(stats->is_valid);
    EXPECT_EQ(SystemPageSize(), stats->decommittable_bytes);
    EXPECT_EQ(SystemPageSize(), stats->resident_bytes);
  }
  allocator.root()->PurgeMemory(PartitionPurgeDecommitEmptySlotSpans);
  {
    MockPartitionStatsDumper dumper;
    allocator.root()->DumpStats("mock_allocator", false /* detailed dump */,
                                &dumper);
    EXPECT_FALSE(dumper.IsMemoryAllocationRecorded());

    const PartitionBucketMemoryStats* stats = dumper.GetBucketStats(2048);
    EXPECT_TRUE(stats);
    EXPECT_TRUE(stats->is_valid);
    EXPECT_EQ(0u, stats->decommittable_bytes);
    EXPECT_EQ(0u, stats->resident_bytes);
  }
  // Calling purge again here is a good way of testing we didn't mess up the
  // state of the free cache ring.
  allocator.root()->PurgeMemory(PartitionPurgeDecommitEmptySlotSpans);

  char* big_ptr =
      reinterpret_cast<char*>(allocator.root()->Alloc(256 * 1024, type_name));
  allocator.root()->Free(big_ptr);
  allocator.root()->PurgeMemory(PartitionPurgeDecommitEmptySlotSpans);

  CHECK_PAGE_IN_CORE(ptr - kPointerOffset, false);
  CHECK_PAGE_IN_CORE(big_ptr - kPointerOffset, false);
}

// Tests that we prefer to allocate into a non-empty partition page over an
// empty one. This is an important aspect of minimizing memory usage for some
// allocation sizes, particularly larger ones.
TEST_F(PartitionAllocTest, PreferActiveOverEmpty) {
  size_t size = (SystemPageSize() * 2) - kExtraAllocSize;
  // Allocate 3 full slot spans worth of 8192-byte allocations.
  // Each slot span for this size is 16384 bytes, or 1 partition page and 2
  // slots.
  void* ptr1 = allocator.root()->Alloc(size, type_name);
  void* ptr2 = allocator.root()->Alloc(size, type_name);
  void* ptr3 = allocator.root()->Alloc(size, type_name);
  void* ptr4 = allocator.root()->Alloc(size, type_name);
  void* ptr5 = allocator.root()->Alloc(size, type_name);
  void* ptr6 = allocator.root()->Alloc(size, type_name);

  SlotSpanMetadata<base::internal::ThreadSafe>* slot_span1 =
      SlotSpanMetadata<base::internal::ThreadSafe>::FromSlotStartPtr(
          allocator.root()->AdjustPointerForExtrasSubtract(ptr1));
  SlotSpanMetadata<base::internal::ThreadSafe>* slot_span2 =
      SlotSpanMetadata<base::internal::ThreadSafe>::FromSlotStartPtr(
          allocator.root()->AdjustPointerForExtrasSubtract(ptr3));
  SlotSpanMetadata<base::internal::ThreadSafe>* slot_span3 =
      SlotSpanMetadata<base::internal::ThreadSafe>::FromSlotStartPtr(
          allocator.root()->AdjustPointerForExtrasSubtract(ptr6));
  EXPECT_NE(slot_span1, slot_span2);
  EXPECT_NE(slot_span2, slot_span3);
  PartitionBucket<base::internal::ThreadSafe>* bucket = slot_span1->bucket;
  EXPECT_EQ(slot_span3, bucket->active_slot_spans_head);

  // Free up the 2nd slot in each slot span.
  // This leaves the active list containing 3 slot spans, each with 1 used and 1
  // free slot. The active slot span will be the one containing ptr1.
  allocator.root()->Free(ptr6);
  allocator.root()->Free(ptr4);
  allocator.root()->Free(ptr2);
  EXPECT_EQ(slot_span1, bucket->active_slot_spans_head);

  // Empty the middle slot span in the active list.
  allocator.root()->Free(ptr3);
  EXPECT_EQ(slot_span1, bucket->active_slot_spans_head);

  // Empty the first slot span in the active list -- also the current slot span.
  allocator.root()->Free(ptr1);

  // A good choice here is to re-fill the third slot span since the first two
  // are empty. We used to fail that.
  void* ptr7 = allocator.root()->Alloc(size, type_name);
  EXPECT_EQ(ptr6, ptr7);
  EXPECT_EQ(slot_span3, bucket->active_slot_spans_head);

  allocator.root()->Free(ptr5);
  allocator.root()->Free(ptr7);
}

// Tests the API to purge discardable memory.
TEST_F(PartitionAllocTest, PurgeDiscardableSecondPage) {
  // Free the second of two 4096 byte allocations and then purge.
  void* ptr1 =
      allocator.root()->Alloc(SystemPageSize() - kExtraAllocSize, type_name);
  char* ptr2 = reinterpret_cast<char*>(
      allocator.root()->Alloc(SystemPageSize() - kExtraAllocSize, type_name));
  allocator.root()->Free(ptr2);
  SlotSpanMetadata<base::internal::ThreadSafe>* slot_span =
      SlotSpanMetadata<base::internal::ThreadSafe>::FromSlotStartPtr(
          allocator.root()->AdjustPointerForExtrasSubtract(ptr1));
  EXPECT_EQ(2u, slot_span->num_unprovisioned_slots);
  {
    MockPartitionStatsDumper dumper;
    allocator.root()->DumpStats("mock_allocator", false /* detailed dump */,
                                &dumper);
    EXPECT_TRUE(dumper.IsMemoryAllocationRecorded());

    const PartitionBucketMemoryStats* stats =
        dumper.GetBucketStats(SystemPageSize());
    EXPECT_TRUE(stats);
    EXPECT_TRUE(stats->is_valid);
    EXPECT_EQ(0u, stats->decommittable_bytes);
    EXPECT_EQ(SystemPageSize(), stats->discardable_bytes);
    EXPECT_EQ(SystemPageSize(), stats->active_bytes);
    EXPECT_EQ(2 * SystemPageSize(), stats->resident_bytes);
  }
  CHECK_PAGE_IN_CORE(ptr2 - kPointerOffset, true);
  allocator.root()->PurgeMemory(PartitionPurgeDiscardUnusedSystemPages);
  CHECK_PAGE_IN_CORE(ptr2 - kPointerOffset, false);
  EXPECT_EQ(3u, slot_span->num_unprovisioned_slots);

  allocator.root()->Free(ptr1);
}

TEST_F(PartitionAllocTest, PurgeDiscardableFirstPage) {
  // Free the first of two 4096 byte allocations and then purge.
  char* ptr1 = reinterpret_cast<char*>(
      allocator.root()->Alloc(SystemPageSize() - kExtraAllocSize, type_name));
  void* ptr2 =
      allocator.root()->Alloc(SystemPageSize() - kExtraAllocSize, type_name);
  allocator.root()->Free(ptr1);
  {
    MockPartitionStatsDumper dumper;
    allocator.root()->DumpStats("mock_allocator", false /* detailed dump */,
                                &dumper);
    EXPECT_TRUE(dumper.IsMemoryAllocationRecorded());

    const PartitionBucketMemoryStats* stats =
        dumper.GetBucketStats(SystemPageSize());
    EXPECT_TRUE(stats);
    EXPECT_TRUE(stats->is_valid);
    EXPECT_EQ(0u, stats->decommittable_bytes);
#if defined(OS_WIN)
    EXPECT_EQ(0u, stats->discardable_bytes);
#else
    EXPECT_EQ(SystemPageSize(), stats->discardable_bytes);
#endif
    EXPECT_EQ(SystemPageSize(), stats->active_bytes);
    EXPECT_EQ(2 * SystemPageSize(), stats->resident_bytes);
  }
  CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset, true);
  allocator.root()->PurgeMemory(PartitionPurgeDiscardUnusedSystemPages);
  CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset, false);

  allocator.root()->Free(ptr2);
}

TEST_F(PartitionAllocTest, PurgeDiscardableNonPageSizedAlloc) {
  const size_t requested_size = 2.5 * SystemPageSize();
  char* ptr1 = reinterpret_cast<char*>(
      allocator.root()->Alloc(requested_size - kExtraAllocSize, type_name));
  void* ptr2 =
      allocator.root()->Alloc(requested_size - kExtraAllocSize, type_name);
  void* ptr3 =
      allocator.root()->Alloc(requested_size - kExtraAllocSize, type_name);
  void* ptr4 =
      allocator.root()->Alloc(requested_size - kExtraAllocSize, type_name);
  memset(ptr1, 'A', requested_size - kExtraAllocSize);
  memset(ptr2, 'A', requested_size - kExtraAllocSize);
  allocator.root()->Free(ptr2);
  allocator.root()->Free(ptr1);
  {
    MockPartitionStatsDumper dumper;
    allocator.root()->DumpStats("mock_allocator", false /* detailed dump */,
                                &dumper);
    EXPECT_TRUE(dumper.IsMemoryAllocationRecorded());

    const PartitionBucketMemoryStats* stats =
        dumper.GetBucketStats(requested_size);
    EXPECT_TRUE(stats);
    EXPECT_TRUE(stats->is_valid);
    EXPECT_EQ(0u, stats->decommittable_bytes);
    EXPECT_EQ(3 * SystemPageSize(), stats->discardable_bytes);
    EXPECT_EQ(requested_size * 2, stats->active_bytes);
    EXPECT_EQ(10 * SystemPageSize(), stats->resident_bytes);
  }
  CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset, true);
  CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset + SystemPageSize(), true);
  CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset + (SystemPageSize() * 2), true);
  CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset + (SystemPageSize() * 3), true);
  CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset + (SystemPageSize() * 4), true);
  allocator.root()->PurgeMemory(PartitionPurgeDiscardUnusedSystemPages);
  CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset, true);
  CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset + SystemPageSize(), false);
  CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset + (SystemPageSize() * 2), true);
  CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset + (SystemPageSize() * 3), false);
  CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset + (SystemPageSize() * 4), false);

  allocator.root()->Free(ptr3);
  allocator.root()->Free(ptr4);
}

TEST_F(PartitionAllocTest, PurgeDiscardableManyPages) {
  // On systems with large pages, use less pages because:
  // 1) There must be a bucket for kFirstAllocPages * SystemPageSize(), and
  // 2) On low-end systems, using too many large pages can OOM during the test
  const bool kHasLargePages = SystemPageSize() > 4096;
  const size_t kFirstAllocPages = kHasLargePages ? 32 : 64;
  const size_t kSecondAllocPages = kHasLargePages ? 31 : 61;

  // Detect case (1) from above.
  DCHECK_LT(kFirstAllocPages * SystemPageSize(), 1UL << kMaxBucketedOrder);

  const size_t kDeltaPages = kFirstAllocPages - kSecondAllocPages;

  {
    ScopedPageAllocation p(allocator, kFirstAllocPages);
    p.TouchAllPages();
  }

  ScopedPageAllocation p(allocator, kSecondAllocPages);

  MockPartitionStatsDumper dumper;
  allocator.root()->DumpStats("mock_allocator", false /* detailed dump */,
                              &dumper);
  EXPECT_TRUE(dumper.IsMemoryAllocationRecorded());

  const PartitionBucketMemoryStats* stats =
      dumper.GetBucketStats(kFirstAllocPages * SystemPageSize());
  EXPECT_TRUE(stats);
  EXPECT_TRUE(stats->is_valid);
  EXPECT_EQ(0u, stats->decommittable_bytes);
  EXPECT_EQ(kDeltaPages * SystemPageSize(), stats->discardable_bytes);
  EXPECT_EQ(kSecondAllocPages * SystemPageSize(), stats->active_bytes);
  EXPECT_EQ(kFirstAllocPages * SystemPageSize(), stats->resident_bytes);

  for (size_t i = 0; i < kFirstAllocPages; i++)
    CHECK_PAGE_IN_CORE(p.PageAtIndex(i), true);

  allocator.root()->PurgeMemory(PartitionPurgeDiscardUnusedSystemPages);

  for (size_t i = 0; i < kSecondAllocPages; i++)
    CHECK_PAGE_IN_CORE(p.PageAtIndex(i), true);
  for (size_t i = kSecondAllocPages; i < kFirstAllocPages; i++)
    CHECK_PAGE_IN_CORE(p.PageAtIndex(i), false);
}

TEST_F(PartitionAllocTest, PurgeDiscardableWithFreeListRewrite) {
  // This sub-test tests truncation of the provisioned slots in a trickier
  // case where the freelist is rewritten.
  allocator.root()->PurgeMemory(PartitionPurgeDecommitEmptySlotSpans);
  char* ptr1 = reinterpret_cast<char*>(
      allocator.root()->Alloc(SystemPageSize() - kExtraAllocSize, type_name));
  void* ptr2 =
      allocator.root()->Alloc(SystemPageSize() - kExtraAllocSize, type_name);
  void* ptr3 =
      allocator.root()->Alloc(SystemPageSize() - kExtraAllocSize, type_name);
  void* ptr4 =
      allocator.root()->Alloc(SystemPageSize() - kExtraAllocSize, type_name);
  ptr1[0] = 'A';
  ptr1[SystemPageSize()] = 'A';
  ptr1[SystemPageSize() * 2] = 'A';
  ptr1[SystemPageSize() * 3] = 'A';
  SlotSpanMetadata<base::internal::ThreadSafe>* slot_span =
      SlotSpanMetadata<base::internal::ThreadSafe>::FromSlotStartPtr(
          allocator.root()->AdjustPointerForExtrasSubtract(ptr1));
  allocator.root()->Free(ptr2);
  allocator.root()->Free(ptr4);
  allocator.root()->Free(ptr1);
  EXPECT_EQ(0u, slot_span->num_unprovisioned_slots);

  {
    MockPartitionStatsDumper dumper;
    allocator.root()->DumpStats("mock_allocator", false /* detailed dump */,
                                &dumper);
    EXPECT_TRUE(dumper.IsMemoryAllocationRecorded());

    const PartitionBucketMemoryStats* stats =
        dumper.GetBucketStats(SystemPageSize());
    EXPECT_TRUE(stats);
    EXPECT_TRUE(stats->is_valid);
    EXPECT_EQ(0u, stats->decommittable_bytes);
#if defined(OS_WIN)
    EXPECT_EQ(SystemPageSize(), stats->discardable_bytes);
#else
    EXPECT_EQ(2 * SystemPageSize(), stats->discardable_bytes);
#endif
    EXPECT_EQ(SystemPageSize(), stats->active_bytes);
    EXPECT_EQ(4 * SystemPageSize(), stats->resident_bytes);
  }
  CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset, true);
  CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset + SystemPageSize(), true);
  CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset + (SystemPageSize() * 2), true);
  CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset + (SystemPageSize() * 3), true);
  allocator.root()->PurgeMemory(PartitionPurgeDiscardUnusedSystemPages);
  EXPECT_EQ(1u, slot_span->num_unprovisioned_slots);
  CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset, true);
  CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset + SystemPageSize(), false);
  CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset + (SystemPageSize() * 2), true);
  CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset + (SystemPageSize() * 3), false);

  // Let's check we didn't brick the freelist.
  void* ptr1b =
      allocator.root()->Alloc(SystemPageSize() - kExtraAllocSize, type_name);
  EXPECT_EQ(ptr1, ptr1b);
  void* ptr2b =
      allocator.root()->Alloc(SystemPageSize() - kExtraAllocSize, type_name);
  EXPECT_EQ(ptr2, ptr2b);
  EXPECT_FALSE(slot_span->freelist_head);

  allocator.root()->Free(ptr1);
  allocator.root()->Free(ptr2);
  allocator.root()->Free(ptr3);
}

TEST_F(PartitionAllocTest, PurgeDiscardableDoubleTruncateFreeList) {
  // This sub-test is similar, but tests a double-truncation.
  allocator.root()->PurgeMemory(PartitionPurgeDecommitEmptySlotSpans);
  char* ptr1 = reinterpret_cast<char*>(
      allocator.root()->Alloc(SystemPageSize() - kExtraAllocSize, type_name));
  void* ptr2 =
      allocator.root()->Alloc(SystemPageSize() - kExtraAllocSize, type_name);
  void* ptr3 =
      allocator.root()->Alloc(SystemPageSize() - kExtraAllocSize, type_name);
  void* ptr4 =
      allocator.root()->Alloc(SystemPageSize() - kExtraAllocSize, type_name);
  ptr1[0] = 'A';
  ptr1[SystemPageSize()] = 'A';
  ptr1[SystemPageSize() * 2] = 'A';
  ptr1[SystemPageSize() * 3] = 'A';
  SlotSpanMetadata<base::internal::ThreadSafe>* slot_span =
      SlotSpanMetadata<base::internal::ThreadSafe>::FromSlotStartPtr(
          allocator.root()->AdjustPointerForExtrasSubtract(ptr1));
  allocator.root()->Free(ptr4);
  allocator.root()->Free(ptr3);
  EXPECT_EQ(0u, slot_span->num_unprovisioned_slots);

  {
    MockPartitionStatsDumper dumper;
    allocator.root()->DumpStats("mock_allocator", false /* detailed dump */,
                                &dumper);
    EXPECT_TRUE(dumper.IsMemoryAllocationRecorded());

    const PartitionBucketMemoryStats* stats =
        dumper.GetBucketStats(SystemPageSize());
    EXPECT_TRUE(stats);
    EXPECT_TRUE(stats->is_valid);
    EXPECT_EQ(0u, stats->decommittable_bytes);
    EXPECT_EQ(2 * SystemPageSize(), stats->discardable_bytes);
    EXPECT_EQ(2 * SystemPageSize(), stats->active_bytes);
    EXPECT_EQ(4 * SystemPageSize(), stats->resident_bytes);
  }
  CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset, true);
  CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset + SystemPageSize(), true);
  CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset + (SystemPageSize() * 2), true);
  CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset + (SystemPageSize() * 3), true);
  allocator.root()->PurgeMemory(PartitionPurgeDiscardUnusedSystemPages);
  EXPECT_EQ(2u, slot_span->num_unprovisioned_slots);
  CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset, true);
  CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset + SystemPageSize(), true);
  CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset + (SystemPageSize() * 2), false);
  CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset + (SystemPageSize() * 3), false);

  EXPECT_FALSE(slot_span->freelist_head);

  allocator.root()->Free(ptr1);
  allocator.root()->Free(ptr2);
}

TEST_F(PartitionAllocTest, ReallocMovesCookies) {
  // Resize so as to be sure to hit a "resize in place" case, and ensure that
  // use of the entire result is compatible with the debug mode's cookies, even
  // when the bucket size is large enough to span more than one partition page
  // and we can track the "raw" size. See https://crbug.com/709271
  static const size_t kSize = base::MaxRegularSlotSpanSize();
  void* ptr = allocator.root()->Alloc(kSize + 1, type_name);
  EXPECT_TRUE(ptr);

  memset(ptr, 0xbd, kSize + 1);
  ptr = allocator.root()->Realloc(ptr, kSize + 2, type_name);
  EXPECT_TRUE(ptr);

  memset(ptr, 0xbd, kSize + 2);
  allocator.root()->Free(ptr);
}

TEST_F(PartitionAllocTest, SmallReallocDoesNotMoveTrailingCookie) {
  // For crbug.com/781473
  static constexpr size_t kSize = 264;
  void* ptr = allocator.root()->Alloc(kSize, type_name);
  EXPECT_TRUE(ptr);

  ptr = allocator.root()->Realloc(ptr, kSize + 16, type_name);
  EXPECT_TRUE(ptr);

  allocator.root()->Free(ptr);
}

TEST_F(PartitionAllocTest, ZeroFill) {
  constexpr static size_t kAllZerosSentinel =
      std::numeric_limits<size_t>::max();
  for (size_t size : kTestSizes) {
    char* p = static_cast<char*>(
        allocator.root()->AllocFlags(PartitionAllocZeroFill, size, nullptr));
    size_t non_zero_position = kAllZerosSentinel;
    for (size_t i = 0; i < size; ++i) {
      if (0 != p[i]) {
        non_zero_position = i;
        break;
      }
    }
    EXPECT_EQ(kAllZerosSentinel, non_zero_position)
        << "test allocation size: " << size;
    allocator.root()->Free(p);
  }

  for (int i = 0; i < 10; ++i) {
    SCOPED_TRACE(i);
    AllocateRandomly(allocator.root(), 250, PartitionAllocZeroFill);
  }
}

TEST_F(PartitionAllocTest, Bug_897585) {
  // Need sizes big enough to be direct mapped and a delta small enough to
  // allow re-use of the slot span when cookied. These numbers fall out of the
  // test case in the indicated bug.
  size_t kInitialSize = 983040;
  size_t kDesiredSize = 983100;
  void* ptr = allocator.root()->AllocFlags(PartitionAllocReturnNull,
                                           kInitialSize, nullptr);
  ASSERT_NE(nullptr, ptr);
  ptr = allocator.root()->ReallocFlags(PartitionAllocReturnNull, ptr,
                                       kDesiredSize, nullptr);
  ASSERT_NE(nullptr, ptr);
  memset(ptr, 0xbd, kDesiredSize);
  allocator.root()->Free(ptr);
}

TEST_F(PartitionAllocTest, OverrideHooks) {
  constexpr size_t kOverriddenSize = 1234;
  constexpr const char* kOverriddenType = "Overridden type";
  constexpr unsigned char kOverriddenChar = 'A';

  // Marked static so that we can use them in non-capturing lambdas below.
  // (Non-capturing lambdas convert directly to function pointers.)
  static volatile bool free_called = false;
  static void* overridden_allocation = malloc(kOverriddenSize);
  memset(overridden_allocation, kOverriddenChar, kOverriddenSize);

  PartitionAllocHooks::SetOverrideHooks(
      [](void** out, int flags, size_t size, const char* type_name) -> bool {
        if (size == kOverriddenSize && type_name == kOverriddenType) {
          *out = overridden_allocation;
          return true;
        }
        return false;
      },
      [](void* address) -> bool {
        if (address == overridden_allocation) {
          free_called = true;
          return true;
        }
        return false;
      },
      [](size_t* out, void* address) -> bool {
        if (address == overridden_allocation) {
          *out = kOverriddenSize;
          return true;
        }
        return false;
      });

  void* ptr = allocator.root()->AllocFlags(PartitionAllocReturnNull,
                                           kOverriddenSize, kOverriddenType);
  ASSERT_EQ(ptr, overridden_allocation);

  allocator.root()->Free(ptr);
  EXPECT_TRUE(free_called);

  // overridden_allocation has not actually been freed so we can now immediately
  // realloc it.
  free_called = false;
  ptr =
      allocator.root()->ReallocFlags(PartitionAllocReturnNull, ptr, 1, nullptr);
  ASSERT_NE(ptr, nullptr);
  EXPECT_NE(ptr, overridden_allocation);
  EXPECT_TRUE(free_called);
  EXPECT_EQ(*(char*)ptr, kOverriddenChar);
  allocator.root()->Free(ptr);

  PartitionAllocHooks::SetOverrideHooks(nullptr, nullptr, nullptr);
  free(overridden_allocation);
}

TEST_F(PartitionAllocTest, Alignment) {
  std::vector<void*> allocated_ptrs;

  for (size_t size = 1; size <= base::PartitionPageSize(); size <<= 1) {
    // All allocations which are not direct-mapped occupy contiguous slots of a
    // span, starting on a page boundary. This means that allocations are first
    // rounded up to the nearest bucket size, then have an address of the form:
    //
    // (page-aligned address) + i * bucket_size.

    // All powers of two are bucket sizes, meaning that all power of two
    // allocations smaller than a page will be aligned on the allocation size.
    size_t expected_alignment = size;
#if DCHECK_IS_ON()
    // When DCHECK_IS_ON(), a kCookieSize cookie is added on both sides before
    // rounding up the allocation size. The returned pointer points after the
    // cookie.
    expected_alignment = std::min(expected_alignment, kCookieSize);
#endif
#if BUILDFLAG(USE_BACKUP_REF_PTR)
    // When BackupRefPtr is enabled, kInSlotRefCountBufferSize is added before
    // rounding up the allocation size, making the raw size not a power of two.
    // The returned pointer points after the ref-count (except when
    // PUT_REF_COUNT_IN_PREVIOUS_SLOT is on, but even then the size increase
    // by kInSlotRefCountBufferSize will cause non-1st allocations to be
    // misaligned).
    expected_alignment =
        std::min({expected_alignment, kInSlotRefCountBufferSize});
#endif
    for (int index = 0; index < 3; index++) {
      void* ptr = allocator.root()->Alloc(size, "");
      allocated_ptrs.push_back(ptr);
      EXPECT_EQ(0u, reinterpret_cast<uintptr_t>(ptr) % expected_alignment)
          << (index + 1) << "-th allocation of size=" << size;
    }
  }

  for (void* ptr : allocated_ptrs)
    allocator.root()->Free(ptr);
}

TEST_F(PartitionAllocTest, FundamentalAlignment) {
  // See the test above for details. Essentially, checking the bucket size is
  // sufficient to ensure that alignment will always be respected, as long as
  // the fundamental alignment is <= 16 bytes.
  size_t fundamental_alignment = base::kAlignment;
  for (size_t size = 0; size < base::SystemPageSize(); size++) {
    // Allocate several pointers, as the first one in use in a size class will
    // be aligned on a page boundary.
    void* ptr = allocator.root()->Alloc(size, "");
    void* ptr2 = allocator.root()->Alloc(size, "");
    void* ptr3 = allocator.root()->Alloc(size, "");

    EXPECT_EQ(reinterpret_cast<uintptr_t>(ptr) % fundamental_alignment,
              static_cast<uintptr_t>(0));
    EXPECT_EQ(reinterpret_cast<uintptr_t>(ptr2) % fundamental_alignment,
              static_cast<uintptr_t>(0));
    EXPECT_EQ(reinterpret_cast<uintptr_t>(ptr3) % fundamental_alignment,
              static_cast<uintptr_t>(0));

#if BUILDFLAG(PUT_REF_COUNT_IN_PREVIOUS_SLOT)
    // The capacity(C) is slot size - kExtraAllocSize.
    // Since slot size is multiples of base::kAlignment,
    // C % kAlignment == (slot_size - kExtraAllocSize) % kAlignment.
    // C % kAlignment == (-kExtraAllocSize) % kAlignment.
    // Since kCookieSize * 2 is multiples of kAlignment,
    // C % kAlignment == (-kInSlotRefCountBufferSize) % kAlignment
    // == (kAlignment - kInSlotRefCountBufferSize) % kAlignment.
    EXPECT_EQ(allocator.root()->AllocationCapacityFromPtr(ptr) %
                  fundamental_alignment,
              static_cast<uintptr_t>(fundamental_alignment -
                                     kInSlotRefCountBufferSize));
#else
    EXPECT_EQ(allocator.root()->AllocationCapacityFromPtr(ptr) %
                  fundamental_alignment,
              static_cast<uintptr_t>(0));
#endif

    allocator.root()->Free(ptr);
    allocator.root()->Free(ptr2);
    allocator.root()->Free(ptr3);
  }
}

void VerifyAlignment(PartitionRoot<ThreadSafe>* root,
                     size_t size,
                     size_t alignment) {
  std::vector<void*> allocated_ptrs;

  for (int index = 0; index < 3; index++) {
    void* ptr = root->AlignedAllocFlags(0, alignment, size);
    ASSERT_TRUE(ptr);
    allocated_ptrs.push_back(ptr);
    EXPECT_EQ(0ull, reinterpret_cast<uintptr_t>(ptr) % alignment)
        << (index + 1) << "-th allocation of size=" << size
        << ", alignment=" << alignment;
  }

  for (void* ptr : allocated_ptrs)
    PartitionRoot<ThreadSafe>::Free(ptr);
}

TEST_F(PartitionAllocTest, AlignedAllocations) {
  size_t alloc_sizes[] = {1,
                          10,
                          100,
                          1000,
                          10000,
                          60000,
                          70000,
                          130000,
                          500000,
                          900000,
                          kMaxBucketed + 1,
                          2 * kMaxBucketed,
                          base::kSuperPageSize - 2 * PartitionPageSize(),
                          10 * kMaxBucketed};
  for (size_t alloc_size : alloc_sizes) {
    for (size_t alignment = 1; alignment <= kMaxSupportedAlignment;
         alignment <<= 1) {
      VerifyAlignment(aligned_allocator.root(), alloc_size, alignment);

      // AlignedAllocFlags() can't be called on regular allocator, if there are
      // extras before the allocation. Extras after the allocation are ok
      // (PUT_REF_COUNT_IN_PREVIOUS_SLOT) and this is what's being tested here.
#if !DCHECK_IS_ON() && (!BUILDFLAG(USE_BACKUP_REF_PTR) || \
                        BUILDFLAG(PUT_REF_COUNT_IN_PREVIOUS_SLOT))
      VerifyAlignment(allocator.root(), alloc_size, alignment);
#endif
    }
  }
}

// Test that the optimized `GetSlotNumber` implementation produces valid
// results.
TEST_F(PartitionAllocTest, OptimizedGetSlotNumber) {
  for (auto& bucket : allocator.root()->buckets) {
    for (size_t slot = 0, offset = bucket.slot_size / 2;
         slot < bucket.get_slots_per_span();
         ++slot, offset += bucket.slot_size) {
      EXPECT_EQ(slot, bucket.GetSlotNumber(offset));
    }
  }
}

TEST_F(PartitionAllocTest, GetUsableSizeNull) {
  EXPECT_EQ(0ULL, PartitionRoot<ThreadSafe>::GetUsableSize(nullptr));
}

TEST_F(PartitionAllocTest, GetUsableSize) {
  size_t delta = SystemPageSize() + 1;
  for (size_t size = 1; size <= kMinDirectMappedDownsize; size += delta) {
    void* ptr = allocator.root()->Alloc(size, "");
    EXPECT_TRUE(ptr);
    size_t usable_size = PartitionRoot<ThreadSafe>::GetUsableSize(ptr);
    EXPECT_LE(size, usable_size);
    memset(ptr, 0xDE, usable_size);
    // Should not crash when free the ptr.
    allocator.root()->Free(ptr);
  }
}

#if defined(OS_MAC) && defined(ARCH_CPU_ARM64)
// Disabled pending investigation: https://crbug.com/1154345
#define MAYBE_Bookkeeping DISABLED_Bookkeeping
#else
#define MAYBE_Bookkeeping Bookkeeping
#endif
TEST_F(PartitionAllocTest, MAYBE_Bookkeeping) {
  auto& root = *allocator.root();

  EXPECT_EQ(0U, root.total_size_of_committed_pages);
  EXPECT_EQ(0U, root.max_size_of_committed_pages);
  EXPECT_EQ(0U, root.get_total_size_of_allocated_bytes());
  EXPECT_EQ(0U, root.get_max_size_of_allocated_bytes());
  EXPECT_EQ(0U, root.total_size_of_super_pages);
  size_t small_size = 1000;

  // A full slot span of size 1 partition page is committed.
  void* ptr = root.Alloc(small_size - kExtraAllocSize, type_name);
  // Lazy commit commits only needed pages.
  size_t expected_committed_size = allocator.root()->use_lazy_commit
                                       ? SystemPageSize()
                                       : PartitionPageSize();
  size_t expected_super_pages_size = kSuperPageSize;
  size_t expected_max_committed_size = expected_committed_size;
  size_t bucket_index = SizeToIndex(small_size - kExtraAllocSize);
  PartitionBucket<base::internal::ThreadSafe>* bucket =
      &root.buckets[bucket_index];
  size_t expected_total_allocated_size = bucket->slot_size;
  size_t expected_max_allocated_size = expected_total_allocated_size;

  EXPECT_EQ(expected_committed_size, root.total_size_of_committed_pages);
  EXPECT_EQ(expected_max_committed_size, root.max_size_of_committed_pages);
  EXPECT_EQ(expected_total_allocated_size,
            root.get_total_size_of_allocated_bytes());
  EXPECT_EQ(expected_max_allocated_size,
            root.get_max_size_of_allocated_bytes());
  EXPECT_EQ(expected_super_pages_size, root.total_size_of_super_pages);

  // Freeing memory doesn't result in decommitting pages right away.
  root.Free(ptr);
  expected_total_allocated_size = 0U;
  EXPECT_EQ(expected_committed_size, root.total_size_of_committed_pages);
  EXPECT_EQ(expected_max_committed_size, root.max_size_of_committed_pages);
  EXPECT_EQ(expected_total_allocated_size,
            root.get_total_size_of_allocated_bytes());
  EXPECT_EQ(expected_max_allocated_size,
            root.get_max_size_of_allocated_bytes());
  EXPECT_EQ(expected_super_pages_size, root.total_size_of_super_pages);

  // Allocating the same size lands it in the same slot span.
  ptr = root.Alloc(small_size - kExtraAllocSize, type_name);
  EXPECT_EQ(expected_committed_size, root.total_size_of_committed_pages);
  EXPECT_EQ(expected_max_committed_size, root.max_size_of_committed_pages);
  EXPECT_EQ(expected_max_allocated_size,
            root.get_max_size_of_allocated_bytes());
  EXPECT_EQ(expected_super_pages_size, root.total_size_of_super_pages);

  // Freeing memory doesn't result in decommitting pages right away.
  root.Free(ptr);
  EXPECT_EQ(expected_committed_size, root.total_size_of_committed_pages);
  EXPECT_EQ(expected_max_committed_size, root.max_size_of_committed_pages);
  EXPECT_EQ(expected_max_allocated_size,
            root.get_max_size_of_allocated_bytes());
  EXPECT_EQ(expected_super_pages_size, root.total_size_of_super_pages);

  // Allocating another size commits another slot span.
  ptr = root.Alloc(2 * small_size - kExtraAllocSize, type_name);
  expected_committed_size += allocator.root()->use_lazy_commit
                                 ? SystemPageSize()
                                 : PartitionPageSize();
  expected_max_committed_size =
      std::max(expected_max_committed_size, expected_committed_size);
  expected_max_allocated_size =
      std::max(expected_max_allocated_size, static_cast<size_t>(2048));
  EXPECT_EQ(expected_committed_size, root.total_size_of_committed_pages);
  EXPECT_EQ(expected_max_committed_size, root.max_size_of_committed_pages);
  EXPECT_EQ(expected_max_allocated_size,
            root.get_max_size_of_allocated_bytes());
  EXPECT_EQ(expected_super_pages_size, root.total_size_of_super_pages);

  // Freeing memory doesn't result in decommitting pages right away.
  root.Free(ptr);
  EXPECT_EQ(expected_committed_size, root.total_size_of_committed_pages);
  EXPECT_EQ(expected_max_committed_size, root.max_size_of_committed_pages);
  EXPECT_EQ(expected_max_allocated_size,
            root.get_max_size_of_allocated_bytes());
  EXPECT_EQ(expected_super_pages_size, root.total_size_of_super_pages);

  // Single-slot slot spans...
  size_t big_size = kMaxBucketed - SystemPageSize();
  bucket_index = SizeToIndex(big_size - kExtraAllocSize);
  bucket = &root.buckets[bucket_index];
  ASSERT_LT(big_size, bucket->get_bytes_per_span());
  ASSERT_NE(big_size % PartitionPageSize(), 0U);
  ptr = root.Alloc(big_size - kExtraAllocSize, type_name);
  expected_committed_size += bucket->get_bytes_per_span();
  expected_max_committed_size =
      std::max(expected_max_committed_size, expected_committed_size);
  expected_total_allocated_size += bucket->get_bytes_per_span();
  expected_max_allocated_size =
      std::max(expected_max_allocated_size, expected_total_allocated_size);
  EXPECT_EQ(expected_committed_size, root.total_size_of_committed_pages);
  EXPECT_EQ(expected_max_committed_size, root.max_size_of_committed_pages);
  EXPECT_EQ(expected_total_allocated_size,
            root.get_total_size_of_allocated_bytes());
  EXPECT_EQ(expected_max_allocated_size,
            root.get_max_size_of_allocated_bytes());
  EXPECT_EQ(expected_super_pages_size, root.total_size_of_super_pages);

  // Allocating 2nd time doesn't overflow the super page...
  void* ptr2 = root.Alloc(big_size - kExtraAllocSize, type_name);
  expected_committed_size += bucket->get_bytes_per_span();
  expected_max_committed_size =
      std::max(expected_max_committed_size, expected_committed_size);
  expected_total_allocated_size += bucket->get_bytes_per_span();
  expected_max_allocated_size =
      std::max(expected_max_allocated_size, expected_total_allocated_size);
  EXPECT_EQ(expected_committed_size, root.total_size_of_committed_pages);
  EXPECT_EQ(expected_max_committed_size, root.max_size_of_committed_pages);
  EXPECT_EQ(expected_total_allocated_size,
            root.get_total_size_of_allocated_bytes());
  EXPECT_EQ(expected_max_allocated_size,
            root.get_max_size_of_allocated_bytes());
  EXPECT_EQ(expected_super_pages_size, root.total_size_of_super_pages);

  // ... but 3rd time does.
  void* ptr3 = root.Alloc(big_size - kExtraAllocSize, type_name);
  expected_committed_size += bucket->get_bytes_per_span();
  expected_max_committed_size =
      std::max(expected_max_committed_size, expected_committed_size);
  expected_total_allocated_size += bucket->get_bytes_per_span();
  expected_max_allocated_size =
      std::max(expected_max_allocated_size, expected_total_allocated_size);
  expected_super_pages_size += kSuperPageSize;
  EXPECT_EQ(expected_committed_size, root.total_size_of_committed_pages);
  EXPECT_EQ(expected_max_committed_size, root.max_size_of_committed_pages);
  EXPECT_EQ(expected_total_allocated_size,
            root.get_total_size_of_allocated_bytes());
  EXPECT_EQ(expected_max_allocated_size,
            root.get_max_size_of_allocated_bytes());
  EXPECT_EQ(expected_super_pages_size, root.total_size_of_super_pages);

  // Freeing memory doesn't result in decommitting pages right away.
  root.Free(ptr);
  root.Free(ptr2);
  root.Free(ptr3);
  expected_total_allocated_size -= 3 * bucket->get_bytes_per_span();
  expected_max_allocated_size =
      std::max(expected_max_allocated_size, expected_total_allocated_size);
  EXPECT_EQ(expected_committed_size, root.total_size_of_committed_pages);
  EXPECT_EQ(expected_max_committed_size, root.max_size_of_committed_pages);
  EXPECT_EQ(expected_total_allocated_size,
            root.get_total_size_of_allocated_bytes());
  EXPECT_EQ(expected_max_allocated_size,
            root.get_max_size_of_allocated_bytes());
  EXPECT_EQ(expected_super_pages_size, root.total_size_of_super_pages);

  // Now everything should be decommitted. The reserved space for super pages
  // stays the same and will never go away (by design).
  root.PurgeMemory(PartitionPurgeDecommitEmptySlotSpans);
  expected_committed_size = 0;
  EXPECT_EQ(expected_committed_size, root.total_size_of_committed_pages);
  EXPECT_EQ(expected_max_committed_size, root.max_size_of_committed_pages);
  EXPECT_EQ(expected_total_allocated_size,
            root.get_total_size_of_allocated_bytes());
  EXPECT_EQ(expected_max_allocated_size,
            root.get_max_size_of_allocated_bytes());
  EXPECT_EQ(expected_super_pages_size, root.total_size_of_super_pages);

  // None of the above should affect the direct map space.
  EXPECT_EQ(0U, root.total_size_of_direct_mapped_pages);

  size_t huge_sizes[] = {
      kMaxBucketed + SystemPageSize(),
      kMaxBucketed + SystemPageSize() + 123,
      kSuperPageSize - PageAllocationGranularity(),
      kSuperPageSize - SystemPageSize() - PartitionPageSize(),
      kSuperPageSize - PartitionPageSize(),
      kSuperPageSize - SystemPageSize(),
      kSuperPageSize,
      kSuperPageSize + SystemPageSize(),
      kSuperPageSize + PartitionPageSize(),
      kSuperPageSize + SystemPageSize() + PartitionPageSize(),
      kSuperPageSize + PageAllocationGranularity(),
      kSuperPageSize + DirectMapAllocationGranularity(),
  };
  size_t alignments[] = {
      PartitionPageSize(),
      2 * PartitionPageSize(),
      kMaxSupportedAlignment / 2,
      kMaxSupportedAlignment,
  };
  for (size_t huge_size : huge_sizes) {
    for (size_t alignment : alignments) {
      // For direct map, we commit only as many pages as needed.
      size_t aligned_size = bits::AlignUp(huge_size, SystemPageSize());
      ptr = root.AllocFlagsInternal(0, huge_size - kExtraAllocSize, alignment,
                                    type_name);
      expected_committed_size += aligned_size;
      expected_max_committed_size =
          std::max(expected_max_committed_size, expected_committed_size);
      expected_total_allocated_size += aligned_size;
      expected_max_allocated_size =
          std::max(expected_max_allocated_size, expected_total_allocated_size);
      // The total reserved map includes metadata and guard pages at the ends.
      // It also includes alignment. However, these would double count the first
      // partition page, so it needs to be subtracted.
      size_t surrounding_pages_size =
          PartitionRoot<ThreadSafe>::GetDirectMapMetadataAndGuardPagesSize() +
          alignment - PartitionPageSize();
      size_t expected_direct_map_size =
          bits::AlignUp(aligned_size + surrounding_pages_size,
                        DirectMapAllocationGranularity());
      EXPECT_EQ(expected_committed_size, root.total_size_of_committed_pages);
      EXPECT_EQ(expected_max_committed_size, root.max_size_of_committed_pages);
      EXPECT_EQ(expected_total_allocated_size,
                root.get_total_size_of_allocated_bytes());
      EXPECT_EQ(expected_max_allocated_size,
                root.get_max_size_of_allocated_bytes());
      EXPECT_EQ(expected_super_pages_size, root.total_size_of_super_pages);
      EXPECT_EQ(expected_direct_map_size,
                root.total_size_of_direct_mapped_pages);

      // Freeing memory in the diret map decommits pages right away. The address
      // space is released for re-use too.
      root.Free(ptr);
      expected_committed_size -= aligned_size;
      expected_direct_map_size = 0;
      expected_max_committed_size =
          std::max(expected_max_committed_size, expected_committed_size);
      expected_total_allocated_size -= aligned_size;
      expected_max_allocated_size =
          std::max(expected_max_allocated_size, expected_total_allocated_size);
      EXPECT_EQ(expected_committed_size, root.total_size_of_committed_pages);
      EXPECT_EQ(expected_max_committed_size, root.max_size_of_committed_pages);
      EXPECT_EQ(expected_total_allocated_size,
                root.get_total_size_of_allocated_bytes());
      EXPECT_EQ(expected_max_allocated_size,
                root.get_max_size_of_allocated_bytes());
      EXPECT_EQ(expected_super_pages_size, root.total_size_of_super_pages);
      EXPECT_EQ(expected_direct_map_size,
                root.total_size_of_direct_mapped_pages);
    }
  }
}

#if BUILDFLAG(USE_BACKUP_REF_PTR)

TEST_F(PartitionAllocTest, RefCountBasic) {
  constexpr uint64_t kCookie = 0x1234567890ABCDEF;
  constexpr uint64_t kQuarantined = 0xEFEFEFEFEFEFEFEF;

  size_t alloc_size = 64 - kExtraAllocSize;
  uint64_t* ptr1 = reinterpret_cast<uint64_t*>(
      allocator.root()->Alloc(alloc_size, type_name));
  EXPECT_TRUE(ptr1);

  *ptr1 = kCookie;

  auto* ref_count =
      PartitionRefCountPointer(reinterpret_cast<char*>(ptr1) - kPointerOffset);
  EXPECT_TRUE(ref_count->IsAliveWithNoKnownRefs());

  ref_count->Acquire();
  EXPECT_FALSE(ref_count->Release());
  EXPECT_TRUE(ref_count->IsAliveWithNoKnownRefs());
  EXPECT_EQ(*ptr1, kCookie);

  ref_count->Acquire();
  EXPECT_FALSE(ref_count->IsAliveWithNoKnownRefs());

  allocator.root()->Free(ptr1);
  // The allocation shouldn't be reclaimed, and its contents should be zapped.
  EXPECT_NE(*ptr1, kCookie);
  EXPECT_EQ(*ptr1, kQuarantined);

  // The allocator should not reuse the original slot since its reference count
  // doesn't equal zero.
  uint64_t* ptr2 = reinterpret_cast<uint64_t*>(
      allocator.root()->Alloc(alloc_size, type_name));
  EXPECT_NE(ptr1, ptr2);
  allocator.root()->Free(ptr2);

  // When the last reference is released, the slot should become reusable.
  EXPECT_TRUE(ref_count->Release());
  PartitionAllocFreeForRefCounting(reinterpret_cast<char*>(ptr1) -
                                   kPointerOffset);
  uint64_t* ptr3 = reinterpret_cast<uint64_t*>(
      allocator.root()->Alloc(alloc_size, type_name));
  EXPECT_EQ(ptr1, ptr3);
  allocator.root()->Free(ptr3);
}

void PartitionAllocTest::RunRefCountReallocSubtest(size_t orig_size,
                                                   size_t new_size) {
  void* ptr1 = allocator.root()->Alloc(orig_size, type_name);
  EXPECT_TRUE(ptr1);

  auto* ref_count1 =
      PartitionRefCountPointer(reinterpret_cast<char*>(ptr1) - kPointerOffset);
  EXPECT_TRUE(ref_count1->IsAliveWithNoKnownRefs());

  ref_count1->Acquire();
  EXPECT_FALSE(ref_count1->IsAliveWithNoKnownRefs());

  void* ptr2 = allocator.root()->Realloc(ptr1, new_size, type_name);
  EXPECT_TRUE(ptr2);

  // Re-query ref-count. It may have moved if Realloc changed the slot.
  auto* ref_count2 =
      PartitionRefCountPointer(reinterpret_cast<char*>(ptr2) - kPointerOffset);

  if (ptr1 == ptr2) {
    // If the slot didn't change, ref-count should stay the same.
    EXPECT_EQ(ref_count1, ref_count2);
    EXPECT_FALSE(ref_count2->IsAliveWithNoKnownRefs());

    EXPECT_FALSE(ref_count2->Release());
  } else {
    // If the allocation was moved to another slot, the old ref-count stayed
    // in the same location in memory, is no longer alive, but still has a
    // reference. The new ref-count is alive, but has no references.
    EXPECT_NE(ref_count1, ref_count2);
    EXPECT_FALSE(ref_count1->IsAlive());
    EXPECT_FALSE(ref_count1->IsAliveWithNoKnownRefs());
    EXPECT_TRUE(ref_count2->IsAliveWithNoKnownRefs());

    EXPECT_TRUE(ref_count1->Release());
  }

  allocator.root()->Free(ptr2);
}

TEST_F(PartitionAllocTest, RefCountRealloc) {
  size_t alloc_sizes[] = {500, 5000, 50000, 400000};

  for (size_t alloc_size : alloc_sizes) {
    alloc_size -= kExtraAllocSize;
    RunRefCountReallocSubtest(alloc_size, alloc_size - 9);
    RunRefCountReallocSubtest(alloc_size, alloc_size + 9);
    RunRefCountReallocSubtest(alloc_size, alloc_size * 2);
    RunRefCountReallocSubtest(alloc_size, alloc_size / 2);
  }
}

#endif  // BUILDFLAG(USE_BACKUP_REF_PTR)

TEST_F(PartitionAllocTest, ReservationOffset) {
  // For normal buckets, offset should be kOffsetTagNormalBuckets.
  void* ptr = allocator.root()->Alloc(kTestAllocSize, type_name);
  EXPECT_TRUE(ptr);
  uintptr_t ptr_as_uintptr = reinterpret_cast<uintptr_t>(ptr);
  EXPECT_EQ(kOffsetTagNormalBuckets, *ReservationOffsetPointer(ptr_as_uintptr));
  allocator.root()->Free(ptr);

  // For not yet allocated memory, offset should be kOffsetTagNotAllocated.
  EXPECT_EQ(kOffsetTagNotAllocated,
            *ReservationOffsetPointer(ptr_as_uintptr + 100 * kSuperPageSize));

  // For direct-map,
  size_t large_size = kSuperPageSize * 5 + PartitionPageSize() * .5f;
  ptr = allocator.root()->Alloc(large_size, type_name);
  EXPECT_TRUE(ptr);
  ptr_as_uintptr = reinterpret_cast<uintptr_t>(ptr);
  EXPECT_EQ(0U, *ReservationOffsetPointer(ptr_as_uintptr));
  EXPECT_EQ(1U, *ReservationOffsetPointer(ptr_as_uintptr + kSuperPageSize));
  EXPECT_EQ(2U, *ReservationOffsetPointer(ptr_as_uintptr + kSuperPageSize * 2));
  EXPECT_EQ(3U, *ReservationOffsetPointer(ptr_as_uintptr + kSuperPageSize * 3));
  EXPECT_EQ(4U, *ReservationOffsetPointer(ptr_as_uintptr + kSuperPageSize * 4));
  EXPECT_EQ(5U, *ReservationOffsetPointer(ptr_as_uintptr + kSuperPageSize * 5));

  // In-place realloc doesn't affect the offsets.
  void* new_ptr = allocator.root()->Realloc(ptr, large_size * .8, type_name);
  EXPECT_EQ(new_ptr, ptr);
  EXPECT_EQ(0U, *ReservationOffsetPointer(ptr_as_uintptr));
  EXPECT_EQ(1U, *ReservationOffsetPointer(ptr_as_uintptr + kSuperPageSize));
  EXPECT_EQ(2U, *ReservationOffsetPointer(ptr_as_uintptr + kSuperPageSize * 2));
  EXPECT_EQ(3U, *ReservationOffsetPointer(ptr_as_uintptr + kSuperPageSize * 3));
  EXPECT_EQ(4U, *ReservationOffsetPointer(ptr_as_uintptr + kSuperPageSize * 4));
  EXPECT_EQ(5U, *ReservationOffsetPointer(ptr_as_uintptr + kSuperPageSize * 5));

  allocator.root()->Free(ptr);
  // After free, the offsets must be kOffsetTagNotAllocated.
  EXPECT_EQ(kOffsetTagNotAllocated, *ReservationOffsetPointer(ptr_as_uintptr));
  EXPECT_EQ(kOffsetTagNotAllocated,
            *ReservationOffsetPointer(ptr_as_uintptr + kSuperPageSize));
  EXPECT_EQ(kOffsetTagNotAllocated,
            *ReservationOffsetPointer(ptr_as_uintptr + kSuperPageSize * 2));
  EXPECT_EQ(kOffsetTagNotAllocated,
            *ReservationOffsetPointer(ptr_as_uintptr + kSuperPageSize * 3));
  EXPECT_EQ(kOffsetTagNotAllocated,
            *ReservationOffsetPointer(ptr_as_uintptr + kSuperPageSize * 4));
  EXPECT_EQ(kOffsetTagNotAllocated,
            *ReservationOffsetPointer(ptr_as_uintptr + kSuperPageSize * 5));
}

TEST_F(PartitionAllocTest, GetReservationStart) {
  size_t large_size = kSuperPageSize * 3 + PartitionPageSize() * .5f;
  void* ptr = allocator.root()->Alloc(large_size, type_name);
  EXPECT_TRUE(ptr);
  void* slot_start = allocator.root()->AdjustPointerForExtrasSubtract(ptr);
  uintptr_t reservation_start =
      reinterpret_cast<uintptr_t>(slot_start) - PartitionPageSize();
  EXPECT_EQ(0U, reservation_start & DirectMapAllocationGranularityOffsetMask());

  for (char* p = static_cast<char*>(ptr); p < (char*)ptr + large_size; ++p) {
    void* ptr2 = reinterpret_cast<char*>(GetDirectMapReservationStart(p)) +
                 PartitionPageSize();
    EXPECT_EQ(slot_start, ptr2);
  }

  EXPECT_EQ(reservation_start, reinterpret_cast<uintptr_t>(
                                   GetDirectMapReservationStart(slot_start)));

  allocator.root()->Free(ptr);
}

TEST_F(PartitionAllocTest, CheckReservationType) {
  char* ptr = reinterpret_cast<char*>(
      allocator.root()->Alloc(kTestAllocSize, type_name));
  EXPECT_TRUE(ptr);
  void* ptr_to_check = ptr;
  EXPECT_FALSE(IsReservationStart(ptr_to_check));
  EXPECT_TRUE(IsManagedByNormalBuckets(ptr_to_check));
  EXPECT_FALSE(IsManagedByDirectMap(ptr_to_check));
  EXPECT_TRUE(IsManagedByNormalBucketsOrDirectMap(ptr_to_check));
  ptr_to_check = ptr + kTestAllocSize - 1;
  EXPECT_FALSE(IsReservationStart(ptr_to_check));
  EXPECT_TRUE(IsManagedByNormalBuckets(ptr_to_check));
  EXPECT_FALSE(IsManagedByDirectMap(ptr_to_check));
  EXPECT_TRUE(IsManagedByNormalBucketsOrDirectMap(ptr_to_check));
  ptr_to_check = bits::AlignDown(ptr, kSuperPageSize);
  EXPECT_TRUE(IsReservationStart(ptr_to_check));
  EXPECT_TRUE(IsManagedByNormalBuckets(ptr_to_check));
  EXPECT_FALSE(IsManagedByDirectMap(ptr_to_check));
  EXPECT_TRUE(IsManagedByNormalBucketsOrDirectMap(ptr_to_check));
  allocator.root()->Free(ptr);
  // Freeing keeps a normal-bucket super page in memory.
  ptr_to_check = bits::AlignDown(ptr, kSuperPageSize);
  EXPECT_TRUE(IsReservationStart(ptr_to_check));
  EXPECT_TRUE(IsManagedByNormalBuckets(ptr_to_check));
  EXPECT_FALSE(IsManagedByDirectMap(ptr_to_check));
  EXPECT_TRUE(IsManagedByNormalBucketsOrDirectMap(ptr_to_check));
  // Pick a likely unallocated super page.
  ptr_to_check = bits::AlignUp(ptr, kSuperPageSize) + 100 * kSuperPageSize;
#if DCHECK_IS_ON()
  // Expect to DCHECK on unallocated region.
  EXPECT_DEATH_IF_SUPPORTED(IsReservationStart(ptr_to_check), "");
#endif
  EXPECT_FALSE(IsManagedByNormalBuckets(ptr_to_check));
  EXPECT_FALSE(IsManagedByDirectMap(ptr_to_check));
  EXPECT_FALSE(IsManagedByNormalBucketsOrDirectMap(ptr_to_check));

  size_t large_size = 2 * kSuperPageSize;
  ptr = reinterpret_cast<char*>(allocator.root()->Alloc(large_size, type_name));
  EXPECT_TRUE(ptr);
  ptr_to_check = ptr;
  EXPECT_FALSE(IsReservationStart(ptr_to_check));
  EXPECT_FALSE(IsManagedByNormalBuckets(ptr_to_check));
  EXPECT_TRUE(IsManagedByDirectMap(ptr_to_check));
  EXPECT_TRUE(IsManagedByNormalBucketsOrDirectMap(ptr_to_check));
  ptr_to_check = bits::AlignUp(ptr, kSuperPageSize);
  EXPECT_FALSE(IsReservationStart(ptr_to_check));
  EXPECT_FALSE(IsManagedByNormalBuckets(ptr_to_check));
  EXPECT_TRUE(IsManagedByDirectMap(ptr_to_check));
  EXPECT_TRUE(IsManagedByNormalBucketsOrDirectMap(ptr_to_check));
  ptr_to_check = ptr + large_size - 1;
  EXPECT_FALSE(IsReservationStart(ptr_to_check));
  EXPECT_FALSE(IsManagedByNormalBuckets(ptr_to_check));
  EXPECT_TRUE(IsManagedByDirectMap(ptr_to_check));
  EXPECT_TRUE(IsManagedByNormalBucketsOrDirectMap(ptr_to_check));
  ptr_to_check = bits::AlignDown(ptr, kSuperPageSize);
  EXPECT_TRUE(IsReservationStart(ptr_to_check));
  EXPECT_FALSE(IsManagedByNormalBuckets(ptr_to_check));
  EXPECT_TRUE(IsManagedByDirectMap(ptr_to_check));
  EXPECT_TRUE(IsManagedByNormalBucketsOrDirectMap(ptr_to_check));
  allocator.root()->Free(ptr);
  // Freeing releases direct-map super pages.
  ptr_to_check = bits::AlignDown(ptr, kSuperPageSize);
#if DCHECK_IS_ON()
  // Expect to DCHECK on unallocated region.
  EXPECT_DEATH_IF_SUPPORTED(IsReservationStart(ptr_to_check), "");
#endif
  EXPECT_FALSE(IsManagedByNormalBuckets(ptr_to_check));
  EXPECT_FALSE(IsManagedByDirectMap(ptr_to_check));
  EXPECT_FALSE(IsManagedByNormalBucketsOrDirectMap(ptr_to_check));
}

// Test for crash http://crbug.com/1169003.
TEST_F(PartitionAllocTest, CrossPartitionRootRealloc) {
  // Size is large enough to satisfy it from a single-slot slot span
  size_t test_size = MaxRegularSlotSpanSize() - kExtraAllocSize;
  void* ptr = allocator.root()->AllocFlags(PartitionAllocReturnNull, test_size,
                                           nullptr);
  EXPECT_TRUE(ptr);

  // Create new root to simulate ConfigurePartitionRefCountSupport(false)

  // Copied from ConfigurePartitionRefCountSupport()
  allocator.root()->PurgeMemory(PartitionPurgeDecommitEmptySlotSpans |
                                PartitionPurgeDiscardUnusedSystemPages);

  // Create a new root
  auto* new_root = new base::PartitionRoot<base::internal::ThreadSafe>({
      base::PartitionOptions::AlignedAlloc::kDisallowed,
      base::PartitionOptions::ThreadCache::kDisabled,
      base::PartitionOptions::Quarantine::kDisallowed,
      base::PartitionOptions::Cookies::kAllowed,
      base::PartitionOptions::RefCount::kDisallowed,
  });

  // Realloc from |allocator.root()| into |new_root|.
  void* ptr2 = new_root->ReallocFlags(PartitionAllocReturnNull, ptr,
                                      test_size + 1024, nullptr);
  EXPECT_TRUE(ptr2);
  EXPECT_NE(ptr, ptr2);
}

TEST_F(PartitionAllocTest, FastPathOrReturnNull) {
  size_t allocation_size = 64;
  // The very first allocation is never a fast path one, since it needs a new
  // super page and a new partition page.
  EXPECT_FALSE(allocator.root()->AllocFlags(PartitionAllocFastPathOrReturnNull,
                                            allocation_size, ""));
  void* ptr = allocator.root()->AllocFlags(0, allocation_size, "");
  ASSERT_TRUE(ptr);

  // Next one is, since the partition page has been activated.
  void* ptr2 = allocator.root()->AllocFlags(PartitionAllocFastPathOrReturnNull,
                                            allocation_size, "");
  EXPECT_TRUE(ptr2);

  // First allocation of a different bucket is slow.
  EXPECT_FALSE(allocator.root()->AllocFlags(PartitionAllocFastPathOrReturnNull,
                                            2 * allocation_size, ""));

  size_t allocated_size = 2 * allocation_size;
  std::vector<void*> ptrs;
  while (void* new_ptr = allocator.root()->AllocFlags(
             PartitionAllocFastPathOrReturnNull, allocation_size, "")) {
    ptrs.push_back(new_ptr);
    allocated_size += allocation_size;
  }
  EXPECT_LE(allocated_size,
            PartitionPageSize() * kMaxPartitionPagesPerRegularSlotSpan);

  for (void* ptr_to_free : ptrs)
    allocator.root()->FreeNoHooks(ptr_to_free);

  allocator.root()->FreeNoHooks(ptr);
  allocator.root()->FreeNoHooks(ptr2);
}

// Death tests misbehave on Android, http://crbug.com/643760.
#if defined(GTEST_HAS_DEATH_TEST) && !defined(OS_ANDROID)
#if !defined(OFFICIAL_BUILD) || !defined(NDEBUG)

TEST_F(PartitionAllocDeathTest, CheckTriggered) {
  using ::testing::ContainsRegex;
#if DCHECK_IS_ON()
  EXPECT_DEATH(PA_CHECK(5 == 7), ContainsRegex("Check failed.*5 == 7"));
#endif
  EXPECT_DEATH(PA_CHECK(5 == 7), ContainsRegex("Check failed.*5 == 7"));
}

#endif  // !defined(OFFICIAL_BUILD) && !defined(NDEBUG)
#endif  // defined(GTEST_HAS_DEATH_TEST) && !defined(OS_ANDROID)

// Not on chromecast, since gtest considers extra output from itself as a test
// failure:
// https://ci.chromium.org/ui/p/chromium/builders/ci/Cast%20Audio%20Linux/98492/overview
#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) &&              \
    defined(GTEST_HAS_DEATH_TEST) && !defined(OS_ANDROID) && \
    !BUILDFLAG(IS_CHROMECAST)

namespace {

class LambdaThreadDelegate : public PlatformThread::Delegate {
 public:
  explicit LambdaThreadDelegate(RepeatingClosure f) : f_(f) {}
  void ThreadMain() override { f_.Run(); }

 private:
  RepeatingClosure f_;
};

NOINLINE void FreeForTest(void* data) {
  free(data);
}

}  // namespace

// Disabled because executing it causes Gtest to show a warning in the output,
// which confuses the runner on some platforms, making the test report an
// "UNKNOWN" status even though it succeeded.
TEST_F(PartitionAllocTest, DISABLED_PreforkHandler) {
  std::atomic<bool> please_stop;
  std::atomic<int> started_threads{0};

  // Continuously allocates / frees memory, bypassing the thread cache. This
  // makes it likely that this thread will own the lock, and that the
  // EXPECT_EXIT() part will deadlock.
  constexpr size_t kAllocSize = ThreadCache::kLargeSizeThreshold + 1;
  LambdaThreadDelegate delegate{BindLambdaForTesting([&]() {
    started_threads++;
    while (!please_stop.load(std::memory_order_relaxed)) {
      void* ptr = malloc(kAllocSize);

      // A simple malloc() / free() pair can be discarded by the compiler (and
      // is), making the test fail. It is sufficient to make |FreeForTest()| a
      // NOINLINE function for the call to not be eliminated, but it is
      // required.
      FreeForTest(ptr);
    }
  })};

  constexpr int kThreads = 4;
  PlatformThreadHandle thread_handles[kThreads];
  for (int i = 0; i < kThreads; i++) {
    PlatformThread::Create(0, &delegate, &thread_handles[i]);
  }
  // Make sure all threads are actually already running.
  while (started_threads != kThreads) {
  }

  EXPECT_EXIT(
      {
        void* ptr = malloc(kAllocSize);
        FreeForTest(ptr);
        exit(1);
      },
      ::testing::ExitedWithCode(1), "");

  please_stop.store(true);
  for (int i = 0; i < kThreads; i++) {
    PlatformThread::Join(thread_handles[i]);
  }
}

#endif  // BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) &&
        // defined(GTEST_HAS_DEATH_TEST) && !defined(OS_ANDROID) &&
        // !BUILDFLAG(IS_CHROMECAST)

// Checks the bucket index logic.
TEST_F(PartitionAllocTest, GetIndex) {
  BucketIndexLookup lookup{};

  for (size_t size = 0; size < kMaxBucketed; size++) {
    size_t index = BucketIndexLookup::GetIndex(size);
    ASSERT_GE(lookup.bucket_sizes()[index], size);
  }

  // Make sure that power-of-two have exactly matching buckets.
  for (size_t size = (1 << (kMinBucketedOrder - 1)); size < kMaxBucketed;
       size <<= 1) {
    size_t index = BucketIndexLookup::GetIndex(size);
    ASSERT_EQ(lookup.bucket_sizes()[index], size);
  }
}

}  // namespace internal
}  // namespace base

#endif  // !defined(MEMORY_TOOL_REPLACES_ALLOCATOR)
