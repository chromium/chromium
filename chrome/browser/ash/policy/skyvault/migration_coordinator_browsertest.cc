// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/skyvault/migration_coordinator.h"

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/ash/policy/skyvault/local_files_migration_constants.h"
#include "chrome/browser/ash/policy/skyvault/policy_utils.h"
#include "chrome/browser/ash/policy/skyvault/skyvault_test_base.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"

namespace policy::local_user_files {

using ::base::test::RunOnceCallback;

using OneDriveMigrationCoordinatorTest = SkyvaultOneDriveTest;

IN_PROC_BROWSER_TEST_F(OneDriveMigrationCoordinatorTest, SuccessfulUpload) {
  SetUpMyFiles();
  SetUpODFS();

  // Set up some files and directories.
  /**
  - MyFiles
    - foo
      - video_long.ogv
    - text.txt
  */
  const std::string file = "text.txt";
  base::FilePath file_path = CopyTestFile(file, my_files_dir());
  const std::string dir = "foo";
  base::FilePath dir_path = CreateTestDir(dir, my_files_dir());

  const std::string nested_file = "video_long.ogv";
  base::FilePath nested_file_path = CopyTestFile(nested_file, dir_path);

  MigrationCoordinator coordinator(profile());
  base::test::TestFuture<std::map<base::FilePath, MigrationUploadError>> future;
  // Upload the files.
  coordinator.Run(CloudProvider::kOneDrive, {file_path, nested_file_path},
                  kDestinationDirName, future.GetCallback());
  ASSERT_TRUE(future.Get().empty());

  // Check that all files have been moved to OneDrive in the correct place.
  CheckPathExistsOnODFS(
      base::FilePath("/").AppendASCII(kDestinationDirName).AppendASCII(file));
  CheckPathExistsOnODFS(base::FilePath("/")
                            .AppendASCII(kDestinationDirName)
                            .AppendASCII(dir)
                            .AppendASCII(nested_file));
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    CHECK(!base::PathExists(dir_path.AppendASCII(nested_file)));
    CHECK(!base::PathExists(file_path));
  }
}

IN_PROC_BROWSER_TEST_F(OneDriveMigrationCoordinatorTest,
                       FailedUpload_IOTaskError) {
  SetUpMyFiles();
  SetUpODFS();
  provided_file_system_->SetCreateFileError(
      base::File::Error::FILE_ERROR_NO_MEMORY);
  provided_file_system_->SetReauthenticationRequired(false);

  const std::string file = "video_long.ogv";
  base::FilePath file_path = CopyTestFile(file, my_files_dir());

  MigrationCoordinator coordinator(profile());
  base::test::TestFuture<std::map<base::FilePath, MigrationUploadError>> future;
  // Upload the file.
  coordinator.Run(CloudProvider::kOneDrive, {file_path}, kDestinationDirName,
                  future.GetCallback());
  auto errors = future.Get();
  ASSERT_TRUE(errors.size() == 1u);
  auto error = errors.find(file_path);
  ASSERT_NE(error, errors.end());
  ASSERT_EQ(error->second, MigrationUploadError::kCopyFailed);

  CheckPathNotFoundOnODFS(base::FilePath("/").AppendASCII(file));
}

IN_PROC_BROWSER_TEST_F(OneDriveMigrationCoordinatorTest, EmptyUrls) {
  SetUpMyFiles();
  SetUpODFS();

  MigrationCoordinator coordinator(profile());
  base::test::TestFuture<std::map<base::FilePath, MigrationUploadError>> future;
  coordinator.Run(CloudProvider::kOneDrive, {}, kDestinationDirName,
                  future.GetCallback());
  ASSERT_TRUE(future.Get().empty());
}

