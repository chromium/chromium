// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains intentional memory errors, some of which may lead to
// crashes if the test is ran without special memory testing tools. We use these
// errors to verify the sanity of the tools.

#include <stddef.h>

#include "base/atomicops.h"
#include "base/cfi_buildflags.h"
#include "base/debug/asan_invalid_access.h"
#include "base/debug/profiler.h"
#include "base/logging.h"
#include "base/sanitizer_buildflags.h"
#include "base/third_party/dynamic_annotations/dynamic_annotations.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

const base::subtle::Atomic32 kMagicValue = 42;

// Helper for memory accesses that can potentially corrupt memory or cause a
// crash during a native run.
#if defined(ADDRESS_SANITIZER)
#define HARMFUL_ACCESS(action, error_regexp) \
  EXPECT_DEATH_IF_SUPPORTED(action, error_regexp)
#elif BUILDFLAG(IS_HWASAN)
#define HARMFUL_ACCESS(action, error_regexp) \
  EXPECT_DEATH(action, "tag-mismatch")
#else
#define HARMFUL_ACCESS(action, error_regexp)
#define HARMFUL_ACCESS_IS_NOOP
#endif

void DoReadUninitializedValue(char *ptr) {
  // Comparison with 64 is to prevent clang from optimizing away the
  // jump -- valgrind only catches jumps and conditional moves, but clang uses
  // the borrow flag if the condition is just `*ptr == '\0'`.  We no longer
  // support valgrind, but this constant should be fine to keep as-is.
  if (*ptr == 64) {
    VLOG(1) << "Uninit condition is true";
  } else {
    VLOG(1) << "Uninit condition is false";
  }
}

void ReadUninitializedValue(char *ptr) {
#if defined(MEMORY_SANITIZER)
  EXPECT_DEATH(DoReadUninitializedValue(ptr),
               "use-of-uninitialized-value");
#else
  DoReadUninitializedValue(ptr);
#endif
}

#ifndef HARMFUL_ACCESS_IS_NOOP
void ReadValueOutOfArrayBoundsLeft(char *ptr) {
  char c = ptr[-2];
  VLOG(1) << "Reading a byte out of bounds: " << c;
}

void ReadValueOutOfArrayBoundsRight(char *ptr, size_t size) {
  char c = ptr[size + 1];
  VLOG(1) << "Reading a byte out of bounds: " << c;
}

void WriteValueOutOfArrayBoundsLeft(char *ptr) {
  ptr[-1] = kMagicValue;
}

void WriteValueOutOfArrayBoundsRight(char *ptr, size_t size) {
  ptr[size] = kMagicValue;
}
#endif  // HARMFUL_ACCESS_IS_NOOP

void MakeSomeErrors(char *ptr, size_t size) {
  ReadUninitializedValue(ptr);

  HARMFUL_ACCESS(ReadValueOutOfArrayBoundsLeft(ptr),
                 "2 bytes to the left");
  HARMFUL_ACCESS(ReadValueOutOfArrayBoundsRight(ptr, size),
                 "1 bytes to the right");
  HARMFUL_ACCESS(WriteValueOutOfArrayBoundsLeft(ptr),
                 "1 bytes to the left");
  HARMFUL_ACCESS(WriteValueOutOfArrayBoundsRight(ptr, size),
                 "0 bytes to the right");
}

}  // namespace

// A memory leak detector should report an error in this test.
TEST(ToolsSanityTest, MemoryLeak) {
  // Without the |volatile|, clang optimizes away the next two lines.
  int* volatile leak = new int[256];  // Leak some memory intentionally.
  leak[4] = 1;  // Make sure the allocated memory is used.
}

// The following tests pass with Clang r170392, but not r172454, which
// makes AddressSanitizer detect errors in them. We disable these tests under
// AddressSanitizer until we fully switch to Clang r172454. After that the
// tests should be put back under the (defined(OS_IOS) || defined(OS_WIN))
// clause above.
// See also http://crbug.com/172614.
#if defined(ADDRESS_SANITIZER)
#define MAYBE_SingleElementDeletedWithBraces \
    DISABLED_SingleElementDeletedWithBraces
