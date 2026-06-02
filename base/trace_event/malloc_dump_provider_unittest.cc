// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/malloc_dump_provider.h"

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

namespace base::trace_event {

#if BUILDFLAG(IS_WIN)

namespace {

class ScopedTestHeap {
 public:
  ScopedTestHeap() : handle_(::HeapCreate(0, 0, 0)) { CHECK(handle_); }
  ~ScopedTestHeap() { CHECK(::HeapDestroy(handle_)); }

  ScopedTestHeap(const ScopedTestHeap&) = delete;
  ScopedTestHeap& operator=(const ScopedTestHeap&) = delete;

  HANDLE handle() { return handle_; }

 private:
  HANDLE handle_;
};

// Above the historical HeapAlloc->VirtualAlloc threshold (~512 KB), so the
// allocation is guaranteed to appear as an orphan busy entry.
constexpr size_t kLargeAllocBytes = 2 * 1024 * 1024;

}  // namespace

TEST(MallocDumpProviderTest, WinHeapInfo_EmptyHeap) {
  ScopedTestHeap heap;

  auto info = internal::WinHeapInfo::FromHandleForTesting(heap.handle());

  EXPECT_EQ(info.allocated_size, 0u);
  EXPECT_EQ(info.block_count, 0u);
  // A fresh heap has at least one reserved region.
  EXPECT_GT(info.committed_size + info.uncommitted_size, 0u);
}

TEST(MallocDumpProviderTest, WinHeapInfo_SmallAllocStaysInRegion) {
  ScopedTestHeap heap;
  void* p = ::HeapAlloc(heap.handle(), 0, 64);
  ASSERT_TRUE(p);

  auto info = internal::WinHeapInfo::FromHandleForTesting(heap.handle());

  EXPECT_GE(info.allocated_size, 64u);
  EXPECT_GE(info.block_count, 1u);
  // The block lives inside a region whose committed bytes already include it,
  // so committed_size must dominate allocated_size.
  EXPECT_GE(info.committed_size, info.allocated_size);

  ::HeapFree(heap.handle(), 0, p);
}

TEST(MallocDumpProviderTest, WinHeapInfo_LargeAllocBecomesOrphanBusy) {
  ScopedTestHeap heap;
  void* p = ::HeapAlloc(heap.handle(), 0, kLargeAllocBytes);
  ASSERT_TRUE(p);

  auto info = internal::WinHeapInfo::FromHandleForTesting(heap.handle());

  EXPECT_GE(info.allocated_size, kLargeAllocBytes);
  EXPECT_GE(info.block_count, 1u);
  // Regression assertion for the orphan-busy-entry fix: committed_size must
  // grow with the large allocation. Before the fix, large blocks lived
  // outside any PROCESS_HEAP_REGION and were not counted as committed, so
  // committed_size would have been (much) less than allocated_size.
  EXPECT_GE(info.committed_size, info.allocated_size);

  ::HeapFree(heap.handle(), 0, p);
}

#endif  // BUILDFLAG(IS_WIN)

}  // namespace base::trace_event
