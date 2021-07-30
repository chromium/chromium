// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/thread_cache.h"

#include <sys/types.h>
#include <algorithm>
#include <atomic>

#include "base/allocator/partition_allocator/partition_alloc_check.h"
#include "base/allocator/partition_allocator/partition_alloc_config.h"
#include "base/allocator/partition_allocator/partition_alloc_constants.h"
#include "base/allocator/partition_allocator/partition_root.h"
#include "base/base_export.h"
#include "base/compiler_specific.h"
#include "base/cxx17_backports.h"
#include "base/dcheck_is_on.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/base_tracing.h"
#include "build/build_config.h"

namespace base {

namespace internal {

namespace {

ThreadCacheRegistry g_instance;

}

BASE_EXPORT PartitionTlsKey g_thread_cache_key;
#if defined(PA_THREAD_CACHE_FAST_TLS)
BASE_EXPORT
thread_local ThreadCache* g_thread_cache;
#endif

namespace {
// Since |g_thread_cache_key| is shared, make sure that no more than one
// PartitionRoot can use it.
static std::atomic<PartitionRoot<ThreadSafe>*> g_thread_cache_root;

#if defined(OS_WIN)
void OnDllProcessDetach() {
  // Very late allocations do occur (see crbug.com/1159411#c7 for instance),
  // including during CRT teardown. This is problematic for the thread cache
  // which relies on the CRT for TLS access for instance. This cannot be
  // mitigated inside the thread cache (since getting to it requires querying
  // TLS), but the PartitionRoot associated wih the thread cache can be made to
  // not use the thread cache anymore.
  g_thread_cache_root.load(std::memory_order_relaxed)->with_thread_cache =
      false;
}
#endif

static bool g_thread_cache_key_created = false;
}  // namespace

constexpr base::TimeDelta ThreadCacheRegistry::kMinPurgeInterval;
constexpr base::TimeDelta ThreadCacheRegistry::kMaxPurgeInterval;
constexpr base::TimeDelta ThreadCacheRegistry::kDefaultPurgeInterval;
constexpr size_t ThreadCacheRegistry::kMinCachedMemoryForPurging;
uint8_t ThreadCache::global_limits_[ThreadCache::kBucketCount];

// Start with the normal size, not the maximum one.
uint16_t ThreadCache::largest_active_bucket_index_ =
    BucketIndexLookup::GetIndex(kDefaultSizeThreshold);

// static
ThreadCacheRegistry& ThreadCacheRegistry::Instance() {
  return g_instance;
}

void ThreadCacheRegistry::RegisterThreadCache(ThreadCache* cache) {
  PartitionAutoLock scoped_locker(GetLock());
  cache->next_ = nullptr;
  cache->prev_ = nullptr;

  ThreadCache* previous_head = list_head_;
  list_head_ = cache;
  cache->next_ = previous_head;
  if (previous_head)
    previous_head->prev_ = cache;
}

void ThreadCacheRegistry::UnregisterThreadCache(ThreadCache* cache) {
  PartitionAutoLock scoped_locker(GetLock());
  if (cache->prev_)
    cache->prev_->next_ = cache->next_;
  if (cache->next_)
    cache->next_->prev_ = cache->prev_;
  if (cache == list_head_)
    list_head_ = cache->next_;
}

void ThreadCacheRegistry::DumpStats(bool my_thread_only,
                                    ThreadCacheStats* stats) {
  ThreadCache::EnsureThreadSpecificDataInitialized();
  memset(reinterpret_cast<void*>(stats), 0, sizeof(ThreadCacheStats));

  PartitionAutoLock scoped_locker(GetLock());
  if (my_thread_only) {
    auto* tcache = ThreadCache::Get();
    if (!ThreadCache::IsValid(tcache))
      return;
    tcache->AccumulateStats(stats);
  } else {
    ThreadCache* tcache = list_head_;
    while (tcache) {
      // Racy, as other threads are still allocating. This is not an issue,
      // since we are only interested in statistics. However, this means that
      // count is not necessarily equal to hits + misses for the various types
      // of events.
      tcache->AccumulateStats(stats);
      tcache = tcache->next_;
    }
  }
}

void ThreadCacheRegistry::PurgeAll() {
  auto* current_thread_tcache = ThreadCache::Get();

  // May take a while, don't hold the lock while purging.
  //
  // In most cases, the current thread is more important than other ones. For
  // instance in renderers, it is the main thread. It is also the only thread
  // that we can synchronously purge.
  //
  // The reason why we trigger the purge for this one first is that assuming
  // that all threads are allocating memory, they will start purging
  // concurrently in the loop below. This will then make them all contend with
  // the main thread for the partition lock, since it is acquired/released once
  // per bucket. By purging the main thread first, we avoid these interferences
  // for this thread at least.
  if (ThreadCache::IsValid(current_thread_tcache))
    current_thread_tcache->Purge();

  {
    PartitionAutoLock scoped_locker(GetLock());
    ThreadCache* tcache = list_head_;
    while (tcache) {
      PA_DCHECK(ThreadCache::IsValid(tcache));
      // Cannot purge directly, need to ask the other thread to purge "at some
      // point".
      // Note that this will not work if the other thread is sleeping forever.
      // TODO(lizeb): Handle sleeping threads.
      if (tcache != current_thread_tcache)
        tcache->SetShouldPurge();
      tcache = tcache->next_;
    }
  }
}

void ThreadCacheRegistry::ForcePurgeAllThreadAfterForkUnsafe() {
  PartitionAutoLock scoped_locker(GetLock());
  ThreadCache* tcache = list_head_;
  while (tcache) {
#if DCHECK_IS_ON()
    // Before fork(), locks are acquired in the parent process. This means that
    // a concurrent allocation in the parent which must be filled by the central
    // allocator (i.e. the thread cache bucket is empty) will block inside the
    // thread cache waiting for the lock to be released.
    //
    // In the child process, this allocation will never complete since this
    // thread will not be resumed. However, calling |Purge()| triggers the
    // reentrancy guard since the parent process thread was suspended from
    // within the thread cache.
    // Clear the guard to prevent this from crashing.
    tcache->is_in_thread_cache_ = false;
#endif
    // There is a PA_DCHECK() in code called from |Purge()| checking that thread
    // cache memory accounting is correct. Since we are after fork() and the
    // other threads got interrupted mid-flight, this guarantee does not hold,
    // and we get inconsistent results.  Rather than giving up on checking this
    // invariant in regular code, reset it here so that the PA_DCHECK()
    // passes. See crbug.com/1216964.
    tcache->cached_memory_ = tcache->CachedMemory();

    tcache->Purge();
    tcache = tcache->next_;
  }
}

void ThreadCacheRegistry::StartPeriodicPurge() {
  ThreadCache::EnsureThreadSpecificDataInitialized();

  // Can be called several times, don't post multiple tasks.
  if (periodic_purge_running_)
    return;

  periodic_purge_running_ = true;
  PostDelayedPurgeTask();
}

void ThreadCacheRegistry::SetThreadCacheMultiplier(float multiplier) {
  // Two steps:
  // - Set the global limits, which will affect newly created threads.
  // - Enumerate all thread caches and set the limit to the global one.
  {
    PartitionAutoLock scoped_locker(GetLock());
    ThreadCache* tcache = list_head_;

    // If this is called before *any* thread cache has serviced *any*
    // allocation, which can happen in tests, and in theory in non-test code as
    // well.
    if (!tcache)
      return;

    // Setting the global limit while locked, because we need |tcache->root_|.
    ThreadCache::SetGlobalLimits(tcache->root_, multiplier);

    while (tcache) {
      PA_DCHECK(ThreadCache::IsValid(tcache));
      for (int index = 0; index < ThreadCache::kBucketCount; index++) {
        // This is racy, but we don't care if the limit is enforced later, and
        // we really want to avoid atomic instructions on the fast path.
        tcache->buckets_[index].limit.store(ThreadCache::global_limits_[index],
                                            std::memory_order_relaxed);
      }

      tcache = tcache->next_;
    }
  }
}

void ThreadCacheRegistry::PostDelayedPurgeTask() {
  ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ThreadCacheRegistry::PeriodicPurge,
                     base::Unretained(this)),
      purge_interval_);
}

