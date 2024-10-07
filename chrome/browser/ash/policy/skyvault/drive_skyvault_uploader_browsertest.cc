// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/skyvault/drive_skyvault_uploader.h"

#include <memory>
#include <optional>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/ash/file_manager/io_task.h"
#include "chrome/browser/ash/policy/skyvault/local_files_migration_constants.h"
#include "chrome/browser/ash/policy/skyvault/policy_utils.h"
#include "chrome/browser/ash/policy/skyvault/skyvault_test_base.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/ash/components/drivefs/mojom/drivefs.mojom.h"
#include "components/drive/file_errors.h"
#include "content/public/test/browser_test.h"
#include "storage/browser/file_system/file_system_url.h"
#include "testing/gmock/include/gmock/gmock.h"

using storage::FileSystemURL;

namespace policy::local_user_files {

using ::base::test::RunOnceCallback;
using testing::_;

// Tests the Drive SkyVault upload workflow.
class DriveSkyvaultUploaderTest : public SkyvaultGoogleDriveTest {
 public:
  DriveSkyvaultUploaderTest() = default;

  DriveSkyvaultUploaderTest(const DriveSkyvaultUploaderTest&) = delete;
  DriveSkyvaultUploaderTest& operator=(const DriveSkyvaultUploaderTest&) =
      delete;

  // `Wait` will not complete until this is called.
  void OnUploadDone(std::optional<MigrationUploadError> error) {
    if (fail_sync_) {
      ASSERT_EQ(error, MigrationUploadError::kCopyFailed);
    } else {
      ASSERT_FALSE(error.has_value());
    }
    EndWait();
  }

  base::FilePath observed_relative_drive_path(const FileInfo& info) override {
    base::FilePath observed_relative_drive_path;
    drive_integration_service()->GetRelativeDrivePath(
        drive_root_dir()
            .Append(kDestinationDirName)
            .Append(info.local_relative_path_),
        &observed_relative_drive_path);
    return observed_relative_drive_path;
  }

 protected:
  base::HistogramTester histogram_tester_;
  bool add_metadata_ = true;
  bool fail_sync_ = false;
  // Overrides `fail_sync_`
  base::RepeatingClosure on_transfer_complete_callback_;

 private:
  // IOTaskController::Observer:
  void OnIOTaskStatus(
      const file_manager::io_task::ProgressStatus& status) override {
    // Wait for the copy task to complete before starting the Drive sync.
    auto it = source_files_.find(status.sources[0].url.path());
    if (status.type == file_manager::io_task::OperationType::kCopy &&
        status.sources.size() == 1 && it != source_files_.end() &&
        status.state == file_manager::io_task::State::kSuccess) {
      if (on_transfer_complete_callback_) {
        on_transfer_complete_callback_.Run();
      } else if (fail_sync_) {
        SimulateDriveUploadFailure(it->second);
      } else {
        SimulateDriveUploadCompleted(it->second);
      }
    }
  }

  // Simulates the upload of the file to Drive by sending a series of fake
  // signals to the DriveFs delegate.
  void SimulateDriveUploadCompleted(const FileInfo& info) {
    if (add_metadata_) {
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
    }
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

IN_PROC_BROWSER_TEST_F(DriveSkyvaultUploaderTest, SuccessfulUpload) {
  SetUpObservers();
  SetUpMyFiles();

  const std::string test_file_name = "text.docx";
  const std::string test_dir_name = "foo";
  const base::FilePath dir_path = CreateTestDir(test_dir_name, my_files_dir());
  const base::FilePath source_file =
      SetUpSourceFile(test_file_name, my_files_dir());

  EXPECT_CALL(fake_drivefs(), ImmediatelyUpload)
      .WillOnce(RunOnceCallback<1>(drive::FileError::FILE_ERROR_OK));

  base::test::TestFuture<std::optional<MigrationUploadError>> future;
  auto drive_upload_handler = std::make_unique<DriveSkyvaultUploader>(
      profile(), source_file, base::FilePath(kDestinationDirName),
      future.GetCallback());
  drive_upload_handler->Run();

  EXPECT_EQ(future.Get(), std::nullopt);

  // Check that the source file has been moved to Drive.
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_FALSE(base::PathExists(source_file));
    CheckPathExistsOnDrive(
        observed_relative_drive_path(source_files_.find(source_file)->second));
  }

  histogram_tester_.ExpectBucketCount(
      "Enterprise.SkyVault.Migration.GoogleDrive.DeleteError", false, 1);
  histogram_tester_.ExpectBucketCount(
      "Enterprise.SkyVault.Migration.GoogleDrive.DeleteError", true, 0);
}

// Test that when the sync to Drive fails, the file is not moved to Drive.
IN_PROC_BROWSER_TEST_F(DriveSkyvaultUploaderTest, FailedUpload) {
  fail_sync_ = true;
  SetUpObservers();
  SetUpMyFiles();

  const std::string test_file_name = "text.docx";
  const base::FilePath source_file =
      SetUpSourceFile(test_file_name, my_files_dir());

  EXPECT_CALL(fake_drivefs(), ImmediatelyUpload)
      .WillOnce(RunOnceCallback<1>(drive::FileError::FILE_ERROR_FAILED));

  base::test::TestFuture<std::optional<MigrationUploadError>> future;
  auto drive_upload_handler = std::make_unique<DriveSkyvaultUploader>(
      profile(), source_file, base::FilePath(kDestinationDirName),
      future.GetCallback());
  drive_upload_handler->Run();

  EXPECT_EQ(future.Get(), MigrationUploadError::kCopyFailed);

  // Check that the source file has not been moved to Drive.
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_TRUE(base::PathExists(my_files_dir().AppendASCII(test_file_name)));
    CheckPathNotFoundOnDrive(
        observed_relative_drive_path(source_files_.find(source_file)->second));
  }

