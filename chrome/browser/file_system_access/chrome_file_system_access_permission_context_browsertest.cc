// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/file_system_access/chrome_file_system_access_permission_context.h"

#include <tuple>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_file_util.h"
#include "base/test/test_future.h"
#include "chrome/browser/apps/platform_apps/app_browsertest_util.h"
#include "chrome/browser/file_system_access/file_system_access_features.h"
#include "chrome/browser/file_system_access/file_system_access_permission_request_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/file_system_chooser_test_helpers.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/update_user_activation_state_interceptor.h"
#include "extensions/test/extension_test_message_listener.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features_generated.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/select_file_dialog_factory.h"
#include "ui/shell_dialogs/select_file_policy.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/apps/app_service/chrome_app_deprecation/chrome_app_deprecation.h"
#endif

namespace {

class TestFileSystemAccessPermissionContext
    : public ChromeFileSystemAccessPermissionContext {
 public:
  explicit TestFileSystemAccessPermissionContext(
      content::BrowserContext* context)
      : ChromeFileSystemAccessPermissionContext(context) {}
  ~TestFileSystemAccessPermissionContext() override = default;

  // ChromeFileSystemAccessPermissionContext:
  void PerformAfterWriteChecks(
      std::unique_ptr<content::FileSystemAccessWriteItem> item,
      content::GlobalRenderFrameHostId frame_id,
      base::OnceCallback<void(AfterWriteCheckResult)> callback) override {
    // Call the callback with `kBlock` to handle the close callback immediately.
    std::move(callback).Run(AfterWriteCheckResult::kBlock);

    content::RenderFrameHost* rfh = content::RenderFrameHost::FromID(frame_id);
    EXPECT_TRUE(rfh->IsActive());
    performed_after_write_checks_ = true;
    if (quit_callback_) {
      std::move(quit_callback_).Run();
    }
  }

  bool performed_after_write_checks() { return performed_after_write_checks_; }

  void WaitForPerformAfterWriteChecks() {
    if (performed_after_write_checks_) {
      return;
    }

    base::RunLoop run_loop;
    quit_callback_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  void ConfirmSensitiveEntryAccess(
      const url::Origin& origin,
      const content::PathInfo& path_info,
      HandleType handle_type,
      UserAction user_action,
      content::GlobalRenderFrameHostId frame_id,
      base::OnceCallback<void(SensitiveEntryResult)> callback) override {
    confirm_sensitive_entry_access_ = true;
    if (auto_abort_on_confirm_sensitive_entry_access_) {
      std::move(callback).Run(SensitiveEntryResult::kAbort);
      return;
    }
    ChromeFileSystemAccessPermissionContext::ConfirmSensitiveEntryAccess(
        origin, path_info, handle_type, user_action, frame_id,
        std::move(callback));
  }

  bool confirm_sensitive_entry_access() const {
    return confirm_sensitive_entry_access_;
  }

  void set_auto_abort_on_confirm_sensitive_entry_access() {
    auto_abort_on_confirm_sensitive_entry_access_ = true;
  }

  void reset() {
    performed_after_write_checks_ = false;
    confirm_sensitive_entry_access_ = false;
  }

 private:
  bool performed_after_write_checks_ = false;
  bool confirm_sensitive_entry_access_ = false;
  bool auto_abort_on_confirm_sensitive_entry_access_ = false;
  base::OnceClosure quit_callback_;
};

}  // anonymous namespace

class ChromeFileSystemAccessPermissionContextBrowserTestBase
    : public InProcessBrowserTest {
 public:
  ChromeFileSystemAccessPermissionContextBrowserTestBase() = default;
  ~ChromeFileSystemAccessPermissionContextBrowserTestBase() override = default;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
    permission_context_ =
        std::make_unique<ChromeFileSystemAccessPermissionContext>(
            browser()->profile());
    content::SetFileSystemAccessPermissionContext(browser()->profile(),
                                                  permission_context_.get());
  }

  void TearDown() override {
    InProcessBrowserTest::TearDown();
    ASSERT_TRUE(temp_dir_.Delete());
    permission_context_.reset();
  }

 protected:
  base::FilePath CreateTestFile(const std::string& contents) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::FilePath result;
    EXPECT_TRUE(base::CreateTemporaryFileInDir(temp_dir_.GetPath(), &result));
    EXPECT_TRUE(base::WriteFile(result, contents));
    return result;
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  url::Origin GetOrigin() {
    return GetWebContents()->GetPrimaryMainFrame()->GetLastCommittedOrigin();
  }

  base::ScopedTempDir& temp_dir() { return temp_dir_; }

  ChromeFileSystemAccessPermissionContext* permission_context() {
    return permission_context_.get();
  }

  // Verifies the read, write, and extended permissions for a given `path` and
  // `handle_type`.
  void VerifyPermissions(
      const url::Origin& origin,
      const base::FilePath& path,
      ChromeFileSystemAccessPermissionContext::HandleType handle_type,
      content::PermissionStatus expected_read_status,
      content::PermissionStatus expected_write_status,
      bool expected_extended_read,
      bool expected_extended_write) {
    // Checks permissions.
    auto read_grant = permission_context()->GetReadPermissionGrant(
        origin, content::PathInfo(path), handle_type,
        ChromeFileSystemAccessPermissionContext::UserAction::kNone);
    EXPECT_EQ(read_grant->GetStatus(), expected_read_status);

    auto write_grant = permission_context()->GetWritePermissionGrant(
        origin, content::PathInfo(path), handle_type,
        ChromeFileSystemAccessPermissionContext::UserAction::kNone);
    EXPECT_EQ(write_grant->GetStatus(), expected_write_status);

    // Checks extended permissions.
    EXPECT_EQ(permission_context()->HasExtendedPermissionForTesting(
                  origin, content::PathInfo(path), handle_type,
                  ChromeFileSystemAccessPermissionContext::GrantType::kRead),
              expected_extended_read);
    EXPECT_EQ(permission_context()->HasExtendedPermissionForTesting(
                  origin, content::PathInfo(path), handle_type,
                  ChromeFileSystemAccessPermissionContext::GrantType::kWrite),
              expected_extended_write);
  }

