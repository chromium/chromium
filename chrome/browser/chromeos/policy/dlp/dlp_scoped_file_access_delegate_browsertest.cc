// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/chromeos/policy/dlp/dlp_scoped_file_access_delegate.h"

#include <memory>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "chrome/browser/apps/platform_apps/app_browsertest_util.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chromeos/dbus/dlp/dlp_client.h"
#include "chromeos/dbus/dlp/fake_dlp_client.h"
#include "components/file_access/scoped_file_access.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/file_select_listener.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"

namespace policy {
namespace {

// This class is used to answer file chooser requests with `file` without any
// user interaction.
class FileChooserDelegate : public content::WebContentsDelegate {
 public:
  explicit FileChooserDelegate(base::FilePath file) : file_(std::move(file)) {}
  void RunFileChooser(content::RenderFrameHost* render_frame_host,
                      scoped_refptr<content::FileSelectListener> listener,
                      const blink::mojom::FileChooserParams& params) override {
    std::vector<blink::mojom::FileChooserFileInfoPtr> files;
    auto file_info = blink::mojom::FileChooserFileInfo::NewNativeFile(
        blink::mojom::NativeFileInfo::New(file_.AppendASCII(""),
                                          std::u16string()));
    files.push_back(std::move(file_info));
    listener->FileSelected(std::move(files), base::FilePath(), params.mode);
  }
  const base::FilePath file_;
};

}  // namespace

using ::testing::_;

class DlpScopedFileAccessDelegateBrowserTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    base::FilePath test_data_path;
    EXPECT_TRUE(base::PathService::Get(base::DIR_SOURCE_ROOT, &test_data_path));
    embedded_test_server()->ServeFilesFromDirectory(
        test_data_path.AppendASCII("chrome/test/data/dlp"));
    host_resolver()->AddRule("*", "127.0.0.1");
    EXPECT_TRUE(embedded_test_server()->Start());
    chromeos::DlpClient::Shutdown();
    chromeos::DlpClient::InitializeFake();
    delegate_ = std::make_unique<DlpScopedFileAccessDelegate>(
        chromeos::DlpClient::Get());
    EXPECT_TRUE(tmp_.CreateUniqueTempDir());

    content::WebContents* const web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    GURL test_url =
        embedded_test_server()->GetURL("localhost", "/dlp_files_test.html");
    EXPECT_TRUE(content::NavigateToURL(web_contents, test_url));
    content::WebContentsConsoleObserver con_observer(web_contents);
    con_observer.SetPattern("db opened");
    EXPECT_TRUE(con_observer.Wait());

    fake_dlp_client_ =
        static_cast<chromeos::FakeDlpClient*>(chromeos::DlpClient::Get());
  }

  void TearDownOnMainThread() override { fake_dlp_client_ = nullptr; }

  // Executes `action` as JavaScript in the context of the opened website. The
  // actions is expected to trigger printing `expectedConsole` on the console.
  void TestJSAction(std::string action, std::string expectedConsole) {
    content::WebContents* const web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();

    content::WebContentsConsoleObserver console_observer(web_contents);
    console_observer.SetPattern(expectedConsole);

    EXPECT_TRUE(content::ExecJs(web_contents, action));

    EXPECT_TRUE(console_observer.Wait());
  }

  // Setup a delegate to answer file chooser requests with a specific file
  // (input.txt). The returned value has to be kept in scope as long as requests
  // should be handled this way.
  std::unique_ptr<FileChooserDelegate> PrepareChooser() {
    base::FilePath file = tmp_.GetPath().AppendASCII("input.txt");
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      base::WriteFile(file, kTestContent);
    }
    std::unique_ptr<FileChooserDelegate> delegate(
        new FileChooserDelegate(std::move(file)));
    browser()->tab_strip_model()->GetActiveWebContents()->SetDelegate(
        delegate.get());
    return delegate;
  }

 protected:
  const std::string kTestContent = "This is file content.";
  const std::string kErrorMessage = "Could not read file.";
  std::unique_ptr<DlpScopedFileAccessDelegate> delegate_;
  base::ScopedTempDir tmp_;
  raw_ptr<chromeos::FakeDlpClient> fake_dlp_client_;
};

IN_PROC_BROWSER_TEST_F(DlpScopedFileAccessDelegateBrowserTest,
                       UploadFrameFileApiProtectedAllow) {
  fake_dlp_client_->SetFileAccessAllowed(/*allowed=*/true);
  auto delegate = PrepareChooser();
  TestJSAction("document.getElementById('file').click()", kTestContent);
}

IN_PROC_BROWSER_TEST_F(DlpScopedFileAccessDelegateBrowserTest,
                       UploadFrameFileApiProtectedDeny) {
  fake_dlp_client_->SetFileAccessAllowed(/*allowed=*/false);
  auto delegate = PrepareChooser();
  TestJSAction("document.getElementById('file').click()", kErrorMessage);
}

IN_PROC_BROWSER_TEST_F(DlpScopedFileAccessDelegateBrowserTest,
                       UploadDedicatedFileApiProtectedAllow) {
  fake_dlp_client_->SetFileAccessAllowed(/*allowed=*/true);
  auto delegate = PrepareChooser();
  TestJSAction("document.getElementById('file_worker').click()",
               kTestContent.substr(1));
}

