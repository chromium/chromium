// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#define _CRT_SECURE_NO_WARNINGS

#include "base/process/memory.h"

#include <stddef.h>

#include <limits>

#include "base/allocator/allocator_check.h"
#include "base/allocator/buildflags.h"
#include "base/compiler_specific.h"
#include "base/debug/alias.h"
#include "base/memory/aligned_memory.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_WIN)
#include <windows.h>
#endif
#if defined(OS_POSIX)
#include <errno.h>
#endif
#if defined(OS_MACOSX)
#include <malloc/malloc.h>
#include "base/allocator/allocator_interception_mac.h"
#include "base/allocator/allocator_shim.h"
#include "base/process/memory_unittest_mac.h"
#endif
#if defined(OS_LINUX)
#include <malloc.h>
#include "base/test/malloc_wrapper.h"
#endif

#if defined(OS_WIN)

#if defined(COMPILER_MSVC)
// ssize_t needed for OutOfMemoryTest.
#if defined(_WIN64)
typedef __int64 ssize_t;
#else
typedef long ssize_t;
#endif
#endif

// HeapQueryInformation function pointer.
typedef BOOL (WINAPI* HeapQueryFn)  \
    (HANDLE, HEAP_INFORMATION_CLASS, PVOID, SIZE_T, PSIZE_T);

#endif  // defined(OS_WIN)

#if defined(OS_MACOSX)

// For the following Mac tests:
// Note that base::EnableTerminationOnHeapCorruption() is called as part of
// test suite setup and does not need to be done again, else mach_override
// will fail.

TEST(ProcessMemoryTest, MacTerminateOnHeapCorruption) {
#if BUILDFLAG(USE_ALLOCATOR_SHIM)
  base::allocator::InitializeAllocatorShim();
#endif
  // Assert that freeing an unallocated pointer will crash the process.
  char buf[9];
  asm("" : "=m"(buf));  // Prevent clang from being too smart.
#if ARCH_CPU_64_BITS
  // On 64 bit Macs, the malloc system automatically abort()s on heap corruption
  // but does not output anything.
  ASSERT_DEATH(free(buf), "");
#elif defined(ADDRESS_SANITIZER)
  // AddressSanitizer replaces malloc() and prints a different error message on
  // heap corruption.
  ASSERT_DEATH(free(buf), "attempting free on address which "
      "was not malloc\\(\\)-ed");
#else
  ADD_FAILURE() << "This test is not supported in this build configuration.";
#endif

#if BUILDFLAG(USE_ALLOCATOR_SHIM)
  base::allocator::UninterceptMallocZonesForTesting();
#endif
}

#endif  // defined(OS_MACOSX)

TEST(MemoryTest, AllocatorShimWorking) {
#if defined(OS_MACOSX)
#if BUILDFLAG(USE_ALLOCATOR_SHIM)
  base::allocator::InitializeAllocatorShim();
#endif
  base::allocator::InterceptAllocationsMac();
#endif
  ASSERT_TRUE(base::allocator::IsAllocatorInitialized());

#if defined(OS_MACOSX)
  base::allocator::UninterceptMallocZonesForTesting();
#endif
}

// OpenBSD does not support these tests. Don't test these on ASan/TSan/MSan
// configurations: only test the real allocator.
// Windows only supports these tests with the allocator shim in place.
#if !defined(OS_OPENBSD) && BUILDFLAG(USE_ALLOCATOR_SHIM) && \
    !defined(MEMORY_TOOL_REPLACES_ALLOCATOR)

namespace {
#if defined(OS_WIN)
// Windows raises an exception rather than using LOG(FATAL) in order to make the
// exit code unique to OOM.
const char* kOomRegex = "";
const int kExitCode = base::win::kOomExceptionCode;
#else
const char* kOomRegex = "Out of memory";
const int kExitCode = 1;
#endif
}  // namespace

