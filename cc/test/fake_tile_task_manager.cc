// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_tile_task_manager.h"


namespace cc {

FakeTileTaskManagerImpl::FakeTileTaskManagerImpl() = default;

FakeTileTaskManagerImpl::~FakeTileTaskManagerImpl() {
  DCHECK_EQ(0u, completed_tasks_.size());
}

void FakeTileTaskManagerImpl::ScheduleTasks(TaskGraph* graph) {
  for (const auto& node : graph->nodes) {
    TileTask* task = static_cast<TileTask*>(node.task.get());
    // Cancel the task and append to |completed_tasks_|.
    task->state().DidCancel();
    completed_tasks_.push_back(node.task);
  }
}

void FakeTileTaskManagerImpl::CheckForCompletedTasks() {
  for (auto& task : completed_tasks_) {
    DCHECK(task->state().IsFinished() || task->state().IsCanceled());
    TileTask* tile_task = static_cast<TileTask*>(task.get());
    tile_task->OnTaskCompleted();
    tile_task->DidComplete();
  }

  completed_tasks_.clear();
}

void FakeTileTaskManagerImpl::Shutdown() {}

}  // namespace cc
