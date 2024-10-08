// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#define _CRT_SECURE_NO_WARNINGS

#include "base/process/memory.h"

#include <stddef.h>

#include <limits>
#include <tuple>
#include <vector>

#include "base/allocator/allocator_check.h"
#include "base/compiler_specific.h"
#include "base/debug/alias.h"
#include "base/memory/aligned_memory.h"
#include "base/memory/page_size.h"
#include "build/build_config.h"
#include "partition_alloc/buildflags.h"
#include "partition_alloc/page_allocator.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif
#if BUILDFLAG(IS_POSIX)
#include <errno.h>
#endif
#if BUILDFLAG(IS_MAC)
#include <malloc/malloc.h>
#include "base/check_op.h"
#include "base/process/memory_unittest_mac.h"
#include "partition_alloc/shim/allocator_interception_apple.h"
#include "partition_alloc/shim/allocator_shim.h"
#endif
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include <malloc.h>
#include "base/test/malloc_wrapper.h"
#endif
#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#endif

#if BUILDFLAG(IS_WIN)

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

#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_MAC)

// For the following Mac tests:
// Note that base::EnableTerminationOnHeapCorruption() is called as part of
// test suite setup and does not need to be done again, else mach_override
// will fail.

// Wrap free() in a function to thwart Clang's -Wfree-nonheap-object warning.
static void callFree(void *ptr) {
  free(ptr);
}

TEST(ProcessMemoryTest, MacTerminateOnHeapCorruption) {
#if PA_BUILDFLAG(USE_ALLOCATOR_SHIM)
  allocator_shim::InitializeAllocatorShim();
#endif
  // Assert that freeing an unallocated pointer will crash the process.
  char buf[9];
  asm("" : "=m"(buf));  // Prevent clang from being too smart.
#if ARCH_CPU_64_BITS
  // On 64 bit Macs, the malloc system automatically abort()s on heap corruption
  // but does not output anything.
  ASSERT_DEATH(callFree(buf), "");
#elif defined(ADDRESS_SANITIZER)
  // AddressSanitizer replaces malloc() and prints a different error message on
  // heap corruption.
  ASSERT_DEATH(callFree(buf), "attempting free on address which "
      "was not malloc\\(\\)-ed");
#else
  ADD_FAILURE() << "This test is not supported in this build configuration.";
#endif

#if PA_BUILDFLAG(USE_ALLOCATOR_SHIM)
  allocator_shim::UninterceptMallocZonesForTesting();
#endif
}

#endif  // BUILDFLAG(IS_MAC)

#if PA_BUILDFLAG(USE_ALLOCATOR_SHIM)
TEST(MemoryTest, AllocatorShimWorking) {
#if BUILDFLAG(IS_MAC)
  allocator_shim::InitializeAllocatorShim();
  allocator_shim::InterceptAllocationsMac();
#endif
  ASSERT_TRUE(base::allocator::IsAllocatorInitialized());

#if BUILDFLAG(IS_MAC)
  allocator_shim::UninterceptMallocZonesForTesting();
#endif
}
#endif  // PA_BUILDFLAG(USE_ALLOCATOR_SHIM)

// OpenBSD does not support these tests. Don't test these on ASan/TSan/MSan
// configurations: only test the real allocator.
#if !BUILDFLAG(IS_OPENBSD) && PA_BUILDFLAG(USE_ALLOCATOR_SHIM) && \
    !defined(MEMORY_TOOL_REPLACES_ALLOCATOR)

namespace {

#if BUILDFLAG(IS_WIN)

// Windows raises an exception in order to make the exit code unique to OOM.
#define ASSERT_OOM_DEATH(statement) \
  ASSERT_EXIT(statement,            \
              testing::ExitedWithCode(base::win::kOomExceptionCode), "")

#else

#define ASSERT_OOM_DEATH(statement) ASSERT_DEATH(statement, "")

#endif  // BUILDFLAG(IS_WIN)

}  // namespace

