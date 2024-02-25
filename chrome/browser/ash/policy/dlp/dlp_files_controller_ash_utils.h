// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_DLP_DLP_FILES_CONTROLLER_ASH_UTILS_H_
#define CHROME_BROWSER_ASH_POLICY_DLP_DLP_FILES_CONTROLLER_ASH_UTILS_H_

#include "base/files/file_path.h"
#include "chrome/browser/ash/file_manager/io_task.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_files_utils.h"
#include "chrome/browser/profiles/profile.h"

namespace policy::files_controller_ash_utils {

// Shows a DLP block desktop notification.
void ShowDlpBlockedFiles(Profile* profile,
                         std::optional<file_manager::io_task::IOTaskId> task_id,
                         std::vector<base::FilePath> blocked_files,
                         dlp::FileAction action);

}  // namespace policy::files_controller_ash_utils

#endif  // CHROME_BROWSER_ASH_POLICY_DLP_DLP_FILES_CONTROLLER_ASH_UTILS_H_
