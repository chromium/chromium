// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/page_allocator.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "partition_alloc/address_space_randomization.h"
#include "partition_alloc/build_config.h"
#include "partition_alloc/buildflags.h"
#include "partition_alloc/page_allocator_constants.h"
#include "partition_alloc/partition_alloc_base/cpu.h"
#include "partition_alloc/partition_alloc_base/logging.h"
#include "partition_alloc/partition_alloc_base/notreached.h"
#include "partition_alloc/partition_alloc_config.h"
#include "partition_alloc/tagging.h"

#if defined(LINUX_NAME_REGION)
#include "partition_alloc/partition_alloc_base/debug/proc_maps_linux.h"
#endif

#include "testing/gtest/include/gtest/gtest.h"

#if PA_BUILDFLAG(IS_POSIX)
#include <sys/mman.h>
#include <sys/time.h>

#include <csetjmp>
#include <csignal>
#endif  // PA_BUILDFLAG(IS_POSIX)

#include "partition_alloc/arm_bti_test_functions.h"

#if PA_BUILDFLAG(HAS_MEMORY_TAGGING)
#include <arm_acle.h>
#if PA_BUILDFLAG(IS_ANDROID) || PA_BUILDFLAG(IS_LINUX)
#define MTE_KILLED_BY_SIGNAL_AVAILABLE
#endif
#endif  // PA_BUILDFLAG(HAS_MEMORY_TAGGING)

#if !defined(MEMORY_TOOL_REPLACES_ALLOCATOR)

