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
constexpr size_t kEasyAllocSize =
    (1024 * 1024) & ~(kPageAllocationGranularity - 1);

// A huge amount of memory, greater than or equal to the ASLR space.
constexpr size_t kHugeMemoryAmount =
    std::max(internal::kASLRMask, std::size_t{2} * internal::kASLRMask);

}  // namespace

TEST(PageAllocatorTest, Rounding) {
  EXPECT_EQ(0u, RoundUpToSystemPage(0u));
  EXPECT_EQ(kSystemPageSize, RoundUpToSystemPage(1));
  EXPECT_EQ(kSystemPageSize, RoundUpToSystemPage(kSystemPageSize - 1));
  EXPECT_EQ(kSystemPageSize, RoundUpToSystemPage(kSystemPageSize));
  EXPECT_EQ(2 * kSystemPageSize, RoundUpToSystemPage(kSystemPageSize + 1));
  EXPECT_EQ(0u, RoundDownToSystemPage(0u));
  EXPECT_EQ(0u, RoundDownToSystemPage(kSystemPageSize - 1));
  EXPECT_EQ(kSystemPageSize, RoundDownToSystemPage(kSystemPageSize));
  EXPECT_EQ(kSystemPageSize, RoundDownToSystemPage(kSystemPageSize + 1));
  EXPECT_EQ(kSystemPageSize, RoundDownToSystemPage(2 * kSystemPageSize - 1));
  EXPECT_EQ(0u, RoundUpToPageAllocationGranularity(0u));
  EXPECT_EQ(kPageAllocationGranularity, RoundUpToPageAllocationGranularity(1));
  EXPECT_EQ(kPageAllocationGranularity,
            RoundUpToPageAllocationGranularity(kPageAllocationGranularity - 1));
  EXPECT_EQ(kPageAllocationGranularity,
            RoundUpToPageAllocationGranularity(kPageAllocationGranularity));
  EXPECT_EQ(2 * kPageAllocationGranularity,
            RoundUpToPageAllocationGranularity(kPageAllocationGranularity + 1));
  EXPECT_EQ(0u, RoundDownToPageAllocationGranularity(0u));
  EXPECT_EQ(
      0u, RoundDownToPageAllocationGranularity(kPageAllocationGranularity - 1));
  EXPECT_EQ(kPageAllocationGranularity,
            RoundDownToPageAllocationGranularity(kPageAllocationGranularity));
  EXPECT_EQ(kPageAllocationGranularity, RoundDownToPageAllocationGranularity(
                                            kPageAllocationGranularity + 1));
  EXPECT_EQ(
      kPageAllocationGranularity,
      RoundDownToPageAllocationGranularity(2 * kPageAllocationGranularity - 1));
}

// Test that failed page allocations invoke base::ReleaseReservation().
// We detect this by making a reservation and ensuring that after failure, we
// can make a new reservation.
TEST(PageAllocatorTest, AllocFailure) {
  // Release any reservation made by another test.
  ReleaseReservation();

  // We can make a reservation.
  EXPECT_TRUE(ReserveAddressSpace(kEasyAllocSize));

  // We can't make another reservation until we trigger an allocation failure.
  EXPECT_FALSE(ReserveAddressSpace(kEasyAllocSize));

  size_t size = kHugeMemoryAmount;
  // Skip the test for sanitizers and platforms with ASLR turned off.
  if (size == 0)
    return;

  void* result = AllocPages(nullptr, size, kPageAllocationGranularity,
                            PageInaccessible, PageTag::kChromium, false);
  if (result == nullptr) {
    // We triggered allocation failure. Our reservation should have been
    // released, and we should be able to make a new reservation.
    EXPECT_TRUE(ReserveAddressSpace(kEasyAllocSize));
    ReleaseReservation();
    return;
  }
  // We couldn't fail. Make sure reservation is still there.
  EXPECT_FALSE(ReserveAddressSpace(kEasyAllocSize));
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

  size_t size = kHugeMemoryAmount;
  // Skip the test for sanitizers and platforms with ASLR turned off.
  if (size == 0)
    return;

  bool success = ReserveAddressSpace(size);
  if (!success) {
    EXPECT_TRUE(ReserveAddressSpace(kEasyAllocSize));
    return;
  }
  // We couldn't fail. Make sure reservation is still there.
  EXPECT_FALSE(ReserveAddressSpace(kEasyAllocSize));
}

TEST(PageAllocatorTest, AllocAndFreePages) {
  void* buffer = AllocPages(nullptr, kPageAllocationGranularity,
                            kPageAllocationGranularity, PageReadWrite,
                            PageTag::kChromium, true);
  EXPECT_TRUE(buffer);
  int* buffer0 = reinterpret_cast<int*>(buffer);
  *buffer0 = 42;
  EXPECT_EQ(42, *buffer0);
  FreePages(buffer, kPageAllocationGranularity);
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
#if defined(OS_MACOSX)
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
  void* buffer = AllocPages(nullptr, kPageAllocationGranularity,
                            kPageAllocationGranularity, PageInaccessible,
                            PageTag::kChromium, true);
  EXPECT_TRUE(buffer);

  FAULT_TEST_BEGIN()

  // Reading from buffer should fault.
  int* buffer0 = reinterpret_cast<int*>(buffer);
  int buffer0_contents = *buffer0;
  EXPECT_EQ(buffer0_contents, *buffer0);
  EXPECT_TRUE(false);

  FAULT_TEST_END()

  FreePages(buffer, kPageAllocationGranularity);
}

TEST(PageAllocatorTest, ReadExecutePages) {
  void* buffer = AllocPages(nullptr, kPageAllocationGranularity,
                            kPageAllocationGranularity, PageReadExecute,
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
  FreePages(buffer, kPageAllocationGranularity);
}

#endif  // defined(OS_POSIX)

#if defined(OS_ANDROID)
TEST(PageAllocatorTest, PageTagging) {
  void* buffer = AllocPages(nullptr, kPageAllocationGranularity,
                            kPageAllocationGranularity, PageInaccessible,
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

  FreePages(buffer, kPageAllocationGranularity);
  EXPECT_TRUE(found);
}
#endif  // defined(OS_ANDROID)

}  // namespace base

#endif  // !defined(MEMORY_TOOL_REPLACES_ALLOCATOR)
