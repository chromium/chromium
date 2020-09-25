// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/thread_cache.h"

#include <atomic>
#include <vector>

#include "base/allocator/buildflags.h"
#include "base/allocator/partition_allocator/partition_alloc.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/synchronization/lock.h"
#include "base/test/bind_test_util.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

// Only a single partition can have a thread cache at a time. When
// PartitionAlloc is malloc(), it is already in use.
//
// With *SAN, PartitionAlloc is replaced in partition_alloc.h by ASAN, so we
// cannot test the thread cache.
//
// Finally, the thread cache currently uses `thread_local`, which causes issues
// on Windows 7 (at least). As long as it doesn't use something else on Windows,
// disable the cache (and tests)
#if !BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) && \
    !defined(MEMORY_TOOL_REPLACES_ALLOCATOR) && defined(OS_LINUX)

namespace base {
namespace internal {

namespace {

class LambdaThreadDelegate : public PlatformThread::Delegate {
 public:
  explicit LambdaThreadDelegate(OnceClosure f) : f_(std::move(f)) {}
  void ThreadMain() override { std::move(f_).Run(); }

 private:
  OnceClosure f_;
};

class DeltaCounter {
 public:
  explicit DeltaCounter(uint64_t& value)
      : current_value_(value), initial_value_(value) {}
  void Reset() { initial_value_ = current_value_; }
  uint64_t Delta() const { return current_value_ - initial_value_; }

 private:
  uint64_t& current_value_;
  uint64_t initial_value_;
};

// Need to be a global object without a destructor, because the cache is a
// global object with a destructor (to handle thread destruction), and the
// PartitionRoot has to outlive it.
//
// Forbid extras, since they make finding out which bucket is used harder.
NoDestructor<ThreadSafePartitionRoot> g_root{
    PartitionOptions{PartitionOptions::Alignment::kAlignedAlloc,
                     PartitionOptions::ThreadCache::kEnabled}};

size_t FillThreadCacheAndReturnIndex(size_t size, size_t count = 1) {
  uint16_t bucket_index = PartitionRoot<ThreadSafe>::SizeToBucketIndex(size);
  std::vector<void*> allocated_data;

  for (size_t i = 0; i < count; ++i) {
    allocated_data.push_back(g_root->Alloc(size, ""));
  }
  for (void* ptr : allocated_data) {
    g_root->Free(ptr);
  }

  return bucket_index;
}

}  // namespace

class ThreadCacheTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Make sure there is a thread cache.
    void* data = g_root->Alloc(1, "");
    g_root->Free(data);

    auto* tcache = g_root->thread_cache_for_testing();
    ASSERT_TRUE(tcache);
    tcache->Purge();
  }
  void TearDown() override {}
};

TEST_F(ThreadCacheTest, Simple) {
  const size_t kTestSize = 12;
  void* ptr = g_root->Alloc(kTestSize, "");
  ASSERT_TRUE(ptr);

  // There is a cache.
  auto* tcache = g_root->thread_cache_for_testing();
  EXPECT_TRUE(tcache);

  uint16_t index = PartitionRoot<ThreadSafe>::SizeToBucketIndex(kTestSize);
  EXPECT_EQ(0u, tcache->bucket_count_for_testing(index));

  g_root->Free(ptr);
  // Freeing fills the thread cache.
  EXPECT_EQ(1u, tcache->bucket_count_for_testing(index));

  void* ptr2 = g_root->Alloc(kTestSize, "");
  EXPECT_EQ(ptr, ptr2);
  // Allocated from the thread cache.
  EXPECT_EQ(0u, tcache->bucket_count_for_testing(index));
}

TEST_F(ThreadCacheTest, InexactSizeMatch) {
  const size_t kTestSize = 12;
  void* ptr = g_root->Alloc(kTestSize, "");
  ASSERT_TRUE(ptr);

  // There is a cache.
  auto* tcache = g_root->thread_cache_for_testing();
  EXPECT_TRUE(tcache);

  uint16_t index = PartitionRoot<ThreadSafe>::SizeToBucketIndex(kTestSize);
  EXPECT_EQ(0u, tcache->bucket_count_for_testing(index));

  g_root->Free(ptr);
  // Freeing fills the thread cache.
  EXPECT_EQ(1u, tcache->bucket_count_for_testing(index));

  void* ptr2 = g_root->Alloc(kTestSize + 1, "");
  EXPECT_EQ(ptr, ptr2);
  // Allocated from the thread cache.
  EXPECT_EQ(0u, tcache->bucket_count_for_testing(index));
}