void ThreadCacheRegistry::PeriodicPurge() {
  TRACE_EVENT0("memory", "PeriodicPurge");
  // To stop periodic purge for testing.
  if (!periodic_purge_running_)
    return;

  // Summing across all threads can be slow, but is necessary. Otherwise we rely
  // on the assumption that the current thread is a good proxy for overall
  // allocation activity. This is not the case for all process types.
  //
  // Since there is no synchronization with other threads, the value is stale,
  // which is fine.
  size_t cached_memory_approx = 0;
  {
    PartitionAutoLock scoped_locker(GetLock());
    ThreadCache* tcache = list_head_;
    // Can run when there is no thread cache, in which case there is nothing to
    // do, and the task should not be rescheduled. This would typically indicate
    // a case where the thread cache was never enabled, or got disabled.
    if (!tcache)
      return;

    while (tcache) {
      cached_memory_approx += tcache->cached_memory_;
      tcache = tcache->next_;
    }
  }

  // If cached memory is low, this means that either memory footprint is fine,
  // or the process is mostly idle, and not allocating much since the last
  // purge. In this case, back off. On the other hand, if there is a lot of
  // cached memory, make purge more frequent, but always within a set frequency
  // range.
  //
  // There is a potential drawback: a process that was idle for a long time and
  // suddenly becomes very active will take some time to go back to regularly
  // scheduled purge with a small enough interval. This is the case for instance
  // of a renderer moving to foreground. To mitigate that, if cached memory
  // jumps is very large, make a greater leap to faster purging.
  if (cached_memory_approx > 10 * kMinCachedMemoryForPurging) {
    purge_interval_ = std::min(kDefaultPurgeInterval, purge_interval_ / 2);
  } else if (cached_memory_approx > 2 * kMinCachedMemoryForPurging) {
    purge_interval_ = std::max(kMinPurgeInterval, purge_interval_ / 2);
  } else if (cached_memory_approx < kMinCachedMemoryForPurging) {
    purge_interval_ = std::min(kMaxPurgeInterval, purge_interval_ * 2);
  }

  PurgeAll();

  PostDelayedPurgeTask();
}

