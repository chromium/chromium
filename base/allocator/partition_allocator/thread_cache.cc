// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/thread_cache.h"

#include <sys/types.h>
#include <atomic>
#include <vector>

#include "base/allocator/partition_allocator/partition_alloc.h"
#include "base/allocator/partition_allocator/partition_alloc_check.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"

namespace base {

namespace internal {

namespace {

ThreadCacheRegistry g_instance;

}

BASE_EXPORT PartitionTlsKey g_thread_cache_key;

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

constexpr base::TimeDelta ThreadCacheRegistry::kPurgeInterval;

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
    if (!tcache)
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

  {
    PartitionAutoLock scoped_locker(GetLock());
    ThreadCache* tcache = list_head_;
    while (tcache) {
      // Cannot purge directly, need to ask the other thread to purge "at some
      // point".
      // Note that this will not work if the other thread is sleeping forever.
      // TODO(lizeb): Handle sleeping threads.
      if (tcache != current_thread_tcache)
        tcache->SetShouldPurge();
      tcache = tcache->next_;
    }
  }

  // May take a while, don't hold the lock while purging.
  if (current_thread_tcache)
    current_thread_tcache->Purge();
}

void ThreadCacheRegistry::StartPeriodicPurge() {
  ThreadCache::EnsureThreadSpecificDataInitialized();

  // Can be called several times, don't post multiple tasks.
  if (!has_pending_purge_task_)
    PostDelayedPurgeTask();
}

void ThreadCacheRegistry::PostDelayedPurgeTask() {
  PA_DCHECK(!has_pending_purge_task_);
  ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ThreadCacheRegistry::PeriodicPurge,
                     base::Unretained(this)),
      kPurgeInterval);
  has_pending_purge_task_ = true;
}

void ThreadCacheRegistry::PeriodicPurge() {
  has_pending_purge_task_ = false;
  ThreadCache* tcache = ThreadCache::Get();
  PA_DCHECK(tcache);
  uint64_t allocations = tcache->stats_.alloc_count;
  uint64_t allocations_since_last_purge =
      allocations - allocations_at_last_purge_;

  // Purge should not run when there is little activity in the process. We
  // assume that the main thread is a reasonable proxy for the process activity,
  // where the main thread is the current one.
  //
  // If we didn't see enough allocations since the last purge, don't schedule a
  // new one, and ask the thread cache to notify us of deallocations. This makes
  // the next |kMinMainThreadAllocationsForPurging| deallocations slightly
  // slower.
  //
  // Once the threshold is reached, reschedule a purge task. We count
  // deallocations rather than allocations because these are the ones that fill
  // the cache, and also because we already have a check on the deallocation
  // path, not on the allocation one that we don't want to slow down.
  bool enough_allocations =
      allocations_since_last_purge >= kMinMainThreadAllocationsForPurging;
  tcache->SetNotifiesRegistry(!enough_allocations);
  deallocations_ = 0;
  PurgeAll();

  if (enough_allocations) {
    allocations_at_last_purge_ = allocations;
    PostDelayedPurgeTask();
  }
}

void ThreadCacheRegistry::OnDeallocation() {
  deallocations_++;
  if (deallocations_ > kMinMainThreadAllocationsForPurging) {
    ThreadCache* tcache = ThreadCache::Get();
    PA_DCHECK(tcache);

    deallocations_ = 0;
    tcache->SetNotifiesRegistry(false);

    if (has_pending_purge_task_)
      return;

    // This is called from the thread cache, which is called from the central
    // allocator. This means that any allocation made by task posting will make
    // it reentrant, unless we disable the thread cache.
    tcache->Disable();
    PostDelayedPurgeTask();
    tcache->Enable();
  }
}

void ThreadCacheRegistry::ResetForTesting() {
  PA_CHECK(!has_pending_purge_task_);
  allocations_at_last_purge_ = 0;
  deallocations_ = 0;
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
  PA_CHECK(root->buckets[kBucketCount - 1].slot_size == kSizeThreshold);

  EnsureThreadSpecificDataInitialized();

  // Make sure that only one PartitionRoot wants a thread cache.
  PartitionRoot<ThreadSafe>* expected = nullptr;
  if (!g_thread_cache_root.compare_exchange_strong(expected, nullptr,
                                                   std::memory_order_seq_cst,
                                                   std::memory_order_seq_cst)) {
    PA_CHECK(false)
        << "Only one PartitionRoot is allowed to have a thread cache";
  }

#if defined(OS_WIN)
  PartitionTlsSetOnDllProcessDetach(OnDllProcessDetach);
#endif
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
  size_t usable_size;
  bool already_zeroed;

  auto* bucket =
      root->buckets + PartitionRoot<internal::ThreadSafe>::SizeToBucketIndex(
                          sizeof(ThreadCache));
  void* buffer =
      root->RawAlloc(bucket, PartitionAllocZeroFill, sizeof(ThreadCache),
                     &usable_size, &already_zeroed);
  ThreadCache* tcache = new (buffer) ThreadCache(root);

  // This may allocate.
  PartitionTlsSet(g_thread_cache_key, tcache);

  return tcache;
}

