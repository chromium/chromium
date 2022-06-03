// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_MANAGER_IO_TASK_CONTROLLER_H_
#define CHROME_BROWSER_ASH_FILE_MANAGER_IO_TASK_CONTROLLER_H_

#include <map>
#include <memory>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/ash/file_manager/io_task.h"
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

  // Cancels or removes a task from the queue.
  void Cancel(IOTaskId task_id);

 private:
  void NotifyIOTaskObservers(const ProgressStatus& status);
  void OnIOTaskProgress(const ProgressStatus& status);
  void OnIOTaskComplete(IOTaskId task_id, ProgressStatus status);

  SEQUENCE_CHECKER(sequence_checker_);

  base::ObserverList<Observer> observers_;

  IOTaskId last_id_ = 0;
  std::map<IOTaskId, std::unique_ptr<IOTask>> tasks_;

  base::WeakPtrFactory<IOTaskController> weak_ptr_factory_{this};
};

}  // namespace io_task

}  // namespace file_manager

#endif  // CHROME_BROWSER_ASH_FILE_MANAGER_IO_TASK_CONTROLLER_H_
