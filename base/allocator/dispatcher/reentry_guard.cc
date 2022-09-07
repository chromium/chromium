// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/dispatcher/reentry_guard.h"

#include "base/check.h"
#include "base/compiler_specific.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_ANDROID)
#include <pthread.h>
#endif

namespace base::allocator::dispatcher {

#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_ANDROID)
pthread_key_t ReentryGuard::entered_key_ = 0;

void ReentryGuard::InitTLSSlot() {
  if (entered_key_ == 0) {
    int error = pthread_key_create(&entered_key_, nullptr);
    CHECK(!error);
  }

  DCHECK(entered_key_ != 0);
}

#else

void ReentryGuard::InitTLSSlot() {}

#endif
}  // namespace base::allocator::dispatcher
