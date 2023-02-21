// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/base/unique_notifier.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/task/sequenced_task_runner.h"

namespace cc {

UniqueNotifier::UniqueNotifier(base::SequencedTaskRunner* task_runner,
                               base::RepeatingClosure closure)
    : task_runner_(task_runner),
      closure_(std::move(closure)),
      notification_pending_(false) {}

UniqueNotifier::~UniqueNotifier() = default;

void UniqueNotifier::Cancel() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  base::AutoLock hold(lock_);
  notification_pending_ = false;
}

void UniqueNotifier::Schedule() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  base::AutoLock hold(lock_);
  if (notification_pending_)
    return;

  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&UniqueNotifier::Notify, weak_ptr_factory_.GetWeakPtr()));
  notification_pending_ = true;
}

void UniqueNotifier::Notify() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  // Scope to release |lock_| before running the closure.
  {
    base::AutoLock hold(lock_);
    if (!notification_pending_)
      return;

    // Note that the order here is important in case closure schedules another
    // run.
    notification_pending_ = false;
  }

  closure_.Run();
}

}  // namespace cc
