// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_PROVIDERS_VM_PLUGIN_VM_PROCESS_TASK_H_
#define CHROME_BROWSER_TASK_MANAGER_PROVIDERS_VM_PLUGIN_VM_PROCESS_TASK_H_

#include <string>

#include "base/macros.h"
#include "chrome/browser/task_manager/providers/vm/vm_process_task.h"

namespace task_manager {

// Defines the task that represents a VM process for PluginVm.
class PluginVmProcessTask : public VmProcessTask {
 public:
  PluginVmProcessTask(base::ProcessId pid,
                      const std::string& owner_id,
                      const std::string& vm_name);
  ~PluginVmProcessTask() override = default;

  // task_manager::Task:
  void Kill() override;
  Type GetType() const override;

 private:
  static gfx::ImageSkia* s_icon_;

  DISALLOW_COPY_AND_ASSIGN(PluginVmProcessTask);
};

}  // namespace task_manager

#endif  // CHROME_BROWSER_TASK_MANAGER_PROVIDERS_VM_PLUGIN_VM_PROCESS_TASK_H_
