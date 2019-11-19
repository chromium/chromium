// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/partition_alloc.h"

#include <stdlib.h>
#include <string.h>

#include <limits>
#include <memory>
#include <vector>

#include "base/allocator/partition_allocator/address_space_randomization.h"
#include "base/allocator/partition_allocator/partition_alloc.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "base/stl_util.h"
#include "base/system/sys_info.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_POSIX)
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/time.h>
#endif  // defined(OS_POSIX)

#if !defined(MEMORY_TOOL_REPLACES_ALLOCATOR)

// Because there is so much deep inspection of the internal objects,
// explicitly annotating the namespaces for commonly expected objects makes the
// code unreadable. Prefer using directives instead.
using base::internal::PartitionBucket;
using base::internal::PartitionPage;

namespace {

constexpr size_t kTestMaxAllocation = base::kSystemPageSize;

bool IsLargeMemoryDevice() {
  // Treat any device with 2GiB or more of physical memory as a "large memory
  // device". We check for slightly less than 2GiB so that devices with a small
  // amount of memory not accessible to the OS still count as "large".
  return base::SysInfo::AmountOfPhysicalMemory() >= 2040LL * 1024 * 1024;
}

bool SetAddressSpaceLimit() {
#if !defined(ARCH_CPU_64_BITS) || !defined(OS_POSIX)
  // 32 bits => address space is limited already.
  return true;
#elif defined(OS_POSIX) && !defined(OS_MACOSX)
  // macOS will accept, but not enforce, |RLIMIT_AS| changes. See
  // https://crbug.com/435269 and rdar://17576114.
  //
  // Note: This number must be not less than 6 GB, because with
  // sanitizer_coverage_flags=edge, it reserves > 5 GB of address space. See
  // https://crbug.com/674665.
  const size_t kAddressSpaceLimit = static_cast<size_t>(6144) * 1024 * 1024;
  struct rlimit limit;
  if (getrlimit(RLIMIT_AS, &limit) != 0)
    return false;
  if (limit.rlim_cur == RLIM_INFINITY || limit.rlim_cur > kAddressSpaceLimit) {
    limit.rlim_cur = kAddressSpaceLimit;
    if (setrlimit(RLIMIT_AS, &limit) != 0)
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
  if (getrlimit(RLIMIT_AS, &limit) != 0)
    return false;
  limit.rlim_cur = limit.rlim_max;
  if (setrlimit(RLIMIT_AS, &limit) != 0)
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
    base::kSystemPageSize,
    base::kSystemPageSize + 1,
    base::internal::PartitionBucket::get_direct_map_size(100),
    1 << 20,
    1 << 21,
};
constexpr size_t kTestSizesCount = base::size(kTestSizes);

void AllocateRandomly(base::PartitionRootGeneric* root,
                      size_t count,
                      int flags) {
  std::vector<void*> allocations(count, nullptr);
  for (size_t i = 0; i < count; ++i) {
    const size_t size = kTestSizes[base::RandGenerator(kTestSizesCount)];
    allocations[i] = PartitionAllocGenericFlags(root, flags, size, nullptr);
    EXPECT_NE(nullptr, allocations[i]) << " size: " << size << " i: " << i;
  }

  for (size_t i = 0; i < count; ++i) {
    if (allocations[i]) {
      base::PartitionFree(allocations[i]);
    }
  }
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

const size_t kTestAllocSize = 16;
#if !DCHECK_IS_ON()
const size_t kPointerOffset = 0;
const size_t kExtraAllocSize = 0;
#else
const size_t kPointerOffset = kCookieSize;
const size_t kExtraAllocSize = kCookieSize * 2;
#endif
const size_t kRealAllocSize = kTestAllocSize + kExtraAllocSize;
const size_t kTestBucketIndex = kRealAllocSize >> kBucketShift;

const char* type_name = nullptr;

class PartitionAllocTest : public testing::Test {
 protected:
  PartitionAllocTest() = default;

  ~PartitionAllocTest() override = default;

  void SetUp() override {
    allocator.init();
    generic_allocator.init();
  }

  PartitionPage* GetFullPage(size_t size) {
    size_t real_size = size + kExtraAllocSize;
    size_t bucket_index = real_size >> kBucketShift;
    PartitionBucket* bucket = &allocator.root()->buckets()[bucket_index];
    size_t num_slots =
        (bucket->num_system_pages_per_slot_span * kSystemPageSize) / real_size;
    void* first = nullptr;
    void* last = nullptr;
    size_t i;
    for (i = 0; i < num_slots; ++i) {
      void* ptr = allocator.root()->Alloc(size, type_name);
      EXPECT_TRUE(ptr);
      if (!i)
        first = PartitionCookieFreePointerAdjust(ptr);
      else if (i == num_slots - 1)
        last = PartitionCookieFreePointerAdjust(ptr);
    }
    EXPECT_EQ(PartitionPage::FromPointer(first),
              PartitionPage::FromPointer(last));
    if (bucket->num_system_pages_per_slot_span ==
        kNumSystemPagesPerPartitionPage)
      EXPECT_EQ(reinterpret_cast<size_t>(first) & kPartitionPageBaseMask,
                reinterpret_cast<size_t>(last) & kPartitionPageBaseMask);
    EXPECT_EQ(num_slots, static_cast<size_t>(
                             bucket->active_pages_head->num_allocated_slots));
    EXPECT_EQ(nullptr, bucket->active_pages_head->freelist_head);
    EXPECT_TRUE(bucket->active_pages_head);
    EXPECT_TRUE(bucket->active_pages_head !=
                PartitionPage::get_sentinel_page());
    return bucket->active_pages_head;
  }

  void CycleFreeCache(size_t size) {
    size_t real_size = size + kExtraAllocSize;
    size_t bucket_index = real_size >> kBucketShift;
    PartitionBucket* bucket = &allocator.root()->buckets()[bucket_index];
    DCHECK(!bucket->active_pages_head->num_allocated_slots);

    for (size_t i = 0; i < kMaxFreeableSpans; ++i) {
      void* ptr = allocator.root()->Alloc(size, type_name);
      EXPECT_EQ(1, bucket->active_pages_head->num_allocated_slots);
      PartitionFree(ptr);
      EXPECT_EQ(0, bucket->active_pages_head->num_allocated_slots);
      EXPECT_NE(-1, bucket->active_pages_head->empty_cache_index);
    }
  }

  void CycleGenericFreeCache(size_t size) {
    for (size_t i = 0; i < kMaxFreeableSpans; ++i) {
      void* ptr = generic_allocator.root()->Alloc(size, type_name);
      PartitionPage* page =
          PartitionPage::FromPointer(PartitionCookieFreePointerAdjust(ptr));
      PartitionBucket* bucket = page->bucket;
      EXPECT_EQ(1, bucket->active_pages_head->num_allocated_slots);
      generic_allocator.root()->Free(ptr);
      EXPECT_EQ(0, bucket->active_pages_head->num_allocated_slots);
      EXPECT_NE(-1, bucket->active_pages_head->empty_cache_index);
    }
  }

  enum ReturnNullTestMode {
    kPartitionAllocGenericFlags,
    kPartitionReallocGenericFlags,
    kPartitionRootGenericTryRealloc,
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

    void** ptrs = reinterpret_cast<void**>(generic_allocator.root()->Alloc(
        num_allocations * sizeof(void*), type_name));
    int i;

    for (i = 0; i < num_allocations; ++i) {
      switch (mode) {
        case kPartitionAllocGenericFlags: {
          ptrs[i] = PartitionAllocGenericFlags(generic_allocator.root(),
                                               PartitionAllocReturnNull,
                                               alloc_size, type_name);
          break;
        }
        case kPartitionReallocGenericFlags: {
          ptrs[i] = PartitionAllocGenericFlags(
              generic_allocator.root(), PartitionAllocReturnNull, 1, type_name);
          ptrs[i] = PartitionReallocGenericFlags(
              generic_allocator.root(), PartitionAllocReturnNull, ptrs[i],
              alloc_size, type_name);
          break;
        }
        case kPartitionRootGenericTryRealloc: {
          ptrs[i] = PartitionAllocGenericFlags(
              generic_allocator.root(), PartitionAllocReturnNull, 1, type_name);
          ptrs[i] = generic_allocator.root()->TryRealloc(ptrs[i], alloc_size,
                                                         type_name);
        }
      }

      if (!i)
        EXPECT_TRUE(ptrs[0]);
      if (!ptrs[i]) {
        ptrs[i] = PartitionAllocGenericFlags(generic_allocator.root(),
                                             PartitionAllocReturnNull,
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
      generic_allocator.root()->Free(ptrs[i]);
      ptrs[i] = PartitionAllocGenericFlags(generic_allocator.root(),
                                           PartitionAllocReturnNull, alloc_size,
                                           type_name);
      EXPECT_TRUE(ptrs[i]);
      generic_allocator.root()->Free(ptrs[i]);
    }

    generic_allocator.root()->Free(ptrs);

    EXPECT_TRUE(ClearAddressSpaceLimit());
    LOG(FATAL) << "DoReturnNullTest";
  }

  SizeSpecificPartitionAllocator<kTestMaxAllocation> allocator;
  PartitionAllocatorGeneric generic_allocator;
};

class PartitionAllocDeathTest : public PartitionAllocTest {};

namespace {

void FreeFullPage(PartitionPage* page) {
  size_t size = page->bucket->slot_size;
  size_t num_slots =
      (page->bucket->num_system_pages_per_slot_span * kSystemPageSize) / size;
  EXPECT_EQ(num_slots, static_cast<size_t>(abs(page->num_allocated_slots)));
  char* ptr = reinterpret_cast<char*>(PartitionPage::ToPointer(page));
  size_t i;
  for (i = 0; i < num_slots; ++i) {
    PartitionFree(ptr + kPointerOffset);
    ptr += size;
  }
}

#if defined(OS_LINUX)
bool CheckPageInCore(void* ptr, bool in_core) {
  unsigned char ret = 0;
  EXPECT_EQ(0, mincore(ptr, kSystemPageSize, &ret));
  return in_core == (ret & 1);
}

#define CHECK_PAGE_IN_CORE(ptr, in_core) \
  EXPECT_TRUE(CheckPageInCore(ptr, in_core))
#else
#define CHECK_PAGE_IN_CORE(ptr, in_core) (void)(0)
#endif  // defined(OS_LINUX)

class MockPartitionStatsDumper : public PartitionStatsDumper {
 public:
  MockPartitionStatsDumper()
      : total_resident_bytes(0),
        total_active_bytes(0),
        total_decommittable_bytes(0),
        total_discardable_bytes(0) {}

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
    EXPECT_EQ(0u, stats->bucket_slot_size & kAllocationGranularityMask);
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
    for (size_t i = 0; i < bucket_stats.size(); ++i) {
      if (bucket_stats[i].bucket_slot_size == bucket_size)
        return &bucket_stats[i];
    }
    return nullptr;
  }

 private:
  size_t total_resident_bytes;
  size_t total_active_bytes;
  size_t total_decommittable_bytes;
  size_t total_discardable_bytes;

  std::vector<PartitionBucketMemoryStats> bucket_stats;
};

}  // namespace

// Check that the most basic of allocate / free pairs work.
TEST_F(PartitionAllocTest, Basic) {
  PartitionBucket* bucket = &allocator.root()->buckets()[kTestBucketIndex];
  PartitionPage* seed_page = PartitionPage::get_sentinel_page();

  EXPECT_FALSE(bucket->empty_pages_head);
  EXPECT_FALSE(bucket->decommitted_pages_head);
  EXPECT_EQ(seed_page, bucket->active_pages_head);
  EXPECT_EQ(nullptr, bucket->active_pages_head->next_page);

  void* ptr = allocator.root()->Alloc(kTestAllocSize, type_name);
  EXPECT_TRUE(ptr);
  EXPECT_EQ(kPointerOffset,
            reinterpret_cast<size_t>(ptr) & kPartitionPageOffsetMask);
  // Check that the offset appears to include a guard page.
  EXPECT_EQ(kPartitionPageSize + kPointerOffset,
            reinterpret_cast<size_t>(ptr) & kSuperPageOffsetMask);

  PartitionFree(ptr);
  // Expect that the last active page gets noticed as empty but doesn't get
  // decommitted.
  EXPECT_TRUE(bucket->empty_pages_head);
  EXPECT_FALSE(bucket->decommitted_pages_head);
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
  PartitionFree(ptr2);
  ptr2 = reinterpret_cast<char*>(
      allocator.root()->Alloc(kTestAllocSize, type_name));
  EXPECT_TRUE(ptr2);
  diff = ptr2 - ptr1;
  EXPECT_EQ(static_cast<ptrdiff_t>(kRealAllocSize), diff);
  PartitionFree(ptr1);
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

  PartitionFree(ptr1);
  PartitionFree(ptr2);
  PartitionFree(ptr3);
}

// Test a bucket with multiple pages.
TEST_F(PartitionAllocTest, MultiPages) {
  PartitionBucket* bucket = &allocator.root()->buckets()[kTestBucketIndex];

  PartitionPage* page = GetFullPage(kTestAllocSize);
  FreeFullPage(page);
  EXPECT_TRUE(bucket->empty_pages_head);
  EXPECT_EQ(PartitionPage::get_sentinel_page(), bucket->active_pages_head);
  EXPECT_EQ(nullptr, page->next_page);
  EXPECT_EQ(0, page->num_allocated_slots);

  page = GetFullPage(kTestAllocSize);
  PartitionPage* page2 = GetFullPage(kTestAllocSize);

  EXPECT_EQ(page2, bucket->active_pages_head);
  EXPECT_EQ(nullptr, page2->next_page);
  EXPECT_EQ(reinterpret_cast<uintptr_t>(PartitionPage::ToPointer(page)) &
                kSuperPageBaseMask,
            reinterpret_cast<uintptr_t>(PartitionPage::ToPointer(page2)) &
                kSuperPageBaseMask);

  // Fully free the non-current page. This will leave us with no current
  // active page because one is empty and the other is full.
  FreeFullPage(page);
  EXPECT_EQ(0, page->num_allocated_slots);
  EXPECT_TRUE(bucket->empty_pages_head);
  EXPECT_EQ(PartitionPage::get_sentinel_page(), bucket->active_pages_head);

  // Allocate a new page, it should pull from the freelist.
  page = GetFullPage(kTestAllocSize);
  EXPECT_FALSE(bucket->empty_pages_head);
  EXPECT_EQ(page, bucket->active_pages_head);

  FreeFullPage(page);
  FreeFullPage(page2);
  EXPECT_EQ(0, page->num_allocated_slots);
  EXPECT_EQ(0, page2->num_allocated_slots);
  EXPECT_EQ(0, page2->num_unprovisioned_slots);
  EXPECT_NE(-1, page2->empty_cache_index);
}

// Test some finer aspects of internal page transitions.
TEST_F(PartitionAllocTest, PageTransitions) {
  PartitionBucket* bucket = &allocator.root()->buckets()[kTestBucketIndex];

  PartitionPage* page1 = GetFullPage(kTestAllocSize);
  EXPECT_EQ(page1, bucket->active_pages_head);
  EXPECT_EQ(nullptr, page1->next_page);
  PartitionPage* page2 = GetFullPage(kTestAllocSize);
  EXPECT_EQ(page2, bucket->active_pages_head);
  EXPECT_EQ(nullptr, page2->next_page);

  // Bounce page1 back into the non-full list then fill it up again.
  char* ptr =
      reinterpret_cast<char*>(PartitionPage::ToPointer(page1)) + kPointerOffset;
  PartitionFree(ptr);
  EXPECT_EQ(page1, bucket->active_pages_head);
  (void)allocator.root()->Alloc(kTestAllocSize, type_name);
  EXPECT_EQ(page1, bucket->active_pages_head);
  EXPECT_EQ(page2, bucket->active_pages_head->next_page);

  // Allocating another page at this point should cause us to scan over page1
  // (which is both full and NOT our current page), and evict it from the
  // freelist. Older code had a O(n^2) condition due to failure to do this.
  PartitionPage* page3 = GetFullPage(kTestAllocSize);
  EXPECT_EQ(page3, bucket->active_pages_head);
  EXPECT_EQ(nullptr, page3->next_page);

  // Work out a pointer into page2 and free it.
  ptr =
      reinterpret_cast<char*>(PartitionPage::ToPointer(page2)) + kPointerOffset;
  PartitionFree(ptr);
  // Trying to allocate at this time should cause us to cycle around to page2
  // and find the recently freed slot.
  char* new_ptr = reinterpret_cast<char*>(
      allocator.root()->Alloc(kTestAllocSize, type_name));
  EXPECT_EQ(ptr, new_ptr);
  EXPECT_EQ(page2, bucket->active_pages_head);
  EXPECT_EQ(page3, page2->next_page);

  // Work out a pointer into page1 and free it. This should pull the page
  // back into the list of available pages.
  ptr =
      reinterpret_cast<char*>(PartitionPage::ToPointer(page1)) + kPointerOffset;
  PartitionFree(ptr);
  // This allocation should be satisfied by page1.
  new_ptr = reinterpret_cast<char*>(
      allocator.root()->Alloc(kTestAllocSize, type_name));
  EXPECT_EQ(ptr, new_ptr);
  EXPECT_EQ(page1, bucket->active_pages_head);
  EXPECT_EQ(page2, page1->next_page);

  FreeFullPage(page3);
  FreeFullPage(page2);
  FreeFullPage(page1);

  // Allocating whilst in this state exposed a bug, so keep the test.
  ptr = reinterpret_cast<char*>(
      allocator.root()->Alloc(kTestAllocSize, type_name));
  PartitionFree(ptr);
}

// Test some corner cases relating to page transitions in the internal
// free page list metadata bucket.
TEST_F(PartitionAllocTest, FreePageListPageTransitions) {
  PartitionBucket* bucket = &allocator.root()->buckets()[kTestBucketIndex];

  size_t num_to_fill_free_list_page =
      kPartitionPageSize / (sizeof(PartitionPage) + kExtraAllocSize);
  // The +1 is because we need to account for the fact that the current page
  // never gets thrown on the freelist.
  ++num_to_fill_free_list_page;
  auto pages = std::make_unique<PartitionPage* []>(num_to_fill_free_list_page);

  size_t i;
  for (i = 0; i < num_to_fill_free_list_page; ++i) {
    pages[i] = GetFullPage(kTestAllocSize);
  }
  EXPECT_EQ(pages[num_to_fill_free_list_page - 1], bucket->active_pages_head);
  for (i = 0; i < num_to_fill_free_list_page; ++i)
    FreeFullPage(pages[i]);
  EXPECT_EQ(PartitionPage::get_sentinel_page(), bucket->active_pages_head);
  EXPECT_TRUE(bucket->empty_pages_head);

  // Allocate / free in a different bucket size so we get control of a
  // different free page list. We need two pages because one will be the last
  // active page and not get freed.
  PartitionPage* page1 = GetFullPage(kTestAllocSize * 2);
  PartitionPage* page2 = GetFullPage(kTestAllocSize * 2);
  FreeFullPage(page1);
  FreeFullPage(page2);

  for (i = 0; i < num_to_fill_free_list_page; ++i) {
    pages[i] = GetFullPage(kTestAllocSize);
  }
  EXPECT_EQ(pages[num_to_fill_free_list_page - 1], bucket->active_pages_head);

  for (i = 0; i < num_to_fill_free_list_page; ++i)
    FreeFullPage(pages[i]);
  EXPECT_EQ(PartitionPage::get_sentinel_page(), bucket->active_pages_head);
  EXPECT_TRUE(bucket->empty_pages_head);
}

// Test a large series of allocations that cross more than one underlying
// 64KB super page allocation.
TEST_F(PartitionAllocTest, MultiPageAllocs) {
  // This is guaranteed to cross a super page boundary because the first
  // partition page "slot" will be taken up by a guard page.
  size_t num_pages_needed = kNumPartitionPagesPerSuperPage;
  // The super page should begin and end in a guard so we one less page in
  // order to allocate a single page in the new super page.
  --num_pages_needed;

  EXPECT_GT(num_pages_needed, 1u);
  auto pages = std::make_unique<PartitionPage* []>(num_pages_needed);
  uintptr_t first_super_page_base = 0;
  size_t i;
  for (i = 0; i < num_pages_needed; ++i) {
    pages[i] = GetFullPage(kTestAllocSize);
    void* storage_ptr = PartitionPage::ToPointer(pages[i]);
    if (!i)
      first_super_page_base =
          reinterpret_cast<uintptr_t>(storage_ptr) & kSuperPageBaseMask;
    if (i == num_pages_needed - 1) {
      uintptr_t second_super_page_base =
          reinterpret_cast<uintptr_t>(storage_ptr) & kSuperPageBaseMask;
      uintptr_t second_super_page_offset =
          reinterpret_cast<uintptr_t>(storage_ptr) & kSuperPageOffsetMask;
      EXPECT_FALSE(second_super_page_base == first_super_page_base);
      // Check that we allocated a guard page for the second page.
      EXPECT_EQ(kPartitionPageSize, second_super_page_offset);
    }
  }
  for (i = 0; i < num_pages_needed; ++i)
    FreeFullPage(pages[i]);
}

// Test the generic allocation functions that can handle arbitrary sizes and
// reallocing etc.
TEST_F(PartitionAllocTest, GenericAlloc) {
  void* ptr = generic_allocator.root()->Alloc(1, type_name);
  EXPECT_TRUE(ptr);
  generic_allocator.root()->Free(ptr);
  ptr = generic_allocator.root()->Alloc(kGenericMaxBucketed + 1, type_name);
  EXPECT_TRUE(ptr);
  generic_allocator.root()->Free(ptr);

  ptr = generic_allocator.root()->Alloc(1, type_name);
  EXPECT_TRUE(ptr);
  void* orig_ptr = ptr;
  char* char_ptr = static_cast<char*>(ptr);
  *char_ptr = 'A';

  // Change the size of the realloc, remaining inside the same bucket.
  void* new_ptr = generic_allocator.root()->Realloc(ptr, 2, type_name);
  EXPECT_EQ(ptr, new_ptr);
  new_ptr = generic_allocator.root()->Realloc(ptr, 1, type_name);
  EXPECT_EQ(ptr, new_ptr);
  new_ptr =
      generic_allocator.root()->Realloc(ptr, kGenericSmallestBucket, type_name);
  EXPECT_EQ(ptr, new_ptr);

  // Change the size of the realloc, switching buckets.
  new_ptr = generic_allocator.root()->Realloc(ptr, kGenericSmallestBucket + 1,
                                              type_name);
  EXPECT_NE(new_ptr, ptr);
  // Check that the realloc copied correctly.
  char* new_char_ptr = static_cast<char*>(new_ptr);
  EXPECT_EQ(*new_char_ptr, 'A');
#if DCHECK_IS_ON()
  // Subtle: this checks for an old bug where we copied too much from the
  // source of the realloc. The condition can be detected by a trashing of
  // the uninitialized value in the space of the upsized allocation.
  EXPECT_EQ(kUninitializedByte, static_cast<unsigned char>(
                                    *(new_char_ptr + kGenericSmallestBucket)));
#endif
  *new_char_ptr = 'B';
  // The realloc moved. To check that the old allocation was freed, we can
  // do an alloc of the old allocation size and check that the old allocation
  // address is at the head of the freelist and reused.
  void* reused_ptr = generic_allocator.root()->Alloc(1, type_name);
  EXPECT_EQ(reused_ptr, orig_ptr);
  generic_allocator.root()->Free(reused_ptr);

  // Downsize the realloc.
  ptr = new_ptr;
  new_ptr = generic_allocator.root()->Realloc(ptr, 1, type_name);
  EXPECT_EQ(new_ptr, orig_ptr);
  new_char_ptr = static_cast<char*>(new_ptr);
  EXPECT_EQ(*new_char_ptr, 'B');
  *new_char_ptr = 'C';

  // Upsize the realloc to outside the partition.
  ptr = new_ptr;
  new_ptr = generic_allocator.root()->Realloc(ptr, kGenericMaxBucketed + 1,
                                              type_name);
  EXPECT_NE(new_ptr, ptr);
  new_char_ptr = static_cast<char*>(new_ptr);
  EXPECT_EQ(*new_char_ptr, 'C');
  *new_char_ptr = 'D';

  // Upsize and downsize the realloc, remaining outside the partition.
  ptr = new_ptr;
  new_ptr = generic_allocator.root()->Realloc(ptr, kGenericMaxBucketed * 10,
                                              type_name);
  new_char_ptr = static_cast<char*>(new_ptr);
  EXPECT_EQ(*new_char_ptr, 'D');
  *new_char_ptr = 'E';
  ptr = new_ptr;
  new_ptr = generic_allocator.root()->Realloc(ptr, kGenericMaxBucketed * 2,
                                              type_name);
  new_char_ptr = static_cast<char*>(new_ptr);
  EXPECT_EQ(*new_char_ptr, 'E');
  *new_char_ptr = 'F';

  // Downsize the realloc to inside the partition.
  ptr = new_ptr;
  new_ptr = generic_allocator.root()->Realloc(ptr, 1, type_name);
  EXPECT_NE(new_ptr, ptr);
  EXPECT_EQ(new_ptr, orig_ptr);
  new_char_ptr = static_cast<char*>(new_ptr);
  EXPECT_EQ(*new_char_ptr, 'F');

  generic_allocator.root()->Free(new_ptr);
}

// Test the generic allocation functions can handle some specific sizes of
// interest.
TEST_F(PartitionAllocTest, GenericAllocSizes) {
  void* ptr = generic_allocator.root()->Alloc(0, type_name);
  EXPECT_TRUE(ptr);
  generic_allocator.root()->Free(ptr);

  // kPartitionPageSize is interesting because it results in just one
  // allocation per page, which tripped up some corner cases.
  size_t size = kPartitionPageSize - kExtraAllocSize;
  ptr = generic_allocator.root()->Alloc(size, type_name);
  EXPECT_TRUE(ptr);
  void* ptr2 = generic_allocator.root()->Alloc(size, type_name);
  EXPECT_TRUE(ptr2);
  generic_allocator.root()->Free(ptr);
  // Should be freeable at this point.
  PartitionPage* page =
      PartitionPage::FromPointer(PartitionCookieFreePointerAdjust(ptr));
  EXPECT_NE(-1, page->empty_cache_index);
  generic_allocator.root()->Free(ptr2);

  size = (((kPartitionPageSize * kMaxPartitionPagesPerSlotSpan) -
           kSystemPageSize) /
          2) -
         kExtraAllocSize;
  ptr = generic_allocator.root()->Alloc(size, type_name);
  EXPECT_TRUE(ptr);
  memset(ptr, 'A', size);
  ptr2 = generic_allocator.root()->Alloc(size, type_name);
  EXPECT_TRUE(ptr2);
  void* ptr3 = generic_allocator.root()->Alloc(size, type_name);
  EXPECT_TRUE(ptr3);
  void* ptr4 = generic_allocator.root()->Alloc(size, type_name);
  EXPECT_TRUE(ptr4);

  page = PartitionPage::FromPointer(PartitionCookieFreePointerAdjust(ptr));
  PartitionPage* page2 =
      PartitionPage::FromPointer(PartitionCookieFreePointerAdjust(ptr3));
  EXPECT_NE(page, page2);

  generic_allocator.root()->Free(ptr);
  generic_allocator.root()->Free(ptr3);
  generic_allocator.root()->Free(ptr2);
  // Should be freeable at this point.
  EXPECT_NE(-1, page->empty_cache_index);
  EXPECT_EQ(0, page->num_allocated_slots);
  EXPECT_EQ(0, page->num_unprovisioned_slots);
  void* new_ptr = generic_allocator.root()->Alloc(size, type_name);
  EXPECT_EQ(ptr3, new_ptr);
  new_ptr = generic_allocator.root()->Alloc(size, type_name);
  EXPECT_EQ(ptr2, new_ptr);

  generic_allocator.root()->Free(new_ptr);
  generic_allocator.root()->Free(ptr3);
  generic_allocator.root()->Free(ptr4);

#if DCHECK_IS_ON()
  // |PartitionPage::Free| must poison the slot's contents with |kFreedByte|.
  EXPECT_EQ(kFreedByte,
            *(reinterpret_cast<unsigned char*>(new_ptr) + (size - 1)));
#endif

  // Can we allocate a massive (512MB) size?
  // Allocate 512MB, but +1, to test for cookie writing alignment issues.
  // Test this only if the device has enough memory or it might fail due
  // to OOM.
  if (IsLargeMemoryDevice()) {
    ptr = generic_allocator.root()->Alloc(512 * 1024 * 1024 + 1, type_name);
    generic_allocator.root()->Free(ptr);
  }

  // Check a more reasonable, but still direct mapped, size.
  // Chop a system page and a byte off to test for rounding errors.
  size = 20 * 1024 * 1024;
  size -= kSystemPageSize;
  size -= 1;
  ptr = generic_allocator.root()->Alloc(size, type_name);
  char* char_ptr = reinterpret_cast<char*>(ptr);
  *(char_ptr + (size - 1)) = 'A';
  generic_allocator.root()->Free(ptr);

  // Can we free null?
  generic_allocator.root()->Free(nullptr);

  // Do we correctly get a null for a failed allocation?
  EXPECT_EQ(nullptr, PartitionAllocGenericFlags(
                         generic_allocator.root(), PartitionAllocReturnNull,
                         3u * 1024 * 1024 * 1024, type_name));
}

// Test that we can fetch the real allocated size after an allocation.
TEST_F(PartitionAllocTest, GenericAllocGetSize) {
  void* ptr;
  size_t requested_size, actual_size, predicted_size;

  EXPECT_TRUE(PartitionAllocSupportsGetSize());

  // Allocate something small.
  requested_size = 511 - kExtraAllocSize;
  predicted_size = generic_allocator.root()->ActualSize(requested_size);
  ptr = generic_allocator.root()->Alloc(requested_size, type_name);
  EXPECT_TRUE(ptr);
  actual_size = PartitionAllocGetSize(ptr);
  EXPECT_EQ(predicted_size, actual_size);
  EXPECT_LT(requested_size, actual_size);
  generic_allocator.root()->Free(ptr);

  // Allocate a size that should be a perfect match for a bucket, because it
  // is an exact power of 2.
  requested_size = (256 * 1024) - kExtraAllocSize;
  predicted_size = generic_allocator.root()->ActualSize(requested_size);
  ptr = generic_allocator.root()->Alloc(requested_size, type_name);
  EXPECT_TRUE(ptr);
  actual_size = PartitionAllocGetSize(ptr);
  EXPECT_EQ(predicted_size, actual_size);
  EXPECT_EQ(requested_size, actual_size);
  generic_allocator.root()->Free(ptr);

  // Allocate a size that is a system page smaller than a bucket. GetSize()
  // should return a larger size than we asked for now.
  size_t num = 64;
  while (num * kSystemPageSize >= 1024 * 1024) {
    num /= 2;
  }
  requested_size = num * kSystemPageSize - kSystemPageSize - kExtraAllocSize;
  predicted_size = generic_allocator.root()->ActualSize(requested_size);
  ptr = generic_allocator.root()->Alloc(requested_size, type_name);
  EXPECT_TRUE(ptr);
  actual_size = PartitionAllocGetSize(ptr);
  EXPECT_EQ(predicted_size, actual_size);
  EXPECT_EQ(requested_size + kSystemPageSize, actual_size);
  // Check that we can write at the end of the reported size too.
  char* char_ptr = reinterpret_cast<char*>(ptr);
  *(char_ptr + (actual_size - 1)) = 'A';
  generic_allocator.root()->Free(ptr);

  // Allocate something very large, and uneven.
  if (IsLargeMemoryDevice()) {
    requested_size = 512 * 1024 * 1024 - 1;
    predicted_size = generic_allocator.root()->ActualSize(requested_size);
    ptr = generic_allocator.root()->Alloc(requested_size, type_name);
    EXPECT_TRUE(ptr);
    actual_size = PartitionAllocGetSize(ptr);
    EXPECT_EQ(predicted_size, actual_size);
    EXPECT_LT(requested_size, actual_size);
    generic_allocator.root()->Free(ptr);
  }

  // Too large allocation.
  requested_size = kGenericMaxDirectMapped + 1;
  predicted_size = generic_allocator.root()->ActualSize(requested_size);
  EXPECT_EQ(requested_size, predicted_size);
}

// Test the realloc() contract.
TEST_F(PartitionAllocTest, Realloc) {
  // realloc(0, size) should be equivalent to malloc().
  void* ptr =
      generic_allocator.root()->Realloc(nullptr, kTestAllocSize, type_name);
  memset(ptr, 'A', kTestAllocSize);
  PartitionPage* page =
      PartitionPage::FromPointer(PartitionCookieFreePointerAdjust(ptr));
  // realloc(ptr, 0) should be equivalent to free().
  void* ptr2 = generic_allocator.root()->Realloc(ptr, 0, type_name);
  EXPECT_EQ(nullptr, ptr2);
  EXPECT_EQ(PartitionCookieFreePointerAdjust(ptr), page->freelist_head);

  // Test that growing an allocation with realloc() copies everything from the
  // old allocation.
  size_t size = kSystemPageSize - kExtraAllocSize;
  EXPECT_EQ(size, generic_allocator.root()->ActualSize(size));
  ptr = generic_allocator.root()->Alloc(size, type_name);
  memset(ptr, 'A', size);
  ptr2 = generic_allocator.root()->Realloc(ptr, size + 1, type_name);
  EXPECT_NE(ptr, ptr2);
  char* char_ptr2 = static_cast<char*>(ptr2);
  EXPECT_EQ('A', char_ptr2[0]);
  EXPECT_EQ('A', char_ptr2[size - 1]);
#if DCHECK_IS_ON()
  EXPECT_EQ(kUninitializedByte, static_cast<unsigned char>(char_ptr2[size]));
#endif

  // Test that shrinking an allocation with realloc() also copies everything
  // from the old allocation.
  ptr = generic_allocator.root()->Realloc(ptr2, size - 1, type_name);
  EXPECT_NE(ptr2, ptr);
  char* char_ptr = static_cast<char*>(ptr);
  EXPECT_EQ('A', char_ptr[0]);
  EXPECT_EQ('A', char_ptr[size - 2]);
#if DCHECK_IS_ON()
  EXPECT_EQ(kUninitializedByte, static_cast<unsigned char>(char_ptr[size - 1]));
#endif

  generic_allocator.root()->Free(ptr);

  // Test that shrinking a direct mapped allocation happens in-place.
  size = kGenericMaxBucketed + 16 * kSystemPageSize;
  ptr = generic_allocator.root()->Alloc(size, type_name);
  size_t actual_size = PartitionAllocGetSize(ptr);
  ptr2 = generic_allocator.root()->Realloc(
      ptr, kGenericMaxBucketed + 8 * kSystemPageSize, type_name);
  EXPECT_EQ(ptr, ptr2);
  EXPECT_EQ(actual_size - 8 * kSystemPageSize, PartitionAllocGetSize(ptr2));

  // Test that a previously in-place shrunk direct mapped allocation can be
  // expanded up again within its original size.
  ptr = generic_allocator.root()->Realloc(ptr2, size - kSystemPageSize,
                                          type_name);
  EXPECT_EQ(ptr2, ptr);
  EXPECT_EQ(actual_size - kSystemPageSize, PartitionAllocGetSize(ptr));

  // Test that a direct mapped allocation is performed not in-place when the
  // new size is small enough.
  ptr2 = generic_allocator.root()->Realloc(ptr, kSystemPageSize, type_name);
  EXPECT_NE(ptr, ptr2);

  generic_allocator.root()->Free(ptr2);
}

// Tests the handing out of freelists for partial pages.
TEST_F(PartitionAllocTest, PartialPageFreelists) {
  size_t big_size = allocator.root()->max_allocation - kExtraAllocSize;
  EXPECT_EQ(kSystemPageSize - kAllocationGranularity,
            big_size + kExtraAllocSize);
  size_t bucket_index = (big_size + kExtraAllocSize) >> kBucketShift;
  PartitionBucket* bucket = &allocator.root()->buckets()[bucket_index];
  EXPECT_EQ(nullptr, bucket->empty_pages_head);

  void* ptr = allocator.root()->Alloc(big_size, type_name);
  EXPECT_TRUE(ptr);

  PartitionPage* page =
      PartitionPage::FromPointer(PartitionCookieFreePointerAdjust(ptr));
  size_t total_slots =
      (page->bucket->num_system_pages_per_slot_span * kSystemPageSize) /
      (big_size + kExtraAllocSize);
  EXPECT_EQ(4u, total_slots);
  // The freelist should have one entry, because we were able to exactly fit
  // one object slot and one freelist pointer (the null that the head points
  // to) into a system page.
  EXPECT_TRUE(page->freelist_head);
  EXPECT_EQ(1, page->num_allocated_slots);
  EXPECT_EQ(2, page->num_unprovisioned_slots);

  void* ptr2 = allocator.root()->Alloc(big_size, type_name);
  EXPECT_TRUE(ptr2);
  EXPECT_FALSE(page->freelist_head);
  EXPECT_EQ(2, page->num_allocated_slots);
  EXPECT_EQ(2, page->num_unprovisioned_slots);

  void* ptr3 = allocator.root()->Alloc(big_size, type_name);
  EXPECT_TRUE(ptr3);
  EXPECT_TRUE(page->freelist_head);
  EXPECT_EQ(3, page->num_allocated_slots);
  EXPECT_EQ(0, page->num_unprovisioned_slots);

  void* ptr4 = allocator.root()->Alloc(big_size, type_name);
  EXPECT_TRUE(ptr4);
  EXPECT_FALSE(page->freelist_head);
  EXPECT_EQ(4, page->num_allocated_slots);
  EXPECT_EQ(0, page->num_unprovisioned_slots);

  void* ptr5 = allocator.root()->Alloc(big_size, type_name);
  EXPECT_TRUE(ptr5);

  PartitionPage* page2 =
      PartitionPage::FromPointer(PartitionCookieFreePointerAdjust(ptr5));
  EXPECT_EQ(1, page2->num_allocated_slots);

  // Churn things a little whilst there's a partial page freelist.
  PartitionFree(ptr);
  ptr = allocator.root()->Alloc(big_size, type_name);
  void* ptr6 = allocator.root()->Alloc(big_size, type_name);

  PartitionFree(ptr);
  PartitionFree(ptr2);
  PartitionFree(ptr3);
  PartitionFree(ptr4);
  PartitionFree(ptr5);
  PartitionFree(ptr6);
  EXPECT_NE(-1, page->empty_cache_index);
  EXPECT_NE(-1, page2->empty_cache_index);
  EXPECT_TRUE(page2->freelist_head);
  EXPECT_EQ(0, page2->num_allocated_slots);

  // And test a couple of sizes that do not cross kSystemPageSize with a single
  // allocation.
  size_t mediumSize = (kSystemPageSize / 2) - kExtraAllocSize;
  bucket_index = (mediumSize + kExtraAllocSize) >> kBucketShift;
  bucket = &allocator.root()->buckets()[bucket_index];
  EXPECT_EQ(nullptr, bucket->empty_pages_head);

  ptr = allocator.root()->Alloc(mediumSize, type_name);
  EXPECT_TRUE(ptr);
  page = PartitionPage::FromPointer(PartitionCookieFreePointerAdjust(ptr));
  EXPECT_EQ(1, page->num_allocated_slots);
  total_slots =
      (page->bucket->num_system_pages_per_slot_span * kSystemPageSize) /
      (mediumSize + kExtraAllocSize);
  size_t first_page_slots = kSystemPageSize / (mediumSize + kExtraAllocSize);
  EXPECT_EQ(2u, first_page_slots);
  EXPECT_EQ(total_slots - first_page_slots, page->num_unprovisioned_slots);

  PartitionFree(ptr);

  size_t smallSize = (kSystemPageSize / 4) - kExtraAllocSize;
  bucket_index = (smallSize + kExtraAllocSize) >> kBucketShift;
  bucket = &allocator.root()->buckets()[bucket_index];
  EXPECT_EQ(nullptr, bucket->empty_pages_head);

  ptr = allocator.root()->Alloc(smallSize, type_name);
  EXPECT_TRUE(ptr);
  page = PartitionPage::FromPointer(PartitionCookieFreePointerAdjust(ptr));
  EXPECT_EQ(1, page->num_allocated_slots);
  total_slots =
      (page->bucket->num_system_pages_per_slot_span * kSystemPageSize) /
      (smallSize + kExtraAllocSize);
  first_page_slots = kSystemPageSize / (smallSize + kExtraAllocSize);
  EXPECT_EQ(total_slots - first_page_slots, page->num_unprovisioned_slots);

  PartitionFree(ptr);
  EXPECT_TRUE(page->freelist_head);
  EXPECT_EQ(0, page->num_allocated_slots);

  size_t verySmallSize = 32 - kExtraAllocSize;
  bucket_index = (verySmallSize + kExtraAllocSize) >> kBucketShift;
  bucket = &allocator.root()->buckets()[bucket_index];
  EXPECT_EQ(nullptr, bucket->empty_pages_head);

  ptr = allocator.root()->Alloc(verySmallSize, type_name);
  EXPECT_TRUE(ptr);
  page = PartitionPage::FromPointer(PartitionCookieFreePointerAdjust(ptr));
  EXPECT_EQ(1, page->num_allocated_slots);
  total_slots =
      (page->bucket->num_system_pages_per_slot_span * kSystemPageSize) /
      (verySmallSize + kExtraAllocSize);
  first_page_slots = kSystemPageSize / (verySmallSize + kExtraAllocSize);
  EXPECT_EQ(total_slots - first_page_slots, page->num_unprovisioned_slots);

  PartitionFree(ptr);
  EXPECT_TRUE(page->freelist_head);
  EXPECT_EQ(0, page->num_allocated_slots);

  // And try an allocation size (against the generic allocator) that is
  // larger than a system page.
  size_t page_and_a_half_size =
      (kSystemPageSize + (kSystemPageSize / 2)) - kExtraAllocSize;
  ptr = generic_allocator.root()->Alloc(page_and_a_half_size, type_name);
  EXPECT_TRUE(ptr);
  page = PartitionPage::FromPointer(PartitionCookieFreePointerAdjust(ptr));
  EXPECT_EQ(1, page->num_allocated_slots);
  EXPECT_TRUE(page->freelist_head);
  total_slots =
      (page->bucket->num_system_pages_per_slot_span * kSystemPageSize) /
      (page_and_a_half_size + kExtraAllocSize);
  EXPECT_EQ(total_slots - 2, page->num_unprovisioned_slots);
  generic_allocator.root()->Free(ptr);

  // And then make sure than exactly the page size only faults one page.
  size_t pageSize = kSystemPageSize - kExtraAllocSize;
  ptr = generic_allocator.root()->Alloc(pageSize, type_name);
  EXPECT_TRUE(ptr);
  page = PartitionPage::FromPointer(PartitionCookieFreePointerAdjust(ptr));
  EXPECT_EQ(1, page->num_allocated_slots);
  EXPECT_FALSE(page->freelist_head);
  total_slots =
      (page->bucket->num_system_pages_per_slot_span * kSystemPageSize) /
      (pageSize + kExtraAllocSize);
  EXPECT_EQ(total_slots - 1, page->num_unprovisioned_slots);
  generic_allocator.root()->Free(ptr);
}

// Test some of the fragmentation-resistant properties of the allocator.
TEST_F(PartitionAllocTest, PageRefilling) {
  PartitionBucket* bucket = &allocator.root()->buckets()[kTestBucketIndex];

  // Grab two full pages and a non-full page.
  PartitionPage* page1 = GetFullPage(kTestAllocSize);
  PartitionPage* page2 = GetFullPage(kTestAllocSize);
  void* ptr = allocator.root()->Alloc(kTestAllocSize, type_name);
  EXPECT_TRUE(ptr);
  EXPECT_NE(page1, bucket->active_pages_head);
  EXPECT_NE(page2, bucket->active_pages_head);
  PartitionPage* page =
      PartitionPage::FromPointer(PartitionCookieFreePointerAdjust(ptr));
  EXPECT_EQ(1, page->num_allocated_slots);

  // Work out a pointer into page2 and free it; and then page1 and free it.
  char* ptr2 =
      reinterpret_cast<char*>(PartitionPage::ToPointer(page1)) + kPointerOffset;
  PartitionFree(ptr2);
  ptr2 =
      reinterpret_cast<char*>(PartitionPage::ToPointer(page2)) + kPointerOffset;
  PartitionFree(ptr2);

  // If we perform two allocations from the same bucket now, we expect to
  // refill both the nearly full pages.
  (void)allocator.root()->Alloc(kTestAllocSize, type_name);
  (void)allocator.root()->Alloc(kTestAllocSize, type_name);
  EXPECT_EQ(1, page->num_allocated_slots);

  FreeFullPage(page2);
  FreeFullPage(page1);
  PartitionFree(ptr);
}

// Basic tests to ensure that allocations work for partial page buckets.
TEST_F(PartitionAllocTest, PartialPages) {
  // Find a size that is backed by a partial partition page.
  size_t size = sizeof(void*);
  PartitionBucket* bucket = nullptr;
  while (size < kTestMaxAllocation) {
    bucket = &allocator.root()->buckets()[size >> kBucketShift];
    if (bucket->num_system_pages_per_slot_span %
        kNumSystemPagesPerPartitionPage)
      break;
    size += sizeof(void*);
  }
  EXPECT_LT(size, kTestMaxAllocation);

  PartitionPage* page1 = GetFullPage(size);
  PartitionPage* page2 = GetFullPage(size);
  FreeFullPage(page2);
  FreeFullPage(page1);
}

// Test correct handling if our mapping collides with another.
TEST_F(PartitionAllocTest, MappingCollision) {
  // The -2 is because the first and last partition pages in a super page are
  // guard pages.
  size_t num_partition_pages_needed = kNumPartitionPagesPerSuperPage - 2;
  auto first_super_page_pages =
      std::make_unique<PartitionPage* []>(num_partition_pages_needed);
  auto second_super_page_pages =
      std::make_unique<PartitionPage* []>(num_partition_pages_needed);

  size_t i;
  for (i = 0; i < num_partition_pages_needed; ++i)
    first_super_page_pages[i] = GetFullPage(kTestAllocSize);

  char* page_base = reinterpret_cast<char*>(
      PartitionPage::ToPointer(first_super_page_pages[0]));
  EXPECT_EQ(kPartitionPageSize,
            reinterpret_cast<uintptr_t>(page_base) & kSuperPageOffsetMask);
  page_base -= kPartitionPageSize;
  // Map a single system page either side of the mapping for our allocations,
  // with the goal of tripping up alignment of the next mapping.
  void* map1 = AllocPages(
      page_base - kPageAllocationGranularity, kPageAllocationGranularity,
      kPageAllocationGranularity, PageInaccessible, PageTag::kPartitionAlloc);
  EXPECT_TRUE(map1);
  void* map2 = AllocPages(
      page_base + kSuperPageSize, kPageAllocationGranularity,
      kPageAllocationGranularity, PageInaccessible, PageTag::kPartitionAlloc);
  EXPECT_TRUE(map2);

  for (i = 0; i < num_partition_pages_needed; ++i)
    second_super_page_pages[i] = GetFullPage(kTestAllocSize);

  FreePages(map1, kPageAllocationGranularity);
  FreePages(map2, kPageAllocationGranularity);

  page_base = reinterpret_cast<char*>(
      PartitionPage::ToPointer(second_super_page_pages[0]));
  EXPECT_EQ(kPartitionPageSize,
            reinterpret_cast<uintptr_t>(page_base) & kSuperPageOffsetMask);
  page_base -= kPartitionPageSize;
  // Map a single system page either side of the mapping for our allocations,
  // with the goal of tripping up alignment of the next mapping.
  map1 = AllocPages(page_base - kPageAllocationGranularity,
                    kPageAllocationGranularity, kPageAllocationGranularity,
                    PageReadWrite, PageTag::kPartitionAlloc);
  EXPECT_TRUE(map1);
  map2 = AllocPages(page_base + kSuperPageSize, kPageAllocationGranularity,
                    kPageAllocationGranularity, PageReadWrite,
                    PageTag::kPartitionAlloc);
  EXPECT_TRUE(map2);
  EXPECT_TRUE(TrySetSystemPagesAccess(map1, kPageAllocationGranularity,
                                      PageInaccessible));
  EXPECT_TRUE(TrySetSystemPagesAccess(map2, kPageAllocationGranularity,
                                      PageInaccessible));

  PartitionPage* page_in_third_super_page = GetFullPage(kTestAllocSize);
  FreePages(map1, kPageAllocationGranularity);
  FreePages(map2, kPageAllocationGranularity);

  EXPECT_EQ(0u, reinterpret_cast<uintptr_t>(
                    PartitionPage::ToPointer(page_in_third_super_page)) &
                    kPartitionPageOffsetMask);

  // And make sure we really did get a page in a new superpage.
  EXPECT_NE(reinterpret_cast<uintptr_t>(
                PartitionPage::ToPointer(first_super_page_pages[0])) &
                kSuperPageBaseMask,
            reinterpret_cast<uintptr_t>(
                PartitionPage::ToPointer(page_in_third_super_page)) &
                kSuperPageBaseMask);
  EXPECT_NE(reinterpret_cast<uintptr_t>(
                PartitionPage::ToPointer(second_super_page_pages[0])) &
                kSuperPageBaseMask,
            reinterpret_cast<uintptr_t>(
                PartitionPage::ToPointer(page_in_third_super_page)) &
                kSuperPageBaseMask);

  FreeFullPage(page_in_third_super_page);
  for (i = 0; i < num_partition_pages_needed; ++i) {
    FreeFullPage(first_super_page_pages[i]);
    FreeFullPage(second_super_page_pages[i]);
  }
}

// Tests that pages in the free page cache do get freed as appropriate.
TEST_F(PartitionAllocTest, FreeCache) {
  EXPECT_EQ(0U, allocator.root()->total_size_of_committed_pages);

  size_t big_size = allocator.root()->max_allocation - kExtraAllocSize;
  size_t bucket_index = (big_size + kExtraAllocSize) >> kBucketShift;
  PartitionBucket* bucket = &allocator.root()->buckets()[bucket_index];

  void* ptr = allocator.root()->Alloc(big_size, type_name);
  EXPECT_TRUE(ptr);
  PartitionPage* page =
      PartitionPage::FromPointer(PartitionCookieFreePointerAdjust(ptr));
  EXPECT_EQ(nullptr, bucket->empty_pages_head);
  EXPECT_EQ(1, page->num_allocated_slots);
  EXPECT_EQ(kPartitionPageSize,
            allocator.root()->total_size_of_committed_pages);
  PartitionFree(ptr);
  EXPECT_EQ(0, page->num_allocated_slots);
  EXPECT_NE(-1, page->empty_cache_index);
  EXPECT_TRUE(page->freelist_head);

  CycleFreeCache(kTestAllocSize);

  // Flushing the cache should have really freed the unused page.
  EXPECT_FALSE(page->freelist_head);
  EXPECT_EQ(-1, page->empty_cache_index);
  EXPECT_EQ(0, page->num_allocated_slots);
  PartitionBucket* cycle_free_cache_bucket =
      &allocator.root()->buckets()[kTestBucketIndex];
  EXPECT_EQ(
      cycle_free_cache_bucket->num_system_pages_per_slot_span * kSystemPageSize,
      allocator.root()->total_size_of_committed_pages);

  // Check that an allocation works ok whilst in this state (a free'd page
  // as the active pages head).
  ptr = allocator.root()->Alloc(big_size, type_name);
  EXPECT_FALSE(bucket->empty_pages_head);
  PartitionFree(ptr);

  // Also check that a page that is bouncing immediately between empty and
  // used does not get freed.
  for (size_t i = 0; i < kMaxFreeableSpans * 2; ++i) {
    ptr = allocator.root()->Alloc(big_size, type_name);
    EXPECT_TRUE(page->freelist_head);
    PartitionFree(ptr);
    EXPECT_TRUE(page->freelist_head);
  }
  EXPECT_EQ(kPartitionPageSize,
            allocator.root()->total_size_of_committed_pages);
}

// Tests for a bug we had with losing references to free pages.
TEST_F(PartitionAllocTest, LostFreePagesBug) {
  size_t size = kPartitionPageSize - kExtraAllocSize;

  void* ptr = generic_allocator.root()->Alloc(size, type_name);
  EXPECT_TRUE(ptr);
  void* ptr2 = generic_allocator.root()->Alloc(size, type_name);
  EXPECT_TRUE(ptr2);

  PartitionPage* page =
      PartitionPage::FromPointer(PartitionCookieFreePointerAdjust(ptr));
  PartitionPage* page2 =
      PartitionPage::FromPointer(PartitionCookieFreePointerAdjust(ptr2));
  PartitionBucket* bucket = page->bucket;

  EXPECT_EQ(nullptr, bucket->empty_pages_head);
  EXPECT_EQ(-1, page->num_allocated_slots);
  EXPECT_EQ(1, page2->num_allocated_slots);

  generic_allocator.root()->Free(ptr);
  generic_allocator.root()->Free(ptr2);

  EXPECT_TRUE(bucket->empty_pages_head);
  EXPECT_TRUE(bucket->empty_pages_head->next_page);
  EXPECT_EQ(0, page->num_allocated_slots);
  EXPECT_EQ(0, page2->num_allocated_slots);
  EXPECT_TRUE(page->freelist_head);
  EXPECT_TRUE(page2->freelist_head);

  CycleGenericFreeCache(kTestAllocSize);

  EXPECT_FALSE(page->freelist_head);
  EXPECT_FALSE(page2->freelist_head);

  EXPECT_TRUE(bucket->empty_pages_head);
  EXPECT_TRUE(bucket->empty_pages_head->next_page);
  EXPECT_EQ(PartitionPage::get_sentinel_page(), bucket->active_pages_head);

  // At this moment, we have two decommitted pages, on the empty list.
  ptr = generic_allocator.root()->Alloc(size, type_name);
  EXPECT_TRUE(ptr);
  generic_allocator.root()->Free(ptr);

  EXPECT_EQ(PartitionPage::get_sentinel_page(), bucket->active_pages_head);
  EXPECT_TRUE(bucket->empty_pages_head);
  EXPECT_TRUE(bucket->decommitted_pages_head);

  CycleGenericFreeCache(kTestAllocSize);

  // We're now set up to trigger a historical bug by scanning over the active
  // pages list. The current code gets into a different state, but we'll keep
  // the test as being an interesting corner case.
  ptr = generic_allocator.root()->Alloc(size, type_name);
  EXPECT_TRUE(ptr);
  generic_allocator.root()->Free(ptr);

  EXPECT_TRUE(bucket->active_pages_head);
  EXPECT_TRUE(bucket->empty_pages_head);
  EXPECT_TRUE(bucket->decommitted_pages_head);
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
     (defined(OS_POSIX) && !(defined(OS_MACOSX) || defined(OS_ANDROID))))

// The following four tests wrap a called function in an expect death statement
// to perform their test, because they are non-hermetic. Specifically they are
// going to attempt to exhaust the allocatable memory, which leaves the
// allocator in a bad global state.
// Performing them as death tests causes them to be forked into their own
// process, so they won't pollute other tests.
TEST_F(PartitionAllocDeathTest, RepeatedAllocReturnNullDirect) {
  // A direct-mapped allocation size.
  EXPECT_DEATH(DoReturnNullTest(32 * 1024 * 1024, kPartitionAllocGenericFlags),
               "DoReturnNullTest");
}

// Repeating above test with Realloc
TEST_F(PartitionAllocDeathTest, RepeatedReallocReturnNullDirect) {
  EXPECT_DEATH(
      DoReturnNullTest(32 * 1024 * 1024, kPartitionReallocGenericFlags),
      "DoReturnNullTest");
}

// Repeating above test with TryRealloc
TEST_F(PartitionAllocDeathTest, RepeatedTryReallocReturnNullDirect) {
  EXPECT_DEATH(
      DoReturnNullTest(32 * 1024 * 1024, kPartitionRootGenericTryRealloc),
      "DoReturnNullTest");
}

// Test "return null" with a 512 kB block size.
TEST_F(PartitionAllocDeathTest, RepeatedAllocReturnNull) {
  // A single-slot but non-direct-mapped allocation size.
  EXPECT_DEATH(DoReturnNullTest(512 * 1024, kPartitionAllocGenericFlags),
               "DoReturnNullTest");
}

// Repeating above test with Realloc.
TEST_F(PartitionAllocDeathTest, RepeatedReallocReturnNull) {
  EXPECT_DEATH(DoReturnNullTest(512 * 1024, kPartitionReallocGenericFlags),
               "DoReturnNullTest");
}

// Repeating above test with TryRealloc.
TEST_F(PartitionAllocDeathTest, RepeatedTryReallocReturnNull) {
  EXPECT_DEATH(DoReturnNullTest(512 * 1024, kPartitionRootGenericTryRealloc),
               "DoReturnNullTest");
}

#endif  // !defined(ARCH_CPU_64_BITS) || (defined(OS_POSIX) &&
        // !(defined(OS_MACOSX) || defined(OS_ANDROID)))

// Make sure that malloc(-1) dies.
// In the past, we had an integer overflow that would alias malloc(-1) to
// malloc(0), which is not good.
TEST_F(PartitionAllocDeathTest, LargeAllocs) {
  // Largest alloc.
  EXPECT_DEATH(
      generic_allocator.root()->Alloc(static_cast<size_t>(-1), type_name), "");
  // And the smallest allocation we expect to die.
  EXPECT_DEATH(
      generic_allocator.root()->Alloc(kGenericMaxDirectMapped + 1, type_name),
      "");
}

// Check that our immediate double-free detection works.
TEST_F(PartitionAllocDeathTest, ImmediateDoubleFree) {
  void* ptr = generic_allocator.root()->Alloc(kTestAllocSize, type_name);
  EXPECT_TRUE(ptr);
  generic_allocator.root()->Free(ptr);

  EXPECT_DEATH(generic_allocator.root()->Free(ptr), "");
}

// Check that our refcount-based double-free detection works.
TEST_F(PartitionAllocDeathTest, RefcountDoubleFree) {
  void* ptr = generic_allocator.root()->Alloc(kTestAllocSize, type_name);
  EXPECT_TRUE(ptr);
  void* ptr2 = generic_allocator.root()->Alloc(kTestAllocSize, type_name);
  EXPECT_TRUE(ptr2);
  generic_allocator.root()->Free(ptr);
  generic_allocator.root()->Free(ptr2);
  // This is not an immediate double-free so our immediate detection won't
  // fire. However, it does take the "refcount" of the partition page to -1,
  // which is illegal and should be trapped.
  EXPECT_DEATH(generic_allocator.root()->Free(ptr), "");
}

// Check that guard pages are present where expected.
TEST_F(PartitionAllocDeathTest, GuardPages) {
// PartitionAlloc adds kPartitionPageSize to the requested size
// (for metadata), and then rounds that size to kPageAllocationGranularity.
// To be able to reliably write one past a direct allocation, choose a size
// that's
// a) larger than kGenericMaxBucketed (to make the allocation direct)
// b) aligned at kPageAllocationGranularity boundaries after
//    kPartitionPageSize has been added to it.
// (On 32-bit, PartitionAlloc adds another kSystemPageSize to the
// allocation size before rounding, but there it marks the memory right
// after size as inaccessible, so it's fine to write 1 past the size we
// hand to PartitionAlloc and we don't need to worry about allocation
// granularities.)
#define ALIGN(N, A) (((N) + (A)-1) / (A) * (A))
  const int kSize = ALIGN(kGenericMaxBucketed + 1 + kPartitionPageSize,
                          kPageAllocationGranularity) -
                    kPartitionPageSize;
#undef ALIGN
  static_assert(kSize > kGenericMaxBucketed,
                "allocation not large enough for direct allocation");
  size_t size = kSize - kExtraAllocSize;
  void* ptr = generic_allocator.root()->Alloc(size, type_name);

  EXPECT_TRUE(ptr);
  char* char_ptr = reinterpret_cast<char*>(ptr) - kPointerOffset;

  EXPECT_DEATH(*(char_ptr - 1) = 'A', "");
  EXPECT_DEATH(*(char_ptr + size + kExtraAllocSize) = 'A', "");

  generic_allocator.root()->Free(ptr);
}

// Check that a bad free() is caught where the free() refers to an unused
// partition page of a large allocation.
TEST_F(PartitionAllocDeathTest, FreeWrongPartitionPage) {
  // This large size will result in a direct mapped allocation with guard
  // pages at either end.
  void* ptr =
      generic_allocator.root()->Alloc(kPartitionPageSize * 2, type_name);
  EXPECT_TRUE(ptr);
  char* badPtr = reinterpret_cast<char*>(ptr) + kPartitionPageSize;

  EXPECT_DEATH(generic_allocator.root()->Free(badPtr), "");

  generic_allocator.root()->Free(ptr);
}

#endif  // !defined(OS_ANDROID) && !defined(OS_IOS)

// Tests that |PartitionDumpStatsGeneric| and |PartitionDumpStats| run without
// crashing and return non-zero values when memory is allocated.
TEST_F(PartitionAllocTest, DumpMemoryStats) {
  {
    void* ptr = allocator.root()->Alloc(kTestAllocSize, type_name);
    MockPartitionStatsDumper mock_stats_dumper;
    allocator.root()->DumpStats("mock_allocator", false /* detailed dump */,
                                &mock_stats_dumper);
    EXPECT_TRUE(mock_stats_dumper.IsMemoryAllocationRecorded());
    PartitionFree(ptr);
  }

  // This series of tests checks the active -> empty -> decommitted states.
  {
    {
      void* ptr =
          generic_allocator.root()->Alloc(2048 - kExtraAllocSize, type_name);
      MockPartitionStatsDumper dumper;
      generic_allocator.root()->DumpStats("mock_generic_allocator",
                                          false /* detailed dump */, &dumper);
      EXPECT_TRUE(dumper.IsMemoryAllocationRecorded());

      const PartitionBucketMemoryStats* stats = dumper.GetBucketStats(2048);
      EXPECT_TRUE(stats);
      EXPECT_TRUE(stats->is_valid);
      EXPECT_EQ(2048u, stats->bucket_slot_size);
      EXPECT_EQ(2048u, stats->active_bytes);
      EXPECT_EQ(kSystemPageSize, stats->resident_bytes);
      EXPECT_EQ(0u, stats->decommittable_bytes);
      EXPECT_EQ(0u, stats->discardable_bytes);
      EXPECT_EQ(0u, stats->num_full_pages);
      EXPECT_EQ(1u, stats->num_active_pages);
      EXPECT_EQ(0u, stats->num_empty_pages);
      EXPECT_EQ(0u, stats->num_decommitted_pages);
      generic_allocator.root()->Free(ptr);
    }

    {
      MockPartitionStatsDumper dumper;
      generic_allocator.root()->DumpStats("mock_generic_allocator",
                                          false /* detailed dump */, &dumper);
      EXPECT_FALSE(dumper.IsMemoryAllocationRecorded());

      const PartitionBucketMemoryStats* stats = dumper.GetBucketStats(2048);
      EXPECT_TRUE(stats);
      EXPECT_TRUE(stats->is_valid);
      EXPECT_EQ(2048u, stats->bucket_slot_size);
      EXPECT_EQ(0u, stats->active_bytes);
      EXPECT_EQ(kSystemPageSize, stats->resident_bytes);
      EXPECT_EQ(kSystemPageSize, stats->decommittable_bytes);
      EXPECT_EQ(0u, stats->discardable_bytes);
      EXPECT_EQ(0u, stats->num_full_pages);
      EXPECT_EQ(0u, stats->num_active_pages);
      EXPECT_EQ(1u, stats->num_empty_pages);
      EXPECT_EQ(0u, stats->num_decommitted_pages);
    }

    // TODO(crbug.com/722911): Commenting this out causes this test to fail when
    // run singly (--gtest_filter=PartitionAllocTest.DumpMemoryStats), but not
    // when run with the others (--gtest_filter=PartitionAllocTest.*).
    CycleGenericFreeCache(kTestAllocSize);

    {
      MockPartitionStatsDumper dumper;
      generic_allocator.root()->DumpStats("mock_generic_allocator",
                                          false /* detailed dump */, &dumper);
      EXPECT_FALSE(dumper.IsMemoryAllocationRecorded());

      const PartitionBucketMemoryStats* stats = dumper.GetBucketStats(2048);
      EXPECT_TRUE(stats);
      EXPECT_TRUE(stats->is_valid);
      EXPECT_EQ(2048u, stats->bucket_slot_size);
      EXPECT_EQ(0u, stats->active_bytes);
      EXPECT_EQ(0u, stats->resident_bytes);
      EXPECT_EQ(0u, stats->decommittable_bytes);
      EXPECT_EQ(0u, stats->discardable_bytes);
      EXPECT_EQ(0u, stats->num_full_pages);
      EXPECT_EQ(0u, stats->num_active_pages);
      EXPECT_EQ(0u, stats->num_empty_pages);
      EXPECT_EQ(1u, stats->num_decommitted_pages);
    }
  }

  // This test checks for correct empty page list accounting.
  {
    size_t size = kPartitionPageSize - kExtraAllocSize;
    void* ptr1 = generic_allocator.root()->Alloc(size, type_name);
    void* ptr2 = generic_allocator.root()->Alloc(size, type_name);
    generic_allocator.root()->Free(ptr1);
    generic_allocator.root()->Free(ptr2);

    CycleGenericFreeCache(kTestAllocSize);

    ptr1 = generic_allocator.root()->Alloc(size, type_name);

    {
      MockPartitionStatsDumper dumper;
      generic_allocator.root()->DumpStats("mock_generic_allocator",
                                          false /* detailed dump */, &dumper);
      EXPECT_TRUE(dumper.IsMemoryAllocationRecorded());

      const PartitionBucketMemoryStats* stats =
          dumper.GetBucketStats(kPartitionPageSize);
      EXPECT_TRUE(stats);
      EXPECT_TRUE(stats->is_valid);
      EXPECT_EQ(kPartitionPageSize, stats->bucket_slot_size);
      EXPECT_EQ(kPartitionPageSize, stats->active_bytes);
      EXPECT_EQ(kPartitionPageSize, stats->resident_bytes);
      EXPECT_EQ(0u, stats->decommittable_bytes);
      EXPECT_EQ(0u, stats->discardable_bytes);
      EXPECT_EQ(1u, stats->num_full_pages);
      EXPECT_EQ(0u, stats->num_active_pages);
      EXPECT_EQ(0u, stats->num_empty_pages);
      EXPECT_EQ(1u, stats->num_decommitted_pages);
    }
    generic_allocator.root()->Free(ptr1);
  }

  // This test checks for correct direct mapped accounting.
  {
    size_t size_smaller = kGenericMaxBucketed + 1;
    size_t size_bigger = (kGenericMaxBucketed * 2) + 1;
    size_t real_size_smaller =
        (size_smaller + kSystemPageOffsetMask) & kSystemPageBaseMask;
    size_t real_size_bigger =
        (size_bigger + kSystemPageOffsetMask) & kSystemPageBaseMask;
    void* ptr = generic_allocator.root()->Alloc(size_smaller, type_name);
    void* ptr2 = generic_allocator.root()->Alloc(size_bigger, type_name);

    {
      MockPartitionStatsDumper dumper;
      generic_allocator.root()->DumpStats("mock_generic_allocator",
                                          false /* detailed dump */, &dumper);
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
      EXPECT_EQ(1u, stats->num_full_pages);
      EXPECT_EQ(0u, stats->num_active_pages);
      EXPECT_EQ(0u, stats->num_empty_pages);
      EXPECT_EQ(0u, stats->num_decommitted_pages);

      stats = dumper.GetBucketStats(real_size_bigger);
      EXPECT_TRUE(stats);
      EXPECT_TRUE(stats->is_valid);
      EXPECT_TRUE(stats->is_direct_map);
      EXPECT_EQ(real_size_bigger, stats->bucket_slot_size);
      EXPECT_EQ(real_size_bigger, stats->active_bytes);
      EXPECT_EQ(real_size_bigger, stats->resident_bytes);
      EXPECT_EQ(0u, stats->decommittable_bytes);
      EXPECT_EQ(0u, stats->discardable_bytes);
      EXPECT_EQ(1u, stats->num_full_pages);
      EXPECT_EQ(0u, stats->num_active_pages);
      EXPECT_EQ(0u, stats->num_empty_pages);
      EXPECT_EQ(0u, stats->num_decommitted_pages);
    }

    generic_allocator.root()->Free(ptr2);
    generic_allocator.root()->Free(ptr);

    // Whilst we're here, allocate again and free with different ordering to
    // give a workout to our linked list code.
    ptr = generic_allocator.root()->Alloc(size_smaller, type_name);
    ptr2 = generic_allocator.root()->Alloc(size_bigger, type_name);
    generic_allocator.root()->Free(ptr);
    generic_allocator.root()->Free(ptr2);
  }

  // This test checks large-but-not-quite-direct allocations.
  {
    constexpr size_t requested_size = 16 * kSystemPageSize;
    void* ptr = generic_allocator.root()->Alloc(requested_size + 1, type_name);

    {
      MockPartitionStatsDumper dumper;
      generic_allocator.root()->DumpStats("mock_generic_allocator",
                                          false /* detailed dump */, &dumper);
      EXPECT_TRUE(dumper.IsMemoryAllocationRecorded());

      size_t slot_size =
          requested_size + (requested_size / kGenericNumBucketsPerOrder);
      const PartitionBucketMemoryStats* stats =
          dumper.GetBucketStats(slot_size);
      EXPECT_TRUE(stats);
      EXPECT_TRUE(stats->is_valid);
      EXPECT_FALSE(stats->is_direct_map);
      EXPECT_EQ(slot_size, stats->bucket_slot_size);
      EXPECT_EQ(requested_size + 1 + kExtraAllocSize, stats->active_bytes);
      EXPECT_EQ(slot_size, stats->resident_bytes);
      EXPECT_EQ(0u, stats->decommittable_bytes);
      EXPECT_EQ(kSystemPageSize, stats->discardable_bytes);
      EXPECT_EQ(1u, stats->num_full_pages);
      EXPECT_EQ(0u, stats->num_active_pages);
      EXPECT_EQ(0u, stats->num_empty_pages);
      EXPECT_EQ(0u, stats->num_decommitted_pages);
    }

    generic_allocator.root()->Free(ptr);

    {
      MockPartitionStatsDumper dumper;
      generic_allocator.root()->DumpStats("mock_generic_allocator",
                                          false /* detailed dump */, &dumper);
      EXPECT_FALSE(dumper.IsMemoryAllocationRecorded());

      size_t slot_size =
          requested_size + (requested_size / kGenericNumBucketsPerOrder);
      const PartitionBucketMemoryStats* stats =
          dumper.GetBucketStats(slot_size);
      EXPECT_TRUE(stats);
      EXPECT_TRUE(stats->is_valid);
      EXPECT_FALSE(stats->is_direct_map);
      EXPECT_EQ(slot_size, stats->bucket_slot_size);
      EXPECT_EQ(0u, stats->active_bytes);
      EXPECT_EQ(slot_size, stats->resident_bytes);
      EXPECT_EQ(slot_size, stats->decommittable_bytes);
      EXPECT_EQ(0u, stats->num_full_pages);
      EXPECT_EQ(0u, stats->num_active_pages);
      EXPECT_EQ(1u, stats->num_empty_pages);
      EXPECT_EQ(0u, stats->num_decommitted_pages);
    }

    void* ptr2 = generic_allocator.root()->Alloc(
        requested_size + kSystemPageSize + 1, type_name);
    EXPECT_EQ(ptr, ptr2);

    {
      MockPartitionStatsDumper dumper;
      generic_allocator.root()->DumpStats("mock_generic_allocator",
                                          false /* detailed dump */, &dumper);
      EXPECT_TRUE(dumper.IsMemoryAllocationRecorded());

      size_t slot_size =
          requested_size + (requested_size / kGenericNumBucketsPerOrder);
      const PartitionBucketMemoryStats* stats =
          dumper.GetBucketStats(slot_size);
      EXPECT_TRUE(stats);
      EXPECT_TRUE(stats->is_valid);
      EXPECT_FALSE(stats->is_direct_map);
      EXPECT_EQ(slot_size, stats->bucket_slot_size);
      EXPECT_EQ(requested_size + kSystemPageSize + 1 + kExtraAllocSize,
                stats->active_bytes);
      EXPECT_EQ(slot_size, stats->resident_bytes);
      EXPECT_EQ(0u, stats->decommittable_bytes);
      EXPECT_EQ(0u, stats->discardable_bytes);
      EXPECT_EQ(1u, stats->num_full_pages);
      EXPECT_EQ(0u, stats->num_active_pages);
      EXPECT_EQ(0u, stats->num_empty_pages);
      EXPECT_EQ(0u, stats->num_decommitted_pages);
    }

    generic_allocator.root()->Free(ptr2);
  }
}

// Tests the API to purge freeable memory.
TEST_F(PartitionAllocTest, Purge) {
  char* ptr = reinterpret_cast<char*>(
      generic_allocator.root()->Alloc(2048 - kExtraAllocSize, type_name));
  generic_allocator.root()->Free(ptr);
  {
    MockPartitionStatsDumper dumper;
    generic_allocator.root()->DumpStats("mock_generic_allocator",
                                        false /* detailed dump */, &dumper);
    EXPECT_FALSE(dumper.IsMemoryAllocationRecorded());

    const PartitionBucketMemoryStats* stats = dumper.GetBucketStats(2048);
    EXPECT_TRUE(stats);
    EXPECT_TRUE(stats->is_valid);
    EXPECT_EQ(kSystemPageSize, stats->decommittable_bytes);
    EXPECT_EQ(kSystemPageSize, stats->resident_bytes);
  }
  generic_allocator.root()->PurgeMemory(PartitionPurgeDecommitEmptyPages);
  {
    MockPartitionStatsDumper dumper;
    generic_allocator.root()->DumpStats("mock_generic_allocator",
                                        false /* detailed dump */, &dumper);
    EXPECT_FALSE(dumper.IsMemoryAllocationRecorded());

    const PartitionBucketMemoryStats* stats = dumper.GetBucketStats(2048);
    EXPECT_TRUE(stats);
    EXPECT_TRUE(stats->is_valid);
    EXPECT_EQ(0u, stats->decommittable_bytes);
    EXPECT_EQ(0u, stats->resident_bytes);
  }
  // Calling purge again here is a good way of testing we didn't mess up the
  // state of the free cache ring.
  generic_allocator.root()->PurgeMemory(PartitionPurgeDecommitEmptyPages);

  char* big_ptr = reinterpret_cast<char*>(
      generic_allocator.root()->Alloc(256 * 1024, type_name));
  generic_allocator.root()->Free(big_ptr);
  generic_allocator.root()->PurgeMemory(PartitionPurgeDecommitEmptyPages);

  CHECK_PAGE_IN_CORE(ptr - kPointerOffset, false);
  CHECK_PAGE_IN_CORE(big_ptr - kPointerOffset, false);
}

// Tests that we prefer to allocate into a non-empty partition page over an
// empty one. This is an important aspect of minimizing memory usage for some
// allocation sizes, particularly larger ones.
TEST_F(PartitionAllocTest, PreferActiveOverEmpty) {
  size_t size = (kSystemPageSize * 2) - kExtraAllocSize;
  // Allocate 3 full slot spans worth of 8192-byte allocations.
  // Each slot span for this size is 16384 bytes, or 1 partition page and 2
  // slots.
  void* ptr1 = generic_allocator.root()->Alloc(size, type_name);
  void* ptr2 = generic_allocator.root()->Alloc(size, type_name);
  void* ptr3 = generic_allocator.root()->Alloc(size, type_name);
  void* ptr4 = generic_allocator.root()->Alloc(size, type_name);
  void* ptr5 = generic_allocator.root()->Alloc(size, type_name);
  void* ptr6 = generic_allocator.root()->Alloc(size, type_name);

  PartitionPage* page1 =
      PartitionPage::FromPointer(PartitionCookieFreePointerAdjust(ptr1));
  PartitionPage* page2 =
      PartitionPage::FromPointer(PartitionCookieFreePointerAdjust(ptr3));
  PartitionPage* page3 =
      PartitionPage::FromPointer(PartitionCookieFreePointerAdjust(ptr6));
  EXPECT_NE(page1, page2);
  EXPECT_NE(page2, page3);
  PartitionBucket* bucket = page1->bucket;
  EXPECT_EQ(page3, bucket->active_pages_head);

  // Free up the 2nd slot in each slot span.
  // This leaves the active list containing 3 pages, each with 1 used and 1
  // free slot. The active page will be the one containing ptr1.
  generic_allocator.root()->Free(ptr6);
  generic_allocator.root()->Free(ptr4);
  generic_allocator.root()->Free(ptr2);
  EXPECT_EQ(page1, bucket->active_pages_head);

  // Empty the middle page in the active list.
  generic_allocator.root()->Free(ptr3);
  EXPECT_EQ(page1, bucket->active_pages_head);

  // Empty the the first page in the active list -- also the current page.
  generic_allocator.root()->Free(ptr1);

  // A good choice here is to re-fill the third page since the first two are
  // empty. We used to fail that.
  void* ptr7 = generic_allocator.root()->Alloc(size, type_name);
  EXPECT_EQ(ptr6, ptr7);
  EXPECT_EQ(page3, bucket->active_pages_head);

  generic_allocator.root()->Free(ptr5);
  generic_allocator.root()->Free(ptr7);
}

// Tests the API to purge discardable memory.
TEST_F(PartitionAllocTest, PurgeDiscardable) {
  // Free the second of two 4096 byte allocations and then purge.
  {
    void* ptr1 = generic_allocator.root()->Alloc(
        kSystemPageSize - kExtraAllocSize, type_name);
    char* ptr2 = reinterpret_cast<char*>(generic_allocator.root()->Alloc(
        kSystemPageSize - kExtraAllocSize, type_name));
    generic_allocator.root()->Free(ptr2);
    PartitionPage* page =
        PartitionPage::FromPointer(PartitionCookieFreePointerAdjust(ptr1));
    EXPECT_EQ(2u, page->num_unprovisioned_slots);
    {
      MockPartitionStatsDumper dumper;
      generic_allocator.root()->DumpStats("mock_generic_allocator",
                                          false /* detailed dump */, &dumper);
      EXPECT_TRUE(dumper.IsMemoryAllocationRecorded());

      const PartitionBucketMemoryStats* stats =
          dumper.GetBucketStats(kSystemPageSize);
      EXPECT_TRUE(stats);
      EXPECT_TRUE(stats->is_valid);
      EXPECT_EQ(0u, stats->decommittable_bytes);
      EXPECT_EQ(kSystemPageSize, stats->discardable_bytes);
      EXPECT_EQ(kSystemPageSize, stats->active_bytes);
      EXPECT_EQ(2 * kSystemPageSize, stats->resident_bytes);
    }
    CHECK_PAGE_IN_CORE(ptr2 - kPointerOffset, true);
    generic_allocator.root()->PurgeMemory(
        PartitionPurgeDiscardUnusedSystemPages);
    CHECK_PAGE_IN_CORE(ptr2 - kPointerOffset, false);
    EXPECT_EQ(3u, page->num_unprovisioned_slots);

    generic_allocator.root()->Free(ptr1);
  }
  // Free the first of two 4096 byte allocations and then purge.
  {
    char* ptr1 = reinterpret_cast<char*>(generic_allocator.root()->Alloc(
        kSystemPageSize - kExtraAllocSize, type_name));
    void* ptr2 = generic_allocator.root()->Alloc(
        kSystemPageSize - kExtraAllocSize, type_name);
    generic_allocator.root()->Free(ptr1);
    {
      MockPartitionStatsDumper dumper;
      generic_allocator.root()->DumpStats("mock_generic_allocator",
                                          false /* detailed dump */, &dumper);
      EXPECT_TRUE(dumper.IsMemoryAllocationRecorded());

      const PartitionBucketMemoryStats* stats =
          dumper.GetBucketStats(kSystemPageSize);
      EXPECT_TRUE(stats);
      EXPECT_TRUE(stats->is_valid);
      EXPECT_EQ(0u, stats->decommittable_bytes);
#if defined(OS_WIN)
      EXPECT_EQ(0u, stats->discardable_bytes);
#else
      EXPECT_EQ(kSystemPageSize, stats->discardable_bytes);
#endif
      EXPECT_EQ(kSystemPageSize, stats->active_bytes);
      EXPECT_EQ(2 * kSystemPageSize, stats->resident_bytes);
    }
    CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset, true);
    generic_allocator.root()->PurgeMemory(
        PartitionPurgeDiscardUnusedSystemPages);
    CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset, false);

    generic_allocator.root()->Free(ptr2);
  }
  {
    constexpr size_t requested_size = 2.25 * kSystemPageSize;
    char* ptr1 = reinterpret_cast<char*>(generic_allocator.root()->Alloc(
        requested_size - kExtraAllocSize, type_name));
    void* ptr2 = generic_allocator.root()->Alloc(
        requested_size - kExtraAllocSize, type_name);
    void* ptr3 = generic_allocator.root()->Alloc(
        requested_size - kExtraAllocSize, type_name);
    void* ptr4 = generic_allocator.root()->Alloc(
        requested_size - kExtraAllocSize, type_name);
    memset(ptr1, 'A', requested_size - kExtraAllocSize);
    memset(ptr2, 'A', requested_size - kExtraAllocSize);
    generic_allocator.root()->Free(ptr2);
    generic_allocator.root()->Free(ptr1);
    {
      MockPartitionStatsDumper dumper;
      generic_allocator.root()->DumpStats("mock_generic_allocator",
                                          false /* detailed dump */, &dumper);
      EXPECT_TRUE(dumper.IsMemoryAllocationRecorded());

      const PartitionBucketMemoryStats* stats =
          dumper.GetBucketStats(requested_size);
      EXPECT_TRUE(stats);
      EXPECT_TRUE(stats->is_valid);
      EXPECT_EQ(0u, stats->decommittable_bytes);
      EXPECT_EQ(2 * kSystemPageSize, stats->discardable_bytes);
      EXPECT_EQ(requested_size * 2, stats->active_bytes);
      EXPECT_EQ(9 * kSystemPageSize, stats->resident_bytes);
    }
    CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset, true);
    CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset + kSystemPageSize, true);
    CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset + (kSystemPageSize * 2), true);
    CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset + (kSystemPageSize * 3), true);
    CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset + (kSystemPageSize * 4), true);
    generic_allocator.root()->PurgeMemory(
        PartitionPurgeDiscardUnusedSystemPages);
    CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset, true);
    CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset + kSystemPageSize, false);
    CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset + (kSystemPageSize * 2), true);
    CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset + (kSystemPageSize * 3), false);
    CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset + (kSystemPageSize * 4), true);

    generic_allocator.root()->Free(ptr3);
    generic_allocator.root()->Free(ptr4);
  }

