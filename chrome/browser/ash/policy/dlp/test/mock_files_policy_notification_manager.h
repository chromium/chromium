// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_DLP_TEST_MOCK_FILES_POLICY_NOTIFICATION_MANAGER_H_
#define CHROME_BROWSER_ASH_POLICY_DLP_TEST_MOCK_FILES_POLICY_NOTIFICATION_MANAGER_H_

#include <optional>

#include "base/files/file_path.h"
#include "chrome/browser/ash/file_manager/io_task.h"
#include "chrome/browser/ash/policy/dlp/dialogs/files_policy_dialog.h"
#include "chrome/browser/ash/policy/dlp/files_policy_notification_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dialogs/policy_dialog_base.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_file_destination.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_files_utils.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::Mock;

namespace policy {

class MockFilesPolicyNotificationManager
    : public FilesPolicyNotificationManager {
 public:
  MockFilesPolicyNotificationManager() = delete;
  explicit MockFilesPolicyNotificationManager(content::BrowserContext* context);
  MockFilesPolicyNotificationManager(
      const MockFilesPolicyNotificationManager& other) = delete;
  MockFilesPolicyNotificationManager& operator=(
      const MockFilesPolicyNotificationManager& other) = delete;
  ~MockFilesPolicyNotificationManager() override;

  MOCK_METHOD(void,
              ShowDlpBlockedFiles,
              (std::optional<file_manager::io_task::IOTaskId> task_id,
               std::vector<base::FilePath> blocked_files,
               dlp::FileAction action),
              (override));

  MOCK_METHOD(void,
              SetConnectorsBlockedFiles,
              (file_manager::io_task::IOTaskId task_id,
               dlp::FileAction action,
               FilesPolicyDialog::BlockReason reason,
               FilesPolicyDialog::Info dialog_into),
              (override));

  MOCK_METHOD(void,
              ShowDlpWarning,
              (WarningWithJustificationCallback callback,
               std::optional<file_manager::io_task::IOTaskId> task_id,
               std::vector<base::FilePath> warning_files,
               const DlpFileDestination& destination,
               dlp::FileAction action),
              (override));

  MOCK_METHOD(void,
              ShowConnectorsWarning,
              (WarningWithJustificationCallback callback,
               file_manager::io_task::IOTaskId task_id,
               dlp::FileAction action,
               FilesPolicyDialog::Info dialog_info),
              (override));

  MOCK_METHOD(void,
              ShowFilesPolicyNotification,
              (const std::string& notification_id,
               const file_manager::io_task::ProgressStatus& status),
              (override));

  MOCK_METHOD(void,
              ShowDialog,
              (file_manager::io_task::IOTaskId task_id, FilesDialogType),
              (override));

  MOCK_METHOD(void,
              OnIOTaskResumed,
              (file_manager::io_task::IOTaskId task_id),
              (override));

  MOCK_METHOD(void,
              OnErrorItemDismissed,
              (file_manager::io_task::IOTaskId task_id),
              (override));

  MOCK_METHOD(void,
              OnIOTaskStatus,
              (const file_manager::io_task::ProgressStatus&),
              (override));
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_DLP_TEST_MOCK_FILES_POLICY_NOTIFICATION_MANAGER_H_
