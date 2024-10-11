// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_SKYVAULT_DRIVE_SKYVAULT_UPLOADER_H_
#define CHROME_BROWSER_ASH_POLICY_SKYVAULT_DRIVE_SKYVAULT_UPLOADER_H_

#include <optional>
#include <string>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/drive/file_system_util.h"
#include "chrome/browser/ash/file_manager/io_task_controller.h"
#include "chrome/browser/ash/policy/skyvault/policy_utils.h"
#include "chromeos/ash/components/drivefs/drivefs_host.h"
#include "chromeos/ash/components/drivefs/mojom/drivefs.mojom.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"

class Profile;

namespace policy::local_user_files {

// Manages the "upload to Drive" workflow as part of the SkyVault files
// migration. Starts with moving the file to the cloud. Gets upload status by
// observing move and Drive events. Calls the UploadCallback with once the
// upload is completed, passing the error if any occurred.
// TODO(b/353475473): Extract code shared with DriveUploadObserver, if possible.
class DriveSkyvaultUploader
    : public file_manager::io_task::IOTaskController::Observer,
      drivefs::DriveFsHost::Observer,
      drive::DriveIntegrationService::Observer {
 public:
  using UploadCallback =
      base::OnceCallback<void(std::optional<MigrationUploadError> error)>;

  DriveSkyvaultUploader(Profile* profile,
                        const base::FilePath& file_path,
                        const base::FilePath& target_path,
                        UploadCallback callback);
  ~DriveSkyvaultUploader() override;

  // Starts the upload workflow:
  // - Copy the file via an IO task.
  // - Sync to Drive.
  // - Remove the source file in case of a move operation. Move mode of the
  //   `CopyOrMoveIOTask` is not used because the source file should only be
  //   deleted at the end of the sync operation.
  // Initiated by the `Upload` static method.
  void Run();

  DriveSkyvaultUploader(const DriveSkyvaultUploader&) = delete;
  DriveSkyvaultUploader& operator=(const DriveSkyvaultUploader&) = delete;

  void SetFailDeleteForTesting(bool fail);

 private:
  // Starts the IOTask to upload the file to Google Drive to
  // `destination_folder_path`, if it was created successfully and fails the
  // operation otherwise.
  void CreateCopyIOTask(const base::FilePath& destination_folder_path,
                        bool created);

  // Called when copy to Drive completes. Cleans up files if needed, or
  // completes the operation immediately. Saves `error` so it's not overridden
  // if delete fails.
  void OnEndCopy(std::optional<MigrationUploadError> error = std::nullopt);

  // Called after unrecoverable error or when all tasks complete successfully.
  // Invokes the upload callback, passing the error if any occurred.
  void OnEndUpload();

  // Callback for when ImmediatelyUpload() is called on DriveFS.
  void ImmediatelyUploadDone(drive::FileError error);

  // Directs IO task status updates to |OnCopyStatus| or |OnDeleteStatus| based
  // on task id.
  void OnIOTaskStatus(
      const file_manager::io_task::ProgressStatus& status) override;

  // Observes copy to Drive IO task status updates. Calls `OnEndCopy` upon any
  // error.
  void OnCopyStatus(const file_manager::io_task::ProgressStatus& status);

  // Observes delete IO task status updates from the delete task for cleaning up
  // the source file. Calls `OnEndUpload` once the delete is finished.
  void OnDeleteStatus(const file_manager::io_task::ProgressStatus& status);

  // DriveFsHost::Observer implementation.
  void OnUnmounted() override;
  void OnSyncingStatusUpdate(
      const drivefs::mojom::SyncingStatus& status) override;
  void OnError(const drivefs::mojom::DriveError& error) override;

  // DriveIntegrationService::Observer implementation.
  void OnDriveConnectionStatusChanged(
      drive::util::ConnectionStatus status) override;

  // Test-only: Simulates a delete failure if true. Actual result of the
  // DeleteIO task is ignored.
  bool fail_delete_for_testing_ = false;

  const raw_ptr<Profile> profile_;
  scoped_refptr<storage::FileSystemContext> file_system_context_;
  const raw_ptr<drive::DriveIntegrationService> drive_integration_service_;

  // Upload details:
  const storage::FileSystemURL source_url_;  // Source file URL
  const base::FilePath target_path_;         // Target folder path on Drive
  UploadCallback callback_;                  // Invoked on completion

  // Tracks upload progress:
  std::optional<file_manager::io_task::IOTaskId> observed_copy_task_id_ =
      std::nullopt;
  std::optional<file_manager::io_task::IOTaskId> observed_delete_task_id_ =
      std::nullopt;
  base::FilePath observed_absolute_dest_path_;
  base::FilePath observed_relative_drive_path_;

  // Whether `EndCopy()` was called.
  bool copy_ended_ = false;

  // Stores the first encountered error, if any.
  std::optional<MigrationUploadError> error_;

  raw_ptr<file_manager::io_task::IOTaskController> io_task_controller_ =
      nullptr;
  base::ScopedObservation<file_manager::io_task::IOTaskController,
                          file_manager::io_task::IOTaskController::Observer>
      io_task_controller_observer_{this};

  base::WeakPtrFactory<DriveSkyvaultUploader> weak_ptr_factory_{this};
};

}  // namespace policy::local_user_files

#endif  // CHROME_BROWSER_ASH_POLICY_SKYVAULT_DRIVE_SKYVAULT_UPLOADER_H_
