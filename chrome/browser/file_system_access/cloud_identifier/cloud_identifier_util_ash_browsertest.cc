// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/file_system_access_cloud_identifier_provider_ash.h"
#include "chrome/browser/ash/drive/drive_integration_service_browser_test_base.h"
#include "chrome/browser/ash/file_manager/file_manager_test_util.h"
#include "chrome/browser/ash/file_manager/volume.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/ash/components/drivefs/fake_drivefs.h"
#include "chromeos/crosapi/mojom/file_system_access_cloud_identifier.mojom-shared.h"
#include "content/public/test/browser_test.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_cloud_identifier.mojom.h"

namespace {

// Calls the `GetCloudIdentifier()` from
// `FileSystemAccessCloudIdentifierProviderAsh`, which is the method tested by
// this browsertest.
crosapi::mojom::FileSystemAccessCloudIdentifierPtr GetCloudIdentifierBlocking(
    const base::FilePath& virtual_path,
    crosapi::mojom::HandleType handle_type) {
  crosapi::FileSystemAccessCloudIdentifierProviderAsh* const provider =
      crosapi::CrosapiManager::Get()
          ->crosapi_ash()
          ->file_system_access_cloud_identifier_provider_ash();

  base::test::TestFuture<crosapi::mojom::FileSystemAccessCloudIdentifierPtr>
      future;
  provider->GetCloudIdentifier(virtual_path, handle_type, future.GetCallback());
  return future.Take();
}

// Returns the virtual path for a given absolute path.
base::FilePath GetVirtualPath(const base::FilePath& absolute_path) {
  base::FilePath virtual_path;
  EXPECT_TRUE(storage::ExternalMountPoints::GetSystemInstance()->GetVirtualPath(
      absolute_path, &virtual_path));
  return virtual_path;
}

// Tests the `GetCloudIdentifier()` for local files.
class GetLocalCloudIdentifierBrowserTest : public InProcessBrowserTest {
 public:
  void SetUp() override {
    // Set up local file system.
    ASSERT_TRUE(tmp_dir_.CreateUniqueTempDir());
    storage::ExternalMountPoints::GetSystemInstance()->RegisterFileSystem(
        "test_fs_mount", storage::kFileSystemTypeLocal,
        storage::FileSystemMountOption(), tmp_dir_.GetPath());

    // Must run after our setup because it actually runs the test.
    InProcessBrowserTest::SetUp();
  }

  base::FilePath CreateTestFile() {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::FilePath result;
    EXPECT_TRUE(base::CreateTemporaryFileInDir(tmp_dir_.GetPath(), &result));
    return result;
  }

  base::FilePath CreateTestDir() {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::FilePath result;
    EXPECT_TRUE(base::CreateTemporaryDirInDir(
        tmp_dir_.GetPath(), FILE_PATH_LITERAL("test"), &result));
    return result;
  }

