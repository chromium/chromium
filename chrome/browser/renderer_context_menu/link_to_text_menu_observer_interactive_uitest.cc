// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_context_menu/link_to_text_menu_observer.h"

#include "base/macros.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/renderer_context_menu/mock_render_view_context_menu.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/shared_highlighting/core/common/shared_highlighting_features.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/clipboard.h"

namespace {

class LinkToTextMenuObserverTest : public extensions::ExtensionBrowserTest,
                                   public ::testing::WithParamInterface<bool> {
 public:
  LinkToTextMenuObserverTest();

  void SetUp() override {
    base::test::ScopedFeatureList scoped_feature_list;
    if (GetParam()) {
      scoped_feature_list.InitWithFeatures(
          {shared_highlighting::kPreemptiveLinkToTextGeneration,
           shared_highlighting::kSharedHighlightingUseBlocklist},
          {});
    } else {
      scoped_feature_list.InitWithFeatures(
          {shared_highlighting::kSharedHighlightingUseBlocklist},
          {shared_highlighting::kPreemptiveLinkToTextGeneration});
    }
    InProcessBrowserTest::SetUp();
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
    content::RenderFrameHost* main_frame = web_contents->GetMainFrame();
    EXPECT_TRUE(ExecuteScript(main_frame, "window.focus();"));
  }
  void TearDownOnMainThread() override {
    observer_.reset();
    menu_.reset();
  }

  void Reset(bool incognito) {
    menu_ = std::make_unique<MockRenderViewContextMenu>(incognito);
    observer_ = LinkToTextMenuObserver::Create(menu_.get());
    menu_->SetObserver(observer_.get());
  }

  void InitMenu(content::ContextMenuParams params) {
    observer_->InitMenu(params);
  }

  bool ShouldPreemptivelyGenerateLink() { return GetParam(); }

  ~LinkToTextMenuObserverTest() override;
  MockRenderViewContextMenu* menu() { return menu_.get(); }
  LinkToTextMenuObserver* observer() { return observer_.get(); }

 private:
  std::unique_ptr<LinkToTextMenuObserver> observer_;
  std::unique_ptr<MockRenderViewContextMenu> menu_;
  DISALLOW_COPY_AND_ASSIGN(LinkToTextMenuObserverTest);
};

LinkToTextMenuObserverTest::LinkToTextMenuObserverTest() = default;
LinkToTextMenuObserverTest::~LinkToTextMenuObserverTest() = default;

}  // namespace

IN_PROC_BROWSER_TEST_P(LinkToTextMenuObserverTest, AddsCopyMenuItem) {
  content::ContextMenuParams params;
  params.page_url = GURL("http://foo.com/");
  params.selection_text = u"hello world";
  observer()->OverrideGeneratedSelectorForTesting(std::string());
  InitMenu(params);
  EXPECT_EQ(1u, menu()->GetMenuSize());
  MockRenderViewContextMenu::MockMenuItem item;
  menu()->GetMenuItem(0, &item);
  EXPECT_EQ(IDC_CONTENT_CONTEXT_COPYLINKTOTEXT, item.command_id);
  EXPECT_FALSE(item.checked);
  EXPECT_FALSE(item.hidden);
  if (ShouldPreemptivelyGenerateLink()) {
    EXPECT_FALSE(item.enabled);
  } else {
    EXPECT_TRUE(item.enabled);
  }
}