TEST_F(ThreadCacheTest, MultipleObjectsCachedPerBucket) {
  size_t bucket_index = FillThreadCacheAndReturnIndex(100, 10);
  auto* tcache = g_root->thread_cache_for_testing();
  EXPECT_EQ(10u, tcache->bucket_count_for_testing(bucket_index));
}

TEST_F(ThreadCacheTest, ObjectsCachedCountIsLimited) {
  size_t bucket_index = FillThreadCacheAndReturnIndex(100, 1000);
  auto* tcache = g_root->thread_cache_for_testing();
  EXPECT_LT(tcache->bucket_count_for_testing(bucket_index), 1000u);
}

TEST_F(ThreadCacheTest, Purge) {
  size_t bucket_index = FillThreadCacheAndReturnIndex(100, 10);
  auto* tcache = g_root->thread_cache_for_testing();
  EXPECT_EQ(10u, tcache->bucket_count_for_testing(bucket_index));
  tcache->Purge();
  EXPECT_EQ(0u, tcache->bucket_count_for_testing(bucket_index));
}

TEST_F(ThreadCacheTest, NoCrossPartitionCache) {
  const size_t kTestSize = 12;
  ThreadSafePartitionRoot root{{PartitionOptions::Alignment::kAlignedAlloc,
                                PartitionOptions::ThreadCache::kDisabled}};

  size_t bucket_index = FillThreadCacheAndReturnIndex(kTestSize);
  void* ptr = root.Alloc(kTestSize, "");
  ASSERT_TRUE(ptr);

  auto* tcache = g_root->thread_cache_for_testing();
  EXPECT_EQ(1u, tcache->bucket_count_for_testing(bucket_index));

  ThreadSafePartitionRoot::Free(ptr);
  EXPECT_EQ(1u, tcache->bucket_count_for_testing(bucket_index));
}

#if defined(PA_ENABLE_THREAD_CACHE_STATISTICS)  // Required to record hits and
                                                // misses.
TEST_F(ThreadCacheTest, LargeAllocationsAreNotCached) {
  auto* tcache = g_root->thread_cache_for_testing();
  DeltaCounter alloc_miss_counter{tcache->stats_.alloc_misses};
  DeltaCounter alloc_miss_too_large_counter{
      tcache->stats_.alloc_miss_too_large};
  DeltaCounter cache_fill_counter{tcache->stats_.cache_fill_count};
  DeltaCounter cache_fill_misses_counter{tcache->stats_.cache_fill_misses};
  DeltaCounter cache_fill_too_large_counter{
      tcache->stats_.cache_fill_too_large};

  FillThreadCacheAndReturnIndex(100 * 1024);
  tcache = g_root->thread_cache_for_testing();
  EXPECT_EQ(1u, alloc_miss_counter.Delta());
  EXPECT_EQ(1u, alloc_miss_too_large_counter.Delta());
  EXPECT_EQ(1u, cache_fill_counter.Delta());
  EXPECT_EQ(1u, cache_fill_misses_counter.Delta());
  EXPECT_EQ(1u, cache_fill_too_large_counter.Delta());
}
#endif  // defined(PA_ENABLE_THREAD_CACHE_STATISTICS)

TEST_F(ThreadCacheTest, DirectMappedAllocationsAreNotCached) {
  FillThreadCacheAndReturnIndex(1024 * 1024);
  // The line above would crash due to out of bounds access if this wasn't
  // properly handled.
}

TEST_F(ThreadCacheTest, MultipleThreadCaches) {
  const size_t kTestSize = 100;
  FillThreadCacheAndReturnIndex(kTestSize);
  auto* parent_thread_tcache = g_root->thread_cache_for_testing();
  ASSERT_TRUE(parent_thread_tcache);

  LambdaThreadDelegate delegate{BindLambdaForTesting([&]() {
    EXPECT_FALSE(g_root->thread_cache_for_testing());  // No allocations yet.
    FillThreadCacheAndReturnIndex(kTestSize);
    auto* tcache = g_root->thread_cache_for_testing();
    EXPECT_TRUE(tcache);

    EXPECT_NE(parent_thread_tcache, tcache);
  })};

  PlatformThreadHandle thread_handle;
  PlatformThread::Create(0, &delegate, &thread_handle);
  PlatformThread::Join(thread_handle);
}

