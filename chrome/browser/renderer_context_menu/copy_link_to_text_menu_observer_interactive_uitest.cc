// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/renderer_context_menu/copy_link_to_text_menu_observer.h"

#include "base/macros.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/renderer_context_menu/mock_render_view_context_menu.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/clipboard.h"

namespace {

class CopyLinkToTextMenuObserverTest : public extensions::ExtensionBrowserTest {
 public:
  CopyLinkToTextMenuObserverTest();

  void SetUp() override { InProcessBrowserTest::SetUp(); }
  void SetUpOnMainThread() override {
    extensions::ExtensionBrowserTest::SetUpOnMainThread();
    Reset(false);

    host_resolver()->AddRule("*", "127.0.0.1");

    // Add content/test/data for cross_site_iframe_factory.html
    embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");

    ASSERT_TRUE(embedded_test_server()->Start());
  }
  void TearDownOnMainThread() override {
    observer_.reset();
    menu_.reset();
  }

  void Reset(bool incognito) {
    menu_ = std::make_unique<MockRenderViewContextMenu>(incognito);
    observer_ = CopyLinkToTextMenuObserver::Create(menu_.get());
    menu_->SetObserver(observer_.get());
  }

  void InitMenu(content::ContextMenuParams params) {
    observer_->InitMenu(params);
  }

  ~CopyLinkToTextMenuObserverTest() override;
  MockRenderViewContextMenu* menu() { return menu_.get(); }
  CopyLinkToTextMenuObserver* observer() { return observer_.get(); }

 private:
  std::unique_ptr<CopyLinkToTextMenuObserver> observer_;
  std::unique_ptr<MockRenderViewContextMenu> menu_;
  DISALLOW_COPY_AND_ASSIGN(CopyLinkToTextMenuObserverTest);
};

CopyLinkToTextMenuObserverTest::CopyLinkToTextMenuObserverTest() = default;
CopyLinkToTextMenuObserverTest::~CopyLinkToTextMenuObserverTest() = default;

}  // namespace

IN_PROC_BROWSER_TEST_F(CopyLinkToTextMenuObserverTest, AddsMenuItem) {
  content::ContextMenuParams params;
  InitMenu(params);
  EXPECT_EQ(1u, menu()->GetMenuSize());
  MockRenderViewContextMenu::MockMenuItem item;
  menu()->GetMenuItem(0, &item);
  EXPECT_EQ(IDC_CONTENT_CONTEXT_COPYLINKTOTEXT, item.command_id);
  EXPECT_TRUE(item.enabled);
  EXPECT_FALSE(item.checked);
  EXPECT_FALSE(item.hidden);
}

IN_PROC_BROWSER_TEST_F(CopyLinkToTextMenuObserverTest, CopiesLinkToText) {
  content::BrowserTestClipboardScope test_clipboard_scope;
  content::ContextMenuParams params;
  params.page_url = GURL("http://foo.com/");
  params.selection_text = base::UTF8ToUTF16("hello world");
  observer()->OverrideGeneratedSelectorForTesting("hello%20world");
  InitMenu(params);
  menu()->ExecuteCommand(IDC_CONTENT_CONTEXT_COPYLINKTOTEXT, 0);

  ui::Clipboard* clipboard = ui::Clipboard::GetForCurrentThread();
  base::string16 text;
  clipboard->ReadText(ui::ClipboardBuffer::kCopyPaste, nullptr, &text);
  EXPECT_EQ(base::UTF8ToUTF16(
                "\"hello world\"\nhttp://foo.com/#:~:text=hello%20world"),
            text);
}

IN_PROC_BROWSER_TEST_F(CopyLinkToTextMenuObserverTest,
                       CopiesLinkForEmptySelector) {
  content::BrowserTestClipboardScope test_clipboard_scope;
  content::ContextMenuParams params;
  params.page_url = GURL("http://foo.com/");
  params.selection_text = base::UTF8ToUTF16("hello world");
  observer()->OverrideGeneratedSelectorForTesting("");
  InitMenu(params);
  menu()->ExecuteCommand(IDC_CONTENT_CONTEXT_COPYLINKTOTEXT, 0);

  ui::Clipboard* clipboard = ui::Clipboard::GetForCurrentThread();
  base::string16 text;
  clipboard->ReadText(ui::ClipboardBuffer::kCopyPaste, nullptr, &text);
  EXPECT_EQ(base::UTF8ToUTF16("\"hello world\"\nhttp://foo.com/"), text);
}

IN_PROC_BROWSER_TEST_F(CopyLinkToTextMenuObserverTest, ReplacesRefInURL) {
  content::BrowserTestClipboardScope test_clipboard_scope;
  content::ContextMenuParams params;
  params.page_url = GURL("http://foo.com/#:~:text=hello%20world");
  params.selection_text = base::UTF8ToUTF16("hello world");
  observer()->OverrideGeneratedSelectorForTesting("hello");
  InitMenu(params);
  menu()->ExecuteCommand(IDC_CONTENT_CONTEXT_COPYLINKTOTEXT, 0);

  ui::Clipboard* clipboard = ui::Clipboard::GetForCurrentThread();
  base::string16 text;
  clipboard->ReadText(ui::ClipboardBuffer::kCopyPaste, nullptr, &text);
  EXPECT_EQ(base::UTF8ToUTF16("\"hello world\"\nhttp://foo.com/#:~:text=hello"),
            text);
}

// crbug.com/1139864
IN_PROC_BROWSER_TEST_F(CopyLinkToTextMenuObserverTest,
                       InvalidSelectorForIframe) {
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
  params.selection_text = base::UTF8ToUTF16("hello world");
  InitMenu(params);
  menu()->ExecuteCommand(IDC_CONTENT_CONTEXT_COPYLINKTOTEXT, 0);

  ui::Clipboard* clipboard = ui::Clipboard::GetForCurrentThread();
  base::string16 text;
  clipboard->ReadText(ui::ClipboardBuffer::kCopyPaste, nullptr, &text);
  EXPECT_EQ(base::UTF8ToUTF16("\"hello world\"\n" + main_url.spec()), text);
}

IN_PROC_BROWSER_TEST_F(CopyLinkToTextMenuObserverTest, HiddenForExtensions) {
  const extensions::Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("simple_with_file"));
  ui_test_utils::NavigateToURL(browser(),
                               extension->GetResourceURL("file.html"));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  menu()->set_web_contents(web_contents);

  std::unique_ptr<CopyLinkToTextMenuObserver> observer =
      CopyLinkToTextMenuObserver::Create(menu());
  EXPECT_EQ(nullptr, observer);
}