  histogram_tester_.ExpectBucketCount(
      "Enterprise.SkyVault.Migration.GoogleDrive.DeleteError", false, 1);
  histogram_tester_.ExpectBucketCount(
      "Enterprise.SkyVault.Migration.GoogleDrive.DeleteError", true, 0);
}

IN_PROC_BROWSER_TEST_F(DriveSkyvaultUploaderTest, FailedDelete) {
  SetUpObservers();
  SetUpMyFiles();

  const std::string test_file_name = "text.docx";
  const base::FilePath source_file =
      SetUpSourceFile(test_file_name, my_files_dir());

  EXPECT_CALL(fake_drivefs(), ImmediatelyUpload)
      .WillOnce(RunOnceCallback<1>(drive::FileError::FILE_ERROR_OK));

  base::test::TestFuture<std::optional<MigrationUploadError>> future;
  auto drive_upload_handler = std::make_unique<DriveSkyvaultUploader>(
      profile(), source_file, base::FilePath(kDestinationDirName),
      future.GetCallback());
  drive_upload_handler->SetFailDeleteForTesting(/*fail=*/true);
  drive_upload_handler->Run();

  EXPECT_EQ(future.Get(), MigrationUploadError::kDeleteFailed);

  // Check that the source file has been moved to Drive.
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_FALSE(base::PathExists(my_files_dir().AppendASCII(test_file_name)));
    CheckPathExistsOnDrive(
        observed_relative_drive_path(source_files_.find(source_file)->second));
  }

  histogram_tester_.ExpectBucketCount(
      "Enterprise.SkyVault.Migration.GoogleDrive.DeleteError", false, 0);
  histogram_tester_.ExpectBucketCount(
      "Enterprise.SkyVault.Migration.GoogleDrive.DeleteError", true, 1);
}

// Test that when connection to Drive isn't available, the upload fails
// immediately.
IN_PROC_BROWSER_TEST_F(DriveSkyvaultUploaderTest, NoConnection) {
  SetUpObservers();
  SetUpMyFiles();
  SetDriveConnectionStatusForTesting(ConnectionStatus::kNoNetwork);

  const std::string test_file_name = "text.docx";
  const base::FilePath source_file =
      SetUpSourceFile(test_file_name, my_files_dir());

  EXPECT_CALL(fake_drivefs(), ImmediatelyUpload).Times(0);

  base::test::TestFuture<std::optional<MigrationUploadError>> future;
  auto drive_upload_handler = std::make_unique<DriveSkyvaultUploader>(
      profile(), source_file, base::FilePath(kDestinationDirName),
      future.GetCallback());
  drive_upload_handler->Run();

  EXPECT_EQ(future.Get(), MigrationUploadError::kServiceUnavailable);

  // Check that the source file has not been moved to Drive.
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_TRUE(base::PathExists(my_files_dir().AppendASCII(test_file_name)));
    CheckPathNotFoundOnDrive(
        observed_relative_drive_path(source_files_.find(source_file)->second));
  }
}

// Test that when connection to Drive fails during upload, the file is not moved
// to Drive.
IN_PROC_BROWSER_TEST_F(DriveSkyvaultUploaderTest, ConnectionLostDuringUpload) {
  SetUpObservers();
  SetUpMyFiles();

  const std::string test_file_name = "text.docx";
  const base::FilePath source_file =
      SetUpSourceFile(test_file_name, my_files_dir());

  on_transfer_complete_callback_ = base::BindLambdaForTesting([this] {
    SetDriveConnectionStatusForTesting(ConnectionStatus::kNoNetwork);
    drive_integration_service()->OnNetworkChanged();
  });

  base::test::TestFuture<std::optional<MigrationUploadError>> future;
  auto drive_upload_handler = std::make_unique<DriveSkyvaultUploader>(
      profile(), source_file, base::FilePath(kDestinationDirName),
      future.GetCallback());
  drive_upload_handler->Run();

  EXPECT_EQ(future.Get(), MigrationUploadError::kServiceUnavailable);

  // Check that the source file has not been moved to Drive.
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_TRUE(base::PathExists(my_files_dir().AppendASCII(test_file_name)));
    CheckPathNotFoundOnDrive(
        observed_relative_drive_path(source_files_.find(source_file)->second));
  }
}

}  // namespace policy::local_user_files
