// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/enterprise/data_controls/desktop_data_controls_dialog_test_helper.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_context_menu/link_to_text_menu_observer.h"
#include "chrome/browser/renderer_context_menu/mock_render_view_context_menu.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "chrome/browser/ui/toasts/toast_features.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/enterprise/data_controls/core/browser/features.h"
#include "components/enterprise/data_controls/core/browser/test_utils.h"
#include "components/shared_highlighting/core/common/shared_highlighting_features.h"
#include "components/shared_highlighting/core/common/shared_highlighting_metrics.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/process_manager.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/clipboard.h"

class MockLinkToTextMenuObserver : public LinkToTextMenuObserver {
 public:
  static std::unique_ptr<MockLinkToTextMenuObserver> Create(
      RenderViewContextMenuProxy* proxy,
      content::GlobalRenderFrameHostId render_frame_host_id,
      ToastController* toast_controller) {
    // WebContents can be null in tests.
    content::WebContents* web_contents = proxy->GetWebContents();
    if (web_contents && extensions::ProcessManager::Get(
                            proxy->GetWebContents()->GetBrowserContext())
                            ->GetExtensionForWebContents(web_contents)) {
      // Do not show menu item for extensions, such as the PDF viewer.
      return nullptr;
    }

    return base::WrapUnique(new MockLinkToTextMenuObserver(
        proxy, render_frame_host_id, toast_controller));
  }
  MockLinkToTextMenuObserver(
      RenderViewContextMenuProxy* proxy,
      content::GlobalRenderFrameHostId render_frame_host_id,
      ToastController* toast_controller)
      : LinkToTextMenuObserver(proxy, render_frame_host_id, toast_controller) {}

  void SetGenerationResults(
      std::string selector,
      shared_highlighting::LinkGenerationError error,
      shared_highlighting::LinkGenerationReadyStatus ready_status) {
    selector_ = selector;
    error_ = error;
    ready_status_ = ready_status;
  }

  void SetReshareSelector(std::string selectors) {
    reshare_selectors_.push_back(selectors);
  }

 private:
  std::string selector_;
  shared_highlighting::LinkGenerationError error_;
  shared_highlighting::LinkGenerationReadyStatus ready_status_;

  std::vector<std::string> reshare_selectors_;

  void StartLinkGenerationRequestWithTimeout() override {
    OnRequestLinkGenerationCompleted(selector_, error_, ready_status_);
  }

  void ReshareLink() override {
    OnGetExistingSelectorsComplete(reshare_selectors_);
  }
};

namespace {

class LinkToTextMenuObserverTest : public extensions::ExtensionBrowserTest {
 public:
  LinkToTextMenuObserverTest() {
    scoped_features_.InitWithFeatures(
        {toast_features::kLinkToHighlightCopiedToast,
         toast_features::kToastFramework},
        {});
  }

  void SetUpOnMainThread() override {
    extensions::ExtensionBrowserTest::SetUpOnMainThread();
    Reset(false);

    host_resolver()->AddRule("*", "127.0.0.1");

    // Add content/test/data for cross_site_iframe_factory.html
    embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");

    ASSERT_TRUE(embedded_test_server()->Start());

    auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
    menu()->set_web_contents(web_contents);
    content::RenderFrameHost* main_frame = web_contents->GetPrimaryMainFrame();
    EXPECT_TRUE(ExecJs(main_frame, "window.focus();"));
  }
  void TearDownOnMainThread() override {
    observer_.reset();
    menu_.reset();
  }

  void Reset(bool incognito) {
    menu_ = std::make_unique<MockRenderViewContextMenu>(incognito);
    observer_ = MockLinkToTextMenuObserver::Create(
        menu_.get(), getRenderFrameHostId(),
        browser()->GetFeatures().toast_controller());
    menu_->SetObserver(observer_.get());
  }

  void InitMenu(content::ContextMenuParams params) {
    observer_->InitMenu(params);
  }

