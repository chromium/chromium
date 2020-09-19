// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/page_allocator.h"

#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include <string>
#include <vector>

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

TEST(PageAllocatorTest, Rounding) {
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

// Test that failed page allocations invoke base::ReleaseReservation().
// We detect this by making a reservation and ensuring that after failure, we
// can make a new reservation.
TEST(PageAllocatorTest, AllocFailure) {
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
                            PageInaccessible, PageTag::kChromium, false);
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
TEST(PageAllocatorTest, MAYBE_ReserveAddressSpace) {
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

TEST(PageAllocatorTest, AllocAndFreePages) {
  void* buffer = AllocPages(nullptr, PageAllocationGranularity(),
                            PageAllocationGranularity(), PageReadWrite,
                            PageTag::kChromium, true);
  EXPECT_TRUE(buffer);
  int* buffer0 = reinterpret_cast<int*>(buffer);
  *buffer0 = 42;
  EXPECT_EQ(42, *buffer0);
  FreePages(buffer, PageAllocationGranularity());
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

TEST(PageAllocatorTest, InaccessiblePages) {
  void* buffer = AllocPages(nullptr, PageAllocationGranularity(),
                            PageAllocationGranularity(), PageInaccessible,
                            PageTag::kChromium, true);
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

TEST(PageAllocatorTest, ReadExecutePages) {
  void* buffer = AllocPages(nullptr, PageAllocationGranularity(),
                            PageAllocationGranularity(), PageReadExecute,
                            PageTag::kChromium, true);
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
TEST(PageAllocatorTest, PageTagging) {
  void* buffer = AllocPages(nullptr, PageAllocationGranularity(),
                            PageAllocationGranularity(), PageInaccessible,
                            PageTag::kChromium, true);
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

TEST(PageAllocatorTest, DecommitErasesMemory) {
  if (!kDecommittedPagesAreAlwaysZeroed)
    return;

  size_t size = PageAllocationGranularity();
  void* buffer = AllocPages(nullptr, size, PageAllocationGranularity(),
                            PageReadWrite, PageTag::kChromium, true);
  ASSERT_TRUE(buffer);

  memset(buffer, 42, size);

  DecommitSystemPages(buffer, size);
  EXPECT_TRUE(RecommitSystemPages(buffer, size, PageReadWrite));

  uint8_t* recommitted_buffer = reinterpret_cast<uint8_t*>(buffer);
  uint32_t sum = 0;
  for (size_t i = 0; i < size; i++) {
    sum += recommitted_buffer[i];
  }
  EXPECT_EQ(0u, sum) << "Data was not erased";

  FreePages(buffer, size);
}

TEST(PageAllocatorTest, MappedPagesAccounting) {
  size_t size = PageAllocationGranularity();
  size_t mapped_size_before = GetTotalMappedSize();

  // Ask for a large alignment to make sure that trimming doesn't change the
  // accounting.
  void* data = AllocPages(nullptr, size, 128 * PageAllocationGranularity(),
                          PageInaccessible, PageTag::kChromium, true);
  ASSERT_TRUE(data);

  EXPECT_EQ(mapped_size_before + size, GetTotalMappedSize());

  DecommitSystemPages(data, size);
  EXPECT_EQ(mapped_size_before + size, GetTotalMappedSize());

  FreePages(data, size);
  EXPECT_EQ(mapped_size_before, GetTotalMappedSize());
}

}  // namespace base

#endif  // !defined(MEMORY_TOOL_REPLACES_ALLOCATOR)