class OutOfMemoryTest : public testing::Test {
 public:
  OutOfMemoryTest()
      :  // Make test size as large as possible minus a few pages so that
         // alignment or other rounding doesn't make it wrap.
        test_size_(std::numeric_limits<std::size_t>::max() -
                   3 * base::GetPageSize()),
        // A test size that is > 2Gb and will cause the allocators to reject
        // the allocation due to security restrictions. See crbug.com/169327.
        insecure_test_size_(std::numeric_limits<int>::max()),
        signed_test_size_(std::numeric_limits<ssize_t>::max()) {}

 protected:
  size_t test_size_;
  size_t insecure_test_size_;
  ssize_t signed_test_size_;
};

class OutOfMemoryDeathTest : public OutOfMemoryTest {
 public:
  void SetUpInDeathAssert() {
#if BUILDFLAG(IS_MAC) && PA_BUILDFLAG(USE_ALLOCATOR_SHIM)
    allocator_shim::InitializeAllocatorShim();
#endif

    // Must call EnableTerminationOnOutOfMemory() because that is called from
    // chrome's main function and therefore hasn't been called yet.
    // Since this call may result in another thread being created and death
    // tests shouldn't be started in a multithread environment, this call
    // should be done inside of the ASSERT_DEATH.
    base::EnableTerminationOnOutOfMemory();
  }

#if BUILDFLAG(IS_MAC)
  void TearDown() override {
    allocator_shim::UninterceptMallocZonesForTesting();
  }
#endif

  // These tests don't work properly on old x86 Android; crbug.com/1181112
  bool ShouldSkipTest() {
#if BUILDFLAG(IS_ANDROID) && defined(ARCH_CPU_X86)
    return base::android::BuildInfo::GetInstance()->sdk_int() <
           base::android::SDK_VERSION_NOUGAT;
#else
    return false;
#endif
  }
};

TEST_F(OutOfMemoryDeathTest, New) {
  if (ShouldSkipTest()) {
    return;
  }
  ASSERT_OOM_DEATH({
    SetUpInDeathAssert();
    [[maybe_unused]] void* volatile ptr = operator new(test_size_);
  });
}

TEST_F(OutOfMemoryDeathTest, NewArray) {
  if (ShouldSkipTest()) {
    return;
  }
  ASSERT_OOM_DEATH({
    SetUpInDeathAssert();
    [[maybe_unused]] void* volatile ptr = new char[test_size_];
  });
}

TEST_F(OutOfMemoryDeathTest, Malloc) {
  if (ShouldSkipTest()) {
    return;
  }
  ASSERT_OOM_DEATH({
    SetUpInDeathAssert();
    [[maybe_unused]] void* volatile ptr = malloc(test_size_);
  });
}

TEST_F(OutOfMemoryDeathTest, Realloc) {
  if (ShouldSkipTest()) {
    return;
  }
  ASSERT_OOM_DEATH({
    SetUpInDeathAssert();
    [[maybe_unused]] void* volatile ptr = realloc(nullptr, test_size_);
  });
}

TEST_F(OutOfMemoryDeathTest, Calloc) {
  if (ShouldSkipTest()) {
    return;
  }
  ASSERT_OOM_DEATH({
    SetUpInDeathAssert();
    [[maybe_unused]] void* volatile ptr = calloc(1024, test_size_ / 1024L);
  });
}

TEST_F(OutOfMemoryDeathTest, AlignedAlloc) {
  if (ShouldSkipTest()) {
    return;
  }
  ASSERT_OOM_DEATH({
    SetUpInDeathAssert();
    [[maybe_unused]] void* volatile ptr = base::AlignedAlloc(test_size_, 8);
  });
}