#define MAYBE_ArrayDeletedWithoutBraces DISABLED_ArrayDeletedWithoutBraces
#else
#define MAYBE_ArrayDeletedWithoutBraces ArrayDeletedWithoutBraces
#define MAYBE_SingleElementDeletedWithBraces SingleElementDeletedWithBraces
#endif  // defined(ADDRESS_SANITIZER)

TEST(ToolsSanityTest, AccessesToNewMemory) {
  char* foo = new char[16];
  MakeSomeErrors(foo, 16);
  delete [] foo;
  // Use after delete.
  HARMFUL_ACCESS(foo[5] = 0, "heap-use-after-free");
}

TEST(ToolsSanityTest, AccessesToMallocMemory) {
  char* foo = reinterpret_cast<char*>(malloc(16));
  MakeSomeErrors(foo, 16);
  free(foo);
  // Use after free.
  HARMFUL_ACCESS(foo[5] = 0, "heap-use-after-free");
}

TEST(ToolsSanityTest, AccessesToStack) {
  char foo[16];

  ReadUninitializedValue(foo);
  HARMFUL_ACCESS(ReadValueOutOfArrayBoundsLeft(foo),
                 "underflows this variable");
  HARMFUL_ACCESS(ReadValueOutOfArrayBoundsRight(foo, 16),
                 "overflows this variable");
  HARMFUL_ACCESS(WriteValueOutOfArrayBoundsLeft(foo),
                 "underflows this variable");
  HARMFUL_ACCESS(WriteValueOutOfArrayBoundsRight(foo, 16),
                 "overflows this variable");
}

#if defined(ADDRESS_SANITIZER)

static int* allocateArray() {
  // Clang warns about the mismatched new[]/delete if they occur in the same
  // function.
  return new int[10];
}

// This test may corrupt memory if not compiled with AddressSanitizer.
TEST(ToolsSanityTest, MAYBE_ArrayDeletedWithoutBraces) {
  // Without the |volatile|, clang optimizes away the next two lines.
  int* volatile foo = allocateArray();
  delete foo;
}
#endif

#if defined(ADDRESS_SANITIZER)
static int* allocateScalar() {
  // Clang warns about the mismatched new/delete[] if they occur in the same
  // function.
  return new int;
}

// This test may corrupt memory if not compiled with AddressSanitizer.
TEST(ToolsSanityTest, MAYBE_SingleElementDeletedWithBraces) {
  // Without the |volatile|, clang optimizes away the next two lines.
  int* volatile foo = allocateScalar();
  (void) foo;
  delete [] foo;
}
#endif

TEST(ToolsSanityTest, DISABLED_AddressSanitizerNullDerefCrashTest) {
  // Intentionally crash to make sure AddressSanitizer is running.
  // This test should not be ran on bots.
  int* volatile zero = NULL;
  *zero = 0;
}

TEST(ToolsSanityTest, DISABLED_AddressSanitizerLocalOOBCrashTest) {
  // Intentionally crash to make sure AddressSanitizer is instrumenting
  // the local variables.
  // This test should not be ran on bots.
  int array[5];
  // Work around the OOB warning reported by Clang.
  int* volatile access = &array[5];
  *access = 43;
}

namespace {
int g_asan_test_global_array[10];
}  // namespace

TEST(ToolsSanityTest, DISABLED_AddressSanitizerGlobalOOBCrashTest) {
  // Intentionally crash to make sure AddressSanitizer is instrumenting
  // the global variables.
  // This test should not be ran on bots.

  // Work around the OOB warning reported by Clang.
  int* volatile access = g_asan_test_global_array - 1;
  *access = 43;
}

#ifndef HARMFUL_ACCESS_IS_NOOP
TEST(ToolsSanityTest, AsanHeapOverflow) {
  HARMFUL_ACCESS(debug::AsanHeapOverflow() ,"to the right");
}

