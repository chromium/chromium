// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "base/allocator/dispatcher/testing/dispatcher_test.h"

#include "base/allocator/dispatcher/reentry_guard.h"

namespace base::allocator::dispatcher::testing {

DispatcherTest::DispatcherTest() {
  base::allocator::dispatcher::ReentryGuard::InitTLSSlot();
}

DispatcherTest::~DispatcherTest() = default;

}  // namespace base::allocator::dispatcher::testing