// POSIX does not define an aligned realloc function.
#if BUILDFLAG(IS_WIN)
TEST_F(OutOfMemoryDeathTest, AlignedRealloc) {
  if (ShouldSkipTest()) {
    return;
  }
  ASSERT_OOM_DEATH({
    SetUpInDeathAssert();
    [[maybe_unused]] void* volatile ptr =
        _aligned_realloc(nullptr, test_size_, 8);
  });
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
        [[maybe_unused]] void* volatile ptr = new char[test_size_];
      },
      testing::ExitedWithCode(kUnhandledExceptionExitCode), "");
}
#endif  // BUILDFLAG(IS_WIN)

// OS X has no 2Gb allocation limit.
// See https://crbug.com/169327.
// PartitionAlloc is not active in component builds, so cannot enforce
// this limit. (//BUILD.gn asserts that we cannot have an official component
// build.)
#if !BUILDFLAG(IS_MAC) && !defined(COMPONENT_BUILD)
TEST_F(OutOfMemoryDeathTest, SecurityNew) {
  if (ShouldSkipTest()) {
    return;
  }
  ASSERT_OOM_DEATH({
    SetUpInDeathAssert();
    [[maybe_unused]] void* volatile ptr = operator new(insecure_test_size_);
  });
}

TEST_F(OutOfMemoryDeathTest, SecurityNewArray) {
  if (ShouldSkipTest()) {
    return;
  }
  ASSERT_OOM_DEATH({
    SetUpInDeathAssert();
    [[maybe_unused]] void* volatile ptr = new char[insecure_test_size_];
  });
}

TEST_F(OutOfMemoryDeathTest, SecurityMalloc) {
  if (ShouldSkipTest()) {
    return;
  }
  ASSERT_OOM_DEATH({
    SetUpInDeathAssert();
    [[maybe_unused]] void* volatile ptr = malloc(insecure_test_size_);
  });
}

TEST_F(OutOfMemoryDeathTest, SecurityRealloc) {
  if (ShouldSkipTest()) {
    return;
  }
  ASSERT_OOM_DEATH({
    SetUpInDeathAssert();
    [[maybe_unused]] void* volatile ptr = realloc(nullptr, insecure_test_size_);
  });
}

TEST_F(OutOfMemoryDeathTest, SecurityCalloc) {
  if (ShouldSkipTest()) {
    return;
  }
  ASSERT_OOM_DEATH({
    SetUpInDeathAssert();
    [[maybe_unused]] void* volatile ptr =
        calloc(1024, insecure_test_size_ / 1024L);
  });
}

TEST_F(OutOfMemoryDeathTest, SecurityAlignedAlloc) {
  if (ShouldSkipTest()) {
    return;
  }
  ASSERT_OOM_DEATH({
    SetUpInDeathAssert();
    [[maybe_unused]] void* volatile ptr =
        base::AlignedAlloc(insecure_test_size_, 8);
  });
}

// POSIX does not define an aligned realloc function.
#if BUILDFLAG(IS_WIN)
TEST_F(OutOfMemoryDeathTest, SecurityAlignedRealloc) {
  if (ShouldSkipTest()) {
    return;
  }
  ASSERT_OOM_DEATH({
    SetUpInDeathAssert();
    [[maybe_unused]] void* volatile ptr =
        _aligned_realloc(nullptr, insecure_test_size_, 8);
  });
}
#endif  // BUILDFLAG(IS_WIN)
#endif  // !BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

TEST_F(OutOfMemoryDeathTest, Valloc) {
  ASSERT_OOM_DEATH({
    SetUpInDeathAssert();
    [[maybe_unused]] void* volatile ptr = valloc(test_size_);
    EXPECT_TRUE(ptr);
  });
}

TEST_F(OutOfMemoryDeathTest, SecurityValloc) {
  ASSERT_OOM_DEATH({
    SetUpInDeathAssert();
    [[maybe_unused]] void* volatile ptr = valloc(insecure_test_size_);
  });
}

TEST_F(OutOfMemoryDeathTest, Pvalloc) {
  ASSERT_OOM_DEATH({
    SetUpInDeathAssert();
    [[maybe_unused]] void* volatile ptr = pvalloc(test_size_);
  });
}

