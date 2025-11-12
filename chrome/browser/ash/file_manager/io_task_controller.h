// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_MANAGER_IO_TASK_CONTROLLER_H_
#define CHROME_BROWSER_ASH_FILE_MANAGER_IO_TASK_CONTROLLER_H_

#include <map>
#include <memory>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/ash/file_manager/io_task.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/wake_lock.mojom.h"
#include "storage/browser/file_system/file_system_file_util.h"

namespace file_manager {

namespace io_task {

// |IOTaskController| queues and executes IOTasks given to it from Files app. It
// also notifies observers when tasks progress or finish. This class must only
// be used from one sequence.
class IOTaskController {
 public:
  IOTaskController();
  IOTaskController(const IOTaskController& other) = delete;
  IOTaskController operator=(const IOTaskController& other) = delete;
  ~IOTaskController();

  class Observer : public base::CheckedObserver {
   public:
    // Called whenever a queued IOTask progresses or completes. Will be called
    // on the same sequence as Add().
    virtual void OnIOTaskStatus(const ProgressStatus& status) = 0;
  };

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Queues an IOTask and returns its ID.
  IOTaskId Add(std::unique_ptr<IOTask> task);

  // Pauses a task from the queue.
  void Pause(IOTaskId task_id, PauseParams params);

  // Resumes a task from the queue.
  void Resume(IOTaskId task_id, ResumeParams params);

  // Cancels or removes a task from the queue.
  void Cancel(IOTaskId task_id);

  // Makes tasks in state::PAUSED emit (broadcast) their progress status.
  void ProgressPausedTasks();

  // Aborts a task from the queue.
  void CompleteWithError(IOTaskId task_id, PolicyError policy_error);

  // The `ProgressStatus` of each task in the queue.
  std::vector<std::reference_wrapper<const ProgressStatus>> TaskStatuses();

  // For tests only; returns the current wake lock counter. This counter is
  // incremented by 1 for every time we get a wake lock and decremented every
  // time we release it.
  int wake_lock_counter_for_tests() const {
    return wake_lock_counter_for_tests_;
  }

 private:
  void MaybeNotifyIOTaskObservers(const ProgressStatus& status);
  void NotifyIOTaskObservers(const ProgressStatus& status);
  void OnIOTaskProgress(const ProgressStatus& status);
  void OnIOTaskComplete(IOTaskId task_id, ProgressStatus status);

  // Service method for lazily getting the wake lock.
  device::mojom::WakeLock* GetWakeLock();

  // Put a new task with the given task_id in the task map. This method also
  // manages the wake lock by requesting a lock if this is the first task. It
  // returns the pointer to the just stored task.
  IOTask* PutIOTask(const IOTaskId task_id, std::unique_ptr<IOTask> task);

  // Removes a task by its ID. This method also manages the wake lock by
  // releasing it if this was the last registered task.
  void RemoveIOTask(const IOTaskId task_id);

  SEQUENCE_CHECKER(sequence_checker_);

  base::ObserverList<Observer> observers_;

  IOTaskId last_id_ = 0;
  std::map<IOTaskId, std::unique_ptr<IOTask>> tasks_;

  // For keeping the device awake during IO tasks.
  mojo::Remote<device::mojom::WakeLock> wake_lock_;

  int wake_lock_counter_for_tests_ = 0;

  // A map of when each task has been last notified to its observers.
  base::flat_map<IOTaskId, base::Time> tasks_last_update_;

  base::WeakPtrFactory<IOTaskController> weak_ptr_factory_{this};
};

}  // namespace io_task

}  // namespace file_manager

#endif  // CHROME_BROWSER_ASH_FILE_MANAGER_IO_TASK_CONTROLLER_H_
