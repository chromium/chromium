// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/thread_cache.h"

#include <algorithm>
#include <atomic>
#include <vector>

#include "base/allocator/buildflags.h"
#include "base/allocator/partition_allocator/extended_api.h"
#include "base/allocator/partition_allocator/partition_address_space.h"
#include "base/allocator/partition_allocator/partition_alloc.h"
#include "base/allocator/partition_allocator/partition_lock.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/test/bind.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

// With *SAN, PartitionAlloc is replaced in partition_alloc.h by ASAN, so we
// cannot test the thread cache.
//
// Finally, the thread cache is not supported on all platforms.
#if !defined(MEMORY_TOOL_REPLACES_ALLOCATOR) && \
    defined(PA_THREAD_CACHE_SUPPORTED)

namespace base {
namespace internal {

namespace {

constexpr size_t kSmallSize = 12;
constexpr size_t kDefaultCountForSmallBucket =
    ThreadCache::kSmallBucketBaseCount * ThreadCache::kDefaultMultiplier;
constexpr size_t kFillCountForSmallBucket =
    kDefaultCountForSmallBucket / ThreadCache::kBatchFillRatio;

constexpr size_t kMediumSize = 200;
constexpr size_t kDefaultCountForMediumBucket = kDefaultCountForSmallBucket / 2;
constexpr size_t kFillCountForMediumBucket =
    kDefaultCountForMediumBucket / ThreadCache::kBatchFillRatio;

static_assert(kMediumSize <= ThreadCache::kDefaultSizeThreshold, "");

class LambdaThreadDelegate : public PlatformThread::Delegate {
 public:
  explicit LambdaThreadDelegate(RepeatingClosure f) : f_(std::move(f)) {}
  void ThreadMain() override { f_.Run(); }

 private:
  RepeatingClosure f_;
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

// Forbid extras, since they make finding out which bucket is used harder.
ThreadSafePartitionRoot* CreatePartitionRoot() {
  ThreadSafePartitionRoot* root = new ThreadSafePartitionRoot(PartitionOptions {
    PartitionOptions::AlignedAlloc::kAllowed,
#if !BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
        PartitionOptions::ThreadCache::kEnabled,
#else
        PartitionOptions::ThreadCache::kDisabled,
#endif  // BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
        PartitionOptions::Quarantine::kAllowed,
        PartitionOptions::Cookie::kDisallowed,
        PartitionOptions::BackupRefPtr::kDisabled,
        PartitionOptions::UseConfigurablePool::kNo,
        PartitionOptions::LazyCommit::kEnabled
  });

  root->UncapEmptySlotSpanMemoryForTesting();

  // We do this here instead of in SetUp()/TearDown() because we need this to
  // run before the task environment (which creates threads and hence is racy
  // with attempting to disable the thread cache).
  SwapOutProcessThreadCacheForTesting(root);

  return root;
}

OnceClosure g_purge_task;

void DelayedAction(OnceClosure task, base::TimeDelta delay) {
  // Need to invoke purge_action manually.
  g_purge_task = std::move(task);
}

void PurgeManually() {
  std::move(g_purge_task).Run();
}

}  // namespace

class PartitionAllocThreadCacheTest : public ::testing::Test {
 public:
  PartitionAllocThreadCacheTest() : root_(CreatePartitionRoot()) {}

  ~PartitionAllocThreadCacheTest() override {
    ThreadCache::SetLargestCachedSize(ThreadCache::kDefaultSizeThreshold);
    SwapInProcessThreadCacheForTesting(root_);

    ThreadSafePartitionRoot::DeleteForTesting(root_);

    // Cleanup the global state so next test can recreate ThreadCache.
    if (internal::ThreadCache::IsTombstone(internal::ThreadCache::Get()))
      internal::ThreadCache::RemoveTombstoneForTesting();
  }

 protected:
  void SetUp() override {
#if defined(PA_HAS_64_BITS_POINTERS)
    // Another test can uninitialize the pools, so make sure they are
    // initialized.
    PartitionAddressSpace::Init();
#endif  // defined(PA_HAS_64_BITS_POINTERS)
    ThreadCacheRegistry::Instance().SetThreadCacheMultiplier(
        ThreadCache::kDefaultMultiplier);
    ThreadCache::SetLargestCachedSize(ThreadCache::kLargeSizeThreshold);

    // Make sure that enough slot spans have been touched, otherwise cache fill
    // becomes unpredictable (because it doesn't take slow paths in the
    // allocator), which is an issue for tests.
    FillThreadCacheAndReturnIndex(kSmallSize, 1000);
    FillThreadCacheAndReturnIndex(kMediumSize, 1000);

    // There are allocations, a thread cache is created.
    auto* tcache = root_->thread_cache_for_testing();
    ASSERT_TRUE(tcache);

    ThreadCacheRegistry::Instance().ResetForTesting();
    tcache->ResetForTesting();
    g_purge_task = base::OnceClosure();
  }

  size_t FillThreadCacheAndReturnIndex(size_t size, size_t count = 1) {
    uint16_t bucket_index = PartitionRoot<ThreadSafe>::SizeToBucketIndex(size);
    std::vector<void*> allocated_data;

    for (size_t i = 0; i < count; ++i) {
      allocated_data.push_back(root_->Alloc(size, ""));
    }
    for (void* ptr : allocated_data) {
      root_->Free(ptr);
    }

    return bucket_index;
  }

