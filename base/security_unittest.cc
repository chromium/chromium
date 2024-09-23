// Copyright 2013 The Chromium Authors
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

#include "base/files/file_util.h"
#include "base/memory/free_deleter.h"
#include "base/sanitizer_buildflags.h"
#include "build/build_config.h"
#include "partition_alloc/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_POSIX)
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
NOINLINE Type HideValueFromCompiler(Type value) {
#if defined(__GNUC__)
  // In a GCC compatible compiler (GCC or Clang), make this compiler barrier
  // more robust.
  __asm__ volatile ("" : "+r" (value));
#endif  // __GNUC__
  return value;
}

// There are platforms where these tests are known to fail. We would like to
// be able to easily check the status on the bots, but marking tests as
// FAILS_ is too clunky.
void OverflowTestsSoftExpectTrue(bool overflow_detected) {
  if (!overflow_detected) {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID) || \
    BUILDFLAG(IS_APPLE)
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

#if BUILDFLAG(IS_APPLE) || defined(ADDRESS_SANITIZER) ||      \
    defined(THREAD_SANITIZER) || defined(MEMORY_SANITIZER) || \
    BUILDFLAG(IS_HWASAN) || PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
#define MAYBE_NewOverflow DISABLED_NewOverflow
#else
#define MAYBE_NewOverflow NewOverflow
#endif
// Test that array[TooBig][X] and array[X][TooBig] allocations fail and not
// succeed with the wrong size allocation in case of size_t overflow.  This
// test is disabled on environments that operator new (nothrow) crashes in
// case of size_t overflow.
//
// - iOS doesn't honor nothrow.
// - XSan aborts when operator new returns nullptr.
// - PartitionAlloc crashes by design when size_t overflows.
//
// TODO(crbug.com/40611888): Fix the test on Mac.
TEST(SecurityTest, MAYBE_NewOverflow) {
  const size_t kArraySize = 4096;
  // We want something "dynamic" here, so that the compiler doesn't
  // immediately reject crazy arrays.
  [[maybe_unused]] const size_t kDynamicArraySize =
      HideValueFromCompiler(kArraySize);
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
#if BUILDFLAG(IS_WIN) && defined(ARCH_CPU_64_BITS)
  // On Windows, the compiler prevents static array sizes of more than
  // 0x7fffffff (error C2148).
#else
  {
    std::unique_ptr<char[][kArraySize2]> array_pointer(
        new (nothrow) char[kDynamicArraySize][kArraySize2]);
    // Prevent clang from optimizing away the whole test.
    char* volatile p = reinterpret_cast<char*>(array_pointer.get());
    OverflowTestsSoftExpectTrue(!p);
  }
#endif  // BUILDFLAG(IS_WIN) && defined(ARCH_CPU_64_BITS)
}

}  // namespace
