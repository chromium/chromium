// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/providers/fallback_task_provider.h"

#include "base/bind.h"
#include "base/process/process.h"
#include "base/stl_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/task_manager/providers/render_process_host_task_provider.h"
#include "chrome/browser/task_manager/providers/web_contents/web_contents_task_provider.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace task_manager {

namespace {

constexpr base::TimeDelta kTimeDelayForPendingTask =
    base::TimeDelta::FromMilliseconds(750);

// Returns a task that is in the vector if the task in the vector shares a Pid
// with the other task.
Task* GetTaskByPidFromVector(base::ProcessId process_id,
                             std::vector<Task*>* which_vector) {
  for (Task* candidate : *which_vector) {
    if (candidate->process_id() == process_id)
      return candidate;
  }
  return nullptr;
}

}  // namespace

FallbackTaskProvider::FallbackTaskProvider(
    std::unique_ptr<TaskProvider> primary_subprovider,
    std::unique_ptr<TaskProvider> secondary_subprovider)
    : sources_{
          std::make_unique<SubproviderSource>(this,
                                              std::move(primary_subprovider)),
          std::make_unique<SubproviderSource>(
              this,
              std::move(secondary_subprovider))} {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

FallbackTaskProvider::~FallbackTaskProvider() {}

Task* FallbackTaskProvider::GetTaskOfUrlRequest(int child_id, int route_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  Task* task_of_url_request;
  for (const auto& source : sources_) {
    task_of_url_request =
        source->subprovider()->GetTaskOfUrlRequest(child_id, route_id);
    if (base::Contains(shown_tasks_, task_of_url_request))
      return task_of_url_request;
  }
  return nullptr;
}

void FallbackTaskProvider::StartUpdating() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(shown_tasks_.empty());
  for (auto& source : sources_) {
    DCHECK(source->tasks()->empty());
    source->subprovider()->SetObserver(source.get());
  }
}

void FallbackTaskProvider::StopUpdating() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  for (auto& source : sources_) {
    source->subprovider()->ClearObserver();
    source->tasks()->clear();
  }

  shown_tasks_.clear();
  pending_shown_tasks_.clear();
}

void FallbackTaskProvider::ShowTaskLater(Task* task) {
  auto it = pending_shown_tasks_.lower_bound(task);
  if (it == pending_shown_tasks_.end() || it->first != task) {
    it = pending_shown_tasks_.emplace_hint(it, std::piecewise_construct,
                                           std::forward_as_tuple(task),
                                           std::forward_as_tuple(this));
  } else {
    NOTREACHED();
    it->second.InvalidateWeakPtrs();
  }

  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&FallbackTaskProvider::ShowPendingTask,
                     it->second.GetWeakPtr(), task),
      kTimeDelayForPendingTask);
}

void FallbackTaskProvider::ShowPendingTask(Task* task) {
  pending_shown_tasks_.erase(task);
  ShowTask(task);
}

void FallbackTaskProvider::ShowTask(Task* task) {
  shown_tasks_.push_back(task);
  NotifyObserverTaskAdded(task);
}

void FallbackTaskProvider::HideTask(Task* task) {
  auto it = std::remove(shown_tasks_.begin(), shown_tasks_.end(), task);
  pending_shown_tasks_.erase(task);
  if (it != shown_tasks_.end()) {
    shown_tasks_.erase(it, shown_tasks_.end());
    NotifyObserverTaskRemoved(task);
  }
}

void FallbackTaskProvider::OnTaskAddedBySource(Task* task,
                                               SubproviderSource* source) {
  DCHECK(source == primary_source() || source == secondary_source());

  // If a secondary task is added but a primary task is already shown for it, we
  // can ignore showing the secondary.
  if (source == secondary_source()) {
    if (GetTaskByPidFromVector(task->process_id(), primary_source()->tasks()))
      return;
  }

  // If we get a primary task that has a secondary task that is both known and
  // shown we then hide the secondary task and then show the primary task.
  if (source == primary_source()) {
    ShowTask(task);
    for (Task* secondary_task : *secondary_source()->tasks()) {
      if (task->process_id() == secondary_task->process_id())
        HideTask(secondary_task);
    }
  } else {
    ShowTaskLater(task);
  }
}

void FallbackTaskProvider::OnTaskRemovedBySource(Task* task,
                                                 SubproviderSource* source) {
  DCHECK(source == primary_source() || source == secondary_source());
  // When a task from the primary subprovider is removed, see if there
  // are any other primary tasks for that process. If not, but there are
  // secondary tasks, show them.
  if (source == primary_source()) {
    Task* primary_task =
        GetTaskByPidFromVector(task->process_id(), primary_source()->tasks());
    if (!primary_task) {
      for (Task* secondary_task : *secondary_source()->tasks()) {
        if (task->process_id() == secondary_task->process_id()) {
          ShowTaskLater(secondary_task);
        }
      }
    }
  }
  HideTask(task);
}

void FallbackTaskProvider::OnTaskUnresponsive(Task* task) {
  DCHECK(task);
  if (base::Contains(shown_tasks_, task))
    NotifyObserverTaskUnresponsive(task);
}

FallbackTaskProvider::SubproviderSource::SubproviderSource(
    FallbackTaskProvider* fallback_task_provider,
    std::unique_ptr<TaskProvider> subprovider)
    : fallback_task_provider_(fallback_task_provider),
      subprovider_(std::move(subprovider)) {}

FallbackTaskProvider::SubproviderSource::~SubproviderSource() {}

void FallbackTaskProvider::SubproviderSource::TaskAdded(Task* task) {
  DCHECK(task);
  tasks_.push_back(task);
  fallback_task_provider_->OnTaskAddedBySource(task, this);
}

void FallbackTaskProvider::SubproviderSource::TaskRemoved(Task* task) {
  DCHECK(task);

  base::Erase(tasks_, task);
  fallback_task_provider_->OnTaskRemovedBySource(task, this);
}

void FallbackTaskProvider::SubproviderSource::TaskUnresponsive(Task* task) {
  fallback_task_provider_->OnTaskUnresponsive(task);
}

}  // namespace task_manager
