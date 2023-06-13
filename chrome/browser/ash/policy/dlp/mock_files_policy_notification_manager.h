// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_DLP_MOCK_FILES_POLICY_NOTIFICATION_MANAGER_H_
#define CHROME_BROWSER_ASH_POLICY_DLP_MOCK_FILES_POLICY_NOTIFICATION_MANAGER_H_

#include "chrome/browser/ash/policy/dlp/files_policy_notification_manager.h"

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
              (absl::optional<file_manager::io_task::IOTaskId> task_id,
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
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_DLP_MOCK_FILES_POLICY_NOTIFICATION_MANAGER_H_