  // Sets up the test environment with a file handle.
  // This includes creating a test file with at `path_to_verify`, setting up a
  // fake file picker, obtaining a file handle with `handle_name`, and verifying
  // initial read/write permissions.
  void SetUpAndGetHandleWithInitialPermissions(
      const std::string& handle_name,
      const base::FilePath& path_to_verify,
      bool expect_extended_grants = false) {
    ui::SelectFileDialog::SetFactory(
        std::make_unique<content::FakeSelectFileDialogFactory>(
            std::vector<base::FilePath>{path_to_verify}));

    // Auto-grant permissions.
    FileSystemAccessPermissionRequestManager::FromWebContents(GetWebContents())
        ->set_auto_response_for_test(permissions::PermissionAction::GRANTED);

    // Get a handle via showSaveFilePicker. This should grant read/write.
    ASSERT_TRUE(
        content::ExecJs(GetWebContents(), base::StringPrintf(R"(
      (async () => {  self.%s = await self.showSaveFilePicker(); })()
    )",
                                                             handle_name)));

    // Verify initial permissions are granted.
    const url::Origin origin = GetOrigin();
    VerifyPermissions(
        origin, path_to_verify,
        ChromeFileSystemAccessPermissionContext::HandleType::kFile,
        content::PermissionStatus::GRANTED, content::PermissionStatus::GRANTED,
        /*expected_extended_read=*/expect_extended_grants,
        /*expected_extended_write=*/expect_extended_grants);
  }

  base::ScopedTempDir temp_dir_;

 private:
  std::unique_ptr<ChromeFileSystemAccessPermissionContext> permission_context_;
};

class ChromeFileSystemAccessPermissionContextBrowserTest
    : public ChromeFileSystemAccessPermissionContextBrowserTestBase {
 public:
  ChromeFileSystemAccessPermissionContextBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kFileSystemAccessPersistentPermissions,
         features::kFileSystemAccessMoveWithOverwrite},
        {});
  }
  ~ChromeFileSystemAccessPermissionContextBrowserTest() override = default;

 protected:
  // Removes the file via the handle.
  void RemoveFile(const std::string& handle_name) {
    // Remove the file via the handle.
    ASSERT_TRUE(
        content::ExecJs(GetWebContents(), base::StringPrintf(R"((async () => {
          await self.%s.remove();
        })())",
                                                             handle_name)));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that moving a file to a destination with a pre-existing permission
// grant should work correctly.
IN_PROC_BROWSER_TEST_F(ChromeFileSystemAccessPermissionContextBrowserTest,
                       Move_FileDestinationPermissionExists) {
  // Navigate to a test page.
  const GURL url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Create a file under a directory, and get their handles.
  const base::FilePath dir_path = temp_dir().GetPath();
  const base::FilePath test_file_path = dir_path.AppendASCII("test.txt");
  SetUpAndGetHandleWithInitialPermissions("handle", test_file_path);

  // Remove the file.
  RemoveFile("handle");

  // Create a new file handle at a different path.
  const base::FilePath test_file_path2 = CreateTestFile("test file contents");
  SetUpAndGetHandleWithInitialPermissions("handle2", test_file_path2);

  // Move the new file to the removed file path which is under the target
  // directory `dir_path`.
  ui::SelectFileDialog::SetFactory(
      std::make_unique<content::FakeSelectFileDialogFactory>(
          std::vector<base::FilePath>{dir_path}));
  ASSERT_TRUE(content::ExecJs(GetWebContents(), R"((async () => {
        self.dirHandle = await self.showDirectoryPicker({mode: 'readwrite'});
      })())"));

  ASSERT_TRUE(content::ExecJs(GetWebContents(), R"((async () => {
        await self.handle2.move(self.dirHandle, 'test.txt');
      })())"));

  // Not verifying any permissions, but the test should end without crashing.

  ui::SelectFileDialog::SetFactory(nullptr);
}

// Tests that renaming a file to a destination with a pre-existing permission
// grant should work correctly.
IN_PROC_BROWSER_TEST_F(ChromeFileSystemAccessPermissionContextBrowserTest,
                       Rename_FileDestinationPermissionExists) {
  // Navigate to a test page.
  const GURL url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Create a file and get its handle.
  const base::FilePath test_file_path = CreateTestFile("test file contents");
  SetUpAndGetHandleWithInitialPermissions("handle", test_file_path);

  // Remove the file.
  RemoveFile("handle");

  // Create a new file handle at a different path.
  const base::FilePath test_file_path2 = CreateTestFile("test file contents 2");
  SetUpAndGetHandleWithInitialPermissions("handle2", test_file_path2);

  // Rename the new file to the removed file path.
  ASSERT_TRUE(content::ExecJs(
      GetWebContents(),
      content::JsReplace(R"((async () => {
        await self.handle2.move($1);
      })())",
                         test_file_path.BaseName().AsUTF8Unsafe())));

  // Not verifying any permissions, but the test should end without crashing.

  ui::SelectFileDialog::SetFactory(nullptr);
}