class OutOfMemoryTest : public testing::Test {
 public:
  OutOfMemoryTest()
      : value_(nullptr),
        // Make test size as large as possible minus a few pages so
        // that alignment or other rounding doesn't make it wrap.
        test_size_(std::numeric_limits<std::size_t>::max() - 12 * 1024),
        // A test size that is > 2Gb and will cause the allocators to reject
        // the allocation due to security restrictions. See crbug.com/169327.
        insecure_test_size_(std::numeric_limits<int>::max()),
        signed_test_size_(std::numeric_limits<ssize_t>::max()) {}

 protected:
  void* value_;
  size_t test_size_;
  size_t insecure_test_size_;
  ssize_t signed_test_size_;
};

class OutOfMemoryDeathTest : public OutOfMemoryTest {
 public:
  void SetUpInDeathAssert() {
#if defined(OS_MACOSX) && BUILDFLAG(USE_ALLOCATOR_SHIM)
    base::allocator::InitializeAllocatorShim();
#endif

    // Must call EnableTerminationOnOutOfMemory() because that is called from
    // chrome's main function and therefore hasn't been called yet.
    // Since this call may result in another thread being created and death
    // tests shouldn't be started in a multithread environment, this call
    // should be done inside of the ASSERT_DEATH.
    base::EnableTerminationOnOutOfMemory();
  }

#if defined(OS_MACOSX)
  void TearDown() override {
    base::allocator::UninterceptMallocZonesForTesting();
  }
#endif
};

TEST_F(OutOfMemoryDeathTest, New) {
  ASSERT_EXIT({
      SetUpInDeathAssert();
      value_ = operator new(test_size_);
    }, testing::ExitedWithCode(kExitCode), kOomRegex);
}

TEST_F(OutOfMemoryDeathTest, NewArray) {
  ASSERT_EXIT({
      SetUpInDeathAssert();
      value_ = new char[test_size_];
    }, testing::ExitedWithCode(kExitCode), kOomRegex);
}

TEST_F(OutOfMemoryDeathTest, Malloc) {
  ASSERT_EXIT({
      SetUpInDeathAssert();
      value_ = malloc(test_size_);
    }, testing::ExitedWithCode(kExitCode), kOomRegex);
}

TEST_F(OutOfMemoryDeathTest, Realloc) {
  ASSERT_EXIT({
      SetUpInDeathAssert();
      value_ = realloc(nullptr, test_size_);
    }, testing::ExitedWithCode(kExitCode), kOomRegex);
}

TEST_F(OutOfMemoryDeathTest, Calloc) {
  ASSERT_EXIT({
      SetUpInDeathAssert();
      value_ = calloc(1024, test_size_ / 1024L);
    }, testing::ExitedWithCode(kExitCode), kOomRegex);
}

TEST_F(OutOfMemoryDeathTest, AlignedAlloc) {
  ASSERT_EXIT({
      SetUpInDeathAssert();
      value_ = base::AlignedAlloc(test_size_, 8);
    }, testing::ExitedWithCode(kExitCode), kOomRegex);
}

// POSIX does not define an aligned realloc function.
#if defined(OS_WIN)
TEST_F(OutOfMemoryDeathTest, AlignedRealloc) {
  ASSERT_EXIT({
      SetUpInDeathAssert();
      value_ = _aligned_realloc(NULL, test_size_, 8);
    }, testing::ExitedWithCode(kExitCode), kOomRegex);
}

namespace {

constexpr uint32_t kUnhandledExceptionExitCode = 0xBADA55;

// This unhandled exception filter exits the process with an exit code distinct
// from the exception code. This is to verify that the out of memory new handler
// causes an unhandled exception.
LONG WINAPI ExitingUnhandledExceptionFilter(EXCEPTION_POINTERS* ExceptionInfo) {
  _exit(kUnhandledExceptionExitCode);
}

}  // namespace

TEST_F(OutOfMemoryDeathTest, NewHandlerGeneratesUnhandledException) {
  ASSERT_EXIT(
      {
        SetUpInDeathAssert();
        SetUnhandledExceptionFilter(&ExitingUnhandledExceptionFilter);
        value_ = new char[test_size_];
      },
      testing::ExitedWithCode(kUnhandledExceptionExitCode), kOomRegex);
}
#endif  // defined(OS_WIN)

