// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fcntl.h>
#include <stdint.h>

#include <sys/mman.h>
#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/files/scoped_file.h"
#include "base/memory/madv_free_discardable_memory_allocator_posix.h"
#include "base/memory/madv_free_discardable_memory_posix.h"
#include "base/process/process_metrics.h"
#include "base/trace_event/memory_allocator_dump.h"
#include "base/trace_event/process_memory_dump.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

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
std::atomic<size_t> allocator_byte_count;
class MadvFreeDiscardableMemoryPosixTester
    : public MadvFreeDiscardableMemoryPosix {
 public:
  MadvFreeDiscardableMemoryPosixTester(size_t size_in_bytes)
      : MadvFreeDiscardableMemoryPosix(size_in_bytes, &allocator_byte_count) {}

  using MadvFreeDiscardableMemoryPosix::DiscardPage;
  using MadvFreeDiscardableMemoryPosix::GetPageCount;
  using MadvFreeDiscardableMemoryPosix::IsLockedForTesting;
  using MadvFreeDiscardableMemoryPosix::IsValid;
  using MadvFreeDiscardableMemoryPosix::SetKeepMemoryForTesting;
};

class MadvFreeDiscardableMemoryTest : public ::testing::Test {
 protected:
  MadvFreeDiscardableMemoryTest() {}
  ~MadvFreeDiscardableMemoryTest() override {}

  const size_t kPageSize = base::GetPageSize();

  std::unique_ptr<MadvFreeDiscardableMemoryPosixTester>
  AllocateLockedDiscardableMemoryPagesForTest(size_t size_in_pages) {
    return std::make_unique<MadvFreeDiscardableMemoryPosixTester>(
        size_in_pages * kPageSize);
  }
};

using MadvFreeDiscardableMemoryDeathTest = MadvFreeDiscardableMemoryTest;

constexpr char kTestPattern[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";

TEST_F(MadvFreeDiscardableMemoryTest, AllocateAndUse) {
  SUCCEED_IF_MADV_FREE_UNSUPPORTED();

  std::unique_ptr<MadvFreeDiscardableMemoryPosixTester> mem =
      AllocateLockedDiscardableMemoryPagesForTest(1);

  mem->SetKeepMemoryForTesting(true);

  ASSERT_TRUE(mem->IsValid());
  ASSERT_TRUE(mem->IsLockedForTesting());

  char buffer[sizeof(kTestPattern)];

  // Write test pattern to block
  uint8_t* data = mem->data_as<uint8_t>();
  memcpy(data, kTestPattern, sizeof(kTestPattern));

  // Read test pattern from block
  data = mem->data_as<uint8_t>();
  memcpy(buffer, data, sizeof(kTestPattern));

  EXPECT_EQ(memcmp(kTestPattern, buffer, sizeof(kTestPattern)), 0);

  // Memory contents should not change after successful unlock and lock.
  mem->Unlock();
  ASSERT_TRUE(mem->Lock());

  EXPECT_EQ(memcmp(kTestPattern, buffer, sizeof(kTestPattern)), 0);
}

TEST_F(MadvFreeDiscardableMemoryTest, LockAndUnlock) {
  SUCCEED_IF_MADV_FREE_UNSUPPORTED();

  const size_t kPageCount = 10;
  std::unique_ptr<MadvFreeDiscardableMemoryPosixTester> mem =
      AllocateLockedDiscardableMemoryPagesForTest(kPageCount);

  ASSERT_TRUE(mem->IsValid());
  ASSERT_TRUE(mem->IsLockedForTesting());
  memset(mem->data(), 0xE7, kPageSize * kPageCount);
  mem->Unlock();
  ASSERT_FALSE(mem->IsLockedForTesting());
  bool result = mem->Lock();
  // If Lock() succeeded, the memory region should be valid. If Lock() failed,
  // the memory region should be invalid.
  ASSERT_EQ(result, mem->IsValid());
}

TEST_F(MadvFreeDiscardableMemoryTest, LockShouldFailAfterDiscard) {
  SUCCEED_IF_MADV_FREE_UNSUPPORTED();

  constexpr size_t kPageCount = 10;

  std::unique_ptr<MadvFreeDiscardableMemoryPosixTester> mem =
      AllocateLockedDiscardableMemoryPagesForTest(kPageCount);
  uint8_t* data = mem->data_as<uint8_t>();

  ASSERT_TRUE(mem->IsValid());
  ASSERT_TRUE(mem->IsLockedForTesting());
  // Modify block data such that at least one page is non-zero.
  memset(data, 0xff, kPageSize * kPageCount);

  mem->Unlock();
  ASSERT_FALSE(mem->IsLockedForTesting());
  // Forcefully discard at least one non-zero page.
  mem->DiscardPage(5);

  // Locking when a page has been discarded should fail.
  ASSERT_FALSE(mem->Lock());
  // Locking after memory is deallocated should fail.
  ASSERT_FALSE(mem->Lock());

  // Check that memory has been deallocated.
  ASSERT_FALSE(mem->IsValid());
}

#ifdef GTEST_HAS_DEATH_TEST
// These tests verify that additional memory protection checks are working
// properly.
#if DCHECK_IS_ON()

TEST_F(MadvFreeDiscardableMemoryDeathTest, ReadWriteShouldFailIfUnlocked) {
  SUCCEED_IF_MADV_FREE_UNSUPPORTED();

  const size_t kPageCount = 10;

  std::unique_ptr<MadvFreeDiscardableMemoryPosixTester> mem =
      AllocateLockedDiscardableMemoryPagesForTest(kPageCount);

  mem->SetKeepMemoryForTesting(true);

  memset(mem->data(), 0xee, kPageCount * kPageSize);

  mem->Unlock();
  ASSERT_FALSE(mem->IsLockedForTesting());

  // Write to unlocked memory should fail.
  ASSERT_DEATH({ memset(mem->data(), 0x11, kPageCount * kPageSize); }, ".*");

  int result;
  // Read from unlocked memory should fail.
  ASSERT_DEATH(
      { result = memcmp(mem->data(), mem->data(), kPageCount * kPageSize); },
      ".*");

  ASSERT_TRUE(mem->Lock());
  ASSERT_TRUE(mem->IsLockedForTesting());
  // Write to memory after re-locking should not fail.
  memset(mem->data(), 0xaa, kPageCount * kPageSize);
  // Read from memory after re-locking should not fail.
  result = memcmp(mem->data(), mem->data(), kPageCount * kPageSize);
}

#endif  // DCHECK_IS_ON()
#endif  // GTEST_HAS_DEATH_TEST

}  // namespace base
