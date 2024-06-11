// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/skyvault/drive_upload_observer.h"

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/file_manager/file_manager_test_util.h"
#include "chrome/browser/ash/file_manager/io_task.h"
#include "chrome/browser/ash/file_manager/io_task_controller.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/ash/components/drivefs/mojom/drivefs.mojom.h"
#include "components/drive/file_errors.h"
#include "content/public/test/browser_test.h"
#include "storage/browser/file_system/external_mount_points.h"

using storage::FileSystemURL;

namespace ash::cloud_upload {

using ::base::test::RunOnceCallback;
using drive::DriveIntegrationService;
using drive::util::ConnectionStatus;
using drive::util::SetDriveConnectionStatusForTesting;
using testing::_;

namespace {

const int64_t kFileSize = 123456;

// Returns full test file path to the given |file_name|.
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

// Tests the Drive upload workflow using the static
// `DriveUploadObserver::Observe` method. Ensures that the upload completes with
// the expected results.
class DriveUploadObserverTest
    : public InProcessBrowserTest,
      ::file_manager::io_task::IOTaskController::Observer {
 public:
  DriveUploadObserverTest() {
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    drive_mount_point_ = temp_dir_.GetPath().Append("drivefs");
    drive_root_dir_ = drive_mount_point_.AppendASCII("root");
  }

  DriveUploadObserverTest(const DriveUploadObserverTest&) = delete;
  DriveUploadObserverTest& operator=(const DriveUploadObserverTest&) = delete;

  void SetUpInProcessBrowserTestFixture() override {
    // Setup drive integration service.
    create_drive_integration_service_ = base::BindRepeating(
        &DriveUploadObserverTest::CreateDriveIntegrationService,
        base::Unretained(this));
    service_factory_for_test_ = std::make_unique<
        drive::DriveIntegrationServiceFactory::ScopedFactoryForTest>(
        &create_drive_integration_service_);
  }

  void SetUpOnMainThread() override {
    SetDriveConnectionStatusForTesting(ConnectionStatus::kConnected);
    InProcessBrowserTest::SetUpOnMainThread();
  }

  void SetUp() override {
    InProcessBrowserTest::SetUp();
    // Create Drive root directory.
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      EXPECT_TRUE(base::CreateDirectory(drive_root_dir_));
    }
  }

  void TearDown() override {
    InProcessBrowserTest::TearDown();
    storage::ExternalMountPoints::GetSystemInstance()->RevokeAllFileSystems();
  }

  void TearDownOnMainThread() override {
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

  // Create and add a file with |test_file_name| to the file system
  // |source_path|. Returns the created file path.
  base::FilePath SetUpSourceFile(const std::string& test_file_name,
                                 base::FilePath source_path) {
    test_file_name_ = test_file_name;
    auto source_file_path = source_path.AppendASCII(test_file_name_);
    const base::FilePath test_file_path = GetTestFilePath(test_file_name_);
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      CHECK(base::CopyFile(test_file_path, source_file_path));
    }

    // Check that the source file exists at the intended source location and is
    // not in Drive.
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      EXPECT_TRUE(base::PathExists(source_path.AppendASCII(test_file_name)));
    }

    return test_file_path;
  }

  void CheckPathExistsOnDrive(const base::FilePath& path) {
    base::test::TestFuture<drive::FileError, drivefs::mojom::FileMetadataPtr>
        future;
    drive_integration_service()->GetDriveFsInterface()->GetMetadata(
        path, base::BindOnce(future.GetCallback()));
    EXPECT_EQ(drive::FILE_ERROR_OK, future.Get<drive::FileError>());
  }

  void CheckPathNotFoundOnDrive(const base::FilePath& path) {
    base::test::TestFuture<drive::FileError, drivefs::mojom::FileMetadataPtr>
        future;
    drive_integration_service()->GetDriveFsInterface()->GetMetadata(
        path, base::BindOnce(future.GetCallback()));
    EXPECT_EQ(drive::FILE_ERROR_NOT_FOUND, future.Get<drive::FileError>());
  }

  Profile* profile() { return browser()->profile(); }

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

  file_manager::test::FakeSimpleDriveFs& fake_drivefs() {
    return fake_drivefs_helpers_[profile()]->fake_drivefs();
  }

  void SetUpObservers() {
    // Subscribe to IOTasks updates to track the delete progressprogress.
    file_manager::VolumeManager::Get(profile())
        ->io_task_controller()
        ->AddObserver(this);
  }

  void RemoveObservers() {
    file_manager::VolumeManager::Get(profile())
        ->io_task_controller()
        ->RemoveObserver(this);
  }

  // IOTaskController::Observer:
  void OnIOTaskStatus(
      const file_manager::io_task::ProgressStatus& status) override {
    if (status.type == file_manager::io_task::OperationType::kDelete &&
        status.sources.size() == 1 &&
        status.sources[0].url.path() ==
            drive_root_dir_.AppendASCII(test_file_name_) &&
        status.state == file_manager::io_task::State::kSuccess) {
      if (on_delete_callback_) {
        std::move(on_delete_callback_).Run();
      }
    }
  }