void ThreadCacheRegistry::ResetForTesting() {
  purge_interval_ = kDefaultPurgeInterval;
  periodic_purge_running_ = false;
}

// static
void ThreadCache::EnsureThreadSpecificDataInitialized() {
  // Using the registry lock to protect from concurrent initialization without
  // adding a special-pupose lock.
  PartitionAutoLock scoped_locker(ThreadCacheRegistry::Instance().GetLock());
  if (g_thread_cache_key_created)
    return;

  bool ok = PartitionTlsCreate(&g_thread_cache_key, Delete);
  PA_CHECK(ok);
  g_thread_cache_key_created = true;
}

// static
void ThreadCache::Init(PartitionRoot<ThreadSafe>* root) {
#if defined(OS_NACL)
  IMMEDIATE_CRASH();
#endif
  PA_CHECK(root->buckets[kBucketCount - 1].slot_size == kLargeSizeThreshold);
  PA_CHECK(root->buckets[largest_active_bucket_index_].slot_size ==
           kDefaultSizeThreshold);

  EnsureThreadSpecificDataInitialized();

  // Make sure that only one PartitionRoot wants a thread cache.
  PartitionRoot<ThreadSafe>* expected = nullptr;
  if (!g_thread_cache_root.compare_exchange_strong(expected, root,
                                                   std::memory_order_seq_cst,
                                                   std::memory_order_seq_cst)) {
    PA_CHECK(false)
        << "Only one PartitionRoot is allowed to have a thread cache";
  }

#if defined(OS_WIN)
  PartitionTlsSetOnDllProcessDetach(OnDllProcessDetach);
#endif

  SetGlobalLimits(root, kDefaultMultiplier);
}

// static
void ThreadCache::SetGlobalLimits(PartitionRoot<ThreadSafe>* root,
                                  float multiplier) {
  size_t initial_value =
      static_cast<size_t>(kSmallBucketBaseCount) * multiplier;

  for (int index = 0; index < kBucketCount; index++) {
    const auto& root_bucket = root->buckets[index];
    // Invalid bucket.
    if (!root_bucket.active_slot_spans_head) {
      global_limits_[index] = 0;
      continue;
    }

    // Smaller allocations are more frequent, and more performance-sensitive.
    // Cache more small objects, and fewer larger ones, to save memory.
    size_t slot_size = root_bucket.slot_size;
    size_t value;
    if (slot_size <= 128) {
      value = initial_value;
    } else if (slot_size <= 256) {
      value = initial_value / 2;
    } else if (slot_size <= 512) {
      value = initial_value / 4;
    } else {
      value = initial_value / 8;
    }

    // Bare minimum so that malloc() / free() in a loop will not hit the central
    // allocator each time.
    constexpr size_t kMinLimit = 1;
    // |PutInBucket()| is called on a full bucket, which should not overflow.
    constexpr size_t kMaxLimit = std::numeric_limits<uint8_t>::max() - 1;
    global_limits_[index] =
        static_cast<uint8_t>(base::clamp(value, kMinLimit, kMaxLimit));
    PA_DCHECK(global_limits_[index] >= kMinLimit);
    PA_DCHECK(global_limits_[index] <= kMaxLimit);
  }
}