class ChromeFileSystemAccessPermissionContextRevokeAndRestoreBrowserTest
    : public ChromeFileSystemAccessPermissionContextBrowserTestBase {
 public:
  ChromeFileSystemAccessPermissionContextRevokeAndRestoreBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {blink::features::kFileSystemAccessWriteMode,
         blink::features::kFileSystemAccessRevokeReadOnRemove,
         features::kFileSystemAccessMoveWithOverwrite},
        {});
  }
  ~ChromeFileSystemAccessPermissionContextRevokeAndRestoreBrowserTest()
      override = default;

 protected:
  // Removes the file via the handle and verifies that read permission is
  // revoked while write permission is retained.
  void RemoveFileAndVerifyPermissionsRevoked(
      const std::string& handle_name,
      const url::Origin& origin,
      const base::FilePath& path,
      bool expect_extended_write = false) {
    // Remove the file via the handle.
    ASSERT_TRUE(content::ExecJs(
        GetWebContents(),
        base::StringPrintf("(async () => { await self.%s.remove(); })()",
                           handle_name)));

    // After removal, only read permission should be revoked.
    VerifyPermissions(
        origin, path,
        ChromeFileSystemAccessPermissionContext::HandleType::kFile,
        content::PermissionStatus::DENIED, content::PermissionStatus::GRANTED,
        /*expected_extended_read=*/false,
        /*expected_extended_write=*/expect_extended_write);
    ASSERT_TRUE(permission_context()->IsPathInDowngradedReadPathsForTesting(
        origin, path));
  }

  // Verifies that both read and write permissions are restored for a given
  // path and that the path is no longer marked as having a downgraded read
  // permission.
  void VerifyPermissionsRestored(const url::Origin& origin,
                                 const base::FilePath& path,
                                 bool expect_extended_grants = false) {
    VerifyPermissions(
        origin, path,
        ChromeFileSystemAccessPermissionContext::HandleType::kFile,
        content::PermissionStatus::GRANTED, content::PermissionStatus::GRANTED,
        /*expected_extended_read=*/expect_extended_grants,
        /*expected_extended_write=*/expect_extended_grants);
    EXPECT_FALSE(permission_context()->IsPathInDowngradedReadPathsForTesting(
        origin, path));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that fileHandle.remove() on a file with readwrite permission DOES
// revoke the read permission for the file, but not the write permission.
IN_PROC_BROWSER_TEST_F(
    ChromeFileSystemAccessPermissionContextRevokeAndRestoreBrowserTest,
    RemoveFile_RevokesReadInFileHandlePermissionOnly) {
  const base::FilePath test_file = CreateTestFile("test file contents");
  ui::SelectFileDialog::SetFactory(
      std::make_unique<content::FakeSelectFileDialogFactory>(
          std::vector<base::FilePath>{test_file}));

  // Auto-grant permissions.
  FileSystemAccessPermissionRequestManager::FromWebContents(GetWebContents())
      ->set_auto_response_for_test(permissions::PermissionAction::GRANTED);

  // Navigate to a test page.
  const GURL url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Get a handle via showSaveFilePicker. This should grant read/write.
  // Note that because a fake file picker factory was installed, this should
  // result in the `test_file` being picked without needs for user interaction.
  ASSERT_TRUE(content::ExecJs(GetWebContents(),
                              "(async () => {"
                              "  self.handle = await self.showSaveFilePicker();"
                              "})()"));

  // Verify initial permissions are granted, including extended permissions.
  EXPECT_EQ("granted", content::EvalJs(GetWebContents(), R"((async () => {
             return await self.handle.queryPermission({mode: 'readwrite'});
            })())"));
  const url::Origin origin = GetOrigin();
  permission_context()->SetOriginHasExtendedPermissionForTesting(origin);
  VerifyPermissions(origin, test_file,
                    ChromeFileSystemAccessPermissionContext::HandleType::kFile,
                    content::PermissionStatus::GRANTED,
                    content::PermissionStatus::GRANTED,
                    /*expected_extended_read=*/true,
                    /*expected_extended_write=*/true);

  // Verify that the path is not added to downgraded_read_paths yet.
  ASSERT_FALSE(permission_context()->IsPathInDowngradedReadPathsForTesting(
      origin, test_file));

  // Remove the file via the handle.
  ASSERT_TRUE(content::ExecJs(GetWebContents(), "self.handle.remove()"));

  // After removal, only read permission should be revoked.
  VerifyPermissions(origin, test_file,
                    ChromeFileSystemAccessPermissionContext::HandleType::kFile,
                    content::PermissionStatus::DENIED,
                    content::PermissionStatus::GRANTED,
                    /*expected_extended_read=*/false,
                    /*expected_extended_write=*/true);

  // Verify that the path is added to downgraded_read_paths.
  EXPECT_TRUE(permission_context()->IsPathInDowngradedReadPathsForTesting(
      origin, test_file));

  // Verify that the querying readwrite and read permission via file handle is
  // now denied instead of granted.
  // TODO(crbug.com/328458680): Query 'write' permission once it's added.
  EXPECT_EQ("denied", content::EvalJs(GetWebContents(), R"((async () => {
             return await self.handle.queryPermission({mode: 'readwrite'});
            })())"));
  EXPECT_EQ("denied", content::EvalJs(GetWebContents(), R"((async () => {
             return await self.handle.queryPermission({mode: 'read'});
            })())"));

  ui::SelectFileDialog::SetFactory(nullptr);
}

// Tests that after fileHandle.remove() is called, both the initial file handle
// and a copy of the handle retrieved from IndexedDB have their read permissions
// revoked.
IN_PROC_BROWSER_TEST_F(
    ChromeFileSystemAccessPermissionContextRevokeAndRestoreBrowserTest,
    RemoveFile_RevokesReadPermissionForIndexedDBCopy) {
  const base::FilePath test_file = CreateTestFile("test file contents");
  ui::SelectFileDialog::SetFactory(
      std::make_unique<content::FakeSelectFileDialogFactory>(
          std::vector<base::FilePath>{test_file}));

  // Auto-grant permissions.
  FileSystemAccessPermissionRequestManager::FromWebContents(GetWebContents())
      ->set_auto_response_for_test(permissions::PermissionAction::GRANTED);

  // Navigate to a test page.
  const GURL url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // 1. Setup IndexedDB helpers.
  ASSERT_TRUE(content::ExecJs(GetWebContents(), R"(
      const db = new Promise((resolve, reject) => {
        const req = indexedDB.open('test-db');
        req.onupgradeneeded = () => req.result.createObjectStore('store');
        req.onsuccess = () => resolve(req.result);
        req.onerror = () => reject(req.error);
      });
      self.get = async function(key) {
        const store = (await db).transaction('store').objectStore('store');
        return new Promise((resolve, reject) => {
          const req = store.get(key);
          req.onsuccess = () => resolve(req.result);
          req.onerror = () => reject(req.error);
        });
      }
      self.set = async function(key, value) {
        const store = (await db)
            .transaction('store', 'readwrite')
            .objectStore('store');
        store.put(value, key);
        return new Promise((resolve, reject) => {
          store.transaction.oncomplete = () => resolve();
          store.transaction.onerror = () => reject(store.transaction.error);
        });
      }
    )"));

  // 2. Get a handle via showSaveFilePicker and verify initial permissions.
  ASSERT_TRUE(content::ExecJs(
      GetWebContents(),
      "(async () => { self.handle1 = await self.showSaveFilePicker(); })()"));
  EXPECT_EQ("granted", content::EvalJs(GetWebContents(), R"((async () => {
             return await self.handle1.queryPermission({mode: 'readwrite'});
            })())"));

  // 3. Store the handle in IndexedDB, retrieve a copy and verify initial
  // permissions.
  ASSERT_TRUE(content::ExecJs(
      GetWebContents(),
      "(async () => { await self.set('fileHandle', self.handle1); })()"));
  ASSERT_TRUE(content::ExecJs(
      GetWebContents(),
      "(async () => { self.handle2 = await self.get('fileHandle'); })()"));
  EXPECT_EQ("granted", content::EvalJs(GetWebContents(), R"((async () => {
             return await self.handle2.queryPermission({mode: 'readwrite'});
            })())"));

  // 4. Remove the file via the initial handle.
  ASSERT_TRUE(content::ExecJs(GetWebContents(), "self.handle1.remove()"));

  // 5. Verify that the permissions for both handles are now 'denied'.
  EXPECT_EQ("denied", content::EvalJs(GetWebContents(), R"((async () => {
             return await self.handle1.queryPermission({mode: 'readwrite'});
            })())"));
  EXPECT_EQ("denied", content::EvalJs(GetWebContents(), R"((async () => {
             return await self.handle1.queryPermission({mode: 'read'});
            })())"));
  EXPECT_EQ("denied", content::EvalJs(GetWebContents(), R"((async () => {
             return await self.handle2.queryPermission({mode: 'readwrite'});
            })())"));
  EXPECT_EQ("denied", content::EvalJs(GetWebContents(), R"((async () => {
             return await self.handle2.queryPermission({mode: 'read'});
            })())"));

  // Verify that the path is added to downgraded_read_paths.
  const url::Origin origin = GetOrigin();
  EXPECT_TRUE(permission_context()->IsPathInDowngradedReadPathsForTesting(
      origin, test_file));

  ui::SelectFileDialog::SetFactory(nullptr);
}

