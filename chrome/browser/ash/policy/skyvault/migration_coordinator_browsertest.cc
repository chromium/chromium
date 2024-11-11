// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/skyvault/migration_coordinator.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/path_service.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/ash/policy/skyvault/local_files_migration_constants.h"
#include "chrome/browser/ash/policy/skyvault/odfs_skyvault_uploader.h"
#include "chrome/browser/ash/policy/skyvault/policy_utils.h"
#include "chrome/browser/ash/policy/skyvault/skyvault_test_base.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "storage/browser/file_system/file_system_url.h"

namespace policy::local_user_files {

namespace {
class MockOdfsUploader : public ash::cloud_upload::OdfsMigrationUploader {
 public:
  static scoped_refptr<MockOdfsUploader> Create(
      Profile* profile,
      int64_t id,
      const storage::FileSystemURL& file_system_url,
      const base::FilePath& path,
      base::RepeatingClosure run_callback) {
    return new MockOdfsUploader(profile, id, file_system_url, path,
                                std::move(run_callback));
  }

  MOCK_METHOD(void,
              Run,
              (ash::cloud_upload::OdfsMigrationUploader::UploadDoneCallback),
              (override));

  MOCK_METHOD(void, Cancel, (), (override));

  base::WeakPtr<MockOdfsUploader> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  MockOdfsUploader(Profile* profile,
                   int64_t id,
                   const storage::FileSystemURL& file_system_url,
                   const base::FilePath& relative_source_path,
                   base::RepeatingClosure run_callback)
      : ash::cloud_upload::OdfsMigrationUploader(profile,
                                                 id,
                                                 file_system_url,
                                                 relative_source_path,
                                                 ""),
        run_callback_(std::move(run_callback)),
        file_system_url_(file_system_url) {
    ON_CALL(*this, Run).WillByDefault([this](UploadDoneCallback callback) {
      done_callback_ = std::move(callback);
      if (run_callback_) {
        run_callback_.Run();
      }
    });

    ON_CALL(*this, Cancel).WillByDefault([this]() {
      std::move(done_callback_)
          .Run(file_system_url_, MigrationUploadError::kCancelled,
               base::FilePath());
    });
  }

  ~MockOdfsUploader() override = default;
  // Called when Run() is invoked.
  base::RepeatingClosure run_callback_;

  UploadDoneCallback done_callback_;
  storage::FileSystemURL file_system_url_;

  base::WeakPtrFactory<MockOdfsUploader> weak_ptr_factory_{this};
};

}  // namespace

using ::base::test::RunOnceCallback;

class OneDriveMigrationCoordinatorTest : public SkyvaultOneDriveTest {
 public:
  OneDriveMigrationCoordinatorTest() = default;

  void SetUpOnMainThread() override {
    SkyvaultOneDriveTest::SetUpOnMainThread();

    CHECK(temp_dir_.CreateUniqueTempDir());
    error_log_path_ = temp_dir_.GetPath().AppendASCII(kErrorLogFileName);
  }

  OneDriveMigrationCoordinatorTest(const OneDriveMigrationCoordinatorTest&) =
      delete;
  OneDriveMigrationCoordinatorTest& operator=(
      const OneDriveMigrationCoordinatorTest&) = delete;

  ~OneDriveMigrationCoordinatorTest() override = default;

  void TearDown() override {
    odfs_uploader_ = nullptr;
    SkyvaultOneDriveTest::TearDown();
  }

 protected:
  void SetMockOdfsUploader(base::RepeatingClosure run_callback) {
    ash::cloud_upload::OdfsMigrationUploader::SetFactoryForTesting(
        base::BindRepeating(
            &OneDriveMigrationCoordinatorTest::CreateOdfsUploader,
            base::Unretained(this), run_callback));
  }

  // Creates a mock version of OdfsMigrationUploader and stores a pointer to it.
  // Note that if called multiple times (e.g. when the coordinator is uploading
  // multiple files), only the last pointer is stored.
  scoped_refptr<ash::cloud_upload::OdfsMigrationUploader> CreateOdfsUploader(
      base::RepeatingClosure run_callback,
      Profile* profile,
      int64_t id,
      const storage::FileSystemURL& file_system_url,
      const base::FilePath& path) {
    scoped_refptr<MockOdfsUploader> uploader = MockOdfsUploader::Create(
        profile, id, file_system_url, path, std::move(run_callback));
    odfs_uploader_ = uploader->GetWeakPtr();
    // Whenever an uploader is created, its Run method is immediately called:
    EXPECT_CALL(*odfs_uploader_, Run).Times(1);
    return std::move(uploader);
  }

