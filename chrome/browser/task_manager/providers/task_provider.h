// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_PROVIDERS_TASK_PROVIDER_H_
#define CHROME_BROWSER_TASK_MANAGER_PROVIDERS_TASK_PROVIDER_H_

#include "base/macros.h"
#include "chrome/browser/task_manager/providers/task_provider_observer.h"

namespace task_manager {

// Defines the interface for task providers. A concrete task provider must be
// able to collect all the tasks of a particular type which this provider
// supports as well as track any tasks addition / removal. Once StartUpdating()
// is called, the provider is responsible for notifying the observer about the
// tasks it's tracking. The TaskProviders own the tasks they provide.
class TaskProvider {
 public:
  TaskProvider();
  virtual ~TaskProvider();

  // Should return the task associated to the specified IDs from a
  // |content::ResourceRequestInfo| that represents a |URLRequest|. A value of
  // nullptr will be returned if the desired task does not belong to this
  // provider.
  //
  // |child_id| is the unique ID of the host of the child process requestor.
  // |route_id| is the ID of the IPC route for the URLRequest (this identifies
  // the RenderFrame in the renderer that initiated the request). |route_id|
  // may be ignored if |child_id| is not a renderer process.
  virtual Task* GetTaskOfUrlRequest(int child_id, int route_id) = 0;

  // Set the sole observer of this provider. It's an error to set an observer
  // if there's already one there.
  void SetObserver(TaskProviderObserver* observer);

  // Clears the currently set observer for this provider. It's an error to clear
  // the observer if there's no one set.
  void ClearObserver();

 protected:
  // Indicates if this instance is currently tracking tasks. Will return true
  // between the calls to StartUpdating() and StopUpdating().
  bool IsUpdating() const;

  // Used by concrete task providers to notify the observer of tasks addition/
  // removal/renderer unresponsive. These methods should only be called after
  // StartUpdating() has been called and before StopUpdating() is called.
  void NotifyObserverTaskAdded(Task* task) const;
  void NotifyObserverTaskRemoved(Task* task) const;
  void NotifyObserverTaskUnresponsive(Task* task) const;
  void UpdateTaskProcessInfoAndNotifyObserver(
      Task* existing_task,
      base::ProcessHandle new_process_handle,
      base::ProcessId new_process_id) const;

 private:
  // This will be called once an observer is set for this provider. When it is
  // called, the concrete provider must notify the observer of all pre-existing
  // tasks as well as track new addition and terminations and notify the
  // observer of these changes.
  virtual void StartUpdating() = 0;

  // This will be called once the observer is cleared, at which point the
  // provider can stop tracking tasks addition / removal and can clear its own
  // resources.
  virtual void StopUpdating() = 0;

  // We support only one single obsever which will be the sampler in this case.
  TaskProviderObserver* observer_;

  DISALLOW_COPY_AND_ASSIGN(TaskProvider);
};

}  // namespace task_manager

#endif  // CHROME_BROWSER_TASK_MANAGER_PROVIDERS_TASK_PROVIDER_H_
