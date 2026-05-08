// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/discardable_shared_memory.h"

#include <fcntl.h>
#include <stdint.h>

#include <algorithm>

#include "base/files/scoped_file.h"
#include "base/memory/page_size.h"
#include "base/memory/shared_memory_tracker.h"
#include "base/test/task_environment.h"
#include "base/trace_event/memory_allocator_dump.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/tracing_buildflags.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

// On 32-bit architectures, DiscardableSharedMemory truncates timestamps to
// whole-second precision to fit the shared state into a 32-bit atomic.
// This helper snaps the mock clock to a whole-second boundary to ensure
// that Time::Now() has no sub-second components, preventing mismatches
// when comparing captured timestamps with values from shared memory.
void MaybeSnapMockClockToNextWholeSecond(
    test::TaskEnvironment& task_environment) {
#if defined(ARCH_CPU_32_BITS)
  Time now = Time::Now();
  Time rounded =
      Time::UnixEpoch() + Seconds((now - Time::UnixEpoch()).InSeconds() + 1);
  task_environment.AdvanceClock(rounded - now);
#endif
}

}  // namespace

TEST(DiscardableSharedMemoryTest, CreateAndMap) {
  const uint32_t kDataSize = 1024;

  DiscardableSharedMemory memory;
  bool rv = memory.CreateAndMap(kDataSize);
  ASSERT_TRUE(rv);
  EXPECT_GE(memory.mapped_size(), kDataSize);
  EXPECT_TRUE(memory.IsMemoryLocked());
}

TEST(DiscardableSharedMemoryTest, CreateFromHandle) {
  const uint32_t kDataSize = 1024;

  DiscardableSharedMemory memory1;
  bool rv = memory1.CreateAndMap(kDataSize);
  ASSERT_TRUE(rv);

  UnsafeSharedMemoryRegion shared_region = memory1.DuplicateRegion();
  ASSERT_TRUE(shared_region.IsValid());

  DiscardableSharedMemory memory2(std::move(shared_region));
  rv = memory2.Map(kDataSize);
  ASSERT_TRUE(rv);
  EXPECT_TRUE(memory2.IsMemoryLocked());
}

TEST(DiscardableSharedMemoryTest, LockAndUnlock) {
  test::TaskEnvironment task_environment{
      test::TaskEnvironment::TimeSource::MOCK_TIME};
  const uint32_t kDataSize = 1024;

  DiscardableSharedMemory memory1;
  bool rv = memory1.CreateAndMap(kDataSize);
  ASSERT_TRUE(rv);

  // Memory is initially locked. Unlock it.
  task_environment.FastForwardBy(Seconds(1));
  memory1.Unlock(0, 0);
  EXPECT_FALSE(memory1.IsMemoryLocked());

  // Lock and unlock memory.
  DiscardableSharedMemory::LockResult lock_rv = memory1.Lock(0, 0);
  EXPECT_EQ(DiscardableSharedMemory::SUCCESS, lock_rv);
  task_environment.FastForwardBy(Seconds(1));
  memory1.Unlock(0, 0);

  // Lock again before duplicating and passing ownership to new instance.
  lock_rv = memory1.Lock(0, 0);
  EXPECT_EQ(DiscardableSharedMemory::SUCCESS, lock_rv);
  EXPECT_TRUE(memory1.IsMemoryLocked());

  UnsafeSharedMemoryRegion shared_region = memory1.DuplicateRegion();
  ASSERT_TRUE(shared_region.IsValid());

  DiscardableSharedMemory memory2(std::move(shared_region));
  rv = memory2.Map(kDataSize);
  ASSERT_TRUE(rv);

  // Unlock second instance.
  task_environment.FastForwardBy(Seconds(1));
  memory2.Unlock(0, 0);

  // Both memory instances should be unlocked now.
  EXPECT_FALSE(memory2.IsMemoryLocked());
  EXPECT_FALSE(memory1.IsMemoryLocked());

  // Lock second instance before passing ownership back to first instance.
  lock_rv = memory2.Lock(0, 0);
  EXPECT_EQ(DiscardableSharedMemory::SUCCESS, lock_rv);

  // Memory should still be resident and locked.
  rv = memory1.IsMemoryResident();
  EXPECT_TRUE(rv);
  EXPECT_TRUE(memory1.IsMemoryLocked());

  // Unlock first instance.
  task_environment.FastForwardBy(Seconds(1));
  memory1.Unlock(0, 0);
}