  base::ScopedTempDir temp_dir_;
  // Expected error log path.
  base::FilePath error_log_path_;
  // Local pointer to the instance created by the factory method. Should be used
  // for single file uploads, as only the last created pointer is stored.
  // The lifetime is managed by the Upload method after which the instance is
  // destroyed.
  base::WeakPtr<MockOdfsUploader> odfs_uploader_ = nullptr;
};

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
  coordinator.SetErrorLogPathForTesting(error_log_path_);
  base::test::TestFuture<std::map<base::FilePath, MigrationUploadError>,
                         base::FilePath, base::FilePath>
      future;
  // Upload the files.
  coordinator.Run(CloudProvider::kOneDrive, {file_path, nested_file_path},
                  kUploadRootPrefix, future.GetCallback());
  auto [errors, upload_root_path, log_error_path] = future.Get();
  ASSERT_TRUE(errors.empty());
  EXPECT_EQ(ash::cloud_upload::GetODFS(profile())
                ->GetFileSystemInfo()
                .mount_path()
                .Append(kUploadRootPrefix),
            upload_root_path);

  // Check that all files have been moved to OneDrive in the correct place.
  CheckPathExistsOnODFS(
      base::FilePath("/").AppendASCII(kUploadRootPrefix).AppendASCII(file));
  CheckPathExistsOnODFS(base::FilePath("/")
                            .AppendASCII(kUploadRootPrefix)
                            .AppendASCII(dir)
                            .AppendASCII(nested_file));
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    CHECK(!base::PathExists(dir_path.AppendASCII(nested_file)));
    CHECK(!base::PathExists(file_path));

    ASSERT_TRUE(base::PathExists(error_log_path_));
    std::string error_log;
    ASSERT_TRUE(base::ReadFileToString(error_log_path_, &error_log));
    EXPECT_EQ("", error_log);
  }
}

IN_PROC_BROWSER_TEST_F(OneDriveMigrationCoordinatorTest,
                       FailedUpload_IOTaskError) {
  SetUpMyFiles();
  SetUpODFS();
  provided_file_system_->SetCreateFileError(
      base::File::Error::FILE_ERROR_NO_SPACE);
  provided_file_system_->SetReauthenticationRequired(false);

  const std::string file = "video_long.ogv";
  base::FilePath file_path = CopyTestFile(file, my_files_dir());

  MigrationCoordinator coordinator(profile());
  coordinator.SetErrorLogPathForTesting(error_log_path_);
  base::test::TestFuture<std::map<base::FilePath, MigrationUploadError>,
                         base::FilePath, base::FilePath>
      future;
  // Upload the file.
  coordinator.Run(CloudProvider::kOneDrive, {file_path}, kUploadRootPrefix,
                  future.GetCallback());
  auto errors = future.Get<0>();
  ASSERT_TRUE(errors.size() == 1u);
  auto error = errors.find(file_path);
  ASSERT_NE(error, errors.end());
  ASSERT_EQ(error->second, MigrationUploadError::kCloudQuotaFull);

  CheckPathNotFoundOnODFS(base::FilePath("/").AppendASCII(file));

  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(base::PathExists(error_log_path_));
    std::string error_log;
    ASSERT_TRUE(base::ReadFileToString(error_log_path_, &error_log));
    EXPECT_EQ(absl::StrFormat("%s - %s\n", file_path.AsUTF8Unsafe(),
                              "Free up space in OneDrive to move this file"),
              error_log);
  }
}

IN_PROC_BROWSER_TEST_F(OneDriveMigrationCoordinatorTest, EmptyUrls) {
  SetUpMyFiles();
  SetUpODFS();

  MigrationCoordinator coordinator(profile());
  coordinator.SetErrorLogPathForTesting(error_log_path_);
  base::test::TestFuture<std::map<base::FilePath, MigrationUploadError>,
                         base::FilePath, base::FilePath>
      future;
  coordinator.Run(CloudProvider::kOneDrive, {}, kUploadRootPrefix,
                  future.GetCallback());
  auto [errors, upload_root_path, log_error_path] = future.Get();
  ASSERT_TRUE(errors.empty());
  // The path won't get populated if no upload is triggered.
  EXPECT_EQ(base::FilePath(), upload_root_path);

  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    // Log file isn't created if no uploads are needed.
    EXPECT_FALSE(base::PathExists(error_log_path_));
  }
}

