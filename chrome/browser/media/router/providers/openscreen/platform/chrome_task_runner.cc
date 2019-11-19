// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <chrono>  // NOLINT
#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/time/time.h"

#include "chrome/browser/media/router/providers/openscreen/platform/chrome_task_runner.h"

namespace media_router {

using openscreen::platform::Clock;
using openscreen::platform::TaskRunner;

namespace {
void ExecuteTask(TaskRunner::Task task) {
  task();
}
}  // namespace

ChromeTaskRunner::ChromeTaskRunner(
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  task_runner_ = task_runner;
}

ChromeTaskRunner::~ChromeTaskRunner() = default;

void ChromeTaskRunner::PostPackagedTask(TaskRunner::Task task) {
  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(ExecuteTask, std::move(task)));
}

void ChromeTaskRunner::PostPackagedTaskWithDelay(TaskRunner::Task task,
                                                 Clock::duration delay) {
  auto time_delta = base::TimeDelta::FromMilliseconds(
      std::chrono::duration_cast<std::chrono::milliseconds>(delay).count());
  task_runner_->PostDelayedTask(
      FROM_HERE, base::BindOnce(ExecuteTask, std::move(task)), time_delta);
}

}  // namespace media_router