IN_PROC_BROWSER_TEST_P(LinkToTextMenuObserverTest, AddsCopyAndRemoveMenuItems) {
  content::ContextMenuParams params;
  params.page_url = GURL("http://foo.com/");
  params.opened_from_highlight = true;
  observer()->OverrideGeneratedSelectorForTesting(std::string());
  InitMenu(params);
  EXPECT_EQ(2u, menu()->GetMenuSize());
  MockRenderViewContextMenu::MockMenuItem item;

  // Check Copy item.
  menu()->GetMenuItem(0, &item);
  EXPECT_EQ(IDC_CONTENT_CONTEXT_COPYLINKTOTEXT, item.command_id);
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

IN_PROC_BROWSER_TEST_P(LinkToTextMenuObserverTest, CopiesLinkToText) {
  content::BrowserTestClipboardScope test_clipboard_scope;
  content::ContextMenuParams params;
  params.page_url = GURL("http://foo.com/");
  params.selection_text = u"hello world";
  observer()->OverrideGeneratedSelectorForTesting("hello%20world");
  InitMenu(params);
  menu()->ExecuteCommand(IDC_CONTENT_CONTEXT_COPYLINKTOTEXT, 0);

  ui::Clipboard* clipboard = ui::Clipboard::GetForCurrentThread();
  std::u16string text;
  clipboard->ReadText(ui::ClipboardBuffer::kCopyPaste, nullptr, &text);
  EXPECT_EQ(u"http://foo.com/#:~:text=hello%20world", text);
}

IN_PROC_BROWSER_TEST_P(LinkToTextMenuObserverTest, CopiesLinkForEmptySelector) {
  content::BrowserTestClipboardScope test_clipboard_scope;
  content::ContextMenuParams params;
  params.page_url = GURL("http://foo.com/");
  params.selection_text = u"hello world";
  observer()->OverrideGeneratedSelectorForTesting(std::string());
  InitMenu(params);

  if (ShouldPreemptivelyGenerateLink()) {
    EXPECT_FALSE(
        menu()->IsCommandIdEnabled(IDC_CONTENT_CONTEXT_COPYLINKTOTEXT));
  } else {
    menu()->ExecuteCommand(IDC_CONTENT_CONTEXT_COPYLINKTOTEXT, 0);
    ui::Clipboard* clipboard = ui::Clipboard::GetForCurrentThread();
    std::u16string text;
    clipboard->ReadText(ui::ClipboardBuffer::kCopyPaste, nullptr, &text);
    EXPECT_EQ(u"http://foo.com/", text);
  }
}

IN_PROC_BROWSER_TEST_P(LinkToTextMenuObserverTest, ReplacesRefInURL) {
  content::BrowserTestClipboardScope test_clipboard_scope;
  content::ContextMenuParams params;
  params.page_url = GURL("http://foo.com/#:~:text=hello%20world");
  params.selection_text = u"hello world";
  observer()->OverrideGeneratedSelectorForTesting("hello");
  InitMenu(params);
  menu()->ExecuteCommand(IDC_CONTENT_CONTEXT_COPYLINKTOTEXT, 0);

  ui::Clipboard* clipboard = ui::Clipboard::GetForCurrentThread();
  std::u16string text;
  clipboard->ReadText(ui::ClipboardBuffer::kCopyPaste, nullptr, &text);
  EXPECT_EQ(u"http://foo.com/#:~:text=hello", text);
}

// crbug.com/1139864
IN_PROC_BROWSER_TEST_P(LinkToTextMenuObserverTest, InvalidSelectorForIframe) {
  GURL main_url(
      embedded_test_server()->GetURL("a.com", "/page_with_iframe.html"));

  ui_test_utils::NavigateToURL(browser(), main_url);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::RenderFrameHost* main_frame_a = web_contents->GetMainFrame();
  content::RenderFrameHost* child_frame_b = ChildFrameAt(main_frame_a, 0);
  EXPECT_TRUE(ExecuteScript(child_frame_b, "window.focus();"));
  EXPECT_EQ(child_frame_b, web_contents->GetFocusedFrame());

  menu()->set_web_contents(web_contents);

  content::BrowserTestClipboardScope test_clipboard_scope;
  content::ContextMenuParams params;
  params.page_url = main_url;
  params.selection_text = u"hello world";
  InitMenu(params);

  if (ShouldPreemptivelyGenerateLink()) {
    EXPECT_FALSE(
        menu()->IsCommandIdEnabled(IDC_CONTENT_CONTEXT_COPYLINKTOTEXT));
  } else {
    menu()->ExecuteCommand(IDC_CONTENT_CONTEXT_COPYLINKTOTEXT, 0);
    ui::Clipboard* clipboard = ui::Clipboard::GetForCurrentThread();
    std::u16string text;
    clipboard->ReadText(ui::ClipboardBuffer::kCopyPaste, nullptr, &text);
    EXPECT_EQ(base::UTF8ToUTF16(main_url.spec()), text);
  }
}

IN_PROC_BROWSER_TEST_P(LinkToTextMenuObserverTest, HiddenForExtensions) {
  const extensions::Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("simple_with_file"));
  ui_test_utils::NavigateToURL(browser(),
                               extension->GetResourceURL("file.html"));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  menu()->set_web_contents(web_contents);

  std::unique_ptr<LinkToTextMenuObserver> observer =
      LinkToTextMenuObserver::Create(menu());
  EXPECT_EQ(nullptr, observer);
}

IN_PROC_BROWSER_TEST_P(LinkToTextMenuObserverTest, Blocklist) {
  content::BrowserTestClipboardScope test_clipboard_scope;
  content::ContextMenuParams params;
  params.page_url = GURL("http://facebook.com/my-profile");
  params.selection_text = u"hello world";
  InitMenu(params);

  if (ShouldPreemptivelyGenerateLink()) {
    EXPECT_FALSE(
        menu()->IsCommandIdEnabled(IDC_CONTENT_CONTEXT_COPYLINKTOTEXT));
  } else {
    menu()->ExecuteCommand(IDC_CONTENT_CONTEXT_COPYLINKTOTEXT, 0);
    ui::Clipboard* clipboard = ui::Clipboard::GetForCurrentThread();
    std::u16string text;
    clipboard->ReadText(ui::ClipboardBuffer::kCopyPaste, nullptr, &text);
    EXPECT_EQ(u"http://facebook.com/my-profile", text);
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         LinkToTextMenuObserverTest,
                         ::testing::Values(true, false));
