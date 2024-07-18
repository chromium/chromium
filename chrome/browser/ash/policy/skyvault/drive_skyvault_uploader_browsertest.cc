// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/skyvault/drive_skyvault_uploader.h"

#include <memory>
#include <optional>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/drive/file_system_util.h"
#include "chrome/browser/ash/file_manager/file_manager_test_util.h"
#include "chrome/browser/ash/file_manager/io_task.h"
#include "chrome/browser/ash/file_manager/io_task_controller.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/ash/policy/skyvault/policy_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/ash/components/drivefs/mojom/drivefs.mojom.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/drive/file_errors.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/network_change_notifier.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "storage/browser/file_system/file_system_url.h"
#include "testing/gmock/include/gmock/gmock.h"

using storage::FileSystemURL;

namespace policy::local_user_files {

using ::base::test::RunOnceCallback;
using drive::DriveIntegrationService;
using drive::util::ConnectionStatus;
using drive::util::SetDriveConnectionStatusForTesting;
using testing::_;

namespace {

constexpr char kDestinationDir[] = "ChromeOS Device";

// Returns full test file path to the given `file_name`.
base::FilePath GetTestFilePath(const std::string& file_name) {
  // Get the path to file manager's test data directory.
  base::FilePath source_dir;
  CHECK(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &source_dir));
  base::FilePath test_data_dir = source_dir.AppendASCII("chrome")
                                     .AppendASCII("test")
                                     .AppendASCII("data")
                                     .AppendASCII("chromeos")
                                     .AppendASCII("file_manager");
  return test_data_dir.Append(base::FilePath::FromUTF8Unsafe(file_name));
}

}  // namespace