TEST_F(ThreadCacheTest, ThreadCacheReclaimedWhenThreadExits) {
  const size_t kTestSize = 100;
  // Make sure that there is always at least one object allocated in the test
  // bucket, so that the PartitionPage is no reclaimed.
  void* tmp = g_root->Alloc(kTestSize, "");
  void* other_thread_ptr;

  LambdaThreadDelegate delegate{BindLambdaForTesting([&]() {
    EXPECT_FALSE(g_root->thread_cache_for_testing());  // No allocations yet.
    other_thread_ptr = g_root->Alloc(kTestSize, "");
    g_root->Free(other_thread_ptr);
    // |other_thread_ptr| is now in the thread cache.
  })};

  PlatformThreadHandle thread_handle;
  PlatformThread::Create(0, &delegate, &thread_handle);
  PlatformThread::Join(thread_handle);

  void* this_thread_ptr = g_root->Alloc(kTestSize, "");
  // |other_thread_ptr| was returned to the central allocator, and is returned
  // |here, as is comes from the freelist.
  EXPECT_EQ(this_thread_ptr, other_thread_ptr);
  g_root->Free(other_thread_ptr);
  g_root->Free(tmp);
}

TEST_F(ThreadCacheTest, ThreadCacheRegistry) {
  const size_t kTestSize = 100;
  auto* parent_thread_tcache = g_root->thread_cache_for_testing();
  ASSERT_TRUE(parent_thread_tcache);

  LambdaThreadDelegate delegate{BindLambdaForTesting([&]() {
    EXPECT_FALSE(g_root->thread_cache_for_testing());  // No allocations yet.
    FillThreadCacheAndReturnIndex(kTestSize);
    auto* tcache = g_root->thread_cache_for_testing();
    EXPECT_TRUE(tcache);

    AutoLock lock(ThreadCacheRegistry::GetLock());
    EXPECT_EQ(tcache->prev_, nullptr);
    EXPECT_EQ(tcache->next_, parent_thread_tcache);
  })};

  PlatformThreadHandle thread_handle;
  PlatformThread::Create(0, &delegate, &thread_handle);
  PlatformThread::Join(thread_handle);

  AutoLock lock(ThreadCacheRegistry::GetLock());
  EXPECT_EQ(parent_thread_tcache->prev_, nullptr);
  EXPECT_EQ(parent_thread_tcache->next_, nullptr);
}

#if defined(PA_ENABLE_THREAD_CACHE_STATISTICS)
TEST_F(ThreadCacheTest, RecordStats) {
  const size_t kTestSize = 100;
  auto* tcache = g_root->thread_cache_for_testing();
  DeltaCounter alloc_counter{tcache->stats_.alloc_count};
  DeltaCounter alloc_hits_counter{tcache->stats_.alloc_hits};
  DeltaCounter alloc_miss_counter{tcache->stats_.alloc_misses};

  DeltaCounter alloc_miss_empty_counter{tcache->stats_.alloc_miss_empty};

  DeltaCounter cache_fill_counter{tcache->stats_.cache_fill_count};
  DeltaCounter cache_fill_hits_counter{tcache->stats_.cache_fill_hits};
  DeltaCounter cache_fill_misses_counter{tcache->stats_.cache_fill_misses};
  DeltaCounter cache_fill_bucket_full_counter{
      tcache->stats_.cache_fill_bucket_full};

  // Cache has been purged, first allocation is a miss.
  void* data = g_root->Alloc(kTestSize, "");
  EXPECT_EQ(1u, alloc_counter.Delta());
  EXPECT_EQ(1u, alloc_miss_counter.Delta());
  EXPECT_EQ(0u, alloc_hits_counter.Delta());

  // Cache fill worked.
  g_root->Free(data);
  EXPECT_EQ(1u, cache_fill_counter.Delta());
  EXPECT_EQ(1u, cache_fill_hits_counter.Delta());
  EXPECT_EQ(0u, cache_fill_misses_counter.Delta());

  tcache->Purge();
  cache_fill_counter.Reset();
  // Bucket full accounting.
  size_t bucket_index = FillThreadCacheAndReturnIndex(
      kTestSize, ThreadCache::kMaxCountPerBucket + 10);
  EXPECT_EQ(ThreadCache::kMaxCountPerBucket + 10, cache_fill_counter.Delta());
  EXPECT_EQ(10u, cache_fill_bucket_full_counter.Delta());
  EXPECT_EQ(10u, cache_fill_misses_counter.Delta());

  // Memory footprint.
  size_t allocated_size = g_root->buckets[bucket_index].slot_size;
  ThreadCacheStats stats;
  ThreadCacheRegistry::Instance().DumpStats(true, &stats);
  EXPECT_EQ(allocated_size * ThreadCache::kMaxCountPerBucket,
            stats.bucket_total_memory);
  EXPECT_EQ(sizeof(ThreadCache), stats.metadata_overhead);
}

