// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Its purpose is to preempt the Libc symbols for malloc/new so they call the
// shim layer entry points.

#ifdef PARTITION_ALLOC_SHIM_ALLOCATOR_SHIM_ALLOC_TOKEN_SYMBOLS_APPLE_H_
#error This header is meant to be included only once by allocator_shim.cc
#endif

#ifndef PARTITION_ALLOC_SHIM_ALLOCATOR_SHIM_ALLOC_TOKEN_SYMBOLS_APPLE_H_
#define PARTITION_ALLOC_SHIM_ALLOCATOR_SHIM_ALLOC_TOKEN_SYMBOLS_APPLE_H_

#include "partition_alloc/buildflags.h"

#if PA_BUILDFLAG(USE_ALLOCATOR_SHIM)
#include "partition_alloc/build_config.h"

#if PA_BUILDFLAG(IS_APPLE)
#include <malloc/malloc.h>
#else
#include <malloc.h>
#endif

#include "partition_alloc/shim/allocator_shim_internals.h"

#define SHIM_NO_INLINE_APPLE __attribute__((used, noinline))

// For the malloc/free family, we route calls to the malloc zone.
// Since malloc/free are implemented using malloc zones, redirecting them to
// ShimMalloc via symbol definition would cause the following differences:
// 1. Malloc zones are initialized at runtime, whereas symbol definitions are
//    active from process startup.
// 2. Malloc zone methods always redirect to the main executable's allocator
//    shim. In contrast, symbol definitions use the shim defined within the
//    dynamic library.
extern "C" {

SHIM_NO_INLINE_APPLE void* __alloc_token_malloc(size_t size,
                                                size_t alloc_token) noexcept {
  return malloc_zone_malloc(malloc_default_zone(), size);
}

SHIM_NO_INLINE_APPLE void* __alloc_token_realloc(void* ptr,
                                                 size_t size,
                                                 size_t alloc_token) noexcept {
  return malloc_zone_realloc(malloc_default_zone(), ptr, size);
}

SHIM_NO_INLINE_APPLE void* __alloc_token_calloc(size_t n,
                                                size_t size,
                                                size_t alloc_token) noexcept {
  return malloc_zone_calloc(malloc_default_zone(), n, size);
}

SHIM_NO_INLINE_APPLE void* __alloc_token_memalign(size_t align,
                                                  size_t size,
                                                  size_t alloc_token) noexcept {
  return malloc_zone_memalign(malloc_default_zone(), align, size);
}

SHIM_NO_INLINE_APPLE void* __alloc_token_aligned_alloc(
    size_t align,
    size_t size,
    size_t alloc_token) noexcept {
  return malloc_zone_memalign(malloc_default_zone(), align, size);
}

SHIM_NO_INLINE_APPLE void* __alloc_token_valloc(size_t size,
                                                size_t alloc_token) noexcept {
  return malloc_zone_valloc(malloc_default_zone(), size);
}

SHIM_NO_INLINE_APPLE void* __alloc_token_pvalloc(size_t size,
                                                 size_t alloc_token) noexcept {
  return malloc_zone_valloc(malloc_default_zone(), size);
}

__attribute__((no_sanitize("alloc-token"))) SHIM_NO_INLINE_APPLE int
__alloc_token_posix_memalign(void** r,
                             size_t a,
                             size_t s,
                             size_t alloc_token) noexcept {
  return posix_memalign(r, a, s);
}

SHIM_NO_INLINE_APPLE void* __alloc_token__Znwm(size_t size,
                                               size_t alloc_token) {
  return ShimCppNew(size, allocator_shim::AllocToken(alloc_token));
}

SHIM_NO_INLINE_APPLE void* __alloc_token__Znam(size_t size,
                                               size_t alloc_token) {
  return ShimCppNew(size, allocator_shim::AllocToken(alloc_token));
}

SHIM_NO_INLINE_APPLE void* __alloc_token__ZnwmRKSt9nothrow_t(
    size_t size,
    const std::nothrow_t&,
    size_t alloc_token) noexcept {
  return ShimCppNewNoThrow(size, allocator_shim::AllocToken(alloc_token));
}

SHIM_NO_INLINE_APPLE void* __alloc_token__ZnamRKSt9nothrow_t(
    size_t size,
    const std::nothrow_t&,
    size_t alloc_token) noexcept {
  return ShimCppNewNoThrow(size, allocator_shim::AllocToken(alloc_token));
}

SHIM_NO_INLINE_APPLE void* __alloc_token__ZnwmSt11align_val_t(
    size_t size,
    std::align_val_t alignment,
    size_t alloc_token) {
  return ShimCppAlignedNew(size, static_cast<size_t>(alignment),
                           allocator_shim::AllocToken(alloc_token));
}

SHIM_NO_INLINE_APPLE void* __alloc_token__ZnamSt11align_val_t(
    size_t size,
    std::align_val_t alignment,
    size_t alloc_token) {
  return ShimCppAlignedNew(size, static_cast<size_t>(alignment),
                           allocator_shim::AllocToken(alloc_token));
}

SHIM_NO_INLINE_APPLE void* __alloc_token__ZnwmSt11align_val_tRKSt9nothrow_t(
    size_t size,
    std::align_val_t alignment,
    const std::nothrow_t& t,
    size_t alloc_token) noexcept {
  return ShimCppAlignedNewNoThrow(size, static_cast<size_t>(alignment),
                                  allocator_shim::AllocToken(alloc_token));
}

SHIM_NO_INLINE_APPLE void* __alloc_token__ZnamSt11align_val_tRKSt9nothrow_t(
    size_t size,
    std::align_val_t alignment,
    const std::nothrow_t&,
    size_t alloc_token) noexcept {
  return ShimCppAlignedNewNoThrow(size, static_cast<size_t>(alignment),
                                  allocator_shim::AllocToken(alloc_token));
}

}  // extern "C"

#undef SHIM_NO_INLINE_APPLE

#endif  // PA_BUILDFLAG(USE_ALLOCATOR_SHIM)

#endif  // PARTITION_ALLOC_SHIM_ALLOCATOR_SHIM_ALLOC_TOKEN_SYMBOLS_APPLE_H_