// Tests the Drive SkyVault upload workflow.
// TODO(b/349336220): Extract common testing code.
class DriveSkyvaultUploaderTest
    : public InProcessBrowserTest,
      public file_manager::io_task::IOTaskController::Observer {
 public:
  DriveSkyvaultUploaderTest() {
    feature_list_.InitAndEnableFeature(
        chromeos::features::kUploadOfficeToCloud);
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    drive_mount_point_ = temp_dir_.GetPath().Append("drivefs");
    drive_root_dir_ = drive_mount_point_.AppendASCII("root");
    my_files_dir_ = temp_dir_.GetPath().Append("myfiles");

    net::NetworkChangeNotifier::SetTestNotificationsOnly(true);
  }

  DriveSkyvaultUploaderTest(const DriveSkyvaultUploaderTest&) = delete;
  DriveSkyvaultUploaderTest& operator=(const DriveSkyvaultUploaderTest&) =
      delete;

  void SetUpInProcessBrowserTestFixture() override {
    // Setup drive integration service.
    create_drive_integration_service_ = base::BindRepeating(
        &DriveSkyvaultUploaderTest::CreateDriveIntegrationService,
        base::Unretained(this));
    service_factory_for_test_ = std::make_unique<
        drive::DriveIntegrationServiceFactory::ScopedFactoryForTest>(
        &create_drive_integration_service_);
  }

  void SetUpOnMainThread() override {
    SetDriveConnectionStatusForTesting(ConnectionStatus::kConnected);
    InProcessBrowserTest::SetUpOnMainThread();
  }

  void TearDown() override {
    InProcessBrowserTest::TearDown();
    storage::ExternalMountPoints::GetSystemInstance()->RevokeAllFileSystems();
  }

  void TearDownOnMainThread() override {
    RemoveObservers();
    InProcessBrowserTest::TearDownOnMainThread();
  }

  DriveIntegrationService* CreateDriveIntegrationService(Profile* profile) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    fake_drivefs_helpers_[profile] =
        std::make_unique<file_manager::test::FakeSimpleDriveFsHelper>(
            profile, drive_mount_point_);
    return new DriveIntegrationService(
        profile, "", drive_mount_point_,
        fake_drivefs_helpers_[profile]->CreateFakeDriveFsListenerFactory());
  }

  // Creates mount point for MyFiles and registers local filesystem.
  void SetUpMyFiles() {
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      ASSERT_TRUE(base::CreateDirectory(my_files_dir_));
    }
    std::string mount_point_name =
        file_manager::util::GetDownloadsMountPointName(profile());
    storage::ExternalMountPoints::GetSystemInstance()->RevokeFileSystem(
        mount_point_name);
    CHECK(storage::ExternalMountPoints::GetSystemInstance()->RegisterFileSystem(
        mount_point_name, storage::kFileSystemTypeLocal,
        storage::FileSystemMountOption(), my_files_dir_));
    file_manager::VolumeManager::Get(profile())
        ->RegisterDownloadsDirectoryForTesting(my_files_dir_);
  }

  void SetUpDrive() {
    // Create Drive root directory.
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      EXPECT_TRUE(base::CreateDirectory(drive_root_dir_));
    }
  }

  // Create and add a file with |test_file_name| to the file system
  // |source_path|. Return the created |source_file_url|.
  base::FilePath SetUpSourceFile(const std::string& test_file_name,
                                 base::FilePath source_path) {
    test_file_name_ = test_file_name;
    source_file_path_ = source_path.AppendASCII(test_file_name_);
    const base::FilePath test_file_path = GetTestFilePath(test_file_name_);
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      CHECK(base::CopyFile(test_file_path, source_file_path_));
    }

    // Check that the source file exists at the intended source location and is
    // not in Drive.
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      EXPECT_TRUE(base::PathExists(source_path.AppendASCII(test_file_name)));
      CheckPathNotFoundOnDrive(observed_relative_drive_path());
    }

    return source_file_path_;
  }

  void SetUpObservers() {
    // Subscribe to IOTasks updates to track the copy/move to Drive progress.
    file_manager::VolumeManager::Get(profile())
        ->io_task_controller()
        ->AddObserver(this);
  }

  void RemoveObservers() {
    file_manager::VolumeManager::Get(profile())
        ->io_task_controller()
        ->RemoveObserver(this);
  }

  // Resolves once the `OnUploadDone` callback is called with a valid URL, which
  // indicates the successful completion of the upload flow.
  void Wait() {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_FALSE(run_loop_);
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
    run_loop_ = nullptr;
  }

  void EndWait() {
    ASSERT_TRUE(run_loop_);
    run_loop_->Quit();
  }

  void CheckPathExistsOnDrive(const base::FilePath& path) {
    drive_integration_service()->GetDriveFsInterface()->GetMetadata(
        path,
        base::BindOnce(&DriveSkyvaultUploaderTest::OnGetMetadataExpectSuccess,
                       base::Unretained(this)));
    base::ScopedAllowBlockingForTesting allow_blocking;
    Wait();
  }

  void CheckPathNotFoundOnDrive(const base::FilePath& path) {
    drive_integration_service()->GetDriveFsInterface()->GetMetadata(
        path,
        base::BindOnce(&DriveSkyvaultUploaderTest::OnGetMetadataExpectNotFound,
                       base::Unretained(this)));
    base::ScopedAllowBlockingForTesting allow_blocking;
    Wait();
  }

  void OnGetMetadataExpectSuccess(drive::FileError error,
                                  drivefs::mojom::FileMetadataPtr metadata) {
    EXPECT_EQ(drive::FILE_ERROR_OK, error);
    EndWait();
  }

  void OnGetMetadataExpectNotFound(drive::FileError error,
                                   drivefs::mojom::FileMetadataPtr metadata) {
    EXPECT_EQ(drive::FILE_ERROR_NOT_FOUND, error);
    EndWait();
  }

  // `Wait` will not complete until this is called.
  void OnUploadDone(std::optional<MigrationUploadError> error) {
    if (fail_sync_) {
      ASSERT_EQ(error, MigrationUploadError::kCopyFailed);
    } else {
      ASSERT_FALSE(error.has_value());
    }
    EndWait();
  }

  Profile* profile() { return browser()->profile(); }

  const base::FilePath source_file_path() { return source_file_path_; }

  mojo::Remote<drivefs::mojom::DriveFsDelegate>& drivefs_delegate() {
    return fake_drivefs().delegate();
  }

  DriveIntegrationService* drive_integration_service() {
    return drive::DriveIntegrationServiceFactory::FindForProfile(profile());
  }

  base::FilePath observed_relative_drive_path() {
    base::FilePath observed_relative_drive_path;
    drive_integration_service()->GetRelativeDrivePath(
        drive_root_dir_.AppendASCII(test_file_name_),
        &observed_relative_drive_path);
    return observed_relative_drive_path;
  }

 protected:
  file_manager::test::FakeSimpleDriveFs& fake_drivefs() {
    return fake_drivefs_helpers_[profile()]->fake_drivefs();
  }

  base::FilePath my_files_dir_;
  base::FilePath drive_mount_point_;
  base::FilePath drive_root_dir_;

  bool add_metadata_ = true;
  bool fail_sync_ = false;
  // Overrides `fail_sync_`
  base::RepeatingClosure on_transfer_complete_callback_;

 private:
  // IOTaskController::Observer:
  void OnIOTaskStatus(
      const file_manager::io_task::ProgressStatus& status) override {
    // Wait for the copy task to complete before starting the Drive sync.
    if (status.type == file_manager::io_task::OperationType::kCopy &&
        status.sources.size() == 1 &&
        status.sources[0].url.path() == source_file_path_ &&
        status.state == file_manager::io_task::State::kSuccess) {
      if (on_transfer_complete_callback_) {
        on_transfer_complete_callback_.Run();
      } else if (fail_sync_) {
        SimulateDriveUploadFailure();
      } else {
        SimulateDriveUploadCompleted();
      }
    }
  }

  // Simulates the upload of the file to Drive by sending a series of fake
  // signals to the DriveFs delegate.
  void SimulateDriveUploadCompleted() {
    if (add_metadata_) {
      // Set file metadata for `drivefs::mojom::DriveFs::GetMetadata`.
      drivefs::FakeMetadata metadata;
      metadata.path = observed_relative_drive_path();
      metadata.mime_type =
          "application/"
          "vnd.openxmlformats-officedocument.wordprocessingml.document";
      metadata.original_name = test_file_name_;
      // metadata.doc_id = "abc123";
      metadata.alternate_url =
          "https://docs.google.com/document/d/"
          "smalldocxid?rtpof=true&usp=drive_fs";
      fake_drivefs().SetMetadata(std::move(metadata));
    }
    // Simulate server sync events.
    drivefs::mojom::SyncingStatusPtr status =
        drivefs::mojom::SyncingStatus::New();
    status->item_events.emplace_back(
        std::in_place, 12, 34, observed_relative_drive_path().value(),
        drivefs::mojom::ItemEvent::State::kQueued, 123, 456,
        drivefs::mojom::ItemEventReason::kTransfer);
    drivefs_delegate()->OnSyncingStatusUpdate(status.Clone());
    drivefs_delegate().FlushForTesting();

    status = drivefs::mojom::SyncingStatus::New();
    status->item_events.emplace_back(
        std::in_place, 12, 34, observed_relative_drive_path().value(),
        drivefs::mojom::ItemEvent::State::kCompleted, 123, 456,
        drivefs::mojom::ItemEventReason::kTransfer);
    drivefs_delegate()->OnSyncingStatusUpdate(status.Clone());
    drivefs_delegate().FlushForTesting();
  }

  void SimulateDriveUploadFailure() {
    // Simulate server sync events.
    drivefs::mojom::SyncingStatusPtr status =
        drivefs::mojom::SyncingStatus::New();
    status->item_events.emplace_back(
        std::in_place, 12, 34, observed_relative_drive_path().value(),
        drivefs::mojom::ItemEvent::State::kQueued, 123, 456,
        drivefs::mojom::ItemEventReason::kTransfer);
    drivefs_delegate()->OnSyncingStatusUpdate(status.Clone());
    drivefs_delegate().FlushForTesting();

    drivefs::mojom::SyncingStatusPtr fail_status =
        drivefs::mojom::SyncingStatus::New();
    fail_status->item_events.emplace_back(
        std::in_place, 12, 34, observed_relative_drive_path().value(),
        drivefs::mojom::ItemEvent::State::kFailed, 123, 456,
        drivefs::mojom::ItemEventReason::kTransfer);
    drivefs_delegate()->OnSyncingStatusUpdate(fail_status->Clone());
    drivefs_delegate().FlushForTesting();
  }

  base::test::ScopedFeatureList feature_list_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<base::RunLoop> run_loop_;

  drive::DriveIntegrationServiceFactory::FactoryCallback
      create_drive_integration_service_;
  std::unique_ptr<drive::DriveIntegrationServiceFactory::ScopedFactoryForTest>
      service_factory_for_test_;
  std::map<Profile*,
           std::unique_ptr<file_manager::test::FakeSimpleDriveFsHelper>>
      fake_drivefs_helpers_;

  // Used to track the upload progress during the tests.
  std::string test_file_name_;
  base::FilePath source_file_path_;
};

