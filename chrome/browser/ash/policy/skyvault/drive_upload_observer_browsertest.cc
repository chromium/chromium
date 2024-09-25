// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/skyvault/drive_upload_observer.h"

#include "base/path_service.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/file_manager/io_task.h"
#include "chrome/browser/ash/policy/skyvault/policy_utils.h"
#include "chrome/browser/ash/policy/skyvault/skyvault_test_base.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/ash/components/drivefs/mojom/drivefs.mojom.h"
#include "components/drive/file_errors.h"
#include "content/public/test/browser_test.h"
#include "storage/browser/file_system/file_system_url.h"

using storage::FileSystemURL;

namespace ash::cloud_upload {

using ::base::test::RunOnceCallback;
using testing::_;

namespace {

const int64_t kFileSize = 123456;

const policy::local_user_files::UploadTrigger kTrigger =
    policy::local_user_files::UploadTrigger::kDownload;

}  // namespace

// Tests the Drive upload workflow using the static
// `DriveUploadObserver::Observe` method. Ensures that the upload completes with
// the expected results.
class DriveUploadObserverTest
    : public policy::local_user_files::SkyvaultGoogleDriveTest {
 public:
  DriveUploadObserverTest() = default;

  DriveUploadObserverTest(const DriveUploadObserverTest&) = delete;
  DriveUploadObserverTest& operator=(const DriveUploadObserverTest&) = delete;

  base::FilePath observed_relative_drive_path(const FileInfo& info) override {
    base::FilePath observed_relative_drive_path;
    drive_integration_service()->GetRelativeDrivePath(
        drive_root_dir().AppendASCII(info.test_file_name_),
        &observed_relative_drive_path);
    return observed_relative_drive_path;
  }

  // IOTaskController::Observer:
  void OnIOTaskStatus(
      const file_manager::io_task::ProgressStatus& status) override {
    // Get the source_file_path_ used in the map; status contains the value
    // after the sync.
    auto it = source_files_.find(
        drive_mount_point().Append(status.sources[0].url.path().BaseName()));
    if (status.type == file_manager::io_task::OperationType::kDelete &&
        status.sources.size() == 1 && it != source_files_.end() &&
        status.state == file_manager::io_task::State::kSuccess) {
      if (on_delete_callback_) {
        std::move(on_delete_callback_).Run();
      }
    }
  }

  void SimulateDriveUploadQueued(const FileInfo& info) {
    // Set file metadata for `drivefs::mojom::DriveFs::GetMetadata`.
    drivefs::FakeMetadata metadata;
    metadata.path = observed_relative_drive_path(info);
    metadata.original_name = info.test_file_name_;
    fake_drivefs().SetMetadata(std::move(metadata));

    // Simulate server sync events.
    drivefs::mojom::SyncingStatusPtr status =
        drivefs::mojom::SyncingStatus::New();
    status->item_events.emplace_back(
        std::in_place, 12, 34, observed_relative_drive_path(info).value(),
        drivefs::mojom::ItemEvent::State::kQueued, 0u, kFileSize,
        drivefs::mojom::ItemEventReason::kTransfer);
    drivefs_delegate()->OnSyncingStatusUpdate(status.Clone());
    drivefs_delegate().FlushForTesting();
  }

  void SimulateDriveUploadInProgress(int64_t bytes_transferred,
                                     const FileInfo& info) {
    // Set file metadata for `drivefs::mojom::DriveFs::GetMetadata`.
    drivefs::FakeMetadata metadata;
    metadata.path = observed_relative_drive_path(info);
    metadata.original_name = info.test_file_name_;
    fake_drivefs().SetMetadata(std::move(metadata));

    // Simulate server sync events.
    drivefs::mojom::SyncingStatusPtr status =
        drivefs::mojom::SyncingStatus::New();
    status->item_events.emplace_back(
        std::in_place, 12, 34, observed_relative_drive_path(info).value(),
        drivefs::mojom::ItemEvent::State::kInProgress, bytes_transferred,
        kFileSize, drivefs::mojom::ItemEventReason::kTransfer);
    drivefs_delegate()->OnSyncingStatusUpdate(status.Clone());
    drivefs_delegate().FlushForTesting();
  }

  void SimulateDriveUploadCompleted(const FileInfo& info) {
    // Set file metadata for `drivefs::mojom::DriveFs::GetMetadata`.
    drivefs::FakeMetadata metadata;
    metadata.path = observed_relative_drive_path(info);
    metadata.original_name = info.test_file_name_;
    fake_drivefs().SetMetadata(std::move(metadata));

    // Simulate server sync events.
    drivefs::mojom::SyncingStatusPtr status =
        drivefs::mojom::SyncingStatus::New();
    status->item_events.emplace_back(
        std::in_place, 12, 34, observed_relative_drive_path(info).value(),
        drivefs::mojom::ItemEvent::State::kCompleted, kFileSize, kFileSize,
        drivefs::mojom::ItemEventReason::kTransfer);
    drivefs_delegate()->OnSyncingStatusUpdate(status.Clone());
    drivefs_delegate().FlushForTesting();
  }

  void SimulateDriveUploadFailure(const FileInfo& info) {
    drivefs::mojom::SyncingStatusPtr fail_status =
        drivefs::mojom::SyncingStatus::New();
    fail_status->item_events.emplace_back(
        std::in_place, 12, 34, observed_relative_drive_path(info).value(),
        drivefs::mojom::ItemEvent::State::kFailed, 123, kFileSize,
        drivefs::mojom::ItemEventReason::kTransfer);
    drivefs_delegate()->OnSyncingStatusUpdate(fail_status->Clone());
    drivefs_delegate().FlushForTesting();
  }

 protected:
  base::OnceClosure on_delete_callback_;
};

// Send file queuing event, the file should be immediately uploaded.
IN_PROC_BROWSER_TEST_F(DriveUploadObserverTest, ImmediatelyUpload) {
  const std::string test_file_name = "archive.tar.gz";
  base::FilePath source_file_path =
      SetUpSourceFile(test_file_name, drive_mount_point());

  EXPECT_CALL(fake_drivefs(), ImmediatelyUpload)
      .WillOnce(RunOnceCallback<1>(drive::FileError::FILE_ERROR_OK));

  base::MockCallback<base::RepeatingCallback<void(int64_t)>> progress_callback;
  base::MockCallback<base::OnceCallback<void(bool)>> upload_callback;
  EXPECT_CALL(upload_callback, Run(/*success=*/true));
  DriveUploadObserver::Observe(
      profile(), drive_root_dir().AppendASCII(test_file_name), kTrigger,
      kFileSize, progress_callback.Get(), upload_callback.Get());

  auto it = source_files_.find(source_file_path);
  SimulateDriveUploadQueued(it->second);

  // Check that the source file has been moved to Drive.
  CheckPathExistsOnDrive(observed_relative_drive_path(it->second));
}

// Send progress updates then completed sync events.
IN_PROC_BROWSER_TEST_F(DriveUploadObserverTest, SuccessfulSync) {
  const std::string test_file_name = "image.webp";
  base::FilePath source_file_path =
      SetUpSourceFile(test_file_name, drive_mount_point());

  base::MockCallback<base::RepeatingCallback<void(int64_t)>> progress_callback;
  base::MockCallback<base::OnceCallback<void(bool)>> upload_callback;
  EXPECT_CALL(progress_callback, Run(/*bytes_so_far=*/kFileSize / 4));
  EXPECT_CALL(progress_callback, Run(/*bytes_so_far=*/kFileSize));
  EXPECT_CALL(upload_callback, Run(/*success=*/true));
  DriveUploadObserver::Observe(
      profile(), drive_root_dir().AppendASCII(test_file_name), kTrigger,
      kFileSize, progress_callback.Get(), upload_callback.Get());

  auto it = source_files_.find(source_file_path);
  SimulateDriveUploadInProgress(kFileSize / 4, it->second);
  SimulateDriveUploadCompleted(it->second);

  // Check that the source file has been moved to Drive.
  CheckPathExistsOnDrive(observed_relative_drive_path(it->second));
}

// Send syncing error event, the cached file should be deleted.
IN_PROC_BROWSER_TEST_F(DriveUploadObserverTest, ErrorSync) {
  const std::string test_file_name = "id3Audio.mp3";
  base::FilePath source_file_path =
      SetUpSourceFile(test_file_name, drive_mount_point());

  base::MockCallback<base::RepeatingCallback<void(int64_t)>> progress_callback;
  base::MockCallback<base::OnceCallback<void(bool)>> upload_callback;
  EXPECT_CALL(upload_callback, Run(/*success=*/false));
  DriveUploadObserver::Observe(
      profile(), drive_root_dir().AppendASCII(test_file_name), kTrigger,
      kFileSize, progress_callback.Get(), upload_callback.Get());

  SetUpObservers();

  auto future = base::test::TestFuture<void>();
  on_delete_callback_ = future.GetCallback();

  auto it = source_files_.find(source_file_path);
  SimulateDriveUploadFailure(it->second);
  EXPECT_TRUE(future.Wait());
}

// When the sync timer times out and no download url in the file metadata, the
// upload will fail and the file will be deleted from the cache.
IN_PROC_BROWSER_TEST_F(DriveUploadObserverTest, NoSyncUpdates) {
  const std::string test_file_name = "popup.pdf";
  base::FilePath source_file_path =
      SetUpSourceFile(test_file_name, drive_mount_point());

  base::MockCallback<base::RepeatingCallback<void(int64_t)>> progress_callback;
  base::MockCallback<base::OnceCallback<void(bool)>> upload_callback;
  scoped_refptr<DriveUploadObserver> drive_upload_observer =
      new DriveUploadObserver(profile(),
                              drive_root_dir().AppendASCII(test_file_name),
                              kTrigger, kFileSize, progress_callback.Get());
  drive_upload_observer->Run(upload_callback.Get());

  EXPECT_TRUE(drive_upload_observer->no_sync_update_timeout_.IsRunning());

  SetUpObservers();

  auto future = base::test::TestFuture<void>();
  on_delete_callback_ = future.GetCallback();

  EXPECT_CALL(upload_callback, Run(/*success=*/false));

  drivefs::FakeMetadata metadata;
  auto it = source_files_.find(source_file_path);
  metadata.path = observed_relative_drive_path(it->second);
  metadata.original_name = test_file_name;
  fake_drivefs().SetMetadata(std::move(metadata));

  drive_upload_observer->no_sync_update_timeout_.FireNow();

  base::RunLoop loop;
  loop.RunUntilIdle();
  EXPECT_TRUE(future.Wait());
}

// When the sync timer times out and no file metadata is returned, the upload
// will fail and the file will be deleted from the cache.
IN_PROC_BROWSER_TEST_F(DriveUploadObserverTest, NoFileMetadata) {
  const std::string test_file_name = "popup.pdf";
  base::FilePath source_file_path =
      SetUpSourceFile(test_file_name, drive_mount_point());

  base::MockCallback<base::RepeatingCallback<void(int64_t)>> progress_callback;
  base::MockCallback<base::OnceCallback<void(bool)>> upload_callback;
  scoped_refptr<DriveUploadObserver> drive_upload_observer =
      new DriveUploadObserver(profile(),
                              drive_root_dir().AppendASCII(test_file_name),
                              kTrigger, kFileSize, progress_callback.Get());
  drive_upload_observer->Run(upload_callback.Get());

  EXPECT_TRUE(drive_upload_observer->no_sync_update_timeout_.IsRunning());

  SetUpObservers();

  auto future = base::test::TestFuture<void>();
  on_delete_callback_ = future.GetCallback();

  EXPECT_CALL(upload_callback, Run(/*success=*/false));
  drive_upload_observer->no_sync_update_timeout_.FireNow();

  base::RunLoop loop;
  loop.RunUntilIdle();
  EXPECT_TRUE(future.Wait());
}

}  // namespace ash::cloud_upload
