// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/tracing/perfetto_task_runner.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/containers/contains.h"
#include "base/no_destructor.h"
#include "base/task/common/checked_lock_impl.h"
#include "base/task/common/scoped_defer_task_posting.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread_local.h"
#include "base/threading/thread_local_storage.h"
#include "base/tracing/tracing_tls.h"

namespace base {
namespace tracing {

PerfettoTaskRunner::PerfettoTaskRunner(
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : task_runner_(std::move(task_runner)) {}

PerfettoTaskRunner::~PerfettoTaskRunner() {
  DCHECK(GetOrCreateTaskRunner()->RunsTasksInCurrentSequence());
#if defined(OS_POSIX) && !defined(OS_NACL)
  fd_controllers_.clear();
#endif  // defined(OS_POSIX) && !defined(OS_NACL)
}

void PerfettoTaskRunner::PostTask(std::function<void()> task) {
  PostDelayedTask(task, /* delay_ms */ 0);
}

void PerfettoTaskRunner::PostDelayedTask(std::function<void()> task,
                                         uint32_t delay_ms) {
  base::ScopedDeferTaskPosting::PostOrDefer(
      GetOrCreateTaskRunner(), FROM_HERE,
      base::BindOnce(
          [](std::function<void()> task) {
            // We block any trace events that happens while any
            // Perfetto task is running, or we'll get deadlocks in
            // situations where the StartupTraceWriterRegistry tries
            // to bind a writer which in turn causes a PostTask where
            // a trace event can be emitted, which then deadlocks as
            // it needs a new chunk from the same StartupTraceWriter
            // which we're trying to bind and are keeping the lock
            // to.
            // TODO(oysteine): Try to see if we can be more selective
            // about this.
            AutoThreadLocalBoolean thread_is_in_trace_event(
                GetThreadIsInTraceEventTLS());
            task();
          },
          task),
      base::TimeDelta::FromMilliseconds(delay_ms));
}

bool PerfettoTaskRunner::RunsTasksOnCurrentThread() const {
  DCHECK(task_runner_);
  return task_runner_->RunsTasksInCurrentSequence();
}

// PlatformHandle is an int on POSIX, a HANDLE on Windows.
void PerfettoTaskRunner::AddFileDescriptorWatch(
    perfetto::base::PlatformHandle fd,
    std::function<void()> callback) {
#if defined(OS_POSIX) && !defined(OS_NACL)
  DCHECK(GetOrCreateTaskRunner()->RunsTasksInCurrentSequence());
  DCHECK(!base::Contains(fd_controllers_, fd));
  // Set it up as a nullptr to signal intent to add a watch. We need to PostTask
  // the WatchReadable creation because if we do it in this task we'll race with
  // perfetto setting up the connection on this task and the IO thread setting
  // up epoll on the |fd|. By posting the task we ensure the Connection has
  // either succeeded (we find the |fd| in the map) or the connection failed
  // (the |fd| is not in the map), and we can gracefully handle either case.
  fd_controllers_[fd];
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](PerfettoTaskRunner* perfetto_runner, int fd,
             std::function<void()> callback) {
            DCHECK(perfetto_runner->GetOrCreateTaskRunner()
                       ->RunsTasksInCurrentSequence());
            auto it = perfetto_runner->fd_controllers_.find(fd);
            // If we can't find this fd, then RemoveFileDescriptor has already
            // been called so just early out.
            if (it == perfetto_runner->fd_controllers_.end()) {
              return;
            }
            DCHECK(!it->second);
            it->second = base::FileDescriptorWatcher::WatchReadable(
                fd, base::BindRepeating(
                        [](std::function<void()> callback) { callback(); },
                        std::move(callback)));
          },
          base::Unretained(this), fd, std::move(callback)));
#else   // defined(OS_POSIX) && !defined(OS_NACL)
  NOTREACHED();
#endif  // defined(OS_POSIX) && !defined(OS_NACL)
}

void PerfettoTaskRunner::RemoveFileDescriptorWatch(
    perfetto::base::PlatformHandle fd) {
#if defined(OS_POSIX) && !defined(OS_NACL)
  DCHECK(GetOrCreateTaskRunner()->RunsTasksInCurrentSequence());
  DCHECK(base::Contains(fd_controllers_, fd));
  fd_controllers_.erase(fd);
#else   // defined(OS_POSIX) && !defined(OS_NACL)
  NOTREACHED();
#endif  // defined(OS_POSIX) && !defined(OS_NACL)
}

void PerfettoTaskRunner::ResetTaskRunnerForTesting(
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  task_runner_ = std::move(task_runner);
}

void PerfettoTaskRunner::SetTaskRunner(
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  DCHECK(!task_runner_);
  task_runner_ = std::move(task_runner);
}

scoped_refptr<base::SequencedTaskRunner>
PerfettoTaskRunner::GetOrCreateTaskRunner() {
  // TODO(eseckler): This is not really thread-safe. We should probably add a
  // lock around this. At the moment we can get away without one because this
  // method is called for the first time on the process's main thread before the
  // tracing service connects.
  if (!task_runner_) {
    DCHECK(base::ThreadPoolInstance::Get());
    task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
        {base::MayBlock(), base::TaskPriority::USER_BLOCKING});
  }

  return task_runner_;
}

}  // namespace tracing
}  // namespace base