IN_PROC_BROWSER_TEST_F(OneDriveMigrationCoordinatorTest, StopUpload) {
  SetUpMyFiles();
  SetUpODFS();

  const std::string test_file_name = "video_long.ogv";
  base::FilePath file_path = CopyTestFile(test_file_name, my_files_dir());

  base::test::TestFuture<void> future;
  // Create directly for more control over Run() and Stop().
  OneDriveMigrationUploader uploader(profile(), {file_path},
                                     kDestinationDirName, base::DoNothing());
  // Ensure Run() doesn't finish before we call Stop().
  uploader.SetEmulateSlowForTesting(true);
  uploader.Run();
  uploader.Stop(future.GetCallback());
  ASSERT_TRUE(future.Wait());

  // Check that the source file has NOT been moved to OneDrive.
  CheckPathNotFoundOnODFS(base::FilePath("/").AppendASCII(test_file_name));
}

class GoogleDriveMigrationCoordinatorTest : public SkyvaultGoogleDriveTest {
 public:
  GoogleDriveMigrationCoordinatorTest() = default;

  GoogleDriveMigrationCoordinatorTest(
      const GoogleDriveMigrationCoordinatorTest&) = delete;
  GoogleDriveMigrationCoordinatorTest& operator=(
      const GoogleDriveMigrationCoordinatorTest&) = delete;

  base::FilePath observed_relative_drive_path(const FileInfo& info) override {
    base::FilePath observed_relative_drive_path;
    drive_integration_service()->GetRelativeDrivePath(
        drive_root_dir()
            .AppendASCII(kDestinationDirName)
            .Append(info.local_relative_path_),
        &observed_relative_drive_path);
    return observed_relative_drive_path;
  }

 protected:
  bool fail_sync_ = false;

 private:
  // IOTaskController::Observer:
  void OnIOTaskStatus(
      const file_manager::io_task::ProgressStatus& status) override {
    // Wait for the copy task to complete before starting the Drive sync.
    auto it = source_files_.find(status.sources[0].url.path());
    if (status.type == file_manager::io_task::OperationType::kCopy &&
        status.sources.size() == 1 && it != source_files_.end() &&
        status.state == file_manager::io_task::State::kSuccess) {
      if (fail_sync_) {
        SimulateDriveUploadFailure(it->second);
      } else {
        SimulateDriveUploadCompleted(it->second);
      }
    }
  }

  // Simulates the upload of the file to Drive by sending a series of fake
  // signals to the DriveFs delegate.
  void SimulateDriveUploadCompleted(const FileInfo& info) {
    // Set file metadata for `drivefs::mojom::DriveFs::GetMetadata`.
    drivefs::FakeMetadata metadata;
    metadata.path = observed_relative_drive_path(info);
    metadata.mime_type =
        "application/"
        "vnd.openxmlformats-officedocument.wordprocessingml.document";
    metadata.original_name = info.test_file_name_;
    metadata.alternate_url =
        "https://docs.google.com/document/d/"
        "smalldocxid?rtpof=true&usp=drive_fs";
    fake_drivefs().SetMetadata(std::move(metadata));
    // Simulate server sync events.
    drivefs::mojom::SyncingStatusPtr status =
        drivefs::mojom::SyncingStatus::New();
    status->item_events.emplace_back(
        std::in_place, 12, 34, observed_relative_drive_path(info).value(),
        drivefs::mojom::ItemEvent::State::kQueued, 123, 456,
        drivefs::mojom::ItemEventReason::kTransfer);
    drivefs_delegate()->OnSyncingStatusUpdate(status.Clone());
    drivefs_delegate().FlushForTesting();

    status = drivefs::mojom::SyncingStatus::New();
    status->item_events.emplace_back(
        std::in_place, 12, 34, observed_relative_drive_path(info).value(),
        drivefs::mojom::ItemEvent::State::kCompleted, 123, 456,
        drivefs::mojom::ItemEventReason::kTransfer);
    drivefs_delegate()->OnSyncingStatusUpdate(status.Clone());
    drivefs_delegate().FlushForTesting();
  }