  LinkToTextMenuObserverTest(const LinkToTextMenuObserverTest&) = delete;
  LinkToTextMenuObserverTest& operator=(const LinkToTextMenuObserverTest&) =
      delete;

  ~LinkToTextMenuObserverTest() override;
  MockRenderViewContextMenu* menu() { return menu_.get(); }
  MockLinkToTextMenuObserver* observer() { return observer_.get(); }

  content::GlobalRenderFrameHostId getRenderFrameHostId() {
    auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
    return web_contents->GetPrimaryMainFrame()->GetGlobalId();
  }

 private:
  base::test::ScopedFeatureList scoped_features_;
  std::unique_ptr<MockLinkToTextMenuObserver> observer_;
  std::unique_ptr<MockRenderViewContextMenu> menu_;
};

LinkToTextMenuObserverTest::~LinkToTextMenuObserverTest() = default;

class LinkToTextMenuObserverDataControlsTest
    : public LinkToTextMenuObserverTest {
 public:
  LinkToTextMenuObserverDataControlsTest() {
    scoped_features_.InitAndEnableFeature(
        data_controls::kEnableDesktopDataControls);
  }

 protected:
  base::test::ScopedFeatureList scoped_features_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(LinkToTextMenuObserverTest, AddsCopyMenuItem) {
  content::ContextMenuParams params;
  params.page_url = GURL("http://foo.com/");
  params.selection_text = u"hello world";
  observer()->SetGenerationResults(
      std::string(), shared_highlighting::LinkGenerationError::kEmptySelection,
      shared_highlighting::LinkGenerationReadyStatus::kRequestedAfterReady);
  InitMenu(params);
  EXPECT_EQ(1u, menu()->GetMenuSize());
  MockRenderViewContextMenu::MockMenuItem item;
  menu()->GetMenuItem(0, &item);
  EXPECT_EQ(IDC_CONTENT_CONTEXT_COPYLINKTOTEXT, item.command_id);
  EXPECT_FALSE(item.checked);
  EXPECT_FALSE(item.hidden);
  EXPECT_FALSE(item.enabled);
}

IN_PROC_BROWSER_TEST_F(LinkToTextMenuObserverTest, AddsCopyAndRemoveMenuItems) {
  content::ContextMenuParams params;
  params.page_url = GURL("http://foo.com/");
  params.opened_from_highlight = true;
  observer()->SetGenerationResults(
      std::string(), shared_highlighting::LinkGenerationError::kEmptySelection,
      shared_highlighting::LinkGenerationReadyStatus::kRequestedAfterReady);
  InitMenu(params);
  EXPECT_EQ(2u, menu()->GetMenuSize());
  MockRenderViewContextMenu::MockMenuItem item;

  // Check Reshare item.
  menu()->GetMenuItem(0, &item);
  EXPECT_EQ(IDC_CONTENT_CONTEXT_RESHARELINKTOTEXT, item.command_id);
  EXPECT_FALSE(item.checked);
  EXPECT_FALSE(item.hidden);
  EXPECT_TRUE(item.enabled);

  // Check Remove item.
  menu()->GetMenuItem(1, &item);
  EXPECT_EQ(IDC_CONTENT_CONTEXT_REMOVELINKTOTEXT, item.command_id);
  EXPECT_FALSE(item.checked);
  EXPECT_FALSE(item.hidden);
  EXPECT_TRUE(item.enabled);
}

IN_PROC_BROWSER_TEST_F(LinkToTextMenuObserverTest, CopiesLinkToText) {
  content::BrowserTestClipboardScope test_clipboard_scope;
  content::ContextMenuParams params;
  params.page_url = GURL("http://foo.com/");
  params.selection_text = u"hello world";
  observer()->SetGenerationResults(
      "hello%20world", shared_highlighting::LinkGenerationError::kNone,
      shared_highlighting::LinkGenerationReadyStatus::kRequestedAfterReady);
  InitMenu(params);
  menu()->ExecuteCommand(IDC_CONTENT_CONTEXT_COPYLINKTOTEXT, 0);

  ui::Clipboard* clipboard = ui::Clipboard::GetForCurrentThread();
  std::u16string text;
  clipboard->ReadText(ui::ClipboardBuffer::kCopyPaste, nullptr, &text);
  EXPECT_EQ(u"http://foo.com/#:~:text=hello%20world", text);
}

IN_PROC_BROWSER_TEST_F(LinkToTextMenuObserverTest, CopiesLinkForEmptySelector) {
  content::BrowserTestClipboardScope test_clipboard_scope;
  content::ContextMenuParams params;
  params.page_url = GURL("http://foo.com/");
  params.selection_text = u"hello world";
  observer()->SetGenerationResults(
      std::string(), shared_highlighting::LinkGenerationError::kEmptySelection,
      shared_highlighting::LinkGenerationReadyStatus::kRequestedAfterReady);
  InitMenu(params);

  EXPECT_FALSE(menu()->IsCommandIdEnabled(IDC_CONTENT_CONTEXT_COPYLINKTOTEXT));
}

IN_PROC_BROWSER_TEST_F(LinkToTextMenuObserverTest, ReplacesRefInURL) {
  content::BrowserTestClipboardScope test_clipboard_scope;
  content::ContextMenuParams params;
  params.page_url = GURL("http://foo.com/#:~:text=hello%20world");
  params.selection_text = u"hello world";
  observer()->SetGenerationResults(
      "hello", shared_highlighting::LinkGenerationError::kNone,
      shared_highlighting::LinkGenerationReadyStatus::kRequestedAfterReady);
  InitMenu(params);
  menu()->ExecuteCommand(IDC_CONTENT_CONTEXT_COPYLINKTOTEXT, 0);

  ui::Clipboard* clipboard = ui::Clipboard::GetForCurrentThread();
  std::u16string text;
  clipboard->ReadText(ui::ClipboardBuffer::kCopyPaste, nullptr, &text);
  EXPECT_EQ(u"http://foo.com/#:~:text=hello", text);
}

IN_PROC_BROWSER_TEST_F(LinkToTextMenuObserverTest, InvalidSelectorForIframe) {
  GURL main_url(
      embedded_test_server()->GetURL("a.com", "/page_with_iframe.html"));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::RenderFrameHost* main_frame_a = web_contents->GetPrimaryMainFrame();
  content::RenderFrameHost* child_frame_b = ChildFrameAt(main_frame_a, 0);
  EXPECT_TRUE(ExecJs(child_frame_b, "window.focus();"));
  EXPECT_EQ(child_frame_b, web_contents->GetFocusedFrame());

  menu()->set_web_contents(web_contents);

  content::BrowserTestClipboardScope test_clipboard_scope;
  content::ContextMenuParams params;
  params.page_url = main_url;
  params.selection_text = u"hello world";
  InitMenu(params);

  EXPECT_FALSE(menu()->IsCommandIdEnabled(IDC_CONTENT_CONTEXT_COPYLINKTOTEXT));
}

IN_PROC_BROWSER_TEST_F(LinkToTextMenuObserverTest, HiddenForExtensions) {
  const extensions::Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("simple_with_file"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), extension->GetResourceURL("file.html")));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  menu()->set_web_contents(web_contents);

