// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/providers/crostini/crostini_process_task.h"

#include "base/i18n/rtl.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/crostini/crostini_manager.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/common/child_process_host.h"
#include "ui/base/l10n/l10n_util.h"

namespace task_manager {

namespace {
base::string16 MakeTitle(const std::string& vm_name) {
  base::string16 title = l10n_util::GetStringFUTF16(
      IDS_TASK_MANAGER_LINUX_VM_PREFIX, base::UTF8ToUTF16(vm_name));
  base::i18n::AdjustStringForLocaleDirection(&title);
  return title;
}

}  // namespace

gfx::ImageSkia* CrostiniProcessTask::s_icon_ = nullptr;

CrostiniProcessTask::CrostiniProcessTask(base::ProcessId pid,
                                         const std::string& owner_id,
                                         const std::string& vm_name)
    : Task(MakeTitle(vm_name),
           vm_name,
           FetchIcon(IDR_LOGO_CROSTINI_DEFAULT, &s_icon_),
           pid),
      owner_id_(owner_id),
      vm_name_(vm_name) {}

CrostiniProcessTask::~CrostiniProcessTask() = default;

bool CrostiniProcessTask::IsKillable() {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  return profile && owner_id_ == crostini::CryptohomeIdForProfile(profile);
}

void CrostiniProcessTask::Kill() {
  crostini::CrostiniManager* crostini_manager =
      crostini::CrostiniManager::GetForProfile(
          ProfileManager::GetActiveUserProfile());
  if (crostini_manager)
    crostini_manager->StopVm(vm_name_, base::DoNothing());
}

Task::Type CrostiniProcessTask::GetType() const {
  return Task::CROSTINI;
}

int CrostiniProcessTask::GetChildProcessUniqueID() const {
  // Crostini VMs are not a child process of the browser.
  return content::ChildProcessHost::kInvalidUniqueID;
}

}  // namespace task_manager
