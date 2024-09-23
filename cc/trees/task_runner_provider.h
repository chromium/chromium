// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_TASK_RUNNER_PROVIDER_H_
#define CC_TREES_TASK_RUNNER_PROVIDER_H_

#include <memory>
#include <string>

#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/platform_thread.h"
#include "cc/cc_export.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace cc {

// Class responsible for controlling access to the main and impl task runners.
// Useful for assertion checks.
class CC_EXPORT TaskRunnerProvider {
 public:
  static std::unique_ptr<TaskRunnerProvider> Create(
      scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> impl_task_runner) {
    return base::WrapUnique(
        new TaskRunnerProvider(main_task_runner, impl_task_runner));
  }

  static std::unique_ptr<TaskRunnerProvider> CreateForDisplayTree(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
    auto provider = Create(task_runner, task_runner);
    provider->for_display_tree_ = true;
    return provider;
  }

  TaskRunnerProvider(const TaskRunnerProvider&) = delete;
  TaskRunnerProvider& operator=(const TaskRunnerProvider&) = delete;

  // TODO(vmpstr): Should these return scoped_refptr to task runners? Many
  // places turn them into scoped_refptrs. How many of them need to?
  base::SingleThreadTaskRunner* MainThreadTaskRunner() const;
  bool HasImplThread() const;
  base::SingleThreadTaskRunner* ImplThreadTaskRunner() const;

  // Debug hooks.
  bool IsMainThread() const;
  bool IsImplThread() const;
  bool IsMainThreadBlocked() const;
#if DCHECK_IS_ON()
  void SetMainThreadBlocked(bool is_main_thread_blocked);
  void SetCurrentThreadIsImplThread(bool is_impl_thread);
#endif

  virtual ~TaskRunnerProvider();

 protected:
  TaskRunnerProvider(
      scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> impl_task_runner);

  friend class DebugScopedSetImplThread;
  friend class DebugScopedSetMainThread;
  friend class DebugScopedSetMainThreadBlocked;

 private:
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> impl_task_runner_;

#if DCHECK_IS_ON()
  const base::PlatformThreadId main_thread_id_;
  bool impl_thread_is_overridden_;
  bool is_main_thread_blocked_;
#endif

  // For GPU-side display trees, TaskRunnerProvider treats the main thread as
  // both the "main" thread and the "impl" thread to placate various assertions
  // about which thread is doing what inside the layer tree. This is a
  // sufficient adaptation since the display tree consists only of a manually
  // driven LayerTreeHostImpl on a single thread with no Proxy or Scheduler.
  bool for_display_tree_ = false;
};

#if DCHECK_IS_ON()
class DebugScopedSetMainThreadBlocked {
 public:
  explicit DebugScopedSetMainThreadBlocked(
      TaskRunnerProvider* task_runner_provider)
      : task_runner_provider_(task_runner_provider) {
    DCHECK(!task_runner_provider_->IsMainThreadBlocked());
    task_runner_provider_->SetMainThreadBlocked(true);
  }
  DebugScopedSetMainThreadBlocked(const DebugScopedSetMainThreadBlocked&) =
      delete;
  ~DebugScopedSetMainThreadBlocked() {
    DCHECK(task_runner_provider_->IsMainThreadBlocked());
    task_runner_provider_->SetMainThreadBlocked(false);
  }

  DebugScopedSetMainThreadBlocked& operator=(
      const DebugScopedSetMainThreadBlocked&) = delete;

 private:
  raw_ptr<TaskRunnerProvider> task_runner_provider_;
};
#else
class DebugScopedSetMainThreadBlocked {
 public:
  explicit DebugScopedSetMainThreadBlocked(
      TaskRunnerProvider* task_runner_provider) {}
  DebugScopedSetMainThreadBlocked(const DebugScopedSetMainThreadBlocked&) =
      delete;
  ~DebugScopedSetMainThreadBlocked() {}

  DebugScopedSetMainThreadBlocked& operator=(
      const DebugScopedSetMainThreadBlocked&) = delete;
};
#endif

}  // namespace cc

#endif  // CC_TREES_TASK_RUNNER_PROVIDER_H_
