// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_PROVIDERS_VM_CROSTINI_PROCESS_TASK_H_
#define CHROME_BROWSER_TASK_MANAGER_PROVIDERS_VM_CROSTINI_PROCESS_TASK_H_

#include <string>

#include "chrome/browser/task_manager/providers/vm/vm_process_task.h"

namespace task_manager {

// Defines the task that represents a VM process for Crostini.
class CrostiniProcessTask : public VmProcessTask {
 public:
  CrostiniProcessTask(base::ProcessId pid,
                      const std::string& owner_id,
                      const std::string& vm_name);
  CrostiniProcessTask(const CrostiniProcessTask&) = delete;
  CrostiniProcessTask& operator=(const CrostiniProcessTask&) = delete;
  ~CrostiniProcessTask() override = default;

  // task_manager::Task:
  void Kill() override;
  Type GetType() const override;

 private:
  static gfx::ImageSkia* s_icon_;
};

}  // namespace task_manager

#endif  // CHROME_BROWSER_TASK_MANAGER_PROVIDERS_VM_CROSTINI_PROCESS_TASK_H_