  void SimulateDriveUploadQueued() {
    // Set file metadata for `drivefs::mojom::DriveFs::GetMetadata`.
    drivefs::FakeMetadata metadata;
    metadata.path = observed_relative_drive_path();
    metadata.original_name = test_file_name_;
    fake_drivefs().SetMetadata(std::move(metadata));

    // Simulate server sync events.
    drivefs::mojom::SyncingStatusPtr status =
        drivefs::mojom::SyncingStatus::New();
    status->item_events.emplace_back(
        std::in_place, 12, 34, observed_relative_drive_path().value(),
        drivefs::mojom::ItemEvent::State::kQueued, 0u, kFileSize,
        drivefs::mojom::ItemEventReason::kTransfer);
    drivefs_delegate()->OnSyncingStatusUpdate(status.Clone());
    drivefs_delegate().FlushForTesting();
  }

  void SimulateDriveUploadInProgress(int64_t bytes_transferred) {
    // Set file metadata for `drivefs::mojom::DriveFs::GetMetadata`.
    drivefs::FakeMetadata metadata;
    metadata.path = observed_relative_drive_path();
    metadata.original_name = test_file_name_;
    fake_drivefs().SetMetadata(std::move(metadata));

    // Simulate server sync events.
    drivefs::mojom::SyncingStatusPtr status =
        drivefs::mojom::SyncingStatus::New();
    status->item_events.emplace_back(
        std::in_place, 12, 34, observed_relative_drive_path().value(),
        drivefs::mojom::ItemEvent::State::kInProgress, bytes_transferred,
        kFileSize, drivefs::mojom::ItemEventReason::kTransfer);
    drivefs_delegate()->OnSyncingStatusUpdate(status.Clone());
    drivefs_delegate().FlushForTesting();
  }

  void SimulateDriveUploadCompleted() {
    // Set file metadata for `drivefs::mojom::DriveFs::GetMetadata`.
    drivefs::FakeMetadata metadata;
    metadata.path = observed_relative_drive_path();
    metadata.original_name = test_file_name_;
    fake_drivefs().SetMetadata(std::move(metadata));

    // Simulate server sync events.
    drivefs::mojom::SyncingStatusPtr status =
        drivefs::mojom::SyncingStatus::New();
    status->item_events.emplace_back(
        std::in_place, 12, 34, observed_relative_drive_path().value(),
        drivefs::mojom::ItemEvent::State::kCompleted, kFileSize, kFileSize,
        drivefs::mojom::ItemEventReason::kTransfer);
    drivefs_delegate()->OnSyncingStatusUpdate(status.Clone());
    drivefs_delegate().FlushForTesting();
  }

  void SimulateDriveUploadFailure() {
    drivefs::mojom::SyncingStatusPtr fail_status =
        drivefs::mojom::SyncingStatus::New();
    fail_status->item_events.emplace_back(
        std::in_place, 12, 34, observed_relative_drive_path().value(),
        drivefs::mojom::ItemEvent::State::kFailed, 123, kFileSize,
        drivefs::mojom::ItemEventReason::kTransfer);
    drivefs_delegate()->OnSyncingStatusUpdate(fail_status->Clone());
    drivefs_delegate().FlushForTesting();
  }

 protected:
  base::FilePath drive_mount_point_;
  base::FilePath drive_root_dir_;
  base::ScopedTempDir temp_dir_;

  base::OnceClosure on_delete_callback_;

  drive::DriveIntegrationServiceFactory::FactoryCallback
      create_drive_integration_service_;
  std::unique_ptr<drive::DriveIntegrationServiceFactory::ScopedFactoryForTest>
      service_factory_for_test_;
  std::map<Profile*,
           std::unique_ptr<file_manager::test::FakeSimpleDriveFsHelper>>
      fake_drivefs_helpers_;

  // Used to track the upload progress during the tests.
  std::string test_file_name_;
};

// Send file queuing event, the file should be immediately uploaded.
IN_PROC_BROWSER_TEST_F(DriveUploadObserverTest, ImmediatelyUpload) {
  const std::string test_file_name = "archive.tar.gz";
  base::FilePath source_file_path =
      SetUpSourceFile(test_file_name, drive_mount_point_);
  base::FilePath observed_relative_drive_path;
  drive_integration_service()->GetRelativeDrivePath(
      drive_root_dir_.AppendASCII(test_file_name_),
      &observed_relative_drive_path);

  EXPECT_CALL(fake_drivefs(), ImmediatelyUpload(_, _))
      .WillOnce(RunOnceCallback<1>(drive::FileError::FILE_ERROR_OK));

  base::MockCallback<base::RepeatingCallback<void(int64_t)>> progress_callback;
  base::MockCallback<base::OnceCallback<void(bool)>> upload_callback;
  EXPECT_CALL(upload_callback, Run(/*success=*/true));
  DriveUploadObserver::Observe(
      profile(), drive_root_dir_.AppendASCII(test_file_name_), kFileSize,
      progress_callback.Get(), upload_callback.Get());

  SimulateDriveUploadQueued();

  // Check that the source file has been moved to Drive.
  CheckPathExistsOnDrive(observed_relative_drive_path);
}

