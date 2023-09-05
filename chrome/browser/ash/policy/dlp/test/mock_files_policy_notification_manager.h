// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_DLP_TEST_MOCK_FILES_POLICY_NOTIFICATION_MANAGER_H_
#define CHROME_BROWSER_ASH_POLICY_DLP_TEST_MOCK_FILES_POLICY_NOTIFICATION_MANAGER_H_

#include "chrome/browser/ash/policy/dlp/files_policy_notification_manager.h"

#include "base/files/file_path.h"
#include "chrome/browser/ash/file_manager/io_task.h"
#include "chrome/browser/chromeos/policy/dlp/dialogs/policy_dialog_base.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_file_destination.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_files_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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
              (absl::optional<file_manager::io_task::IOTaskId> task_id,
               std::vector<base::FilePath> blocked_files,
               dlp::FileAction action),
              (override));

  MOCK_METHOD(void,
              AddConnectorsBlockedFiles,
              (file_manager::io_task::IOTaskId task_id,
               std::vector<base::FilePath> blocked_files,
               dlp::FileAction action),
              (override));

  MOCK_METHOD(void,
              ShowDlpWarning,
              (OnDlpRestrictionCheckedCallback callback,
               absl::optional<file_manager::io_task::IOTaskId> task_id,
               std::vector<base::FilePath> warning_files,
               const DlpFileDestination& destination,
               dlp::FileAction action),
              (override));

  MOCK_METHOD(void,
              ShowConnectorsWarning,
              (OnDlpRestrictionCheckedCallback callback,
               file_manager::io_task::IOTaskId task_id,
               std::vector<base::FilePath> warning_files,
               dlp::FileAction action),
              (override));

  MOCK_METHOD(void,
              ShowFilesPolicyNotification,
              (const std::string& notification_id,
               const file_manager::io_task::ProgressStatus& status),
              (override));

  MOCK_METHOD(void,
              OnIOTaskResumed,
              (file_manager::io_task::IOTaskId task_id),
              (override));

  MOCK_METHOD(void,
              OnErrorItemDismissed,
              (file_manager::io_task::IOTaskId task_id),
              (override));
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_DLP_TEST_MOCK_FILES_POLICY_NOTIFICATION_MANAGER_H_
