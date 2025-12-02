// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUILD_SANITIZERS_SANITIZER_SHARED_HOOKS_H_
#define BUILD_SANITIZERS_SANITIZER_SHARED_HOOKS_H_

#include "build/build_config.h"

#if defined(ADDRESS_SANITIZER)

#include <cstddef>

#if defined(WIN32)
#if defined(SANITIZERS_IMPLEMENTATION)
#define SANITIZERS_EXPORT __declspec(dllexport)
#else
#define SANITIZERS_EXPORT __declspec(dllimport)
#endif  // defined(SANITIZERS_IMPLEMENTATION)

#else  // defined(WIN32)
#define SANITIZERS_EXPORT __attribute__((visibility("default"), noinline))
#endif

typedef void (*SanitizerMallocHook)(const volatile void*, size_t);
typedef void (*SanitizerFreeHook)(const volatile void*);
typedef int (*SanitizerIgnoreFreeHook)(const volatile void*);

namespace build_sanitizers {

// Install malloc(), free() and ignore_free() hooks.
SANITIZERS_EXPORT void InstallSanitizerHooks(SanitizerMallocHook,
                                             SanitizerFreeHook,
                                             SanitizerIgnoreFreeHook);

// Uninstall the hooks
SANITIZERS_EXPORT void UninstallSanitizerHooks();

SANITIZERS_EXPORT void RunSanitizerMallocHook(const volatile void*, size_t);
SANITIZERS_EXPORT void RunSanitizerFreeHook(const volatile void*);
SANITIZERS_EXPORT int RunSanitizerIgnoreFreeHook(const volatile void*);

}  // namespace build_sanitizers

#endif  // ADDRESS_SANITIZER

#endif  // BUILD_SANITIZERS_SANITIZER_SHARED_HOOKS_H_