// When kSystemPageSize = 16384 (as on _MIPS_ARCH_LOONGSON), 64 *
// kSystemPageSize (see the #else branch below) caused this test to OOM.
// Therefore, for systems with 16 KiB pages, use 32 * kSystemPageSize.
//
// TODO(palmer): Refactor this to branch on page size instead of architecture,
// for clarity of purpose and for applicability to more architectures.
#if defined(_MIPS_ARCH_LOONGSON)
  {
    char* ptr1 = reinterpret_cast<char*>(generic_allocator.root()->Alloc(
        (32 * kSystemPageSize) - kExtraAllocSize, type_name));
    memset(ptr1, 'A', (32 * kSystemPageSize) - kExtraAllocSize);
    generic_allocator.root()->Free(ptr1);
    ptr1 = reinterpret_cast<char*>(generic_allocator.root()->Alloc(
        (31 * kSystemPageSize) - kExtraAllocSize, type_name));
    {
      MockPartitionStatsDumper dumper;
      generic_allocator.root()->DumpStats("mock_generic_allocator",
                                          false /* detailed dump */, &dumper);
      EXPECT_TRUE(dumper.IsMemoryAllocationRecorded());

      const PartitionBucketMemoryStats* stats =
          dumper.GetBucketStats(32 * kSystemPageSize);
      EXPECT_TRUE(stats);
      EXPECT_TRUE(stats->is_valid);
      EXPECT_EQ(0u, stats->decommittable_bytes);
      EXPECT_EQ(kSystemPageSize, stats->discardable_bytes);
      EXPECT_EQ(31 * kSystemPageSize, stats->active_bytes);
      EXPECT_EQ(32 * kSystemPageSize, stats->resident_bytes);
    }
    CheckPageInCore(ptr1 - kPointerOffset + (kSystemPageSize * 30), true);
    CheckPageInCore(ptr1 - kPointerOffset + (kSystemPageSize * 31), true);
    generic_allocator.root()->PurgeMemory(
        PartitionPurgeDiscardUnusedSystemPages);
    CheckPageInCore(ptr1 - kPointerOffset + (kSystemPageSize * 30), true);
    CheckPageInCore(ptr1 - kPointerOffset + (kSystemPageSize * 31), false);

    generic_allocator.root()->Free(ptr1);
  }
