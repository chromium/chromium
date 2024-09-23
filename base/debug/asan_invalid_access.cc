// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "base/debug/asan_invalid_access.h"

#include <stddef.h>

#include <memory>

#include "base/check.h"
#include "base/debug/alias.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

namespace base {
namespace debug {

namespace {

#if BUILDFLAG(IS_WIN) && defined(ADDRESS_SANITIZER)
// Corrupt a memory block and make sure that the corruption gets detected either
// when we free it or when another crash happens (if |induce_crash| is set to
// true).
NOINLINE void CorruptMemoryBlock(bool induce_crash) {
  // NOTE(sebmarchand): We intentionally corrupt a memory block here in order to
  //     trigger an Address Sanitizer (ASAN) error report.
  static const int kArraySize = 5;
  LONG* array = new LONG[kArraySize];

  // Explicitly call out to a kernel32 function to perform the memory access.
  // This way the underflow won't be detected but the corruption will (as the
  // allocator will still be hooked).
  auto InterlockedIncrementFn =
      reinterpret_cast<LONG (*)(LONG volatile * addend)>(
          GetProcAddress(GetModuleHandle(L"kernel32"), "InterlockedIncrement"));
  CHECK(InterlockedIncrementFn);

  LONG volatile dummy = InterlockedIncrementFn(array - 1);
  base::debug::Alias(const_cast<LONG*>(&dummy));

  if (induce_crash)
    CHECK(false);
  delete[] array;
}
#endif  // BUILDFLAG(IS_WIN) && defined(ADDRESS_SANITIZER)

}  // namespace

#if defined(ADDRESS_SANITIZER) || BUILDFLAG(IS_HWASAN)
// NOTE(sebmarchand): We intentionally perform some invalid heap access here in
//     order to trigger an AddressSanitizer (ASan) error report.

// This variable is used to size an array of ints. It needs to be a multiple of
// 4 so that off-by-one overflows are detected by HWASan, which has a shadow
// granularity of 16 bytes.
static const size_t kArraySize = 4;

void AsanHeapOverflow() {
  // Declares the array as volatile to make sure it doesn't get optimized away.
  std::unique_ptr<volatile int[]> array(
      const_cast<volatile int*>(new int[kArraySize]));
  int dummy = array[kArraySize];
  base::debug::Alias(&dummy);
}

void AsanHeapUnderflow() {
  // Declares the array as volatile to make sure it doesn't get optimized away.
  std::unique_ptr<volatile int[]> array(
      const_cast<volatile int*>(new int[kArraySize]));
  // We need to store the underflow address in a temporary variable as trying to
  // access array[-1] will trigger a warning C4245: "conversion from 'int' to
  // 'size_t', signed/unsigned mismatch".
  volatile int* underflow_address = &array[0] - 1;
  int dummy = *underflow_address;
  base::debug::Alias(&dummy);
}

void AsanHeapUseAfterFree() {
  // Declares the array as volatile to make sure it doesn't get optimized away.
  std::unique_ptr<volatile int[]> array(
      const_cast<volatile int*>(new int[kArraySize]));
  volatile int* dangling = array.get();
  array.reset();
  int dummy = dangling[kArraySize / 2];
  base::debug::Alias(&dummy);
}

#if BUILDFLAG(IS_WIN)
void AsanCorruptHeapBlock() {
  CorruptMemoryBlock(false);
}

void AsanCorruptHeap() {
  CorruptMemoryBlock(true);
}
#endif  // BUILDFLAG(IS_WIN)
#endif  // ADDRESS_SANITIZER

}  // namespace debug
}  // namespace base
