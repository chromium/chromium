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

#define DEFINE_ALLOC_TOKEN_STDLIB(id)                                          \
  SHIM_ALWAYS_EXPORT void* __alloc_token_##id##_malloc(size_t size) noexcept { \
    return ShimMalloc(size, nullptr, AllocToken(id));                          \
  }                                                                            \
  SHIM_ALWAYS_EXPORT void* __alloc_token_##id##_realloc(                       \
      void* ptr, size_t size) noexcept {                                       \
    return ShimRealloc(ptr, size, nullptr, AllocToken(id));                    \
  }                                                                            \
  SHIM_ALWAYS_EXPORT void* __alloc_token_##id##_calloc(size_t n,               \
                                                       size_t size) noexcept { \
    return ShimCalloc(n, size, nullptr, AllocToken(id));                       \
  }                                                                            \
  SHIM_ALWAYS_EXPORT void* __alloc_token_##id##_memalign(                      \
      size_t align, size_t size) noexcept {                                    \
    return ShimMemalign(align, size, nullptr, AllocToken(id));                 \
  }                                                                            \
  SHIM_ALWAYS_EXPORT void* __alloc_token_##id##_aligned_alloc(                 \
      size_t align, size_t size) noexcept {                                    \
    return ShimMemalign(align, size, nullptr, AllocToken(id));                 \
  }                                                                            \
  SHIM_ALWAYS_EXPORT void* __alloc_token_##id##_valloc(size_t size) noexcept { \
    return ShimValloc(size, nullptr, AllocToken(id));                          \
  }                                                                            \
  SHIM_ALWAYS_EXPORT void* __alloc_token_##id##_pvalloc(                       \
      size_t size) noexcept {                                                  \
    return ShimPvalloc(size, AllocToken(id));                                  \
    ;                                                                          \
  }                                                                            \
  SHIM_ALWAYS_EXPORT int __alloc_token_##id##_posix_memalign(                  \
      void** r, size_t a, size_t s) noexcept {                                 \
    return ShimPosixMemalign(r, a, s, AllocToken(id));                         \
  }

extern "C" {
DEFINE_ALLOC_TOKEN_STDLIB(0)
DEFINE_ALLOC_TOKEN_STDLIB(1)
}

// The mangled name of operator new differs between operator new(unsigned long)
// and operator new(unsigned int). Therefore, we need to define the symbols
// differently depending on the size of size_t.
#if __SIZEOF_SIZE_T__ != __SIZEOF_INT__

#define DEFINE_ALLOC_TOKEN_NEW(id)                                          \
  SHIM_ALWAYS_EXPORT void* __alloc_token_##id##__Znwm(size_t size) {        \
    return ShimCppNew(size, AllocToken(id));                                \
  }                                                                         \
  SHIM_ALWAYS_EXPORT void* __alloc_token_##id##__Znam(size_t size) {        \
    return ShimCppNew(size, AllocToken(id));                                \
  }                                                                         \
  SHIM_ALWAYS_EXPORT void* __alloc_token_##id##__ZnwmRKSt9nothrow_t(        \
      size_t size, const std::nothrow_t&) noexcept {                        \
    return ShimCppNewNoThrow(size, AllocToken(id));                         \
  }                                                                         \
  SHIM_ALWAYS_EXPORT void* __alloc_token_##id##__ZnamRKSt9nothrow_t(        \
      size_t size, const std::nothrow_t&) {                                 \
    return ShimCppNewNoThrow(size, AllocToken(id));                         \
  }                                                                         \
  SHIM_ALWAYS_EXPORT void* __alloc_token_##id##__ZnwmSt11align_val_t(       \
      size_t size, std::align_val_t alignment) {                            \
    return ShimCppAlignedNew(size, static_cast<size_t>(alignment),          \
                             AllocToken(id));                               \
  }                                                                         \
  SHIM_ALWAYS_EXPORT void* __alloc_token_##id##__ZnamSt11align_val_t(       \
      size_t size, std::align_val_t alignment) {                            \
    return ShimCppAlignedNew(size, static_cast<size_t>(alignment),          \
                             AllocToken(id));                               \
  }                                                                         \
  SHIM_ALWAYS_EXPORT void*                                                  \
      __alloc_token_##id##__ZnwmSt11align_val_tRKSt9nothrow_t(              \
          size_t size, std::align_val_t alignment,                          \
          const std::nothrow_t& t) noexcept {                               \
    return ShimCppAlignedNew(size, static_cast<size_t>(alignment),          \
                             AllocToken(id));                               \
  }                                                                         \
  SHIM_ALWAYS_EXPORT void*                                                  \
      __alloc_token_##id##__ZnamSt11align_val_tRKSt9nothrow_t(              \
          size_t size, std::align_val_t alignment, const std::nothrow_t&) { \
    return ShimCppAlignedNew(size, static_cast<size_t>(alignment),          \
                             AllocToken(id));                               \
  }

