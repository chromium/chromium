// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/skyvault/skyvault_rename_handler.h"

#include "base/files/file_util.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/policy/skyvault/drive_upload_observer.h"
#include "chrome/browser/ash/policy/skyvault/odfs_skyvault_uploader.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/download/public/common/download_item.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/download_item_utils.h"

namespace policy {

namespace {
// Returns true if the file should be uploaded to the specified cloud provider.
bool ShouldUploadFile(Profile* profile,
                      SkyvaultRenameHandler::CloudProvider cloud_provider,
                      const base::FilePath& path) {
  switch (cloud_provider) {
    case SkyvaultRenameHandler::kGoogleDrive: {
      drive::DriveIntegrationService* service =
          drive::DriveIntegrationServiceFactory::FindForProfile(profile);
      return service &&
             service->GetRelativeDrivePath(path, /*drive_path=*/nullptr);
    }
    case SkyvaultRenameHandler::kOneDrive:
      base::FilePath tmp_dir;
      if (!base::GetTempDir(&tmp_dir)) {
        return false;
      }
      return tmp_dir.IsParent(path);
  }
}
}  // namespace

// static
std::unique_ptr<SkyvaultRenameHandler> SkyvaultRenameHandler::CreateIfNeeded(
    download::DownloadItem* download_item) {
  if (!base::FeatureList::IsEnabled(features::kSkyVault)) {
    return nullptr;
  }

  Profile* profile = Profile::FromBrowserContext(
      content::DownloadItemUtils::GetBrowserContext(download_item));
  if (!profile) {
    return nullptr;
  }

  const auto downloads_path =
      profile->GetPrefs()->GetFilePath(::prefs::kDownloadDefaultDirectory);

  size_t position = downloads_path.value().find(
      local_user_files::kOneDrivePolicyVariableName);
  if (position != base::FilePath::StringType::npos) {
    return std::make_unique<SkyvaultRenameHandler>(
        profile, CloudProvider::kOneDrive, download_item);
  }

  position = downloads_path.value().find(
      local_user_files::kGoogleDrivePolicyVariableName);
  if (position != base::FilePath::StringType::npos &&
      !local_user_files::LocalUserFilesAllowed()) {
    return std::make_unique<SkyvaultRenameHandler>(
        profile, CloudProvider::kGoogleDrive, download_item);
  }

  return nullptr;
}

SkyvaultRenameHandler::SkyvaultRenameHandler(
    Profile* profile,
    CloudProvider cloud_provider,
    download::DownloadItem* download_item)
    : download::DownloadItemRenameHandler(download_item),
      profile_(profile),
      cloud_provider_(cloud_provider) {}

SkyvaultRenameHandler::~SkyvaultRenameHandler() = default;

void SkyvaultRenameHandler::Start(ProgressCallback progress_callback,
                                  RenameCallback rename_callback) {
  progress_callback_ = std::move(progress_callback);
  rename_callback_ = std::move(rename_callback);

  if (!ShouldUploadFile(profile_, cloud_provider_,
                        download_item_->GetTargetFilePath())) {
    std::move(rename_callback_)
        .Run(download::DOWNLOAD_INTERRUPT_REASON_NONE,
             download_item_->GetTargetFilePath());
    return;
  }

  switch (cloud_provider_) {
    case CloudProvider::kGoogleDrive:
      ash::cloud_upload::DriveUploadObserver::Observe(
          profile_, download_item_->GetTargetFilePath(),
          policy::local_user_files::UploadTrigger::kDownload,
          download_item_->GetTotalBytes(),
          base::BindRepeating(&SkyvaultRenameHandler::OnProgressUpdate,
                              weak_factory_.GetWeakPtr()),
          base::BindOnce(&SkyvaultRenameHandler::OnDriveUploadDone,
                         weak_factory_.GetWeakPtr()));
      break;

    case CloudProvider::kOneDrive:
      ash::cloud_upload::OdfsSkyvaultUploader::Upload(
          profile_, download_item_->GetTargetFilePath(),
          policy::local_user_files::UploadTrigger::kDownload,
          base::BindRepeating(&SkyvaultRenameHandler::OnProgressUpdate,
                              weak_factory_.GetWeakPtr()),
          base::BindOnce(&SkyvaultRenameHandler::OnOneDriveUploadDone,
                         weak_factory_.GetWeakPtr()));
      break;
  }
}

bool SkyvaultRenameHandler::ShowRenameProgress() {
  return true;
}

void SkyvaultRenameHandler::StartForTesting(ProgressCallback progress_callback,
                                            RenameCallback rename_callback) {
  progress_callback_ = std::move(progress_callback);
  rename_callback_ = std::move(rename_callback);
}

void SkyvaultRenameHandler::OnProgressUpdate(int64_t bytes_so_far) {
  if (!progress_callback_.is_null() && bytes_so_far > -1) {
    progress_callback_.Run(bytes_so_far, /*bytes_per_sec=*/0u);
  }
}

void SkyvaultRenameHandler::OnDriveUploadDone(bool success) {
  DCHECK(!rename_callback_.is_null());

  auto reason = success ? download::DOWNLOAD_INTERRUPT_REASON_NONE
                        : download::DOWNLOAD_INTERRUPT_REASON_FILE_FAILED;
  std::move(rename_callback_).Run(reason, download_item_->GetTargetFilePath());
}

void SkyvaultRenameHandler::OnOneDriveUploadDone(
    bool success,
    storage::FileSystemURL file_url) {
  DCHECK(!rename_callback_.is_null());

  auto reason = success ? download::DOWNLOAD_INTERRUPT_REASON_NONE
                        : download::DOWNLOAD_INTERRUPT_REASON_FILE_FAILED;
  if (success) {
    download_item_->SetDisplayName(file_url.path().BaseName());
  }
  std::move(rename_callback_).Run(reason, file_url.path());
}

}  // namespace policy