#else
  {
    char* ptr1 = reinterpret_cast<char*>(generic_allocator.root()->Alloc(
        (64 * kSystemPageSize) - kExtraAllocSize, type_name));
    memset(ptr1, 'A', (64 * kSystemPageSize) - kExtraAllocSize);
    generic_allocator.root()->Free(ptr1);
    ptr1 = reinterpret_cast<char*>(generic_allocator.root()->Alloc(
        (61 * kSystemPageSize) - kExtraAllocSize, type_name));
    {
      MockPartitionStatsDumper dumper;
      generic_allocator.root()->DumpStats("mock_generic_allocator",
                                          false /* detailed dump */, &dumper);
      EXPECT_TRUE(dumper.IsMemoryAllocationRecorded());

      const PartitionBucketMemoryStats* stats =
          dumper.GetBucketStats(64 * kSystemPageSize);
      EXPECT_TRUE(stats);
      EXPECT_TRUE(stats->is_valid);
      EXPECT_EQ(0u, stats->decommittable_bytes);
      EXPECT_EQ(3 * kSystemPageSize, stats->discardable_bytes);
      EXPECT_EQ(61 * kSystemPageSize, stats->active_bytes);
      EXPECT_EQ(64 * kSystemPageSize, stats->resident_bytes);
    }
    CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset + (kSystemPageSize * 60), true);
    CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset + (kSystemPageSize * 61), true);
    CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset + (kSystemPageSize * 62), true);
    CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset + (kSystemPageSize * 63), true);
    generic_allocator.root()->PurgeMemory(
        PartitionPurgeDiscardUnusedSystemPages);
    CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset + (kSystemPageSize * 60), true);
    CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset + (kSystemPageSize * 61), false);
    CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset + (kSystemPageSize * 62), false);
    CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset + (kSystemPageSize * 63), false);

    generic_allocator.root()->Free(ptr1);
  }