// static
void ThreadCache::SetLargestCachedSize(size_t size) {
  if (size > ThreadCache::kLargeSizeThreshold)
    size = ThreadCache::kLargeSizeThreshold;
  largest_active_bucket_index_ =
      PartitionRoot<internal::ThreadSafe>::SizeToBucketIndex(size);
  PA_CHECK(largest_active_bucket_index_ < kBucketCount);
}

// static
ThreadCache* ThreadCache::Create(PartitionRoot<internal::ThreadSafe>* root) {
  PA_CHECK(root);

  // Placement new and RawAlloc() are used, as otherwise when this partition is
  // the malloc() implementation, the memory allocated for the new thread cache
  // would make this code reentrant.
  //
  // This also means that deallocation must use RawFreeStatic(), hence the
  // operator delete() implementation below.
  size_t raw_size = root->AdjustSizeForExtrasAdd(sizeof(ThreadCache));
  size_t usable_size;
  bool already_zeroed;

  auto* bucket =
      root->buckets +
      PartitionRoot<internal::ThreadSafe>::SizeToBucketIndex(raw_size);
  void* buffer =
      root->RawAlloc(bucket, PartitionAllocZeroFill, raw_size,
                     PartitionPageSize(), &usable_size, &already_zeroed);
  ThreadCache* tcache = new (buffer) ThreadCache(root);

  // This may allocate.
  PartitionTlsSet(g_thread_cache_key, tcache);
#if defined(PA_THREAD_CACHE_FAST_TLS)
  // |thread_local| variables with destructors cause issues on some platforms.
  // Since we need a destructor (to empty the thread cache), we cannot use it
  // directly. However, TLS accesses with |thread_local| are typically faster,
  // as it can turn into a fixed offset load from a register (GS/FS on Linux
  // x86, for instance). On Windows, saving/restoring the last error increases
  // cost as well.
  //
  // To still get good performance, use |thread_local| to store a raw pointer,
  // and rely on the platform TLS to call the destructor.
  g_thread_cache = tcache;
#endif  // defined(PA_THREAD_CACHE_FAST_TLS)

  return tcache;
}

ThreadCache::ThreadCache(PartitionRoot<ThreadSafe>* root)
    : buckets_(),
      should_purge_(false),
      stats_(),
      root_(root),
      next_(nullptr),
      prev_(nullptr) {
  ThreadCacheRegistry::Instance().RegisterThreadCache(this);

  memset(&stats_, 0, sizeof(stats_));

  for (int index = 0; index < kBucketCount; index++) {
    const auto& root_bucket = root->buckets[index];
    Bucket* tcache_bucket = &buckets_[index];
    tcache_bucket->freelist_head = nullptr;
    tcache_bucket->count = 0;
    tcache_bucket->limit.store(global_limits_[index],
                               std::memory_order_relaxed);

    // Invalid bucket.
    if (!root_bucket.is_valid()) {
      // Explicitly set this, as size computations iterate over all buckets.
      tcache_bucket->limit.store(0, std::memory_order_relaxed);
      tcache_bucket->slot_size = 0;
    } else {
      tcache_bucket->slot_size = root_bucket.slot_size;
    }
  }
}

ThreadCache::~ThreadCache() {
  ThreadCacheRegistry::Instance().UnregisterThreadCache(this);
  Purge();
}

// static
void ThreadCache::Delete(void* tcache_ptr) {
  auto* tcache = reinterpret_cast<ThreadCache*>(tcache_ptr);
#if defined(PA_THREAD_CACHE_FAST_TLS)
  g_thread_cache = nullptr;
#endif

  auto* root = tcache->root_;
  reinterpret_cast<ThreadCache*>(tcache_ptr)->~ThreadCache();
  root->RawFree(tcache_ptr);

#if defined(OS_WIN)
  // On Windows, allocations do occur during thread/process teardown, make sure
  // they don't resurrect the thread cache.
  //
  // TODO(lizeb): Investigate whether this is needed on POSIX as well.
  PartitionTlsSet(g_thread_cache_key, reinterpret_cast<void*>(kTombstone));
#if defined(PA_THREAD_CACHE_FAST_TLS)
  g_thread_cache = reinterpret_cast<ThreadCache*>(kTombstone);
#endif

#endif  // defined(OS_WIN)
}

