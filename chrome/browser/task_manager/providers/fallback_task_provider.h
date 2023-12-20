// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_PROVIDERS_FALLBACK_TASK_PROVIDER_H_
#define CHROME_BROWSER_TASK_MANAGER_PROVIDERS_FALLBACK_TASK_PROVIDER_H_

#include <map>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/task_manager/providers/task_provider.h"
#include "chrome/browser/task_manager/providers/task_provider_observer.h"

namespace task_manager {

// This FallbackTaskProvider is created to manage a hierarchy of subproviders.
// Tasks from the primary subproviders are always shown in the Task Manager.
// Tasks from the secondary subprovider are only shown when no task from any of
// the primary providers exists for that process.
class FallbackTaskProvider : public TaskProvider {
 public:
  FallbackTaskProvider(
      std::vector<std::unique_ptr<TaskProvider>> primary_subproviders,
      std::unique_ptr<TaskProvider> secondary_subprovider);
  FallbackTaskProvider(const FallbackTaskProvider&) = delete;
  FallbackTaskProvider& operator=(const FallbackTaskProvider&) = delete;
  ~FallbackTaskProvider() override;

  // task_manager::TaskProvider:
  Task* GetTaskOfUrlRequest(int child_id, int route_id) override;

 private:
  friend class FallbackTaskProviderTest;
  class SubproviderSource;

  // task_manager::TaskProvider:
  void StartUpdating() override;
  void StopUpdating() override;

  // This is used to show a task after |OnTaskAddedBySource| has decided that it
  // is appropriate to show that task.
  void ShowTask(Task* task);

  // Called to add a task to the |pending_shown_tasks_| which delays showing the
  // task for a duration controlled by kTimeDelayForPendingTask.
  void ShowTaskLater(Task* task);

  // This is called after the delay to show the task that has been delayed.
  void ShowPendingTask(Task* task);

  // This is used to hide a task after |OnTaskAddedBySource| has decided that it
  // is appropriate to hide that task.
  void HideTask(Task* task);

  void OnTaskUnresponsive(Task* task);

  void OnTaskAddedBySource(Task* task, SubproviderSource* source);
  void OnTaskRemovedBySource(Task* task, SubproviderSource* source);

  // Stores the wrapped subproviders.
  std::vector<std::unique_ptr<SubproviderSource>> primary_sources_;
  std::unique_ptr<SubproviderSource> secondary_source_;

  // This is the set of tasks that this provider is currently passing up to
  // whatever is observing it.
  std::vector<raw_ptr<Task, VectorExperimental>> shown_tasks_;

  // This maps a Task to a WeakPtrFactory so when a task is removed we can
  // cancel showing a task that has been removed before it has been shown.
  std::map<Task*, base::WeakPtrFactory<FallbackTaskProvider>>
      pending_shown_tasks_;

  // This flag specifies whether the use of a fallback task is an error. For
  // releases it is, but the checking needs to be turned off during testing of
  // this class itself.
  bool allow_fallback_for_testing_ = false;
};

class FallbackTaskProvider::SubproviderSource : public TaskProviderObserver {
 public:
  SubproviderSource(FallbackTaskProvider* fallback_task_provider,
                    std::unique_ptr<TaskProvider> subprovider);
  ~SubproviderSource() override;

  TaskProvider* subprovider() { return subprovider_.get(); }
  std::vector<raw_ptr<Task, VectorExperimental>>* tasks() { return &tasks_; }

 private:
  friend class FallbackTaskProviderTest;
  void TaskAdded(Task* task) override;
  void TaskRemoved(Task* task) override;
  void TaskUnresponsive(Task* task) override;

  // The outer task provider on whose behalf we observe the |subprovider_|. This
  // is a pointer back to the class that owns us.
  raw_ptr<FallbackTaskProvider> fallback_task_provider_;

  // The task provider that we are observing.
  std::unique_ptr<TaskProvider> subprovider_;

  // The vector of tasks that have been created by |subprovider_|.
  std::vector<raw_ptr<Task, VectorExperimental>> tasks_;
};

}  // namespace task_manager

#endif  // CHROME_BROWSER_TASK_MANAGER_PROVIDERS_FALLBACK_TASK_PROVIDER_H_
