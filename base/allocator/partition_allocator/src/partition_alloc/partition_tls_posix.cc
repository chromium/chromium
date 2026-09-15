// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/partition_tls.h"

#include <pthread.h>

#include "partition_alloc/build_config.h"
#include "partition_alloc/internal/thread_cache_internal.h"
#include "partition_alloc/internal_allocator.h"
#include "partition_alloc/partition_alloc_config.h"
#include "partition_alloc/partition_lock.h"

#if !PA_BUILDFLAG(IS_POSIX) && !PA_BUILDFLAG(IS_FUCHSIA)
#error "This file is only for POSIX or Fuchsia platforms."
#endif

namespace partition_alloc::internal {

PA_COMPONENT_EXPORT(PARTITION_ALLOC)
std::atomic<PartitionTlsKey> g_tls_key{kInvalidTlsKey};

namespace {
Lock g_tls_key_lock;
}  // namespace

void EnsureThreadSpecificDataInitialized() {
  if (GetTlsKey() != kInvalidTlsKey) {
    return;
  }
  ScopedGuard scoped_locker(g_tls_key_lock);
  if (GetTlsKey() != kInvalidTlsKey) {
    return;
  }

  PartitionTlsKey key;
  bool ok = PartitionTlsCreate(&key, TlsDestructor);
  PA_CHECK(ok);
  g_tls_key.store(key, std::memory_order_release);
}

PartitionTls* GetTlsSlowPath() {
  EnsureThreadSpecificDataInitialized();
  PartitionTlsKey key = GetTlsKey();
  if (key == kInvalidTlsKey) {
    return nullptr;
  }
  void* existing = PartitionTlsGet(key);
  if (IsTlsPtrTombstone(existing)) {
    return nullptr;
  }
  if (existing) {
    return static_cast<PartitionTls*>(existing);
  }
  PartitionTls* tls = ConstructAtInternalPartition<PartitionTls>();
  PartitionTlsSet(key, tls);
  return tls;
}

void TlsDestructor(void* value) {
  if (!IsTlsPtrValid(value)) {
    return;
  }
  auto* tls = static_cast<PartitionTls*>(value);

  PartitionTlsKey key = GetTlsKey();
  if (key != kInvalidTlsKey) {
    PartitionTlsSet(key, reinterpret_cast<void*>(kTombstone));
  }

  for (size_t i = 0; i < kMaxThreadCacheIndex; i++) {
    auto* tcache = tls->GetThreadCache(i);
    if (ThreadCache::IsValid(tcache)) {
#if PA_CONFIG(THREAD_CACHE_FAST_TLS)
      PA_UNSAFE_TODO(internal::g_thread_caches[i]) = nullptr;
#endif
      tcache->~ThreadCache();
      tls->ClearThreadCache(i);
    }
  }

#if PA_CONFIG(THREAD_CACHE_FAST_TLS)
  PA_UNSAFE_TODO(
      internal::g_thread_caches[internal::kThreadCacheTombstoneIndex]) =
      reinterpret_cast<ThreadCache*>(kTombstone);
#endif

  DestroyAtInternalPartition(tls);
}

void RemoveTombstoneForTesting() {
  PartitionTlsKey key = GetTlsKey();
  if (key != kInvalidTlsKey) {
    if (IsTombstoneSlow()) {
      PartitionTlsSet(key, nullptr);
    }
  }
#if PA_CONFIG(THREAD_CACHE_FAST_TLS)
  PA_UNSAFE_TODO(
      internal::g_thread_caches[internal::kThreadCacheTombstoneIndex]) =
      nullptr;
#endif
}

}  // namespace partition_alloc::internal