  std::unique_ptr<MockLinkToTextMenuObserver> observer =
      MockLinkToTextMenuObserver::Create(
          menu(), getRenderFrameHostId(),
          browser()->GetFeatures().toast_controller());
  EXPECT_EQ(nullptr, observer);
}

IN_PROC_BROWSER_TEST_F(LinkToTextMenuObserverTest, Blocklist) {
  content::BrowserTestClipboardScope test_clipboard_scope;
  content::ContextMenuParams params;
  params.page_url = GURL("http://facebook.com/my-profile");
  params.selection_text = u"hello world";
  InitMenu(params);

  EXPECT_FALSE(menu()->IsCommandIdEnabled(IDC_CONTENT_CONTEXT_COPYLINKTOTEXT));
}

IN_PROC_BROWSER_TEST_F(LinkToTextMenuObserverTest,
                       SelectionOverlappingHighlightCopiesNewLinkToText) {
  content::BrowserTestClipboardScope test_clipboard_scope;
  content::ContextMenuParams params;
  params.page_url = GURL("http://foo.com/");
  params.selection_text = u"hello world";
  params.opened_from_highlight = true;
  observer()->SetGenerationResults(
      "hello%20world", shared_highlighting::LinkGenerationError::kNone,
      shared_highlighting::LinkGenerationReadyStatus::kRequestedAfterReady);
  InitMenu(params);
  menu()->ExecuteCommand(IDC_CONTENT_CONTEXT_COPYLINKTOTEXT, 0);

  ui::Clipboard* clipboard = ui::Clipboard::GetForCurrentThread();
  std::u16string text;
  clipboard->ReadText(ui::ClipboardBuffer::kCopyPaste, nullptr, &text);
  EXPECT_EQ(u"http://foo.com/#:~:text=hello%20world", text);
}