// Tests that fileHandle.remove() on a file from a directory with readwrite
// permission does NOT revoke the read permission for the file.
IN_PROC_BROWSER_TEST_F(
    ChromeFileSystemAccessPermissionContextRevokeAndRestoreBrowserTest,
    RemoveFileInReadWriteDirectory_DoesNotRevokePermissions) {
  // Create a directory and a file inside it.
  const base::FilePath test_file = CreateTestFile("test file contents");
  const base::FilePath test_dir = temp_dir().GetPath();

  ui::SelectFileDialog::SetFactory(
      std::make_unique<content::FakeSelectFileDialogFactory>(
          std::vector<base::FilePath>{test_dir}));

  // Auto-grant permissions.
  // NOTE: This only works for operations not requiring UserActivation.
  FileSystemAccessPermissionRequestManager::FromWebContents(GetWebContents())
      ->set_auto_response_for_test(permissions::PermissionAction::GRANTED);

  // Navigate to a test page.
  const GURL url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Get a directory handle. This consumes the initial user activation.
  ASSERT_TRUE(
      content::ExecJs(GetWebContents(),
                      "(async () => {"
                      "  self.dirHandle = await self.showDirectoryPicker();"
                      "})()"));

  // Pre-request a write grant to the test directory
  // We can't use self.dirHandle.requestPermission() because there is no
  // reliable way to ensure a user activation is present at the time
  // `PermissionGrantImpl::RequestPermission()` is called.
  auto grant = permission_context()->GetWritePermissionGrant(
      GetOrigin(), content::PathInfo(test_dir),
      ChromeFileSystemAccessPermissionContext::HandleType::kDirectory,
      ChromeFileSystemAccessPermissionContext::UserAction::kOpen);
  base::test::TestFuture<
      content::FileSystemAccessPermissionGrant::PermissionRequestOutcome>
      future;
  auto* rfh = GetWebContents()->GetPrimaryMainFrame();
  grant->RequestPermission(
      content::GlobalRenderFrameHostId(rfh->GetProcess()->GetDeprecatedID(),
                                       rfh->GetRoutingID()),
      content::FileSystemAccessPermissionGrant::UserActivationState::
          kNotRequired,
      future.GetCallback());

  // Verifies in JS that the readwrite permission is granted to the directory.
  EXPECT_EQ("granted", content::EvalJs(GetWebContents(), R"((async () => {
             return await self.dirHandle.queryPermission({mode: 'readwrite'});
            })())"));

  // Verify directory permissions are granted.
  const url::Origin origin = GetOrigin();
  permission_context()->SetOriginHasExtendedPermissionForTesting(origin);
  VerifyPermissions(
      origin, test_dir,
      ChromeFileSystemAccessPermissionContext::HandleType::kDirectory,
      content::PermissionStatus::GRANTED, content::PermissionStatus::GRANTED,
      /*expected_extended_read=*/true,
      /*expected_extended_write=*/true);

  // Verify file permissions are also granted (inherited).
  VerifyPermissions(origin, test_file,
                    ChromeFileSystemAccessPermissionContext::HandleType::kFile,
                    content::PermissionStatus::GRANTED,
                    content::PermissionStatus::GRANTED,
                    /*expected_extended_read=*/true,
                    /*expected_extended_write=*/true);

  // Verify that the path is not added to downgraded_read_paths yet.
  ASSERT_FALSE(permission_context()->IsPathInDowngradedReadPathsForTesting(
      origin, test_file));

  // Get a handle to the file using the directory handle.
  ASSERT_TRUE(content::ExecJs(
      GetWebContents(),
      content::JsReplace(
          "(async () => {"
          "  self.fileHandle = await self.dirHandle.getFileHandle($1);"
          "})()",
          test_file.BaseName().AsUTF8Unsafe())));

  // Verify initial permissions are granted.
  EXPECT_EQ("granted", content::EvalJs(GetWebContents(), R"((async () => {
             return await self.fileHandle.queryPermission({mode: 'readwrite'});
            })())"));

  // Remove the file using the file handle obtained from directory handle.
  ASSERT_TRUE(content::ExecJs(GetWebContents(),
                              "(async () => {"
                              "  await self.fileHandle.remove();"
                              "})()"));

  // After removal, permissions for the directory should be unchanged.
  VerifyPermissions(
      origin, test_dir,
      ChromeFileSystemAccessPermissionContext::HandleType::kDirectory,
      content::PermissionStatus::GRANTED, content::PermissionStatus::GRANTED,
      /*expected_extended_read=*/true,
      /*expected_extended_write=*/true);

  // Permissions for the file path should also be unchanged (inherited).
  VerifyPermissions(origin, test_file,
                    ChromeFileSystemAccessPermissionContext::HandleType::kFile,
                    content::PermissionStatus::GRANTED,
                    content::PermissionStatus::GRANTED,
                    /*expected_extended_read=*/true,
                    /*expected_extended_write=*/true);

  // Verify that the removed path is NOT added to downgraded_read_paths.
  EXPECT_FALSE(permission_context()->IsPathInDowngradedReadPathsForTesting(
      origin, test_file));

  // Verify that querying the readwrite and read permission using the existing
  // file handle still returns granted, as it is inherited from the
  // still-granted parent directory.
  // TODO(crbug.com/328458680): Query 'write' permission once it's added.
  EXPECT_EQ("granted", content::EvalJs(GetWebContents(), R"((async () => {
             return await self.fileHandle.queryPermission({mode: 'readwrite'});
            })())"));
  EXPECT_EQ("granted", content::EvalJs(GetWebContents(), R"((async () => {
             return await self.fileHandle.queryPermission({mode: 'read'});
            })())"));
  // NOTE: As the file is removed, we can't check the path permission via a new
  // file handle.

  ui::SelectFileDialog::SetFactory(nullptr);
}

