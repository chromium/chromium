// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_SKYVAULT_SKYVAULT_RENAME_HANDLER_H_
#define CHROME_BROWSER_ASH_POLICY_SKYVAULT_SKYVAULT_RENAME_HANDLER_H_

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "components/download/public/common/download_item_rename_handler.h"
#include "storage/browser/file_system/file_system_url.h"

class Profile;

namespace policy {

// An implementation of download::DownloadItemRenameHandler that sends a
// download item file to a cloud-based storage provider as specified in the
// DownloadDirectory policy.
// This class is used when the download directory is set to OneDrive or set to
// GoogleDrive and local files are disabled.
class SkyvaultRenameHandler : public download::DownloadItemRenameHandler {
 public:
  // The cloud provider to which the file will be uploaded.
  enum CloudProvider {
    kGoogleDrive,  // Google Drive.
    kOneDrive,     // Microsoft OneDrive.
  };

  static std::unique_ptr<policy::SkyvaultRenameHandler> CreateIfNeeded(
      download::DownloadItem* download_item);

  SkyvaultRenameHandler(Profile* profile,
                        CloudProvider cloud_provider,
                        download::DownloadItem* download_item);

  SkyvaultRenameHandler(const SkyvaultRenameHandler&) = delete;
  SkyvaultRenameHandler& operator=(const SkyvaultRenameHandler&) = delete;

  ~SkyvaultRenameHandler() override;

  // download::DownloadItemRenameHandler interface.
  void Start(ProgressCallback progress_callback,
             RenameCallback rename_callback) override;
  bool ShowRenameProgress() override;

  void StartForTesting(ProgressCallback progress_callback,
                       RenameCallback rename_callback);

  FRIEND_TEST_ALL_PREFIXES(SkyvaultRenameHandlerTest, SuccessfulUploadToGDrive);
  FRIEND_TEST_ALL_PREFIXES(SkyvaultRenameHandlerTest, FailedUploadToGDrive);
  FRIEND_TEST_ALL_PREFIXES(SkyvaultRenameHandlerTest,
                           SuccessfulUploadToOneDrive);
  FRIEND_TEST_ALL_PREFIXES(SkyvaultRenameHandlerTest, FailedUploadToOneDrive);

 private:
  // Invoked when there's progress updated.
  void OnProgressUpdate(int64_t bytes_so_far);

  // Invoked when the file upload to GoogleDrive is complete.
  void OnDriveUploadDone(bool success);

  // Invoked when the file upload to Microsoft OneDrive is complete.
  void OnOneDriveUploadDone(bool success, storage::FileSystemURL file_url);

  raw_ptr<Profile> profile_;

  // The cloud provider to which the file is uploaded.
  CloudProvider cloud_provider_;

  // Progress callback repeatedly run with progress updates.
  ProgressCallback progress_callback_;

  // Upload callback run once with upload success/failure.
  RenameCallback rename_callback_;

  base::WeakPtrFactory<SkyvaultRenameHandler> weak_factory_{this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_SKYVAULT_SKYVAULT_RENAME_HANDLER_H_
