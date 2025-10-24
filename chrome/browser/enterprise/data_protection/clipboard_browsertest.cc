// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/clipboard.h"

#include <memory>

#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/test_future.h"
#include "chrome/browser/enterprise/data_controls/desktop_data_controls_dialog.h"
#include "chrome/browser/enterprise/data_controls/desktop_data_controls_dialog_test_helper.h"
#include "chrome/browser/printing/print_preview_dialog_controller.h"
#include "chrome/browser/printing/print_test_utils.h"
#include "chrome/browser/printing/test_print_preview_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/enterprise/data_controls/core/browser/features.h"
#include "components/enterprise/data_controls/core/browser/test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/permissions_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "printing/buildflags/buildflags.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/clipboard/test/test_clipboard.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "ui/views/test/widget_activation_waiter.h"

namespace enterprise_data_protection {

namespace {

// Browser tests that test data protection integration with Chrome's clipboard
// logic. If a browser test you're adding is specific to a single
// function/class, consider using a browsertest.cc file specific to that code.
class DataProtectionClipboardBrowserTest : public InProcessBrowserTest {
 public:
  DataProtectionClipboardBrowserTest() = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
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

  void FocusWebContents(content::WebContents* web_contents = nullptr) {
#if BUILDFLAG(IS_MAC)
    content::HandleMissingKeyWindow();
#endif
    if (!web_contents) {
      web_contents = browser()->tab_strip_model()->GetActiveWebContents();
    }
    web_contents->Focus();
    views::test::WaitForWidgetActive(
        BrowserView::GetBrowserViewForBrowser(browser())->GetWidget(), true);
  }

  void WriteTextToClipboard(const std::string& text,
                            content::WebContents* web_contents = nullptr) {
    // Clear the clipboard before writing so that test cases where the write is
    // blocked don't read whatever data happened to have been in the system
    // clipboard at the time of the test running.
    ui::Clipboard::GetForCurrentThread()->Clear(
        ui::ClipboardBuffer::kCopyPaste);

    FocusWebContents(web_contents);
    ASSERT_TRUE(content::ExecJs(
        web_contents ? web_contents->GetPrimaryMainFrame() : rfh(),
        base::StringPrintf("navigator.clipboard.writeText(\"%s\");",
                           text.c_str())));
    base::RunLoop().RunUntilIdle();
  }