TEST(DiscardableSharedMemoryTest, Purge) {
  test::TaskEnvironment task_environment{
      test::TaskEnvironment::TimeSource::MOCK_TIME};
  const uint32_t kDataSize = 1024;

  DiscardableSharedMemory memory1;
  bool rv = memory1.CreateAndMap(kDataSize);
  ASSERT_TRUE(rv);

  UnsafeSharedMemoryRegion shared_region = memory1.DuplicateRegion();
  ASSERT_TRUE(shared_region.IsValid());

  DiscardableSharedMemory memory2(std::move(shared_region));
  rv = memory2.Map(kDataSize);
  ASSERT_TRUE(rv);

  // This should fail as memory is locked.
  rv = memory1.Purge(Time::Now() + Seconds(1));
  EXPECT_FALSE(rv);

  task_environment.FastForwardBy(Seconds(2));
  memory2.Unlock(0, 0);

  ASSERT_TRUE(memory2.IsMemoryResident());

  // Memory is unlocked, but our usage timestamp is incorrect.
  rv = memory1.Purge(Time::Now() + Seconds(1));
  EXPECT_FALSE(rv);

  ASSERT_TRUE(memory2.IsMemoryResident());

  // Memory is unlocked and our usage timestamp should be correct.
  rv = memory1.Purge(Time::Now() + Seconds(2));
  EXPECT_TRUE(rv);

  // Lock should fail as memory has been purged.
  DiscardableSharedMemory::LockResult lock_rv = memory2.Lock(0, 0);
  EXPECT_EQ(DiscardableSharedMemory::FAILED, lock_rv);

  ASSERT_FALSE(memory2.IsMemoryResident());
}

TEST(DiscardableSharedMemoryTest, PurgeAfterClose) {
  test::TaskEnvironment task_environment{
      test::TaskEnvironment::TimeSource::MOCK_TIME};
  const uint32_t kDataSize = 1024;

  DiscardableSharedMemory memory;
  bool rv = memory.CreateAndMap(kDataSize);
  ASSERT_TRUE(rv);

  // Unlock things so we can Purge().
  task_environment.FastForwardBy(Seconds(2));
  memory.Unlock(0, 0);

  // It should be safe to Purge() |memory| after Close()ing the handle.
  memory.Close();
  rv = memory.Purge(Time::Now() + Seconds(2));
  EXPECT_TRUE(rv);
}

TEST(DiscardableSharedMemoryTest, LastUsed) {
  test::TaskEnvironment task_environment{
      test::TaskEnvironment::TimeSource::MOCK_TIME};
  MaybeSnapMockClockToNextWholeSecond(task_environment);
  const uint32_t kDataSize = 1024;

  DiscardableSharedMemory memory1;
  bool rv = memory1.CreateAndMap(kDataSize);
  ASSERT_TRUE(rv);

  UnsafeSharedMemoryRegion shared_region = memory1.DuplicateRegion();
  ASSERT_TRUE(shared_region.IsValid());

  DiscardableSharedMemory memory2(std::move(shared_region));
  rv = memory2.Map(kDataSize);
  ASSERT_TRUE(rv);

  task_environment.FastForwardBy(Seconds(1));
  Time time1 = Time::Now();
  memory2.Unlock(0, 0);

  EXPECT_EQ(memory2.last_known_usage(), time1);

  DiscardableSharedMemory::LockResult lock_rv = memory2.Lock(0, 0);
  EXPECT_EQ(DiscardableSharedMemory::SUCCESS, lock_rv);

  // This should fail as memory is locked. This failed purge attempt updates
  // memory1's local last_known_usage to time2.
  Time time2 = Time::Now() + Seconds(1);
  rv = memory1.Purge(time2);
  ASSERT_FALSE(rv);

  // Last usage should have been updated to timestamp passed to Purge above.
  EXPECT_EQ(memory1.last_known_usage(), time2);

  // Advance time by more than the 1s offset used above. This ensures that the
  // actual unlock time (time3) is different from memory1's local timestamp
  // (time2), so memory1's next purge attempt will fail with an out-of-sync
  // timestamp.
  task_environment.FastForwardBy(Seconds(2));
  Time time3 = Time::Now();
  memory2.Unlock(0, 0);

  // Usage time should be correct for |memory2| instance.
  EXPECT_EQ(memory2.last_known_usage(), time3);

  // However, usage time has not changed as far as |memory1| instance knows.
  EXPECT_EQ(memory1.last_known_usage(), time2);

  // Memory is unlocked, but our usage timestamp is incorrect.
  Time time4 = Time::Now() + Seconds(1);
  rv = memory1.Purge(time4);
  EXPECT_FALSE(rv);

  // The failed purge attempt should have updated usage time to the correct
  // value.
  EXPECT_EQ(memory1.last_known_usage(), time3);

  // Purge memory through |memory2| instance. The last usage time should be
  // set to 0 as a result of this.
  Time time5 = Time::Now() + Seconds(2);
  rv = memory2.Purge(time5);
  EXPECT_TRUE(rv);
  EXPECT_TRUE(memory2.last_known_usage().is_null());

  // This should fail as memory has already been purged and |memory1|'s usage
  // time is incorrect as a result.
  Time time6 = Time::Now() + Seconds(3);
  rv = memory1.Purge(time6);
  EXPECT_FALSE(rv);

  // The failed purge attempt should have updated usage time to the correct
  // value.
  EXPECT_TRUE(memory1.last_known_usage().is_null());

  // Purge should succeed now that usage time is correct.
  Time time7 = Time::Now() + Seconds(4);
  rv = memory1.Purge(time7);
  EXPECT_TRUE(rv);
}