IN_PROC_BROWSER_TEST_F(OneDriveMigrationCoordinatorTest, CancelUpload) {
  SetUpMyFiles();
  SetUpODFS();
  // Ensure Run() is called before cancelling
  base::test::TestFuture<void> run_future;
  SetMockOdfsUploader(run_future.GetRepeatingCallback());

  const std::string test_file_name = "video_long.ogv";
  base::FilePath file_path = CopyTestFile(test_file_name, my_files_dir());

  MigrationCoordinator coordinator(profile());
  coordinator.SetErrorLogPathForTesting(error_log_path_);
  coordinator.Run(CloudProvider::kOneDrive, {file_path}, kUploadRootPrefix,
                  base::DoNothing());

  // The uploader is only created during the Run call. At this point, its Run
  // method has also already been called
  ASSERT_TRUE(run_future.Wait());
  EXPECT_TRUE(odfs_uploader_);
  EXPECT_CALL(*odfs_uploader_, Cancel).Times(1);
  coordinator.Cancel(base::DoNothing());

  // Check that the source file has NOT been moved to OneDrive.
  CheckPathNotFoundOnODFS(base::FilePath("/").AppendASCII(test_file_name));

  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    // File is deleted on cancellation.
    EXPECT_FALSE(base::PathExists(error_log_path_));
  }
}

class GoogleDriveMigrationCoordinatorTest : public SkyvaultGoogleDriveTest {
 public:
  // Possible final states of the file sync
  enum class SyncStatus {
    kCompleted,
    kFailure,
  };
  GoogleDriveMigrationCoordinatorTest() = default;

  GoogleDriveMigrationCoordinatorTest(
      const GoogleDriveMigrationCoordinatorTest&) = delete;
  GoogleDriveMigrationCoordinatorTest& operator=(
      const GoogleDriveMigrationCoordinatorTest&) = delete;

  void SetUpOnMainThread() override {
    SkyvaultGoogleDriveTest::SetUpOnMainThread();

    CHECK(temp_dir_.CreateUniqueTempDir());
    error_log_path_ = temp_dir_.GetPath().AppendASCII(kErrorLogFileName);
  }

  base::FilePath observed_relative_drive_path(const FileInfo& info) override {
    base::FilePath observed_relative_drive_path;
    drive_integration_service()->GetRelativeDrivePath(
        drive_root_dir()
            .AppendASCII(kUploadRootPrefix)
            .Append(info.local_relative_path_),
        &observed_relative_drive_path);
    return observed_relative_drive_path;
  }

 protected:
  SyncStatus sync_status_ = SyncStatus::kCompleted;
  // Invoked when the copy is in progress.
  base::RepeatingClosure on_transfer_in_progress_callback_;
  base::ScopedTempDir temp_dir_;
  // Expected error log path.
  base::FilePath error_log_path_;