#endif
  // This sub-test tests truncation of the provisioned slots in a trickier
  // case where the freelist is rewritten.
  generic_allocator.root()->PurgeMemory(PartitionPurgeDecommitEmptyPages);
  {
    char* ptr1 = reinterpret_cast<char*>(generic_allocator.root()->Alloc(
        kSystemPageSize - kExtraAllocSize, type_name));
    void* ptr2 = generic_allocator.root()->Alloc(
        kSystemPageSize - kExtraAllocSize, type_name);
    void* ptr3 = generic_allocator.root()->Alloc(
        kSystemPageSize - kExtraAllocSize, type_name);
    void* ptr4 = generic_allocator.root()->Alloc(
        kSystemPageSize - kExtraAllocSize, type_name);
    ptr1[0] = 'A';
    ptr1[kSystemPageSize] = 'A';
    ptr1[kSystemPageSize * 2] = 'A';
    ptr1[kSystemPageSize * 3] = 'A';
    PartitionPage* page =
        PartitionPage::FromPointer(PartitionCookieFreePointerAdjust(ptr1));
    generic_allocator.root()->Free(ptr2);
    generic_allocator.root()->Free(ptr4);
    generic_allocator.root()->Free(ptr1);
    EXPECT_EQ(0u, page->num_unprovisioned_slots);

    {
      MockPartitionStatsDumper dumper;
      generic_allocator.root()->DumpStats("mock_generic_allocator",
                                          false /* detailed dump */, &dumper);
      EXPECT_TRUE(dumper.IsMemoryAllocationRecorded());

      const PartitionBucketMemoryStats* stats =
          dumper.GetBucketStats(kSystemPageSize);
      EXPECT_TRUE(stats);
      EXPECT_TRUE(stats->is_valid);
      EXPECT_EQ(0u, stats->decommittable_bytes);
#if defined(OS_WIN)
      EXPECT_EQ(kSystemPageSize, stats->discardable_bytes);
#else
      EXPECT_EQ(2 * kSystemPageSize, stats->discardable_bytes);
#endif
      EXPECT_EQ(kSystemPageSize, stats->active_bytes);
      EXPECT_EQ(4 * kSystemPageSize, stats->resident_bytes);
    }
    CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset, true);
    CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset + kSystemPageSize, true);
    CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset + (kSystemPageSize * 2), true);
    CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset + (kSystemPageSize * 3), true);
    generic_allocator.root()->PurgeMemory(
        PartitionPurgeDiscardUnusedSystemPages);
    EXPECT_EQ(1u, page->num_unprovisioned_slots);
    CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset, true);
    CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset + kSystemPageSize, false);
    CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset + (kSystemPageSize * 2), true);
    CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset + (kSystemPageSize * 3), false);

    // Let's check we didn't brick the freelist.
    void* ptr1b = generic_allocator.root()->Alloc(
        kSystemPageSize - kExtraAllocSize, type_name);
    EXPECT_EQ(ptr1, ptr1b);
    void* ptr2b = generic_allocator.root()->Alloc(
        kSystemPageSize - kExtraAllocSize, type_name);
    EXPECT_EQ(ptr2, ptr2b);
    EXPECT_FALSE(page->freelist_head);

    generic_allocator.root()->Free(ptr1);
    generic_allocator.root()->Free(ptr2);
    generic_allocator.root()->Free(ptr3);
  }
  // This sub-test is similar, but tests a double-truncation.
  generic_allocator.root()->PurgeMemory(PartitionPurgeDecommitEmptyPages);
  {
    char* ptr1 = reinterpret_cast<char*>(generic_allocator.root()->Alloc(
        kSystemPageSize - kExtraAllocSize, type_name));
    void* ptr2 = generic_allocator.root()->Alloc(
        kSystemPageSize - kExtraAllocSize, type_name);
    void* ptr3 = generic_allocator.root()->Alloc(
        kSystemPageSize - kExtraAllocSize, type_name);
    void* ptr4 = generic_allocator.root()->Alloc(
        kSystemPageSize - kExtraAllocSize, type_name);
    ptr1[0] = 'A';
    ptr1[kSystemPageSize] = 'A';
    ptr1[kSystemPageSize * 2] = 'A';
    ptr1[kSystemPageSize * 3] = 'A';
    PartitionPage* page =
        PartitionPage::FromPointer(PartitionCookieFreePointerAdjust(ptr1));
    generic_allocator.root()->Free(ptr4);
    generic_allocator.root()->Free(ptr3);
    EXPECT_EQ(0u, page->num_unprovisioned_slots);

    {
      MockPartitionStatsDumper dumper;
      generic_allocator.root()->DumpStats("mock_generic_allocator",
                                          false /* detailed dump */, &dumper);
      EXPECT_TRUE(dumper.IsMemoryAllocationRecorded());

      const PartitionBucketMemoryStats* stats =
          dumper.GetBucketStats(kSystemPageSize);
      EXPECT_TRUE(stats);
      EXPECT_TRUE(stats->is_valid);
      EXPECT_EQ(0u, stats->decommittable_bytes);
      EXPECT_EQ(2 * kSystemPageSize, stats->discardable_bytes);
      EXPECT_EQ(2 * kSystemPageSize, stats->active_bytes);
      EXPECT_EQ(4 * kSystemPageSize, stats->resident_bytes);
    }
    CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset, true);
    CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset + kSystemPageSize, true);
    CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset + (kSystemPageSize * 2), true);
    CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset + (kSystemPageSize * 3), true);
    generic_allocator.root()->PurgeMemory(
        PartitionPurgeDiscardUnusedSystemPages);
    EXPECT_EQ(2u, page->num_unprovisioned_slots);
    CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset, true);
    CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset + kSystemPageSize, true);
    CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset + (kSystemPageSize * 2), false);
    CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset + (kSystemPageSize * 3), false);

    EXPECT_FALSE(page->freelist_head);

    generic_allocator.root()->Free(ptr1);
    generic_allocator.root()->Free(ptr2);
  }
}