// Tests that dirHandle.removeEntry() on a file from a directory with readwrite
// permission does NOT revoke the read permission for the file.
IN_PROC_BROWSER_TEST_F(
    ChromeFileSystemAccessPermissionContextRevokeAndRestoreBrowserTest,
    DirectoryRemoveFile_DoesNotRevokePermissions) {
  // Create a directory and a file inside it.
  const base::FilePath test_file = CreateTestFile("test file contents");
  const base::FilePath test_dir = temp_dir().GetPath();

  ui::SelectFileDialog::SetFactory(
      std::make_unique<content::FakeSelectFileDialogFactory>(
          std::vector<base::FilePath>{test_dir}));

  // Auto-grant permissions.
  FileSystemAccessPermissionRequestManager::FromWebContents(GetWebContents())
      ->set_auto_response_for_test(permissions::PermissionAction::GRANTED);

  // Navigate to a test page.
  const GURL url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Get a directory handle and request read/write permissions.
  ASSERT_TRUE(content::ExecJs(
      GetWebContents(),
      "(async () => {"
      "  self.dirHandle = await self.showDirectoryPicker({mode: 'readwrite'});"
      "})()"));

  // Verify directory permissions are granted.
  const url::Origin origin = GetOrigin();
  permission_context()->SetOriginHasExtendedPermissionForTesting(origin);
  VerifyPermissions(
      origin, test_dir,
      ChromeFileSystemAccessPermissionContext::HandleType::kDirectory,
      content::PermissionStatus::GRANTED, content::PermissionStatus::GRANTED,
      /*expected_extended_read=*/true,
      /*expected_extended_write=*/true);

  // Verify file permissions are also granted (inherited).
  VerifyPermissions(origin, test_file,
                    ChromeFileSystemAccessPermissionContext::HandleType::kFile,
                    content::PermissionStatus::GRANTED,
                    content::PermissionStatus::GRANTED,
                    /*expected_extended_read=*/true,
                    /*expected_extended_write=*/true);

  // Verify that the path is not added to downgraded_read_paths yet.
  ASSERT_FALSE(permission_context()->IsPathInDowngradedReadPathsForTesting(
      origin, test_file));

  // Get a handle to the file using the directory handle.
  ASSERT_TRUE(content::ExecJs(
      GetWebContents(),
      content::JsReplace(
          "(async () => {"
          "  self.fileHandle = await self.dirHandle.getFileHandle($1);"
          "})()",
          test_file.BaseName().AsUTF8Unsafe())));

  // Verify initial permissions are granted.
  EXPECT_EQ("granted", content::EvalJs(GetWebContents(), R"((async () => {
             return await self.fileHandle.queryPermission({mode: 'readwrite'});
            })())"));

  // Remove the file via the directory handle.
  ASSERT_TRUE(content::ExecJs(
      GetWebContents(),
      content::JsReplace("(async () => {"
                         "  await self.dirHandle.removeEntry($1);"
                         "})()",
                         test_file.BaseName().AsUTF8Unsafe())));

  // After removal, permissions for the directory should be unchanged.
  VerifyPermissions(
      origin, test_dir,
      ChromeFileSystemAccessPermissionContext::HandleType::kDirectory,
      content::PermissionStatus::GRANTED, content::PermissionStatus::GRANTED,
      /*expected_extended_read=*/true,
      /*expected_extended_write=*/true);

  // Permissions for the file path should also be unchanged (inherited).
  VerifyPermissions(origin, test_file,
                    ChromeFileSystemAccessPermissionContext::HandleType::kFile,
                    content::PermissionStatus::GRANTED,
                    content::PermissionStatus::GRANTED,
                    /*expected_extended_read=*/true,
                    /*expected_extended_write=*/true);

  // Verify that the removed file path is NOT added to downgraded_read_paths.
  EXPECT_FALSE(permission_context()->IsPathInDowngradedReadPathsForTesting(
      origin, test_file));

  // Verify that querying the readwrite and read permission using the existing
  // file handle still returns granted, as it is inherited from the
  // still-granted parent directory.
  // TODO(crbug.com/328458680): Query 'write' permission once it's added.
  EXPECT_EQ("granted", content::EvalJs(GetWebContents(), R"((async () => {
             return await self.fileHandle.queryPermission({mode: 'readwrite'});
            })())"));
  EXPECT_EQ("granted", content::EvalJs(GetWebContents(), R"((async () => {
             return await self.fileHandle.queryPermission({mode: 'read'});
            })())"));
  // NOTE: As the file is removed, we can't check the path permission via a new
  // file handle.

  ui::SelectFileDialog::SetFactory(nullptr);
}

// Tests that after a file is written to the removed file path via
// `createWritable()`, the read permission is restored.
IN_PROC_BROWSER_TEST_F(
    ChromeFileSystemAccessPermissionContextRevokeAndRestoreBrowserTest,
    RestoreReadOnWrite_CreateWritable) {
  // Navigate to a test page.
  const GURL url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Create a file and get a handle to it.
  const base::FilePath test_file_path = CreateTestFile("test file contents");
  SetUpAndGetHandleWithInitialPermissions("handle", test_file_path);

  // Remove the file.
  const url::Origin origin = GetOrigin();
  RemoveFileAndVerifyPermissionsRevoked("handle", origin, test_file_path);

  // Write to the same file path via a new writable stream created from the
  // existing file handle.
  ASSERT_TRUE(content::ExecJs(GetWebContents(), R"((async () => {
    const w = await self.handle.createWritable();
    await w.write('new contents');
    await w.close();
  })()
  )"));

  // After writing, read permission should be restored.
  VerifyPermissionsRestored(origin, test_file_path);

  ui::SelectFileDialog::SetFactory(nullptr);
}

