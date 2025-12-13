// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/sanitizers/sanitizer_shared_hooks.h"

#include <atomic>

#include "build/build_config.h"

namespace build_sanitizers {

namespace {

std::atomic<SanitizerMallocHook> g_malloc_hook = nullptr;
std::atomic<SanitizerFreeHook> g_free_hook = nullptr;
std::atomic<SanitizerIgnoreFreeHook> g_ignore_free_hook = nullptr;

}  // namespace

#define SANITIZER_HOOK_ATTRIBUTE                                           \
  __attribute__((no_sanitize("address", "memory", "thread", "undefined"))) \
  __attribute__((used)) __attribute__((noinline))

SANITIZER_HOOK_ATTRIBUTE SANITIZERS_EXPORT void InstallSanitizerHooks(
    SanitizerMallocHook malloc_hook,
    SanitizerFreeHook free_hook,
    SanitizerIgnoreFreeHook ignore_free_hook) {
  g_free_hook = free_hook;
  g_ignore_free_hook = ignore_free_hook;
  g_malloc_hook = malloc_hook;
}

SANITIZER_HOOK_ATTRIBUTE SANITIZERS_EXPORT void UninstallSanitizerHooks() {
  g_malloc_hook = nullptr;
  g_free_hook = nullptr;
  g_ignore_free_hook = nullptr;
}

SANITIZER_HOOK_ATTRIBUTE SANITIZERS_EXPORT void RunSanitizerMallocHook(
    const volatile void* ptr,
    size_t size) {
  if (SanitizerMallocHook hook =
          g_malloc_hook.load(std::memory_order_relaxed)) {
    hook(ptr, size);
  }
}

SANITIZER_HOOK_ATTRIBUTE SANITIZERS_EXPORT void RunSanitizerFreeHook(
    const volatile void* ptr) {
  if (SanitizerFreeHook hook = g_free_hook.load(std::memory_order_relaxed)) {
    hook(ptr);
  }
}

SANITIZER_HOOK_ATTRIBUTE SANITIZERS_EXPORT int RunSanitizerIgnoreFreeHook(
    const volatile void* ptr) {
  if (SanitizerIgnoreFreeHook hook =
          g_ignore_free_hook.load(std::memory_order_relaxed)) {
    return hook(ptr);
  }
  return 0;
}

}  // namespace build_sanitizers
