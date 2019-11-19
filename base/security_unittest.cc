// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <algorithm>
#include <limits>
#include <memory>

#include "base/allocator/buildflags.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/free_deleter.h"
#include "base/sanitizer_buildflags.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_POSIX)
#include <sys/mman.h>
#include <unistd.h>
#endif

using std::nothrow;
using std::numeric_limits;

namespace {

// This function acts as a compiler optimization barrier. We use it to
// prevent the compiler from making an expression a compile-time constant.
// We also use it so that the compiler doesn't discard certain return values
// as something we don't need (see the comment with calloc below).
template <typename Type>
NOINLINE Type HideValueFromCompiler(volatile Type value) {
#if defined(__GNUC__)
  // In a GCC compatible compiler (GCC or Clang), make this compiler barrier
  // more robust than merely using "volatile".
  __asm__ volatile ("" : "+r" (value));
#endif  // __GNUC__
  return value;
}

// TCmalloc, currently supported only by Linux/CrOS, supports malloc limits.
// - USE_TCMALLOC (should be set if compiled with use_allocator=="tcmalloc")
// - ADDRESS_SANITIZER it has its own memory allocator
#if defined(OS_LINUX) && BUILDFLAG(USE_TCMALLOC) && !defined(ADDRESS_SANITIZER)
#define MALLOC_OVERFLOW_TEST(function) function
#else
#define MALLOC_OVERFLOW_TEST(function) DISABLED_##function
#endif

// There are platforms where these tests are known to fail. We would like to
// be able to easily check the status on the bots, but marking tests as
// FAILS_ is too clunky.
void OverflowTestsSoftExpectTrue(bool overflow_detected) {
  if (!overflow_detected) {
#if defined(OS_LINUX) || defined(OS_ANDROID) || defined(OS_MACOSX)
    // Sadly, on Linux, Android, and OSX we don't have a good story yet. Don't
    // fail the test, but report.
    printf("Platform has overflow: %s\n",
           !overflow_detected ? "yes." : "no.");
#else
    // Otherwise, fail the test. (Note: EXPECT are ok in subfunctions, ASSERT
    // aren't).
    EXPECT_TRUE(overflow_detected);
#endif
  }
}

#if defined(OS_IOS) || defined(OS_FUCHSIA) || defined(OS_MACOSX) || \
    defined(ADDRESS_SANITIZER) || defined(THREAD_SANITIZER) ||      \
    defined(MEMORY_SANITIZER) || BUILDFLAG(IS_HWASAN)
#define MAYBE_NewOverflow DISABLED_NewOverflow
#else
#define MAYBE_NewOverflow NewOverflow
#endif
// Test array[TooBig][X] and array[X][TooBig] allocations for int
// overflows.  IOS doesn't honor nothrow, so disable the test there.
// TODO(https://crbug.com/828229): Fuchsia SDK exports an incorrect
// new[] that gets picked up in Debug/component builds, breaking this
// test.  Disabled on Mac for the same reason.  Disabled under XSan
// because asan aborts when new returns nullptr,
// https://bugs.chromium.org/p/chromium/issues/detail?id=690271#c15
TEST(SecurityTest, MAYBE_NewOverflow) {
  const size_t kArraySize = 4096;
  // We want something "dynamic" here, so that the compiler doesn't
  // immediately reject crazy arrays.
  const size_t kDynamicArraySize = HideValueFromCompiler(kArraySize);
  const size_t kMaxSizeT = std::numeric_limits<size_t>::max();
  const size_t kArraySize2 = kMaxSizeT / kArraySize + 10;
  const size_t kDynamicArraySize2 = HideValueFromCompiler(kArraySize2);
  {
    std::unique_ptr<char[][kArraySize]> array_pointer(
        new (nothrow) char[kDynamicArraySize2][kArraySize]);
    // Prevent clang from optimizing away the whole test.
    char* volatile p = reinterpret_cast<char*>(array_pointer.get());
    OverflowTestsSoftExpectTrue(!p);
  }
  // On windows, the compiler prevents static array sizes of more than
  // 0x7fffffff (error C2148).
#if defined(OS_WIN) && defined(ARCH_CPU_64_BITS)
  ALLOW_UNUSED_LOCAL(kDynamicArraySize);
#else
  {
    std::unique_ptr<char[][kArraySize2]> array_pointer(
        new (nothrow) char[kDynamicArraySize][kArraySize2]);
    // Prevent clang from optimizing away the whole test.
    char* volatile p = reinterpret_cast<char*>(array_pointer.get());
    OverflowTestsSoftExpectTrue(!p);
  }
#endif  // !defined(OS_WIN) || !defined(ARCH_CPU_64_BITS)
}

#if defined(OS_LINUX) && defined(__x86_64__)
// Check if ptr1 and ptr2 are separated by less than size chars.
bool ArePointersToSameArea(void* ptr1, void* ptr2, size_t size) {
  ptrdiff_t ptr_diff = reinterpret_cast<char*>(std::max(ptr1, ptr2)) -
                       reinterpret_cast<char*>(std::min(ptr1, ptr2));
  return static_cast<size_t>(ptr_diff) <= size;
}

// Check if TCMalloc uses an underlying random memory allocator.
TEST(SecurityTest, MALLOC_OVERFLOW_TEST(RandomMemoryAllocations)) {
  size_t kPageSize = 4096;  // We support x86_64 only.
  // Check that malloc() returns an address that is neither the kernel's
  // un-hinted mmap area, nor the current brk() area. The first malloc() may
  // not be at a random address because TCMalloc will first exhaust any memory
  // that it has allocated early on, before starting the sophisticated
  // allocators.
  void* default_mmap_heap_address =
      mmap(nullptr, kPageSize, PROT_READ | PROT_WRITE,
           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  ASSERT_NE(default_mmap_heap_address,
            static_cast<void*>(MAP_FAILED));
  ASSERT_EQ(munmap(default_mmap_heap_address, kPageSize), 0);
  void* brk_heap_address = sbrk(0);
  ASSERT_NE(brk_heap_address, reinterpret_cast<void*>(-1));
  ASSERT_TRUE(brk_heap_address != nullptr);
  // 1 MB should get us past what TCMalloc pre-allocated before initializing
  // the sophisticated allocators.
  size_t kAllocSize = 1<<20;
  std::unique_ptr<char, base::FreeDeleter> ptr(
      static_cast<char*>(malloc(kAllocSize)));
  ASSERT_TRUE(ptr != nullptr);
  // If two pointers are separated by less than 512MB, they are considered
  // to be in the same area.
  // Our random pointer could be anywhere within 0x3fffffffffff (46bits),
  // and we are checking that it's not withing 1GB (30 bits) from two
  // addresses (brk and mmap heap). We have roughly one chance out of
  // 2^15 to flake.
  const size_t kAreaRadius = 1<<29;
  bool in_default_mmap_heap = ArePointersToSameArea(
      ptr.get(), default_mmap_heap_address, kAreaRadius);
  EXPECT_FALSE(in_default_mmap_heap);

  bool in_default_brk_heap = ArePointersToSameArea(
      ptr.get(), brk_heap_address, kAreaRadius);
  EXPECT_FALSE(in_default_brk_heap);

  // In the implementation, we always mask our random addresses with
  // kRandomMask, so we use it as an additional detection mechanism.
  const uintptr_t kRandomMask = 0x3fffffffffffULL;
  bool impossible_random_address =
      reinterpret_cast<uintptr_t>(ptr.get()) & ~kRandomMask;
  EXPECT_FALSE(impossible_random_address);
}

#endif  // defined(OS_LINUX) && defined(__x86_64__)

}  // namespace