  void FillThreadCacheWithMemory(size_t target_cached_memory) {
    for (int batch : {1, 2, 4, 8, 16}) {
      for (size_t allocation_size = 1;
           allocation_size <= ThreadCache::kLargeSizeThreshold;
           allocation_size++) {
        FillThreadCacheAndReturnIndex(allocation_size, batch);

        if (ThreadCache::Get()->CachedMemory() >= target_cached_memory)
          return;
      }
    }

    ASSERT_GE(ThreadCache::Get()->CachedMemory(), target_cached_memory);
  }

  ThreadSafePartitionRoot* root_;
};

TEST_F(PartitionAllocThreadCacheTest, Simple) {
  // There is a cache.
  auto* tcache = root_->thread_cache_for_testing();
  EXPECT_TRUE(tcache);
  DeltaCounter batch_fill_counter{tcache->stats_.batch_fill_count};

  void* ptr = root_->Alloc(kSmallSize, "");
  ASSERT_TRUE(ptr);

  uint16_t index = PartitionRoot<ThreadSafe>::SizeToBucketIndex(kSmallSize);
  EXPECT_EQ(kFillCountForSmallBucket - 1,
            tcache->bucket_count_for_testing(index));

  root_->Free(ptr);
  // Freeing fills the thread cache.
  EXPECT_EQ(kFillCountForSmallBucket, tcache->bucket_count_for_testing(index));

  void* ptr2 = root_->Alloc(kSmallSize, "");
  EXPECT_EQ(memory::UnmaskPtr(ptr), memory::UnmaskPtr(ptr2));
  // Allocated from the thread cache.
  EXPECT_EQ(kFillCountForSmallBucket - 1,
            tcache->bucket_count_for_testing(index));

  EXPECT_EQ(1u, batch_fill_counter.Delta());

  root_->Free(ptr2);
}

TEST_F(PartitionAllocThreadCacheTest, InexactSizeMatch) {
  void* ptr = root_->Alloc(kSmallSize, "");
  ASSERT_TRUE(ptr);

  // There is a cache.
  auto* tcache = root_->thread_cache_for_testing();
  EXPECT_TRUE(tcache);

  uint16_t index = PartitionRoot<ThreadSafe>::SizeToBucketIndex(kSmallSize);
  EXPECT_EQ(kFillCountForSmallBucket - 1,
            tcache->bucket_count_for_testing(index));

  root_->Free(ptr);
  // Freeing fills the thread cache.
  EXPECT_EQ(kFillCountForSmallBucket, tcache->bucket_count_for_testing(index));

  void* ptr2 = root_->Alloc(kSmallSize + 1, "");
  EXPECT_EQ(memory::UnmaskPtr(ptr), memory::UnmaskPtr(ptr2));
  // Allocated from the thread cache.
  EXPECT_EQ(kFillCountForSmallBucket - 1,
            tcache->bucket_count_for_testing(index));
}

TEST_F(PartitionAllocThreadCacheTest, MultipleObjectsCachedPerBucket) {
  auto* tcache = root_->thread_cache_for_testing();
  DeltaCounter batch_fill_counter{tcache->stats_.batch_fill_count};
  size_t bucket_index =
      FillThreadCacheAndReturnIndex(kMediumSize, kFillCountForMediumBucket + 2);
  EXPECT_EQ(2 * kFillCountForMediumBucket,
            tcache->bucket_count_for_testing(bucket_index));
  // 2 batches, since there were more than |kFillCountForMediumBucket|
  // allocations.
  EXPECT_EQ(2u, batch_fill_counter.Delta());
}

TEST_F(PartitionAllocThreadCacheTest, ObjectsCachedCountIsLimited) {
  size_t bucket_index = FillThreadCacheAndReturnIndex(kMediumSize, 1000);
  auto* tcache = root_->thread_cache_for_testing();
  EXPECT_LT(tcache->bucket_count_for_testing(bucket_index), 1000u);
}

TEST_F(PartitionAllocThreadCacheTest, Purge) {
  size_t allocations = 10;
  size_t bucket_index = FillThreadCacheAndReturnIndex(kMediumSize, allocations);
  auto* tcache = root_->thread_cache_for_testing();
  EXPECT_EQ(
      (1 + allocations / kFillCountForMediumBucket) * kFillCountForMediumBucket,
      tcache->bucket_count_for_testing(bucket_index));
  tcache->Purge();
  EXPECT_EQ(0u, tcache->bucket_count_for_testing(bucket_index));
}

TEST_F(PartitionAllocThreadCacheTest, NoCrossPartitionCache) {
  ThreadSafePartitionRoot root{{PartitionOptions::AlignedAlloc::kAllowed,
                                PartitionOptions::ThreadCache::kDisabled,
                                PartitionOptions::Quarantine::kAllowed,
                                PartitionOptions::Cookie::kDisallowed,
                                PartitionOptions::BackupRefPtr::kDisabled,
                                PartitionOptions::UseConfigurablePool::kNo,
                                PartitionOptions::LazyCommit::kEnabled}};

  size_t bucket_index = FillThreadCacheAndReturnIndex(kSmallSize);
  void* ptr = root.Alloc(kSmallSize, "");
  ASSERT_TRUE(ptr);

  auto* tcache = root_->thread_cache_for_testing();
  EXPECT_EQ(kFillCountForSmallBucket,
            tcache->bucket_count_for_testing(bucket_index));

  ThreadSafePartitionRoot::Free(ptr);
  EXPECT_EQ(kFillCountForSmallBucket,
            tcache->bucket_count_for_testing(bucket_index));
}

#if defined(PA_ENABLE_THREAD_CACHE_STATISTICS)  // Required to record hits and
                                                // misses.
TEST_F(PartitionAllocThreadCacheTest, LargeAllocationsAreNotCached) {
  auto* tcache = root_->thread_cache_for_testing();
  DeltaCounter alloc_miss_counter{tcache->stats_.alloc_misses};
  DeltaCounter alloc_miss_too_large_counter{
      tcache->stats_.alloc_miss_too_large};
  DeltaCounter cache_fill_counter{tcache->stats_.cache_fill_count};
  DeltaCounter cache_fill_misses_counter{tcache->stats_.cache_fill_misses};

  FillThreadCacheAndReturnIndex(100 * 1024);
  tcache = root_->thread_cache_for_testing();
  EXPECT_EQ(1u, alloc_miss_counter.Delta());
  EXPECT_EQ(1u, alloc_miss_too_large_counter.Delta());
  EXPECT_EQ(1u, cache_fill_counter.Delta());
  EXPECT_EQ(1u, cache_fill_misses_counter.Delta());
}
#endif  // defined(PA_ENABLE_THREAD_CACHE_STATISTICS)

TEST_F(PartitionAllocThreadCacheTest, DirectMappedAllocationsAreNotCached) {
  FillThreadCacheAndReturnIndex(1024 * 1024);
  // The line above would crash due to out of bounds access if this wasn't
  // properly handled.
}

// This tests that Realloc properly handles bookkeeping, specifically the path
// that reallocates in place.
TEST_F(PartitionAllocThreadCacheTest, DirectMappedReallocMetrics) {
  root_->ResetBookkeepingForTesting();

  size_t expected_allocated_size = root_->get_total_size_of_allocated_bytes();

  EXPECT_EQ(expected_allocated_size,
            root_->get_total_size_of_allocated_bytes());
  EXPECT_EQ(expected_allocated_size, root_->get_max_size_of_allocated_bytes());

  void* ptr = root_->Alloc(10 * kMaxBucketed, "");

  EXPECT_EQ(expected_allocated_size + 10 * kMaxBucketed,
            root_->get_total_size_of_allocated_bytes());

  void* ptr2 = root_->Realloc(ptr, 9 * kMaxBucketed, "");

  ASSERT_EQ(ptr, ptr2);
  EXPECT_EQ(expected_allocated_size + 9 * kMaxBucketed,
            root_->get_total_size_of_allocated_bytes());

  ptr2 = root_->Realloc(ptr, 10 * kMaxBucketed, "");

  ASSERT_EQ(ptr, ptr2);
  EXPECT_EQ(expected_allocated_size + 10 * kMaxBucketed,
            root_->get_total_size_of_allocated_bytes());

  root_->Free(ptr);
}

TEST_F(PartitionAllocThreadCacheTest, MultipleThreadCaches) {
  FillThreadCacheAndReturnIndex(kMediumSize);
  auto* parent_thread_tcache = root_->thread_cache_for_testing();
  ASSERT_TRUE(parent_thread_tcache);

  LambdaThreadDelegate delegate{BindLambdaForTesting([&]() {
    EXPECT_FALSE(root_->thread_cache_for_testing());  // No allocations yet.
    FillThreadCacheAndReturnIndex(kMediumSize);
    auto* tcache = root_->thread_cache_for_testing();
    EXPECT_TRUE(tcache);

    EXPECT_NE(parent_thread_tcache, tcache);
  })};

  PlatformThreadHandle thread_handle;
  PlatformThread::Create(0, &delegate, &thread_handle);
  PlatformThread::Join(thread_handle);
}

TEST_F(PartitionAllocThreadCacheTest, ThreadCacheReclaimedWhenThreadExits) {
  // Make sure that there is always at least one object allocated in the test
  // bucket, so that the PartitionPage is no reclaimed.
  //
  // Allocate enough objects to force a cache fill at the next allocation.
  std::vector<void*> tmp;
  for (size_t i = 0; i < kDefaultCountForMediumBucket / 4; i++) {
    tmp.push_back(root_->Alloc(kMediumSize, ""));
  }

  void* other_thread_ptr;

  LambdaThreadDelegate delegate{BindLambdaForTesting([&]() {
    EXPECT_FALSE(root_->thread_cache_for_testing());  // No allocations yet.
    other_thread_ptr = root_->Alloc(kMediumSize, "");
    root_->Free(other_thread_ptr);
    // |other_thread_ptr| is now in the thread cache.
  })};

  PlatformThreadHandle thread_handle;
  PlatformThread::Create(0, &delegate, &thread_handle);
  PlatformThread::Join(thread_handle);

  void* this_thread_ptr = root_->Alloc(kMediumSize, "");
  // |other_thread_ptr| was returned to the central allocator, and is returned
  // here, as it comes from the freelist.
  EXPECT_EQ(memory::UnmaskPtr(this_thread_ptr),
            memory::UnmaskPtr(other_thread_ptr));
  root_->Free(other_thread_ptr);

  for (void* ptr : tmp)
    root_->Free(ptr);
}

// On Android and macOS with PartitionAlloc as malloc, we have extra thread
// caches being created, causing this test to fail.
#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) && \
    (defined(OS_ANDROID) || defined(OS_APPLE))
#define MAYBE_ThreadCacheRegistry DISABLED_ThreadCacheRegistry
#else
#define MAYBE_ThreadCacheRegistry ThreadCacheRegistry
#endif  // BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) && defined(OS_ANDROID)

TEST_F(PartitionAllocThreadCacheTest, MAYBE_ThreadCacheRegistry) {
  auto* parent_thread_tcache = root_->thread_cache_for_testing();
  ASSERT_TRUE(parent_thread_tcache);

  {
    PartitionAutoLock lock(ThreadCacheRegistry::GetLock());
    EXPECT_EQ(parent_thread_tcache->prev_, nullptr);
    EXPECT_EQ(parent_thread_tcache->next_, nullptr);
  }

  LambdaThreadDelegate delegate{BindLambdaForTesting([&]() {
    EXPECT_FALSE(root_->thread_cache_for_testing());  // No allocations yet.
    FillThreadCacheAndReturnIndex(kSmallSize);
    auto* tcache = root_->thread_cache_for_testing();
    EXPECT_TRUE(tcache);

    PartitionAutoLock lock(ThreadCacheRegistry::GetLock());
    EXPECT_EQ(tcache->prev_, nullptr);
    EXPECT_EQ(tcache->next_, parent_thread_tcache);
  })};

  PlatformThreadHandle thread_handle;
  PlatformThread::Create(0, &delegate, &thread_handle);
  PlatformThread::Join(thread_handle);

  PartitionAutoLock lock(ThreadCacheRegistry::GetLock());
  EXPECT_EQ(parent_thread_tcache->prev_, nullptr);
  EXPECT_EQ(parent_thread_tcache->next_, nullptr);
}

#if defined(PA_ENABLE_THREAD_CACHE_STATISTICS)
TEST_F(PartitionAllocThreadCacheTest, RecordStats) {
  auto* tcache = root_->thread_cache_for_testing();
  DeltaCounter alloc_counter{tcache->stats_.alloc_count};
  DeltaCounter alloc_hits_counter{tcache->stats_.alloc_hits};
  DeltaCounter alloc_miss_counter{tcache->stats_.alloc_misses};

  DeltaCounter alloc_miss_empty_counter{tcache->stats_.alloc_miss_empty};

  DeltaCounter cache_fill_counter{tcache->stats_.cache_fill_count};
  DeltaCounter cache_fill_hits_counter{tcache->stats_.cache_fill_hits};
  DeltaCounter cache_fill_misses_counter{tcache->stats_.cache_fill_misses};

  // Cache has been purged, first allocation is a miss.
  void* data = root_->Alloc(kMediumSize, "");
  EXPECT_EQ(1u, alloc_counter.Delta());
  EXPECT_EQ(1u, alloc_miss_counter.Delta());
  EXPECT_EQ(0u, alloc_hits_counter.Delta());

  // Cache fill worked.
  root_->Free(data);
  EXPECT_EQ(1u, cache_fill_counter.Delta());
  EXPECT_EQ(1u, cache_fill_hits_counter.Delta());
  EXPECT_EQ(0u, cache_fill_misses_counter.Delta());

  tcache->Purge();
  cache_fill_counter.Reset();
  // Buckets are never full, fill always succeeds.
  size_t allocations = 10;
  size_t bucket_index = FillThreadCacheAndReturnIndex(
      kMediumSize, kDefaultCountForMediumBucket + allocations);
  EXPECT_EQ(kDefaultCountForMediumBucket + allocations,
            cache_fill_counter.Delta());
  EXPECT_EQ(0u, cache_fill_misses_counter.Delta());

  // Memory footprint.
  ThreadCacheStats stats;
  ThreadCacheRegistry::Instance().DumpStats(true, &stats);
  // Bucket was cleared (set to kDefaultCountForMediumBucket / 2) after going
  // above the limit (-1), then refilled by batches (1 + floor(allocations /
  // kFillCountForSmallBucket) times).
  size_t expected_count =
      kDefaultCountForMediumBucket / 2 - 1 +
      (1 + allocations / kFillCountForMediumBucket) * kFillCountForMediumBucket;
  EXPECT_EQ(root_->buckets[bucket_index].slot_size * expected_count,
            stats.bucket_total_memory);
  EXPECT_EQ(sizeof(ThreadCache), stats.metadata_overhead);
}

TEST_F(PartitionAllocThreadCacheTest, MultipleThreadCachesAccounting) {
  FillThreadCacheAndReturnIndex(kMediumSize);
  uint64_t alloc_count = root_->thread_cache_for_testing()->stats_.alloc_count;

  LambdaThreadDelegate delegate{BindLambdaForTesting([&]() {
    EXPECT_FALSE(root_->thread_cache_for_testing());  // No allocations yet.
    size_t bucket_index = FillThreadCacheAndReturnIndex(kMediumSize);

    ThreadCacheStats stats;
    ThreadCacheRegistry::Instance().DumpStats(false, &stats);
    // 2* for this thread and the parent one.
    EXPECT_EQ(
        2 * root_->buckets[bucket_index].slot_size * kFillCountForMediumBucket,
        stats.bucket_total_memory);
    EXPECT_EQ(2 * sizeof(ThreadCache), stats.metadata_overhead);

    uint64_t this_thread_alloc_count =
        root_->thread_cache_for_testing()->stats_.alloc_count;
    EXPECT_EQ(alloc_count + this_thread_alloc_count, stats.alloc_count);
  })};

  PlatformThreadHandle thread_handle;
  PlatformThread::Create(0, &delegate, &thread_handle);
  PlatformThread::Join(thread_handle);
}

#endif  // defined(PA_ENABLE_THREAD_CACHE_STATISTICS)

TEST_F(PartitionAllocThreadCacheTest, PurgeAll) NO_THREAD_SAFETY_ANALYSIS {
  std::atomic<bool> other_thread_started{false};
  std::atomic<bool> purge_called{false};

  size_t bucket_index = FillThreadCacheAndReturnIndex(kSmallSize);
  ThreadCache* this_thread_tcache = root_->thread_cache_for_testing();
  ThreadCache* other_thread_tcache = nullptr;

  LambdaThreadDelegate delegate{
      BindLambdaForTesting([&]() NO_THREAD_SAFETY_ANALYSIS {
        FillThreadCacheAndReturnIndex(kSmallSize);
        other_thread_tcache = root_->thread_cache_for_testing();

        other_thread_started.store(true, std::memory_order_release);
        while (!purge_called.load(std::memory_order_acquire)) {
        }

        // Purge() was not triggered from the other thread.
        EXPECT_EQ(kFillCountForSmallBucket,
                  other_thread_tcache->bucket_count_for_testing(bucket_index));
        // Allocations do not trigger Purge().
        void* data = root_->Alloc(kSmallSize, "");
        EXPECT_EQ(kFillCountForSmallBucket - 1,
                  other_thread_tcache->bucket_count_for_testing(bucket_index));
        // But deallocations do.
        root_->Free(data);
        EXPECT_EQ(0u,
                  other_thread_tcache->bucket_count_for_testing(bucket_index));
      })};

  PlatformThreadHandle thread_handle;
  PlatformThread::Create(0, &delegate, &thread_handle);

  while (!other_thread_started.load(std::memory_order_acquire)) {
  }

  EXPECT_EQ(kFillCountForSmallBucket,
            this_thread_tcache->bucket_count_for_testing(bucket_index));
  EXPECT_EQ(kFillCountForSmallBucket,
            other_thread_tcache->bucket_count_for_testing(bucket_index));

  ThreadCacheRegistry::Instance().PurgeAll();
  // This thread is synchronously purged.
  EXPECT_EQ(0u, this_thread_tcache->bucket_count_for_testing(bucket_index));
  // Not the other one.
  EXPECT_EQ(kFillCountForSmallBucket,
            other_thread_tcache->bucket_count_for_testing(bucket_index));

  purge_called.store(true, std::memory_order_release);
  PlatformThread::Join(thread_handle);
}

TEST_F(PartitionAllocThreadCacheTest, PeriodicPurge) {
  auto& registry = ThreadCacheRegistry::Instance();
  registry.StartPeriodicPurge(DelayedAction);
  EXPECT_EQ(ThreadCacheRegistry::kDefaultPurgeInterval,
            registry.purge_interval_for_testing());

  // Small amount of memory, the period gets longer.
  auto* tcache = ThreadCache::Get();
  ASSERT_LT(tcache->CachedMemory(),
            ThreadCacheRegistry::kMinCachedMemoryForPurging);
  PurgeManually();
  EXPECT_EQ(2 * ThreadCacheRegistry::kDefaultPurgeInterval,
            registry.purge_interval_for_testing());
  PurgeManually();
  EXPECT_EQ(4 * ThreadCacheRegistry::kDefaultPurgeInterval,
            registry.purge_interval_for_testing());

  // Check that the purge interval is clamped at the maximum value.
  while (registry.purge_interval_for_testing() <
         ThreadCacheRegistry::kMaxPurgeInterval) {
    PurgeManually();
  }
  PurgeManually();

  // Not enough memory to decrease the interval.
  FillThreadCacheWithMemory(ThreadCacheRegistry::kMinCachedMemoryForPurging +
                            1);
  PurgeManually();
  EXPECT_EQ(ThreadCacheRegistry::kMaxPurgeInterval,
            registry.purge_interval_for_testing());

  FillThreadCacheWithMemory(
      2 * ThreadCacheRegistry::kMinCachedMemoryForPurging + 1);
  PurgeManually();
  EXPECT_EQ(ThreadCacheRegistry::kMaxPurgeInterval / 2,
            registry.purge_interval_for_testing());

  // Enough memory, interval doesn't change.
  FillThreadCacheWithMemory(ThreadCacheRegistry::kMinCachedMemoryForPurging);
  PurgeManually();
  EXPECT_EQ(ThreadCacheRegistry::kMaxPurgeInterval / 2,
            registry.purge_interval_for_testing());

  // No cached memory, increase the interval.
  PurgeManually();
  EXPECT_EQ(ThreadCacheRegistry::kMaxPurgeInterval,
            registry.purge_interval_for_testing());

  // Cannot test the very large size with only one thread, this is tested below
  // in the multiple threads test.
}

// Disabled due to flakiness: crbug.com/1220371
TEST_F(PartitionAllocThreadCacheTest,
       DISABLED_PeriodicPurgeSumsOverAllThreads) {
  auto& registry = ThreadCacheRegistry::Instance();
  registry.StartPeriodicPurge(DelayedAction);
  EXPECT_EQ(ThreadCacheRegistry::kDefaultPurgeInterval,
            registry.purge_interval_for_testing());

  // Small amount of memory, the period gets longer.
  auto* tcache = ThreadCache::Get();
  ASSERT_LT(tcache->CachedMemory(),
            ThreadCacheRegistry::kMinCachedMemoryForPurging);
  PurgeManually();
  EXPECT_EQ(2 * ThreadCacheRegistry::kDefaultPurgeInterval,
            registry.purge_interval_for_testing());
  PurgeManually();
  EXPECT_EQ(4 * ThreadCacheRegistry::kDefaultPurgeInterval,
            registry.purge_interval_for_testing());

  // Check that the purge interval is clamped at the maximum value.
  while (registry.purge_interval_for_testing() <
         ThreadCacheRegistry::kMaxPurgeInterval) {
    PurgeManually();
  }
  PurgeManually();

  // Not enough memory on this thread to decrease the interval.
  FillThreadCacheWithMemory(ThreadCacheRegistry::kMinCachedMemoryForPurging /
                            2);
  PurgeManually();
  EXPECT_EQ(ThreadCacheRegistry::kMaxPurgeInterval,
            registry.purge_interval_for_testing());

  std::atomic<int> allocations_done{0};
  std::atomic<bool> can_finish{false};
  LambdaThreadDelegate delegate{BindLambdaForTesting([&]() {
    FillThreadCacheWithMemory(5 *
                              ThreadCacheRegistry::kMinCachedMemoryForPurging);
    allocations_done.fetch_add(1, std::memory_order_release);

    // This thread needs to be alive when the next periodic purge task runs.
    while (!can_finish.load(std::memory_order_acquire)) {
    }
  })};

  PlatformThreadHandle thread_handle;
  PlatformThread::Create(0, &delegate, &thread_handle);
  PlatformThreadHandle thread_handle_2;
  PlatformThread::Create(0, &delegate, &thread_handle_2);

  while (allocations_done.load(std::memory_order_acquire) != 2) {
  }

  // Many allocations on the other thread.
  PurgeManually();
  EXPECT_EQ(ThreadCacheRegistry::kDefaultPurgeInterval,
            registry.purge_interval_for_testing());

  can_finish.store(true, std::memory_order_release);
  PlatformThread::Join(thread_handle);
  PlatformThread::Join(thread_handle_2);
}

TEST_F(PartitionAllocThreadCacheTest, DynamicCountPerBucket) {
  auto* tcache = root_->thread_cache_for_testing();
  size_t bucket_index =
      FillThreadCacheAndReturnIndex(kMediumSize, kDefaultCountForMediumBucket);
  EXPECT_EQ(kDefaultCountForMediumBucket, tcache->buckets_[bucket_index].count);

  ThreadCacheRegistry::Instance().SetThreadCacheMultiplier(
      ThreadCache::kDefaultMultiplier / 2);
  // No immediate batch deallocation.
  EXPECT_EQ(kDefaultCountForMediumBucket, tcache->buckets_[bucket_index].count);
  void* data = root_->Alloc(kMediumSize, "");
  // Not triggered by allocations.
  EXPECT_EQ(kDefaultCountForMediumBucket - 1,
            tcache->buckets_[bucket_index].count);

  // Free() triggers the purge within limits.
  root_->Free(data);
  EXPECT_LE(tcache->buckets_[bucket_index].count,
            kDefaultCountForMediumBucket / 2);

  // Won't go above anymore.
  FillThreadCacheAndReturnIndex(kMediumSize, 1000);
  EXPECT_LE(tcache->buckets_[bucket_index].count,
            kDefaultCountForMediumBucket / 2);

  // Limit can be raised.
  ThreadCacheRegistry::Instance().SetThreadCacheMultiplier(
      ThreadCache::kDefaultMultiplier * 2);
  FillThreadCacheAndReturnIndex(kMediumSize, 1000);
  EXPECT_GT(tcache->buckets_[bucket_index].count,
            kDefaultCountForMediumBucket / 2);
}

TEST_F(PartitionAllocThreadCacheTest, DynamicCountPerBucketClamping) {
  auto* tcache = root_->thread_cache_for_testing();

  ThreadCacheRegistry::Instance().SetThreadCacheMultiplier(
      ThreadCache::kDefaultMultiplier / 1000.);
  for (size_t i = 0; i < ThreadCache::kBucketCount; i++) {
    // Invalid bucket.
    if (!tcache->buckets_[i].limit.load(std::memory_order_relaxed)) {
      EXPECT_EQ(root_->buckets[i].active_slot_spans_head, nullptr);
      continue;
    }
    EXPECT_GE(tcache->buckets_[i].limit.load(std::memory_order_relaxed), 1u);
  }

  ThreadCacheRegistry::Instance().SetThreadCacheMultiplier(
      ThreadCache::kDefaultMultiplier * 1000.);
  for (size_t i = 0; i < ThreadCache::kBucketCount; i++) {
    // Invalid bucket.
    if (!tcache->buckets_[i].limit.load(std::memory_order_relaxed)) {
      EXPECT_EQ(root_->buckets[i].active_slot_spans_head, nullptr);
      continue;
    }
    EXPECT_LT(tcache->buckets_[i].limit.load(std::memory_order_relaxed), 0xff);
  }
}

TEST_F(PartitionAllocThreadCacheTest, DynamicCountPerBucketMultipleThreads) {
  std::atomic<bool> other_thread_started{false};
  std::atomic<bool> threshold_changed{false};

  auto* tcache = root_->thread_cache_for_testing();
  size_t bucket_index =
      FillThreadCacheAndReturnIndex(kSmallSize, kDefaultCountForSmallBucket);
  EXPECT_EQ(kDefaultCountForSmallBucket, tcache->buckets_[bucket_index].count);

  // Change the ratio before starting the threads, checking that it will applied
  // to newly-created threads.
  ThreadCacheRegistry::Instance().SetThreadCacheMultiplier(
      ThreadCache::kDefaultMultiplier + 1);

  LambdaThreadDelegate delegate{BindLambdaForTesting([&]() {
    FillThreadCacheAndReturnIndex(kSmallSize, kDefaultCountForSmallBucket + 10);
    auto* this_thread_tcache = root_->thread_cache_for_testing();
    // More than the default since the multiplier has changed.
    EXPECT_GT(this_thread_tcache->buckets_[bucket_index].count,
              kDefaultCountForSmallBucket + 10);

    other_thread_started.store(true, std::memory_order_release);
    while (!threshold_changed.load(std::memory_order_acquire)) {
    }

    void* data = root_->Alloc(kSmallSize, "");
    // Deallocations trigger limit enforcement.
    root_->Free(data);
    // Since the bucket is too full, it gets halved by batched deallocation.
    EXPECT_EQ(static_cast<uint8_t>(ThreadCache::kSmallBucketBaseCount / 2),
              this_thread_tcache->bucket_count_for_testing(bucket_index));
  })};

  PlatformThreadHandle thread_handle;
  PlatformThread::Create(0, &delegate, &thread_handle);

  while (!other_thread_started.load(std::memory_order_acquire)) {
  }

  ThreadCacheRegistry::Instance().SetThreadCacheMultiplier(1.);
  threshold_changed.store(true, std::memory_order_release);

  PlatformThread::Join(thread_handle);
}

TEST_F(PartitionAllocThreadCacheTest, DynamicSizeThreshold) {
  auto* tcache = root_->thread_cache_for_testing();
  DeltaCounter alloc_miss_counter{tcache->stats_.alloc_misses};
  DeltaCounter alloc_miss_too_large_counter{
      tcache->stats_.alloc_miss_too_large};
  DeltaCounter cache_fill_counter{tcache->stats_.cache_fill_count};
  DeltaCounter cache_fill_misses_counter{tcache->stats_.cache_fill_misses};

  // Default threshold at first.
  ThreadCache::SetLargestCachedSize(ThreadCache::kDefaultSizeThreshold);
  FillThreadCacheAndReturnIndex(ThreadCache::kDefaultSizeThreshold);
  EXPECT_EQ(0u, alloc_miss_too_large_counter.Delta());
  EXPECT_EQ(1u, cache_fill_counter.Delta());

  // Too large to be cached.
  FillThreadCacheAndReturnIndex(ThreadCache::kDefaultSizeThreshold + 1);
  EXPECT_EQ(1u, alloc_miss_too_large_counter.Delta());

  // Increase.
  ThreadCache::SetLargestCachedSize(ThreadCache::kLargeSizeThreshold);
  FillThreadCacheAndReturnIndex(ThreadCache::kDefaultSizeThreshold + 1);
  // No new miss.
  EXPECT_EQ(1u, alloc_miss_too_large_counter.Delta());

  // Lower.
  ThreadCache::SetLargestCachedSize(ThreadCache::kDefaultSizeThreshold);
  FillThreadCacheAndReturnIndex(ThreadCache::kDefaultSizeThreshold + 1);
  EXPECT_EQ(2u, alloc_miss_too_large_counter.Delta());

  // Value is clamped.
  size_t too_large = 1024 * 1024;
  ThreadCache::SetLargestCachedSize(too_large);
  FillThreadCacheAndReturnIndex(too_large);
  EXPECT_EQ(3u, alloc_miss_too_large_counter.Delta());
}

TEST_F(PartitionAllocThreadCacheTest, DynamicSizeThresholdPurge) {
  auto* tcache = root_->thread_cache_for_testing();
  DeltaCounter alloc_miss_counter{tcache->stats_.alloc_misses};
  DeltaCounter alloc_miss_too_large_counter{
      tcache->stats_.alloc_miss_too_large};
  DeltaCounter cache_fill_counter{tcache->stats_.cache_fill_count};
  DeltaCounter cache_fill_misses_counter{tcache->stats_.cache_fill_misses};

  // Cache large allocations.
  size_t large_allocation_size = ThreadCache::kLargeSizeThreshold;
  ThreadCache::SetLargestCachedSize(ThreadCache::kLargeSizeThreshold);
  size_t index = FillThreadCacheAndReturnIndex(large_allocation_size);
  EXPECT_EQ(0u, alloc_miss_too_large_counter.Delta());

  // Lower.
  ThreadCache::SetLargestCachedSize(ThreadCache::kDefaultSizeThreshold);
  FillThreadCacheAndReturnIndex(large_allocation_size);
  EXPECT_EQ(1u, alloc_miss_too_large_counter.Delta());

  // There is memory trapped in the cache bucket.
  EXPECT_GT(tcache->buckets_[index].count, 0u);

  // Which is reclaimed by Purge().
  tcache->Purge();
  EXPECT_EQ(0u, tcache->buckets_[index].count);
}

TEST_F(PartitionAllocThreadCacheTest, ClearFromTail) {
  auto count_items = [](ThreadCache* tcache, size_t index) {
    uint8_t count = 0;
    auto* head = tcache->buckets_[index].freelist_head;
    while (head) {
      head = head->GetNext(tcache->buckets_[index].slot_size);
      count++;
    }
    return count;
  };

  auto* tcache = root_->thread_cache_for_testing();
  size_t index = FillThreadCacheAndReturnIndex(kSmallSize, 10);
  ASSERT_GE(count_items(tcache, index), 10);
  void* head = tcache->buckets_[index].freelist_head;

  for (size_t limit : {8, 3, 1}) {
    tcache->ClearBucket(tcache->buckets_[index], limit);
    EXPECT_EQ(head, static_cast<void*>(tcache->buckets_[index].freelist_head));
    EXPECT_EQ(count_items(tcache, index), limit);
  }
  tcache->ClearBucket(tcache->buckets_[index], 0);
  EXPECT_EQ(nullptr, static_cast<void*>(tcache->buckets_[index].freelist_head));
}

TEST_F(PartitionAllocThreadCacheTest, Bookkeeping) {
  void* arr[kFillCountForMediumBucket] = {};
  auto* tcache = root_->thread_cache_for_testing();

  root_->PurgeMemory(PartitionPurgeDecommitEmptySlotSpans |
                     PartitionPurgeDiscardUnusedSystemPages);
  root_->ResetBookkeepingForTesting();

  size_t tc_bucket_index = root_->SizeToBucketIndex(sizeof(ThreadCache));
  auto* tc_bucket = &root_->buckets[tc_bucket_index];
  size_t expected_allocated_size =
      tc_bucket->slot_size;  // For the ThreadCache itself.
  size_t expected_committed_size = root_->use_lazy_commit
                                       ? SystemPageSize()
                                       : tc_bucket->get_bytes_per_span();

  EXPECT_EQ(expected_committed_size, root_->total_size_of_committed_pages);
  EXPECT_EQ(expected_committed_size, root_->max_size_of_committed_pages);
  EXPECT_EQ(expected_allocated_size,
            root_->get_total_size_of_allocated_bytes());
  EXPECT_EQ(expected_allocated_size, root_->get_max_size_of_allocated_bytes());

  void* ptr = root_->Alloc(kMediumSize, "");

  auto* medium_bucket = &root_->buckets[root_->SizeToBucketIndex(kMediumSize)];
  size_t medium_alloc_size = medium_bucket->slot_size;
  expected_allocated_size += medium_alloc_size;
  expected_committed_size += root_->use_lazy_commit
                                 ? SystemPageSize()
                                 : medium_bucket->get_bytes_per_span();

  EXPECT_EQ(expected_committed_size, root_->total_size_of_committed_pages);
  EXPECT_EQ(expected_committed_size, root_->max_size_of_committed_pages);
  EXPECT_EQ(expected_allocated_size,
            root_->get_total_size_of_allocated_bytes());
  EXPECT_EQ(expected_allocated_size, root_->get_max_size_of_allocated_bytes());

  expected_allocated_size += kFillCountForMediumBucket * medium_alloc_size;

  // These allocations all come from the thread-cache.
  for (size_t i = 0; i < kFillCountForMediumBucket; i++) {
    arr[i] = root_->Alloc(kMediumSize, "");
    EXPECT_EQ(expected_committed_size, root_->total_size_of_committed_pages);
    EXPECT_EQ(expected_committed_size, root_->max_size_of_committed_pages);
    EXPECT_EQ(expected_allocated_size,
              root_->get_total_size_of_allocated_bytes());
    EXPECT_EQ(expected_allocated_size,
              root_->get_max_size_of_allocated_bytes());
    EXPECT_EQ((kFillCountForMediumBucket - 1 - i) * medium_alloc_size,
              tcache->CachedMemory());
  }

  EXPECT_EQ(0U, tcache->CachedMemory());

  root_->Free(ptr);

  for (auto*& el : arr) {
    root_->Free(el);
  }
}

}  // namespace internal
}  // namespace base

#endif  // !defined(MEMORY_TOOL_REPLACES_ALLOCATOR) &&
        // defined(PA_THREAD_CACHE_SUPPORTED)