IN_PROC_BROWSER_TEST_F(LinkToTextMenuObserverTest,
                       LinkGenerationCopiedLinkTypeMetric_NewGeneration) {
  base::HistogramTester histogram_tester;

  content::BrowserTestClipboardScope test_clipboard_scope;
  content::ContextMenuParams params;
  params.page_url = GURL("http://foo.com/");
  params.selection_text = u"hello world";
  observer()->SetGenerationResults(
      "hello%20world", shared_highlighting::LinkGenerationError::kNone,
      shared_highlighting::LinkGenerationReadyStatus::kRequestedAfterReady);
  InitMenu(params);
  menu()->ExecuteCommand(IDC_CONTENT_CONTEXT_COPYLINKTOTEXT, 0);

  // Verify that the copy type metric was correctly set
  histogram_tester.ExpectTotalCount("SharedHighlights.Desktop.CopiedLinkType",
                                    1);
  histogram_tester.ExpectBucketCount(
      "SharedHighlights.Desktop.CopiedLinkType",
      shared_highlighting::LinkGenerationCopiedLinkType::
          kCopiedFromNewGeneration,
      1);
}

IN_PROC_BROWSER_TEST_F(LinkToTextMenuObserverTest,
                       LinkGenerationCopiedLinkTypeMetric_ReShare) {
  base::HistogramTester histogram_tester;

  content::BrowserTestClipboardScope test_clipboard_scope;
  content::ContextMenuParams params;
  params.page_url = GURL("http://foo.com/#:~:text=hello%20world");
  params.selection_text = u"";
  params.opened_from_highlight = true;
  observer()->SetReshareSelector("hello%20world");
  InitMenu(params);
  menu()->ExecuteCommand(IDC_CONTENT_CONTEXT_RESHARELINKTOTEXT, 0);

  // Verify that the copy type metric was correctly set
  histogram_tester.ExpectTotalCount("SharedHighlights.Desktop.CopiedLinkType",
                                    1);
  histogram_tester.ExpectBucketCount(
      "SharedHighlights.Desktop.CopiedLinkType",
      shared_highlighting::LinkGenerationCopiedLinkType::
          kCopiedFromExistingHighlight,
      1);
}

