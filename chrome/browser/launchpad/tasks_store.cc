// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/launchpad/tasks_store.h"

#include <utility>
#include <vector>

#include "base/containers/adapters.h"
#include "base/functional/callback.h"
#include "chrome/browser/launchpad/features.h"

namespace launchpad {

TasksStore::TasksStore() : entries_(features::kMaxTasksCacheSize.Get()) {}

TasksStore::~TasksStore() = default;

void TasksStore::AddAllTasks(base::span<const LaunchpadTask> tasks,
                             OperationCallback callback) {
  for (const LaunchpadTask& task : base::Reversed(tasks)) {
    if (task.id.empty()) {
      continue;
    }
    entries_.Put(task.id, task);
  }
  std::move(callback).Run(true);
}

void TasksStore::Clear(base::OnceClosure callback) {
  entries_.Clear();
  std::move(callback).Run();
}

void TasksStore::GetAllTasks(GetAllTasksCallback callback) const {
  std::vector<LaunchpadTask> result;
  result.reserve(entries_.size());
  for (const auto& [id, task] : entries_) {
    result.push_back(task);
  }
  std::move(callback).Run(std::move(result));
}

}  // namespace launchpad
