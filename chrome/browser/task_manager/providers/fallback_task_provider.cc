// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/providers/fallback_task_provider.h"

#include <vector>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/process/process.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/task_manager/providers/render_process_host_task_provider.h"
#include "chrome/browser/task_manager/providers/web_contents/web_contents_task_provider.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace task_manager {

namespace {

constexpr base::TimeDelta kTimeDelayForPendingTask = base::Milliseconds(750);

// Returns a task that is in the vector if the task in the vector shares a Pid
// with the other task.
Task* GetTaskByPidFromVector(
    base::ProcessId process_id,
    std::vector<raw_ptr<Task, VectorExperimental>>* which_vector) {
  for (Task* candidate : *which_vector) {
    if (candidate->process_id() == process_id)
      return candidate;
  }
  return nullptr;
}

}  // namespace

FallbackTaskProvider::FallbackTaskProvider(
    std::vector<std::unique_ptr<TaskProvider>> primary_subproviders,
    std::unique_ptr<TaskProvider> secondary_subprovider)
    : secondary_source_(std::make_unique<SubproviderSource>(
          this,
          std::move(secondary_subprovider))) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  for (auto& provider : primary_subproviders) {
    primary_sources_.push_back(
        std::make_unique<SubproviderSource>(this, std::move(provider)));
  }
}

FallbackTaskProvider::~FallbackTaskProvider() {}

Task* FallbackTaskProvider::GetTaskOfUrlRequest(int child_id, int route_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  for (const auto& source : primary_sources_) {
    Task* task = source->subprovider()->GetTaskOfUrlRequest(child_id, route_id);
    if (task)
      return task;
  }

  return secondary_source_->subprovider()->GetTaskOfUrlRequest(child_id,
                                                               route_id);
}

void FallbackTaskProvider::StartUpdating() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(shown_tasks_.empty());

  for (auto& source : primary_sources_) {
    DCHECK(source->tasks()->empty());
    source->subprovider()->SetObserver(source.get());
  }

  DCHECK(secondary_source_->tasks()->empty());
  secondary_source_->subprovider()->SetObserver(secondary_source_.get());
}

void FallbackTaskProvider::StopUpdating() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  for (auto& source : primary_sources_) {
    source->subprovider()->ClearObserver();
    source->tasks()->clear();
  }

  secondary_source_->subprovider()->ClearObserver();
  secondary_source_->tasks()->clear();

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
    NOTREACHED_IN_MIGRATION();
    it->second.InvalidateWeakPtrs();
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&FallbackTaskProvider::ShowPendingTask,
                     it->second.GetWeakPtr(), task),
      kTimeDelayForPendingTask);
}

void FallbackTaskProvider::ShowPendingTask(Task* task) {
  // Pending tasks belong to the secondary source, and showing one means that
  // Chromium is missing a primary task provider.
  if (!allow_fallback_for_testing_) {
    // Log when we use the secondary task provider, to help drive this count to
    // zero and have providers for all known processes.
    // TODO(avi): Turn this into a DCHECK and remove the log once there are
    // providers for all known processes. See https://crbug.com/1083509.
    base::UmaHistogramBoolean("BrowserRenderProcessHost.LabeledInTaskManager",
                              false);
    LOG(ERROR)
        << "Every renderer should have at least one task provided by a primary "
        << "task provider. If a \"Renderer\" fallback task is shown, it is a "
        << "bug. If you have repro steps, please file a new bug and tag it as "
        << "a dependency of crbug.com/739782.";
  }

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
  if (source == secondary_source_.get()) {
    // If a secondary task is added but a primary task is already shown for it,
    // we can ignore showing the secondary.
    for (const auto& primary_source : primary_sources_) {
      if (GetTaskByPidFromVector(task->process_id(), primary_source->tasks()))
        return;
    }

    // Always delay showing a secondary source in case a primary source comes in
    // soon after.
    ShowTaskLater(task);
    return;
  }

  // Log when a primary task is shown instead, to provide a point of comparison
  // for cases the secondary task is shown. Remove when there are providers for
  // for all known processes. See https://crbug.com/1083509.
  base::UmaHistogramBoolean("BrowserRenderProcessHost.LabeledInTaskManager",
                            true);

  // If we get a primary task that has a secondary task that is both known and
  // shown we then hide the secondary task and then show the primary task.
  ShowTask(task);
  for (Task* secondary_task : *secondary_source_->tasks()) {
    if (task->process_id() == secondary_task->process_id())
      HideTask(secondary_task);
  }
}

void FallbackTaskProvider::OnTaskRemovedBySource(Task* task,
                                                 SubproviderSource* source) {
  HideTask(task);

  // When a task from a primary subprovider is removed, see if there are any
  // other primary tasks for that process. If not, but there are secondary
  // tasks, show them.
  if (source != secondary_source_.get()) {
    for (const auto& primary_source : primary_sources_) {
      if (GetTaskByPidFromVector(task->process_id(), primary_source->tasks()))
        return;
    }

    for (Task* secondary_task : *secondary_source_->tasks()) {
      if (task->process_id() == secondary_task->process_id())
        ShowTaskLater(secondary_task);
    }
  }
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

  std::erase(tasks_, task);
  fallback_task_provider_->OnTaskRemovedBySource(task, this);
}

void FallbackTaskProvider::SubproviderSource::TaskUnresponsive(Task* task) {
  fallback_task_provider_->OnTaskUnresponsive(task);
}

}  // namespace task_manager