IN_PROC_BROWSER_TEST_F(DriveSkyvaultUploaderTest, SuccessfulUpload) {
  SetUpObservers();
  SetUpMyFiles();
  SetUpDrive();

  const std::string test_file_name = "text.docx";
  base::FilePath source_file = SetUpSourceFile(test_file_name, my_files_dir_);

  EXPECT_CALL(fake_drivefs(), ImmediatelyUpload)
      .WillOnce(RunOnceCallback<1>(drive::FileError::FILE_ERROR_OK));

  base::test::TestFuture<std::optional<MigrationUploadError>> future;
  auto drive_upload_handler = std::make_unique<DriveSkyvaultUploader>(
      profile(), source_file, base::FilePath(kDestinationDir),
      future.GetCallback());
  drive_upload_handler->Run();

  EXPECT_EQ(future.Get(), std::nullopt);

  // Check that the source file has been moved to Drive.
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_FALSE(base::PathExists(my_files_dir_.AppendASCII(test_file_name)));
    CheckPathExistsOnDrive(observed_relative_drive_path());
  }
}

// Test that when the sync to Drive fails, the file is not moved to Drive.
IN_PROC_BROWSER_TEST_F(DriveSkyvaultUploaderTest, FailedUpload) {
  fail_sync_ = true;
  SetUpObservers();
  SetUpMyFiles();
  SetUpDrive();

  const std::string test_file_name = "text.docx";
  base::FilePath source_file = SetUpSourceFile(test_file_name, my_files_dir_);

  EXPECT_CALL(fake_drivefs(), ImmediatelyUpload)
      .WillOnce(RunOnceCallback<1>(drive::FileError::FILE_ERROR_FAILED));

  base::test::TestFuture<std::optional<MigrationUploadError>> future;
  auto drive_upload_handler = std::make_unique<DriveSkyvaultUploader>(
      profile(), source_file, base::FilePath(kDestinationDir),
      future.GetCallback());
  drive_upload_handler->Run();

  EXPECT_EQ(future.Get(), MigrationUploadError::kCopyFailed);

  // Check that the source file has not been moved to Drive.
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_TRUE(base::PathExists(my_files_dir_.AppendASCII(test_file_name)));
    CheckPathNotFoundOnDrive(observed_relative_drive_path());
  }
}