 protected:
  base::ScopedTempDir tmp_dir_;
};

IN_PROC_BROWSER_TEST_F(GetLocalCloudIdentifierBrowserTest,
                       NoHandleForLocalFileOrFolder) {
  const base::FilePath file_path = CreateTestFile();
  const base::FilePath file_virtual_path = GetVirtualPath(file_path);
  crosapi::mojom::FileSystemAccessCloudIdentifierPtr file_result =
      GetCloudIdentifierBlocking(file_virtual_path,
                                 crosapi::mojom::HandleType::kFile);
  EXPECT_TRUE(file_result.is_null());

  const base::FilePath dir_path = CreateTestDir();
  const base::FilePath dir_virtual_path = GetVirtualPath(dir_path);
  crosapi::mojom::FileSystemAccessCloudIdentifierPtr dir_result =
      GetCloudIdentifierBlocking(dir_virtual_path,
                                 crosapi::mojom::HandleType::kDirectory);
  EXPECT_TRUE(dir_result.is_null());
}

// Tests the `GetCloudIdentifier()` for files backed by DriveFS.
class GetDriveFsCloudIdentifierBrowserTest
    : public drive::DriveIntegrationServiceBrowserTestBase {
 protected:
  base::FilePath AddDriveFsFile(const std::string& item_id) {
    base::FilePath absolute_path;
    base::FilePath relateive_path;
    AddDriveFileWithRelativePath(browser()->profile(), item_id,
                                 base::FilePath(), &relateive_path,
                                 &absolute_path);
    return absolute_path;
  }
};

IN_PROC_BROWSER_TEST_F(GetDriveFsCloudIdentifierBrowserTest, GetHandleSuccess) {
  // Set up DriveFS file system.
  InitTestFileMountRoot(browser()->profile());

  const std::string provider_name = "drive.google.com";

  const std::string file_1_item_id = "item-id-1";
  const std::string file_2_item_id = "item-id-2";

  const base::FilePath file_1_path = AddDriveFsFile(file_1_item_id);
  const base::FilePath file_2_path = AddDriveFsFile(file_2_item_id);

  const base::FilePath file_1_virtual_path = GetVirtualPath(file_1_path);
  const base::FilePath file_2_virtual_path = GetVirtualPath(file_2_path);

  crosapi::mojom::FileSystemAccessCloudIdentifierPtr file_1_result =
      GetCloudIdentifierBlocking(file_1_virtual_path,
                                 crosapi::mojom::HandleType::kFile);
  crosapi::mojom::FileSystemAccessCloudIdentifierPtr file_2_result =
      GetCloudIdentifierBlocking(file_2_virtual_path,
                                 crosapi::mojom::HandleType::kFile);
  ASSERT_FALSE(file_1_result.is_null());
  ASSERT_FALSE(file_2_result.is_null());

  crosapi::mojom::FileSystemAccessCloudIdentifierPtr expected_result_1 =
      crosapi::mojom::FileSystemAccessCloudIdentifier::New(provider_name,
                                                           file_1_item_id);
  crosapi::mojom::FileSystemAccessCloudIdentifierPtr expected_result_2 =
      crosapi::mojom::FileSystemAccessCloudIdentifier::New(provider_name,
                                                           file_2_item_id);
  EXPECT_EQ(expected_result_1, file_1_result);
  EXPECT_EQ(expected_result_2, file_2_result);
}

IN_PROC_BROWSER_TEST_F(GetDriveFsCloudIdentifierBrowserTest, GetHandleError) {
  // Set up DriveFS file system.
  InitTestFileMountRoot(browser()->profile());

  // Unexpected type (expect dir for file) should fail.
  const std::string file_item_id = "file-item-id";
  const base::FilePath file_path = AddDriveFsFile(file_item_id);
  const base::FilePath file_virtual_path = GetVirtualPath(file_path);
  crosapi::mojom::FileSystemAccessCloudIdentifierPtr file_result =
      GetCloudIdentifierBlocking(file_virtual_path,
                                 crosapi::mojom::HandleType::kDirectory);
  EXPECT_TRUE(file_result.is_null());

  // Files that have not been synced (prefixed with "local") should fail.
  const std::string local_file_item_id = "local-item-id";
  const base::FilePath local_file_path = AddDriveFsFile(local_file_item_id);
  const base::FilePath local_file_virtual_path =
      GetVirtualPath(local_file_path);
  crosapi::mojom::FileSystemAccessCloudIdentifierPtr local_fileresult =
      GetCloudIdentifierBlocking(local_file_virtual_path,
                                 crosapi::mojom::HandleType::kFile);
  EXPECT_TRUE(local_fileresult.is_null());

  // Non-existent files should fail.
  const base::FilePath non_existant_file_virtual_path =
      file_virtual_path.DirName().Append("does-not-exist");
  crosapi::mojom::FileSystemAccessCloudIdentifierPtr non_existant_file_result =
      GetCloudIdentifierBlocking(non_existant_file_virtual_path,
                                 crosapi::mojom::HandleType::kFile);
  EXPECT_TRUE(non_existant_file_result.is_null());
}

// Tests the `GetCloudIdentifier()` for files provided via
// `chrome.fileSystemProvider` API.
using GetProvidedFsCloudIdentifierBrowserTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(GetProvidedFsCloudIdentifierBrowserTest,
                       GetHandleSuccess) {
  base::WeakPtr<file_manager::Volume> fsp_volume =
      file_manager::test::InstallFileSystemProviderChromeApp(
          browser()->profile());

  const std::string provider_name = "provided-file-system-provider";

  base::FilePath file_1_path =
      fsp_volume->mount_path().AppendASCII("readonly.txt");
  base::FilePath file_2_path =
      fsp_volume->mount_path().AppendASCII("readwrite.gif");
  base::FilePath dir_path = fsp_volume->mount_path();

  const base::FilePath file_1_virtual_path = GetVirtualPath(file_1_path);
  const base::FilePath file_2_virtual_path = GetVirtualPath(file_2_path);
  const base::FilePath dir_virtual_path = GetVirtualPath(dir_path);

  crosapi::mojom::FileSystemAccessCloudIdentifierPtr file_1_result =
      GetCloudIdentifierBlocking(file_1_virtual_path,
                                 crosapi::mojom::HandleType::kFile);
  crosapi::mojom::FileSystemAccessCloudIdentifierPtr file_2_result =
      GetCloudIdentifierBlocking(file_2_virtual_path,
                                 crosapi::mojom::HandleType::kFile);
  crosapi::mojom::FileSystemAccessCloudIdentifierPtr dir_result =
      GetCloudIdentifierBlocking(dir_virtual_path,
                                 crosapi::mojom::HandleType::kDirectory);
  ASSERT_FALSE(file_1_result.is_null());
  ASSERT_FALSE(file_2_result.is_null());
  ASSERT_FALSE(dir_result.is_null());

  crosapi::mojom::FileSystemAccessCloudIdentifierPtr expected_result_file_1 =
      crosapi::mojom::FileSystemAccessCloudIdentifier::New(provider_name,
                                                           "readonly-txt-id");
  crosapi::mojom::FileSystemAccessCloudIdentifierPtr expected_result_file_2 =
      crosapi::mojom::FileSystemAccessCloudIdentifier::New(provider_name,
                                                           "readwrite-gif-id");
  crosapi::mojom::FileSystemAccessCloudIdentifierPtr expected_result_dir =
      crosapi::mojom::FileSystemAccessCloudIdentifier::New(provider_name,
                                                           "root-id");
  EXPECT_EQ(expected_result_file_1, file_1_result);
  EXPECT_EQ(expected_result_file_2, file_2_result);
  EXPECT_EQ(expected_result_dir, dir_result);
}

IN_PROC_BROWSER_TEST_F(GetProvidedFsCloudIdentifierBrowserTest,
                       GetHandleError) {
  base::WeakPtr<file_manager::Volume> fsp_volume =
      file_manager::test::InstallFileSystemProviderChromeApp(
          browser()->profile());

  // Unexpected type (expect dir for file) should fail.
  base::FilePath file_1_path =
      fsp_volume->mount_path().AppendASCII("readonly.txt");
  const base::FilePath file_1_virtual_path = GetVirtualPath(file_1_path);
  crosapi::mojom::FileSystemAccessCloudIdentifierPtr file_1_result =
      GetCloudIdentifierBlocking(file_1_virtual_path,
                                 crosapi::mojom::HandleType::kDirectory);
  EXPECT_TRUE(file_1_result.is_null());

  // Unexpected type (expect file for dir) should fail.
  base::FilePath dir_path = fsp_volume->mount_path();
  const base::FilePath dir_virtual_path = GetVirtualPath(dir_path);
  crosapi::mojom::FileSystemAccessCloudIdentifierPtr dir_result =
      GetCloudIdentifierBlocking(dir_virtual_path,
                                 crosapi::mojom::HandleType::kFile);
  EXPECT_TRUE(dir_result.is_null());

  // readonly.png has no cloud identifier.
  base::FilePath file_2_path =
      fsp_volume->mount_path().AppendASCII("readonly.png");
  const base::FilePath file_2_virtual_path = GetVirtualPath(file_2_path);
  crosapi::mojom::FileSystemAccessCloudIdentifierPtr file_2_result =
      GetCloudIdentifierBlocking(file_2_virtual_path,
                                 crosapi::mojom::HandleType::kFile);
  EXPECT_TRUE(file_2_result.is_null());

  // Non-existent files should fail.
  const base::FilePath non_existant_file_virtual_path =
      file_1_virtual_path.DirName().Append("does-not-exist");
  crosapi::mojom::FileSystemAccessCloudIdentifierPtr non_existant_file_result =
      GetCloudIdentifierBlocking(non_existant_file_virtual_path,
                                 crosapi::mojom::HandleType::kFile);
  EXPECT_TRUE(non_existant_file_result.is_null());
}

}  // namespace