  void SimulateDriveUploadFailure(const FileInfo& info) {
    // Simulate server sync events.
    drivefs::mojom::SyncingStatusPtr status =
        drivefs::mojom::SyncingStatus::New();
    status->item_events.emplace_back(
        std::in_place, 12, 34, observed_relative_drive_path(info).value(),
        drivefs::mojom::ItemEvent::State::kQueued, 123, 456,
        drivefs::mojom::ItemEventReason::kTransfer);
    drivefs_delegate()->OnSyncingStatusUpdate(status.Clone());
    drivefs_delegate().FlushForTesting();

    drivefs::mojom::SyncingStatusPtr fail_status =
        drivefs::mojom::SyncingStatus::New();
    fail_status->item_events.emplace_back(
        std::in_place, 12, 34, observed_relative_drive_path(info).value(),
        drivefs::mojom::ItemEvent::State::kFailed, 123, 456,
        drivefs::mojom::ItemEventReason::kTransfer);
    drivefs_delegate()->OnSyncingStatusUpdate(fail_status->Clone());
    drivefs_delegate().FlushForTesting();
  }
};

IN_PROC_BROWSER_TEST_F(GoogleDriveMigrationCoordinatorTest, SuccessfulUpload) {
  SetUpObservers();
  SetUpMyFiles();

  // Set up some files and directories.
  /**
  - MyFiles
    - foo
      - video_long.ogv
    - text.txt
  */
  // TODO(b/363480542): Uncomment when testing multi file syncs is supported.
  // const std::string file = "text.txt";
  // base::FilePath file_path = SetUpSourceFile(file, my_files_dir());
  const std::string dir = "foo";
  base::FilePath dir_path = CreateTestDir(dir, my_files_dir());

  const std::string nested_file = "video_long.ogv";
  base::FilePath nested_file_path = SetUpSourceFile(nested_file, dir_path);

  EXPECT_CALL(fake_drivefs(), ImmediatelyUpload)
      .WillOnce(
          base::test::RunOnceCallback<1>(drive::FileError::FILE_ERROR_OK));

  MigrationCoordinator coordinator(profile());
  base::test::TestFuture<std::map<base::FilePath, MigrationUploadError>> future;
  // Upload the files.
  coordinator.Run(CloudProvider::kGoogleDrive, {nested_file_path},
                  kDestinationDirName, future.GetCallback());
  ASSERT_TRUE(future.Get().empty());

  // Check that all files have been moved to Google Drive in the correct place.
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    // TODO(b/363480542): Uncomment when testing multi file syncs is supported.
    // EXPECT_FALSE(base::PathExists(my_files_dir().AppendASCII(file)));
    // CheckPathExistsOnDrive(
    //     observed_relative_drive_path(source_files_.find(file_path)->second));

    EXPECT_FALSE(base::PathExists(
        my_files_dir().AppendASCII(dir).AppendASCII(nested_file)));
    CheckPathExistsOnDrive(observed_relative_drive_path(
        source_files_.find(nested_file_path)->second));
  }
}

IN_PROC_BROWSER_TEST_F(GoogleDriveMigrationCoordinatorTest, FailedUpload) {
  SetUpObservers();
  SetUpMyFiles();
  fail_sync_ = true;

  const std::string file = "text.txt";
  base::FilePath file_path = SetUpSourceFile(file, my_files_dir());

  EXPECT_CALL(fake_drivefs(), ImmediatelyUpload)
      .WillOnce(
          base::test::RunOnceCallback<1>(drive::FileError::FILE_ERROR_FAILED));

  MigrationCoordinator coordinator(profile());
  base::test::TestFuture<std::map<base::FilePath, MigrationUploadError>> future;
  // Upload the files.
  coordinator.Run(CloudProvider::kGoogleDrive, {file_path}, kDestinationDirName,
                  future.GetCallback());
  auto errors = future.Get();
  ASSERT_TRUE(errors.size() == 1u);
  auto error = errors.find(file_path);
  ASSERT_NE(error, errors.end());
  ASSERT_EQ(error->second, MigrationUploadError::kCopyFailed);

  // Check that the file hasn't been moved to Google Drive.
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_TRUE(base::PathExists(my_files_dir().AppendASCII(file)));
    CheckPathNotFoundOnDrive(
        observed_relative_drive_path(source_files_.find(file_path)->second));
  }
}

}  // namespace policy::local_user_files