TEST(ToolsSanityTest, AsanHeapUnderflow) {
  HARMFUL_ACCESS(debug::AsanHeapUnderflow(), "to the left");
}

TEST(ToolsSanityTest, AsanHeapUseAfterFree) {
  HARMFUL_ACCESS(debug::AsanHeapUseAfterFree(), "heap-use-after-free");
}

#if defined(OS_WIN)
// The ASAN runtime doesn't detect heap corruption, this needs fixing before
// ASAN builds can ship to the wild. See https://crbug.com/818747.
TEST(ToolsSanityTest, DISABLED_AsanCorruptHeapBlock) {
  HARMFUL_ACCESS(debug::AsanCorruptHeapBlock(), "");
}

TEST(ToolsSanityTest, DISABLED_AsanCorruptHeap) {
  // This test will kill the process by raising an exception, there's no
  // particular string to look for in the stack trace.
  EXPECT_DEATH(debug::AsanCorruptHeap(), "");
}
#endif  // OS_WIN
#endif  // !HARMFUL_ACCESS_IS_NOOP

namespace {

// We use caps here just to ensure that the method name doesn't interfere with
// the wildcarded suppressions.
class TOOLS_SANITY_TEST_CONCURRENT_THREAD : public PlatformThread::Delegate {
 public:
  explicit TOOLS_SANITY_TEST_CONCURRENT_THREAD(bool *value) : value_(value) {}
  ~TOOLS_SANITY_TEST_CONCURRENT_THREAD() override = default;
  void ThreadMain() override {
    *value_ = true;

    // Sleep for a few milliseconds so the two threads are more likely to live
    // simultaneously. Otherwise we may miss the report due to mutex
    // lock/unlock's inside thread creation code in pure-happens-before mode...
    PlatformThread::Sleep(TimeDelta::FromMilliseconds(100));
  }
 private:
  bool *value_;
};

class ReleaseStoreThread : public PlatformThread::Delegate {
 public:
  explicit ReleaseStoreThread(base::subtle::Atomic32 *value) : value_(value) {}
  ~ReleaseStoreThread() override = default;
  void ThreadMain() override {
    base::subtle::Release_Store(value_, kMagicValue);

    // Sleep for a few milliseconds so the two threads are more likely to live
    // simultaneously. Otherwise we may miss the report due to mutex
    // lock/unlock's inside thread creation code in pure-happens-before mode...
    PlatformThread::Sleep(TimeDelta::FromMilliseconds(100));
  }
 private:
  base::subtle::Atomic32 *value_;
};

class AcquireLoadThread : public PlatformThread::Delegate {
 public:
  explicit AcquireLoadThread(base::subtle::Atomic32 *value) : value_(value) {}
  ~AcquireLoadThread() override = default;
  void ThreadMain() override {
    // Wait for the other thread to make Release_Store
    PlatformThread::Sleep(TimeDelta::FromMilliseconds(100));
    base::subtle::Acquire_Load(value_);
  }
 private:
  base::subtle::Atomic32 *value_;
};

void RunInParallel(PlatformThread::Delegate *d1, PlatformThread::Delegate *d2) {
  PlatformThreadHandle a;
  PlatformThreadHandle b;
  PlatformThread::Create(0, d1, &a);
  PlatformThread::Create(0, d2, &b);
  PlatformThread::Join(a);
  PlatformThread::Join(b);
}

#if defined(THREAD_SANITIZER)
void DataRace() {
  bool *shared = new bool(false);
  TOOLS_SANITY_TEST_CONCURRENT_THREAD thread1(shared), thread2(shared);
  RunInParallel(&thread1, &thread2);
  EXPECT_TRUE(*shared);
  delete shared;
  // We're in a death test - crash.
  CHECK(0);
}
#endif

}  // namespace

