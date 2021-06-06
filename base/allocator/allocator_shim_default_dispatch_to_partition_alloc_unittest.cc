// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/allocator_shim_default_dispatch_to_partition_alloc.h"

#include <cstdlib>
#include <cstring>

#include "base/allocator/buildflags.h"
#include "base/compiler_specific.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
#include <malloc.h>
#endif

#if !defined(MEMORY_TOOL_REPLACES_ALLOCATOR) && BUILDFLAG(USE_PARTITION_ALLOC)
namespace base {
namespace internal {

#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

// Platforms on which we override weak libc symbols.
#if defined(OS_LINUX) || defined(OS_CHROMEOS)

NOINLINE void FreeForTest(void* data) {
  free(data);
}

TEST(PartitionAllocAsMalloc, Mallinfo) {
  constexpr int kLargeAllocSize = 10 * 1024 * 1024;
  struct mallinfo before = mallinfo();
  void* data = malloc(1000);
  ASSERT_TRUE(data);
  void* aligned_data;
  ASSERT_EQ(0, posix_memalign(&aligned_data, 1024, 1000));
  ASSERT_TRUE(aligned_data);
  void* direct_mapped_data = malloc(kLargeAllocSize);
  ASSERT_TRUE(direct_mapped_data);
  struct mallinfo after_alloc = mallinfo();

  // Something is reported.
  EXPECT_GT(after_alloc.hblks, 0);
  EXPECT_GT(after_alloc.hblkhd, 0);
  EXPECT_GT(after_alloc.uordblks, 0);

  EXPECT_GT(after_alloc.hblks, kLargeAllocSize);

  // malloc() can reuse memory, so sizes are not necessarily changing, which
  // would mean that we need EXPECT_G*E*() rather than EXPECT_GT().
  //
  // However since we allocate direct-mapped memory, this increases the total.
  EXPECT_GT(after_alloc.hblks, before.hblks);
  EXPECT_GT(after_alloc.hblkhd, before.hblkhd);
  EXPECT_GT(after_alloc.uordblks, before.uordblks);

  // a simple malloc() / free() pair can be discarded by the compiler (and is),
  // making the test fail. It is sufficient to make |FreeForTest()| a NOINLINE
  // function for the call to not be eliminated, but this is required.
  FreeForTest(data);
  FreeForTest(aligned_data);
  FreeForTest(direct_mapped_data);
  struct mallinfo after_free = mallinfo();

  EXPECT_LT(after_free.hblks, after_alloc.hblks);
  EXPECT_LT(after_free.hblkhd, after_alloc.hblkhd);
  EXPECT_LT(after_free.uordblks, after_alloc.uordblks);
}

#endif  // defined(OS_LINUX) || defined(OS_CHROMEOS)

#endif  // BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

// Note: the tests below are quite simple, they are used as simple smoke tests
// for PartitionAlloc-Everywhere. Most of these directly dispatch to
// PartitionAlloc, which has much more extensive tests.
TEST(PartitionAllocAsMalloc, Simple) {
  void* data = PartitionMalloc(nullptr, 10, nullptr);
  EXPECT_TRUE(data);
  PartitionFree(nullptr, data, nullptr);
}

TEST(PartitionAllocAsMalloc, MallocUnchecked) {
  void* data = PartitionMallocUnchecked(nullptr, 10, nullptr);
  EXPECT_TRUE(data);
  PartitionFree(nullptr, data, nullptr);

  void* too_large = PartitionMallocUnchecked(nullptr, 4e9, nullptr);
  EXPECT_FALSE(too_large);  // No crash.
}

TEST(PartitionAllocAsMalloc, Calloc) {
  constexpr size_t alloc_size = 100;
  void* data = PartitionCalloc(nullptr, 1, alloc_size, nullptr);
  EXPECT_TRUE(data);

  char* zeroes[alloc_size];
  memset(zeroes, 0, alloc_size);

  EXPECT_EQ(0, memcmp(zeroes, data, alloc_size));
  PartitionFree(nullptr, data, nullptr);
}

TEST(PartitionAllocAsMalloc, Memalign) {
  constexpr size_t alloc_size = 100;
  constexpr size_t alignment = 1024;
  void* data = PartitionMemalign(nullptr, alignment, alloc_size, nullptr);
  EXPECT_TRUE(data);
  EXPECT_EQ(0u, reinterpret_cast<uintptr_t>(data) % alignment);
  PartitionFree(nullptr, data, nullptr);
}

TEST(PartitionAllocAsMalloc, AlignedAlloc) {
  for (size_t alloc_size : {100, 100000, 10000000}) {
    for (size_t alignment = 1; alignment <= kMaxSupportedAlignment;
         alignment <<= 1) {
      void* data =
          PartitionAlignedAlloc(nullptr, alloc_size, alignment, nullptr);
      EXPECT_TRUE(data);
      EXPECT_EQ(0u, reinterpret_cast<uintptr_t>(data) % alignment);
      PartitionFree(nullptr, data, nullptr);
    }
  }
}

TEST(PartitionAllocAsMalloc, AlignedRealloc) {
  for (size_t alloc_size : {100, 100000, 10000000}) {
    for (size_t alignment = 1; alignment <= kMaxSupportedAlignment;
         alignment <<= 1) {
      void* data =
          PartitionAlignedAlloc(nullptr, alloc_size, alignment, nullptr);
      EXPECT_TRUE(data);

      void* data2 = PartitionAlignedRealloc(nullptr, data, alloc_size,
                                            alignment, nullptr);
      EXPECT_TRUE(data2);

      // Aligned realloc always relocates.
      EXPECT_NE(reinterpret_cast<uintptr_t>(data),
                reinterpret_cast<uintptr_t>(data2));
      PartitionFree(nullptr, data2, nullptr);
    }
  }
}

TEST(PartitionAllocAsMalloc, Realloc) {
  constexpr size_t alloc_size = 100;
  void* data = PartitionMalloc(nullptr, alloc_size, nullptr);
  EXPECT_TRUE(data);
  void* data2 = PartitionMalloc(nullptr, 2 * alloc_size, nullptr);
  EXPECT_TRUE(data2);
  EXPECT_NE(data2, data);
  PartitionFree(nullptr, data2, nullptr);
}

// crbug.com/1141752
TEST(PartitionAllocAsMalloc, Alignment) {
  EXPECT_EQ(0u, reinterpret_cast<uintptr_t>(PartitionAllocMalloc::Allocator()) %
                    alignof(ThreadSafePartitionRoot));
  // This works fine even if nullptr is returned.
  EXPECT_EQ(0u, reinterpret_cast<uintptr_t>(
                    PartitionAllocMalloc::OriginalAllocator()) %
                    alignof(ThreadSafePartitionRoot));
  EXPECT_EQ(0u, reinterpret_cast<uintptr_t>(
                    PartitionAllocMalloc::AlignedAllocator()) %
                    alignof(ThreadSafePartitionRoot));
}

}  // namespace internal
}  // namespace base
#endif  // !defined(MEMORY_TOOL_REPLACES_ALLOCATOR) &&
        // BUILDFLAG(USE_PARTITION_ALLOC)
