// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef BASE_ALLOCATOR_PARTITION_ALLOCATOR_SHIM_ALLOCATOR_SHIM_OVERRIDE_CPP_SYMBOLS_H_
#error This header is meant to be included only once by allocator_shim.cc
#endif

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_SHIM_ALLOCATOR_SHIM_OVERRIDE_CPP_SYMBOLS_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_SHIM_ALLOCATOR_SHIM_OVERRIDE_CPP_SYMBOLS_H_

// Preempt the default new/delete C++ symbols so they call the shim entry
// points. This file is strongly inspired by tcmalloc's
// libc_override_redefine.h.

#include <new>

#include "base/allocator/partition_allocator/partition_alloc_base/compiler_specific.h"
#include "base/allocator/partition_allocator/shim/allocator_shim_internals.h"
#include "build/build_config.h"

#if !BUILDFLAG(IS_APPLE)
#define SHIM_CPP_SYMBOLS_EXPORT SHIM_ALWAYS_EXPORT
#else
// On Apple OSes, prefer not exporting these symbols (as this reverts to the
// default behavior, they are still exported in e.g. component builds). This is
// partly due to intentional limits on exported symbols in the main library, but
// it is also needless, since no library used on macOS imports these.
//
// TODO(lizeb): It may not be necessary anywhere to export these.
#define SHIM_CPP_SYMBOLS_EXPORT PA_NOINLINE
#endif

SHIM_CPP_SYMBOLS_EXPORT void* operator new(size_t size) {
  return ShimCppNew(size);
}

SHIM_CPP_SYMBOLS_EXPORT void operator delete(void* p) __THROW {
  ShimCppDelete(p);
}

SHIM_CPP_SYMBOLS_EXPORT void* operator new[](size_t size) {
  return ShimCppNew(size);
}

SHIM_CPP_SYMBOLS_EXPORT void operator delete[](void* p) __THROW {
  ShimCppDelete(p);
}

SHIM_CPP_SYMBOLS_EXPORT void* operator new(size_t size,
                                           const std::nothrow_t&) __THROW {
  return ShimCppNewNoThrow(size);
}

SHIM_CPP_SYMBOLS_EXPORT void* operator new[](size_t size,
                                             const std::nothrow_t&) __THROW {
  return ShimCppNewNoThrow(size);
}

SHIM_CPP_SYMBOLS_EXPORT void operator delete(void* p,
                                             const std::nothrow_t&) __THROW {
  ShimCppDelete(p);
}

SHIM_CPP_SYMBOLS_EXPORT void operator delete[](void* p,
                                               const std::nothrow_t&) __THROW {
  ShimCppDelete(p);
}

SHIM_CPP_SYMBOLS_EXPORT void operator delete(void* p, size_t) __THROW {
  ShimCppDelete(p);
}

SHIM_CPP_SYMBOLS_EXPORT void operator delete[](void* p, size_t) __THROW {
  ShimCppDelete(p);
}

SHIM_CPP_SYMBOLS_EXPORT void* operator new(std::size_t size,
                                           std::align_val_t alignment) {
  return ShimCppAlignedNew(size, static_cast<size_t>(alignment));
}

SHIM_CPP_SYMBOLS_EXPORT void* operator new(std::size_t size,
                                           std::align_val_t alignment,
                                           const std::nothrow_t&) __THROW {
  return ShimCppAlignedNew(size, static_cast<size_t>(alignment));
}

SHIM_CPP_SYMBOLS_EXPORT void operator delete(void* p,
                                             std::align_val_t) __THROW {
  ShimCppDelete(p);
}

SHIM_CPP_SYMBOLS_EXPORT void operator delete(void* p,
                                             std::size_t size,
                                             std::align_val_t) __THROW {
  ShimCppDelete(p);
}

SHIM_CPP_SYMBOLS_EXPORT void operator delete(void* p,
                                             std::align_val_t,
                                             const std::nothrow_t&) __THROW {
  ShimCppDelete(p);
}

SHIM_CPP_SYMBOLS_EXPORT void* operator new[](std::size_t size,
                                             std::align_val_t alignment) {
  return ShimCppAlignedNew(size, static_cast<size_t>(alignment));
}

SHIM_CPP_SYMBOLS_EXPORT void* operator new[](std::size_t size,
                                             std::align_val_t alignment,
                                             const std::nothrow_t&) __THROW {
  return ShimCppAlignedNew(size, static_cast<size_t>(alignment));
}

SHIM_CPP_SYMBOLS_EXPORT void operator delete[](void* p,
                                               std::align_val_t) __THROW {
  ShimCppDelete(p);
}

SHIM_CPP_SYMBOLS_EXPORT void operator delete[](void* p,
                                               std::size_t size,
                                               std::align_val_t) __THROW {
  ShimCppDelete(p);
}

SHIM_CPP_SYMBOLS_EXPORT void operator delete[](void* p,
                                               std::align_val_t,
                                               const std::nothrow_t&) __THROW {
  ShimCppDelete(p);
}

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_SHIM_ALLOCATOR_SHIM_OVERRIDE_CPP_SYMBOLS_H_