IN_PROC_BROWSER_TEST_F(DriveSkyvaultUploaderTest, FailedDelete) {
  SetUpObservers();
  SetUpMyFiles();
  SetUpDrive();

  const std::string test_file_name = "text.docx";
  base::FilePath source_file = SetUpSourceFile(test_file_name, my_files_dir_);

  EXPECT_CALL(fake_drivefs(), ImmediatelyUpload)
      .WillOnce(RunOnceCallback<1>(drive::FileError::FILE_ERROR_OK));

  base::test::TestFuture<std::optional<MigrationUploadError>> future;
  auto drive_upload_handler = std::make_unique<DriveSkyvaultUploader>(
      profile(), source_file, base::FilePath(kDestinationDir),
      future.GetCallback());
  drive_upload_handler->SetFailDeleteForTesting(/*fail=*/true);
  drive_upload_handler->Run();

  EXPECT_EQ(future.Get(), MigrationUploadError::kDeleteFailed);

  // Check that the source file has been moved to Drive.
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_FALSE(base::PathExists(my_files_dir_.AppendASCII(test_file_name)));
    CheckPathExistsOnDrive(observed_relative_drive_path());
  }
}

// Test that when connection to Drive isn't available, the upload fails
// immediately.
IN_PROC_BROWSER_TEST_F(DriveSkyvaultUploaderTest, NoConnection) {
  SetUpObservers();
  SetUpMyFiles();
  SetUpDrive();
  SetDriveConnectionStatusForTesting(ConnectionStatus::kNoNetwork);

  const std::string test_file_name = "text.docx";
  base::FilePath source_file = SetUpSourceFile(test_file_name, my_files_dir_);

  EXPECT_CALL(fake_drivefs(), ImmediatelyUpload).Times(0);

  base::test::TestFuture<std::optional<MigrationUploadError>> future;
  auto drive_upload_handler = std::make_unique<DriveSkyvaultUploader>(
      profile(), source_file, base::FilePath(kDestinationDir),
      future.GetCallback());
  drive_upload_handler->Run();

  EXPECT_EQ(future.Get(), MigrationUploadError::kServiceUnavailable);

  // Check that the source file has not been moved to Drive.
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_TRUE(base::PathExists(my_files_dir_.AppendASCII(test_file_name)));
    CheckPathNotFoundOnDrive(observed_relative_drive_path());
  }
}

// Test that when connection to Drive fails during upload, the file is not moved
// to Drive.
IN_PROC_BROWSER_TEST_F(DriveSkyvaultUploaderTest, ConnectionLostDuringUpload) {
  SetUpObservers();
  SetUpMyFiles();
  SetUpDrive();

  const std::string test_file_name = "text.docx";
  base::FilePath source_file = SetUpSourceFile(test_file_name, my_files_dir_);

  on_transfer_complete_callback_ = base::BindLambdaForTesting([this] {
    SetDriveConnectionStatusForTesting(ConnectionStatus::kNoNetwork);
    drive_integration_service()->OnNetworkChanged();
  });

  base::test::TestFuture<std::optional<MigrationUploadError>> future;
  auto drive_upload_handler = std::make_unique<DriveSkyvaultUploader>(
      profile(), source_file, base::FilePath(kDestinationDir),
      future.GetCallback());
  drive_upload_handler->Run();

  EXPECT_EQ(future.Get(), MigrationUploadError::kServiceUnavailable);

  // Check that the source file has not been moved to Drive.
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_TRUE(base::PathExists(my_files_dir_.AppendASCII(test_file_name)));
    CheckPathNotFoundOnDrive(observed_relative_drive_path());
  }
}

}  // namespace policy::local_user_files
