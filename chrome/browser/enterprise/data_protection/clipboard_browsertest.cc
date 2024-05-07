// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/clipboard.h"

#include <memory>

#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/test_future.h"
#include "chrome/browser/enterprise/data_controls/data_controls_dialog.h"
#include "chrome/browser/enterprise/data_controls/data_controls_dialog_test_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/enterprise/data_controls/features.h"
#include "components/enterprise/data_controls/test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/permissions_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/clipboard/test/test_clipboard.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"

namespace enterprise_data_protection {

namespace {

// Browser tests that test data protection integration with Chrome's clipboard
// logic. If a browser test you're adding is specific to a single
// function/class, consider using a browsertest.cc file specific to that code.
class DataProtectionClipboardBrowserTest : public InProcessBrowserTest {
 public:
  DataProtectionClipboardBrowserTest() {
    scoped_features_.InitAndEnableFeature(
        data_controls::kEnableDesktopDataControls);
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());

    ui::TestClipboard::CreateForCurrentThread();
  }

  void TearDownOnMainThread() override {
    ui::TestClipboard::DestroyClipboardForCurrentThread();
    InProcessBrowserTest::TearDownOnMainThread();
  }

  content::RenderFrameHost* rfh() {
    return browser()
        ->tab_strip_model()
        ->GetActiveWebContents()
        ->GetPrimaryMainFrame();
  }

  GURL url() {
    return embedded_test_server()->GetURL(
        "/enterprise/data_protection/clipboard_test_page.html");
  }

  void WriteTextToClipboard(const std::string& text) {
    // Clear the clipboard before writing so that test cases where the write is
    // blocked don't read whatever data happened to have been in the system
    // clipboard at the time of the test running.
    ui::Clipboard::GetForCurrentThread()->Clear(
        ui::ClipboardBuffer::kCopyPaste);

    browser()->tab_strip_model()->GetActiveWebContents()->Focus();
    ASSERT_TRUE(content::ExecJs(
        rfh(), base::StringPrintf("navigator.clipboard.writeText(\"%s\");",
                                  text.c_str())));
    base::RunLoop().RunUntilIdle();
  }

 protected:
  base::test::ScopedFeatureList scoped_features_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(DataProtectionClipboardBrowserTest,
                       CopyBlockedByDataControls) {
  data_controls::SetDataControls(browser()->profile()->GetPrefs(), {R"({
                    "sources": { "urls": ["*"] },
                    "restrictions": [
                      {"class": "CLIPBOARD", "level": "BLOCK"}
                    ]
                  })"});
  data_controls::DataControlsDialogTestHelper helper(
      data_controls::DataControlsDialog::Type::kClipboardCopyBlock);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url()));
  WriteTextToClipboard("Blocked");

  helper.WaitForDialogToInitialize();

  // No data should be written into the clipboard while the dialog is present.
  base::test::TestFuture<std::u16string> first_future;
  ui::Clipboard::GetForCurrentThread()->ReadText(
      ui::ClipboardBuffer::kCopyPaste, /*data_dst=*/nullptr,
      first_future.GetCallback());
  EXPECT_TRUE(first_future.Wait());
  EXPECT_TRUE(first_future.Get().empty());

  helper.CancelDialog();
  helper.WaitForDialogToClose();

  // No data should be in the clipboard after closing the dialog since the
  // verdict was "block".
  base::test::TestFuture<std::u16string> second_future;
  ui::Clipboard::GetForCurrentThread()->ReadText(
      ui::ClipboardBuffer::kCopyPaste, /*data_dst=*/nullptr,
      second_future.GetCallback());
  EXPECT_TRUE(second_future.Wait());
  EXPECT_TRUE(second_future.Get().empty());
}

