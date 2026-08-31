// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PARTITION_ALLOC_PARTITION_TLS_H_
#define PARTITION_ALLOC_PARTITION_TLS_H_

#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>

#include "partition_alloc/build_config.h"
#include "partition_alloc/buildflags.h"
#include "partition_alloc/partition_alloc_base/compiler_specific.h"
#include "partition_alloc/partition_alloc_base/component_export.h"
#include "partition_alloc/partition_alloc_base/immediate_crash.h"
#include "partition_alloc/partition_alloc_check.h"

#if PA_BUILDFLAG(IS_POSIX) || PA_BUILDFLAG(IS_FUCHSIA)
#include <pthread.h>
#endif

#if PA_BUILDFLAG(IS_WIN)
#include "partition_alloc/partition_alloc_base/win/windows_types.h"
#endif

// Barebones TLS implementation for use in PartitionAlloc. This doesn't use the
// general chromium TLS handling to avoid dependencies, but more importantly
// because it allocates memory.
namespace partition_alloc::internal {

class ThreadCache;

constexpr inline size_t kMaxThreadCacheIndex = 4;

constexpr inline uintptr_t kTombstone = 1;
constexpr inline uintptr_t kTombstoneMask = ~kTombstone;

#if PA_BUILDFLAG(HAS_64_BIT_POINTERS)
constexpr inline size_t kThreadCacheStorageSize = 4096;
#else
constexpr inline size_t kThreadCacheStorageSize = 2048;
#endif

struct PartitionTls {
  alignas(uint64_t) std::array<std::array<uint8_t, kThreadCacheStorageSize>,
                               kMaxThreadCacheIndex> thread_cache_storage = {};
  bool disallow_allocations = false;

  PA_ALWAYS_INLINE ThreadCache* GetThreadCache(size_t index) {
    return reinterpret_cast<ThreadCache*>(&thread_cache_storage[index]);
  }

