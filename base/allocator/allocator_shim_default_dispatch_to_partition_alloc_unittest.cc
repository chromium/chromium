// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/buildflags.h"
#include "base/compiler_specific.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_POSIX) && !defined(OS_APPLE)
#include <malloc.h>
#include <stdlib.h>
#endif

#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) && \
    !defined(MEMORY_TOOL_REPLACES_ALLOCATOR)

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

#endif  // BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) && \
        // !defined(MEMORY_TOOL_REPLACES_ALLOCATOR)
