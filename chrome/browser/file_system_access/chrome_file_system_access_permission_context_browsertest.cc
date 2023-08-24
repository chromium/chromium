// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/file_system_access/chrome_file_system_access_permission_context.h"

#include <tuple>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/test_file_util.h"
#include "chrome/browser/apps/platform_apps/app_browsertest_util.h"
#include "chrome/browser/file_system_access/file_system_access_permission_request_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/permissions/permission_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/file_system_chooser_test_helpers.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_utils.h"
#include "extensions/test/extension_test_message_listener.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/select_file_dialog_factory.h"
#include "ui/shell_dialogs/select_file_policy.h"
#include "url/gurl.h"

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

 private:
  bool performed_after_write_checks_ = false;
  base::OnceClosure quit_callback_;
};

}  // anonymous namespace

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
  int host_id = prerender_helper().AddPrerender(prerender_url);
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

class FileSystemChromeAppTest : public extensions::PlatformAppBrowserTest {};

// TODO(b/276433834): Implement an end-to-end test for getDirectoryPicker in
// Chrome apps.
IN_PROC_BROWSER_TEST_F(FileSystemChromeAppTest,
                       FileSystemAccessPermissionRequestManagerExists) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ExtensionTestMessageListener launched_listener("Launched");

  // Install Platform App
  content::CreateAndLoadWebContentsObserver app_loaded_observer;
  const extensions::Extension* extension =
      InstallPlatformApp("file_system_test");
  ASSERT_TRUE(extension);

  // Launch Platform App
  LaunchPlatformApp(extension);
  app_loaded_observer.Wait();
  ASSERT_TRUE(launched_listener.WaitUntilSatisfied());

  content::WebContents* web_contents = GetFirstAppWindowWebContents();
  EXPECT_TRUE(web_contents);
  EXPECT_NE(nullptr, FileSystemAccessPermissionRequestManager::FromWebContents(
                         web_contents));
}
