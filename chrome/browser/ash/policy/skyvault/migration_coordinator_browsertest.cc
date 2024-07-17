// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/skyvault/migration_coordinator.h"

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/file_manager/file_manager_test_util.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/ash/policy/skyvault/policy_utils.h"
#include "chrome/browser/download/download_dir_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "storage/browser/file_system/file_system_url.h"

namespace policy::local_user_files {

namespace {

constexpr char kDestinationDir[] = "ChromeOS Device";

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

// TODO(b/334008191): Extract code shared with OdfsSkyvaultUploaderTest to a
// utils file.
// Tests the SkyVault migration workflow with different cloud
// providers.
class MigrationCoordinatorTest : public InProcessBrowserTest {
 public:
  MigrationCoordinatorTest() = default;
  MigrationCoordinatorTest(const MigrationCoordinatorTest&) = delete;
  MigrationCoordinatorTest& operator=(const MigrationCoordinatorTest&) = delete;
  ~MigrationCoordinatorTest() override = default;

  void TearDown() override {
    InProcessBrowserTest::TearDown();
    storage::ExternalMountPoints::GetSystemInstance()->RevokeAllFileSystems();
  }

  // Creates mount point for My files and registers local filesystem.
  void SetUpMyFiles() {
    my_files_dir_ = GetMyFilesPath(browser()->profile());
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

  base::FilePath CreateTestDir(const std::string& test_dir_name,
                               base::FilePath target_dir) {
    const base::FilePath dir_path = target_dir.AppendASCII(test_dir_name);
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      CHECK(base::CreateDirectory(dir_path));
      CHECK(base::PathExists(dir_path));
    }

    return dir_path;
  }

  // Copy the test file with `test_file_name` into the directory `target_dir`.
  base::FilePath CopyTestFile(const std::string& test_file_name,
                              base::FilePath target_dir) {
    const base::FilePath copied_file_path =
        target_dir.AppendASCII(test_file_name);

    // Copy the test file into `target_dir`.
    const base::FilePath test_file_path = GetTestFilePath(test_file_name);
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      CHECK(base::CopyFile(test_file_path, copied_file_path));
      CHECK(base::PathExists(copied_file_path));
    }

    return copied_file_path;
  }

  Profile* profile() { return browser()->profile(); }

 protected:
  base::ScopedTempDir temp_dir_;
  base::FilePath my_files_dir_;
};

class OneDriveMigrationCoordinatorTest : public MigrationCoordinatorTest {
 public:
  OneDriveMigrationCoordinatorTest() = default;
  OneDriveMigrationCoordinatorTest(const OneDriveMigrationCoordinatorTest&) =
      delete;
  OneDriveMigrationCoordinatorTest& operator=(
      const OneDriveMigrationCoordinatorTest&) = delete;
  ~OneDriveMigrationCoordinatorTest() override = default;

  // Creates and mounts fake provided file system for OneDrive.
  void SetUpODFS() {
    provided_file_system_ =
        file_manager::test::MountFakeProvidedFileSystemOneDrive(profile());
  }

  void CheckPathExistsOnODFS(const base::FilePath& path) {
    ASSERT_TRUE(provided_file_system_);
    base::test::TestFuture<
        std::unique_ptr<ash::file_system_provider::EntryMetadata>,
        base::File::Error>
        future;
    provided_file_system_->GetMetadata(path, {}, future.GetCallback());
    EXPECT_EQ(base::File::Error::FILE_OK, future.Get<base::File::Error>());
  }

  void CheckPathNotFoundOnODFS(const base::FilePath& path) {
    ASSERT_TRUE(provided_file_system_);
    base::test::TestFuture<
        std::unique_ptr<ash::file_system_provider::EntryMetadata>,
        base::File::Error>
        future;
    provided_file_system_->GetMetadata(path, {}, future.GetCallback());
    EXPECT_EQ(base::File::Error::FILE_ERROR_NOT_FOUND,
              future.Get<base::File::Error>());
  }

 protected:
  raw_ptr<file_manager::test::FakeProvidedFileSystemOneDrive,
          DanglingUntriaged>
      provided_file_system_;  // Owned by Service.
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
  base::FilePath file_path = CopyTestFile(file, my_files_dir_);

  const std::string dir = "foo";
  base::FilePath dir_path = CreateTestDir(dir, my_files_dir_);

  const std::string nested_file = "video_long.ogv";
  base::FilePath nested_file_path = CopyTestFile(nested_file, dir_path);

  MigrationCoordinator coordinator(profile());
  base::test::TestFuture<std::map<base::FilePath, MigrationUploadError>> future;
  // Upload the files.
  coordinator.Run(CloudProvider::kOneDrive, {file_path, nested_file_path},
                  kDestinationDir, future.GetCallback());
  ASSERT_TRUE(future.Get().empty());

  // Check that all files have been moved to OneDrive in the correct place.
  CheckPathExistsOnODFS(
      base::FilePath("/").AppendASCII(kDestinationDir).AppendASCII(file));
  CheckPathExistsOnODFS(base::FilePath("/")
                            .AppendASCII(kDestinationDir)
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
  base::FilePath file_path = CopyTestFile(file, my_files_dir_);

  MigrationCoordinator coordinator(profile());
  base::test::TestFuture<std::map<base::FilePath, MigrationUploadError>> future;
  // Upload the file.
  coordinator.Run(CloudProvider::kOneDrive, {file_path}, kDestinationDir,
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
  coordinator.Run(CloudProvider::kOneDrive, {}, kDestinationDir,
                  future.GetCallback());
  ASSERT_TRUE(future.Get().empty());
}

IN_PROC_BROWSER_TEST_F(OneDriveMigrationCoordinatorTest, StopUpload) {
  SetUpMyFiles();
  SetUpODFS();

  const std::string test_file_name = "video_long.ogv";
  base::FilePath file_path = CopyTestFile(test_file_name, my_files_dir_);

  base::test::TestFuture<void> future;
  // Create directly for more control over Run() and Stop().
  OneDriveMigrationUploader uploader(profile(), {file_path}, kDestinationDir,
                                     base::DoNothing());
  // Ensure Run() doesn't finish before we call Stop().
  uploader.SetEmulateSlowForTesting(true);
  uploader.Run();
  uploader.Stop(future.GetCallback());
  ASSERT_TRUE(future.Wait());

  // Check that the source file has NOT been moved to OneDrive.
  CheckPathNotFoundOnODFS(base::FilePath("/").AppendASCII(test_file_name));
}

}  // namespace policy::local_user_files
