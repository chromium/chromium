// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SMB_CLIENT_SMB_TASK_QUEUE_H_
#define CHROME_BROWSER_CHROMEOS_SMB_CLIENT_SMB_TASK_QUEUE_H_

#include "base/callback.h"
#include "base/containers/queue.h"
#include "chromeos/dbus/smb_provider_client.h"

namespace chromeos {
namespace smb_client {

// An SmbTask is a call to SmbProviderClient with a bound SmbFileSystem callback
// that runs when SmbProviderClient receives a D-Bus message response.
using SmbTask = base::OnceClosure;
using OperationId = uint32_t;

// SmbTaskQueue handles the queuing of SmbTasks. Tasks are 'pending' while
// SmbProviderClient awaits a D-Bus Message Response. Tasks are added to
// the queue via SmbTaskQueue::AddTask. Upon the SmbFileSystem callback in the
// task running, the caller must call SmbTaskQueue::TaskFinished to allow the
// next task to run.
//
// Example:
//
//  AbortCallback CreateDirectory(FilePath directory_path, bool recursive,
//                                StatusCallback callback) {
//    auto reply = base::BindOnce(&SmbFileSystem::HandleStatusCallback,
//                                AsWeakPtr(), callback);
//
//    SmbTask task = base::BindOnce(&SmbProviderClient::CreateDirectory,
//                                  base::Unretained(GetSmbProviderClient()),
//                                  GetMountId(),
//                                  directory_path,
//                                  recursive,
//                                  std::move(reply));
//    OperationId operation_id = 1;
//    tq_.AddTask(std::move(task));
//
//    return CreateAbortCallbackForOperationId(operation_id);
//  }
//
//  void HandleStatusCallback(StatusCallback callback, ErrorType error) {
//    tq_.TaskFinished();
//    callback.Run(error);
//  }
class SmbTaskQueue {
 public:
  explicit SmbTaskQueue(size_t max_pending);
  SmbTaskQueue(const SmbTaskQueue&) = delete;
  SmbTaskQueue& operator=(const SmbTaskQueue&) = delete;
  ~SmbTaskQueue();

  // Provides the caller with a new OperationId to associate new tasks with.
  OperationId GetNextOperationId();

  // Adds the sub-task |task| for the Operation |operation_id| to the task
  // queue. If fewer that max_pending_ tasks are outstanding, |task| will run
  // immediately. Otherwise, it will be added to operations_ and run in FIFO
  // order.
  void AddTask(SmbTask task, OperationId operation_id);

  // Attempts to abort any outstanding tasks associated with the operation
  // |operation_id|. Any subtasks that have not been sent over to D-Bus to the
  // Smb Daemon will be cancelled, and OperationId will be removed from
  // operation_map_.
  void AbortOperation(OperationId operation_id);

  // Must be called by the owner of this class to indicate that a response to a
  // task was received.
  void TaskFinished();

 private:
  using TaskList = base::queue<SmbTask>;

  // This runs the next task in operations_ if there is capacity to run an
  // additional task, and a task remaing to run.
  void RunTaskIfNecessary();

  // Helper method that returns the next task to run.
  SmbTask GetNextTask();

  // Helper method that runs the next task.
  void RunNextTask();

  // Prunes operations_ by removing OperationIds from the front of the queue if
  // there are no tasks associated with them.
  void PruneOperationQueue();

  // Helper method that returns whether operations_ has been pruned.
  bool IsPruned() const;

  // Helper method that returns whether there are tasks in operations_ to run.
  // operations_ must be pruned (i.e. the top Operation in operations_ must have
  // atleast 1 task associated with it).
  bool IsTaskToRun() const;

  // Helper method that returns whether there are fewer than max_pending tasks
  // outstanding.
  bool IsCapacityToRunTask() const;

  // Helper method that returns whether |operation_id| is valid.
  bool IsValidOperationId(OperationId operation_id) const;

  const size_t max_pending_;
  size_t num_pending_ = 0;
  OperationId next_operation_id = 0;
  base::queue<OperationId> operations_;
  std::map<OperationId, TaskList> operation_map_;
};

}  // namespace smb_client
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SMB_CLIENT_SMB_TASK_QUEUE_H_