TEST_F(OutOfMemoryDeathTest, SecurityPvalloc) {
  ASSERT_OOM_DEATH({
    SetUpInDeathAssert();
    [[maybe_unused]] void* volatile ptr = pvalloc(insecure_test_size_);
  });
}

TEST_F(OutOfMemoryDeathTest, Memalign) {
  ASSERT_OOM_DEATH({
    SetUpInDeathAssert();
    [[maybe_unused]] void* volatile ptr = memalign(4, test_size_);
  });
}

TEST_F(OutOfMemoryDeathTest, ViaSharedLibraries) {
  // This tests that the run-time symbol resolution is overriding malloc for
  // shared libraries as well as for our code.
  ASSERT_OOM_DEATH({
    SetUpInDeathAssert();
    [[maybe_unused]] void* volatile ptr = MallocWrapper(test_size_);
  });
}
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

// Android doesn't implement posix_memalign().
#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_ANDROID)
TEST_F(OutOfMemoryDeathTest, Posix_memalign) {
  // Grab the return value of posix_memalign to silence a compiler warning
  // about unused return values. We don't actually care about the return
  // value, since we're asserting death.
  ASSERT_OOM_DEATH({
    SetUpInDeathAssert();
    void* ptr;
    EXPECT_EQ(ENOMEM, posix_memalign(&ptr, 8, test_size_));
  });
}
#endif  // BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_MAC)

// Purgeable zone tests

TEST_F(OutOfMemoryDeathTest, MallocPurgeable) {
  malloc_zone_t* zone = malloc_default_purgeable_zone();
  ASSERT_OOM_DEATH({
    SetUpInDeathAssert();
    [[maybe_unused]] void* volatile ptr = malloc_zone_malloc(zone, test_size_);
  });
}

TEST_F(OutOfMemoryDeathTest, ReallocPurgeable) {
  malloc_zone_t* zone = malloc_default_purgeable_zone();
  ASSERT_OOM_DEATH({
    SetUpInDeathAssert();
    [[maybe_unused]] void* volatile ptr =
        malloc_zone_realloc(zone, nullptr, test_size_);
  });
}

TEST_F(OutOfMemoryDeathTest, CallocPurgeable) {
  malloc_zone_t* zone = malloc_default_purgeable_zone();
  ASSERT_OOM_DEATH({
    SetUpInDeathAssert();
    [[maybe_unused]] void* volatile ptr =
        malloc_zone_calloc(zone, 1024, test_size_ / 1024L);
  });
}

TEST_F(OutOfMemoryDeathTest, VallocPurgeable) {
  malloc_zone_t* zone = malloc_default_purgeable_zone();
  ASSERT_OOM_DEATH({
    SetUpInDeathAssert();
    [[maybe_unused]] void* volatile ptr = malloc_zone_valloc(zone, test_size_);
  });
}

TEST_F(OutOfMemoryDeathTest, PosixMemalignPurgeable) {
  malloc_zone_t* zone = malloc_default_purgeable_zone();
  ASSERT_OOM_DEATH({
    SetUpInDeathAssert();
    [[maybe_unused]] void* volatile ptr =
        malloc_zone_memalign(zone, 8, test_size_);
  });
}

// Since these allocation functions take a signed size, it's possible that
// calling them just once won't be enough to exhaust memory. In the 32-bit
// environment, it's likely that these allocation attempts will fail because
// not enough contiguous address space is available. In the 64-bit environment,
// it's likely that they'll fail because they would require a preposterous
// amount of (virtual) memory.

TEST_F(OutOfMemoryDeathTest, CFAllocatorMalloc) {
  ASSERT_OOM_DEATH({
    SetUpInDeathAssert();
    [[maybe_unused]] void* ptr;
    while ((ptr = base::AllocateViaCFAllocatorMalloc(signed_test_size_))) {
    }
  });
}