IN_PROC_BROWSER_TEST_F(DataProtectionClipboardBrowserTest,
                       CopyWarnedByDataControls_Cancel) {
  data_controls::SetDataControls(browser()->profile()->GetPrefs(), {R"({
                    "sources": { "urls": ["*"] },
                    "restrictions": [
                      {"class": "CLIPBOARD", "level": "WARN"}
                    ]
                  })"});
  data_controls::DataControlsDialogTestHelper helper(
      data_controls::DataControlsDialog::Type::kClipboardCopyWarn);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url()));
  WriteTextToClipboard("Cancel");

  helper.WaitForDialogToInitialize();

  // No data should be written into the clipboard while the dialog is present.
  base::test::TestFuture<std::u16string> first_future;
  ui::Clipboard::GetForCurrentThread()->ReadText(
      ui::ClipboardBuffer::kCopyPaste, /*data_dst=*/nullptr,
      first_future.GetCallback());
  EXPECT_TRUE(first_future.Wait());
  EXPECT_TRUE(first_future.Get().empty());

  helper.CancelDialog();
  helper.WaitForDialogToClose();

  // No data should be in the clipboard after closing the dialog since it wasn't
  // bypassed.
  base::test::TestFuture<std::u16string> second_future;
  ui::Clipboard::GetForCurrentThread()->ReadText(
      ui::ClipboardBuffer::kCopyPaste, /*data_dst=*/nullptr,
      second_future.GetCallback());
  EXPECT_TRUE(second_future.Wait());
  EXPECT_TRUE(second_future.Get().empty());
}

IN_PROC_BROWSER_TEST_F(DataProtectionClipboardBrowserTest,
                       CopyWarnedByDataControls_Bypass) {
  data_controls::SetDataControls(browser()->profile()->GetPrefs(), {R"({
                    "sources": { "urls": ["*"] },
                    "restrictions": [
                      {"class": "CLIPBOARD", "level": "WARN"}
                    ]
                  })"});
  data_controls::DataControlsDialogTestHelper helper(
      data_controls::DataControlsDialog::Type::kClipboardCopyWarn);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url()));
  WriteTextToClipboard("Bypassed");

  helper.WaitForDialogToInitialize();

  // No data should be written into the clipboard while the dialog is present.
  base::test::TestFuture<std::u16string> first_future;
  ui::Clipboard::GetForCurrentThread()->ReadText(
      ui::ClipboardBuffer::kCopyPaste, /*data_dst=*/nullptr,
      first_future.GetCallback());
  EXPECT_TRUE(first_future.Wait());
  EXPECT_TRUE(first_future.Get().empty());

  helper.AcceptDialog();
  helper.WaitForDialogToClose();
  base::RunLoop().RunUntilIdle();

  // Data should be in the clipboard after closing the dialog since it was
  // bypassed.
  base::test::TestFuture<std::u16string> second_future;
  ui::Clipboard::GetForCurrentThread()->ReadText(
      ui::ClipboardBuffer::kCopyPaste, /*data_dst=*/nullptr,
      second_future.GetCallback());
  EXPECT_TRUE(second_future.Wait());
  EXPECT_EQ(second_future.Get(), u"Bypassed");
}

IN_PROC_BROWSER_TEST_F(DataProtectionClipboardBrowserTest,
                       CopyAllowedByDataControls) {
  data_controls::SetDataControls(browser()->profile()->GetPrefs(), {R"({
                    "sources": { "urls": ["google.com"] },
                    "restrictions": [
                      {"class": "CLIPBOARD", "level": "WARN"}
                    ]
                  })"});
  data_controls::DataControlsDialogTestHelper helper(
      data_controls::DataControlsDialog::Type::kClipboardCopyWarn);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url()));
  WriteTextToClipboard("Allowed");

  // No dialog should be present since the copy was allowed, and the data should
  // be in the clipboard.
  ASSERT_FALSE(helper.dialog());
  base::test::TestFuture<std::u16string> future;
  ui::Clipboard::GetForCurrentThread()->ReadText(
      ui::ClipboardBuffer::kCopyPaste, /*data_dst=*/nullptr,
      future.GetCallback());
  EXPECT_TRUE(future.Wait());
  EXPECT_EQ(future.Get(), u"Allowed");
}

}  // namespace enterprise_data_protection