TEST_F(PartitionAllocTest, ReallocMovesCookies) {
  // Resize so as to be sure to hit a "resize in place" case, and ensure that
  // use of the entire result is compatible with the debug mode's cookies, even
  // when the bucket size is large enough to span more than one partition page
  // and we can track the "raw" size. See https://crbug.com/709271
  static constexpr size_t kSize =
      base::kMaxSystemPagesPerSlotSpan * base::kSystemPageSize;
  void* ptr = generic_allocator.root()->Alloc(kSize + 1, type_name);
  EXPECT_TRUE(ptr);

  memset(ptr, 0xbd, kSize + 1);
  ptr = generic_allocator.root()->Realloc(ptr, kSize + 2, type_name);
  EXPECT_TRUE(ptr);

  memset(ptr, 0xbd, kSize + 2);
  generic_allocator.root()->Free(ptr);
}

TEST_F(PartitionAllocTest, SmallReallocDoesNotMoveTrailingCookie) {
  // For crbug.com/781473
  static constexpr size_t kSize = 264;
  void* ptr = generic_allocator.root()->Alloc(kSize, type_name);
  EXPECT_TRUE(ptr);

  ptr = generic_allocator.root()->Realloc(ptr, kSize + 16, type_name);
  EXPECT_TRUE(ptr);

  generic_allocator.root()->Free(ptr);
}

