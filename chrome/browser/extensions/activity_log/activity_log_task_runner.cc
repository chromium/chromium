// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/activity_log/activity_log_task_runner.h"

#include "base/task/lazy_thread_pool_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/single_thread_task_runner_thread_mode.h"

namespace extensions {

namespace {

base::SingleThreadTaskRunner* g_task_runner_for_testing = nullptr;

base::LazyThreadPoolSingleThreadTaskRunner g_task_runner =
    LAZY_THREAD_POOL_SINGLE_THREAD_TASK_RUNNER_INITIALIZER(
        base::TaskTraits(base::MayBlock(), base::TaskPriority::BEST_EFFORT),
        base::SingleThreadTaskRunnerThreadMode::SHARED);

}  // namespace

const scoped_refptr<base::SingleThreadTaskRunner> GetActivityLogTaskRunner() {
  if (g_task_runner_for_testing)
    return g_task_runner_for_testing;

  return g_task_runner.Get();
}

void SetActivityLogTaskRunnerForTesting(
    base::SingleThreadTaskRunner* task_runner) {
  g_task_runner_for_testing = task_runner;
}

}  // namespace extensions
