// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/dlp/files_policy_notification_manager.h"

#include "base/check.h"
#include "chrome/browser/ash/file_manager/io_task.h"
#include "chrome/browser/chromeos/policy/dlp/dialogs/files_policy_dialog.h"

namespace policy {

FilesPolicyNotificationManager::FilesPolicyNotificationManager(
    content::BrowserContext* context) {
  DCHECK(context);
}

FilesPolicyNotificationManager::~FilesPolicyNotificationManager() = default;

// TODO(b/281047025): Add implementation.
void FilesPolicyNotificationManager::ShowDialog(
    file_manager::io_task::IOTaskId task_id,
    FilesDialogType type) {}

}  // namespace policy