TEST_F(ThreadCacheTest, MultipleThreadCachesAccounting) {
  const size_t kTestSize = 100;
  void* data = g_root->Alloc(kTestSize, "");
  g_root->Free(data);
  uint64_t alloc_count = g_root->thread_cache_for_testing()->stats_.alloc_count;

  LambdaThreadDelegate delegate{BindLambdaForTesting([&]() {
    EXPECT_FALSE(g_root->thread_cache_for_testing());  // No allocations yet.
    size_t bucket_index = FillThreadCacheAndReturnIndex(kTestSize);

    ThreadCacheStats stats;
    ThreadCacheRegistry::Instance().DumpStats(false, &stats);
    size_t allocated_size = g_root->buckets[bucket_index].slot_size;
    // 2* for this thread and the parent one.
    EXPECT_EQ(2 * allocated_size, stats.bucket_total_memory);
    EXPECT_EQ(2 * sizeof(ThreadCache), stats.metadata_overhead);

    uint64_t this_thread_alloc_count =
        g_root->thread_cache_for_testing()->stats_.alloc_count;
    EXPECT_EQ(alloc_count + this_thread_alloc_count, stats.alloc_count);
  })};

  PlatformThreadHandle thread_handle;
  PlatformThread::Create(0, &delegate, &thread_handle);
  PlatformThread::Join(thread_handle);
}

#endif  // defined(PA_ENABLE_THREAD_CACHE_STATISTICS)

TEST_F(ThreadCacheTest, PurgeAll) NO_THREAD_SAFETY_ANALYSIS {
  std::atomic<bool> other_thread_started{false};
  std::atomic<bool> purge_called{false};

  const size_t kTestSize = 100;
  size_t bucket_index = FillThreadCacheAndReturnIndex(kTestSize);
  ThreadCache* this_thread_tcache = g_root->thread_cache_for_testing();
  ThreadCache* other_thread_tcache = nullptr;

  LambdaThreadDelegate delegate{
      BindLambdaForTesting([&]() NO_THREAD_SAFETY_ANALYSIS {
        FillThreadCacheAndReturnIndex(kTestSize);
        other_thread_tcache = g_root->thread_cache_for_testing();

        other_thread_started.store(true, std::memory_order_release);
        while (!purge_called.load(std::memory_order_acquire)) {
        }

        // Purge() was not triggered from the other thread.
        EXPECT_EQ(1u,
                  other_thread_tcache->bucket_count_for_testing(bucket_index));
        // Allocations do not trigger Purge().
        void* data = g_root->Alloc(1, "");
        EXPECT_EQ(1u,
                  other_thread_tcache->bucket_count_for_testing(bucket_index));
        // But deallocations do.
        g_root->Free(data);
        EXPECT_EQ(0u,
                  other_thread_tcache->bucket_count_for_testing(bucket_index));
      })};

  PlatformThreadHandle thread_handle;
  PlatformThread::Create(0, &delegate, &thread_handle);

  while (!other_thread_started.load(std::memory_order_acquire)) {
  }

  EXPECT_EQ(1u, this_thread_tcache->bucket_count_for_testing(bucket_index));
  EXPECT_EQ(1u, other_thread_tcache->bucket_count_for_testing(bucket_index));

  ThreadCacheRegistry::Instance().PurgeAll();
  // This thread is synchronously purged.
  EXPECT_EQ(0u, this_thread_tcache->bucket_count_for_testing(bucket_index));
  // Not the other one.
  EXPECT_EQ(1u, other_thread_tcache->bucket_count_for_testing(bucket_index));

  purge_called.store(true, std::memory_order_release);
  PlatformThread::Join(thread_handle);
}

}  // namespace internal
}  // namespace base

#endif  // !BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) &&
        // !defined(MEMORY_TOOL_REPLACES_ALLOCATOR) && defined(OS_LINUX)
