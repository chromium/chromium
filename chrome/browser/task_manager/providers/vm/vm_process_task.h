// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_PROVIDERS_VM_VM_PROCESS_TASK_H_
#define CHROME_BROWSER_TASK_MANAGER_PROVIDERS_VM_VM_PROCESS_TASK_H_

#include <string>

#include "base/macros.h"
#include "chrome/browser/task_manager/providers/task.h"

namespace task_manager {

// Defines the task that represents a VM process.
class VmProcessTask : public Task {
 public:
  VmProcessTask(gfx::ImageSkia* icon,
                int ids_vm_prefix,
                base::ProcessId pid,
                const std::string& owner_id,
                const std::string& vm_name);
  ~VmProcessTask() override = default;

  // task_manager::Task:
  bool IsKillable() override;
  int GetChildProcessUniqueID() const override;

 protected:
  std::string owner_id_;
  std::string vm_name_;

  DISALLOW_COPY_AND_ASSIGN(VmProcessTask);
};

}  // namespace task_manager

#endif  // CHROME_BROWSER_TASK_MANAGER_PROVIDERS_VM_VM_PROCESS_TASK_H_
