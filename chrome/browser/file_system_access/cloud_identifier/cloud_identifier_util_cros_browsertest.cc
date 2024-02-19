// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/file_system_access/cloud_identifier/cloud_identifier_util_cros.h"

#include <optional>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_reader.h"
#include "base/test/gmock_callback_support.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/crosapi/mojom/file_system_access_cloud_identifier.mojom.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/fake_file_system_access_permission_context.h"
#include "content/public/test/file_system_chooser_test_helpers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/selected_file_info.h"

namespace {

base::FilePath GetVirtualPath(const base::FilePath& absolute_path) {
  base::FilePath virtual_path;
  EXPECT_TRUE(storage::ExternalMountPoints::GetSystemInstance()->GetVirtualPath(
      absolute_path, &virtual_path));
  return virtual_path;
}

class MockFileSystemAccessCloudIdentifierProvider
    : public crosapi::mojom::FileSystemAccessCloudIdentifierProvider {
 public:
  MockFileSystemAccessCloudIdentifierProvider() = default;
  MockFileSystemAccessCloudIdentifierProvider(
      const MockFileSystemAccessCloudIdentifierProvider&) = delete;
  MockFileSystemAccessCloudIdentifierProvider& operator=(
      const MockFileSystemAccessCloudIdentifierProvider&) = delete;
  ~MockFileSystemAccessCloudIdentifierProvider() override = default;

  void BindReceiver(
      mojo::PendingReceiver<
          crosapi::mojom::FileSystemAccessCloudIdentifierProvider> receiver) {
    receivers_.Add(this, std::move(receiver));
  }

  // crosapi::mojom::FileSystemAccessCloudIdentifierProvider:
  MOCK_METHOD3(GetCloudIdentifier,
               void(const base::FilePath& virtual_path,
                    crosapi::mojom::HandleType handle_type,
                    GetCloudIdentifierCallback callback));

 private:
  mojo::ReceiverSet<crosapi::mojom::FileSystemAccessCloudIdentifierProvider>
      receivers_;
};

class FileSystemAccessCloudIdentifierDelegateCrosBrowsertest
    : public InProcessBrowserTest {
 public:
  FileSystemAccessCloudIdentifierDelegateCrosBrowsertest() {
    scoped_feature_list_.InitAndEnableFeature(
        blink::features::kFileSystemAccessGetCloudIdentifiers);
  }

  void SetUp() override {
    // Set up local file system.
    ASSERT_TRUE(local_fs_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(drive_fs_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(provided_fs_dir_.CreateUniqueTempDir());

    storage::ExternalMountPoints* mount_points =
        storage::ExternalMountPoints::GetSystemInstance();
    ASSERT_TRUE(mount_points->RegisterFileSystem(
        "local_fs_mount", storage::kFileSystemTypeLocal,
        storage::FileSystemMountOption(), local_fs_dir_.GetPath()));
    ASSERT_TRUE(mount_points->RegisterFileSystem(
        "drive_fs_mount", storage::kFileSystemTypeDriveFs,
        storage::FileSystemMountOption(), drive_fs_dir_.GetPath()));
    ASSERT_TRUE(mount_points->RegisterFileSystem(
        "provided_fs_mount", storage::kFileSystemTypeProvided,
        storage::FileSystemMountOption(), provided_fs_dir_.GetPath()));

    cloud_identifier::SetCloudIdentifierProviderForTesting(
        &mock_cloud_identifier_provider_);

    ASSERT_TRUE(embedded_test_server()->Start());

    // Must run after our setup because it actually runs the test.
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    // Required to bypass permission prompt for directory picker.
    content::SetFileSystemAccessPermissionContext(browser()->profile(),
                                                  &permission_context_);

    InProcessBrowserTest::SetUpOnMainThread();
  }

 protected:
  base::FilePath CreateTestFile(base::ScopedTempDir& parent_dir) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::FilePath result;
    EXPECT_TRUE(base::CreateTemporaryFileInDir(parent_dir.GetPath(), &result));
    return GetVirtualPath(result);
  }

  base::FilePath CreateTestDir(base::ScopedTempDir& parent_dir) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::FilePath result;
    EXPECT_TRUE(base::CreateTemporaryDirInDir(
        parent_dir.GetPath(), FILE_PATH_LITERAL("test"), &result));
    return GetVirtualPath(result);
  }

  void SetFakeFilePicker(const base::FilePath& virtual_path) {
    ui::SelectedFileInfo selected_file = {base::FilePath(), base::FilePath()};
    selected_file.virtual_path = virtual_path;
    ui::SelectFileDialog::SetFactory(
        std::make_unique<content::FakeSelectFileDialogFactory>(
            std::vector<ui::SelectedFileInfo>{selected_file}, nullptr));
  }

  content::EvalJsResult GetCloudFileHandleForFile(
      const base::FilePath& virtual_file_path) {
    SetFakeFilePicker(virtual_file_path);
    static constexpr char script[] = R"(
      (async () => {
        const [file_handle] = await self.showOpenFilePicker();
        return await file_handle.getCloudIdentifiers();
      })()
    )";
    return EvalJs(GetActiveWebContents(), script);
  }