ThreadCache::Bucket::Bucket() {
  limit.store(0, std::memory_order_relaxed);
}

void ThreadCache::FillBucket(size_t bucket_index) {
  // Filling multiple elements from the central allocator at a time has several
  // advantages:
  // - Amortize lock acquisition
  // - Increase hit rate
  // - Can improve locality, as consecutive allocations from the central
  //   allocator will likely return close addresses, especially early on.
  //
  // However, do not take too many items, to prevent memory bloat.
  //
  // Cache filling / purging policy:
  // We aim at keeping the buckets neither empty nor full, while minimizing
  // requests to the central allocator.
  //
  // For each bucket, there is a |limit| of how many cached objects there are in
  // the bucket, so |count| < |limit| at all times.
  // - Clearing: limit -> limit / 2
  // - Filling: 0 -> limit / kBatchFillRatio
  //
  // These thresholds are somewhat arbitrary, with these considerations:
  // (1) Batched filling should not completely fill the bucket
  // (2) Batched clearing should not completely clear the bucket
  // (3) Batched filling should not be too eager
  //
  // If (1) and (2) do not hold, we risk oscillations of bucket filling /
  // clearing which would greatly increase calls to the central allocator. (3)
  // tries to keep memory usage low. So clearing half of the bucket, and filling
  // a quarter of it are sensible defaults.
  INCREMENT_COUNTER(stats_.batch_fill_count);

  Bucket& bucket = buckets_[bucket_index];
  // Some buckets may have a limit lower than |kBatchFillRatio|, but we still
  // want to at least allocate a single slot, otherwise we wrongly return
  // nullptr, which ends up deactivating the bucket.
  //
  // In these cases, we do not really batch bucket filling, but this is expected
  // to be used for the largest buckets, where over-allocating is not advised.
  int count = std::max(
      1, bucket.limit.load(std::memory_order_relaxed) / kBatchFillRatio);

  size_t usable_size;
  bool is_already_zeroed;

  PA_DCHECK(!root_->buckets[bucket_index].CanStoreRawSize());
  PA_DCHECK(!root_->buckets[bucket_index].is_direct_mapped());

  size_t allocated_slots = 0;
  // Same as calling RawAlloc() |count| times, but acquires the lock only once.
  internal::ScopedGuard<internal::ThreadSafe> guard(root_->lock_);
  for (int i = 0; i < count; i++) {
    // Thread cache fill should not trigger expensive operations, to not grab
    // the lock for a long time needlessly, but also to not inflate memory
    // usage. Indeed, without PartitionAllocFastPathOrReturnNull, cache fill may
    // activate a new PartitionPage, or even a new SuperPage, which is clearly
    // not desirable.
    //
    // |raw_size| is set to the slot size, as we don't know it. However, it is
    // only used for direct-mapped allocations and single-slot ones anyway,
    // which are not handled here.
    void* ptr = root_->AllocFromBucket(
        &root_->buckets[bucket_index],
        PartitionAllocFastPathOrReturnNull | PartitionAllocReturnNull,
        root_->buckets[bucket_index].slot_size /* raw_size */,
        PartitionPageSize(), &usable_size, &is_already_zeroed);

    // Either the previous allocation would require a slow path allocation, or
    // the central allocator is out of memory. If the bucket was filled with
    // some objects, then the allocation will be handled normally. Otherwise,
    // this goes to the central allocator, which will service the allocation,
    // return nullptr or crash.
    if (!ptr)
      break;

    allocated_slots++;
    PutInBucket(bucket, ptr);
  }

  cached_memory_ += allocated_slots * bucket.slot_size;
}

void ThreadCache::ClearBucket(ThreadCache::Bucket& bucket, size_t limit) {
  // Avoids acquiring the lock needlessly.
  if (!bucket.count || bucket.count <= limit)
    return;

  // This serves two purposes: error checking and avoiding stalls when grabbing
  // the lock:
  // 1. Error checking: this is pretty clear. Since this path is taken
  //    infrequently, and is going to walk the entire freelist anyway, its
  //    incremental cost should be very small. Indeed, we free from the tail of
  //    the list, so all calls here will end up walking the entire freelist, and
  //    incurring the same amount of cache misses.
  // 2. Avoiding stalls: If one of the freelist accesses in |FreeAfter()|
  //    triggers a major page fault, and we are running on a low-priority
  //    thread, we don't want the thread to be blocked while holding the lock,
  //    causing a priority inversion.
  bucket.freelist_head->CheckFreeList(bucket.slot_size);

  uint8_t count_before = bucket.count;
  if (limit == 0) {
    FreeAfter(bucket.freelist_head, bucket.slot_size);
    bucket.freelist_head = nullptr;
  } else {
    // Free the *end* of the list, not the head, since the head contains the
    // most recently touched memory.
    auto* head = bucket.freelist_head;
    size_t items = 1;  // Cannot free the freelist head.
    while (items < limit) {
      head = head->GetNext(bucket.slot_size);
      items++;
    }
    FreeAfter(head->GetNext(bucket.slot_size), bucket.slot_size);
    head->SetNext(nullptr);
  }
  bucket.count = limit;
  uint8_t count_after = bucket.count;
  size_t freed_memory = (count_before - count_after) * bucket.slot_size;
  PA_DCHECK(cached_memory_ >= freed_memory);
  cached_memory_ -= freed_memory;

  PA_DCHECK(cached_memory_ == CachedMemory());
}

