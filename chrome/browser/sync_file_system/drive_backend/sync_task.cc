// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/sync_task.h"

#include <utility>

#include "base/bind.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_task_manager.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_task_token.h"
#include "chrome/browser/sync_file_system/drive_backend/task_dependency_manager.h"

namespace sync_file_system {
namespace drive_backend {

namespace {

void CallRunExclusive(const base::WeakPtr<ExclusiveTask>& task,
                      std::unique_ptr<SyncTaskToken> token) {
  if (task)
    task->RunExclusive(SyncTaskToken::WrapToCallback(std::move(token)));
}

}  // namespace

ExclusiveTask::ExclusiveTask() {}
ExclusiveTask::~ExclusiveTask() {}

void ExclusiveTask::RunPreflight(std::unique_ptr<SyncTaskToken> token) {
  std::unique_ptr<TaskBlocker> task_blocker(new TaskBlocker);
  task_blocker->exclusive = true;

  SyncTaskManager::UpdateTaskBlocker(
      std::move(token), std::move(task_blocker),
      base::Bind(&CallRunExclusive, weak_ptr_factory_.GetWeakPtr()));
}

}  // namespace drive_backend
}  // namespace sync_file_system
