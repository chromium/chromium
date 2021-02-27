// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/thread_pool/pooled_task_runner_delegate.h"

#include "base/debug/task_trace.h"
#include "base/logging.h"

namespace base {
namespace internal {

namespace {

// Stores the current PooledTaskRunnerDelegate in this process(null if none).
// Used to tell when a task is posted from the main thread after the
// task environment was brought down in unit tests so that TaskRunners can
// return false on PostTask, letting callers know they should complete
// necessary work synchronously. A PooledTaskRunnerDelegate is usually
// instantiated before worker threads are started and deleted after worker
// threads have been joined. This makes the variable const while worker threads
// are up and as such it doesn't need to be atomic.
// Also used to tell if an attempt is made to run a task after its
// task runner's delegate is no longer the current delegate, i.e., a task runner
// is created in one unit test and posted to in a subsequent test, due to global
// state leaking between tests.
PooledTaskRunnerDelegate* g_current_delegate = nullptr;

}  // namespace

PooledTaskRunnerDelegate::PooledTaskRunnerDelegate() {
  DCHECK(!g_current_delegate);
  g_current_delegate = this;
}

PooledTaskRunnerDelegate::~PooledTaskRunnerDelegate() {
  DCHECK(g_current_delegate);
  g_current_delegate = nullptr;
}

// static
bool PooledTaskRunnerDelegate::MatchesCurrentDelegate(
    PooledTaskRunnerDelegate* delegate) {
  if (g_current_delegate && g_current_delegate != delegate) {
    LOG(ERROR)
        << "Stale pooled_task_runner_delegate_ - task not posted. This is\n"
           "almost certainly caused by a previous test leaving a stale task\n"
           "runner in a global object, and a subsequent test triggering the\n "
           "global object to post a task to the stale task runner.\n"
        << base::debug::StackTrace();
  }
  return g_current_delegate == delegate;
}

}  // namespace internal
}  // namespace base
