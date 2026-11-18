// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LAUNCHPAD_TASKS_STORE_H_
#define CHROME_BROWSER_LAUNCHPAD_TASKS_STORE_H_

#include <string>
#include <vector>

#include "base/containers/lru_cache.h"
#include "base/containers/span.h"
#include "base/functional/callback_forward.h"
#include "chrome/browser/launchpad/launchpad_task.h"

namespace launchpad {

// In-memory store for `LaunchpadTask` items backed by `base::LRUCache`.
class TasksStore {
 public:
  using OperationCallback = base::OnceCallback<void(bool success)>;
  using GetAllTasksCallback =
      base::OnceCallback<void(std::vector<LaunchpadTask>)>;

  TasksStore();
  TasksStore(const TasksStore&) = delete;
  TasksStore& operator=(const TasksStore&) = delete;
  ~TasksStore();

  // Adds or replaces multiple tasks in the store, preserving the order of
  // `tasks` from most-recently-used to least-recently-used. Tasks with an
  // empty `id` are ignored.
  void AddAllTasks(base::span<const LaunchpadTask> tasks,
                   OperationCallback callback);

  // Clears all tasks from the store.
  void Clear(base::OnceClosure callback);

  // Retrieves all tasks currently stored in the store from most-recently-used
  // to least-recently-used.
  void GetAllTasks(GetAllTasksCallback callback) const;

 private:
  base::LRUCache<std::string, LaunchpadTask> entries_;
};

}  // namespace launchpad

#endif  // CHROME_BROWSER_LAUNCHPAD_TASKS_STORE_H_
