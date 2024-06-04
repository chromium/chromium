// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/sequence_manager/task_time_observer.h"

#include "base/debug/stack_trace.h"
#include "base/notreached.h"

namespace base::sequence_manager {

TaskTimeObserver::TaskTimeObserver()
    : alloc_stack_(base::debug::StackTrace()) {}

TaskTimeObserver::~TaskTimeObserver() {
  if (IsInObserverList()) {
    NOTREACHED() << alloc_stack_;
  }
}

}  // namespace base::sequence_manager
