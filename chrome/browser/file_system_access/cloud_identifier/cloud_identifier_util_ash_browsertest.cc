// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/file_system_access/cloud_identifier/cloud_identifier_util_ash.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/drive/drive_integration_service_browser_test_base.h"
#include "chrome/browser/ash/file_manager/file_manager_test_util.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/volume.h"
#include "chrome/browser/extensions/scoped_test_mv2_enabler.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/ash/components/drivefs/fake_drivefs.h"
#include "content/public/test/browser_test.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "storage/browser/file_system/file_system_context.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_cloud_identifier.mojom.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_error.mojom.h"

namespace {

// Calls the `GetCloudIdentifier()` from
// `FileSystemAccessCloudIdentifierProviderAsh`, which is the method tested by
// this browsertest.
std::tuple<blink::mojom::FileSystemAccessErrorPtr,
           std::vector<blink::mojom::FileSystemAccessCloudIdentifierPtr>>
GetCloudIdentifierBlocking(
    const storage::FileSystemURL& url,
    content::FileSystemAccessPermissionContext::HandleType handle_type) {
  base::test::TestFuture<
      blink::mojom::FileSystemAccessErrorPtr,
      std::vector<blink::mojom::FileSystemAccessCloudIdentifierPtr>>
      future;
  cloud_identifier::GetCloudIdentifier(url, handle_type, future.GetCallback());
  return future.Take();
}

// Returns the virtual path for a given absolute path.
base::FilePath GetVirtualPath(const base::FilePath& absolute_path) {
  base::FilePath virtual_path;
  EXPECT_TRUE(storage::ExternalMountPoints::GetSystemInstance()->GetVirtualPath(
      absolute_path, &virtual_path));
  return virtual_path;
}

storage::FileSystemURL GetFileSystemURL(
    content::BrowserContext* browser_context,
    const base::FilePath& virtual_path) {
  storage::FileSystemContext* file_system_context =
      file_manager::util::GetFileManagerFileSystemContext(browser_context);
  return file_system_context->CreateCrackedFileSystemURL(
      blink::StorageKey(), storage::kFileSystemTypeExternal, virtual_path);
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
  {
    const base::FilePath file_path = CreateTestFile();
    const storage::FileSystemURL file_url =
        GetFileSystemURL(GetProfile(), GetVirtualPath(file_path));
    auto [error, handles] = GetCloudIdentifierBlocking(
        file_url,
        content::FileSystemAccessPermissionContext::HandleType::kFile);
    EXPECT_EQ(error->status, blink::mojom::FileSystemAccessStatus::kOk);
    EXPECT_TRUE(handles.empty());
  }

  {
    const base::FilePath dir_path = CreateTestDir();
    const storage::FileSystemURL dir_url =
        GetFileSystemURL(GetProfile(), GetVirtualPath(dir_path));
    auto [error, handles] = GetCloudIdentifierBlocking(
        dir_url,
        content::FileSystemAccessPermissionContext::HandleType::kDirectory);
    EXPECT_EQ(error->status, blink::mojom::FileSystemAccessStatus::kOk);
    EXPECT_TRUE(handles.empty());
  }
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

  const storage::FileSystemURL file_1_url =
      GetFileSystemURL(GetProfile(), GetVirtualPath(file_1_path));
  const storage::FileSystemURL file_2_url =
      GetFileSystemURL(GetProfile(), GetVirtualPath(file_2_path));

  auto [file_1_error, file_1_handles] = GetCloudIdentifierBlocking(
      file_1_url,
      content::FileSystemAccessPermissionContext::HandleType::kFile);
  auto [file_2_error, file_2_handles] = GetCloudIdentifierBlocking(
      file_2_url,
      content::FileSystemAccessPermissionContext::HandleType::kFile);

  ASSERT_EQ(file_1_error->status, blink::mojom::FileSystemAccessStatus::kOk);
  ASSERT_EQ(file_1_handles.size(), 1u);
  EXPECT_EQ(file_1_handles[0],
            blink::mojom::FileSystemAccessCloudIdentifier::New(provider_name,
                                                               file_1_item_id));

  ASSERT_EQ(file_2_error->status, blink::mojom::FileSystemAccessStatus::kOk);
  ASSERT_EQ(file_2_handles.size(), 1u);
  EXPECT_EQ(file_2_handles[0],
            blink::mojom::FileSystemAccessCloudIdentifier::New(provider_name,
                                                               file_2_item_id));
}

IN_PROC_BROWSER_TEST_F(GetDriveFsCloudIdentifierBrowserTest, GetHandleError) {
  // Set up DriveFS file system.
  InitTestFileMountRoot(browser()->profile());

  // Unexpected type (expect dir for file) should fail.
  const std::string file_item_id = "file-item-id";
  const base::FilePath file_path = AddDriveFsFile(file_item_id);
  const storage::FileSystemURL file_url =
      GetFileSystemURL(GetProfile(), GetVirtualPath(file_path));
  auto [file_error, file_handles] = GetCloudIdentifierBlocking(
      file_url,
      content::FileSystemAccessPermissionContext::HandleType::kDirectory);
  EXPECT_NE(file_error->status, blink::mojom::FileSystemAccessStatus::kOk);

  // Files that have not been synced (prefixed with "local") should fail.
  const std::string local_file_item_id = "local-item-id";
  const base::FilePath local_file_path = AddDriveFsFile(local_file_item_id);
  const storage::FileSystemURL local_file_url =
      GetFileSystemURL(GetProfile(), GetVirtualPath(local_file_path));
  auto [local_file_error, local_file_handles] = GetCloudIdentifierBlocking(
      local_file_url,
      content::FileSystemAccessPermissionContext::HandleType::kFile);
  EXPECT_NE(local_file_error->status,
            blink::mojom::FileSystemAccessStatus::kOk);

  // Non-existent files should fail.
  const storage::FileSystemURL non_existant_file_virtual_path =
      GetFileSystemURL(GetProfile(), GetVirtualPath(file_path).DirName().Append(
                                         "does-not-exist"));
  auto [non_existant_file_error, non_existant_file_handles] =
      GetCloudIdentifierBlocking(
          non_existant_file_virtual_path,
          content::FileSystemAccessPermissionContext::HandleType::kFile);
  EXPECT_NE(non_existant_file_error->status,
            blink::mojom::FileSystemAccessStatus::kOk);
}

// Tests the `GetCloudIdentifier()` for files provided via
// `chrome.fileSystemProvider` API.
using GetProvidedFsCloudIdentifierBrowserTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(GetProvidedFsCloudIdentifierBrowserTest,
                       GetHandleSuccess) {
  // TODO(https://crbug.com/40804030): Remove this when updated to use MV3.
  extensions::ScopedTestMV2Enabler mv2_enabler;

  base::WeakPtr<file_manager::Volume> fsp_volume =
      file_manager::test::InstallFileSystemProviderChromeApp(
          browser()->profile());

  const std::string provider_name = "provided-file-system-provider";

  base::FilePath file_1_path =
      fsp_volume->mount_path().AppendASCII("readonly.txt");
  base::FilePath file_2_path =
      fsp_volume->mount_path().AppendASCII("readwrite.gif");
  base::FilePath dir_path = fsp_volume->mount_path();

  const storage::FileSystemURL file_1_url =
      GetFileSystemURL(GetProfile(), GetVirtualPath(file_1_path));
  const storage::FileSystemURL file_2_url =
      GetFileSystemURL(GetProfile(), GetVirtualPath(file_2_path));
  const storage::FileSystemURL dir_url =
      GetFileSystemURL(GetProfile(), GetVirtualPath(dir_path));

  auto [file_1_error, file_1_handles] = GetCloudIdentifierBlocking(
      file_1_url,
      content::FileSystemAccessPermissionContext::HandleType::kFile);
  auto [file_2_error, file_2_handles] = GetCloudIdentifierBlocking(
      file_2_url,
      content::FileSystemAccessPermissionContext::HandleType::kFile);
  auto [dir_error, dir_handles] = GetCloudIdentifierBlocking(
      dir_url,
      content::FileSystemAccessPermissionContext::HandleType::kDirectory);
  ASSERT_EQ(file_1_error->status, blink::mojom::FileSystemAccessStatus::kOk);
  ASSERT_EQ(file_2_error->status, blink::mojom::FileSystemAccessStatus::kOk);
  ASSERT_EQ(dir_error->status, blink::mojom::FileSystemAccessStatus::kOk);

  ASSERT_EQ(file_1_handles.size(), 1u);
  EXPECT_EQ(file_1_handles[0],
            blink::mojom::FileSystemAccessCloudIdentifier::New(
                provider_name, "readonly-txt-id"));

  ASSERT_EQ(file_2_handles.size(), 1u);
  EXPECT_EQ(file_2_handles[0],
            blink::mojom::FileSystemAccessCloudIdentifier::New(
                provider_name, "readwrite-gif-id"));

  ASSERT_EQ(dir_handles.size(), 1u);
  EXPECT_EQ(dir_handles[0], blink::mojom::FileSystemAccessCloudIdentifier::New(
                                provider_name, "root-id"));
}

IN_PROC_BROWSER_TEST_F(GetProvidedFsCloudIdentifierBrowserTest,
                       GetHandleError) {
  // TODO(https://crbug.com/40804030): Remove this when updated to use MV3.
  extensions::ScopedTestMV2Enabler mv2_enabler;

  base::WeakPtr<file_manager::Volume> fsp_volume =
      file_manager::test::InstallFileSystemProviderChromeApp(
          browser()->profile());

  // Unexpected type (expect dir for file) should fail.
  base::FilePath file_1_path =
      fsp_volume->mount_path().AppendASCII("readonly.txt");
  const storage::FileSystemURL file_1_url =
      GetFileSystemURL(GetProfile(), GetVirtualPath(file_1_path));
  auto [file_1_error, file_1_handles] = GetCloudIdentifierBlocking(
      file_1_url,
      content::FileSystemAccessPermissionContext::HandleType::kDirectory);
  EXPECT_NE(file_1_error->status, blink::mojom::FileSystemAccessStatus::kOk);

  // Unexpected type (expect file for dir) should fail.
  base::FilePath dir_path = fsp_volume->mount_path();
  const storage::FileSystemURL dir_url =
      GetFileSystemURL(GetProfile(), GetVirtualPath(dir_path));
  auto [dir_error, dir_handles] = GetCloudIdentifierBlocking(
      dir_url, content::FileSystemAccessPermissionContext::HandleType::kFile);
  EXPECT_NE(dir_error->status, blink::mojom::FileSystemAccessStatus::kOk);

  // readonly.png has no cloud identifier.
  base::FilePath file_2_path =
      fsp_volume->mount_path().AppendASCII("readonly.png");
  const storage::FileSystemURL file_2_url =
      GetFileSystemURL(GetProfile(), GetVirtualPath(file_2_path));
  auto [file_2_error, file_2_handles] = GetCloudIdentifierBlocking(
      file_2_url,
      content::FileSystemAccessPermissionContext::HandleType::kFile);
  EXPECT_NE(file_2_error->status, blink::mojom::FileSystemAccessStatus::kOk);

  // Non-existent files should fail.
  const storage::FileSystemURL non_existant_file_url = GetFileSystemURL(
      GetProfile(),
      GetVirtualPath(file_1_path).DirName().Append("does-not-exist"));
  auto [non_existant_file_error, non_existant_file_handles] =
      GetCloudIdentifierBlocking(
          non_existant_file_url,
          content::FileSystemAccessPermissionContext::HandleType::kFile);
  EXPECT_NE(non_existant_file_error->status,
            blink::mojom::FileSystemAccessStatus::kOk);
}

// TODO(crbug.com/434161032): Add end-to-end integration test calling JavaScript
// APIs.

}  // namespace
