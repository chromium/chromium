// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_DISPATCHER_REENTRY_GUARD_H_
#define BASE_ALLOCATOR_DISPATCHER_REENTRY_GUARD_H_

#include "base/base_export.h"
#include "base/check.h"
#include "base/compiler_specific.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_ANDROID)
#include <pthread.h>
#endif

namespace base::allocator::dispatcher {

#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_ANDROID)

// The macOS implementation of libmalloc sometimes calls malloc recursively,
// delegating allocations between zones. That causes our hooks being called
// twice. The scoped guard allows us to detect that.
//
// Besides that the implementations of thread_local on macOS and Android
// seem to allocate memory lazily on the first access to thread_local variables.
// Make use of pthread TLS instead of C++ thread_local there.
struct BASE_EXPORT ReentryGuard {
  ReentryGuard() : allowed_(!pthread_getspecific(entered_key_)) {
    pthread_setspecific(entered_key_, reinterpret_cast<void*>(true));
  }

  ~ReentryGuard() {
    if (LIKELY(allowed_))
      pthread_setspecific(entered_key_, nullptr);
  }

  explicit operator bool() const noexcept { return allowed_; }

  // This function must be called in very early of the process start-up in
  // order to acquire a low TLS slot number because glibc TLS implementation
  // will require a malloc call to allocate storage for a higher slot number
  // (>= PTHREAD_KEY_2NDLEVEL_SIZE == 32).  c.f. heap_profiling::InitTLSSlot.
  static void InitTLSSlot();

 private:
  static pthread_key_t entered_key_;
  const bool allowed_;
};

#else

// Use [[maybe_unused]] as this lightweight stand-in for the more heavyweight
// ReentryGuard above will otherwise trigger the "unused code" warnings.
struct [[maybe_unused]] BASE_EXPORT ReentryGuard {
  constexpr explicit operator bool() const noexcept { return true; }

  static void InitTLSSlot();
};

#endif

}  // namespace base::allocator::dispatcher

#endif  // BASE_ALLOCATOR_DISPATCHER_REENTRY_GUARD_H_
