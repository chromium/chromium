// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_DLP_TEST_MOCK_DLP_FILES_CONTROLLER_ASH_H_
#define CHROME_BROWSER_ASH_POLICY_DLP_TEST_MOCK_DLP_FILES_CONTROLLER_ASH_H_

#include "chrome/browser/ash/policy/dlp/dlp_files_controller_ash.h"

#include "testing/gmock/include/gmock/gmock.h"

using ::testing::Mock;

namespace policy {

class MockDlpFilesControllerAsh : public DlpFilesControllerAsh {
 public:
  MockDlpFilesControllerAsh() = delete;
  explicit MockDlpFilesControllerAsh(const DlpRulesManager& rules_manager,
                                     Profile* profile);
  MockDlpFilesControllerAsh(const MockDlpFilesControllerAsh& other) = delete;
  MockDlpFilesControllerAsh& operator=(const MockDlpFilesControllerAsh& other) =
      delete;
  ~MockDlpFilesControllerAsh() override;

  MOCK_METHOD(void,
              CheckIfTransferAllowed,
              (std::optional<file_manager::io_task::IOTaskId> task_id,
               const std::vector<storage::FileSystemURL>& transferred_files,
               storage::FileSystemURL destination,
               bool is_move,
               CheckIfTransferAllowedCallback result_callback),
              (override));

  MOCK_METHOD(void,
              GetDlpMetadata,
              (const std::vector<storage::FileSystemURL>& files,
               std::optional<DlpFileDestination> destination,
               GetDlpMetadataCallback result_callback),
              (override));

  MOCK_METHOD(void,
              FilterDisallowedUploads,
              (std::vector<ui::SelectedFileInfo> selected_files,
               const DlpFileDestination& destination,
               FilterDisallowedUploadsCallback result_callback),
              (override));

  MOCK_METHOD(void,
              CheckIfDownloadAllowed,
              (const DlpFileDestination& download_src,
               const base::FilePath& file_path,
               CheckIfDlpAllowedCallback result_callback),
              (override));

  MOCK_METHOD(bool,
              ShouldPromptBeforeDownload,
              (const DlpFileDestination& download_src,
               const base::FilePath& file_path),
              (override));

  MOCK_METHOD(void,
              CheckIfLaunchAllowed,
              (const apps::AppUpdate& app_update,
               apps::IntentPtr intent,
               CheckIfDlpAllowedCallback result_callback),
              (override));

  MOCK_METHOD(bool,
              IsLaunchBlocked,
              (const apps::AppUpdate& app_update,
               const apps::IntentPtr& intent),
              (override));

  MOCK_METHOD(void,
              IsFilesTransferRestricted,
              (std::optional<file_manager::io_task::IOTaskId> task_id,
               const std::vector<FileDaemonInfo>& transferred_files,
               const DlpFileDestination& destination,
               dlp::FileAction files_action,
               IsFilesTransferRestrictedCallback result_callback),
              (override));

  MOCK_METHOD(std::vector<DlpFileRestrictionDetails>,
              GetDlpRestrictionDetails,
              (const std::string& source_url),
              (override));

  MOCK_METHOD(std::vector<data_controls::Component>,
              GetBlockedComponents,
              (const std::string& source_url),
              (override));

  MOCK_METHOD(bool,
              IsDlpPolicyMatched,
              (const FileDaemonInfo& file),
              (override));

  MOCK_METHOD(void,
              CheckIfPasteOrDropIsAllowed,
              (const std::vector<base::FilePath>& files,
               const ui::DataTransferEndpoint* data_dst,
               CheckIfDlpAllowedCallback result_callback),
              (override));
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_DLP_TEST_MOCK_DLP_FILES_CONTROLLER_ASH_H_
