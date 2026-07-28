// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/dlp/test/mock_dlp_files_controller_ash.h"

#include "testing/gmock/include/gmock/gmock.h"

namespace policy {

MockDlpFilesControllerAsh::MockDlpFilesControllerAsh(
    const DlpRulesManager& rules_manager,
    Profile* profile)
    : DlpFilesControllerAsh(rules_manager, profile) {
  ON_CALL(*this, CheckIfLaunchAllowed)
      .WillByDefault([](const apps::AppUpdate& app_update,
                        apps::IntentPtr intent,
                        CheckIfDlpAllowedCallback result_callback) {
        std::move(result_callback).Run(true);
      });
  ON_CALL(*this, GetDlpMetadata)
      .WillByDefault([](const std::vector<storage::FileSystemURL>& files,
                        std::optional<DlpFileDestination> destination,
                        GetDlpMetadataCallback result_callback) {
        std::move(result_callback).Run(std::vector<DlpFileMetadata>());
      });
  ON_CALL(*this, FilterDisallowedUploads)
      .WillByDefault([](std::vector<ui::SelectedFileInfo> selected_files,
                        const DlpFileDestination& destination,
                        FilterDisallowedUploadsCallback result_callback) {
        std::move(result_callback).Run(std::move(selected_files));
      });
  ON_CALL(*this, CheckIfDownloadAllowed)
      .WillByDefault([](const DlpFileDestination& download_src,
                        const base::FilePath& file_path,
                        CheckIfDlpAllowedCallback result_callback) {
        std::move(result_callback).Run(true);
      });
}

MockDlpFilesControllerAsh::~MockDlpFilesControllerAsh() = default;

}  // namespace policy