IN_PROC_BROWSER_TEST_F(DlpScopedFileAccessDelegateBrowserTest,
                       UploadDedicatedFileApiProtectedDeny) {
  fake_dlp_client_->SetFileAccessAllowed(/*allowed=*/false);
  auto delegate = PrepareChooser();
  TestJSAction("document.getElementById('file_worker').click()", kErrorMessage);
}

IN_PROC_BROWSER_TEST_F(DlpScopedFileAccessDelegateBrowserTest,
                       UploadSharedFileApiProtectedAllow) {
  fake_dlp_client_->SetFileAccessAllowed(/*allowed=*/true);
  auto delegate = PrepareChooser();
  TestJSAction("document.getElementById('file_shared').click()",
               kTestContent.substr(1));
}

IN_PROC_BROWSER_TEST_F(DlpScopedFileAccessDelegateBrowserTest,
                       UploadSharedFileApiProtectedDeny) {
  fake_dlp_client_->SetFileAccessAllowed(/*allowed=*/false);
  auto delegate = PrepareChooser();
  TestJSAction("document.getElementById('file_shared').click()", kErrorMessage);
}

IN_PROC_BROWSER_TEST_F(DlpScopedFileAccessDelegateBrowserTest,
                       UploadServiceFileApiProtectedAllow) {
  fake_dlp_client_->SetFileAccessAllowed(/*allowed=*/true);
  auto delegate = PrepareChooser();
  TestJSAction("document.getElementById('file_service').click()",
               kTestContent.substr(1));
}

IN_PROC_BROWSER_TEST_F(DlpScopedFileAccessDelegateBrowserTest,
                       UploadServiceFileApiProtectedDeny) {
  fake_dlp_client_->SetFileAccessAllowed(/*allowed=*/false);
  auto delegate = PrepareChooser();
  TestJSAction("document.getElementById('file_service').click()",
               kErrorMessage);
}

IN_PROC_BROWSER_TEST_F(DlpScopedFileAccessDelegateBrowserTest,
                       UploadFrameIDBProtectedAllow) {
  fake_dlp_client_->SetFileAccessAllowed(true);
  auto delegate = PrepareChooser();
  TestJSAction("document.getElementById('idb_clear').click()", "cleared");
  TestJSAction("document.getElementById('idb_save').click()", "saved");
  TestJSAction("document.getElementById('idb_open').click()", kTestContent);
}

IN_PROC_BROWSER_TEST_F(DlpScopedFileAccessDelegateBrowserTest,
                       UploadFrameIDBProtectedDeny) {
  fake_dlp_client_->SetFileAccessAllowed(false);
  auto delegate = PrepareChooser();
  TestJSAction("document.getElementById('idb_clear').click()", "cleared");
  TestJSAction("document.getElementById('idb_save').click()", "saved");
  TestJSAction("document.getElementById('idb_open').click()", kErrorMessage);
}

IN_PROC_BROWSER_TEST_F(DlpScopedFileAccessDelegateBrowserTest,
                       UploadFrameRestoreProtectedAllow) {
  fake_dlp_client_->SetFileAccessAllowed(true);
  auto delegate = PrepareChooser();
  TestJSAction("document.getElementById('idb_save').click()", "saved");

  NavigateParams params(browser(), GURL("about:blank"),
                        ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
  params.disposition = WindowOpenDisposition::NEW_BACKGROUND_TAB;
  Navigate(&params);

  TestJSAction("document.getElementById('idb_cached').click()", kTestContent);

  chrome::CloseTab(browser());

  chrome::RestoreTab(browser());

  content::WebContentsConsoleObserver console_observer(
      browser()->tab_strip_model()->GetActiveWebContents());
  console_observer.SetPattern("db opened");
  EXPECT_TRUE(console_observer.Wait());

  TestJSAction("document.getElementById('idb_cached').click()", kTestContent);
}

IN_PROC_BROWSER_TEST_F(DlpScopedFileAccessDelegateBrowserTest,
                       UploadFrameRestoreProtectedDenyRestore) {
  fake_dlp_client_->SetFileAccessAllowed(true);
  auto delegate = PrepareChooser();
  TestJSAction("document.getElementById('idb_save').click()", "saved");

  NavigateParams params(browser(), GURL("about:blank"),
                        ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
  params.disposition = WindowOpenDisposition::NEW_BACKGROUND_TAB;
  Navigate(&params);

  TestJSAction("document.getElementById('idb_cached').click()", kTestContent);

  chrome::CloseTab(browser());

  fake_dlp_client_->SetFileAccessAllowed(false);

  chrome::RestoreTab(browser());

  content::WebContentsConsoleObserver console_observer(
      browser()->tab_strip_model()->GetActiveWebContents());
  console_observer.SetPattern("db opened");
  EXPECT_TRUE(console_observer.Wait());

  TestJSAction("document.getElementById('idb_cached').click()", kErrorMessage);
}

}  // namespace policy
