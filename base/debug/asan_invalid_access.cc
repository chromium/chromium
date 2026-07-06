// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/asan_invalid_access.h"

#include <stddef.h>

#include "base/check.h"
#include "base/containers/heap_array.h"
#include "base/debug/alias.h"
#include "base/immediate_crash.h"
#include "base/logging.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

namespace base::debug {

namespace {

#if BUILDFLAG(IS_WIN) && defined(ADDRESS_SANITIZER)
// Corrupt a memory block and make sure that the corruption gets detected either
// when we free it or when another crash happens (if |induce_crash| is set to
// true).
NOINLINE void CorruptMemoryBlock(bool induce_crash) {
  // NOTE(sebmarchand): We intentionally corrupt a memory block here in order to
  //     trigger an Address Sanitizer (ASAN) error report.
  static const size_t kArraySize = 5;
  auto array = base::HeapArray<LONG>::Uninit(kArraySize);

  // Explicitly call out to a kernel32 function to perform the memory access.
  // This way the underflow won't be detected but the corruption will (as the
  // allocator will still be hooked).
  auto InterlockedIncrementFn =
      reinterpret_cast<LONG (*)(LONG volatile* addend)>(
          GetProcAddress(GetModuleHandle(L"kernel32"), "InterlockedIncrement"));
  CHECK(InterlockedIncrementFn);

  LONG volatile dummy = InterlockedIncrementFn(UNSAFE_TODO(array.data() - 1));
  base::debug::Alias(const_cast<LONG*>(&dummy));

  if (induce_crash) {
    base::ImmediateCrash();
  }
}
#endif  // BUILDFLAG(IS_WIN) && defined(ADDRESS_SANITIZER)

void MaybeImmediateCrash() {
  // On non-ASan builds, the invalid memory access above is not guaranteed
  // to crash and may leave the heap memory in corrupted state.
  // To prevent potential exploitation, ensure the process crashes.
#if !defined(ADDRESS_SANITIZER) && !BUILDFLAG(IS_HWASAN)
  base::ImmediateCrash();
#endif
}

}  // namespace

// NOTE(sebmarchand): We intentionally perform some invalid heap access here in
//     order to trigger an AddressSanitizer (ASan) error report.

// This variable is used to size an array of ints. It needs to be a multiple of
// 4 so that off-by-one overflows are detected by HWASan, which has a shadow
// granularity of 16 bytes.
static const size_t kArraySize = 4;

void AsanHeapOverflow() {
  // Don't fold so that crash reports will clearly show this method.
  NO_CODE_FOLDING();

  // Declares the array as volatile to make sure it doesn't get optimized away.
  auto array = base::HeapArray<volatile int>::Uninit(kArraySize);
  // SAFETY: required for test.
  int dummy = UNSAFE_BUFFERS(array.data()[kArraySize]);
  base::debug::Alias(&dummy);

  MaybeImmediateCrash();
}

void AsanHeapUnderflow() {
  // Don't fold so that crash reports will clearly show this method.
  NO_CODE_FOLDING();

  // Declares the array as volatile to make sure it doesn't get optimized away.
  auto array = base::HeapArray<volatile int>::Uninit(kArraySize);
  // We need to store the underflow address in a temporary variable as trying to
  // access array[-1] will trigger a warning C4245: "conversion from 'int' to
  // 'size_t', signed/unsigned mismatch".
  volatile int* underflow_address = UNSAFE_TODO(&array[0] - 1);
  int dummy = *underflow_address;
  base::debug::Alias(&dummy);

  MaybeImmediateCrash();
}

void AsanHeapUseAfterFree() {
  // Don't fold so that crash reports will clearly show this method.
  NO_CODE_FOLDING();

  // Declares the array as volatile to make sure it doesn't get optimized away.
  auto array = base::HeapArray<volatile int>::Uninit(kArraySize);
  volatile int* dangling = array.data();
  array = base::HeapArray<volatile int>();
  int dummy = UNSAFE_TODO(dangling[kArraySize / 2]);
  base::debug::Alias(&dummy);

  MaybeImmediateCrash();
}

void AsanHeapMemberDereferenceAfterFree() {
  // Don't fold so that crash reports will clearly show this method.
  NO_CODE_FOLDING();

  int on_stack_variable = 123;
  // Allocate a pointer on the heap that points to a stack variable.
  auto array = base::HeapArray<int*>::Uninit(kArraySize);
  array[0] = &on_stack_variable;

  CHECK(*array[0] == 123);

  // Cast to a pointer to a volatile pointer. This forces the compiler to
  // perform a volatile read from this memory location when we dereference it
  // later, preventing the compiler from optimizing out the read after the
  // memory is freed.
  int* volatile* dangling = reinterpret_cast<int* volatile*>(array.data());

  // Free the heap memory.
  array = base::HeapArray<int*>();

  // Perform a volatile read from the freed memory (Use-After-Free).
  // This reads the address of `on_stack_variable` back from the freed heap
  // allocation.
  volatile int* bypassed_ptr = *dangling;

  LOG(ERROR) << "Invalid Access Debug: dereferencing a pointer to "
             << const_cast<int*>(bypassed_ptr);

  // Dereference the pointer we just read to force the access.
  int val = *bypassed_ptr;

  // Ensure the read value is not optimized away.
  base::debug::Alias(&val);

  MaybeImmediateCrash();
}

#if defined(ADDRESS_SANITIZER) || BUILDFLAG(IS_HWASAN)
// The "corrupt-block" and "corrupt-heap" classes of bugs is specific to
// Windows.
#if BUILDFLAG(IS_WIN)
void AsanCorruptHeapBlock() {
  CorruptMemoryBlock(false);
}

void AsanCorruptHeap() {
  CorruptMemoryBlock(true);
}
#endif  // BUILDFLAG(IS_WIN)
#endif  // ADDRESS_SANITIZER

}  // namespace base::debug
