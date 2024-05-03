// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/local_user_files/file_location_utils.h"

#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/ash/file_system_provider/fake_extension_provider.h"
#include "chrome/browser/ash/file_system_provider/service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_util.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"

using FileLocationUtilsTest = InProcessBrowserTest;

namespace policy::local_user_files {

namespace {

void MountODFS(Profile* profile) {
  auto fake_provider = ash::file_system_provider::FakeExtensionProvider::Create(
      extension_misc::kODFSExtensionId);
  auto* service = ash::file_system_provider::Service::Get(profile);
  service->RegisterProvider(std::move(fake_provider));
  ash::file_system_provider::ProviderId provider_id =
      ash::file_system_provider::ProviderId::CreateFromExtensionId(
          extension_misc::kODFSExtensionId);
  ash::file_system_provider::MountOptions options("odfs", "ODFS");
  ASSERT_EQ(base::File::FILE_OK,
            service->MountFileSystem(provider_id, options));
}

void UnmountODFS(Profile* profile) {
  const auto odfs_info = ash::cloud_upload::GetODFSInfo(profile);
  ASSERT_TRUE(odfs_info);

  file_manager::VolumeManager* volume_manager =
      file_manager::VolumeManager::Get(profile);
  ASSERT_TRUE(volume_manager);

  for (const auto& volume : volume_manager->GetVolumeList()) {
    if (volume->volume_label() == odfs_info->display_name()) {
      volume_manager->RemoveVolumeForTesting(volume->volume_id());
      return;
    }
  }
  FAIL() << "Not able to unmount ODFS";
}

}  // namespace

IN_PROC_BROWSER_TEST_F(FileLocationUtilsTest, ResolveGoogleDrive) {
  auto* drive_integration_service =
      drive::DriveIntegrationServiceFactory::FindForProfile(
          browser()->profile());
  ASSERT_FALSE(drive_integration_service->GetMountPointPath().empty());
  EXPECT_EQ(drive_integration_service->GetMountPointPath()
                .AppendASCII("root")
                .AppendASCII("folder"),
            ResolvePath("${google_drive}/folder"));
  drive_integration_service->SetEnabled(false);
  EXPECT_EQ(base::FilePath(), ResolvePath("${google_drive}/folder"));
}

IN_PROC_BROWSER_TEST_F(FileLocationUtilsTest, ResolveODFS) {
  MountODFS(browser()->profile());
  ASSERT_FALSE(
      ash::cloud_upload::GetODFSFuseboxMount(browser()->profile()).empty());
  EXPECT_EQ(ash::cloud_upload::GetODFSFuseboxMount(browser()->profile()),
            ResolvePath("${microsoft_onedrive}"));
  UnmountODFS(browser()->profile());
  EXPECT_EQ(base::FilePath(), ResolvePath("${microsoft_onedrive}"));
}

IN_PROC_BROWSER_TEST_F(FileLocationUtilsTest, ResolveRegular) {
  EXPECT_EQ(base::FilePath("/some/path"), ResolvePath("/some/path"));
}

IN_PROC_BROWSER_TEST_F(FileLocationUtilsTest, ResolveEmpty) {
  EXPECT_EQ(
      file_manager::util::GetDownloadsFolderForProfile(browser()->profile()),
      ResolvePath(""));
}

}  // namespace policy::local_user_files