ThreadCache::ThreadCache(PartitionRoot<ThreadSafe>* root)
    : buckets_(),
      stats_(),
      root_(root),
      registry_(&ThreadCacheRegistry::Instance()),
      next_(nullptr),
      prev_(nullptr) {
  registry_->RegisterThreadCache(this);

  for (int index = 0; index < kBucketCount; index++) {
    const auto& root_bucket = root->buckets[index];
    Bucket* tcache_bucket = &buckets_[index];
    // Invalid bucket.
    if (!root_bucket.active_slot_spans_head) {
      // Explicitly set this, as size computations iterate over all buckets.
      tcache_bucket->limit = 0;
      tcache_bucket->count = 0;
      tcache_bucket->slot_size = 0;
      continue;
    }

    // Smaller allocations are more frequent, and more performance-sensitive.
    // Cache more small objects, and fewer larger ones, to save memory.
    size_t slot_size = root_bucket.slot_size;
    PA_CHECK(slot_size <= std::numeric_limits<uint16_t>::max());
    tcache_bucket->slot_size = static_cast<uint16_t>(slot_size);

    if (slot_size <= 128) {
      tcache_bucket->limit = kMaxCountPerBucket;
    } else if (slot_size <= 256) {
      tcache_bucket->limit = kMaxCountPerBucket / 2;
    } else {
      tcache_bucket->limit = kMaxCountPerBucket / 4;
    }
  }
}

ThreadCache::~ThreadCache() {
  registry_->UnregisterThreadCache(this);
  Purge();
}

// static
void ThreadCache::Delete(void* tcache_ptr) {
  auto* tcache = reinterpret_cast<ThreadCache*>(tcache_ptr);
  auto* root = tcache->root_;
  reinterpret_cast<ThreadCache*>(tcache_ptr)->~ThreadCache();
  root->RawFree(tcache_ptr);
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
  int count = bucket.limit / kBatchFillRatio;

  size_t usable_size;
  bool is_already_zeroed;

  PA_DCHECK(!root_->buckets[bucket_index].CanStoreRawSize());
  PA_DCHECK(!root_->buckets[bucket_index].is_direct_mapped());

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
        root_->buckets[bucket_index].slot_size /* raw_size */, &usable_size,
        &is_already_zeroed);

    // Either the previous allocation would require a slow path allocation, or
    // the central allocator is out of memory. If the bucket was filled with
    // some objects, then the allocation will be handled normally. Otherwise,
    // this goes to the central allocator, which will service the allocation,
    // return nullptr or crash.
    if (!ptr)
      break;

    PutInBucket(bucket, ptr);
  }
}

void ThreadCache::ClearBucket(ThreadCache::Bucket& bucket, size_t limit) {
  // Avoids acquiring the lock needlessly.
  if (!bucket.count)
    return;

  // Acquire the lock once for the bucket. Allocations from the same bucket are
  // likely to be hitting the same cache lines in the central allocator, and
  // lock acquisitions can be expensive.
  internal::ScopedGuard<internal::ThreadSafe> guard(root_->lock_);
  while (bucket.count > limit) {
    auto* entry = bucket.freelist_head;
    PA_DCHECK(entry);
    bucket.freelist_head = entry->GetNext();

    root_->RawFreeLocked(entry);
    bucket.count--;
  }
  PA_DCHECK(bucket.count == limit);
}

void ThreadCache::HandleNonNormalMode() {
  switch (mode_.load(std::memory_order_relaxed)) {
    case Mode::kPurge:
      PurgeInternal();
      mode_.store(Mode::kNormal, std::memory_order_relaxed);
      break;

    case Mode::kNotifyRegistry:
      registry_->OnDeallocation();
      break;

    default:
      break;
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
  mode_.store(Mode::kNormal, std::memory_order_relaxed);
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

  for (const Bucket& bucket : buckets_) {
    stats->bucket_total_memory +=
        bucket.count * static_cast<size_t>(bucket.slot_size);
  }
  stats->metadata_overhead += sizeof(*this);
}

void ThreadCache::SetShouldPurge() {
  // Purge may be triggered by an external event, in which case it should not
  // take precedence over the notification mode, otherwise we risk disabling
  // periodic purge entirely.
  //
  // Also, no other thread can set this to notification mode.
  if (mode_.load(std::memory_order_relaxed) != Mode::kNormal)
    return;

  // We don't need any synchronization, and don't really care if the purge is
  // carried out "right away", hence relaxed atomics.
  mode_.store(Mode::kPurge, std::memory_order_relaxed);
}

void ThreadCache::SetNotifiesRegistry(bool enabled) {
  mode_.store(enabled ? Mode::kNotifyRegistry : Mode::kNormal,
              std::memory_order_relaxed);
}

void ThreadCache::Purge() {
  PA_REENTRANCY_GUARD(is_in_thread_cache_);
  PurgeInternal();
}

void ThreadCache::PurgeInternal() {
  for (auto& bucket : buckets_)
    ClearBucket(bucket, 0);
}

void ThreadCache::Disable() {
  root_->with_thread_cache = false;
}

void ThreadCache::Enable() {
  root_->with_thread_cache = true;
}

}  // namespace internal

}  // namespace base
