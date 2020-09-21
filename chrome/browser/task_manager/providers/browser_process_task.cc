// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/providers/browser_process_task.h"

#include "chrome/browser/task_manager/task_manager_observer.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "third_party/sqlite/sqlite3.h"
#include "ui/base/l10n/l10n_util.h"

namespace task_manager {

gfx::ImageSkia* BrowserProcessTask::s_icon_ = nullptr;

BrowserProcessTask::BrowserProcessTask()
    : Task(l10n_util::GetStringUTF16(IDS_TASK_MANAGER_WEB_BROWSER_CELL_TEXT),
           FetchIcon(IDR_PRODUCT_LOGO_16, &s_icon_),
           base::GetCurrentProcessHandle()),
      used_sqlite_memory_(-1) {}

BrowserProcessTask::~BrowserProcessTask() {
}

bool BrowserProcessTask::IsKillable() {
  // Never kill the browser process.
  return false;
}

void BrowserProcessTask::Kill() {
  // Never kill the browser process.
}

void BrowserProcessTask::Refresh(const base::TimeDelta& update_interval,
                                 int64_t refresh_flags) {
  Task::Refresh(update_interval, refresh_flags);

  if ((refresh_flags & REFRESH_TYPE_SQLITE_MEMORY) != 0)
    used_sqlite_memory_ = static_cast<int64_t>(sqlite3_memory_used());
}

Task::Type BrowserProcessTask::GetType() const {
  return Task::BROWSER;
}

int BrowserProcessTask::GetChildProcessUniqueID() const {
  return 0;
}

int64_t BrowserProcessTask::GetSqliteMemoryUsed() const {
  return used_sqlite_memory_;
}

}  // namespace task_manager
