// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/dispatcher/reentry_guard.h"

#include "base/check.h"
#include "base/compiler_specific.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_ANDROID)

#include <pthread.h>

namespace base::allocator::dispatcher {

pthread_key_t ReentryGuard::entered_key_ = 0;

}  // namespace base::allocator::dispatcher
#endif
