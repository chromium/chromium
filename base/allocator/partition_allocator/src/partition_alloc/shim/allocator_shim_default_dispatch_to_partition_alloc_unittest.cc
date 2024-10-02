// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/shim/allocator_shim_default_dispatch_to_partition_alloc.h"

#include <cstdlib>
#include <cstring>

#include "partition_alloc/build_config.h"
#include "partition_alloc/buildflags.h"
#include "partition_alloc/partition_alloc_base/compiler_specific.h"
#include "partition_alloc/partition_alloc_base/memory/page_size.h"
#include "partition_alloc/partition_alloc_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

#if PA_BUILDFLAG(IS_LINUX) || PA_BUILDFLAG(IS_CHROMEOS)
#include <malloc.h>
#endif

#if PA_BUILDFLAG(IS_APPLE)
#include <malloc/malloc.h>
#endif

#if !defined(MEMORY_TOOL_REPLACES_ALLOCATOR) && \
    PA_BUILDFLAG(USE_PARTITION_ALLOC)
namespace allocator_shim::internal {

#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

// Platforms on which we override weak libc symbols.
#if PA_BUILDFLAG(IS_LINUX) || PA_BUILDFLAG(IS_CHROMEOS)

PA_NOINLINE void FreeForTest(void* data) {
  free(data);
}

TEST(PartitionAllocAsMalloc, Mallinfo) {
  // mallinfo was deprecated in glibc 2.33. The Chrome OS device sysroot has
  // a new-enough glibc, but the Linux one doesn't yet, so we can't switch to
  // the replacement mallinfo2 yet.
  // Once we update the Linux sysroot to be new enough, this warning will
  // start firing on Linux too. At that point, s/mallinfo/mallinfo2/ in this
  // file and remove the pragma here and and the end of this function.
#if defined(__GLIBC_PREREQ) && __GLIBC_PREREQ(2, 33)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif
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
  // making the test fail. It is sufficient to make |FreeForTest()| a
  // PA_NOINLINE function for the call to not be eliminated, but this is
  // required.
  FreeForTest(data);
  FreeForTest(aligned_data);
  FreeForTest(direct_mapped_data);
  struct mallinfo after_free = mallinfo();

  EXPECT_LT(after_free.hblks, after_alloc.hblks);
  EXPECT_LT(after_free.hblkhd, after_alloc.hblkhd);
  EXPECT_LT(after_free.uordblks, after_alloc.uordblks);
#if defined(__GLIBC_PREREQ) && __GLIBC_PREREQ(2, 33)
#pragma clang diagnostic pop
#endif
}

#endif  // PA_BUILDFLAG(IS_LINUX) || PA_BUILDFLAG(IS_CHROMEOS)

#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

// Note: the tests below are quite simple, they are used as simple smoke tests
// for PartitionAlloc-Everywhere. Most of these directly dispatch to
// PartitionAlloc, which has much more extensive tests.
TEST(PartitionAllocAsMalloc, Simple) {
  void* data = PartitionAllocFunctions::Malloc(10, nullptr);
  EXPECT_TRUE(data);
  PartitionAllocFunctions::Free(data, nullptr);
}

TEST(PartitionAllocAsMalloc, MallocUnchecked) {
  void* data = PartitionAllocFunctions::MallocUnchecked(10, nullptr);
  EXPECT_TRUE(data);
  PartitionAllocFunctions::Free(data, nullptr);

  void* too_large = PartitionAllocFunctions::MallocUnchecked(4e9, nullptr);
  EXPECT_FALSE(too_large);  // No crash.
}

TEST(PartitionAllocAsMalloc, Calloc) {
  constexpr size_t alloc_size = 100;
  void* data = PartitionAllocFunctions::Calloc(1, alloc_size, nullptr);
  EXPECT_TRUE(data);

  char* zeroes[alloc_size];
  memset(zeroes, 0, alloc_size);

  EXPECT_EQ(0, memcmp(zeroes, data, alloc_size));
  PartitionAllocFunctions::Free(data, nullptr);
}

TEST(PartitionAllocAsMalloc, Memalign) {
  constexpr size_t alloc_size = 100;
  constexpr size_t alignment = 1024;
  void* data =
      PartitionAllocFunctions::Memalign(alignment, alloc_size, nullptr);
  EXPECT_TRUE(data);
  EXPECT_EQ(0u, reinterpret_cast<uintptr_t>(data) % alignment);
  PartitionAllocFunctions::Free(data, nullptr);
}

TEST(PartitionAllocAsMalloc, AlignedAlloc) {
  for (size_t alloc_size : {100, 100000, 10000000}) {
    for (size_t alignment = 1;
         alignment <= partition_alloc::kMaxSupportedAlignment;
         alignment <<= 1) {
      void* data =
          PartitionAllocFunctions::AlignedAlloc(alloc_size, alignment, nullptr);
      EXPECT_TRUE(data);
      EXPECT_EQ(0u, reinterpret_cast<uintptr_t>(data) % alignment);
      PartitionAllocFunctions::Free(data, nullptr);
    }
  }
}

TEST(PartitionAllocAsMalloc, AlignedRealloc) {
  for (size_t alloc_size : {100, 100000, 10000000}) {
    for (size_t alignment = 1;
         alignment <= partition_alloc::kMaxSupportedAlignment;
         alignment <<= 1) {
      void* data =
          PartitionAllocFunctions::AlignedAlloc(alloc_size, alignment, nullptr);
      EXPECT_TRUE(data);

      void* data2 = PartitionAllocFunctions::AlignedRealloc(data, alloc_size,
                                                            alignment, nullptr);
      EXPECT_TRUE(data2);

      // Aligned realloc always relocates.
      EXPECT_NE(reinterpret_cast<uintptr_t>(data),
                reinterpret_cast<uintptr_t>(data2));
      PartitionAllocFunctions::Free(data2, nullptr);
    }
  }
}

TEST(PartitionAllocAsMalloc, Realloc) {
  constexpr size_t alloc_size = 100;
  void* data = PartitionAllocFunctions::Malloc(alloc_size, nullptr);
  EXPECT_TRUE(data);
  void* data2 =
      PartitionAllocFunctions::Realloc(data, 2u * alloc_size, nullptr);
  EXPECT_TRUE(data2);
  EXPECT_NE(data2, data);
  PartitionAllocFunctions::Free(data2, nullptr);
}

// crbug.com/1141752
TEST(PartitionAllocAsMalloc, Alignment) {
  EXPECT_EQ(0u, reinterpret_cast<uintptr_t>(PartitionAllocMalloc::Allocator()) %
                    alignof(partition_alloc::PartitionRoot));
  // This works fine even if nullptr is returned.
  EXPECT_EQ(0u, reinterpret_cast<uintptr_t>(
                    PartitionAllocMalloc::OriginalAllocator()) %
                    alignof(partition_alloc::PartitionRoot));
}

#if PA_BUILDFLAG(IS_APPLE) && PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
// Make sure that a sequence a "good sizes" grows fast enough. This is
// implicitly required by CoreFoundation, and to match Apple's implementation.
// Non-regression test for crbug.com/1501312
TEST(PartitionAllocAsMalloc, GoodSize) {
  size_t size = 1;
  int iterations = 0;
  while (size < 256 * 1024) {
    iterations++;
    size = malloc_good_size(size + 1);
  }
  EXPECT_LT(iterations, 100);
}
#endif  // PA_BUILDFLAG(IS_APPLE) && PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

}  // namespace allocator_shim::internal
#endif  // !defined(MEMORY_TOOL_REPLACES_ALLOCATOR) &&
        // PA_BUILDFLAG(USE_PARTITION_ALLOC)