// Tests that after a file is moved to the removed file path, the read
// permission for the removed file path is restored.
//
// To prevent the `showDirectoryPicker` call from wiping dormant permissions
// (a security feature for new sessions), this test grants the origin "Extended
// Permission" at the beginning. This simulates the behavior of a trusted,
// installed PWA and ensures that the initial grants are preserved throughout
// the test, allowing us to verify the `move` operation's restoration logic
// on a stable set of permissions. Consequently, all permission checks from the
// beginning of the test expect auto-grantable ("extended") permissions.
IN_PROC_BROWSER_TEST_F(
    ChromeFileSystemAccessPermissionContextRevokeAndRestoreBrowserTest,
    RestoreReadOnWrite_Move) {
  // Navigate to a test page.
  const GURL url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  const url::Origin origin = GetOrigin();

  // Grant the origin Extended Permission at the start of the test.
  // Unlike `RestoreReadOnWrite_Rename` test case which doesn't use
  // `showDirectoryPicker()`, this step is necessary in this test to prevent the
  // subsequent `showDirectoryPicker()` call from wiping all pre-existing
  // "dormant" grants from the two files handles when it reaches
  // `UpdateGrantsOnPermissionRequestResult()`.
  // As a result, all extended permissions in this test are expected to be true.
  permission_context()->SetOriginHasExtendedPermissionForTesting(origin);

  // Create the 1st file handles under a directory, and get their handles.
  const base::FilePath dir_path = temp_dir().GetPath();
  const base::FilePath test_file_path = dir_path.AppendASCII("test.txt");
  SetUpAndGetHandleWithInitialPermissions("handle", test_file_path,
                                          /*expect_extended_grants=*/true);

  // Remove the 1st file.
  RemoveFileAndVerifyPermissionsRevoked("handle", origin, test_file_path,
                                        /*expect_extended_write=*/true);

  // Create the 2nd file handle at a different path.
  const base::FilePath test_file_path2 = dir_path.AppendASCII("test2.txt");
  SetUpAndGetHandleWithInitialPermissions("handle2", test_file_path2,
                                          /*expect_extended_grants=*/true);

  // Move the new file to the removed file path which is under the target
  // directory `dir_path`.
  ui::SelectFileDialog::SetFactory(
      std::make_unique<content::FakeSelectFileDialogFactory>(
          std::vector<base::FilePath>{dir_path}));
  ASSERT_TRUE(content::ExecJs(GetWebContents(), R"((async () => {
        self.dirHandle = await self.showDirectoryPicker({mode: 'readwrite'});
      })())"));
  ASSERT_TRUE(content::ExecJs(GetWebContents(), R"((async () => {
        await self.handle2.move(self.dirHandle, 'test.txt');
      })())"));

  // After moving, read permission should be restored for the previously removed
  // file path.
  VerifyPermissionsRestored(origin, test_file_path,
                            /*expect_extended_grants=*/true);

  ui::SelectFileDialog::SetFactory(nullptr);
}

// Tests that after a file is renamed to the removed file path, the read
// permission for that file path is restored.
IN_PROC_BROWSER_TEST_F(
    ChromeFileSystemAccessPermissionContextRevokeAndRestoreBrowserTest,
    RestoreReadOnWrite_Rename) {
  // Navigate to a test page.
  const GURL url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Create a file and get its handle.
  const base::FilePath test_file_path = CreateTestFile("test file contents");
  SetUpAndGetHandleWithInitialPermissions("handle", test_file_path);

  // Remove the file.
  const url::Origin origin = GetOrigin();
  RemoveFileAndVerifyPermissionsRevoked("handle", origin, test_file_path);

  // Create a new file handle at a different path.
  const base::FilePath test_file_path2 = CreateTestFile("test file contents 2");
  SetUpAndGetHandleWithInitialPermissions("handle2", test_file_path2);

  // Rename the new file to the removed file path.
  ASSERT_TRUE(content::ExecJs(
      GetWebContents(),
      content::JsReplace(R"((async () => {
        await self.handle2.move($1);
      })())",
                         test_file_path.BaseName().AsUTF8Unsafe())));

  // After renaming, read permission should be restored for the previously
  // removed file path.
  VerifyPermissionsRestored(origin, test_file_path);

  ui::SelectFileDialog::SetFactory(nullptr);
}

class ChromeFileSystemAccessPermissionContextPrerenderingBrowserTest
    : public InProcessBrowserTest {
 public:
  ChromeFileSystemAccessPermissionContextPrerenderingBrowserTest()
      : prerender_test_helper_(base::BindRepeating(
            &ChromeFileSystemAccessPermissionContextPrerenderingBrowserTest::
                GetWebContents,
            base::Unretained(this))) {}
  ~ChromeFileSystemAccessPermissionContextPrerenderingBrowserTest() override =
      default;

  void SetUp() override {
    // Create a scoped directory under %TEMP% instead of using
    // `base::ScopedTempDir::CreateUniqueTempDir`.
    // `base::ScopedTempDir::CreateUniqueTempDir` creates a path under
    // %ProgramFiles% on Windows when running as Admin, which is a blocked path
    // (`kBlockedPaths`). This can fail some of the tests.
    ASSERT_TRUE(
        temp_dir_.CreateUniqueTempDirUnderPath(base::GetTempDirForTesting()));

    prerender_test_helper_.RegisterServerRequestMonitor(embedded_test_server());
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    test_server_handle_ = embedded_test_server()->StartAndReturnHandle();
  }

  void TearDown() override {
    InProcessBrowserTest::TearDown();
    ASSERT_TRUE(temp_dir_.Delete());
  }

  base::FilePath CreateTestFile(const std::string& contents) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::FilePath result;
    EXPECT_TRUE(base::CreateTemporaryFileInDir(temp_dir_.GetPath(), &result));
    EXPECT_TRUE(base::WriteFile(result, contents));
    return result;
  }

  content::test::PrerenderTestHelper& prerender_helper() {
    return prerender_test_helper_;
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

 private:
  content::test::PrerenderTestHelper prerender_test_helper_;
  net::test_server::EmbeddedTestServerHandle test_server_handle_;
  base::ScopedTempDir temp_dir_;
};

