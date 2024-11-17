// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_SKYVAULT_DRIVE_UPLOAD_OBSERVER_H_
#define CHROME_BROWSER_ASH_POLICY_SKYVAULT_DRIVE_UPLOAD_OBSERVER_H_

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/file_manager/io_task.h"
#include "chrome/browser/ash/file_manager/io_task_controller.h"
#include "chrome/browser/ash/policy/skyvault/policy_utils.h"
#include "chromeos/ash/components/drivefs/drivefs_host.h"
#include "storage/browser/file_system/file_system_context.h"

class Profile;

namespace ash::cloud_upload {

using policy::local_user_files::UploadTrigger;

// Observes the "upload to Google Drive" after the file is written to the local
// cache of Google Drive. Immediately uploads the file to Google Drive if the
// file is still queued. Instantiated by the static `Observe` method. Gets
// upload status by observing Drive events. Calls the `upload_callback_` when
// the file is successfully uploaded, which is when `DriveUploadObserver` goes
// out of scope. If the upload to Drive fails, it'll delete the file from the
// local cache.
class DriveUploadObserver
    : public base::RefCounted<DriveUploadObserver>,
      drivefs::DriveFsHost::Observer,
      drive::DriveIntegrationService::Observer,
      ::file_manager::io_task::IOTaskController::Observer {
 public:
  // Starts observing the upload of the file specified at construct time.
  static void Observe(Profile* profile,
                      base::FilePath file_path,
                      UploadTrigger trigger,
                      int64_t file_bytes,
                      base::RepeatingCallback<void(int64_t)> progress_callback,
                      base::OnceCallback<void(bool)> upload_callback);

  DriveUploadObserver(const DriveUploadObserver&) = delete;
  DriveUploadObserver& operator=(const DriveUploadObserver&) = delete;

  FRIEND_TEST_ALL_PREFIXES(DriveUploadObserverTest, NoSyncUpdates);
  FRIEND_TEST_ALL_PREFIXES(DriveUploadObserverTest, NoFileMetadata);

 private:
  friend base::RefCounted<DriveUploadObserver>;

  DriveUploadObserver(Profile* profile,
                      base::FilePath file_path,
                      UploadTrigger trigger,
                      int64_t file_bytes,
                      base::RepeatingCallback<void(int64_t)> progress_callback);
  ~DriveUploadObserver() override;

  void Run(base::OnceCallback<void(bool)> upload_callback);

  void OnEndUpload(bool success);

  // DriveFsHost::Observer implementation.
  void OnUnmounted() override;
  void OnSyncingStatusUpdate(
      const drivefs::mojom::SyncingStatus& status) override;
  void OnError(const drivefs::mojom::DriveError& error) override;

  // DriveIntegrationService::Observer implementation.
  void OnDriveConnectionStatusChanged(
      drive::util::ConnectionStatus status) override;

  // IOTaskController::Observer implementation.
  void OnIOTaskStatus(
      const ::file_manager::io_task::ProgressStatus& status) override;

  void OnImmediatelyUploadDone(int64_t bytes_transferred,
                               drive::FileError error);

  void StartNoSyncUpdateTimer();

  void NoSyncTimedOut();

  void OnGetDriveMetadata(drive::FileError error,
                          drivefs::mojom::FileMetadataPtr metadata);

  raw_ptr<Profile> profile_;
  scoped_refptr<storage::FileSystemContext> file_system_context_;
  const raw_ptr<drive::DriveIntegrationService> drive_integration_service_;

  // The Id of the delete task. It'll have a value only if the upload fails.
  std::optional<::file_manager::io_task::IOTaskId> observed_delete_task_id_ =
      std::nullopt;

  // The observed file local path.
  base::FilePath observed_local_path_;

  // The observed file Drive path.
  base::FilePath observed_drive_path_;

  // The size of the observed file.
  int64_t file_bytes_;

  // The event or action that initiated the file upload.
  const UploadTrigger trigger_;

  // Progress callback repeatedly run with progress updates.
  base::RepeatingCallback<void(int64_t)> progress_callback_;

  // Upload callback run once with upload success/failure.
  base::OnceCallback<void(bool)> upload_callback_;

  // If there's no sync updates received, the timer will timeout.
  base::OneShotTimer no_sync_update_timeout_;

  base::ScopedObservation<::file_manager::io_task::IOTaskController,
                          ::file_manager::io_task::IOTaskController::Observer>
      io_task_controller_observer_{this};

  base::WeakPtrFactory<DriveUploadObserver> weak_ptr_factory_{this};
};

}  // namespace ash::cloud_upload

#endif  // CHROME_BROWSER_ASH_POLICY_SKYVAULT_DRIVE_UPLOAD_OBSERVER_H_
