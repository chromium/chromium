// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/tiles/tile_task_manager.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/threading/thread_restrictions.h"
#include "base/trace_event/trace_event.h"

namespace cc {

TileTaskManager::TileTaskManager() = default;

TileTaskManager::~TileTaskManager() = default;

// static
std::unique_ptr<TileTaskManagerImpl> TileTaskManagerImpl::Create(
    TaskGraphRunner* task_graph_runner,
    base::RepeatingCallback<void(scoped_refptr<TileTask>)>
        notify_external_dependent) {
  return base::WrapUnique<TileTaskManagerImpl>(new TileTaskManagerImpl(
      task_graph_runner, std::move(notify_external_dependent)));
}

TileTaskManagerImpl::TileTaskManagerImpl(
    TaskGraphRunner* task_graph_runner,
    base::RepeatingCallback<void(scoped_refptr<TileTask>)>
        notify_external_dependent)
    : task_graph_runner_(task_graph_runner),
      notify_external_dependent_(std::move(notify_external_dependent)),
      namespace_token_(task_graph_runner->GenerateNamespaceToken()) {}

TileTaskManagerImpl::~TileTaskManagerImpl() = default;

void TileTaskManagerImpl::ScheduleTasks(TaskGraph* graph) {
  TRACE_EVENT0("cc", "TileTaskManagerImpl::ScheduleTasks");
  task_graph_runner_->ScheduleTasks(namespace_token_, graph);
}

void TileTaskManagerImpl::ExternalDependencyCompletedForTask(
    scoped_refptr<TileTask> task) {
  task_graph_runner_->ExternalDependencyCompletedForTask(namespace_token_,
                                                         std::move(task));
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
    // TODO(szager): this should be PostTask-ed right when the dependency task
    // runs, rather than waiting for completed tasks to be collected. That will
    // likely require ImageController and TaskGraphRunner to share a base::Lock.
    if (auto& dependent = tile_task->external_dependent()) {
      dependent->ExternalDependencyCompleted();
      notify_external_dependent_.Run(std::move(dependent));
    }
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