namespace partition_alloc::internal {

namespace {

// Any number of bytes that can be allocated with no trouble.
size_t EasyAllocSize() {
  return (1024 * 1024) & ~(PageAllocationGranularity() - 1);
}

// A huge amount of memory, greater than or equal to the ASLR space.
size_t HugeMemoryAmount() {
  return std::max(::partition_alloc::internal::ASLRMask(),
                  std::size_t{2} * ::partition_alloc::internal::ASLRMask());
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
  if (size == 0) {
    return;
  }

  uintptr_t result =
      AllocPages(size, PageAllocationGranularity(),
                 PageAccessibilityConfiguration(
                     PageAccessibilityConfiguration::kInaccessible),
                 PageTag::kChromium);
  if (!result) {
    // We triggered allocation failure. Our reservation should have been
    // released, and we should be able to make a new reservation.
    EXPECT_TRUE(ReserveAddressSpace(EasyAllocSize()));
    ReleaseReservation();
    return;
  }
  // We couldn't fail. Make sure reservation is still there.
  EXPECT_FALSE(ReserveAddressSpace(EasyAllocSize()));
}

// TODO(crbug.com/41344946): Test failed on chromium.win/Win10 Tests x64.
#if PA_BUILDFLAG(IS_WIN) && PA_BUILDFLAG(PA_ARCH_CPU_64_BITS)
#define MAYBE_ReserveAddressSpace DISABLED_ReserveAddressSpace
#else
#define MAYBE_ReserveAddressSpace ReserveAddressSpace
#endif  // PA_BUILDFLAG(IS_WIN) && PA_BUILDFLAG(PA_ARCH_CPU_64_BITS)

// Test that reserving address space can fail.
TEST(PartitionAllocPageAllocatorTest, MAYBE_ReserveAddressSpace) {
  // Release any reservation made by another test.
  ReleaseReservation();

  size_t size = HugeMemoryAmount();
  // Skip the test for sanitizers and platforms with ASLR turned off.
  if (size == 0) {
    return;
  }

  bool success = ReserveAddressSpace(size);
  if (!success) {
    EXPECT_TRUE(ReserveAddressSpace(EasyAllocSize()));
    return;
  }
  // We couldn't fail. Make sure reservation is still there.
  EXPECT_FALSE(ReserveAddressSpace(EasyAllocSize()));
}

TEST(PartitionAllocPageAllocatorTest, AllocAndFreePages) {
  uintptr_t buffer =
      AllocPages(PageAllocationGranularity(), PageAllocationGranularity(),
                 PageAccessibilityConfiguration(
                     PageAccessibilityConfiguration::kReadWrite),
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
      uintptr_t buffer = AllocPagesWithAlignOffset(
          0, size, alignment, offset,
          PageAccessibilityConfiguration(
              PageAccessibilityConfiguration::kReadWrite),
          PageTag::kChromium);
      EXPECT_TRUE(buffer);
      EXPECT_EQ(buffer % alignment, offset);
      FreePages(buffer, size);
    }
  }
}

TEST(PartitionAllocPageAllocatorTest,
     AllocAndFreePagesWithPageReadWriteTagged) {
  // This test checks that a page allocated with
  // PageAccessibilityConfiguration::kReadWriteTagged is safe to use on all
  // systems (even those which don't support MTE).
  uintptr_t buffer =
      AllocPages(PageAllocationGranularity(), PageAllocationGranularity(),
                 PageAccessibilityConfiguration(
                     PageAccessibilityConfiguration::kReadWriteTagged),
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
  // branch target in a PageAccessibilityConfiguration::kReadExecute-mapped
  // crash on systems which support the Armv8.5 Branch Target Identification
  // extension.
  base::CPU cpu;
  if (!cpu.has_bti()) {
#if PA_BUILDFLAG(IS_IOS)
    // Workaround for incorrectly failed iOS tests with GTEST_SKIP,
    // see crbug.com/912138 for details.
    return;
#else
    GTEST_SKIP();
#endif
  }
#if defined(MTE_KILLED_BY_SIGNAL_AVAILABLE)
  // Next, map some read-write memory and copy the BTI-enabled function there.
  uintptr_t buffer =
      AllocPages(PageAllocationGranularity(), PageAllocationGranularity(),
                 PageAccessibilityConfiguration(
                     PageAccessibilityConfiguration::kReadWrite),
                 PageTag::kChromium);
  ptrdiff_t function_range =
      reinterpret_cast<char*>(arm_bti_test_function_end) -
      reinterpret_cast<char*>(arm_bti_test_function);
  ptrdiff_t invalid_offset =
      reinterpret_cast<char*>(arm_bti_test_function_invalid_offset) -
      reinterpret_cast<char*>(arm_bti_test_function);
  memcpy(reinterpret_cast<void*>(buffer),
         reinterpret_cast<void*>(arm_bti_test_function), function_range);

  // Next re-protect the page.
  SetSystemPagesAccess(
      buffer, PageAllocationGranularity(),
      PageAccessibilityConfiguration(
          PageAccessibilityConfiguration::kReadExecuteProtected));

  using BTITestFunction = int64_t (*)(int64_t);

  // Attempt to call the function through the BTI-enabled entrypoint. Confirm
  // that it works.
  BTITestFunction bti_enabled_fn = reinterpret_cast<BTITestFunction>(buffer);
  BTITestFunction bti_invalid_fn =
      reinterpret_cast<BTITestFunction>(buffer + invalid_offset);
  EXPECT_EQ(bti_enabled_fn(15), 18);
  // Next, attempt to call the function without the entrypoint.
  EXPECT_EXIT({ bti_invalid_fn(15); }, testing::KilledBySignal(SIGILL),
              "");  // Should crash with SIGILL.
  FreePages(buffer, PageAllocationGranularity());
#else
  PA_NOTREACHED();
#endif
}

TEST(PartitionAllocPageAllocatorTest,
     AllocAndFreePagesWithPageReadWriteTaggedSynchronous) {
  // This test checks that a page allocated with
  // PageAccessibilityConfiguration::kReadWriteTagged generates tag violations
  // if allocated on a system which supports the
  // Armv8.5 Memory Tagging Extension.
  base::CPU cpu;
  if (!cpu.has_mte()) {
    // Skip this test if there's no MTE.
#if PA_BUILDFLAG(IS_IOS)
    return;
#else
    GTEST_SKIP();
#endif
  }

#if defined(MTE_KILLED_BY_SIGNAL_AVAILABLE)
  uintptr_t buffer =
      AllocPages(PageAllocationGranularity(), PageAllocationGranularity(),
                 PageAccessibilityConfiguration(
                     PageAccessibilityConfiguration::kReadWriteTagged),
                 PageTag::kChromium);
  EXPECT_TRUE(buffer);
  int* buffer0 = reinterpret_cast<int*>(buffer);
  // Assign an 0x1 tag to the first granule of buffer.
  int* buffer1 = __arm_mte_increment_tag(buffer0, 0x1);
  EXPECT_NE(buffer0, buffer1);
  __arm_mte_set_tag(buffer1);
  // Retrieve the tag to ensure that it's set.
  buffer1 = __arm_mte_get_tag(buffer0);
  // Prove that the tag is different (if they're the same, the test won't work).
  ASSERT_NE(buffer0, buffer1);
  TagViolationReportingMode parent_tagging_mode =
      GetMemoryTaggingModeForCurrentThread();
  EXPECT_EXIT(
      {
  // Switch to synchronous mode.
#if PA_BUILDFLAG(IS_ANDROID)
        bool success = ChangeMemoryTaggingModeForAllThreadsPerProcess(
            TagViolationReportingMode::kSynchronous);
        EXPECT_TRUE(success);
#else
        ChangeMemoryTaggingModeForCurrentThread(
            TagViolationReportingMode::kSynchronous);
#endif  // PA_BUILDFLAG(IS_ANDROID)
        EXPECT_EQ(GetMemoryTaggingModeForCurrentThread(),
                  TagViolationReportingMode::kSynchronous);
        // Write to the buffer using its previous tag. A segmentation fault
        // should be delivered.
        *buffer0 = 42;
      },
      testing::KilledBySignal(SIGSEGV), "");
  EXPECT_EQ(GetMemoryTaggingModeForCurrentThread(), parent_tagging_mode);
  FreePages(buffer, PageAllocationGranularity());
#else
  PA_NOTREACHED();
#endif
}

TEST(PartitionAllocPageAllocatorTest,
     AllocAndFreePagesWithPageReadWriteTaggedAsynchronous) {
  // This test checks that a page allocated with
  // PageAccessibilityConfiguration::kReadWriteTagged generates tag violations
  // if allocated on a system which supports MTE.
  base::CPU cpu;
  if (!cpu.has_mte()) {
    // Skip this test if there's no MTE.
#if PA_BUILDFLAG(IS_IOS)
    return;
#else
    GTEST_SKIP();
#endif
  }

#if defined(MTE_KILLED_BY_SIGNAL_AVAILABLE)
  uintptr_t buffer =
      AllocPages(PageAllocationGranularity(), PageAllocationGranularity(),
                 PageAccessibilityConfiguration(
                     PageAccessibilityConfiguration::kReadWriteTagged),
                 PageTag::kChromium);
  EXPECT_TRUE(buffer);
  int* buffer0 = reinterpret_cast<int*>(buffer);
  __arm_mte_set_tag(__arm_mte_increment_tag(buffer0, 0x1));
  int* buffer1 = __arm_mte_get_tag(buffer0);
  EXPECT_NE(buffer0, buffer1);
  TagViolationReportingMode parent_tagging_mode =
      GetMemoryTaggingModeForCurrentThread();
  EXPECT_EXIT(
      {
  // Switch to asynchronous mode.
#if PA_BUILDFLAG(IS_ANDROID)
        bool success = ChangeMemoryTaggingModeForAllThreadsPerProcess(
            TagViolationReportingMode::kAsynchronous);
        EXPECT_TRUE(success);
#else
        ChangeMemoryTaggingModeForCurrentThread(
            TagViolationReportingMode::kAsynchronous);
#endif  // PA_BUILDFLAG(IS_ANDROID)
        EXPECT_EQ(GetMemoryTaggingModeForCurrentThread(),
                  TagViolationReportingMode::kAsynchronous);
        // Write to the buffer using its previous tag. A fault should be
        // generated at this point but we may not notice straight away...
        *buffer0 = 42;
        EXPECT_EQ(42, *buffer0);
        PA_LOG(ERROR) << "=";  // Until we receive control back from the kernel
                               // (e.g. on a system call).
      },
      testing::KilledBySignal(SIGSEGV), "");
  FreePages(buffer, PageAllocationGranularity());
  EXPECT_EQ(GetMemoryTaggingModeForCurrentThread(), parent_tagging_mode);
#else
  PA_NOTREACHED();
#endif
}

// Test permission setting on POSIX, where we can set a trap handler.
#if PA_BUILDFLAG(IS_POSIX)

namespace {
sigjmp_buf g_continuation;

void SignalHandler(int signal, siginfo_t* info, void*) {
  siglongjmp(g_continuation, 1);
}
}  // namespace

// On Mac, sometimes we get SIGBUS instead of SIGSEGV, so handle that too.
#if PA_BUILDFLAG(IS_APPLE)
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
  uintptr_t buffer =
      AllocPages(PageAllocationGranularity(), PageAllocationGranularity(),
                 PageAccessibilityConfiguration(
                     PageAccessibilityConfiguration::kInaccessible),
                 PageTag::kChromium);
  EXPECT_TRUE(buffer);

  FAULT_TEST_BEGIN()

  // Reading from buffer should fault.
  // Volatile prevents the compiler from eliminating the load by folding
  // buffer0_contents == *buffer0.
  volatile int* buffer0 = reinterpret_cast<int*>(buffer);
  int buffer0_contents = *buffer0;
  EXPECT_EQ(buffer0_contents, *buffer0);
  EXPECT_TRUE(false);

  FAULT_TEST_END()

  FreePages(buffer, PageAllocationGranularity());
}

// TODO(crbug.com/40212918): Understand why we can't read from Read-Execute
// pages on iOS.
#if PA_BUILDFLAG(IS_IOS)
#define MAYBE_ReadExecutePages DISABLED_ReadExecutePages
#else
#define MAYBE_ReadExecutePages ReadExecutePages
#endif  // PA_BUILDFLAG(IS_IOS)
TEST(PartitionAllocPageAllocatorTest, MAYBE_ReadExecutePages) {
  uintptr_t buffer =
      AllocPages(PageAllocationGranularity(), PageAllocationGranularity(),
                 PageAccessibilityConfiguration(
                     PageAccessibilityConfiguration::kReadExecute),
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

#endif  // PA_BUILDFLAG(IS_POSIX)

#if defined(LINUX_NAME_REGION)
TEST(PartitionAllocPageAllocatorTest, PageTagging) {
  size_t size = PageAllocationGranularity();
  uintptr_t buffer =
      AllocPages(size, PageAllocationGranularity(),
                 PageAccessibilityConfiguration(
                     PageAccessibilityConfiguration::kInaccessible),
                 PageTag::kChromium);
  ASSERT_TRUE(buffer);

  auto is_region_named = [](uintptr_t start_address) {
    std::string proc_maps;
    EXPECT_TRUE(base::debug::ReadProcMaps(&proc_maps));
    std::vector<base::debug::MappedMemoryRegion> regions;
    EXPECT_TRUE(base::debug::ParseProcMaps(proc_maps, &regions));

    bool found = false;
    for (const auto& region : regions) {
      if (region.start == start_address) {
        found = true;
        return "[anon:chromium]" == region.path;
      }
    }
    EXPECT_TRUE(found);
    return false;
  };

  bool before = is_region_named(buffer);
  DecommitAndZeroSystemPages(buffer, size);
  bool after = is_region_named(buffer);

#if PA_BUILDFLAG(IS_ANDROID)
  EXPECT_TRUE(before) << "VMA tagging should always work on Android";
#endif
  // When not running on Android, the prctl() command may be defined in the
  // headers, but not be implemented by the host kernel.
  EXPECT_EQ(before, after);

  FreePages(buffer, size);
}
#endif  // defined(LINUX_NAME_REGION)

TEST(PartitionAllocPageAllocatorTest, DecommitErasesMemory) {
  if (!DecommittedMemoryIsAlwaysZeroed()) {
    return;
  }

  size_t size = PageAllocationGranularity();
  uintptr_t buffer = AllocPages(size, PageAllocationGranularity(),
                                PageAccessibilityConfiguration(
                                    PageAccessibilityConfiguration::kReadWrite),
                                PageTag::kChromium);
  ASSERT_TRUE(buffer);

  memset(reinterpret_cast<void*>(buffer), 42, size);

  DecommitSystemPages(buffer, size,
                      PageAccessibilityDisposition::kAllowKeepForPerf);
  RecommitSystemPages(buffer, size,
                      PageAccessibilityConfiguration(
                          PageAccessibilityConfiguration::kReadWrite),
                      PageAccessibilityDisposition::kAllowKeepForPerf);

  uint8_t* recommitted_buffer = reinterpret_cast<uint8_t*>(buffer);
  uint32_t sum = 0;
  for (size_t i = 0; i < size; i++) {
    sum += recommitted_buffer[i];
  }
  EXPECT_EQ(0u, sum) << "Data was not erased";

  FreePages(buffer, size);
}

TEST(PartitionAllocPageAllocatorTest, DecommitAndZero) {
  size_t size = PageAllocationGranularity();
  uintptr_t buffer = AllocPages(size, PageAllocationGranularity(),
                                PageAccessibilityConfiguration(
                                    PageAccessibilityConfiguration::kReadWrite),
                                PageTag::kChromium);
  ASSERT_TRUE(buffer);

  memset(reinterpret_cast<void*>(buffer), 42, size);

  DecommitAndZeroSystemPages(buffer, size);

// Test permission setting on POSIX, where we can set a trap handler.
#if PA_BUILDFLAG(IS_POSIX)

  FAULT_TEST_BEGIN()

  // Reading from buffer should now fault.
  int* buffer0 = reinterpret_cast<int*>(buffer);
  int buffer0_contents = *buffer0;
  EXPECT_EQ(buffer0_contents, *buffer0);
  EXPECT_TRUE(false);

  FAULT_TEST_END()

#endif

  // Clients of the DecommitAndZero API (in particular, V8), currently just
  // call SetSystemPagesAccess to mark the region as accessible again, so we
  // use that here as well.
  SetSystemPagesAccess(buffer, size,
                       PageAccessibilityConfiguration(
                           PageAccessibilityConfiguration::kReadWrite));

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
    uintptr_t data = AllocPagesWithAlignOffset(
        0, size, alignment, offset,
        PageAccessibilityConfiguration(
            PageAccessibilityConfiguration::kInaccessible),
        PageTag::kChromium);
    ASSERT_TRUE(data);

    EXPECT_EQ(mapped_size_before + size, GetTotalMappedSize());

    DecommitSystemPages(data, size,
                        PageAccessibilityDisposition::kAllowKeepForPerf);
    EXPECT_EQ(mapped_size_before + size, GetTotalMappedSize());

    FreePages(data, size);
    EXPECT_EQ(mapped_size_before, GetTotalMappedSize());
  }
}

TEST(PartitionAllocPageAllocatorTest, AllocInaccessibleWillJitLater) {
  // Verify that kInaccessibleWillJitLater allows read/write, and read/execute
  // permissions to be set.
  uintptr_t buffer =
      AllocPages(PageAllocationGranularity(), PageAllocationGranularity(),
                 PageAccessibilityConfiguration(
                     PageAccessibilityConfiguration::kInaccessibleWillJitLater),
                 PageTag::kChromium);
  EXPECT_TRUE(
      TrySetSystemPagesAccess(buffer, PageAllocationGranularity(),
                              PageAccessibilityConfiguration(
                                  PageAccessibilityConfiguration::kReadWrite)));
  EXPECT_TRUE(TrySetSystemPagesAccess(
      buffer, PageAllocationGranularity(),
      PageAccessibilityConfiguration(
          PageAccessibilityConfiguration::kReadExecute)));
  FreePages(buffer, PageAllocationGranularity());
}

#if PA_BUILDFLAG(IS_IOS) || PA_BUILDFLAG(IS_MAC)
// TODO(crbug.com/40916148): Fix test to GTEST_SKIP() if MAP_JIT is in-use,
// or to be run otherwise, since kReadWriteExecute is used in some other
// configurations.
#define MAYBE_AllocReadWriteExecute DISABLED_AllocReadWriteExecute
#else
#define MAYBE_AllocReadWriteExecute AllocReadWriteExecute
#endif  // PA_BUILDFLAG(IS_IOS) || PA_BUILDFLAG(IS_MAC)
TEST(PartitionAllocPageAllocatorTest, MAYBE_AllocReadWriteExecute) {
  // Verify that kReadWriteExecute is similarly functional.
  uintptr_t buffer =
      AllocPages(PageAllocationGranularity(), PageAllocationGranularity(),
                 PageAccessibilityConfiguration(
                     PageAccessibilityConfiguration::kReadWriteExecute),
                 PageTag::kChromium);
  EXPECT_TRUE(
      TrySetSystemPagesAccess(buffer, PageAllocationGranularity(),
                              PageAccessibilityConfiguration(
                                  PageAccessibilityConfiguration::kReadWrite)));
  EXPECT_TRUE(TrySetSystemPagesAccess(
      buffer, PageAllocationGranularity(),
      PageAccessibilityConfiguration(
          PageAccessibilityConfiguration::kReadExecute)));
  FreePages(buffer, PageAllocationGranularity());
}

}  // namespace partition_alloc::internal

#endif  // !defined(MEMORY_TOOL_REPLACES_ALLOCATOR)