 private:
  // IOTaskController::Observer:
  void OnIOTaskStatus(
      const file_manager::io_task::ProgressStatus& status) override {
    auto it = source_files_.find(status.sources[0].url.path());
    if (status.type != file_manager::io_task::OperationType::kCopy ||
        status.sources.size() != 1 || it == source_files_.end()) {
      return;
    }

    // Invoke in progress callback if needed.
    if (status.state == file_manager::io_task::State::kQueued ||
        status.state == file_manager::io_task::State::kInProgress) {
      if (on_transfer_in_progress_callback_) {
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, std::move(on_transfer_in_progress_callback_));
        return;
      }
    }
    // Wait for the copy task to complete before starting the Drive sync.
    if (status.state == file_manager::io_task::State::kSuccess) {
      switch (sync_status_) {
        case SyncStatus::kCompleted:
          SimulateDriveUploadCompleted(it->second);
          break;
        case SyncStatus::kFailure:
          SimulateDriveUploadFailure(it->second);
          break;
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

  // Simulates a failed upload of the file to Drive by sending a series of fake
  // signals to the DriveFs delegate.
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
  coordinator.SetErrorLogPathForTesting(error_log_path_);
  base::test::TestFuture<std::map<base::FilePath, MigrationUploadError>,
                         base::FilePath, base::FilePath>
      future;
  // Upload the files.
  coordinator.Run(CloudProvider::kGoogleDrive, {nested_file_path},
                  kUploadRootPrefix, future.GetCallback());

  auto [errors, upload_root_path, log_error_path] = future.Get();
  ASSERT_TRUE(errors.empty());
  EXPECT_EQ(drive_root_dir().Append(kUploadRootPrefix), upload_root_path);

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

    ASSERT_TRUE(base::PathExists(error_log_path_));
    std::string error_log;
    ASSERT_TRUE(base::ReadFileToString(error_log_path_, &error_log));
    EXPECT_EQ("", error_log);
  }
}

IN_PROC_BROWSER_TEST_F(GoogleDriveMigrationCoordinatorTest, FailedUpload) {
  SetUpObservers();
  SetUpMyFiles();
  sync_status_ = SyncStatus::kFailure;

  const std::string file = "text.txt";
  base::FilePath file_path = SetUpSourceFile(file, my_files_dir());

  EXPECT_CALL(fake_drivefs(), ImmediatelyUpload)
      .WillOnce(
          base::test::RunOnceCallback<1>(drive::FileError::FILE_ERROR_FAILED));

  MigrationCoordinator coordinator(profile());
  coordinator.SetErrorLogPathForTesting(error_log_path_);
  base::test::TestFuture<std::map<base::FilePath, MigrationUploadError>,
                         base::FilePath, base::FilePath>
      future;
  // Upload the files.
  coordinator.Run(CloudProvider::kGoogleDrive, {file_path}, kUploadRootPrefix,
                  future.GetCallback());
  auto [errors, upload_root_path, log_error_path] = future.Get();
  ASSERT_EQ(1u, errors.size());
  auto error = errors.find(file_path);
  ASSERT_NE(errors.end(), error);
  ASSERT_EQ(MigrationUploadError::kSyncFailed, error->second);
  // The path should be populated by the time sync starts.
  EXPECT_EQ(drive_root_dir().Append(kUploadRootPrefix), upload_root_path);

  // Check that the file hasn't been moved to Google Drive.
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_TRUE(base::PathExists(my_files_dir().AppendASCII(file)));
    CheckPathNotFoundOnDrive(
        observed_relative_drive_path(source_files_.find(file_path)->second));

    ASSERT_TRUE(base::PathExists(error_log_path_));
    std::string error_log;
    ASSERT_TRUE(base::ReadFileToString(error_log_path_, &error_log));
    EXPECT_EQ(absl::StrFormat("%s - %s\n", file_path.AsUTF8Unsafe(),
                              "Something went wrong. Try again."),
              error_log);
  }
}

IN_PROC_BROWSER_TEST_F(GoogleDriveMigrationCoordinatorTest, CancelUpload) {
  SetUpObservers();
  SetUpMyFiles();

  const std::string file = "video_long.ogv";
  base::FilePath file_path = SetUpSourceFile(file, my_files_dir());

  MigrationCoordinator coordinator(profile());
  coordinator.SetErrorLogPathForTesting(error_log_path_);
  base::RunLoop run_loop;
  coordinator.SetCancelledCallbackForTesting(
      base::BindLambdaForTesting([&run_loop]() { run_loop.Quit(); }));

  on_transfer_in_progress_callback_ = base::BindLambdaForTesting(
      [&coordinator] { coordinator.Cancel(base::DoNothing()); });
  coordinator.Run(CloudProvider::kGoogleDrive, {file_path}, kUploadRootPrefix,
                  base::DoNothing());
  run_loop.Run();

  // Check that the file hasn't been moved to Google Drive.
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_TRUE(base::PathExists(my_files_dir().AppendASCII(file)));
    CheckPathNotFoundOnDrive(
        observed_relative_drive_path(source_files_.find(file_path)->second));

    // File is deleted on cancellation.
    EXPECT_FALSE(base::PathExists(error_log_path_));
  }
}

}  // namespace policy::local_user_files