  void SetTextInClipboardAndWritePermission(const std::u16string& text) {
    {
      ui::ScopedClipboardWriter writer(ui::ClipboardBuffer::kCopyPaste);
      writer.WriteText(text);
    }
    base::RunLoop().RunUntilIdle();

    // This permission is required to use `readText()` without user input.
    content::PermissionController* permission_controller =
        rfh()->GetBrowserContext()->GetPermissionController();
    url::Origin origin = url::Origin::Create(url());
    SetPermissionControllerOverride(permission_controller, origin, origin,
                                    blink::PermissionType::CLIPBOARD_READ_WRITE,
                                    blink::mojom::PermissionStatus::GRANTED);
    base::RunLoop().RunUntilIdle();
  }
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
  data_controls::DesktopDataControlsDialogTestHelper helper(
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

  helper.CloseDialogWithoutBypass();
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
  data_controls::DesktopDataControlsDialogTestHelper helper(
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

  helper.CloseDialogWithoutBypass();
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
  data_controls::DesktopDataControlsDialogTestHelper helper(
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

  helper.BypassWarning();
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
  data_controls::DesktopDataControlsDialogTestHelper helper(
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

IN_PROC_BROWSER_TEST_F(DataProtectionClipboardBrowserTest,
                       PasteBlockedByDataControls) {
  data_controls::SetDataControls(browser()->profile()->GetPrefs(), {R"({
                    "destinations": { "urls": ["*"] },
                    "restrictions": [
                      {"class": "CLIPBOARD", "level": "BLOCK"}
                    ]
                  })"});
  data_controls::DesktopDataControlsDialogTestHelper helper(
      data_controls::DataControlsDialog::Type::kClipboardPasteBlock);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url()));
  SetTextInClipboardAndWritePermission(u"Blocked");

  // This is required because pasting fails if it's attempted by JS while the
  // page is not focused.
  FocusWebContents();
  content::ExecuteScriptAsync(
      rfh(), R"(var pasted_text = navigator.clipboard.readText();)");

  helper.WaitForDialogToInitialize();
  helper.CloseDialogWithoutBypass();
  helper.WaitForDialogToClose();

  // No data should have been read from the clipboard after closing the dialog
  // since the verdict was "block".
  EXPECT_EQ(content::EvalJs(rfh(), "pasted_text"), "");
}

IN_PROC_BROWSER_TEST_F(DataProtectionClipboardBrowserTest,
                       PasteWarnedByDataControls_Cancel) {
  data_controls::SetDataControls(browser()->profile()->GetPrefs(), {R"({
                    "destinations": { "urls": ["*"] },
                    "restrictions": [
                      {"class": "CLIPBOARD", "level": "WARN"}
                    ]
                  })"});
  data_controls::DesktopDataControlsDialogTestHelper helper(
      data_controls::DataControlsDialog::Type::kClipboardPasteWarn);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url()));
  SetTextInClipboardAndWritePermission(u"Warned_Cancel");

  // This is required because pasting fails if it's attempted by JS while the
  // page is not focused.
  FocusWebContents();
  content::ExecuteScriptAsync(
      rfh(), R"(var pasted_text = navigator.clipboard.readText();)");

  helper.WaitForDialogToInitialize();
  helper.CloseDialogWithoutBypass();
  helper.WaitForDialogToClose();

  // No data should have been read from the clipboard after closing the dialog
  // since the verdict was "block".
  EXPECT_EQ(content::EvalJs(rfh(), "pasted_text"), "");
}

IN_PROC_BROWSER_TEST_F(DataProtectionClipboardBrowserTest,
                       PasteWarnedByDataControls_Bypass) {
  data_controls::SetDataControls(browser()->profile()->GetPrefs(), {R"({
                    "destinations": { "urls": ["*"] },
                    "restrictions": [
                      {"class": "CLIPBOARD", "level": "WARN"}
                    ]
                  })"});
  data_controls::DesktopDataControlsDialogTestHelper helper(
      data_controls::DataControlsDialog::Type::kClipboardPasteWarn);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url()));
  SetTextInClipboardAndWritePermission(u"Warned_Bypassed");

  // This is required because pasting fails if it's attempted by JS while the
  // page is not focused.
  FocusWebContents();
  content::ExecuteScriptAsync(
      rfh(), R"(var pasted_text = navigator.clipboard.readText();)");

  helper.WaitForDialogToInitialize();
  helper.BypassWarning();
  helper.WaitForDialogToClose();

  // Data should be pasted in the page after closing the dialog since it was
  // bypassed.
  EXPECT_EQ(content::EvalJs(rfh(), "pasted_text"), "Warned_Bypassed");
}

IN_PROC_BROWSER_TEST_F(DataProtectionClipboardBrowserTest,
                       PasteAllowedByDataControls) {
  data_controls::SetDataControls(browser()->profile()->GetPrefs(), {R"({
                    "destinations": { "urls": ["google.com"] },
                    "restrictions": [
                      {"class": "CLIPBOARD", "level": "BLOCK"}
                    ]
                  })"});
  data_controls::DesktopDataControlsDialogTestHelper helper(
      data_controls::DataControlsDialog::Type::kClipboardPasteBlock);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url()));
  SetTextInClipboardAndWritePermission(u"Allowed");

  // This is required because pasting fails if it's attempted by JS while the
  // page is not focused.
  FocusWebContents();
  content::ExecuteScriptAsync(
      rfh(), R"(var pasted_text = navigator.clipboard.readText();)");

  ASSERT_FALSE(helper.dialog());
  EXPECT_EQ(content::EvalJs(rfh(), "pasted_text"), "Allowed");
}

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
#if BUILDFLAG(IS_MAC)
// TODO(crbug.com/452329561) fix flaky focus on macOS.
#define MAYBE_ChromePrintReportsInitiator DISABLED_ChromePrintReportsInitiator
#else
#define MAYBE_ChromePrintReportsInitiator ChromePrintReportsInitiator
#endif  // BUILDFLAG(IS_MAC)

