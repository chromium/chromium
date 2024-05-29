// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/task_runner_provider.h"
#include "base/task/single_thread_task_runner.h"

namespace cc {

base::SingleThreadTaskRunner* TaskRunnerProvider::MainThreadTaskRunner() const {
  return main_task_runner_.get();
}

bool TaskRunnerProvider::HasImplThread() const {
  return for_display_tree_ || !!impl_task_runner_.get();
}

base::SingleThreadTaskRunner* TaskRunnerProvider::ImplThreadTaskRunner() const {
  return for_display_tree_ ? main_task_runner_.get() : impl_task_runner_.get();
}

bool TaskRunnerProvider::IsMainThread() const {
  if (for_display_tree_) {
    return true;
  }
#if DCHECK_IS_ON()
  if (impl_thread_is_overridden_)
    return false;

  bool is_main_thread = base::PlatformThread::CurrentId() == main_thread_id_;
  if (is_main_thread && main_task_runner_.get()) {
    DCHECK(main_task_runner_->BelongsToCurrentThread());
  }
  return is_main_thread;
#else
  return true;
#endif
}

bool TaskRunnerProvider::IsImplThread() const {
  if (for_display_tree_) {
    return true;
  }
#if DCHECK_IS_ON()
  if (impl_thread_is_overridden_)
    return true;
  if (!impl_task_runner_.get())
    return false;
  return impl_task_runner_->BelongsToCurrentThread();
#else
  return true;
#endif
}

#if DCHECK_IS_ON()
void TaskRunnerProvider::SetCurrentThreadIsImplThread(bool is_impl_thread) {
  impl_thread_is_overridden_ = is_impl_thread;
}
#endif

bool TaskRunnerProvider::IsMainThreadBlocked() const {
  if (for_display_tree_) {
    // There's no need for thread blocking on the display tree. Since this
    // method is used in various places only to assert that the thread is
    // blocked (never to assert that it's *not* blocked), we lie to bypass such
    // assertions.
    return true;
  }
#if DCHECK_IS_ON()
  return is_main_thread_blocked_;
#else
  return true;
#endif
}

#if DCHECK_IS_ON()
void TaskRunnerProvider::SetMainThreadBlocked(bool is_main_thread_blocked) {
  is_main_thread_blocked_ = is_main_thread_blocked;
}
#endif

TaskRunnerProvider::TaskRunnerProvider(
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> impl_task_runner)
#if !DCHECK_IS_ON()
    : main_task_runner_(main_task_runner), impl_task_runner_(impl_task_runner) {
#else
    : main_task_runner_(main_task_runner),
      impl_task_runner_(impl_task_runner),
      main_thread_id_(base::PlatformThread::CurrentId()),
      impl_thread_is_overridden_(false),
      is_main_thread_blocked_(false) {
#endif
}

TaskRunnerProvider::~TaskRunnerProvider() {
  DCHECK(IsMainThread());
}

}  // namespace cc