// Send progress updates then completed sync events.
IN_PROC_BROWSER_TEST_F(DriveUploadObserverTest, SuccessfulSync) {
  const std::string test_file_name = "image.webp";
  base::FilePath source_file_path =
      SetUpSourceFile(test_file_name, drive_mount_point_);
  base::FilePath observed_relative_drive_path;
  drive_integration_service()->GetRelativeDrivePath(
      drive_root_dir_.AppendASCII(test_file_name_),
      &observed_relative_drive_path);

  base::MockCallback<base::RepeatingCallback<void(int64_t)>> progress_callback;
  base::MockCallback<base::OnceCallback<void(bool)>> upload_callback;
  EXPECT_CALL(progress_callback, Run(/*bytes_so_far=*/kFileSize / 4));
  EXPECT_CALL(progress_callback, Run(/*bytes_so_far=*/kFileSize));
  EXPECT_CALL(upload_callback, Run(/*success=*/true));
  DriveUploadObserver::Observe(
      profile(), drive_root_dir_.AppendASCII(test_file_name_), kFileSize,
      progress_callback.Get(), upload_callback.Get());

  SimulateDriveUploadInProgress(kFileSize / 4);
  SimulateDriveUploadCompleted();

  // Check that the source file has been moved to Drive.
  CheckPathExistsOnDrive(observed_relative_drive_path);
}

// Send syncing error event, the cached file should be deleted.
IN_PROC_BROWSER_TEST_F(DriveUploadObserverTest, ErrorSync) {
  const std::string test_file_name = "id3Audio.mp3";
  base::FilePath source_file_path =
      SetUpSourceFile(test_file_name, drive_mount_point_);

  base::MockCallback<base::RepeatingCallback<void(int64_t)>> progress_callback;
  base::MockCallback<base::OnceCallback<void(bool)>> upload_callback;
  EXPECT_CALL(upload_callback, Run(/*success=*/false));
  DriveUploadObserver::Observe(
      profile(), drive_root_dir_.AppendASCII(test_file_name_), kFileSize,
      progress_callback.Get(), upload_callback.Get());

  SetUpObservers();

  auto future = base::test::TestFuture<void>();
  on_delete_callback_ = future.GetCallback();
  SimulateDriveUploadFailure();
  EXPECT_TRUE(future.Wait());

  RemoveObservers();
}

// When the sync timer times out and no download url in the file metadata, the
// upload will fail and the file will be deleted from the cache.
IN_PROC_BROWSER_TEST_F(DriveUploadObserverTest, NoSyncUpdates) {
  const std::string test_file_name = "popup.pdf";
  base::FilePath source_file_path =
      SetUpSourceFile(test_file_name, drive_mount_point_);

  base::MockCallback<base::RepeatingCallback<void(int64_t)>> progress_callback;
  base::MockCallback<base::OnceCallback<void(bool)>> upload_callback;
  scoped_refptr<DriveUploadObserver> drive_upload_observer =
      new DriveUploadObserver(profile(),
                              drive_root_dir_.AppendASCII(test_file_name_),
                              kFileSize, progress_callback.Get());
  drive_upload_observer->Run(upload_callback.Get());

  EXPECT_TRUE(drive_upload_observer->no_sync_update_timeout_.IsRunning());

  SetUpObservers();

  auto future = base::test::TestFuture<void>();
  on_delete_callback_ = future.GetCallback();

  EXPECT_CALL(upload_callback, Run(/*success=*/false));

  drivefs::FakeMetadata metadata;
  metadata.path = observed_relative_drive_path();
  metadata.original_name = test_file_name_;
  fake_drivefs().SetMetadata(std::move(metadata));

  drive_upload_observer->no_sync_update_timeout_.FireNow();

  base::RunLoop loop;
  loop.RunUntilIdle();
  EXPECT_TRUE(future.Wait());

  RemoveObservers();
}

// When the sync timer times out and no file metadata is returned, the upload
// will fail and the file will be deleted from the cache.
IN_PROC_BROWSER_TEST_F(DriveUploadObserverTest, NoFileMetadata) {
  const std::string test_file_name = "popup.pdf";
  base::FilePath source_file_path =
      SetUpSourceFile(test_file_name, drive_mount_point_);

  base::MockCallback<base::RepeatingCallback<void(int64_t)>> progress_callback;
  base::MockCallback<base::OnceCallback<void(bool)>> upload_callback;
  scoped_refptr<DriveUploadObserver> drive_upload_observer =
      new DriveUploadObserver(profile(),
                              drive_root_dir_.AppendASCII(test_file_name_),
                              kFileSize, progress_callback.Get());
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

  RemoveObservers();
}

}  // namespace ash::cloud_upload
