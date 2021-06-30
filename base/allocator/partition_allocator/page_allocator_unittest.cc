// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/page_allocator.h"

#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>
#include "base/cpu.h"
#include "base/logging.h"
#include "base/memory/tagging.h"
#include "base/notreached.h"

#include "base/allocator/partition_allocator/address_space_randomization.h"
#include "build/build_config.h"
#if defined(OS_ANDROID)
#include "base/debug/proc_maps_linux.h"
#endif  // defined(OS_ANDROID)
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_POSIX)
#include <setjmp.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/time.h>
#endif  // defined(OS_POSIX)

#include "base/allocator/partition_allocator/arm_bti_test_functions.h"

#if defined(__ARM_FEATURE_MEMORY_TAGGING)
#include <arm_acle.h>
#if defined(OS_ANDROID) || defined(OS_LINUX)
#define MTE_KILLED_BY_SIGNAL_AVAILABLE
#endif
#endif

#if !defined(MEMORY_TOOL_REPLACES_ALLOCATOR)

namespace base {

namespace {

// Any number of bytes that can be allocated with no trouble.
size_t EasyAllocSize() {
  return (1024 * 1024) & ~(PageAllocationGranularity() - 1);
}

// A huge amount of memory, greater than or equal to the ASLR space.
size_t HugeMemoryAmount() {
  return std::max(internal::ASLRMask(), std::size_t{2} * internal::ASLRMask());
}

}  // namespace

TEST(PartitionAllocPageAllocatorTest, Rounding) {
  EXPECT_EQ(0u, RoundUpToSystemPage(0u));
  EXPECT_EQ(SystemPageSize(), RoundUpToSystemPage(1));
  EXPECT_EQ(SystemPageSize(), RoundUpToSystemPage(SystemPageSize() - 1));
  EXPECT_EQ(SystemPageSize(), RoundUpToSystemPage(SystemPageSize()));
  EXPECT_EQ(2 * SystemPageSize(), RoundUpToSystemPage(SystemPageSize() + 1));
  EXPECT_EQ(0u, RoundDownToSystemPage(0u));
  EXPECT_EQ(0u, RoundDownToSystemPage(SystemPageSize() - 1));
  EXPECT_EQ(SystemPageSize(), RoundDownToSystemPage(SystemPageSize()));
  EXPECT_EQ(SystemPageSize(), RoundDownToSystemPage(SystemPageSize() + 1));
  EXPECT_EQ(SystemPageSize(), RoundDownToSystemPage(2 * SystemPageSize() - 1));
  EXPECT_EQ(0u, RoundUpToPageAllocationGranularity(0u));
  EXPECT_EQ(PageAllocationGranularity(), RoundUpToPageAllocationGranularity(1));
  EXPECT_EQ(PageAllocationGranularity(), RoundUpToPageAllocationGranularity(
                                             PageAllocationGranularity() - 1));
  EXPECT_EQ(PageAllocationGranularity(),
            RoundUpToPageAllocationGranularity(PageAllocationGranularity()));
  EXPECT_EQ(
      2 * PageAllocationGranularity(),
      RoundUpToPageAllocationGranularity(PageAllocationGranularity() + 1));
  EXPECT_EQ(0u, RoundDownToPageAllocationGranularity(0u));
  EXPECT_EQ(0u, RoundDownToPageAllocationGranularity(
                    PageAllocationGranularity() - 1));
  EXPECT_EQ(PageAllocationGranularity(),
            RoundDownToPageAllocationGranularity(PageAllocationGranularity()));
  EXPECT_EQ(PageAllocationGranularity(), RoundDownToPageAllocationGranularity(
                                             PageAllocationGranularity() + 1));
  EXPECT_EQ(PageAllocationGranularity(),
            RoundDownToPageAllocationGranularity(
                2 * PageAllocationGranularity() - 1));
}

TEST(PartitionAllocPageAllocatorTest, NextAlignedWithOffset) {
  EXPECT_EQ(1024u, NextAlignedWithOffset(1024, 1, 0));
  EXPECT_EQ(2024u, NextAlignedWithOffset(1024, 1024, 1000));
  EXPECT_EQ(2024u, NextAlignedWithOffset(2024, 1024, 1000));
  EXPECT_EQ(3048u, NextAlignedWithOffset(2025, 1024, 1000));
  EXPECT_EQ(2048u, NextAlignedWithOffset(1024, 2048, 0));
  EXPECT_EQ(2148u, NextAlignedWithOffset(1024, 2048, 100));
  EXPECT_EQ(2000u, NextAlignedWithOffset(1024, 2048, 2000));
}

// Test that failed page allocations invoke base::ReleaseReservation().
// We detect this by making a reservation and ensuring that after failure, we
// can make a new reservation.
TEST(PartitionAllocPageAllocatorTest, AllocFailure) {
  // Release any reservation made by another test.
  ReleaseReservation();

  // We can make a reservation.
  EXPECT_TRUE(ReserveAddressSpace(EasyAllocSize()));

  // We can't make another reservation until we trigger an allocation failure.
  EXPECT_FALSE(ReserveAddressSpace(EasyAllocSize()));

  size_t size = HugeMemoryAmount();
  // Skip the test for sanitizers and platforms with ASLR turned off.
  if (size == 0)
    return;

  void* result = AllocPages(nullptr, size, PageAllocationGranularity(),
                            PageInaccessible, PageTag::kChromium);
  if (result == nullptr) {
    // We triggered allocation failure. Our reservation should have been
    // released, and we should be able to make a new reservation.
    EXPECT_TRUE(ReserveAddressSpace(EasyAllocSize()));
    ReleaseReservation();
    return;
  }
  // We couldn't fail. Make sure reservation is still there.
  EXPECT_FALSE(ReserveAddressSpace(EasyAllocSize()));
}

// TODO(crbug.com/765801): Test failed on chromium.win/Win10 Tests x64.
#if defined(OS_WIN) && defined(ARCH_CPU_64_BITS)
#define MAYBE_ReserveAddressSpace DISABLED_ReserveAddressSpace
#else
#define MAYBE_ReserveAddressSpace ReserveAddressSpace
#endif  // defined(OS_WIN) && defined(ARCH_CPU_64_BITS)

// Test that reserving address space can fail.
TEST(PartitionAllocPageAllocatorTest, MAYBE_ReserveAddressSpace) {
  // Release any reservation made by another test.
  ReleaseReservation();

  size_t size = HugeMemoryAmount();
  // Skip the test for sanitizers and platforms with ASLR turned off.
  if (size == 0)
    return;

  bool success = ReserveAddressSpace(size);
  if (!success) {
    EXPECT_TRUE(ReserveAddressSpace(EasyAllocSize()));
    return;
  }
  // We couldn't fail. Make sure reservation is still there.
  EXPECT_FALSE(ReserveAddressSpace(EasyAllocSize()));
}

TEST(PartitionAllocPageAllocatorTest, AllocAndFreePages) {
  void* buffer = AllocPages(nullptr, PageAllocationGranularity(),
                            PageAllocationGranularity(), PageReadWrite,
                            PageTag::kChromium);
  EXPECT_TRUE(buffer);
  int* buffer0 = reinterpret_cast<int*>(buffer);
  *buffer0 = 42;
  EXPECT_EQ(42, *buffer0);
  FreePages(buffer, PageAllocationGranularity());
}

TEST(PartitionAllocPageAllocatorTest, AllocPagesAligned) {
  size_t alignment = 8 * PageAllocationGranularity();
  size_t sizes[] = {PageAllocationGranularity(),
                    alignment - PageAllocationGranularity(), alignment,
                    alignment + PageAllocationGranularity(), alignment * 4};
  size_t offsets[] = {0, PageAllocationGranularity(), alignment / 2,
                      alignment - PageAllocationGranularity()};
  for (size_t size : sizes) {
    for (size_t offset : offsets) {
      void* buffer = AllocPagesWithAlignOffset(
          nullptr, size, alignment, offset, PageReadWrite, PageTag::kChromium);
      EXPECT_TRUE(buffer);
      EXPECT_EQ(reinterpret_cast<uintptr_t>(buffer) % alignment, offset);
      FreePages(buffer, size);
    }
  }
}

TEST(PartitionAllocPageAllocatorTest,
     AllocAndFreePagesWithPageReadWriteTagged) {
  // This test checks that a page allocated with PageReadWriteTagged is
  // safe to use on all systems (even those which don't support MTE).
  void* buffer = AllocPages(nullptr, PageAllocationGranularity(),
                            PageAllocationGranularity(), PageReadWriteTagged,
                            PageTag::kChromium);
  EXPECT_TRUE(buffer);
  int* buffer0 = reinterpret_cast<int*>(buffer);
  *buffer0 = 42;
  EXPECT_EQ(42, *buffer0);
  FreePages(buffer, PageAllocationGranularity());
}

TEST(PartitionAllocPageAllocatorTest,
     AllocAndFreePagesWithPageReadExecuteConfirmCFI) {
  // This test checks that indirect branches to anything other than a valid
  // branch target in a PageReadExecute-mapped crash on systems which support
  // the Armv8.5 Branch Target Identification extension.
  base::CPU cpu;
  if (!cpu.has_bti()) {
#if defined(OS_IOS)
    // Workaround for incorrectly failed iOS tests with GTEST_SKIP,
    // see crbug.com/912138 for details.
    return;
#else
    GTEST_SKIP();
#endif
  }
#if defined(MTE_KILLED_BY_SIGNAL_AVAILABLE)
  // Next, map some read-write memory and copy the BTI-enabled function there.
  void* buffer = AllocPages(nullptr, PageAllocationGranularity(),
                            PageAllocationGranularity(), PageReadWrite,
                            PageTag::kChromium);
  ptrdiff_t function_range =
      reinterpret_cast<ptrdiff_t>(arm_bti_test_function_end) -
      reinterpret_cast<ptrdiff_t>(arm_bti_test_function);
  ptrdiff_t invalid_offset =
      reinterpret_cast<ptrdiff_t>(arm_bti_test_function_invalid_offset) -
      reinterpret_cast<ptrdiff_t>(arm_bti_test_function);
  memcpy(buffer, reinterpret_cast<void*>(arm_bti_test_function),
         function_range);
  uint32_t* bufferi = reinterpret_cast<uint32_t*>(buffer);
  // Next re-protect the page.
  SetSystemPagesAccess(buffer, PageAllocationGranularity(),
                       PageReadExecuteProtected);
  // Attempt to call the function through the BTI-enabled entrypoint. Confirm
  // that it works.
  int64_t (*bti_enabled_fn)(int64_t) =
      reinterpret_cast<int64_t (*)(int64_t)>(bufferi);
  int64_t (*bti_invalid_fn)(int64_t) =
      reinterpret_cast<int64_t (*)(int64_t)>(bufferi + invalid_offset);
  EXPECT_EQ(bti_enabled_fn(15), 18);
  // Next, attempt to call the function without the entrypoint.
  EXPECT_EXIT({ bti_invalid_fn(15); }, testing::KilledBySignal(SIGILL),
              "");  // Should crash with SIGILL.
  FreePages(buffer, PageAllocationGranularity());
#else
  NOTREACHED();
#endif
}

TEST(PartitionAllocPageAllocatorTest,
     AllocAndFreePagesWithPageReadWriteTaggedSynchronous) {
  // This test checks that a page allocated with PageReadWriteTagged
  // generates tag violations if allocated on a system which supports the
  // Armv8.5 Memory Tagging Extension.
  base::CPU cpu;
  if (!cpu.has_mte()) {
    // Skip this test if there's no MTE.
#if defined(OS_IOS)
    return;
#else
    GTEST_SKIP();
#endif
  }

#if defined(MTE_KILLED_BY_SIGNAL_AVAILABLE)
  void* buffer = AllocPages(nullptr, PageAllocationGranularity(),
                            PageAllocationGranularity(), PageReadWriteTagged,
                            PageTag::kChromium);
  EXPECT_TRUE(buffer);
  int* buffer0 = reinterpret_cast<int*>(buffer);
  // Assign an 0x1 tag to the first granule of buffer.
  int* buffer1 = reinterpret_cast<int*>(__arm_mte_increment_tag(buffer, 0x1));
  EXPECT_NE(buffer0, buffer1);
  __arm_mte_set_tag(buffer1);
  // Retrieve the tag to ensure that it's set.
  buffer1 = reinterpret_cast<int*>(__arm_mte_get_tag(buffer));
  // Prove that the tag is different (if they're the same, the test won't work).
  ASSERT_NE(buffer0, buffer1);
  memory::TagViolationReportingMode parent_tagging_mode =
      memory::GetMemoryTaggingModeForCurrentThread();
  EXPECT_EXIT(
      {
  // Switch to synchronous mode.
#if defined(OS_ANDROID)
        memory::ChangeMemoryTaggingModeForAllThreadsPerProcess(
            memory::TagViolationReportingMode::kSynchronous);
#else
        memory::ChangeMemoryTaggingModeForCurrentThread(
            memory::TagViolationReportingMode::kSynchronous);
#endif  // defined(OS_ANDROID)
        EXPECT_EQ(memory::GetMemoryTaggingModeForCurrentThread(),
                  memory::TagViolationReportingMode::kSynchronous);
        // Write to the buffer using its previous tag. A segmentation fault
        // should be delivered.
        *buffer0 = 42;
      },
      testing::KilledBySignal(SIGSEGV), "");
  EXPECT_EQ(memory::GetMemoryTaggingModeForCurrentThread(),
            parent_tagging_mode);
  FreePages(buffer, PageAllocationGranularity());
#else
  NOTREACHED();
#endif
}

TEST(PartitionAllocPageAllocatorTest,
     AllocAndFreePagesWithPageReadWriteTaggedAsynchronous) {
  // This test checks that a page allocated with PageReadWriteTagged
  // generates tag violations if allocated on a system which supports MTE.
  base::CPU cpu;
  if (!cpu.has_mte()) {
    // Skip this test if there's no MTE.
#if defined(OS_IOS)
    return;
#else
    GTEST_SKIP();
#endif
  }

#if defined(MTE_KILLED_BY_SIGNAL_AVAILABLE)
  void* buffer = AllocPages(nullptr, PageAllocationGranularity(),
                            PageAllocationGranularity(), PageReadWriteTagged,
                            PageTag::kChromium);
  EXPECT_TRUE(buffer);
  int* buffer0 = reinterpret_cast<int*>(buffer);
  __arm_mte_set_tag(__arm_mte_increment_tag(buffer, 0x1));
  int* buffer1 = reinterpret_cast<int*>(__arm_mte_get_tag(buffer));
  EXPECT_NE(buffer0, buffer1);
  memory::TagViolationReportingMode parent_tagging_mode =
      memory::GetMemoryTaggingModeForCurrentThread();
  EXPECT_EXIT(
      {
  // Switch to asynchronous mode.
#if defined(OS_ANDROID)
        memory::ChangeMemoryTaggingModeForAllThreadsPerProcess(
            memory::TagViolationReportingMode::kAsynchronous);
#else
        memory::ChangeMemoryTaggingModeForCurrentThread(
            memory::TagViolationReportingMode::kAsynchronous);
#endif  // defined(OS_ANDROID)
        EXPECT_EQ(memory::GetMemoryTaggingModeForCurrentThread(),
                  memory::TagViolationReportingMode::kAsynchronous);
        // Write to the buffer using its previous tag. A fault should be
        // generated at this point but we may not notice straight away...
        *buffer0 = 42;
        EXPECT_EQ(42, *buffer0);
        LOG(ERROR) << "=";  // Until we receive control back from the kernel
                            // (e.g. on a system call).
      },
      testing::KilledBySignal(SIGSEGV), "");
  FreePages(buffer, PageAllocationGranularity());
  EXPECT_EQ(memory::GetMemoryTaggingModeForCurrentThread(),
            parent_tagging_mode);
#else
  NOTREACHED();
#endif
}

// Test permission setting on POSIX, where we can set a trap handler.
#if defined(OS_POSIX)

namespace {
sigjmp_buf g_continuation;

void SignalHandler(int signal, siginfo_t* info, void*) {
  siglongjmp(g_continuation, 1);
}
}  // namespace

// On Mac, sometimes we get SIGBUS instead of SIGSEGV, so handle that too.
#if defined(OS_APPLE)
#define EXTRA_FAULT_BEGIN_ACTION() \
  struct sigaction old_bus_action; \
  sigaction(SIGBUS, &action, &old_bus_action);
#define EXTRA_FAULT_END_ACTION() sigaction(SIGBUS, &old_bus_action, nullptr);
#else
#define EXTRA_FAULT_BEGIN_ACTION()
#define EXTRA_FAULT_END_ACTION()
#endif

// Install a signal handler so we can catch the fault we're about to trigger.
#define FAULT_TEST_BEGIN()                  \
  struct sigaction action = {};             \
  struct sigaction old_action = {};         \
  action.sa_sigaction = SignalHandler;      \
  sigemptyset(&action.sa_mask);             \
  action.sa_flags = SA_SIGINFO;             \
  sigaction(SIGSEGV, &action, &old_action); \
  EXTRA_FAULT_BEGIN_ACTION();               \
  int const save_sigs = 1;                  \
  if (!sigsetjmp(g_continuation, save_sigs)) {
// Fault generating code goes here...

// Handle when sigsetjmp returns nonzero (we are returning from our handler).
#define FAULT_TEST_END()                      \
  }                                           \
  else {                                      \
    sigaction(SIGSEGV, &old_action, nullptr); \
    EXTRA_FAULT_END_ACTION();                 \
  }

TEST(PartitionAllocPageAllocatorTest, InaccessiblePages) {
  void* buffer = AllocPages(nullptr, PageAllocationGranularity(),
                            PageAllocationGranularity(), PageInaccessible,
                            PageTag::kChromium);
  EXPECT_TRUE(buffer);

  FAULT_TEST_BEGIN()

  // Reading from buffer should fault.
  int* buffer0 = reinterpret_cast<int*>(buffer);
  int buffer0_contents = *buffer0;
  EXPECT_EQ(buffer0_contents, *buffer0);
  EXPECT_TRUE(false);

  FAULT_TEST_END()

  FreePages(buffer, PageAllocationGranularity());
}

TEST(PartitionAllocPageAllocatorTest, ReadExecutePages) {
  void* buffer = AllocPages(nullptr, PageAllocationGranularity(),
                            PageAllocationGranularity(), PageReadExecute,
                            PageTag::kChromium);
  EXPECT_TRUE(buffer);
  int* buffer0 = reinterpret_cast<int*>(buffer);
  // Reading from buffer should succeed.
  int buffer0_contents = *buffer0;

  FAULT_TEST_BEGIN()

  // Writing to buffer should fault.
  *buffer0 = ~buffer0_contents;
  EXPECT_TRUE(false);

  FAULT_TEST_END()

  // Make sure no write occurred.
  EXPECT_EQ(buffer0_contents, *buffer0);
  FreePages(buffer, PageAllocationGranularity());
}

#endif  // defined(OS_POSIX)

#if defined(OS_ANDROID)
TEST(PartitionAllocPageAllocatorTest, PageTagging) {
  void* buffer = AllocPages(nullptr, PageAllocationGranularity(),
                            PageAllocationGranularity(), PageInaccessible,
                            PageTag::kChromium);
  EXPECT_TRUE(buffer);

  std::string proc_maps;
  EXPECT_TRUE(debug::ReadProcMaps(&proc_maps));
  std::vector<debug::MappedMemoryRegion> regions;
  EXPECT_TRUE(debug::ParseProcMaps(proc_maps, &regions));

  bool found = false;
  for (const auto& region : regions) {
    if (region.start == reinterpret_cast<uintptr_t>(buffer)) {
      found = true;
      EXPECT_EQ("[anon:chromium]", region.path);
      break;
    }
  }

  FreePages(buffer, PageAllocationGranularity());
  EXPECT_TRUE(found);
}
#endif  // defined(OS_ANDROID)

TEST(PartitionAllocPageAllocatorTest, DecommitErasesMemory) {
  if (!DecommittedMemoryIsAlwaysZeroed())
    return;

  size_t size = PageAllocationGranularity();
  void* buffer = AllocPages(nullptr, size, PageAllocationGranularity(),
                            PageReadWrite, PageTag::kChromium);
  ASSERT_TRUE(buffer);

  memset(buffer, 42, size);

  DecommitSystemPages(buffer, size, PageKeepPermissionsIfPossible);
  RecommitSystemPages(buffer, size, PageReadWrite,
                      PageKeepPermissionsIfPossible);

  uint8_t* recommitted_buffer = reinterpret_cast<uint8_t*>(buffer);
  uint32_t sum = 0;
  for (size_t i = 0; i < size; i++) {
    sum += recommitted_buffer[i];
  }
  EXPECT_EQ(0u, sum) << "Data was not erased";

  FreePages(buffer, size);
}

TEST(PartitionAllocPageAllocatorTest, MappedPagesAccounting) {
  size_t size = PageAllocationGranularity();
  // Ask for a large alignment to make sure that trimming doesn't change the
  // accounting.
  size_t alignment = 128 * PageAllocationGranularity();
  size_t offsets[] = {0, PageAllocationGranularity(), alignment / 2,
                      alignment - PageAllocationGranularity()};

  size_t mapped_size_before = GetTotalMappedSize();

  for (size_t offset : offsets) {
    void* data = AllocPagesWithAlignOffset(
        nullptr, size, alignment, offset, PageInaccessible, PageTag::kChromium);
    ASSERT_TRUE(data);

    EXPECT_EQ(mapped_size_before + size, GetTotalMappedSize());

    DecommitSystemPages(data, size, PageKeepPermissionsIfPossible);
    EXPECT_EQ(mapped_size_before + size, GetTotalMappedSize());

    FreePages(data, size);
    EXPECT_EQ(mapped_size_before, GetTotalMappedSize());
  }
}

}  // namespace base

#endif  // !defined(MEMORY_TOOL_REPLACES_ALLOCATOR)
