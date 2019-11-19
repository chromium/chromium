// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/tiles/tile_task_manager.h"

#include "base/memory/ptr_util.h"
#include "base/threading/thread_restrictions.h"
#include "base/trace_event/trace_event.h"

namespace cc {

TileTaskManager::TileTaskManager() = default;

TileTaskManager::~TileTaskManager() = default;

// static
std::unique_ptr<TileTaskManagerImpl> TileTaskManagerImpl::Create(
    TaskGraphRunner* task_graph_runner) {
  return base::WrapUnique<TileTaskManagerImpl>(
      new TileTaskManagerImpl(task_graph_runner));
}

TileTaskManagerImpl::TileTaskManagerImpl(TaskGraphRunner* task_graph_runner)
    : task_graph_runner_(task_graph_runner),
      namespace_token_(task_graph_runner->GenerateNamespaceToken()) {}

TileTaskManagerImpl::~TileTaskManagerImpl() = default;

void TileTaskManagerImpl::ScheduleTasks(TaskGraph* graph) {
  TRACE_EVENT0("cc", "TileTaskManagerImpl::ScheduleTasks");
  task_graph_runner_->ScheduleTasks(namespace_token_, graph);
}

void TileTaskManagerImpl::CheckForCompletedTasks() {
  TRACE_EVENT0("cc", "TileTaskManagerImpl::CheckForCompletedTasks");
  Task::Vector completed_tasks;
  task_graph_runner_->CollectCompletedTasks(namespace_token_, &completed_tasks);

  for (auto& task : completed_tasks) {
    DCHECK(task->state().IsFinished() || task->state().IsCanceled());
    TileTask* tile_task = static_cast<TileTask*>(task.get());
    tile_task->OnTaskCompleted();
    tile_task->DidComplete();
  }
}

void TileTaskManagerImpl::Shutdown() {
  TRACE_EVENT0("cc", "TileTaskManagerImpl::Shutdown");

  // Cancel non-scheduled tasks and wait for running tasks to finish.
  TaskGraph empty;
  task_graph_runner_->ScheduleTasks(namespace_token_, &empty);
  {
    base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_wait;
    task_graph_runner_->WaitForTasksToFinishRunning(namespace_token_);
  }
}

}  // namespace cc
