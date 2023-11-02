// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/task_runner_test_template.h"

namespace base {

namespace test {

TaskTracker::TaskTracker() : task_runs_(0), task_runs_cv_(&lock_) {}

TaskTracker::~TaskTracker() = default;

RepeatingClosure TaskTracker::WrapTask(RepeatingClosure task, int i) {
  return BindRepeating(&TaskTracker::RunTask, this, std::move(task), i);
}

void TaskTracker::RunTask(RepeatingClosure task, int i) {
  AutoLock lock(lock_);
  if (!task.is_null()) {
    task.Run();
  }
  ++task_run_counts_[i];
  ++task_runs_;
  task_runs_cv_.Signal();
}

std::map<int, int> TaskTracker::GetTaskRunCounts() const {
  AutoLock lock(lock_);
  return task_run_counts_;
}

void TaskTracker::WaitForCompletedTasks(int count) {
  AutoLock lock(lock_);
  while (task_runs_ < count)
    task_runs_cv_.Wait();
}

}  // namespace test

// This suite is instantiated in binaries that use //base:test_support.
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(TaskRunnerTest);

}  // namespace base
