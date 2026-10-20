// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Weak fallbacks of the C allocation functions that -fsanitize=alloc-token
// rewrites calls to (e.g. malloc() to __alloc_token_malloc()), for binaries
// without allocator_shim. They drop the token and call the original function.
//
// Their visibility matches allocator_shim's definitions: default on Linux and
// Android, where a hidden fallback would make allocator_shim's definitions
// hidden too, and hidden on Apple platforms. `used` keeps them with (Thin)LTO,
// since the calls to them are created by the alloc-token pass.

// posix_memalign() is POSIX, not ISO C. Unlike C++, C doesn't get
// _GNU_SOURCE from the compiler, so define it before including any header.
#if !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif

#include <errno.h>
#include <stddef.h>
#include <stdlib.h>

#if defined(__linux__)
#include <malloc.h>
#endif

#if !defined(_WIN32)
#include <unistd.h>
#endif

#if defined(_WIN32)
#define ALLOC_TOKEN_VISIBILITY
#elif defined(__APPLE__)
#define ALLOC_TOKEN_VISIBILITY __attribute__((visibility("hidden")))
#else
#define ALLOC_TOKEN_VISIBILITY __attribute__((visibility("default")))
#endif

#define ALLOC_TOKEN_FALLBACK           \
  __attribute__((weak, used, noinline, \
                 no_sanitize("alloc-token"))) ALLOC_TOKEN_VISIBILITY

ALLOC_TOKEN_FALLBACK void* __alloc_token_malloc(size_t size, size_t token) {
  return malloc(size);
}

ALLOC_TOKEN_FALLBACK void* __alloc_token_calloc(size_t n,
                                                size_t size,
                                                size_t token) {
  return calloc(n, size);
}

ALLOC_TOKEN_FALLBACK void* __alloc_token_realloc(void* ptr,
                                                 size_t size,
                                                 size_t token) {
  return realloc(ptr, size);
}

// allocator_shim doesn't define the following two functions, so these are
// always used. They are built on __alloc_token_realloc() so that they use the
// same allocator as realloc() and are available on every platform.
ALLOC_TOKEN_FALLBACK void* __alloc_token_reallocarray(void* ptr,
                                                      size_t n,
                                                      size_t size,
                                                      size_t token) {
  size_t bytes;
  if (__builtin_mul_overflow(n, size, &bytes)) {
    errno = ENOMEM;
    return NULL;
  }
  return __alloc_token_realloc(ptr, bytes, token);
}

// reallocf() is a BSD extension that frees `ptr` on failure.
ALLOC_TOKEN_FALLBACK void* __alloc_token_reallocf(void* ptr,
                                                  size_t size,
                                                  size_t token) {
  void* result = __alloc_token_realloc(ptr, size, token);
  if (!result && size) {
    free(ptr);
  }
  return result;
}

// The Windows C runtime doesn't provide the following functions.
#if !defined(_WIN32)

ALLOC_TOKEN_FALLBACK void* __alloc_token_aligned_alloc(size_t alignment,
                                                       size_t size,
                                                       size_t token) {
#if defined(__ANDROID__) && __ANDROID_API__ < 28
  // aligned_alloc() is only available from API level 28.
  return memalign(alignment, size);
#else
  return aligned_alloc(alignment, size);
#endif
}

ALLOC_TOKEN_FALLBACK int __alloc_token_posix_memalign(void** result,
                                                      size_t alignment,
                                                      size_t size,
                                                      size_t token) {
  return posix_memalign(result, alignment, size);
}

// valloc() and pvalloc() are obsolete and not available in every C library.
ALLOC_TOKEN_FALLBACK void* __alloc_token_valloc(size_t size, size_t token) {
  void* result = NULL;
  const int error =
      posix_memalign(&result, (size_t)sysconf(_SC_PAGESIZE), size);
  if (error) {
    errno = error;
    return NULL;
  }
  return result;
}

ALLOC_TOKEN_FALLBACK void* __alloc_token_pvalloc(size_t size, size_t token) {
  const size_t page_size = (size_t)sysconf(_SC_PAGESIZE);
  // Round up to a multiple of the page size, and allocate at least one page.
  size_t rounded_size;
  if (__builtin_add_overflow(size, page_size - 1, &rounded_size)) {
    errno = ENOMEM;
    return NULL;
  }
  rounded_size &= ~(page_size - 1);
  if (!rounded_size) {
    rounded_size = page_size;
  }
  void* result = NULL;
  const int error = posix_memalign(&result, page_size, rounded_size);
  if (error) {
    errno = error;
    return NULL;
  }
  return result;
}

#endif  // !defined(_WIN32)

// memalign() is only available on Linux and Android.
#if defined(__linux__)
ALLOC_TOKEN_FALLBACK void* __alloc_token_memalign(size_t alignment,
                                                  size_t size,
                                                  size_t token) {
  return memalign(alignment, size);
}
#endif  // defined(__linux__)