TEST(DiscardableSharedMemoryTest, LockShouldAlwaysFailAfterSuccessfulPurge) {
  test::TaskEnvironment task_environment{
      test::TaskEnvironment::TimeSource::MOCK_TIME};
  const uint32_t kDataSize = 1024;

  DiscardableSharedMemory memory1;
  bool rv = memory1.CreateAndMap(kDataSize);
  ASSERT_TRUE(rv);

  UnsafeSharedMemoryRegion shared_region = memory1.DuplicateRegion();
  ASSERT_TRUE(shared_region.IsValid());

  DiscardableSharedMemory memory2(std::move(shared_region));
  rv = memory2.Map(kDataSize);
  ASSERT_TRUE(rv);

  task_environment.FastForwardBy(Seconds(1));
  memory2.Unlock(0, 0);

  rv = memory2.Purge(Time::Now() + Seconds(1));
  EXPECT_TRUE(rv);

  // Lock should fail as memory has been purged.
  DiscardableSharedMemory::LockResult lock_rv = memory2.Lock(0, 0);
  EXPECT_EQ(DiscardableSharedMemory::FAILED, lock_rv);
}

#if BUILDFLAG(IS_ANDROID)
TEST(DiscardableSharedMemoryTest, LockShouldFailIfPlatformLockPagesFails) {
  const uint32_t kDataSize = 1024;

  // This test cannot succeed on devices without a proper ashmem device
  // because Lock() will always succeed.
  if (!DiscardableSharedMemory::IsAshmemDeviceSupportedForTesting()) {
    return;
  }

  DiscardableSharedMemory memory1;
  bool rv1 = memory1.CreateAndMap(kDataSize);
  ASSERT_TRUE(rv1);

  base::UnsafeSharedMemoryRegion region = memory1.DuplicateRegion();
  int fd = region.GetPlatformHandle();
  DiscardableSharedMemory memory2(std::move(region));
  bool rv2 = memory2.Map(kDataSize);
  ASSERT_TRUE(rv2);

  // Unlock() the first page of memory, so we can test Lock()ing it.
  memory2.Unlock(0, base::GetPageSize());
  // To cause ashmem_pin_region() to fail, we arrange for it to be called with
  // an invalid file-descriptor, which requires a valid-looking fd (i.e. we
  // can't just Close() |memory|), but one on which the operation is invalid.
  // We can overwrite the |memory| fd with a handle to a different file using
  // dup2(), which has the nice properties that |memory| still has a valid fd
  // that it can close, etc without errors, but on which ashmem_pin_region()
  // will fail.
  base::ScopedFD null(open("/dev/null", O_RDONLY));
  ASSERT_EQ(fd, dup2(null.get(), fd));

  // Now re-Lock()ing the first page should fail.
  DiscardableSharedMemory::LockResult lock_rv =
      memory2.Lock(0, base::GetPageSize());
  EXPECT_EQ(DiscardableSharedMemory::FAILED, lock_rv);
}
#endif  // BUILDFLAG(IS_ANDROID)

