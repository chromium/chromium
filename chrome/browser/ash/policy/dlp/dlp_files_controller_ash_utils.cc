// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/dlp/dlp_files_controller_ash_utils.h"

#include "base/files/file_path.h"
#include "chrome/browser/ash/file_manager/io_task.h"
#include "chrome/browser/ash/policy/dlp/files_policy_notification_manager.h"
#include "chrome/browser/ash/policy/dlp/files_policy_notification_manager_factory.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_files_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"

namespace policy::files_controller_ash_utils {

void ShowDlpBlockedFiles(Profile* profile,
                         std::optional<file_manager::io_task::IOTaskId> task_id,
                         std::vector<base::FilePath> blocked_files,
                         dlp::FileAction action) {
  DCHECK(profile);

  auto* fpnm =
      FilesPolicyNotificationManagerFactory::GetForBrowserContext(profile);
  if (!fpnm) {
    LOG(ERROR) << "No FilesPolicyNotificationManager instantiated,"
                  "can't show policy block UI";
    return;
  }

  fpnm->ShowDlpBlockedFiles(std::move(task_id), std::move(blocked_files),
                            action);
}
}  // namespace policy::files_controller_ash_utils
