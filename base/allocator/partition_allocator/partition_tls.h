// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_TLS_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_TLS_H_

#include "base/allocator/partition_allocator/partition_alloc_check.h"
#include "base/compiler_specific.h"
#include "build/build_config.h"

#if defined(OS_POSIX)
#include <pthread.h>
#endif

#if defined(OS_WIN)
#include "base/win/windows_types.h"
#endif

// Barebones TLS implementation for use in PartitionAlloc. This doesn't use the
// general chromium TLS handling to avoid dependencies, but more importantly
// because it allocates memory.
namespace base {
namespace internal {

#if defined(OS_POSIX)
typedef pthread_key_t PartitionTlsKey;

ALWAYS_INLINE bool PartitionTlsCreate(PartitionTlsKey* key,
                                      void (*destructor)(void*)) {
  return !pthread_key_create(key, destructor);
}
ALWAYS_INLINE void* PartitionTlsGet(PartitionTlsKey key) {
  return pthread_getspecific(key);
}
ALWAYS_INLINE void PartitionTlsSet(PartitionTlsKey key, void* value) {
  int ret = pthread_setspecific(key, value);
  PA_DCHECK(!ret);
}
#elif defined(OS_WIN)
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
#endif  // defined(OS_WIN)

}  // namespace internal
}  // namespace base

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_TLS_H_
