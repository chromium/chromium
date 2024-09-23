// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_system_provider/queue.h"

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/notreached.h"
#include "base/task/single_thread_task_runner.h"

namespace ash::file_system_provider {

Queue::Task::Task() : token(0) {
}

Queue::Task::Task(size_t token, AbortableCallback callback)
    : token(token), callback(std::move(callback)) {}

Queue::Task::Task(Task&& other) = default;
Queue::Task& Queue::Task::operator=(Task&& other) = default;

Queue::Task::~Task() = default;

Queue::Queue(size_t max_in_parallel)
    : max_in_parallel_(max_in_parallel), next_token_(1) {
  CHECK_LT(0u, max_in_parallel);
}

Queue::~Queue() = default;

size_t Queue::NewToken() {
  return next_token_++;
}

void Queue::Enqueue(size_t token, AbortableCallback callback) {
#if !NDEBUG
  CHECK(executed_.find(token) == executed_.end());
  for (auto& task : pending_) {
    CHECK(token != task.token);
  }
#endif
  pending_.push_back(Task(token, std::move(callback)));
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&Queue::MaybeRun, weak_ptr_factory_.GetWeakPtr()));
}

void Queue::Complete(size_t token) {
  const auto it = executed_.find(token);
  DCHECK(it != executed_.end());
  executed_.erase(it);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&Queue::MaybeRun, weak_ptr_factory_.GetWeakPtr()));
}

void Queue::MaybeRun() {
  if (executed_.size() == max_in_parallel_ || pending_.empty())
    return;

  CHECK_GT(max_in_parallel_, executed_.size());
  Task task = std::move(pending_.front());
  const size_t token = task.token;
  pending_.pop_front();

  auto callback = std::move(task.callback);
  executed_[token] = std::move(task);
  AbortCallback abort_callback = std::move(callback).Run();

  // It may happen that the task is completed and removed synchronously. Hence,
  // we need to check if the task is still in the executed collection.
  const auto executed_task_it = executed_.find(token);
  if (executed_task_it != executed_.end())
    executed_task_it->second.abort_callback = std::move(abort_callback);
}

void Queue::Abort(size_t token) {
  // Check if it's running. If so, then abort and expect a Complete() call soon.
  if (const auto it = executed_.find(token); it != executed_.end()) {
    Task& task = it->second;
    AbortCallback abort_callback = std::move(task.abort_callback);
    DCHECK(!abort_callback.is_null());
    std::move(abort_callback).Run();
    return;
  }

  // Aborting not running tasks is linear. TODO(mtomasz): Optimize if feasible.
  for (auto it = pending_.begin(); it != pending_.end(); ++it) {
    if (token == it->token) {
      pending_.erase(it);
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(&Queue::MaybeRun, weak_ptr_factory_.GetWeakPtr()));
      return;
    }
  }

  // The task is already removed, marked as completed or aborted.
  NOTREACHED_IN_MIGRATION();
}

}  // namespace ash::file_system_provider
