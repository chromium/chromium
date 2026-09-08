// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_uninstall_dialog.h"

#include <memory>
#include <string>

#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/browser/extension_dialog_auto_confirm.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/common/extension.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/test_extension_dir.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {
namespace {

constexpr char kUninstallUrl[] = "https://www.google.com/";

constexpr char kReportAbuseUrlTemplate[] =
    "https://chromewebstore.google.com/detail/%s/"
    "report?utm_source=chrome-remove-extension-dialog";

class TestExtensionUninstallDialogDelegate
    : public ExtensionUninstallDialog::Delegate {
 public:
  explicit TestExtensionUninstallDialogDelegate(
      base::RepeatingClosure quit_closure)
      : quit_closure_(quit_closure) {}

  TestExtensionUninstallDialogDelegate(
      const TestExtensionUninstallDialogDelegate&) = delete;
  TestExtensionUninstallDialogDelegate& operator=(
      const TestExtensionUninstallDialogDelegate&) = delete;

  ~TestExtensionUninstallDialogDelegate() override = default;

  bool canceled() const { return canceled_; }
  const std::u16string& error() const { return error_; }

 private:
  void OnExtensionUninstallDialogClosed(bool did_start_uninstall,
                                        const std::u16string& error) override {
    ASSERT_FALSE(did_close_)
        << "OnExtensionUninstallDialogClosed() was called twice!";
    did_close_ = true;
    canceled_ = !did_start_uninstall;
    error_ = error;
    quit_closure_.Run();
  }

  base::RepeatingClosure quit_closure_;
  bool did_close_ = false;
  bool canceled_ = false;
  std::u16string error_;
};

}  // namespace

class ExtensionUninstallDialogBrowserTestBase : public ExtensionBrowserTest {
 public:
  scoped_refptr<const Extension> LoadExtensionWithUninstallUrl() {
    static constexpr char kManifest[] =
        R"({
             "name": "Uninstall Dialog Test Extension",
             "version": "0.1",
             "manifest_version": 3,
             "background": {
               "service_worker": "background.js"
             }
           })";
    static constexpr char kBackgroundJs[] =
        R"(chrome.runtime.setUninstallURL('%s', () => {
             chrome.test.sendMessage('ready!');
           });)";

    test_dir_.WriteManifest(kManifest);
    test_dir_.WriteFile(FILE_PATH_LITERAL("background.js"),
                        base::StringPrintf(kBackgroundJs, kUninstallUrl));

    ExtensionTestMessageListener listener("ready!");
    const Extension* extension = LoadExtension(test_dir_.UnpackedPath());
    if (!extension || !listener.WaitUntilSatisfied()) {
      return nullptr;
    }
    return extension;
  }

 private:
  TestExtensionDir test_dir_;
};

class ParameterizedExtensionUninstallDialogBrowserTest
    : public ExtensionUninstallDialogBrowserTestBase,
      public testing::WithParamInterface<UninstallReason> {};

// Test that when the user clicks Uninstall on the ExtensionUninstallDialog the
// extension's uninstall url (when it is specified) should open and be the
// active tab.
IN_PROC_BROWSER_TEST_P(ParameterizedExtensionUninstallDialogBrowserTest,
                       EnsureExtensionUninstallURLIsActiveTabAfterUninstall) {
  scoped_refptr<const Extension> extension = LoadExtensionWithUninstallUrl();
  ASSERT_TRUE(extension);

  // Auto-confirm the uninstall dialog.
  ScopedTestDialogAutoConfirm auto_confirm(ScopedTestDialogAutoConfirm::ACCEPT);

  base::RunLoop run_loop;
  TestExtensionUninstallDialogDelegate delegate(run_loop.QuitClosure());
  std::unique_ptr<ExtensionUninstallDialog> dialog(
      ExtensionUninstallDialog::Create(
          profile(), GetActiveWebContents()->GetTopLevelNativeWindow(),
          &delegate));

  const GURL uninstall_url(kUninstallUrl);
  content::TestNavigationObserver navigation_observer(uninstall_url);
  navigation_observer.StartWatchingNewWebContents();

  UninstallReason uninstall_reason = GetParam();
  dialog->ConfirmUninstall(extension, uninstall_reason,
                           UNINSTALL_SOURCE_FOR_TESTING);

  run_loop.Run();
  // The delegate should not be canceled because the user chose to uninstall
  // the extension, which should be successful.
  EXPECT_FALSE(delegate.canceled());

  navigation_observer.Wait();

  // There should be 2 tabs open: initial tab and the extension's uninstall url.
  EXPECT_EQ(2, GetTabCount());
  // Verifying that the extension's uninstall url is the active tab.
  EXPECT_EQ(kUninstallUrl,
            GetActiveWebContents()->GetLastCommittedURL().spec());
}

INSTANTIATE_TEST_SUITE_P(All,
                         ParameterizedExtensionUninstallDialogBrowserTest,
                         testing::Values(UNINSTALL_REASON_USER_INITIATED,
                                         UNINSTALL_REASON_CHROME_WEBSTORE));

using ExtensionUninstallDialogBrowserTest =
    ExtensionUninstallDialogBrowserTestBase;

// Test that when the user clicks the Report Abuse checkbox and clicks Uninstall
// on the ExtensionUninstallDialog, the extension's uninstall url (when it is
// specified) and the CWS Report Abuse survey are opened in the browser, also
// testing that the CWS survey is the active tab.
IN_PROC_BROWSER_TEST_F(ExtensionUninstallDialogBrowserTest,
                       EnsureCWSReportAbusePageIsActiveTabAfterUninstall) {
  scoped_refptr<const Extension> extension = LoadExtensionWithUninstallUrl();
  ASSERT_TRUE(extension);

  // Auto-confirm the uninstall dialog with report abuse checkbox checked.
  ScopedTestDialogAutoConfirm auto_confirm(
      ScopedTestDialogAutoConfirm::ACCEPT_AND_OPTION);

  base::RunLoop run_loop;
  TestExtensionUninstallDialogDelegate delegate(run_loop.QuitClosure());
  std::unique_ptr<ExtensionUninstallDialog> dialog(
      ExtensionUninstallDialog::Create(
          profile(), GetActiveWebContents()->GetTopLevelNativeWindow(),
          &delegate));

  dialog->ConfirmUninstall(extension, UNINSTALL_REASON_USER_INITIATED,
                           UNINSTALL_SOURCE_FOR_TESTING);

  run_loop.Run();
  // The delegate should not be canceled because the user chose to uninstall the
  // extension, which should be successful.
  EXPECT_FALSE(delegate.canceled());

  // There should be 3 tabs open: initial tab, the extension's uninstall url,
  // and the CWS Report Abuse survey.
  EXPECT_EQ(3, GetTabCount());
  content::WaitForLoadStop(GetActiveWebContents());
  // The CWS Report Abuse survey should be the active tab. We test this with the
  // actual string for the current "Report Abuse" page for the webstore, to be
  // explicit about what URL we are opening.
  EXPECT_EQ(
      base::StringPrintf(kReportAbuseUrlTemplate, extension->id().c_str()),
      GetActiveWebContents()->GetLastCommittedURL().spec());
  // Similar to the scenario above, this navigation can fail. The uninstall url
  // isn't hooked up to our test server.
  content::WaitForLoadStop(GetWebContentsAt(1));
  // Verifying that the extension's uninstall url was opened. It should not be
  // the active tab.
  EXPECT_EQ(kUninstallUrl,
            GetWebContentsAt(1)->GetLastCommittedURL().spec());
}

}  // namespace extensions
