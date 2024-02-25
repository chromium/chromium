// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fcntl.h>
#include <stdint.h>

#include <memory>

#include "base/files/scoped_file.h"
#include "base/memory/madv_free_discardable_memory_allocator_posix.h"
#include "base/memory/madv_free_discardable_memory_posix.h"
#include "base/memory/page_size.h"
#include "base/tracing_buildflags.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(ENABLE_BASE_TRACING)
#include "base/trace_event/memory_allocator_dump.h"  // no-presubmit-check
#include "base/trace_event/process_memory_dump.h"    // no-presubmit-check
#endif  // BUILDFLAG(ENABLE_BASE_TRACING)

#define SUCCEED_IF_MADV_FREE_UNSUPPORTED()                                  \
  do {                                                                      \
    if (GetMadvFreeSupport() != base::MadvFreeSupport::kSupported) {        \
      SUCCEED()                                                             \
          << "MADV_FREE is not supported (Linux 4.5+ required), vacuously " \
             "passing test";                                                \
      return;                                                               \
    }                                                                       \
  } while (0)

namespace base {

class MadvFreeDiscardableMemoryAllocatorPosixTest : public ::testing::Test {
 protected:
  MadvFreeDiscardableMemoryAllocatorPosixTest() {
#if BUILDFLAG(ENABLE_BASE_TRACING)
    base::trace_event::MemoryDumpArgs dump_args = {
        base::trace_event::MemoryDumpLevelOfDetail::kDetailed};
    pmd_ = std::make_unique<base::trace_event::ProcessMemoryDump>(dump_args);
#endif  // BUILDFLAG(ENABLE_BASE_TRACING)
  }

  std::unique_ptr<MadvFreeDiscardableMemoryPosix>
  AllocateLockedMadvFreeDiscardableMemory(size_t size) {
    return std::unique_ptr<MadvFreeDiscardableMemoryPosix>(
        static_cast<MadvFreeDiscardableMemoryPosix*>(
            allocator_.AllocateLockedDiscardableMemory(size).release()));
  }

#if BUILDFLAG(ENABLE_BASE_TRACING)
  size_t GetDiscardableMemorySizeFromDump(const DiscardableMemory& mem,
                                          const std::string& dump_id) {
    return mem.CreateMemoryAllocatorDump(dump_id.c_str(), pmd_.get())
        ->GetSizeInternal();
  }
#endif  // BUILDFLAG(ENABLE_BASE_TRACING)

  MadvFreeDiscardableMemoryAllocatorPosix allocator_;
  const size_t kPageSize = base::GetPageSize();
#if BUILDFLAG(ENABLE_BASE_TRACING)
  std::unique_ptr<base::trace_event::ProcessMemoryDump> pmd_;
#endif  // BUILDFLAG(ENABLE_BASE_TRACING)
};

TEST_F(MadvFreeDiscardableMemoryAllocatorPosixTest, AllocateAndUseMemory) {
  SUCCEED_IF_MADV_FREE_UNSUPPORTED();

  // Allocate 4 pages of discardable memory.
  auto mem1 = AllocateLockedMadvFreeDiscardableMemory(kPageSize * 3 + 1);

  EXPECT_TRUE(mem1->IsLockedForTesting());
#if BUILDFLAG(ENABLE_BASE_TRACING)
  EXPECT_EQ(GetDiscardableMemorySizeFromDump(*mem1, "dummy_dump_1"),
            kPageSize * 3 + 1);
#endif  // BUILDFLAG(ENABLE_BASE_TRACING)
  EXPECT_EQ(allocator_.GetBytesAllocated(), kPageSize * 3 + 1);

  // Allocate 3 pages of discardable memory, and free the previously allocated
  // pages.
  auto mem2 = AllocateLockedMadvFreeDiscardableMemory(kPageSize * 3);

  EXPECT_TRUE(mem2->IsLockedForTesting());
#if BUILDFLAG(ENABLE_BASE_TRACING)
  EXPECT_EQ(GetDiscardableMemorySizeFromDump(*mem2, "dummy_dump_2"),
            kPageSize * 3);
#endif  // BUILDFLAG(ENABLE_BASE_TRACING)
  EXPECT_EQ(allocator_.GetBytesAllocated(), kPageSize * 6 + 1);

  mem1.reset();

  EXPECT_EQ(allocator_.GetBytesAllocated(), kPageSize * 3);

  // Write to and read from an allocated discardable memory buffer.
  const char test_pattern[] = "ABCDEFGHIJKLMNOP";
  char buffer[sizeof(test_pattern)];

  void* data = mem2->data();
  memcpy(data, test_pattern, sizeof(test_pattern));

  data = mem2->data_as<uint8_t>();
  memcpy(buffer, data, sizeof(test_pattern));

  EXPECT_EQ(memcmp(test_pattern, buffer, sizeof(test_pattern)), 0);
}
}  // namespace base