IN_PROC_BROWSER_TEST_F(LinkToTextMenuObserverTest,
                       LinkGenerationRequestedMetric_Success_NoDelay) {
  base::HistogramTester histogram_tester;

  content::BrowserTestClipboardScope test_clipboard_scope;
  content::ContextMenuParams params;
  params.page_url = GURL("http://foo.com/");
  params.selection_text = u"hello world";
  observer()->SetGenerationResults(
      "hello%20world", shared_highlighting::LinkGenerationError::kNone,
      shared_highlighting::LinkGenerationReadyStatus::kRequestedAfterReady);
  InitMenu(params);

  histogram_tester.ExpectBucketCount("SharedHighlights.LinkGenerated.Requested",
                                     true, 1);
  histogram_tester.ExpectTotalCount("SharedHighlights.LinkGenerated.Requested",
                                    1);

  histogram_tester.ExpectBucketCount(
      "SharedHighlights.LinkGenerated.RequestedAfterReady", true, 1);
  histogram_tester.ExpectTotalCount(
      "SharedHighlights.LinkGenerated.RequestedAfterReady", 1);
  histogram_tester.ExpectTotalCount(
      "SharedHighlights.LinkGenerated.RequestedBeforeReady", 0);
}

IN_PROC_BROWSER_TEST_F(LinkToTextMenuObserverTest,
                       LinkGenerationRequestedMetric_Success_WithDelay) {
  base::HistogramTester histogram_tester;

  content::BrowserTestClipboardScope test_clipboard_scope;
  content::ContextMenuParams params;
  params.page_url = GURL("http://foo.com/");
  params.selection_text = u"hello world";
  observer()->SetGenerationResults(
      "hello%20world", shared_highlighting::LinkGenerationError::kNone,
      shared_highlighting::LinkGenerationReadyStatus::kRequestedBeforeReady);
  InitMenu(params);

  histogram_tester.ExpectBucketCount("SharedHighlights.LinkGenerated.Requested",
                                     true, 1);
  histogram_tester.ExpectTotalCount("SharedHighlights.LinkGenerated.Requested",
                                    1);

  histogram_tester.ExpectBucketCount(
      "SharedHighlights.LinkGenerated.RequestedBeforeReady", true, 1);
  histogram_tester.ExpectTotalCount(
      "SharedHighlights.LinkGenerated.RequestedBeforeReady", 1);
  histogram_tester.ExpectTotalCount(
      "SharedHighlights.LinkGenerated.RequestedAfterReady", 0);
}

IN_PROC_BROWSER_TEST_F(LinkToTextMenuObserverTest,
                       LinkGenerationRequestedMetric_Failure_NoDelay) {
  base::HistogramTester histogram_tester;

  content::BrowserTestClipboardScope test_clipboard_scope;
  content::ContextMenuParams params;
  params.page_url = GURL("http://foo.com/");
  params.selection_text = u"hello world";
  observer()->SetGenerationResults(
      "", shared_highlighting::LinkGenerationError::kEmptySelection,
      shared_highlighting::LinkGenerationReadyStatus::kRequestedAfterReady);
  InitMenu(params);

  histogram_tester.ExpectBucketCount("SharedHighlights.LinkGenerated.Requested",
                                     false, 1);
  histogram_tester.ExpectTotalCount("SharedHighlights.LinkGenerated.Requested",
                                    1);

  histogram_tester.ExpectBucketCount(
      "SharedHighlights.LinkGenerated.RequestedAfterReady", false, 1);
  histogram_tester.ExpectTotalCount(
      "SharedHighlights.LinkGenerated.RequestedAfterReady", 1);
  histogram_tester.ExpectTotalCount(
      "SharedHighlights.LinkGenerated.RequestedBeforeReady", 0);
}

IN_PROC_BROWSER_TEST_F(LinkToTextMenuObserverTest,
                       LinkGenerationRequestedMetric_Failure_WithDelay) {
  base::HistogramTester histogram_tester;

  content::BrowserTestClipboardScope test_clipboard_scope;
  content::ContextMenuParams params;
  params.page_url = GURL("http://foo.com/");
  params.selection_text = u"hello world";
  observer()->SetGenerationResults(
      "", shared_highlighting::LinkGenerationError::kEmptySelection,
      shared_highlighting::LinkGenerationReadyStatus::kRequestedBeforeReady);
  InitMenu(params);

  histogram_tester.ExpectBucketCount("SharedHighlights.LinkGenerated.Requested",
                                     false, 1);
  histogram_tester.ExpectTotalCount("SharedHighlights.LinkGenerated.Requested",
                                    1);

  histogram_tester.ExpectBucketCount(
      "SharedHighlights.LinkGenerated.RequestedBeforeReady", false, 1);
  histogram_tester.ExpectTotalCount(
      "SharedHighlights.LinkGenerated.RequestedBeforeReady", 1);
  histogram_tester.ExpectTotalCount(
      "SharedHighlights.LinkGenerated.RequestedAfterReady", 0);
}