IN_PROC_BROWSER_TEST_F(DataProtectionClipboardBrowserTest,
                       MAYBE_ChromePrintReportsInitiator) {
  data_controls::SetDataControls(browser()->profile()->GetPrefs(), {R"({
                    "sources": { "urls": ["http://127.0.0.1"] },
                    "restrictions": [
                      {"class": "CLIPBOARD", "level": "BLOCK"}
                    ]
                  })"});

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url()));

  printing::TestPrintPreviewObserver print_preview_observer(
      /*wait_for_loaded=*/true);
  printing::test::StartPrint(
      browser()->tab_strip_model()->GetActiveWebContents());
  content::WebContents* preview_web_contents =
      print_preview_observer.WaitUntilPreviewIsReadyAndReturnPreviewDialog();

  WriteTextToClipboard("Blocked", preview_web_contents);

  // Verify that nothing was written to the clipboard.
  base::test::TestFuture<std::u16string> future;
  ui::Clipboard::GetForCurrentThread()->ReadText(
      ui::ClipboardBuffer::kCopyPaste, /*data_dst=*/nullptr,
      future.GetCallback());
  ASSERT_TRUE(future.Wait());
  EXPECT_TRUE(future.Get().empty());
}

#if BUILDFLAG(IS_MAC)
// TODO(crbug.com/452329561) fix flaky focus on macOS.
#define MAYBE_ChromePrintReportsPrimaryMainFrameURLWithinSubframe \
  DISABLED_ChromePrintReportsPrimaryMainFrameURLWithinSubframe
#else
#define MAYBE_ChromePrintReportsPrimaryMainFrameURLWithinSubframe \
  ChromePrintReportsPrimaryMainFrameURLWithinSubframe
#endif  // BUILDFLAG(IS_MAC)

IN_PROC_BROWSER_TEST_F(
    DataProtectionClipboardBrowserTest,
    MAYBE_ChromePrintReportsPrimaryMainFrameURLWithinSubframe) {
  data_controls::SetDataControls(browser()->profile()->GetPrefs(), {R"({
                    "sources": { "urls": ["a.com"] },
                    "restrictions": [
                      {"class": "CLIPBOARD", "level": "BLOCK"}
                    ]
                  })"});

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "a.com", "/cross_site_iframe_factory.html?a(b)")));
  content::WebContents* original_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  content::RenderFrameHost* main_frame =
      original_contents->GetPrimaryMainFrame();
  content::RenderFrameHost* sub_frame = content::ChildFrameAt(main_frame, 0);
  ASSERT_EQ(sub_frame->GetLastCommittedOrigin().host(), "b.com");

  printing::TestPrintPreviewObserver print_preview_observer(
      /*wait_for_loaded=*/true);
  content::ExecuteScriptAsync(sub_frame, R"(window.print();)");
  content::WebContents* preview_web_contents =
      print_preview_observer.WaitUntilPreviewIsReadyAndReturnPreviewDialog();

  WriteTextToClipboard("Blocked", preview_web_contents);

  // Verify that nothing was written to the clipboard. Only consider the
  // primary main frame when matching Data Control rules.
  base::test::TestFuture<std::u16string> future;
  ui::Clipboard::GetForCurrentThread()->ReadText(
      ui::ClipboardBuffer::kCopyPaste, /*data_dst=*/nullptr,
      future.GetCallback());
  ASSERT_TRUE(future.Wait());
  EXPECT_TRUE(future.Get().empty());
}
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)

}  // namespace enterprise_data_protection
