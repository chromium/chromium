// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Its purpose is to preempt the Libc symbols for malloc/new so they call the
// shim layer entry points.

#ifdef PARTITION_ALLOC_SHIM_ALLOCATOR_SHIM_ALLOC_TOKEN_SYMBOLS_H_
#error This header is meant to be included only once by allocator_shim.cc
#endif

#ifndef PARTITION_ALLOC_SHIM_ALLOCATOR_SHIM_ALLOC_TOKEN_SYMBOLS_H_
#define PARTITION_ALLOC_SHIM_ALLOCATOR_SHIM_ALLOC_TOKEN_SYMBOLS_H_

#include "partition_alloc/buildflags.h"

#if PA_BUILDFLAG(USE_ALLOCATOR_SHIM)
#include "partition_alloc/build_config.h"

#if PA_BUILDFLAG(IS_APPLE)
#include <malloc/malloc.h>
#else
#include <malloc.h>
#endif

#include "partition_alloc/shim/allocator_shim_internals.h"

extern "C" {

SHIM_ALWAYS_EXPORT void* __alloc_token_malloc(size_t size,
                                              size_t alloc_token) noexcept {
  return ShimMalloc(size, nullptr, allocator_shim::AllocToken(alloc_token));
}

SHIM_ALWAYS_EXPORT void* __alloc_token_realloc(void* ptr,
                                               size_t size,
                                               size_t alloc_token) noexcept {
  return ShimRealloc(ptr, size, nullptr,
                     allocator_shim::AllocToken(alloc_token));
}

SHIM_ALWAYS_EXPORT void* __alloc_token_calloc(size_t n,
                                              size_t size,
                                              size_t alloc_token) noexcept {
  return ShimCalloc(n, size, nullptr, allocator_shim::AllocToken(alloc_token));
}

SHIM_ALWAYS_EXPORT void* __alloc_token_memalign(size_t align,
                                                size_t size,
                                                size_t alloc_token) noexcept {
  return ShimMemalign(align, size, nullptr,
                      allocator_shim::AllocToken(alloc_token));
}

SHIM_ALWAYS_EXPORT void* __alloc_token_aligned_alloc(
    size_t align,
    size_t size,
    size_t alloc_token) noexcept {
  return ShimMemalign(align, size, nullptr,
                      allocator_shim::AllocToken(alloc_token));
}

SHIM_ALWAYS_EXPORT void* __alloc_token_valloc(size_t size,
                                              size_t alloc_token) noexcept {
  return ShimValloc(size, nullptr, allocator_shim::AllocToken(alloc_token));
}

SHIM_ALWAYS_EXPORT void* __alloc_token_pvalloc(size_t size,
                                               size_t alloc_token) noexcept {
  return ShimPvalloc(size, allocator_shim::AllocToken(alloc_token));
}

SHIM_ALWAYS_EXPORT int __alloc_token_posix_memalign(
    void** r,
    size_t a,
    size_t s,
    size_t alloc_token) noexcept {
  return ShimPosixMemalign(r, a, s, allocator_shim::AllocToken(alloc_token));
}

// The mangled name of operator new differs between operator new(unsigned long)
// and operator new(unsigned int). Therefore, we need to define the symbols
// differently depending on the size of size_t.
#if __SIZEOF_SIZE_T__ != __SIZEOF_INT__

SHIM_ALWAYS_EXPORT void* __alloc_token__Znwm(size_t size, size_t alloc_token) {
  return ShimCppNew(size, allocator_shim::AllocToken(alloc_token));
}

SHIM_ALWAYS_EXPORT void* __alloc_token__Znam(size_t size, size_t alloc_token) {
  return ShimCppNew(size, allocator_shim::AllocToken(alloc_token));
}

SHIM_ALWAYS_EXPORT void* __alloc_token__ZnwmRKSt9nothrow_t(
    size_t size,
    const std::nothrow_t&,
    size_t alloc_token) noexcept {
  return ShimCppNewNoThrow(size, allocator_shim::AllocToken(alloc_token));
}

SHIM_ALWAYS_EXPORT void* __alloc_token__ZnamRKSt9nothrow_t(
    size_t size,
    const std::nothrow_t&,
    size_t alloc_token) noexcept {
  return ShimCppNewNoThrow(size, allocator_shim::AllocToken(alloc_token));
}

SHIM_ALWAYS_EXPORT void* __alloc_token__ZnwmSt11align_val_t(
    size_t size,
    std::align_val_t alignment,
    size_t alloc_token) {
  return ShimCppAlignedNew(size, static_cast<size_t>(alignment),
                           allocator_shim::AllocToken(alloc_token));
}

SHIM_ALWAYS_EXPORT void* __alloc_token__ZnamSt11align_val_t(
    size_t size,
    std::align_val_t alignment,
    size_t alloc_token) {
  return ShimCppAlignedNew(size, static_cast<size_t>(alignment),
                           allocator_shim::AllocToken(alloc_token));
}

SHIM_ALWAYS_EXPORT void* __alloc_token__ZnwmSt11align_val_tRKSt9nothrow_t(
    size_t size,
    std::align_val_t alignment,
    const std::nothrow_t& t,
    size_t alloc_token) noexcept {
  return ShimCppAlignedNewNoThrow(size, static_cast<size_t>(alignment),
                                  allocator_shim::AllocToken(alloc_token));
}

SHIM_ALWAYS_EXPORT void* __alloc_token__ZnamSt11align_val_tRKSt9nothrow_t(
    size_t size,
    std::align_val_t alignment,
    const std::nothrow_t&,
    size_t alloc_token) noexcept {
  return ShimCppAlignedNewNoThrow(size, static_cast<size_t>(alignment),
                                  allocator_shim::AllocToken(alloc_token));
}

#else

SHIM_ALWAYS_EXPORT void* __alloc_token__Znwj(size_t size, size_t alloc_token) {
  return ShimCppNew(size, allocator_shim::AllocToken(alloc_token));
}

SHIM_ALWAYS_EXPORT void* __alloc_token__Znaj(size_t size, size_t alloc_token) {
  return ShimCppNew(size, allocator_shim::AllocToken(alloc_token));
}

SHIM_ALWAYS_EXPORT void* __alloc_token__ZnwjRKSt9nothrow_t(
    size_t size,
    const std::nothrow_t&,
    size_t alloc_token) noexcept {
  return ShimCppNewNoThrow(size, allocator_shim::AllocToken(alloc_token));
}

SHIM_ALWAYS_EXPORT void* __alloc_token__ZnajRKSt9nothrow_t(
    size_t size,
    const std::nothrow_t&,
    size_t alloc_token) noexcept {
  return ShimCppNewNoThrow(size, allocator_shim::AllocToken(alloc_token));
}

SHIM_ALWAYS_EXPORT void* __alloc_token__ZnwjSt11align_val_t(
    size_t size,
    std::align_val_t alignment,
    size_t alloc_token) {
  return ShimCppAlignedNew(size, static_cast<size_t>(alignment),
                           allocator_shim::AllocToken(alloc_token));
}

SHIM_ALWAYS_EXPORT void* __alloc_token__ZnajSt11align_val_t(
    size_t size,
    std::align_val_t alignment,
    size_t alloc_token) {
  return ShimCppAlignedNew(size, static_cast<size_t>(alignment),
                           allocator_shim::AllocToken(alloc_token));
}

SHIM_ALWAYS_EXPORT void* __alloc_token__ZnwjSt11align_val_tRKSt9nothrow_t(
    size_t size,
    std::align_val_t alignment,
    const std::nothrow_t& t,
    size_t alloc_token) noexcept {
  return ShimCppAlignedNewNoThrow(size, static_cast<size_t>(alignment),
                                  allocator_shim::AllocToken(alloc_token));
}

SHIM_ALWAYS_EXPORT void* __alloc_token__ZnajSt11align_val_tRKSt9nothrow_t(
    size_t size,
    std::align_val_t alignment,
    const std::nothrow_t&,
    size_t alloc_token) noexcept {
  return ShimCppAlignedNewNoThrow(size, static_cast<size_t>(alignment),
                                  allocator_shim::AllocToken(alloc_token));
}

#endif  // __SIZEOF_SIZE_T__ != __SIZEOF_INT__

}  // extern "C"

#endif  // PA_BUILDFLAG(USE_ALLOCATOR_SHIM)

#endif  // PARTITION_ALLOC_SHIM_ALLOCATOR_SHIM_ALLOC_TOKEN_SYMBOLS_H_
