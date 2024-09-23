// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/tracing/perfetto_task_runner.h"

#include <memory>
#include <utility>

#include "base/auto_reset.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/task/common/checked_lock_impl.h"
#include "base/task/common/scoped_defer_task_posting.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/tracing/tracing_tls.h"
#include "build/build_config.h"

#include "base/debug/stack_trace.h"

namespace base {
namespace tracing {

PerfettoTaskRunner::PerfettoTaskRunner(
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : task_runner_(std::move(task_runner)) {}

PerfettoTaskRunner::~PerfettoTaskRunner() {
  DCHECK(GetOrCreateTaskRunner()->RunsTasksInCurrentSequence());
#if (BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_NACL)) || BUILDFLAG(IS_FUCHSIA)
  fd_controllers_.clear();
#endif  // (BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_NACL)) || BUILDFLAG(IS_FUCHSIA)
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
            const AutoReset<bool> resetter(GetThreadIsInTraceEvent(), true,
                                           false);
            task();
          },
          task),
      base::Milliseconds(delay_ms));
}

bool PerfettoTaskRunner::RunsTasksOnCurrentThread() const {
  DCHECK(task_runner_);
  return task_runner_->RunsTasksInCurrentSequence();
}

// PlatformHandle is an int on POSIX, a HANDLE on Windows.
void PerfettoTaskRunner::AddFileDescriptorWatch(
    perfetto::base::PlatformHandle fd,
    std::function<void()> callback) {
#if (BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_NACL)) || BUILDFLAG(IS_FUCHSIA)
  DCHECK(GetOrCreateTaskRunner()->RunsTasksInCurrentSequence());
  DCHECK(!base::Contains(fd_controllers_, fd));
  // Set up the |fd| in the map to signal intent to add a watch. We need to
  // PostTask the WatchReadable creation because if we do it in this task we'll
  // race with perfetto setting up the connection on this task and the IO thread
  // setting up epoll on the |fd|. Using a CancelableOnceClosure ensures that
  // the |fd| won't be added for watch if RemoveFileDescriptorWatch is called.
  fd_controllers_[fd].callback.Reset(
      base::BindOnce(
          [](PerfettoTaskRunner* perfetto_runner, int fd,
             std::function<void()> callback) {
            DCHECK(perfetto_runner->GetOrCreateTaskRunner()
                       ->RunsTasksInCurrentSequence());
            // When this callback runs, we must not have removed |fd|'s watch.
            CHECK(base::Contains(perfetto_runner->fd_controllers_, fd));
            auto& controller_and_cb = perfetto_runner->fd_controllers_[fd];
            // We should never overwrite an existing watch.
            CHECK(!controller_and_cb.controller);
            controller_and_cb.controller =
                base::FileDescriptorWatcher::WatchReadable(
                    fd, base::BindRepeating(
                            [](std::function<void()> callback) { callback(); },
                            std::move(callback)));
          },
          base::Unretained(this), fd, std::move(callback)));
  task_runner_->PostTask(FROM_HERE, fd_controllers_[fd].callback.callback());
#else   // (BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_NACL)) || BUILDFLAG(IS_FUCHSIA)
  NOTREACHED();
#endif  // (BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_NACL)) || BUILDFLAG(IS_FUCHSIA)
}

void PerfettoTaskRunner::RemoveFileDescriptorWatch(
    perfetto::base::PlatformHandle fd) {
#if (BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_NACL)) || BUILDFLAG(IS_FUCHSIA)
  DCHECK(GetOrCreateTaskRunner()->RunsTasksInCurrentSequence());
  DCHECK(base::Contains(fd_controllers_, fd));
  // This also cancels the base::FileDescriptorWatcher::WatchReadable() task if
  // it's pending.
  fd_controllers_.erase(fd);
#else   // (BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_NACL)) || BUILDFLAG(IS_FUCHSIA)
  NOTREACHED();
#endif  // (BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_NACL)) || BUILDFLAG(IS_FUCHSIA)
}

void PerfettoTaskRunner::ResetTaskRunnerForTesting(
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
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

#if (BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_NACL)) || BUILDFLAG(IS_FUCHSIA)
PerfettoTaskRunner::FDControllerAndCallback::FDControllerAndCallback() =
    default;

PerfettoTaskRunner::FDControllerAndCallback::~FDControllerAndCallback() =
    default;
#endif  // (BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_NACL)) || BUILDFLAG(IS_FUCHSIA)

}  // namespace tracing
}  // namespace base
