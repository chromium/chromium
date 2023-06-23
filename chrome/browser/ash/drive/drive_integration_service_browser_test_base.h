// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_DRIVE_DRIVE_INTEGRATION_SERVICE_BROWSER_TEST_BASE_H_
#define CHROME_BROWSER_ASH_DRIVE_DRIVE_INTEGRATION_SERVICE_BROWSER_TEST_BASE_H_

#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"

namespace drivefs {
class FakeDriveFs;
}  // namespace drivefs

namespace drive {
class FakeDriveFsHelper;

// The test base that supports adding drive files.
class DriveIntegrationServiceBrowserTestBase
    : public MixinBasedInProcessBrowserTest {
 public:
  DriveIntegrationServiceBrowserTestBase();
  DriveIntegrationServiceBrowserTestBase(
      const DriveIntegrationServiceBrowserTestBase&) = delete;
  DriveIntegrationServiceBrowserTestBase& operator=(
      const DriveIntegrationServiceBrowserTestBase&) = delete;
  ~DriveIntegrationServiceBrowserTestBase() override;

 protected:
  drivefs::FakeDriveFs* GetFakeDriveFsForProfile(Profile* profile);
  virtual drive::DriveIntegrationService* CreateDriveIntegrationService(
      Profile* profile);

  // Initiates the test file mount root associated with `profile` . Only used
  // when preparing for `AddDriveFileWithRelativePath()`.
  void InitTestFileMountRoot(Profile* profile);

  // Adds a file to the drive file system associated with `profile`. Returns the
  // relative and the absolute paths to the generated file through params.
  // `directory_path` is a relative path from the test file mount directory
  // associated with `profile` to the parent directory where a new file is
  // added. For example, if `directory_path` is "foo/bar" and the test
  // file mount root of `profile` is "/fsdrive/xxx", then a new file will be
  // added to "/fsdrive/xxx/foo/bar/". NOTE:
  // 1. `directory_path` should not be absolute;
  // 2. `directory_path` could be empty. If so, a new file is added to the test
  // file mount directory;
  // 3. `InitTestFileMountRoot()` has to be called before using this function.
  void AddDriveFileWithRelativePath(Profile* profile,
                                    const std::string& drive_file_id,
                                    const base::FilePath& directory_path,
                                    base::FilePath* new_file_relative_path,
                                    base::FilePath* new_file_absolute_path);

  // InProcessBrowserTest:
  bool SetUpUserDataDirectory() override;
  void SetUpInProcessBrowserTestFixture() override;

 private:
  // In each element:
  // 1. The key is a profile pointer;
  // 2. The value is a temporary directory under the drive file system
  // associated with the profile. It hosts temporary drive files.
  std::map<Profile*, base::ScopedTempDir> test_file_mount_root_mappings_;

  drive::DriveIntegrationServiceFactory::FactoryCallback
      create_drive_integration_service_;
  std::unique_ptr<drive::DriveIntegrationServiceFactory::ScopedFactoryForTest>
      service_factory_for_test_;
  std::map<Profile*, std::unique_ptr<drive::FakeDriveFsHelper>>
      fake_drivefs_helpers_;
};

}  // namespace drive

#endif  // CHROME_BROWSER_ASH_DRIVE_DRIVE_INTEGRATION_SERVICE_BROWSER_TEST_BASE_H_