TEST(DiscardableSharedMemoryTest, LockAndUnlockRange) {
  test::TaskEnvironment task_environment{
      test::TaskEnvironment::TimeSource::MOCK_TIME};
  MaybeSnapMockClockToNextWholeSecond(task_environment);
  const size_t kDataSize = 32;

  size_t data_size_in_bytes = kDataSize * base::GetPageSize();

  DiscardableSharedMemory memory1;
  bool rv = memory1.CreateAndMap(data_size_in_bytes);
  ASSERT_TRUE(rv);

  UnsafeSharedMemoryRegion shared_region = memory1.DuplicateRegion();
  ASSERT_TRUE(shared_region.IsValid());

  DiscardableSharedMemory memory2(std::move(shared_region));
  rv = memory2.Map(data_size_in_bytes);
  ASSERT_TRUE(rv);

  // Unlock first page.
  task_environment.FastForwardBy(Seconds(2));
  memory2.Unlock(0, base::GetPageSize());

  rv = memory1.Purge(Time::Now() + Seconds(1));
  EXPECT_FALSE(rv);

  // Lock first page again.
  task_environment.FastForwardBy(Seconds(2));
  DiscardableSharedMemory::LockResult lock_rv =
      memory2.Lock(0, base::GetPageSize());
  EXPECT_NE(DiscardableSharedMemory::FAILED, lock_rv);

  // Unlock first page.
  task_environment.FastForwardBy(Seconds(2));
  memory2.Unlock(0, base::GetPageSize());

  rv = memory1.Purge(Time::Now() + Seconds(1));
  EXPECT_FALSE(rv);

  // Unlock second page.
  task_environment.FastForwardBy(Seconds(2));
  memory2.Unlock(base::GetPageSize(), base::GetPageSize());

  rv = memory1.Purge(Time::Now() + Seconds(1));
  EXPECT_FALSE(rv);

  // Unlock anything onwards.
  // Advance time by more than the 1s offset used in the previous failed
  // Purge() call. This ensures memory1's local timestamp stays out of sync
  // with the shared state after this unlock.
  task_environment.FastForwardBy(Seconds(2));
  Time time8 = Time::Now();
  memory2.Unlock(2 * base::GetPageSize(), 0);

  // Memory is unlocked, but our usage timestamp is incorrect.
  rv = memory1.Purge(Time::Now() + Seconds(1));
  EXPECT_FALSE(rv);

  // The failed purge attempt should have updated usage time to the correct
  // value.
  EXPECT_EQ(time8, memory1.last_known_usage());

  // Purge should now succeed.
  rv = memory1.Purge(Time::Now() + Seconds(2));
  EXPECT_TRUE(rv);
}

TEST(DiscardableSharedMemoryTest, MappedSize) {
  const uint32_t kDataSize = 1024;

  DiscardableSharedMemory memory;
  bool rv = memory.CreateAndMap(kDataSize);
  ASSERT_TRUE(rv);

  EXPECT_LE(kDataSize, memory.mapped_size());

  // Mapped size should be 0 after memory segment has been unmapped.
  rv = memory.Unmap();
  EXPECT_TRUE(rv);
  EXPECT_EQ(0u, memory.mapped_size());
}

TEST(DiscardableSharedMemoryTest, Close) {
  test::TaskEnvironment task_environment{
      test::TaskEnvironment::TimeSource::MOCK_TIME};
  const uint32_t kDataSize = 1024;

  DiscardableSharedMemory memory;
  bool rv = memory.CreateAndMap(kDataSize);
  ASSERT_TRUE(rv);

  // Mapped size should be unchanged after memory segment has been closed.
  memory.Close();
  EXPECT_LE(kDataSize, memory.mapped_size());

  // Memory is initially locked. Unlock it.
  task_environment.FastForwardBy(Seconds(1));
  memory.Unlock(0, 0);

  // Lock and unlock memory.
  DiscardableSharedMemory::LockResult lock_rv = memory.Lock(0, 0);
  EXPECT_EQ(DiscardableSharedMemory::SUCCESS, lock_rv);
  task_environment.FastForwardBy(Seconds(1));
  memory.Unlock(0, 0);
}

