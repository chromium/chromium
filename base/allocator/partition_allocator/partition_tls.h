// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_TLS_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_TLS_H_

#include "base/allocator/partition_allocator/partition_alloc_check.h"
#include "base/compiler_specific.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_POSIX)
#include <pthread.h>
#endif

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_types.h"
#endif

// Barebones TLS implementation for use in PartitionAlloc. This doesn't use the
// general chromium TLS handling to avoid dependencies, but more importantly
// because it allocates memory.
namespace base {
namespace internal {

#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
typedef pthread_key_t PartitionTlsKey;

#if BUILDFLAG(IS_MAC) && defined(ARCH_CPU_X86_64)
namespace {

ALWAYS_INLINE void* FastTlsGet(intptr_t index) {
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
  asm("movq %%gs:(,%1,8), %0;" : "=r"(result) : "r"(index));

  return reinterpret_cast<void*>(result);
}

}  // namespace
#endif  // BUILDFLAG(IS_MAC) && defined(ARCH_CPU_X86_64)

ALWAYS_INLINE bool PartitionTlsCreate(PartitionTlsKey* key,
                                      void (*destructor)(void*)) {
  return !pthread_key_create(key, destructor);
}
ALWAYS_INLINE void* PartitionTlsGet(PartitionTlsKey key) {
#if BUILDFLAG(IS_MAC) && defined(ARCH_CPU_X86_64)
  PA_DCHECK(pthread_getspecific(key) == FastTlsGet(key));
  return FastTlsGet(key);
#else
  return pthread_getspecific(key);
#endif
}
ALWAYS_INLINE void PartitionTlsSet(PartitionTlsKey key, void* value) {
  int ret = pthread_setspecific(key, value);
  PA_DCHECK(!ret);
}
#elif BUILDFLAG(IS_WIN)
// Note: supports only a single TLS key on Windows. Not a hard constraint, may
// be lifted.
typedef unsigned long PartitionTlsKey;

BASE_EXPORT bool PartitionTlsCreate(PartitionTlsKey* key,
                                    void (*destructor)(void*));

ALWAYS_INLINE void* PartitionTlsGet(PartitionTlsKey key) {
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
  if (UNLIKELY(saved_error))
    SetLastError(saved_error);
  return ret;
}

ALWAYS_INLINE void PartitionTlsSet(PartitionTlsKey key, void* value) {
  BOOL ret = TlsSetValue(key, value);
  PA_DCHECK(ret);
}

// Registers a callback for DLL_PROCESS_DETACH events.
void PartitionTlsSetOnDllProcessDetach(void (*callback)());

#else
// Not supported.
typedef int PartitionTlsKey;
ALWAYS_INLINE bool PartitionTlsCreate(PartitionTlsKey* key,
                                      void (*destructor)(void*)) {
  // NOTIMPLEMENTED() may allocate, crash instead.
  IMMEDIATE_CRASH();
}
ALWAYS_INLINE void* PartitionTlsGet(PartitionTlsKey key) {
  IMMEDIATE_CRASH();
}
ALWAYS_INLINE void PartitionTlsSet(PartitionTlsKey key, void* value) {
  IMMEDIATE_CRASH();
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace internal
}  // namespace base

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_TLS_H_
