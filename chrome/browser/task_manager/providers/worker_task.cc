// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/providers/worker_task.h"

#include <string>

#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace task_manager {

namespace {

int GetTaskTitlePrefixMessageId(Task::Type task_type) {
  switch (task_type) {
    case Task::Type::DEDICATED_WORKER:
      return IDS_TASK_MANAGER_DEDICATED_WORKER_PREFIX;
    case Task::Type::SHARED_WORKER:
      return IDS_TASK_MANAGER_SHARED_WORKER_PREFIX;
    case Task::Type::SERVICE_WORKER:
      return IDS_TASK_MANAGER_SERVICE_WORKER_PREFIX;
    default:
      NOTREACHED_IN_MIGRATION();
      return 0;
  }
}

std::u16string GetTaskTitle(const GURL& script_url, Task::Type task_type) {
  return l10n_util::GetStringFUTF16(GetTaskTitlePrefixMessageId(task_type),
                                    base::UTF8ToUTF16(script_url.spec()));
}

}  // namespace

WorkerTask::WorkerTask(base::ProcessHandle handle,
                       Task::Type task_type,
                       int render_process_id)
    : Task(GetTaskTitle(/*script_url=*/GURL(), task_type),
           /*icon=*/nullptr,
           handle),
      task_type_(task_type),
      render_process_id_(render_process_id) {}

WorkerTask::~WorkerTask() = default;

Task::Type WorkerTask::GetType() const {
  return task_type_;
}

int WorkerTask::GetChildProcessUniqueID() const {
  return render_process_id_;
}

void WorkerTask::SetScriptUrl(const GURL& script_url) {
  set_title(GetTaskTitle(script_url, task_type_));
}

}  // namespace task_manager