// OS X and Android have no 2Gb allocation limit.
// See https://crbug.com/169327.
#if !defined(OS_MACOSX) && !defined(OS_ANDROID)
TEST_F(OutOfMemoryDeathTest, SecurityNew) {
  ASSERT_EXIT({
      SetUpInDeathAssert();
      value_ = operator new(insecure_test_size_);
    }, testing::ExitedWithCode(kExitCode), kOomRegex);
}

TEST_F(OutOfMemoryDeathTest, SecurityNewArray) {
  ASSERT_EXIT({
      SetUpInDeathAssert();
      value_ = new char[insecure_test_size_];
    }, testing::ExitedWithCode(kExitCode), kOomRegex);
}

TEST_F(OutOfMemoryDeathTest, SecurityMalloc) {
  ASSERT_EXIT({
      SetUpInDeathAssert();
      value_ = malloc(insecure_test_size_);
    }, testing::ExitedWithCode(kExitCode), kOomRegex);
}

TEST_F(OutOfMemoryDeathTest, SecurityRealloc) {
  ASSERT_EXIT({
      SetUpInDeathAssert();
      value_ = realloc(nullptr, insecure_test_size_);
    }, testing::ExitedWithCode(kExitCode), kOomRegex);
}

TEST_F(OutOfMemoryDeathTest, SecurityCalloc) {
  ASSERT_EXIT({
      SetUpInDeathAssert();
      value_ = calloc(1024, insecure_test_size_ / 1024L);
    }, testing::ExitedWithCode(kExitCode), kOomRegex);
}

TEST_F(OutOfMemoryDeathTest, SecurityAlignedAlloc) {
  ASSERT_EXIT({
      SetUpInDeathAssert();
      value_ = base::AlignedAlloc(insecure_test_size_, 8);
    }, testing::ExitedWithCode(kExitCode), kOomRegex);
}

// POSIX does not define an aligned realloc function.
#if defined(OS_WIN)
TEST_F(OutOfMemoryDeathTest, SecurityAlignedRealloc) {
  ASSERT_EXIT({
      SetUpInDeathAssert();
      value_ = _aligned_realloc(NULL, insecure_test_size_, 8);
    }, testing::ExitedWithCode(kExitCode), kOomRegex);
}
#endif  // defined(OS_WIN)
#endif  // !defined(OS_MACOSX) && !defined(OS_ANDROID)

#if defined(OS_LINUX)

TEST_F(OutOfMemoryDeathTest, Valloc) {
  ASSERT_DEATH({
      SetUpInDeathAssert();
      value_ = valloc(test_size_);
    }, kOomRegex);
}

TEST_F(OutOfMemoryDeathTest, SecurityValloc) {
  ASSERT_DEATH({
      SetUpInDeathAssert();
      value_ = valloc(insecure_test_size_);
    }, kOomRegex);
}

TEST_F(OutOfMemoryDeathTest, Pvalloc) {
  ASSERT_DEATH({
      SetUpInDeathAssert();
      value_ = pvalloc(test_size_);
    }, kOomRegex);
}

TEST_F(OutOfMemoryDeathTest, SecurityPvalloc) {
  ASSERT_DEATH({
      SetUpInDeathAssert();
      value_ = pvalloc(insecure_test_size_);
    }, kOomRegex);
}

TEST_F(OutOfMemoryDeathTest, Memalign) {
  ASSERT_DEATH({
      SetUpInDeathAssert();
      value_ = memalign(4, test_size_);
    }, kOomRegex);
}

TEST_F(OutOfMemoryDeathTest, ViaSharedLibraries) {
  // This tests that the run-time symbol resolution is overriding malloc for
  // shared libraries as well as for our code.
  ASSERT_DEATH({
    SetUpInDeathAssert();
    value_ = MallocWrapper(test_size_);
  }, kOomRegex);
}
#endif  // OS_LINUX