#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
// PartitionAlloc-Everywhere does not intercept other malloc zones than the
// default (the top) malloc zone.  Plus,
// CFAllocatorAllocate(kCFAllocatorSystemDefault, size, 0) does not call the
// default (the top) malloc zone on macOS 10.xx (does call it on macOS 11 and
// later though).
#define MAYBE_CFAllocatorSystemDefault DISABLED_CFAllocatorSystemDefault
#else
#define MAYBE_CFAllocatorSystemDefault CFAllocatorSystemDefault
#endif
TEST_F(OutOfMemoryDeathTest, MAYBE_CFAllocatorSystemDefault) {
  ASSERT_OOM_DEATH({
    SetUpInDeathAssert();
    [[maybe_unused]] void* ptr;
    while (
        (ptr = base::AllocateViaCFAllocatorSystemDefault(signed_test_size_))) {
    }
  });
}

#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
// PartitionAlloc-Everywhere does not intercept other malloc zones than the
// default (the top) malloc zone.  Plus,
// CFAllocatorAllocate(kCFAllocatorMallocZone, size, 0) does not call the
// default (the top) malloc zone on macOS 10.xx (does call it on macOS 11 and
// later though).
#define MAYBE_CFAllocatorMallocZone DISABLED_CFAllocatorMallocZone
#else
#define MAYBE_CFAllocatorMallocZone CFAllocatorMallocZone
#endif
TEST_F(OutOfMemoryDeathTest, MAYBE_CFAllocatorMallocZone) {
  ASSERT_OOM_DEATH({
    SetUpInDeathAssert();
    [[maybe_unused]] void* ptr;
    while ((ptr = base::AllocateViaCFAllocatorMallocZone(signed_test_size_))) {
    }
  });
}

#endif  // BUILDFLAG(IS_MAC)

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
#if BUILDFLAG(IS_MAC)
    allocator_shim::UninterceptMallocZonesForTesting();
#endif
  }
};

#if BUILDFLAG(IS_WIN)

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
#endif  // BUILDFLAG(IS_WIN)

#if defined(ARCH_CPU_32_BITS) && \
    (BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS))

void TestAllocationsReleaseReservation(void* (*alloc_fn)(size_t),
                                       void (*free_fn)(void*)) {
  partition_alloc::ReleaseReservation();
  base::EnableTerminationOnOutOfMemory();

  constexpr size_t kMiB = 1 << 20;
  constexpr size_t kReservationSize = 512 * kMiB;  // MiB.

  size_t reservation_size = kReservationSize;
  while (!partition_alloc::ReserveAddressSpace(reservation_size)) {
    reservation_size -= 16 * kMiB;
  }
  ASSERT_TRUE(partition_alloc::HasReservationForTesting());
  ASSERT_GT(reservation_size, 0u);

  // Allocate a large area at a time to bump into address space exhaustion
  // before other limits. It is important not to do a larger allocation, to
  // verify that we can allocate without removing the reservation. On the other
  // hand, must be large enough to make the underlying implementation call
  // mmap()/VirtualAlloc().
  size_t allocation_size = reservation_size / 2;

  std::vector<void*> areas;
  // Pre-reserve the vector to make sure that we don't hit the address space
  // limit while resizing the array.
  areas.reserve(((2 * 4096 * kMiB) / allocation_size) + 1);

  while (true) {
    void* area = alloc_fn(allocation_size / 2);
    ASSERT_TRUE(area);
    areas.push_back(area);

    // Working as intended, the allocation was successful, and the reservation
    // was dropped instead of crashing.
    //
    // Meaning that the test is either successful, or crashes.
    if (!partition_alloc::HasReservationForTesting())
      break;
  }

  EXPECT_GE(areas.size(), 2u)
      << "Should be able to allocate without releasing the reservation";

  for (void* ptr : areas)
    free_fn(ptr);
}

TEST_F(OutOfMemoryHandledTest, MallocReleasesReservation) {
  TestAllocationsReleaseReservation(malloc, free);
}