#if defined(THREAD_SANITIZER)
// A data race detector should report an error in this test.
TEST(ToolsSanityTest, DataRace) {
  // The suppression regexp must match that in base/debug/tsan_suppressions.cc.
  EXPECT_DEATH(DataRace(), "1 race:base/tools_sanity_unittest.cc");
}
#endif

TEST(ToolsSanityTest, AnnotateBenignRace) {
  bool shared = false;
  ANNOTATE_BENIGN_RACE(&shared, "Intentional race - make sure doesn't show up");
  TOOLS_SANITY_TEST_CONCURRENT_THREAD thread1(&shared), thread2(&shared);
  RunInParallel(&thread1, &thread2);
  EXPECT_TRUE(shared);
}

TEST(ToolsSanityTest, AtomicsAreIgnored) {
  base::subtle::Atomic32 shared = 0;
  ReleaseStoreThread thread1(&shared);
  AcquireLoadThread thread2(&shared);
  RunInParallel(&thread1, &thread2);
  EXPECT_EQ(kMagicValue, shared);
}

#if BUILDFLAG(CFI_ENFORCEMENT_TRAP)
#if defined(OS_WIN)
#define CFI_ERROR_MSG "EXCEPTION_ILLEGAL_INSTRUCTION"
#elif defined(OS_ANDROID)
// TODO(pcc): Produce proper stack dumps on Android and test for the correct
// si_code here.
#define CFI_ERROR_MSG "^$"
#else
#define CFI_ERROR_MSG "ILL_ILLOPN"
#endif
#elif BUILDFLAG(CFI_ENFORCEMENT_DIAGNOSTIC)
#define CFI_ERROR_MSG "runtime error: control flow integrity check"
#endif  // BUILDFLAG(CFI_ENFORCEMENT_TRAP || CFI_ENFORCEMENT_DIAGNOSTIC)

#if defined(CFI_ERROR_MSG)
class A {
 public:
  A(): n_(0) {}
  virtual void f() { n_++; }
 protected:
  int n_;
};

class B: public A {
 public:
  void f() override { n_--; }
};

class C: public B {
 public:
  void f() override { n_ += 2; }
};

NOINLINE void KillVptrAndCall(A *obj) {
  *reinterpret_cast<void **>(obj) = 0;
  obj->f();
}

TEST(ToolsSanityTest, BadVirtualCallNull) {
  A a;
  B b;
  EXPECT_DEATH({ KillVptrAndCall(&a); KillVptrAndCall(&b); }, CFI_ERROR_MSG);
}

NOINLINE void OverwriteVptrAndCall(B *obj, A *vptr) {
  *reinterpret_cast<void **>(obj) = *reinterpret_cast<void **>(vptr);
  obj->f();
}

TEST(ToolsSanityTest, BadVirtualCallWrongType) {
  A a;
  B b;
  C c;
  EXPECT_DEATH({ OverwriteVptrAndCall(&b, &a); OverwriteVptrAndCall(&b, &c); },
               CFI_ERROR_MSG);
}

// TODO(pcc): remove CFI_CAST_CHECK, see https://crbug.com/626794.
#if BUILDFLAG(CFI_CAST_CHECK)
TEST(ToolsSanityTest, BadDerivedCast) {
  A a;
  EXPECT_DEATH((void)(B*)&a, CFI_ERROR_MSG);
}

TEST(ToolsSanityTest, BadUnrelatedCast) {
  class A {
    virtual void f() {}
  };

  class B {
    virtual void f() {}
  };

  A a;
  EXPECT_DEATH((void)(B*)&a, CFI_ERROR_MSG);
}
#endif  // BUILDFLAG(CFI_CAST_CHECK)

#endif  // CFI_ERROR_MSG

#undef CFI_ERROR_MSG
#undef MAYBE_AccessesToNewMemory
#undef MAYBE_AccessesToMallocMemory
#undef MAYBE_ArrayDeletedWithoutBraces
#undef MAYBE_SingleElementDeletedWithBraces
#undef HARMFUL_ACCESS
#undef HARMFUL_ACCESS_IS_NOOP

}  // namespace base
