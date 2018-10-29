// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_PROVIDERS_CROSTINI_CROSTINI_PROCESS_TASK_H_
#define CHROME_BROWSER_TASK_MANAGER_PROVIDERS_CROSTINI_CROSTINI_PROCESS_TASK_H_

#include <string>

#include "base/macros.h"
#include "chrome/browser/task_manager/providers/task.h"

namespace task_manager {

// Defines the task that represents a VM process for Crostini.
class CrostiniProcessTask : public Task {
 public:
  CrostiniProcessTask(base::ProcessId pid,
                      const std::string& owner_id,
                      const std::string& vm_name);
  ~CrostiniProcessTask() override;

  // task_manager::Task:
  bool IsKillable() override;
  void Kill() override;
  Type GetType() const override;
  int GetChildProcessUniqueID() const override;

 private:
  static gfx::ImageSkia* s_icon_;

  std::string owner_id_;
  std::string vm_name_;

  DISALLOW_COPY_AND_ASSIGN(CrostiniProcessTask);
};

}  // namespace task_manager

#endif  // CHROME_BROWSER_TASK_MANAGER_PROVIDERS_CROSTINI_CROSTINI_PROCESS_TASK_H_