IN_PROC_BROWSER_TEST_F(LinkToTextMenuObserverTest,
                       CopiesLinkToTextWithExistingFragments) {
  content::BrowserTestClipboardScope test_clipboard_scope;
  content::ContextMenuParams params;
  params.page_url = GURL("http://foo.com/#bar");
  params.selection_text = u"hello world";
  observer()->SetGenerationResults(
      "hello%20world", shared_highlighting::LinkGenerationError::kNone,
      shared_highlighting::LinkGenerationReadyStatus::kRequestedAfterReady);
  InitMenu(params);
  menu()->ExecuteCommand(IDC_CONTENT_CONTEXT_COPYLINKTOTEXT, 0);

  ui::Clipboard* clipboard = ui::Clipboard::GetForCurrentThread();
  std::u16string text;
  clipboard->ReadText(ui::ClipboardBuffer::kCopyPaste, nullptr, &text);
  EXPECT_EQ(u"http://foo.com/#bar:~:text=hello%20world", text);
}

IN_PROC_BROWSER_TEST_F(LinkToTextMenuObserverTest,
                       CopiesLinkToTextWithExistingFragmentsWithTextSelection) {
  content::BrowserTestClipboardScope test_clipboard_scope;
  content::ContextMenuParams params;
  params.page_url = GURL("http://foo.com/#bar:~:text=baz");
  params.selection_text = u"hello world";
  observer()->SetGenerationResults(
      "hello%20world", shared_highlighting::LinkGenerationError::kNone,
      shared_highlighting::LinkGenerationReadyStatus::kRequestedAfterReady);
  InitMenu(params);
  menu()->ExecuteCommand(IDC_CONTENT_CONTEXT_COPYLINKTOTEXT, 0);

  ui::Clipboard* clipboard = ui::Clipboard::GetForCurrentThread();
  std::u16string text;
  clipboard->ReadText(ui::ClipboardBuffer::kCopyPaste, nullptr, &text);
  EXPECT_EQ(u"http://foo.com/#bar:~:text=hello%20world", text);
}

IN_PROC_BROWSER_TEST_F(
    LinkToTextMenuObserverTest,
    CopiesLinkToTextWithExistingFragmentsWithMultipleTextSelections) {
  content::BrowserTestClipboardScope test_clipboard_scope;
  content::ContextMenuParams params;
  params.page_url = GURL("http://foo.com/#bar:~:text=baz&text=qux");
  params.selection_text = u"hello world";
  observer()->SetGenerationResults(
      "hello%20world", shared_highlighting::LinkGenerationError::kNone,
      shared_highlighting::LinkGenerationReadyStatus::kRequestedAfterReady);
  InitMenu(params);
  menu()->ExecuteCommand(IDC_CONTENT_CONTEXT_COPYLINKTOTEXT, 0);

  ui::Clipboard* clipboard = ui::Clipboard::GetForCurrentThread();
  std::u16string text;
  clipboard->ReadText(ui::ClipboardBuffer::kCopyPaste, nullptr, &text);
  EXPECT_EQ(u"http://foo.com/#bar:~:text=hello%20world", text);
}