  content::EvalJsResult GetCloudFileHandleForDir(
      const base::FilePath& virtual_dir_path) {
    SetFakeFilePicker(virtual_dir_path);
    static constexpr char script[] = R"(
      (async () => {
        const directory_handle = await self.showDirectoryPicker();
        return await directory_handle.getCloudIdentifiers();
      })()
    )";
    return EvalJs(GetActiveWebContents(), script);
  }

  content::WebContents* GetActiveWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  base::test::ScopedFeatureList scoped_feature_list_;

  content::FakeFileSystemAccessPermissionContext permission_context_;

  base::ScopedTempDir local_fs_dir_;
  base::ScopedTempDir drive_fs_dir_;
  base::ScopedTempDir provided_fs_dir_;
  testing::StrictMock<MockFileSystemAccessCloudIdentifierProvider>
      mock_cloud_identifier_provider_;
};

IN_PROC_BROWSER_TEST_F(FileSystemAccessCloudIdentifierDelegateCrosBrowsertest,
                       LocalFileAndFolder) {
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(),
                            embedded_test_server()->GetURL("/title1.html")));

  // Local-FS
  const base::FilePath local_fs_file_virtual_path =
      CreateTestFile(local_fs_dir_);
  const base::FilePath local_fs_dir_virtual_path = CreateTestDir(local_fs_dir_);
  EXPECT_CALL(mock_cloud_identifier_provider_,
              GetCloudIdentifier(local_fs_file_virtual_path,
                                 crosapi::mojom::HandleType::kFile,
                                 base::test::IsNotNullCallback()))
      .Times(0);
  EXPECT_CALL(mock_cloud_identifier_provider_,
              GetCloudIdentifier(local_fs_dir_virtual_path,
                                 crosapi::mojom::HandleType::kDirectory,
                                 base::test::IsNotNullCallback()))
      .Times(0);
  EXPECT_EQ(base::Value(base::Value::Type::LIST),
            GetCloudFileHandleForFile(local_fs_file_virtual_path));
  EXPECT_EQ(base::Value(base::Value::Type::LIST),
            GetCloudFileHandleForDir(local_fs_dir_virtual_path));
  testing::Mock::VerifyAndClearExpectations(&mock_cloud_identifier_provider_);

  // Drive-FS
  const base::FilePath drive_fs_file_virtual_path =
      CreateTestFile(drive_fs_dir_);
  const base::FilePath drive_fs_dir_virtual_path = CreateTestDir(drive_fs_dir_);
  EXPECT_CALL(mock_cloud_identifier_provider_,
              GetCloudIdentifier(drive_fs_file_virtual_path,
                                 crosapi::mojom::HandleType::kFile,
                                 base::test::IsNotNullCallback()))
      .Times(1)
      .WillOnce(base::test::RunOnceCallback<2>(
          crosapi::mojom::FileSystemAccessCloudIdentifier::New(
              "drive-fs-provider-name", "drive-fs-file-item-id")));
  EXPECT_CALL(mock_cloud_identifier_provider_,
              GetCloudIdentifier(drive_fs_dir_virtual_path,
                                 crosapi::mojom::HandleType::kDirectory,
                                 base::test::IsNotNullCallback()))
      .Times(1)
      .WillOnce(base::test::RunOnceCallback<2>(
          crosapi::mojom::FileSystemAccessCloudIdentifier::New(
              "drive-fs-provider-name", "drive-fs-dir-item-id")));
  const std::optional<base::Value> expected_drive_fs_file_result =
      base::JSONReader::Read(R"(
        [{
          "id": "drive-fs-file-item-id",
          "providerName": "drive-fs-provider-name"
        }]
      )");
  const std::optional<base::Value> expected_drive_fs_dir_result =
      base::JSONReader::Read(R"(
        [{
          "id": "drive-fs-dir-item-id",
          "providerName": "drive-fs-provider-name"
        }]
      )");
  ASSERT_TRUE(expected_drive_fs_file_result);
  ASSERT_TRUE(expected_drive_fs_dir_result);
  EXPECT_EQ(expected_drive_fs_file_result.value(),
            GetCloudFileHandleForFile(drive_fs_file_virtual_path));
  EXPECT_EQ(expected_drive_fs_dir_result.value(),
            GetCloudFileHandleForDir(drive_fs_dir_virtual_path));
  testing::Mock::VerifyAndClearExpectations(&mock_cloud_identifier_provider_);

  // Provided-FS
  const base::FilePath provided_fs_file_virtual_path =
      CreateTestFile(provided_fs_dir_);
  const base::FilePath provided_fs_dir_virtual_path =
      CreateTestDir(provided_fs_dir_);
  EXPECT_CALL(mock_cloud_identifier_provider_,
              GetCloudIdentifier(provided_fs_file_virtual_path,
                                 crosapi::mojom::HandleType::kFile,
                                 base::test::IsNotNullCallback()))
      .Times(1)
      .WillOnce(base::test::RunOnceCallback<2>(
          crosapi::mojom::FileSystemAccessCloudIdentifier::New(
              "provided-fs-provider-name", "provided-fs-file-item-id")));
  EXPECT_CALL(mock_cloud_identifier_provider_,
              GetCloudIdentifier(provided_fs_dir_virtual_path,
                                 crosapi::mojom::HandleType::kDirectory,
                                 base::test::IsNotNullCallback()))
      .Times(1)
      .WillOnce(base::test::RunOnceCallback<2>(
          crosapi::mojom::FileSystemAccessCloudIdentifier::New(
              "provided-fs-provider-name", "provided-fs-dir-item-id")));
  const std::optional<base::Value> expected_provided_fs_file_result =
      base::JSONReader::Read(R"(
        [{
          "id": "provided-fs-file-item-id",
          "providerName": "provided-fs-provider-name"
        }]
      )");
  const std::optional<base::Value> expected_provided_fs_dir_result =
      base::JSONReader::Read(R"(
        [{
          "id": "provided-fs-dir-item-id",
          "providerName": "provided-fs-provider-name"
        }]
      )");
  ASSERT_TRUE(expected_provided_fs_file_result);
  ASSERT_TRUE(expected_provided_fs_dir_result);
  EXPECT_EQ(expected_provided_fs_file_result.value(),
            GetCloudFileHandleForFile(provided_fs_file_virtual_path));
  EXPECT_EQ(expected_provided_fs_dir_result.value(),
            GetCloudFileHandleForDir(provided_fs_dir_virtual_path));
  testing::Mock::VerifyAndClearExpectations(&mock_cloud_identifier_provider_);
}

}  // anonymous namespace
