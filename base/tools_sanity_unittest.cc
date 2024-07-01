// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

// This file contains intentional memory errors, some of which may lead to
// crashes if the test is ran without special memory testing tools. We use these
// errors to verify the sanity of the tools.

#include <stddef.h>

#include "base/atomicops.h"
#include "base/cfi_buildflags.h"
#include "base/debug/asan_invalid_access.h"
#include "base/debug/profiler.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/sanitizer_buildflags.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/base/dynamic_annotations.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

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

void DoReadUninitializedValue(volatile char *ptr) {
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

void ReadUninitializedValue(volatile char *ptr) {
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

  HARMFUL_ACCESS(ReadValueOutOfArrayBoundsLeft(ptr), "2 bytes before");
  HARMFUL_ACCESS(ReadValueOutOfArrayBoundsRight(ptr, size), "1 bytes after");
  HARMFUL_ACCESS(WriteValueOutOfArrayBoundsLeft(ptr), "1 bytes before");
  HARMFUL_ACCESS(WriteValueOutOfArrayBoundsRight(ptr, size), "0 bytes after");
}

}  // namespace

#if defined(ADDRESS_SANITIZER) || defined(LEAK_SANITIZER) ||  \
    defined(MEMORY_SANITIZER) || defined(THREAD_SANITIZER) || \
    defined(UNDEFINED_SANITIZER)
// build/sanitizers/sanitizer_options.cc defines symbols like
// __asan_default_options which the sanitizer runtime calls if they exist
// in the executable. If they don't, the sanitizer runtime silently uses an
// internal default value instead. The build puts the symbol
// _sanitizer_options_link_helper (which the sanitizer runtime doesn't know
// about, it's a chrome thing) in that file and then tells the linker that
// that symbol must exist. This causes sanitizer_options.cc to be part of
// our binaries, which in turn makes sure our __asan_default_options are used.
// We had problems with __asan_default_options not being used, so this test
// verifies that _sanitizer_options_link_helper actually makes it into our
// binaries.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_WIN)
// TODO(crbug.com/40224191): Sanitizer options are currently broken
// on Android.
// TODO(crbug.com/40223949): __asan_default_options should be used
// on Windows too, but currently isn't.
#define MAYBE_LinksSanitizerOptions DISABLED_LinksSanitizerOptions
#else
#define MAYBE_LinksSanitizerOptions LinksSanitizerOptions
#endif
TEST(ToolsSanityTest, MAYBE_LinksSanitizerOptions) {
  constexpr char kSym[] = "_sanitizer_options_link_helper";
#if BUILDFLAG(IS_WIN)
  auto sym = GetProcAddress(GetModuleHandle(nullptr), kSym);
#else
  void* sym = dlsym(RTLD_DEFAULT, kSym);
#endif
  EXPECT_TRUE(sym != nullptr);
}
#endif  // sanitizers

// A memory leak detector should report an error in this test.
TEST(ToolsSanityTest, MemoryLeak) {
  // Without the |volatile|, clang optimizes away the next two lines.
  int* volatile leak = new int[256];  // Leak some memory intentionally.
  leak[4] = 1;  // Make sure the allocated memory is used.
}

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

// alloc_dealloc_mismatch defaults to
// !SANITIZER_MAC && !SANITIZER_WINDOWS && !SANITIZER_ANDROID,
// in the sanitizer runtime upstream.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || \
    BUILDFLAG(IS_FUCHSIA)
#define MAYBE_SingleElementDeletedWithBraces \
    DISABLED_SingleElementDeletedWithBraces
#define MAYBE_ArrayDeletedWithoutBraces DISABLED_ArrayDeletedWithoutBraces
#else
#define MAYBE_ArrayDeletedWithoutBraces ArrayDeletedWithoutBraces
#define MAYBE_SingleElementDeletedWithBraces SingleElementDeletedWithBraces
#endif  // defined(ADDRESS_SANITIZER)

static int* allocateArray() {
  // Clang warns about the mismatched new[]/delete if they occur in the same
  // function.
  return new int[10];
}

// This test may corrupt memory if not compiled with AddressSanitizer.
TEST(ToolsSanityTest, MAYBE_ArrayDeletedWithoutBraces) {
  // Without the |volatile|, clang optimizes away the next two lines.
  int* volatile foo = allocateArray();
  HARMFUL_ACCESS(delete foo, "alloc-dealloc-mismatch");
  // Under ASan the crash happens in the process spawned by HARMFUL_ACCESS,
  // need to free the memory in the parent.
  delete [] foo;
}

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
  HARMFUL_ACCESS(delete [] foo, "alloc-dealloc-mismatch");
  // Under ASan the crash happens in the process spawned by HARMFUL_ACCESS,
  // need to free the memory in the parent.
  delete foo;
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
  HARMFUL_ACCESS(debug::AsanHeapOverflow(), "after");
}

TEST(ToolsSanityTest, AsanHeapUnderflow) {
  HARMFUL_ACCESS(debug::AsanHeapUnderflow(), "before");
}

TEST(ToolsSanityTest, AsanHeapUseAfterFree) {
  HARMFUL_ACCESS(debug::AsanHeapUseAfterFree(), "heap-use-after-free");
}

#if BUILDFLAG(IS_WIN)
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
#endif  // BUILDFLAG(IS_WIN)
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
    PlatformThread::Sleep(Milliseconds(100));
  }
 private:
  raw_ptr<bool> value_;
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
    PlatformThread::Sleep(Milliseconds(100));
  }
 private:
  raw_ptr<base::subtle::Atomic32> value_;
};

class AcquireLoadThread : public PlatformThread::Delegate {
 public:
  explicit AcquireLoadThread(base::subtle::Atomic32 *value) : value_(value) {}
  ~AcquireLoadThread() override = default;
  void ThreadMain() override {
    // Wait for the other thread to make Release_Store
    PlatformThread::Sleep(Milliseconds(100));
    base::subtle::Acquire_Load(value_);
  }
 private:
  raw_ptr<base::subtle::Atomic32> value_;
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
  ABSL_ANNOTATE_BENIGN_RACE(
      &shared, "Intentional race - make sure doesn't show up");
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
#if BUILDFLAG(IS_WIN)
#define CFI_ERROR_MSG "EXCEPTION_ILLEGAL_INSTRUCTION"
#elif BUILDFLAG(IS_ANDROID)
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
#undef HARMFUL_ACCESS
#undef HARMFUL_ACCESS_IS_NOOP

}  // namespace base
