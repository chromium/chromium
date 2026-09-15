// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
#include "partition_alloc/partition_tls.h"
// clang-format on

#include <windows.h>

#include "partition_alloc/build_config.h"

#if !PA_BUILDFLAG(IS_WIN)
#error "This file is only for Windows builds."
#endif

#include "partition_alloc/internal/thread_cache_internal.h"
#include "partition_alloc/internal_allocator.h"
#include "partition_alloc/partition_alloc_config.h"
#include "partition_alloc/partition_lock.h"

namespace partition_alloc::internal {

PA_COMPONENT_EXPORT(PARTITION_ALLOC)
std::atomic<PartitionTlsKey> g_tls_key{kInvalidTlsKey};

namespace {

void (*g_on_dll_process_detach)() = nullptr;
Lock g_tls_key_lock;

// Static callback function to call with each thread termination.
void NTAPI PartitionTlsOnThreadExit(PVOID module,
                                    DWORD reason,
                                    PVOID reserved) {
  if (reason != DLL_THREAD_DETACH && reason != DLL_PROCESS_DETACH) {
    return;
  }

  if (reason == DLL_PROCESS_DETACH && g_on_dll_process_detach) {
    g_on_dll_process_detach();
  }

  PartitionTlsKey key = GetTlsKey();
  if (key != kInvalidTlsKey) {
    void* per_thread_data = PartitionTlsGet(key);
    if (per_thread_data) {
      TlsDestructor(per_thread_data);
    }
  }
}

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

bool PartitionTlsCreate(PartitionTlsKey* key, void (*destructor)(void*)) {
  PartitionTlsKey value = TlsAlloc();
  if (value != TLS_OUT_OF_INDEXES) {
    *key = value;
    return true;
  }
  return false;
}

void PartitionTlsSetOnDllProcessDetach(void (*callback)()) {
  g_on_dll_process_detach = callback;
}

}  // namespace partition_alloc::internal

// See thread_local_storage_win.cc for details and reference.
//
// The callback has to be in any section between .CRT$XLA and .CRT$XLZ, as these
// are sentinels used by the TLS code to find the callback array bounds. As we
// don't particularly care about where we are called but would prefer to be
// deinitialized towards the end (in particular after Chromium's TLS), we locate
// ourselves in .CRT$XLY.

// Force a reference to _tls_used to make the linker create the TLS directory if
// it's not already there.  (e.g. if __declspec(thread) is not used).  Force a
// reference to partition_tls_thread_exit_callback to prevent whole program
// optimization from discarding the variable.
#ifdef _WIN64

#pragma comment(linker, "/INCLUDE:_tls_used")
#pragma comment(linker, "/INCLUDE:partition_tls_thread_exit_callback")

#else  // _WIN64

#pragma comment(linker, "/INCLUDE:__tls_used")
#pragma comment(linker, "/INCLUDE:_partition_tls_thread_exit_callback")

#endif  // _WIN64

// extern "C" suppresses C++ name mangling so we know the symbol name for the
// linker /INCLUDE:symbol pragma above.
extern "C" {
// The linker must not discard partition_tls_thread_exit_callback.  (We force a
// reference to this variable with a linker /INCLUDE:symbol pragma to ensure
// that.) If this variable is discarded, PartitionTlsOnThreadExit will never be
// called.
#ifdef _WIN64

// .CRT section is merged with .rdata on x64 so it must be constant data.
#pragma const_seg(".CRT$XLY")
// When defining a const variable, it must have external linkage to be sure the
// linker doesn't discard it.
extern const PIMAGE_TLS_CALLBACK partition_tls_thread_exit_callback;
const PIMAGE_TLS_CALLBACK partition_tls_thread_exit_callback =
    partition_alloc::internal::PartitionTlsOnThreadExit;

// Reset the default section.
#pragma const_seg()

#else  // _WIN64

#pragma data_seg(".CRT$XLY")
PIMAGE_TLS_CALLBACK partition_tls_thread_exit_callback =
    partition_alloc::internal::PartitionTlsOnThreadExit;

// Reset the default section.
#pragma data_seg()

#endif  // _WIN64
}  // extern "C"