// Android doesn't implement posix_memalign().
#if defined(OS_POSIX) && !defined(OS_ANDROID)
TEST_F(OutOfMemoryDeathTest, Posix_memalign) {
  // Grab the return value of posix_memalign to silence a compiler warning
  // about unused return values. We don't actually care about the return
  // value, since we're asserting death.
  ASSERT_DEATH({
      SetUpInDeathAssert();
      EXPECT_EQ(ENOMEM, posix_memalign(&value_, 8, test_size_));
    }, kOomRegex);
}
#endif  // defined(OS_POSIX) && !defined(OS_ANDROID)

#if defined(OS_MACOSX)

// Purgeable zone tests

TEST_F(OutOfMemoryDeathTest, MallocPurgeable) {
  malloc_zone_t* zone = malloc_default_purgeable_zone();
  ASSERT_DEATH({
      SetUpInDeathAssert();
      value_ = malloc_zone_malloc(zone, test_size_);
    }, kOomRegex);
}

TEST_F(OutOfMemoryDeathTest, ReallocPurgeable) {
  malloc_zone_t* zone = malloc_default_purgeable_zone();
  ASSERT_DEATH({
      SetUpInDeathAssert();
      value_ = malloc_zone_realloc(zone, NULL, test_size_);
    }, kOomRegex);
}

TEST_F(OutOfMemoryDeathTest, CallocPurgeable) {
  malloc_zone_t* zone = malloc_default_purgeable_zone();
  ASSERT_DEATH({
      SetUpInDeathAssert();
      value_ = malloc_zone_calloc(zone, 1024, test_size_ / 1024L);
    }, kOomRegex);
}

TEST_F(OutOfMemoryDeathTest, VallocPurgeable) {
  malloc_zone_t* zone = malloc_default_purgeable_zone();
  ASSERT_DEATH({
      SetUpInDeathAssert();
      value_ = malloc_zone_valloc(zone, test_size_);
    }, kOomRegex);
}

TEST_F(OutOfMemoryDeathTest, PosixMemalignPurgeable) {
  malloc_zone_t* zone = malloc_default_purgeable_zone();
  ASSERT_DEATH({
      SetUpInDeathAssert();
      value_ = malloc_zone_memalign(zone, 8, test_size_);
    }, kOomRegex);
}

// Since these allocation functions take a signed size, it's possible that
// calling them just once won't be enough to exhaust memory. In the 32-bit
// environment, it's likely that these allocation attempts will fail because
// not enough contiguous address space is available. In the 64-bit environment,
// it's likely that they'll fail because they would require a preposterous
// amount of (virtual) memory.

TEST_F(OutOfMemoryDeathTest, CFAllocatorSystemDefault) {
  ASSERT_DEATH({
      SetUpInDeathAssert();
      while ((value_ =
              base::AllocateViaCFAllocatorSystemDefault(signed_test_size_))) {}
    }, kOomRegex);
}

TEST_F(OutOfMemoryDeathTest, CFAllocatorMalloc) {
  ASSERT_DEATH({
      SetUpInDeathAssert();
      while ((value_ =
              base::AllocateViaCFAllocatorMalloc(signed_test_size_))) {}
    }, kOomRegex);
}

TEST_F(OutOfMemoryDeathTest, CFAllocatorMallocZone) {
  ASSERT_DEATH({
      SetUpInDeathAssert();
      while ((value_ =
              base::AllocateViaCFAllocatorMallocZone(signed_test_size_))) {}
    }, kOomRegex);
}

#if !defined(ARCH_CPU_64_BITS)

// See process_util_unittest_mac.mm for an explanation of why this test isn't
// run in the 64-bit environment.

TEST_F(OutOfMemoryDeathTest, PsychoticallyBigObjCObject) {
  ASSERT_DEATH({
      SetUpInDeathAssert();
      while ((value_ = base::AllocatePsychoticallyBigObjCObject())) {}
    }, kOomRegex);
}

