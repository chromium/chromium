// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#if defined(ADDRESS_SANITIZER)

#include <cstddef>

#include "build/sanitizers/sanitizer_shared_hooks.h"

#define SANITIZER_HOOK_ATTRIBUTE                                               \
  extern "C"                                                                   \
      __attribute__((no_sanitize("address", "memory", "thread", "undefined"))) \
      __attribute__((visibility("default"))) __attribute__((used))             \
      __attribute__((noinline))

SANITIZER_HOOK_ATTRIBUTE
void __sanitizer_malloc_hook(const volatile void* ptr, size_t size) {
  build_sanitizers::RunSanitizerMallocHook(ptr, size);
}

SANITIZER_HOOK_ATTRIBUTE
void __sanitizer_free_hook(const volatile void* ptr) {
  build_sanitizers::RunSanitizerFreeHook(ptr);
}

SANITIZER_HOOK_ATTRIBUTE int __sanitizer_ignore_free_hook(
    const volatile void* ptr) {
  return build_sanitizers::RunSanitizerIgnoreFreeHook(ptr);
}

#endif  // defined(ADDRESS_SANITIZER)