void ThreadCache::FreeAfter(PartitionFreelistEntry* head, size_t slot_size) {
  // Acquire the lock once. Deallocation from the same bucket are likely to be
  // hitting the same cache lines in the central allocator, and lock
  // acquisitions can be expensive.
  internal::ScopedGuard<internal::ThreadSafe> guard(root_->lock_);
  while (head) {
    void* ptr = head;
    head = head->GetNext(slot_size);
    root_->RawFreeLocked(ptr);
  }
}

void ThreadCache::ResetForTesting() {
  stats_.alloc_count = 0;
  stats_.alloc_hits = 0;
  stats_.alloc_misses = 0;

  stats_.alloc_miss_empty = 0;
  stats_.alloc_miss_too_large = 0;

  stats_.cache_fill_count = 0;
  stats_.cache_fill_hits = 0;
  stats_.cache_fill_misses = 0;

  stats_.batch_fill_count = 0;

  stats_.bucket_total_memory = 0;
  stats_.metadata_overhead = 0;

  Purge();
  PA_CHECK(cached_memory_ == 0u);
  should_purge_.store(false, std::memory_order_relaxed);
}

size_t ThreadCache::CachedMemory() const {
  size_t total = 0;
  for (const Bucket& bucket : buckets_)
    total += bucket.count * static_cast<size_t>(bucket.slot_size);

  return total;
}

void ThreadCache::AccumulateStats(ThreadCacheStats* stats) const {
  stats->alloc_count += stats_.alloc_count;
  stats->alloc_hits += stats_.alloc_hits;
  stats->alloc_misses += stats_.alloc_misses;

  stats->alloc_miss_empty += stats_.alloc_miss_empty;
  stats->alloc_miss_too_large += stats_.alloc_miss_too_large;

  stats->cache_fill_count += stats_.cache_fill_count;
  stats->cache_fill_hits += stats_.cache_fill_hits;
  stats->cache_fill_misses += stats_.cache_fill_misses;

  stats->batch_fill_count += stats_.batch_fill_count;

#if defined(PA_THREAD_CACHE_ALLOC_STATS)
  for (size_t i = 0; i < kNumBuckets + 1; i++) {
    stats->allocs_per_bucket_[i] += stats_.allocs_per_bucket_[i];
  }
#endif  // defined(PA_THREAD_CACHE_ALLOC_STATS)

  // cached_memory_ is not necessarily equal to |CachedMemory()| here, since
  // this function can be called racily from another thread, to collect
  // statistics. Hence no DCHECK_EQ(CachedMemory(), cached_memory_).
  stats->bucket_total_memory += cached_memory_;

  stats->metadata_overhead += sizeof(*this);
}

void ThreadCache::SetShouldPurge() {
  should_purge_.store(true, std::memory_order_relaxed);
}

void ThreadCache::Purge() {
  PA_REENTRANCY_GUARD(is_in_thread_cache_);
  PurgeInternal();
}

// static
void ThreadCache::PurgeCurrentThread() {
  auto* tcache = Get();
  if (IsValid(tcache))
    tcache->Purge();
}

void ThreadCache::PurgeInternal() {
  should_purge_.store(false, std::memory_order_relaxed);
  // TODO(lizeb): Investigate whether lock acquisition should be less frequent.
  //
  // Note: iterate over all buckets, even the inactive ones. Since
  // |largest_active_bucket_index_| can be lowered at runtime, there may be
  // memory already cached in the inactive buckets. They should still be purged.
  for (auto& bucket : buckets_)
    ClearBucket(bucket, 0);
}

}  // namespace internal

}  // namespace base
