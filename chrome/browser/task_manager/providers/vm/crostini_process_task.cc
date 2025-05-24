// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/providers/vm/crostini_process_task.h"

#include "base/functional/callback_helpers.h"
#include "chrome/browser/ash/crostini/crostini_manager.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "chrome/grit/generated_resources.h"

namespace task_manager {

gfx::ImageSkia* CrostiniProcessTask::s_icon_ = nullptr;

CrostiniProcessTask::CrostiniProcessTask(base::ProcessId pid,
                                         const std::string& owner_id,
                                         const std::string& vm_name)
    : VmProcessTask(FetchIcon(IDR_LOGO_CROSTINI_DEFAULT, &s_icon_),
                    IDS_TASK_MANAGER_LINUX_VM_PREFIX,
                    pid,
                    owner_id,
                    vm_name) {}

bool CrostiniProcessTask::Kill() {
  crostini::CrostiniManager* crostini_manager =
      crostini::CrostiniManager::GetForProfile(
          ProfileManager::GetActiveUserProfile());
  if (crostini_manager) {
    // TODO(crbug.com/409837763): Crostini StopVM() doesn't return a value
    // because it's asynchronous, but a synchronous result would be useful to
    // bubble upward. This requires some additional design. For now, assume that
    // calling this function guaranteed stops the VM.
    crostini_manager->StopVm(vm_name_, base::DoNothing());
    return true;
  }
  return false;
}

Task::Type CrostiniProcessTask::GetType() const {
  return Task::CROSTINI;
}

}  // namespace task_manager