#endif  // !ARCH_CPU_64_BITS
#endif  // OS_MACOSX

class OutOfMemoryHandledTest : public OutOfMemoryTest {
 public:
  static const size_t kSafeMallocSize = 512;
  static const size_t kSafeCallocSize = 128;
  static const size_t kSafeCallocItems = 4;

  void SetUp() override {
    OutOfMemoryTest::SetUp();

    // We enable termination on OOM - just as Chrome does at early
    // initialization - and test that UncheckedMalloc and  UncheckedCalloc
    // properly by-pass this in order to allow the caller to handle OOM.
    base::EnableTerminationOnOutOfMemory();
  }

  void TearDown() override {
#if defined(OS_MACOSX)
    base::allocator::UninterceptMallocZonesForTesting();
#endif
  }
};

#if defined(OS_WIN)

namespace {

DWORD HandleOutOfMemoryException(EXCEPTION_POINTERS* exception_ptrs,
                                 size_t expected_size) {
  EXPECT_EQ(base::win::kOomExceptionCode,
            exception_ptrs->ExceptionRecord->ExceptionCode);
  EXPECT_LE(1U, exception_ptrs->ExceptionRecord->NumberParameters);
  EXPECT_EQ(expected_size,
            exception_ptrs->ExceptionRecord->ExceptionInformation[0]);
  return EXCEPTION_EXECUTE_HANDLER;
}

}  // namespace

TEST_F(OutOfMemoryTest, TerminateBecauseOutOfMemoryReportsAllocSize) {
// On Windows, TerminateBecauseOutOfMemory reports the attempted allocation
// size in the exception raised.
#if defined(ARCH_CPU_64_BITS)
  // Test with a size larger than 32 bits on 64 bit machines.
  const size_t kAttemptedAllocationSize = 0xBADA55F00DULL;
#else
  const size_t kAttemptedAllocationSize = 0xBADA55;
#endif

  __try {
    base::TerminateBecauseOutOfMemory(kAttemptedAllocationSize);
  } __except (HandleOutOfMemoryException(GetExceptionInformation(),
                                         kAttemptedAllocationSize)) {
  }
}
#endif  // OS_WIN

// TODO(b.kelemen): make UncheckedMalloc and UncheckedCalloc work
// on Windows as well.
TEST_F(OutOfMemoryHandledTest, UncheckedMalloc) {
  EXPECT_TRUE(base::UncheckedMalloc(kSafeMallocSize, &value_));
  EXPECT_TRUE(value_ != nullptr);
  free(value_);

  EXPECT_FALSE(base::UncheckedMalloc(test_size_, &value_));
  EXPECT_TRUE(value_ == nullptr);
}

TEST_F(OutOfMemoryHandledTest, UncheckedCalloc) {
  EXPECT_TRUE(base::UncheckedCalloc(1, kSafeMallocSize, &value_));
  EXPECT_TRUE(value_ != nullptr);
  const char* bytes = static_cast<const char*>(value_);
  for (size_t i = 0; i < kSafeMallocSize; ++i)
    EXPECT_EQ(0, bytes[i]);
  free(value_);

  EXPECT_TRUE(
      base::UncheckedCalloc(kSafeCallocItems, kSafeCallocSize, &value_));
  EXPECT_TRUE(value_ != nullptr);
  bytes = static_cast<const char*>(value_);
  for (size_t i = 0; i < (kSafeCallocItems * kSafeCallocSize); ++i)
    EXPECT_EQ(0, bytes[i]);
  free(value_);

  EXPECT_FALSE(base::UncheckedCalloc(1, test_size_, &value_));
  EXPECT_TRUE(value_ == nullptr);
}
#endif  // !defined(OS_OPENBSD) && BUILDFLAG(ENABLE_WIN_ALLOCATOR_SHIM_TESTS) &&
        // !defined(MEMORY_TOOL_REPLACES_ALLOCATOR)
