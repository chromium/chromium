// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/thread_task_runner_handle.h"

namespace base {

// DEPRECATED: Use SequencedTaskRunner::GetCurrentDefault instead
// static
const scoped_refptr<SingleThreadTaskRunner>& ThreadTaskRunnerHandle::Get() {
  return SingleThreadTaskRunner::GetCurrentDefault();
}

// DEPRECATED: Use SequencedTaskRunner::HasCurrentDefault instead
// static
bool ThreadTaskRunnerHandle::IsSet() {
  return SingleThreadTaskRunner::HasCurrentDefault();
}

}  // namespace base
