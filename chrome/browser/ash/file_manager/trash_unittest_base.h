// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_MANAGER_TRASH_UNITTEST_BASE_H_
#define CHROME_BROWSER_ASH_FILE_MANAGER_TRASH_UNITTEST_BASE_H_

#include <memory>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/drive/drivefs_test_support.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/ash/file_manager/volume_manager_factory.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/disks/fake_disk_mount_manager.h"
#include "chromeos/ash/components/trash_service/public/cpp/trash_service.h"
#include "chromeos/ash/components/trash_service/public/mojom/trash_service.mojom-forward.h"
#include "chromeos/ash/components/trash_service/trash_service_impl.h"
#include "components/user_manager/scoped_user_manager.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace file_manager::io_task {

inline constexpr size_t kTestFileSize = 32;

class TrashBaseTest : public testing::Test {
 public:
  TrashBaseTest(const TrashBaseTest&) = delete;
  TrashBaseTest& operator=(const TrashBaseTest&) = delete;

 protected:
  TrashBaseTest();
  ~TrashBaseTest() override;

  void SetUp() override;
  void TearDown() override;

  drive::DriveIntegrationService* CreateDriveIntegrationService(
      Profile* profile);

  mojo::PendingRemote<ash::trash_service::mojom::TrashService>
  CreateInProcessTrashService();

  storage::FileSystemURL CreateFileSystemURL(
      const base::FilePath& absolute_path);

  const base::FilePath GenerateInfoPath(const std::string& file_name);

  const base::FilePath GenerateFilesPath(const std::string& file_name);

  const std::string CreateTrashInfoContentsFromPath(
      const base::FilePath& file_path,
      const base::FilePath& base_path,
      const base::FilePath& prefix_path);

  const std::string CreateTrashInfoContentsFromPath(
      const base::FilePath& file_path);

  bool EnsureTrashDirectorySetup(const base::FilePath& parent_path);

  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      fake_user_manager_;
  std::unique_ptr<TestingProfile> profile_;
  const blink::StorageKey kTestStorageKey =
      blink::StorageKey::CreateFromStringForTesting("chrome-extension://abc");

  // DriveFS setup methods to ensure the tests have access to a mock
  // DriveIntegrationService tied to the TestingProfile.
  std::unique_ptr<drive::FakeDriveFsHelper> fake_drivefs_helper_;
  raw_ptr<drive::DriveIntegrationService, DanglingUntriaged>
      integration_service_ = nullptr;
  drive::DriveIntegrationServiceFactory::FactoryCallback
      create_drive_integration_service_;
  std::unique_ptr<drive::DriveIntegrationServiceFactory::ScopedFactoryForTest>
      service_factory_for_test_;

  base::ScopedTempDir temp_dir_;
  base::FilePath downloads_dir_;
  base::FilePath my_files_dir_;
  base::FilePath drive_dir_;
  scoped_refptr<storage::FileSystemContext> file_system_context_;

 private:
  // Maintains ownership for the in-process parsing service.
  std::unique_ptr<ash::trash_service::TrashServiceImpl> trash_service_impl_;
};

class TrashBaseIOTest : public TrashBaseTest {
 public:
  TrashBaseIOTest(const TrashBaseIOTest&) = delete;
  TrashBaseIOTest& operator=(const TrashBaseIOTest&) = delete;

 protected:
  TrashBaseIOTest();
  ~TrashBaseIOTest() override;

  void SetUp() override;

 private:
  ash::disks::FakeDiskMountManager disk_mount_manager_;
};

}  // namespace file_manager::io_task

#endif  // CHROME_BROWSER_ASH_FILE_MANAGER_TRASH_UNITTEST_BASE_H_