TEST_F(OutOfMemoryHandledTest, NewReleasesReservation) {
  TestAllocationsReleaseReservation(
      [](size_t size) { return static_cast<void*>(new char[size]); },
      [](void* ptr) { delete[] static_cast<char*>(ptr); });
}
#endif  // defined(ARCH_CPU_32_BITS) && (BUILDFLAG(IS_WIN) ||
        // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS))

#if BUILDFLAG(IS_ANDROID)

// Android's allocator does not allow overcommits, so very large
// UncheckedMallocs will yield OOM errors.
// TODO(crbug.com/40143202): Fails on some Android bots.
#define MAYBE_UncheckedMallocDies DISABLED_UncheckedMallocDies
#define MAYBE_UncheckedCallocDies DISABLED_UncheckedCallocDies
TEST_F(OutOfMemoryDeathTest, MAYBE_UncheckedMallocDies) {
  ASSERT_OOM_DEATH({
    SetUpInDeathAssert();
    void* data;
    std::ignore = base::UncheckedMalloc(test_size_, &data);
    // Death expected here.
  });
}

TEST_F(OutOfMemoryDeathTest, MAYBE_UncheckedCallocDies) {
  ASSERT_OOM_DEATH({
    SetUpInDeathAssert();
    void* data;
    std::ignore = base::UncheckedCalloc(1, test_size_, &data);
    // Death expected here.
  });
}

#else

TEST_F(OutOfMemoryHandledTest, UncheckedMalloc) {
  void* ptr;
  EXPECT_TRUE(base::UncheckedMalloc(kSafeMallocSize, &ptr));
  EXPECT_TRUE(ptr != nullptr);
  base::UncheckedFree(ptr);

  EXPECT_FALSE(base::UncheckedMalloc(test_size_, &ptr));
  EXPECT_TRUE(ptr == nullptr);
}

TEST_F(OutOfMemoryHandledTest, UncheckedCalloc) {
  void* ptr;
  EXPECT_TRUE(base::UncheckedCalloc(1, kSafeMallocSize, &ptr));
  EXPECT_TRUE(ptr != nullptr);
  const char* bytes = static_cast<const char*>(ptr);
  for (size_t i = 0; i < kSafeMallocSize; ++i)
    EXPECT_EQ(0, bytes[i]);
  base::UncheckedFree(ptr);

  EXPECT_TRUE(base::UncheckedCalloc(kSafeCallocItems, kSafeCallocSize, &ptr));
  EXPECT_TRUE(ptr != nullptr);
  bytes = static_cast<const char*>(ptr);
  for (size_t i = 0; i < (kSafeCallocItems * kSafeCallocSize); ++i)
    EXPECT_EQ(0, bytes[i]);
  base::UncheckedFree(ptr);

  EXPECT_FALSE(base::UncheckedCalloc(1, test_size_, &ptr));
  EXPECT_TRUE(ptr == nullptr);
}

#endif  // BUILDFLAG(IS_ANDROID)
#endif  // !BUILDFLAG(IS_OPENBSD) && PA_BUILDFLAG(USE_ALLOCATOR_SHIM) &&
        // !defined(MEMORY_TOOL_REPLACES_ALLOCATOR)

#if BUILDFLAG(IS_MAC) && PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

// Not a proper test because it needs to be in a static initializer, see the
// comment in UncheckedMalloc() in memory_mac.mm.
//
// The "test" passes if the binary doesn't crash.
size_t need_a_static_initializer = [] {
  void* ptr;
  constexpr size_t kRequestedSize = 1000u;
  bool ok = base::UncheckedMalloc(kRequestedSize, &ptr);
  CHECK(ok);
  size_t actual_size = malloc_size(ptr);
  // If no known zone owns the pointer, dispatching code in libmalloc returns 0.
  CHECK_GE(actual_size, kRequestedSize);
  // If no zone owns the pointer, libmalloc aborts here.
  free(ptr);

  return actual_size;
}();

#endif  // BUILDFLAG(IS_MAC) && PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
