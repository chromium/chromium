// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/skyvault/odfs_skyvault_uploader.h"

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/file_manager/file_manager_test_util.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "storage/browser/file_system/file_system_url.h"

namespace ash::cloud_upload {

namespace {

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

// Tests the OneDrive upload workflow using the static
// `OneDriveUploadHandler::Upload` method. Ensures that the upload completes
// with the expected results.
class OdfsSkyvaultUploaderTest : public InProcessBrowserTest {
 public:
  OdfsSkyvaultUploaderTest() {
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    my_files_dir_ = temp_dir_.GetPath().Append("myfiles");
  }

  OdfsSkyvaultUploaderTest(const OdfsSkyvaultUploaderTest&) = delete;
  OdfsSkyvaultUploaderTest& operator=(const OdfsSkyvaultUploaderTest&) = delete;

  void TearDown() override {
    InProcessBrowserTest::TearDown();
    storage::ExternalMountPoints::GetSystemInstance()->RevokeAllFileSystems();
  }

  // Creates mount point for My files and registers local filesystem.
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

  // Creates and mounts fake provided file system for OneDrive.
  void SetUpODFS() {
    provided_file_system_ =
        file_manager::test::MountFakeProvidedFileSystemOneDrive(profile());
  }

  // Copy the test file with `test_file_name` into the directory `target_dir`.
  storage::FileSystemURL CopyTestFile(const std::string& test_file_name,
                                      base::FilePath target_dir) {
    const base::FilePath copied_file_path =
        target_dir.AppendASCII(test_file_name);

    // Copy the test file into `target_dir`.
    const base::FilePath test_file_path = GetTestFilePath(test_file_name);
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      CHECK(base::CopyFile(test_file_path, copied_file_path));
    }

    FileSystemURL copied_file_url = FilePathToFileSystemURL(
        profile(),
        file_manager::util::GetFileManagerFileSystemContext(profile()),
        copied_file_path);

    // Check that the copied file exists at the intended location.
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      EXPECT_TRUE(base::PathExists(copied_file_path));
    }

    return copied_file_url;
  }

  void CheckPathExistsOnODFS(const base::FilePath& path) {
    ASSERT_TRUE(provided_file_system_);
    base::test::TestFuture<std::unique_ptr<file_system_provider::EntryMetadata>,
                           base::File::Error>
        future;
    provided_file_system_->GetMetadata(path, {}, future.GetCallback());
    EXPECT_EQ(base::File::Error::FILE_OK, future.Get<base::File::Error>());
  }

  void CheckPathNotFoundOnODFS(const base::FilePath& path) {
    ASSERT_TRUE(provided_file_system_);
    base::test::TestFuture<std::unique_ptr<file_system_provider::EntryMetadata>,
                           base::File::Error>
        future;
    provided_file_system_->GetMetadata(path, {}, future.GetCallback());
    EXPECT_EQ(base::File::Error::FILE_ERROR_NOT_FOUND,
              future.Get<base::File::Error>());
  }

  Profile* profile() { return browser()->profile(); }

 protected:
  raw_ptr<file_manager::test::FakeProvidedFileSystemOneDrive,
          DanglingUntriaged>
      provided_file_system_;  // Owned by Service.

  base::ScopedTempDir temp_dir_;
  base::FilePath my_files_dir_;
};

IN_PROC_BROWSER_TEST_F(OdfsSkyvaultUploaderTest, SuccessfulUpload) {
  SetUpMyFiles();
  SetUpODFS();
  const std::string test_file_name = "video_long.ogv";
  storage::FileSystemURL source_file_url =
      CopyTestFile(test_file_name, my_files_dir_);

  // Start the upload workflow and end the test once the upload upload callback
  // is run.
  base::MockCallback<base::RepeatingCallback<void(int)>> progress_callback;
  base::test::TestFuture<bool> upload_callback;
  EXPECT_CALL(progress_callback, Run(/*progress=*/100));
  OdfsSkyvaultUploader::Upload(profile(), source_file_url.path(),
                               OdfsSkyvaultUploader::FileType::kDownload,
                               progress_callback.Get(),
                               upload_callback.GetCallback());
  EXPECT_EQ(upload_callback.Get<bool>(), true);

  // Check that the source file has been moved to OneDrive.
  CheckPathExistsOnODFS(base::FilePath("/").AppendASCII(test_file_name));
}

IN_PROC_BROWSER_TEST_F(OdfsSkyvaultUploaderTest, FailedUpload) {
  SetUpMyFiles();
  SetUpODFS();
  // Ensure Upload fails due to memory error and that reauthentication to
  // OneDrive is not required.
  provided_file_system_->SetCreateFileError(
      base::File::Error::FILE_ERROR_NO_MEMORY);
  provided_file_system_->SetReauthenticationRequired(false);
  const std::string test_file_name = "id3Audio.mp3";
  storage::FileSystemURL source_file_url =
      CopyTestFile(test_file_name, my_files_dir_);

  // Start the upload workflow and end the test once the upload upload callback
  // is run.
  base::MockCallback<base::RepeatingCallback<void(int)>> progress_callback;
  base::test::TestFuture<bool> upload_callback;
  OdfsSkyvaultUploader::Upload(profile(), source_file_url.path(),
                               OdfsSkyvaultUploader::FileType::kDownload,
                               progress_callback.Get(),
                               upload_callback.GetCallback());
  EXPECT_EQ(upload_callback.Get<bool>(), false);

  // Check that the source file has not been moved to OneDrive.
  CheckPathNotFoundOnODFS(base::FilePath("/").AppendASCII(test_file_name));
}

}  // namespace ash::cloud_upload
