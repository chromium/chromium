// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_SKYVAULT_SKYVAULT_TEST_BASE_H_
#define CHROME_BROWSER_ASH_POLICY_SKYVAULT_SKYVAULT_TEST_BASE_H_

#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/file_manager/file_manager_test_util.h"
#include "chrome/browser/ash/file_manager/io_task.h"
#include "chrome/browser/ash/file_manager/io_task_controller.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"

namespace policy::local_user_files {

using drive::DriveIntegrationService;
using drive::util::ConnectionStatus;
using drive::util::SetDriveConnectionStatusForTesting;
using testing::_;

// Base class for all Skyvault tests
// Contains common setup and utility functions
class SkyvaultTestBase : public InProcessBrowserTest {
 public:
  SkyvaultTestBase() = default;

  SkyvaultTestBase(const SkyvaultTestBase&) = delete;
  SkyvaultTestBase& operator=(const SkyvaultTestBase&) = delete;

  ~SkyvaultTestBase() override = default;

  // InProcessBrowserTest implementation:
  void TearDown() override;

 protected:
  Profile* profile() { return browser()->profile(); }

  const base::FilePath& my_files_dir() { return my_files_dir_; }

  // Creates mount point for My files and registers local filesystem.
  void SetUpMyFiles();

  // Creates a test directory with `test_dir_name` inside `parent_dir`.
  base::FilePath CreateTestDir(const std::string& test_dir_name,
                               const base::FilePath& parent_dir);

  // Copies the test file with `test_file_name` into `parent_dir`.
  base::FilePath CopyTestFile(const std::string& test_file_name,
                              const base::FilePath& parent_dir);

 private:
  base::FilePath my_files_dir_;
};

// Base class for Skyvault tests with Microsoft OneDrive
class SkyvaultOneDriveTest : public SkyvaultTestBase {
 public:
  // Creates and mounts fake provided file system for OneDrive.
  void SetUpODFS();

  // Asserts that `path` exists on OneDrive.
  void CheckPathExistsOnODFS(const base::FilePath& path);

  // Asserts that `path` doesn't exist on OneDrive.
  void CheckPathNotFoundOnODFS(const base::FilePath& path);

 protected:
  raw_ptr<file_manager::test::FakeProvidedFileSystemOneDrive,
          DanglingUntriaged>
      provided_file_system_;  // Owned by Service.
};

// Base class for Skyvault tests with Google Drive
class SkyvaultGoogleDriveTest
    : public SkyvaultTestBase,
      public file_manager::io_task::IOTaskController::Observer {
 public:
  struct FileInfo {
    // Test file name, e.g. "example.txt"
    std::string test_file_name_;
    // Path after MyFiles e.g. "foo/bar/example.txt"
    base::FilePath local_relative_path_;
  };
  SkyvaultGoogleDriveTest();

  SkyvaultGoogleDriveTest(const SkyvaultGoogleDriveTest&) = delete;
  SkyvaultGoogleDriveTest& operator=(const SkyvaultGoogleDriveTest&) = delete;

  ~SkyvaultGoogleDriveTest() override;

  void SetUpInProcessBrowserTestFixture() override;
  void SetUpOnMainThread() override;
  void SetUp() override;
  void TearDown() override;
  void TearDownOnMainThread() override;

  // Tracking IO task progress
  void SetUpObservers();
  void RemoveObservers();

 protected:
  // Getters
  base::FilePath drive_mount_point() { return drive_mount_point_; }
  base::FilePath drive_root_dir() { return drive_root_dir_; }
  DriveIntegrationService* drive_integration_service() {
    return drive::DriveIntegrationServiceFactory::FindForProfile(profile());
  }
  file_manager::test::FakeSimpleDriveFs& fake_drivefs() {
    return fake_drivefs_helpers_[profile()]->fake_drivefs();
  }
  mojo::Remote<drivefs::mojom::DriveFsDelegate>& drivefs_delegate() {
    return fake_drivefs().delegate();
  }
  virtual base::FilePath observed_relative_drive_path(const FileInfo& info) = 0;

  // Copies the test file with `test_file_name` into `parent_dir`, saves
  // `source_file_path_` and returns it.
  base::FilePath SetUpSourceFile(const std::string& test_file_name,
                                 base::FilePath parent_dir);

  // Asserts that `path` exists on Google Drive.
  void CheckPathExistsOnDrive(const base::FilePath& path);

  // Asserts that `path` doesn't exist on Google Drive.
  void CheckPathNotFoundOnDrive(const base::FilePath& path);

  // Resolves when the upload completes, or a an error occurs.
  void Wait();
  void EndWait();

  // Used to track the upload progress during the tests.
  std::map<base::FilePath, FileInfo> source_files_;

 private:
  // Creates the fake Google Drive service.
  DriveIntegrationService* CreateDriveIntegrationService(Profile* profile);
  // Stop Wait() after asserting the expected `error`.
  void OnGetMetadataExpectSuccess(drive::FileError error,
                                  drivefs::mojom::FileMetadataPtr metadata);
  void OnGetMetadataExpectNotFound(drive::FileError error,
                                   drivefs::mojom::FileMetadataPtr metadata);

  // Drive integration service
  base::ScopedTempDir temp_dir_;
  base::FilePath drive_mount_point_;
  base::FilePath drive_root_dir_;
  drive::DriveIntegrationServiceFactory::FactoryCallback
      create_drive_integration_service_;
  std::unique_ptr<drive::DriveIntegrationServiceFactory::ScopedFactoryForTest>
      service_factory_for_test_;
  std::map<Profile*,
           std::unique_ptr<file_manager::test::FakeSimpleDriveFsHelper>>
      fake_drivefs_helpers_;

  raw_ptr<file_manager::test::FakeProvidedFileSystemOneDrive,
          DanglingUntriaged>
      provided_file_system_;  // Owned by Service.

  std::unique_ptr<base::RunLoop> run_loop_;
};

}  // namespace policy::local_user_files

#endif  // CHROME_BROWSER_ASH_POLICY_SKYVAULT_SKYVAULT_TEST_BASE_H_
