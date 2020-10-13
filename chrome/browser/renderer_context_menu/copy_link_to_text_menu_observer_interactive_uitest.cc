// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_context_menu/copy_link_to_text_menu_observer.h"

#include "base/macros.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/renderer_context_menu/mock_render_view_context_menu.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/clipboard.h"

namespace {

class CopyLinkToTextMenuObserverTest : public InProcessBrowserTest {
 public:
  CopyLinkToTextMenuObserverTest();

  void SetUp() override { InProcessBrowserTest::SetUp(); }
  void SetUpOnMainThread() override { Reset(false); }
  void TearDownOnMainThread() override {
    observer_.reset();
    menu_.reset();
  }

  void Reset(bool incognito) {
    menu_ = std::make_unique<MockRenderViewContextMenu>(incognito);
    observer_ = std::make_unique<CopyLinkToTextMenuObserver>(menu_.get());
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