// Tests that subscribers are notified of file creation events originating from
// `window.showSaveFilePicker()`.
IN_PROC_BROWSER_TEST_F(
    ChromeFileSystemAccessPermissionContextPrerenderingBrowserTest,
    NotifyFileCreatedFromShowSaveFilePicker) {
  // Install fake file picker factory.
  const base::FilePath expected_file_path = CreateTestFile("");
  ui::SelectFileDialog::SetFactory(
      std::make_unique<content::FakeSelectFileDialogFactory>(
          std::vector<base::FilePath>{expected_file_path}));

  // Initialize permission context.
  Profile* const profile = browser()->profile();
  TestFileSystemAccessPermissionContext permission_context(profile);
  content::SetFileSystemAccessPermissionContext(profile, &permission_context);
  FileSystemAccessPermissionRequestManager::FromWebContents(GetWebContents())
      ->set_auto_response_for_test(permissions::PermissionAction::GRANTED);

  // Subscribe to be notified of file creation events.
  base::test::TestFuture<const GURL&, const storage::FileSystemURL&>
      file_created_from_show_save_file_picker_future;
  base::CallbackListSubscription
      file_created_from_show_save_file_picker_subscription_ =
          permission_context.AddFileCreatedFromShowSaveFilePickerCallback(
              file_created_from_show_save_file_picker_future
                  .GetRepeatingCallback());

  // Navigate web contents.
  const GURL expected_url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_NE(ui_test_utils::NavigateToURL(browser(), expected_url), nullptr);

  // Invoke `window.showSaveFilePicker()` from web contents. Note that because
  // a fake file picker factory was installed, this should result in the
  // `expected_file_path` being picked without the need for user interaction.
  ASSERT_TRUE(content::ExecJs(GetWebContents(),
                              "(() => { self.showSaveFilePicker({}); })()"));

  // Wait for and verify details of the file creation event.
  auto [file_picker_binding_context, url] =
      file_created_from_show_save_file_picker_future.Take();
  EXPECT_EQ(file_picker_binding_context, expected_url);
  EXPECT_EQ(url.path(), expected_file_path);

  // Uninstall fake file picker factory.
  ui::SelectFileDialog::SetFactory(nullptr);
}

// Tests that PerformAfterWriteChecks() that is called by
// 'FileSystemWritableFileStream.close()' works with the RenderFrameHost in an
// active state, not the prerendered RenderFrameHost.
IN_PROC_BROWSER_TEST_F(
    ChromeFileSystemAccessPermissionContextPrerenderingBrowserTest,
    PerformAfterWriteChecks) {
  const base::FilePath test_file = CreateTestFile("");
  ui::SelectFileDialog::SetFactory(
      std::make_unique<content::FakeSelectFileDialogFactory>(
          std::vector<base::FilePath>{test_file}));

  TestFileSystemAccessPermissionContext permission_context(
      browser()->profile());
  content::SetFileSystemAccessPermissionContext(browser()->profile(),
                                                &permission_context);
  FileSystemAccessPermissionRequestManager::FromWebContents(GetWebContents())
      ->set_auto_response_for_test(permissions::PermissionAction::GRANTED);

  // Initial navigation.
  GURL initial_url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_NE(ui_test_utils::NavigateToURL(browser(), initial_url), nullptr);

  // Add prerendering.
  GURL prerender_url = embedded_test_server()->GetURL("/title1.html");
  content::FrameTreeNodeId host_id =
      prerender_helper().AddPrerender(prerender_url);
  content::RenderFrameHost* prerendered_frame_host =
      prerender_helper().GetPrerenderedMainFrameHost(host_id);

  // In order to get the file handle without the file picker dialog in the
  // prerendered page, BroadcastChannel gets the file handle from the current
  // active page.
  std::ignore =
      content::ExecJs(prerendered_frame_host, R"(
            var createWritableAndClose = (async () => {
              let b = new BroadcastChannel('channel');
              self.message_promise = new Promise(resolve => {
                b.onmessage = resolve;
              });
              let e = await self.message_promise;
              self.entry = e.data.entry;
              const w = await self.entry.createWritable();
              await w.write(new Blob(['hello']));
              await w.close();
              return "";})();
            )",
                      content::EvalJsOptions::EXECUTE_SCRIPT_NO_USER_GESTURE);

  // The active page picks files and sends it to the prerendered page to test
  // 'close()' in prerendering.
  std::ignore = content::ExecJs(
      GetWebContents(),
      "(async () => {"
      "  let [e] = await self.showOpenFilePicker();"
      "  self.entry = e;"
      "  new BroadcastChannel('channel').postMessage({entry: e});"
      "  return e.name; })()");

  // PerformAfterWriteChecks() is not called in prerendering.
  EXPECT_FALSE(permission_context.performed_after_write_checks());

  // Activate the prerendered page.
  prerender_helper().NavigatePrimaryPage(prerender_url);
  content::UpdateUserActivationStateInterceptor user_activation_interceptor(
      GetWebContents()->GetPrimaryMainFrame());
  user_activation_interceptor.UpdateUserActivationState(
      blink::mojom::UserActivationUpdateType::kNotifyActivation,
      blink::mojom::UserActivationNotificationType::kTest);
  permission_context.WaitForPerformAfterWriteChecks();

  // PerformAfterWriteChecks() should be called in the activated page.
  EXPECT_TRUE(permission_context.performed_after_write_checks());

  ui::SelectFileDialog::SetFactory(nullptr);
}

