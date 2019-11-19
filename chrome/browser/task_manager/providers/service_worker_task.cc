// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/providers/service_worker_task.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace task_manager {

ServiceWorkerTask::ServiceWorkerTask(base::ProcessHandle handle,
                                     int render_process_id,
                                     const GURL& script_url)
    : Task(l10n_util::GetStringFUTF16(IDS_TASK_MANAGER_SERVICE_WORKER_PREFIX,
                                      base::UTF8ToUTF16(script_url.spec())),
           script_url.spec(),
           nullptr /* icon */,
           handle),
      render_process_id_(render_process_id) {}

ServiceWorkerTask::~ServiceWorkerTask() = default;

Task::Type ServiceWorkerTask::GetType() const {
  return Task::SERVICE_WORKER;
}

int ServiceWorkerTask::GetChildProcessUniqueID() const {
  return render_process_id_;
}

}  // namespace task_manager