IN_PROC_BROWSER_TEST_F(
    LinkToTextMenuObserverTest,
    CopiesLinkToTextWithExistingFragmentsWithExistingRefAndTextSelections) {
  content::BrowserTestClipboardScope test_clipboard_scope;
  content::ContextMenuParams params;
  params.page_url =
      GURL("http://foo.com/#bar:~:baz=keep&text=remove&baz=keep2");
  params.selection_text = u"hello world";
  observer()->SetGenerationResults(
      "hello%20world", shared_highlighting::LinkGenerationError::kNone,
      shared_highlighting::LinkGenerationReadyStatus::kRequestedAfterReady);
  InitMenu(params);
  menu()->ExecuteCommand(IDC_CONTENT_CONTEXT_COPYLINKTOTEXT, 0);

  ui::Clipboard* clipboard = ui::Clipboard::GetForCurrentThread();
  std::u16string text;
  clipboard->ReadText(ui::ClipboardBuffer::kCopyPaste, nullptr, &text);
  EXPECT_EQ(u"http://foo.com/#bar:~:baz=keep&baz=keep2&text=hello%20world",
            text);
}

IN_PROC_BROWSER_TEST_F(LinkToTextMenuObserverDataControlsTest,
                       BlocksCopyingLinkToText) {
  data_controls::SetDataControls(browser()->profile()->GetPrefs(), {R"({
                                   "name": "rule_name",
                                   "rule_id": "rule_id",
                                   "sources": {
                                     "urls": ["*"]
                                   },
                                   "restrictions": [
                                     {"class": "CLIPBOARD", "level": "BLOCK"}
                                   ]
                                 })"});
  data_controls::DesktopDataControlsDialogTestHelper helper(
      data_controls::DataControlsDialog::Type::kClipboardCopyBlock);

  content::BrowserTestClipboardScope test_clipboard_scope;
  content::ContextMenuParams params;
  params.page_url = GURL("http://foo.com/");
  params.selection_text = u"hello world";
  observer()->SetGenerationResults(
      "hello%20world", shared_highlighting::LinkGenerationError::kNone,
      shared_highlighting::LinkGenerationReadyStatus::kRequestedAfterReady);
  InitMenu(params);
  menu()->ExecuteCommand(IDC_CONTENT_CONTEXT_COPYLINKTOTEXT, 0);

  helper.WaitForDialogToInitialize();
  helper.CloseDialogWithoutBypass();
  helper.WaitForDialogToClose();

  ui::Clipboard* clipboard = ui::Clipboard::GetForCurrentThread();
  std::u16string text;
  clipboard->ReadText(ui::ClipboardBuffer::kCopyPaste, nullptr, &text);
  EXPECT_TRUE(text.empty());
}

IN_PROC_BROWSER_TEST_F(LinkToTextMenuObserverDataControlsTest,
                       WarnsCopyingLinkToTextAndCancel) {
  data_controls::SetDataControls(browser()->profile()->GetPrefs(), {R"({
                                   "name": "rule_name",
                                   "rule_id": "rule_id",
                                   "sources": {
                                     "urls": ["*"]
                                   },
                                   "restrictions": [
                                     {"class": "CLIPBOARD", "level": "WARN"}
                                   ]
                                 })"});
  data_controls::DesktopDataControlsDialogTestHelper helper(
      data_controls::DataControlsDialog::Type::kClipboardCopyWarn);

  content::BrowserTestClipboardScope test_clipboard_scope;
  content::ContextMenuParams params;
  params.page_url = GURL("http://foo.com/");
  params.selection_text = u"hello world";
  observer()->SetGenerationResults(
      "hello%20world", shared_highlighting::LinkGenerationError::kNone,
      shared_highlighting::LinkGenerationReadyStatus::kRequestedAfterReady);
  InitMenu(params);
  menu()->ExecuteCommand(IDC_CONTENT_CONTEXT_COPYLINKTOTEXT, 0);

  helper.WaitForDialogToInitialize();
  helper.CloseDialogWithoutBypass();
  helper.WaitForDialogToClose();

  ui::Clipboard* clipboard = ui::Clipboard::GetForCurrentThread();
  std::u16string text;
  clipboard->ReadText(ui::ClipboardBuffer::kCopyPaste, nullptr, &text);
  EXPECT_TRUE(text.empty());
}