TEST_F(PartitionAllocTest, ZeroFill) {
  constexpr static size_t kAllZerosSentinel =
      std::numeric_limits<size_t>::max();
  for (size_t size : kTestSizes) {
    char* p = static_cast<char*>(PartitionAllocGenericFlags(
        generic_allocator.root(), PartitionAllocZeroFill, size, nullptr));
    size_t non_zero_position = kAllZerosSentinel;
    for (size_t i = 0; i < size; ++i) {
      if (0 != p[i]) {
        non_zero_position = i;
        break;
      }
    }
    EXPECT_EQ(kAllZerosSentinel, non_zero_position)
        << "test allocation size: " << size;
    PartitionFree(p);
  }

  for (int i = 0; i < 10; ++i) {
    SCOPED_TRACE(i);
    AllocateRandomly(generic_allocator.root(), 250, PartitionAllocZeroFill);
  }
}

TEST_F(PartitionAllocTest, Bug_897585) {
  // Need sizes big enough to be direct mapped and a delta small enough to
  // allow re-use of the page when cookied. These numbers fall out of the
  // test case in the indicated bug.
  size_t kInitialSize = 983040;
  size_t kDesiredSize = 983100;
  void* ptr = PartitionAllocGenericFlags(generic_allocator.root(),
                                         PartitionAllocReturnNull, kInitialSize,
                                         nullptr);
  ASSERT_NE(nullptr, ptr);
  ptr = PartitionReallocGenericFlags(generic_allocator.root(),
                                     PartitionAllocReturnNull, ptr,
                                     kDesiredSize, nullptr);
  ASSERT_NE(nullptr, ptr);
  memset(ptr, 0xbd, kDesiredSize);
  PartitionFree(ptr);
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

  void* ptr = PartitionAllocGenericFlags(generic_allocator.root(),
                                         PartitionAllocReturnNull,
                                         kOverriddenSize, kOverriddenType);
  ASSERT_EQ(ptr, overridden_allocation);

  PartitionFree(ptr);
  EXPECT_TRUE(free_called);

  // overridden_allocation has not actually been freed so we can now immediately
  // realloc it.
  free_called = false;
  ptr = PartitionReallocGenericFlags(generic_allocator.root(),
                                     PartitionAllocReturnNull, ptr, 1, nullptr);
  ASSERT_NE(ptr, nullptr);
  EXPECT_NE(ptr, overridden_allocation);
  EXPECT_TRUE(free_called);
  EXPECT_EQ(*(char*)ptr, kOverriddenChar);
  PartitionFree(ptr);

  PartitionAllocHooks::SetOverrideHooks(nullptr, nullptr, nullptr);
  free(overridden_allocation);
}

}  // namespace internal
}  // namespace base

#endif  // !defined(MEMORY_TOOL_REPLACES_ALLOCATOR)