TEST(DiscardableSharedMemoryTest, ZeroSize) {
  test::TaskEnvironment task_environment{
      test::TaskEnvironment::TimeSource::MOCK_TIME};
  DiscardableSharedMemory memory;
  bool rv = memory.CreateAndMap(0);
  ASSERT_TRUE(rv);

  EXPECT_LE(0u, memory.mapped_size());

  // Memory is initially locked. Unlock it.
  task_environment.FastForwardBy(Seconds(1));
  memory.Unlock(0, 0);

  // Lock and unlock memory.
  DiscardableSharedMemory::LockResult lock_rv = memory.Lock(0, 0);
  EXPECT_NE(DiscardableSharedMemory::FAILED, lock_rv);
  task_environment.FastForwardBy(Seconds(1));
  memory.Unlock(0, 0);
}

// This test checks that zero-filled pages are returned after purging a segment
// when DISCARDABLE_SHARED_MEMORY_ZERO_FILL_ON_DEMAND_PAGES_AFTER_PURGE is
// defined and MADV_REMOVE is supported.
#if defined(DISCARDABLE_SHARED_MEMORY_ZERO_FILL_ON_DEMAND_PAGES_AFTER_PURGE)
TEST(DiscardableSharedMemoryTest, ZeroFilledPagesAfterPurge) {
  test::TaskEnvironment task_environment{
      test::TaskEnvironment::TimeSource::MOCK_TIME};
  const uint32_t kDataSize = 1024;

  DiscardableSharedMemory memory1;
  bool rv = memory1.CreateAndMap(kDataSize);
  ASSERT_TRUE(rv);

  UnsafeSharedMemoryRegion shared_region = memory1.DuplicateRegion();
  ASSERT_TRUE(shared_region.IsValid());

  DiscardableSharedMemory memory2(std::move(shared_region));
  rv = memory2.Map(kDataSize);
  ASSERT_TRUE(rv);

  // Initialize all memory to '0xaa'.
  std::ranges::fill(memory2.memory(), 0xaa);

  // Unlock memory.
  task_environment.FastForwardBy(Seconds(1));
  memory2.Unlock(0, 0);
  EXPECT_FALSE(memory1.IsMemoryLocked());

  // Memory is unlocked, but our usage timestamp is incorrect.
  rv = memory1.Purge(Time::Now() + Seconds(1));
  EXPECT_FALSE(rv);
  rv = memory1.Purge(Time::Now() + Seconds(2));
  EXPECT_TRUE(rv);

  // Check that reading memory after it has been purged is returning
  // zero-filled pages.
  uint8_t expected_data[kDataSize] = {};
  EXPECT_EQ(base::span(expected_data), memory2.memory());
}
#endif

TEST(DiscardableSharedMemoryTest, TracingOwnershipEdges) {
  const uint32_t kDataSize = 1024;
  DiscardableSharedMemory memory1;
  bool rv = memory1.CreateAndMap(kDataSize);
  ASSERT_TRUE(rv);

  base::trace_event::MemoryDumpArgs args = {
      base::trace_event::MemoryDumpLevelOfDetail::kDetailed};
  trace_event::ProcessMemoryDump pmd(args);
  trace_event::MemoryAllocatorDump* client_dump =
      pmd.CreateAllocatorDump("discardable_manager/map1");
  const bool is_owned = false;
  memory1.CreateSharedMemoryOwnershipEdge(client_dump, &pmd, is_owned);
  const auto* shm_dump = pmd.GetAllocatorDump(
      SharedMemoryTracker::GetDumpNameForTracing(memory1.mapped_id()));
  EXPECT_TRUE(shm_dump);
  EXPECT_EQ(shm_dump->GetSizeInternal(), client_dump->GetSizeInternal());
  const auto edges = pmd.allocator_dumps_edges();
  EXPECT_EQ(2u, edges.size());
  EXPECT_NE(edges.end(), edges.find(shm_dump->guid()));
  EXPECT_NE(edges.end(), edges.find(client_dump->guid()));
  // TODO(ssid): test for weak global dump once the
  // CreateWeakSharedMemoryOwnershipEdge() is fixed, crbug.com/661257.
}

}  // namespace base