#else

#define DEFINE_ALLOC_TOKEN_NEW(id)                                          \
  SHIM_ALWAYS_EXPORT void* __alloc_token_##id##__Znwj(size_t size) {        \
    return ShimCppNew(size, AllocToken(id));                                \
  }                                                                         \
  SHIM_ALWAYS_EXPORT void* __alloc_token_##id##__Znaj(size_t size) {        \
    return ShimCppNew(size, AllocToken(id));                                \
  }                                                                         \
  SHIM_ALWAYS_EXPORT void* __alloc_token_##id##__ZnwjRKSt9nothrow_t(        \
      size_t size, const std::nothrow_t&) noexcept {                        \
    return ShimCppNewNoThrow(size, AllocToken(id));                         \
  }                                                                         \
  SHIM_ALWAYS_EXPORT void* __alloc_token_##id##__ZnajRKSt9nothrow_t(        \
      size_t size, const std::nothrow_t&) {                                 \
    return ShimCppNewNoThrow(size, AllocToken(id));                         \
  }                                                                         \
  SHIM_ALWAYS_EXPORT void* __alloc_token_##id##__ZnwjSt11align_val_t(       \
      size_t size, std::align_val_t alignment) {                            \
    return ShimCppAlignedNew(size, static_cast<size_t>(alignment),          \
                             AllocToken(id));                               \
  }                                                                         \
  SHIM_ALWAYS_EXPORT void* __alloc_token_##id##__ZnajSt11align_val_t(       \
      size_t size, std::align_val_t alignment) {                            \
    return ShimCppAlignedNew(size, static_cast<size_t>(alignment),          \
                             AllocToken(id));                               \
  }                                                                         \
  SHIM_ALWAYS_EXPORT void*                                                  \
      __alloc_token_##id##__ZnwjSt11align_val_tRKSt9nothrow_t(              \
          size_t size, std::align_val_t alignment,                          \
          const std::nothrow_t& t) noexcept {                               \
    return ShimCppAlignedNew(size, static_cast<size_t>(alignment),          \
                             AllocToken(id));                               \
  }                                                                         \
  SHIM_ALWAYS_EXPORT void*                                                  \
      __alloc_token_##id##__ZnajSt11align_val_tRKSt9nothrow_t(              \
          size_t size, std::align_val_t alignment, const std::nothrow_t&) { \
    return ShimCppAlignedNew(size, static_cast<size_t>(alignment),          \
                             AllocToken(id));                               \
  }

#endif  // !PA_BUILDFLAG(IS_ANDROID)

extern "C" {
DEFINE_ALLOC_TOKEN_NEW(0)
DEFINE_ALLOC_TOKEN_NEW(1)
}

#undef DEFINE_ALLOC_TOKEN_STDLIB
#undef DEFINE_ALLOC_TOKEN_NEW

#endif  // PA_BUILDFLAG(USE_ALLOCATOR_SHIM)

#endif  // PARTITION_ALLOC_SHIM_ALLOCATOR_SHIM_ALLOC_TOKEN_SYMBOLS_H_