  PA_ALWAYS_INLINE void ClearThreadCache(size_t index) {
    thread_cache_storage[index].fill(0);
  }
};

#if PA_BUILDFLAG(IS_POSIX) || PA_BUILDFLAG(IS_FUCHSIA)
using PartitionTlsKey = pthread_key_t;

PA_ALWAYS_INLINE bool PartitionTlsCreate(PartitionTlsKey* key,
                                         void (*destructor)(void*)) {
  return !pthread_key_create(key, destructor);
}

// Only on x86_64, the implementation is not stable on ARM64. For instance, in
// macOS 11, the TPIDRRO_EL0 registers holds the CPU index in the low bits,
// which is not the case in macOS 12. See libsyscall/os/tsd.h in XNU
// (_os_tsd_get_direct() is used by pthread_getspecific() internally).
#if PA_BUILDFLAG(IS_MAC) && PA_BUILDFLAG(PA_ARCH_CPU_X86_64)

PA_ALWAYS_INLINE void* FastTlsGet(PartitionTlsKey index) {
  // On macOS, pthread_getspecific() is in libSystem, so a call to it has to go
  // through PLT. However, and contrary to some other platforms, *all* TLS keys
  // are in a static array in the thread structure. So they are *always* at a
  // fixed offset from the segment register holding the thread structure
  // address.
  //
  // We could use _pthread_getspecific_direct(), but it is not
  // exported. However, on all macOS versions we support, the TLS array is at
  // %gs. This is used in V8 to back up InternalGetExistingThreadLocal(), and
  // can also be seen by looking at pthread_getspecific() disassembly:
  //
  // libsystem_pthread.dylib`pthread_getspecific:
  // libsystem_pthread.dylib[0x7ff800316099] <+0>: movq   %gs:(,%rdi,8), %rax
  // libsystem_pthread.dylib[0x7ff8003160a2] <+9>: retq
  //
  // This function is essentially inlining the content of pthread_getspecific()
  // here.
  intptr_t result;
  static_assert(sizeof index <= sizeof(intptr_t));
  asm("movq %%gs:(,%1,8), %0;"
      : "=r"(result)
      : "r"(static_cast<intptr_t>(index)));

  return reinterpret_cast<void*>(result);
}

#endif  // PA_BUILDFLAG(IS_MAC) && PA_BUILDFLAG(PA_ARCH_CPU_X86_64)

PA_ALWAYS_INLINE void* PartitionTlsGet(PartitionTlsKey key) {
#if PA_BUILDFLAG(IS_MAC) && PA_BUILDFLAG(PA_ARCH_CPU_X86_64)
  PA_DCHECK(pthread_getspecific(key) == FastTlsGet(key));
  return FastTlsGet(key);
#else
  return pthread_getspecific(key);
#endif
}

PA_ALWAYS_INLINE void PartitionTlsSet(PartitionTlsKey key, void* value) {
  int ret = pthread_setspecific(key, value);
  PA_DCHECK(!ret);
}

#elif PA_BUILDFLAG(IS_WIN)
using PartitionTlsKey = unsigned long;

PA_COMPONENT_EXPORT(PARTITION_ALLOC)
bool PartitionTlsCreate(PartitionTlsKey* key, void (*destructor)(void*));

PA_ALWAYS_INLINE void* PartitionTlsGet(PartitionTlsKey key) {
  // Accessing TLS resets the last error, which then makes |GetLastError()|
  // return something misleading. While this means that properly using
  // |GetLastError()| is difficult, there is currently code in Chromium which
  // expects malloc() to *not* reset it. Meaning that we either have to fix this
  // code, or pay the cost of saving/restoring it.
  //
  // Source:
  // https://docs.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-tlsgetvalue
  // "Functions that return indications of failure call SetLastError() when they
  // fail. They generally do not call SetLastError() when they succeed. The
  // TlsGetValue() function is an exception to this general rule. The
  // TlsGetValue() function calls SetLastError() to clear a thread's last error
  // when it succeeds."
  DWORD saved_error = GetLastError();
  void* ret = TlsGetValue(key);
  // Only non-zero errors need to be restored.
  if (saved_error) [[unlikely]] {
    SetLastError(saved_error);
  }
  return ret;
}

PA_ALWAYS_INLINE void PartitionTlsSet(PartitionTlsKey key, void* value) {
  BOOL ret = TlsSetValue(key, value);
  PA_DCHECK(ret);
}

// Registers a callback for DLL_PROCESS_DETACH events.
PA_COMPONENT_EXPORT(PARTITION_ALLOC)
void PartitionTlsSetOnDllProcessDetach(void (*callback)());

#else
using PartitionTlsKey = int;

PA_ALWAYS_INLINE void* PartitionTlsGet(PartitionTlsKey key) = delete;

PA_ALWAYS_INLINE void PartitionTlsSet(PartitionTlsKey key,
                                      void* value) = delete;

#endif  // PA_BUILDFLAG(IS_WIN)

constexpr inline PartitionTlsKey kInvalidTlsKey =
    static_cast<PartitionTlsKey>(-1);

extern PA_COMPONENT_EXPORT(
    PARTITION_ALLOC) std::atomic<PartitionTlsKey> g_tls_key;

// Returns the global TLS key. It is safe to use relaxed memory ordering
// because:
// 1. `g_tls_key` is monotonically initialized once under a lock from
//    `kInvalidTlsKey` to a valid key and never modified or unset afterward.
// 2. If a thread sees a stale `kInvalidTlsKey`, it falls back to the slow path
//    (`EnsureThreadSpecificDataInitialized()`), which acquires `g_tls_key_lock`
//    and establishes synchronization with the initializing thread.
// 3. The key is an opaque scalar handle passed by value to OS TLS APIs. Each
//    thread independently allocates and accesses its own `PartitionTls` in its
//    own TLS slot, so there are no cross-thread data dependencies published via
//    this key.
PA_ALWAYS_INLINE PartitionTlsKey GetTlsKey() {
  return g_tls_key.load(std::memory_order_relaxed);
}

PA_ALWAYS_INLINE bool IsTlsPtrTombstone(void* ptr) {
  return reinterpret_cast<uintptr_t>(ptr) == kTombstone;
}

PA_ALWAYS_INLINE bool IsTlsPtrValid(void* ptr) {
  return reinterpret_cast<uintptr_t>(ptr) & kTombstoneMask;
}

PA_COMPONENT_EXPORT(PARTITION_ALLOC) PartitionTls* GetTlsSlowPath();

PA_ALWAYS_INLINE bool IsTombstoneSlow() {
  PartitionTlsKey key = GetTlsKey();
  if (key == kInvalidTlsKey) [[unlikely]] {
    return false;
  }
  void* ptr = PartitionTlsGet(key);
  return IsTlsPtrTombstone(ptr);
}

PA_ALWAYS_INLINE PartitionTls* GetTls() {
  PartitionTlsKey key = GetTlsKey();
  if (key == kInvalidTlsKey) [[unlikely]] {
    return nullptr;
  }
  auto* tls = reinterpret_cast<PartitionTls*>(PartitionTlsGet(key));
  if (!IsTlsPtrValid(tls)) [[unlikely]] {
    if (IsTlsPtrTombstone(tls)) {
      return nullptr;
    }
    tls = GetTlsSlowPath();
  }
  return tls;
}

PA_COMPONENT_EXPORT(PARTITION_ALLOC) void EnsureThreadSpecificDataInitialized();

PA_COMPONENT_EXPORT(PARTITION_ALLOC) void RemoveTombstoneForTesting();

void TlsDestructor(void* data);

}  // namespace partition_alloc::internal

#endif  // PARTITION_ALLOC_PARTITION_TLS_H_