IN_PROC_BROWSER_TEST_F(LinkToTextMenuObserverDataControlsTest,
                       WarnsCopyingLinkToTextAndBypass) {
  data_controls::SetDataControls(browser()->profile()->GetPrefs(), {R"({
                                   "name": "rule_name",
                                   "rule_id": "rule_id",
                                   "sources": {
                                     "urls": ["*"]
                                   },
                                   "restrictions": [
                                     {"class": "CLIPBOARD", "level": "WARN"}
                                   ]
                                 })"});
  data_controls::DesktopDataControlsDialogTestHelper helper(
      data_controls::DataControlsDialog::Type::kClipboardCopyWarn);

  content::BrowserTestClipboardScope test_clipboard_scope;
  content::ContextMenuParams params;
  params.page_url = GURL("http://foo.com/");
  params.selection_text = u"hello world";
  observer()->SetGenerationResults(
      "hello%20world", shared_highlighting::LinkGenerationError::kNone,
      shared_highlighting::LinkGenerationReadyStatus::kRequestedAfterReady);
  InitMenu(params);
  menu()->ExecuteCommand(IDC_CONTENT_CONTEXT_COPYLINKTOTEXT, 0);

  helper.WaitForDialogToInitialize();
  helper.BypassWarning();
  helper.WaitForDialogToClose();

  ui::Clipboard* clipboard = ui::Clipboard::GetForCurrentThread();
  std::u16string text;
  clipboard->ReadText(ui::ClipboardBuffer::kCopyPaste, nullptr, &text);
  EXPECT_EQ(u"http://foo.com/#:~:text=hello%20world", text);
}

IN_PROC_BROWSER_TEST_F(LinkToTextMenuObserverDataControlsTest,
                       ReplacesCopyingLinkToText) {
  data_controls::SetDataControls(browser()->profile()->GetPrefs(), {R"({
                                   "name": "rule_name",
                                   "rule_id": "rule_id",
                                   "destinations": {
                                     "os_clipboard": true
                                   },
                                   "restrictions": [
                                     {"class": "CLIPBOARD", "level": "BLOCK"}
                                   ]
                                 })"});

  content::BrowserTestClipboardScope test_clipboard_scope;
  content::ContextMenuParams params;
  params.page_url = GURL("http://foo.com/");
  params.selection_text = u"hello world";
  observer()->SetGenerationResults(
      "hello%20world", shared_highlighting::LinkGenerationError::kNone,
      shared_highlighting::LinkGenerationReadyStatus::kRequestedAfterReady);
  InitMenu(params);
  menu()->ExecuteCommand(IDC_CONTENT_CONTEXT_COPYLINKTOTEXT, 0);

  ui::Clipboard* clipboard = ui::Clipboard::GetForCurrentThread();
  std::u16string text;
  clipboard->ReadText(ui::ClipboardBuffer::kCopyPaste, nullptr, &text);
  EXPECT_EQ(u"Pasting this content here is blocked by your administrator.",
            text);
}

IN_PROC_BROWSER_TEST_F(LinkToTextMenuObserverTest, ShowsToastOnCopyingLink) {
  content::BrowserTestClipboardScope test_clipboard_scope;
  content::ContextMenuParams params;
  params.page_url = GURL("http://foo.com/");
  params.selection_text = u"hello world";
  params.opened_from_highlight = true;
  observer()->SetGenerationResults(
      "hello%20world", shared_highlighting::LinkGenerationError::kNone,
      shared_highlighting::LinkGenerationReadyStatus::kRequestedAfterReady);
  InitMenu(params);
  menu()->ExecuteCommand(IDC_CONTENT_CONTEXT_COPYLINKTOTEXT, 0);

  EXPECT_TRUE(browser()->GetFeatures().toast_controller()->IsShowingToast());
}
