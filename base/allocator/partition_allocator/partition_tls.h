// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_TLS_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_TLS_H_

#include "base/allocator/partition_allocator/partition_alloc_check.h"
#include "build/build_config.h"

#if defined(OS_POSIX)
#include <pthread.h>
#endif

// Barebones TLS implementation for use in PartitionAlloc. This doesn't use the
// general chromium TLS handling to avoid dependencies, but more importantly
// because it allocates memory.
namespace base {
namespace internal {

#if defined(OS_POSIX)
typedef pthread_key_t PartitionTlsKey;

inline bool PartitionTlsCreate(PartitionTlsKey* key,
                               void (*destructor)(void*)) {
  return !pthread_key_create(key, destructor);
}
inline void* PartitionTlsGet(PartitionTlsKey key) {
  return pthread_getspecific(key);
}
inline void PartitionTlsSet(PartitionTlsKey key, void* value) {
  int ret = pthread_setspecific(key, value);
  PA_DCHECK(!ret);
}
#else
// Not implemented.
typedef int PartitionTlsKey;

inline bool PartitionTlsCreate(PartitionTlsKey* key,
                               void (*destructor)(void*)) {
  // Cannot use NOIMPLEMENTED() as it may allocate.
  IMMEDIATE_CRASH();
}
inline void* PartitionTlsGet(PartitionTlsKey key) {
  IMMEDIATE_CRASH();
}
inline void PartitionTlsSet(PartitionTlsKey key, void* value) {
  IMMEDIATE_CRASH();
}
#endif  // defined(OS_POSIX)

}  // namespace internal
}  // namespace base

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_TLS_H_
