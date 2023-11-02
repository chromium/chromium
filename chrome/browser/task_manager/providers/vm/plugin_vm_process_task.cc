// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/providers/vm/plugin_vm_process_task.h"

#include "chrome/browser/ash/plugin_vm/plugin_vm_manager.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_manager_factory.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "chrome/grit/generated_resources.h"

namespace task_manager {

gfx::ImageSkia* PluginVmProcessTask::s_icon_ = nullptr;

PluginVmProcessTask::PluginVmProcessTask(base::ProcessId pid,
                                         const std::string& owner_id,
                                         const std::string& vm_name)
    : VmProcessTask(FetchIcon(IDR_LOGO_PLUGIN_VM_DEFAULT_32, &s_icon_),
                    IDS_TASK_MANAGER_PLUGIN_VM_PREFIX,
                    pid,
                    owner_id,
                    vm_name) {}

void PluginVmProcessTask::Kill() {
  plugin_vm::PluginVmManager* plugin_vm_manager =
      plugin_vm::PluginVmManagerFactory::GetForProfile(
          ProfileManager::GetActiveUserProfile());
  if (plugin_vm_manager)
    plugin_vm_manager->StopPluginVm(vm_name_, /*force=*/true);
}

Task::Type PluginVmProcessTask::GetType() const {
  return Task::PLUGIN_VM;
}

}  // namespace task_manager
