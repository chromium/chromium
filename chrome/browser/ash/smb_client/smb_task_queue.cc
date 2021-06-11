// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/smb_client/smb_task_queue.h"

#include "base/bind.h"
#include "base/callback.h"

namespace chromeos {
namespace smb_client {

SmbTaskQueue::SmbTaskQueue(size_t max_pending) : max_pending_(max_pending) {}
SmbTaskQueue::~SmbTaskQueue() {}

OperationId SmbTaskQueue::GetNextOperationId() {
  return next_operation_id++;
}

void SmbTaskQueue::AddTask(SmbTask task, OperationId operation_id) {
  DCHECK(IsValidOperationId(operation_id));

  if (!operation_map_.count(operation_id)) {
    operations_.push(operation_id);
  }
  operation_map_[operation_id].push(std::move(task));

  RunTaskIfNecessary();
}

void SmbTaskQueue::TaskFinished() {
  DCHECK_GT(num_pending_, 0u);
  --num_pending_;
  RunTaskIfNecessary();
}

void SmbTaskQueue::AbortOperation(OperationId operation_id) {
  DCHECK(IsValidOperationId(operation_id));

  operation_map_.erase(operation_id);
}

void SmbTaskQueue::RunTaskIfNecessary() {
  PruneOperationQueue();
  if (IsCapacityToRunTask() && IsTaskToRun()) {
    RunNextTask();

    // Sanity check that either the maximum number of tasks are running or
    // nothing is in the queue. If there is anything left in the queue to run,
    // then the maximum number of tasks should already be running.
    DCHECK(!IsCapacityToRunTask() || !IsTaskToRun());
  }
}

SmbTask SmbTaskQueue::GetNextTask() {
  DCHECK(IsTaskToRun());
  const OperationId operation_id = operations_.front();

  DCHECK(operation_map_.count(operation_id));
  auto& queue = operation_map_.find(operation_id)->second;

  SmbTask next_task = std::move(queue.front());
  queue.pop();

  if (queue.empty()) {
    operation_map_.erase(operation_id);
    operations_.pop();
  }

  return next_task;
}

void SmbTaskQueue::RunNextTask() {
  DCHECK(IsTaskToRun());

  ++num_pending_;
  GetNextTask().Run();
}

void SmbTaskQueue::PruneOperationQueue() {
  while (!IsPruned()) {
    operations_.pop();
  }
}

bool SmbTaskQueue::IsPruned() const {
  return (operations_.empty() || operation_map_.count(operations_.front()));
}

bool SmbTaskQueue::IsTaskToRun() const {
  DCHECK(IsPruned());
  return !operations_.empty();
}

bool SmbTaskQueue::IsCapacityToRunTask() const {
  return num_pending_ < max_pending_;
}

bool SmbTaskQueue::IsValidOperationId(OperationId operation_id) const {
  return operation_id < next_operation_id;
}

}  // namespace smb_client
}  // namespace chromeos
