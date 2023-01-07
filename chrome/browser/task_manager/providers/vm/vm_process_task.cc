// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/providers/vm/vm_process_task.h"

#include "base/functional/callback_helpers.h"
#include "base/i18n/rtl.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/crostini/crostini_manager.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/child_process_host.h"
#include "ui/base/l10n/l10n_util.h"

namespace task_manager {

namespace {

std::u16string MakeTitle(int ids_vm_prefix, const std::string& vm_name) {
  std::u16string title =
      l10n_util::GetStringFUTF16(ids_vm_prefix, base::UTF8ToUTF16(vm_name));
  base::i18n::AdjustStringForLocaleDirection(&title);
  return title;
}

}  // namespace

VmProcessTask::VmProcessTask(gfx::ImageSkia* icon,
                             int ids_vm_prefix,
                             base::ProcessId pid,
                             const std::string& owner_id,
                             const std::string& vm_name)
    : Task(MakeTitle(ids_vm_prefix, vm_name), icon, pid),
      owner_id_(owner_id),
      vm_name_(vm_name) {}

bool VmProcessTask::IsKillable() {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  return profile && owner_id_ == crostini::CryptohomeIdForProfile(profile);
}

int VmProcessTask::GetChildProcessUniqueID() const {
  // VMs are not child processes of the browser.
  return content::ChildProcessHost::kInvalidUniqueID;
}

}  // namespace task_manager