// Tests that ConfirmSensitiveEntryAccess() is called by
// 'FileSystemFileHandle.move()'.
IN_PROC_BROWSER_TEST_F(
    ChromeFileSystemAccessPermissionContextPrerenderingBrowserTest,
    MoveFileAndConfirmSensitiveEntryAccess) {
  const base::FilePath test_file = CreateTestFile("test.txt");
  ui::SelectFileDialog::SetFactory(
      std::make_unique<content::FakeSelectFileDialogFactory>(
          std::vector<base::FilePath>{test_file}));

  TestFileSystemAccessPermissionContext permission_context(
      browser()->profile());
  content::SetFileSystemAccessPermissionContext(browser()->profile(),
                                                &permission_context);
  FileSystemAccessPermissionRequestManager::FromWebContents(GetWebContents())
      ->set_auto_response_for_test(permissions::PermissionAction::GRANTED);

  // Initial navigation.
  GURL initial_url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_NE(ui_test_utils::NavigateToURL(browser(), initial_url), nullptr);

  // Expects no user interaction: showSaveFilePicker() automatically gets
  // `test_file` from the fake file picker factory
  // `FakeSelectFileDialogFactory` without the need for user interaction.
  ASSERT_TRUE(ExecJs(GetWebContents(),
                     R"(
    var handle;
    (async () =>{
      handle = await self.showSaveFilePicker();
    })()
  )"));
  EXPECT_EQ(test_file.BaseName().AsUTF8Unsafe(),
            EvalJs(GetWebContents(), "handle.name"));
  // Checks that PerformAfterWriteChecks() must not be called.
  EXPECT_FALSE(permission_context.performed_after_write_checks());
  // Checks that ConfirmSensitiveEntryAccess() is called within file picker,
  // i.e. FileSystemAccessManagerImpl::DidChooseEntries.
  EXPECT_TRUE(permission_context.confirm_sensitive_entry_access());

  // Resets permission_context to receive new behavior.
  permission_context.reset();

  // Calling move() with '.swf' will trigger a SafeBrowsing check after calling
  // `ConfirmSensitiveEntryAccess()`, which prompts the user to confirm saving
  // such file.

  // This line automatically aborts on calling ConfirmSensitiveEntryAccess() to
  // bypass the SafeBrowsing dialog, as there is no way to accept the prompt
  // in browser tests.
  // Commenting this out will bring up the dialog and fail the test without a
  // manual click.
  permission_context.set_auto_abort_on_confirm_sensitive_entry_access();

  EXPECT_THAT(
      EvalJs(GetWebContents(),
             R"(
      handle.move("test.swf");
  )"),
      content::EvalJsResult::ErrorIs(testing::Eq(
          "a JavaScript error: \"TypeError: Failed to execute 'move' on "
          "'FileSystemFileHandle'\"\n")));
  // Checks that ConfirmSensitiveEntryAccess() is called again to verify the
  // move target file name.
  EXPECT_TRUE(permission_context.confirm_sensitive_entry_access());

  // Uninstall fake file picker factory.
  ui::SelectFileDialog::SetFactory(nullptr);
}

class FileSystemChromeAppTest : public extensions::PlatformAppBrowserTest {
 public:
  FileSystemChromeAppTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kFileSystemAccessPersistentPermissions}, {});
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(FileSystemChromeAppTest,
                       FileSystemAccessPermissionRequestManagerExists) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ExtensionTestMessageListener launched_listener("Launched");

  // Install Platform App
  content::CreateAndLoadWebContentsObserver app_loaded_observer;
  const extensions::Extension* extension =
      InstallPlatformApp("file_system_test");
  ASSERT_TRUE(extension);

#if BUILDFLAG(IS_CHROMEOS)
  apps::chrome_app_deprecation::ScopedAddAppToAllowlistForTesting allowlist(
      extension->id());
#endif

  // Launch Platform App
  LaunchPlatformApp(extension);
  app_loaded_observer.Wait();
  ASSERT_TRUE(launched_listener.WaitUntilSatisfied());

  content::WebContents* web_contents = GetFirstAppWindowWebContents();
  EXPECT_TRUE(web_contents);
  EXPECT_NE(nullptr, FileSystemAccessPermissionRequestManager::FromWebContents(
                         web_contents));
}

IN_PROC_BROWSER_TEST_F(FileSystemChromeAppTest,
                       FileSystemAccessPersistentPermissionsPrompt) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ExtensionTestMessageListener launched_listener("Launched");

  // Install Platform App.
  content::CreateAndLoadWebContentsObserver app_loaded_observer;
  const extensions::Extension* extension =
      InstallPlatformApp("file_system_test");
  ASSERT_TRUE(extension);

#if BUILDFLAG(IS_CHROMEOS)
  apps::chrome_app_deprecation::ScopedAddAppToAllowlistForTesting allowlist(
      extension->id());
#endif

  // Launch Platform App.
  LaunchPlatformApp(extension);
  app_loaded_observer.Wait();
  ASSERT_TRUE(launched_listener.WaitUntilSatisfied());

  // Initialize permission context.
  content::WebContents* web_contents = GetFirstAppWindowWebContents();
  Profile* const profile = browser()->profile();
  TestFileSystemAccessPermissionContext permission_context(profile);
  content::SetFileSystemAccessPermissionContext(profile, &permission_context);
  FileSystemAccessPermissionRequestManager::FromWebContents(web_contents)
      ->set_auto_response_for_test(permissions::PermissionAction::GRANTED);

  // Initialize file permission grant.
  const url::Origin kTestOrigin = extension->origin();
  const content::PathInfo kTestPathInfo(FILE_PATH_LITERAL("/foo/bar"));
  auto grant = permission_context.GetReadPermissionGrant(
      kTestOrigin, kTestPathInfo,
      ChromeFileSystemAccessPermissionContext::HandleType::kFile,
      ChromeFileSystemAccessPermissionContext::UserAction::kOpen);
  EXPECT_EQ(grant->GetStatus(), content::PermissionStatus::GRANTED);

  // Dormant grants exist after tabs are backgrounded for the amount of time
  // specified by the extended permissions policy.
  permission_context.OnAllTabsInBackgroundTimerExpired(
      kTestOrigin,
      OneTimePermissionsTrackerObserver::BackgroundExpiryType::kLongTimeout);
  EXPECT_EQ(grant->GetStatus(), content::PermissionStatus::ASK);

  // When `requestPermission()` is called on the handle of an existing
  // dormant grant, the restore prompt is not triggered because there is a
  // platform app installed.
  base::test::TestFuture<
      content::FileSystemAccessPermissionGrant::PermissionRequestOutcome>
      future;
  auto* rfh = web_contents->GetPrimaryMainFrame();
  grant->RequestPermission(
      content::GlobalRenderFrameHostId(rfh->GetProcess()->GetDeprecatedID(),
                                       rfh->GetRoutingID()),
      content::FileSystemAccessPermissionGrant::UserActivationState::
          kNotRequired,
      future.GetCallback());
  auto result = future.Get();
  EXPECT_NE(result, content::FileSystemAccessPermissionGrant::
                        PermissionRequestOutcome::kGrantedByRestorePrompt);
}
