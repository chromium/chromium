// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/one_shot_event.h"

#include <stddef.h>

#include <algorithm>
#include <utility>

#include "base/functional/callback.h"
#include "base/location.h"
#include "base/sequence_checker.h"
#include "base/task/task_runner.h"
#include "base/time/time.h"

namespace base {

struct OneShotEvent::TaskInfo {
  TaskInfo() = default;
  TaskInfo(const Location& from_here,
           scoped_refptr<TaskRunner> runner,
           OnceClosure task,
           const TimeDelta& delay)
      : from_here(from_here),
        runner(std::move(runner)),
        task(std::move(task)),
        delay(delay) {
    CHECK(this->runner);  // Detect mistakes with a decent stack frame.
  }
  TaskInfo(TaskInfo&&) = default;
  TaskInfo& operator=(TaskInfo&&) = default;

  Location from_here;
  scoped_refptr<TaskRunner> runner;
  OnceClosure task;
  TimeDelta delay;
};

OneShotEvent::OneShotEvent() {
  // It's acceptable to construct the `OneShotEvent` on one sequence, but
  // immediately move it to another sequence.
  DETACH_FROM_SEQUENCE(sequence_checker_);
}
OneShotEvent::OneShotEvent(bool signaled) : signaled_(signaled) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}
OneShotEvent::~OneShotEvent() = default;

void OneShotEvent::Post(const Location& from_here,
                        OnceClosure task,
                        scoped_refptr<TaskRunner> runner) const {
  PostDelayed(from_here, std::move(task), TimeDelta(), std::move(runner));
}

void OneShotEvent::PostDelayed(const Location& from_here,
                               OnceClosure task,
                               const TimeDelta& delay,
                               scoped_refptr<TaskRunner> runner) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (is_signaled()) {
    runner->PostDelayedTask(from_here, std::move(task), delay);
  } else {
    tasks_.emplace_back(from_here, std::move(runner), std::move(task), delay);
  }
}

void OneShotEvent::Signal() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CHECK(!signaled_) << "Only call Signal once.";

  signaled_ = true;
  // After this point, a call to `Post` from one of the queued tasks could
  // proceed immediately, but the fact that this object is sequence-bound
  // prevents that from being relevant.

  // Move tasks to a temporary to ensure no new ones are added.
  std::vector<TaskInfo> moved_tasks = std::exchange(tasks_, {});

  // We could randomize tasks in debug mode in order to check that
  // the order doesn't matter...
  for (TaskInfo& task : moved_tasks) {
    task.runner->PostDelayedTask(task.from_here, std::move(task.task),
                                 task.delay);
  }
  DCHECK(tasks_.empty()) << "No new tasks should be added during task running!";
}

}  // namespace base
