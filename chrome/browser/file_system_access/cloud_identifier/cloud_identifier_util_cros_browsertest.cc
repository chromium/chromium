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
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/fake_file_system_access_permission_context.h"
#include "content/public/test/file_system_chooser_test_helpers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "storage/browser/file_system/external_mount_points.h"
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

    storage::ExternalMountPoints* mount_points =
        storage::ExternalMountPoints::GetSystemInstance();
    ASSERT_TRUE(mount_points->RegisterFileSystem(
        "local_fs_mount", storage::kFileSystemTypeLocal,
        storage::FileSystemMountOption(), local_fs_dir_.GetPath()));

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
};

IN_PROC_BROWSER_TEST_F(FileSystemAccessCloudIdentifierDelegateCrosBrowsertest,
                       LocalFileAndFolder) {
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(),
                            embedded_test_server()->GetURL("/title1.html")));

  // Local-FS
  const base::FilePath local_fs_file_virtual_path =
      CreateTestFile(local_fs_dir_);
  const base::FilePath local_fs_dir_virtual_path = CreateTestDir(local_fs_dir_);
  EXPECT_EQ(base::Value(base::Value::Type::LIST),
            GetCloudFileHandleForFile(local_fs_file_virtual_path));
  EXPECT_EQ(base::Value(base::Value::Type::LIST),
            GetCloudFileHandleForDir(local_fs_dir_virtual_path));
}

}  // anonymous namespace
