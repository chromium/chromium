// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/platform_thread.h"

#include "build/build_config.h"

namespace base {

// static
void PlatformThread::SetCurrentThreadPriority(ThreadPriority priority) {
  SetCurrentThreadPriorityImpl(priority);
}

}  // namespace base
