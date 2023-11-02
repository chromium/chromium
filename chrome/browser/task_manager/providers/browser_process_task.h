// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_PROVIDERS_BROWSER_PROCESS_TASK_H_
#define CHROME_BROWSER_TASK_MANAGER_PROVIDERS_BROWSER_PROCESS_TASK_H_

#include <stdint.h>

#include "chrome/browser/task_manager/providers/task.h"

namespace task_manager {

// Defines the task that represents the one and only browser main process.
class BrowserProcessTask : public Task {
 public:
  BrowserProcessTask();
  BrowserProcessTask(const BrowserProcessTask&) = delete;
  BrowserProcessTask& operator=(const BrowserProcessTask&) = delete;
  ~BrowserProcessTask() override;

  // task_manager::Task:
  bool IsKillable() override;
  void Kill() override;
  void Refresh(const base::TimeDelta& update_interval,
               int64_t refresh_flags) override;
  Type GetType() const override;
  int GetChildProcessUniqueID() const override;
  int64_t GetSqliteMemoryUsed() const override;

 private:
  static gfx::ImageSkia* s_icon_;

  int64_t used_sqlite_memory_;
};

}  // namespace task_manager

#endif  // CHROME_BROWSER_TASK_MANAGER_PROVIDERS_BROWSER_PROCESS_TASK_H_
