// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_mock_time_message_loop_task_runner.h"

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/task/current_thread.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/test_pending_task.h"
#include "base/time/time.h"

namespace base {

ScopedMockTimeMessageLoopTaskRunner::ScopedMockTimeMessageLoopTaskRunner()
    : task_runner_(new TestMockTimeTaskRunner),
      previous_task_runner_(SingleThreadTaskRunner::GetCurrentDefault()) {
  DCHECK(CurrentThread::Get());
  current_default_handle_.emplace(
      scoped_refptr<SingleThreadTaskRunner>(task_runner_),
      SingleThreadTaskRunner::CurrentDefaultHandle::MayAlreadyExist{});
  main_thread_default_task_runner_handle_.emplace(
      task_runner_,
      SingleThreadTaskRunner::MainThreadDefaultHandle::MayAlreadyExist{});
}

ScopedMockTimeMessageLoopTaskRunner::~ScopedMockTimeMessageLoopTaskRunner() {
  DCHECK(previous_task_runner_->RunsTasksInCurrentSequence());
  DCHECK_EQ(task_runner_, SingleThreadTaskRunner::GetCurrentDefault());
  for (auto& pending_task : task_runner_->TakePendingTasks()) {
    previous_task_runner_->PostDelayedTask(
        pending_task.location, std::move(pending_task.task),
        pending_task.GetTimeToRun() - task_runner_->NowTicks());
  }
}

}  // namespace base
