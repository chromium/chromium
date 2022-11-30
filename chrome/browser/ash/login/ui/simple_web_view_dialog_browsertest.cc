// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/ui/simple_web_view_dialog.h"

#include <memory>

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/login/login_handler_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/views/chrome_test_widget.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/views/widget/widget.h"

namespace ash {

using SimpleWebViewDialogTest = ::InProcessBrowserTest;

// Tests that http auth triggered web dialog does not crash.
IN_PROC_BROWSER_TEST_F(SimpleWebViewDialogTest, HttpAuthWebDialog) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto dialog_ptr = std::make_unique<SimpleWebViewDialog>(browser()->profile());
  auto delegate = dialog_ptr->MakeWidgetDelegate();
  auto* dialog = delegate->SetContentsView(std::move(dialog_ptr));

  views::Widget::InitParams params;
  params.delegate = delegate.release();  // Pass ownership to widget.

  views::UniqueWidgetPtr widget(std::make_unique<ChromeTestWidget>());
  widget->Init(std::move(params));

  // Load a http auth challenged page.
  dialog->StartLoad(embedded_test_server()->GetURL("/auth-basic"));
  dialog->Init();

  // Wait for http auth login view to show up. No crash should happen.
  content::WebContents* contents = dialog->GetActiveWebContents();
  content::NavigationController* controller = &contents->GetController();
  WindowedAuthNeededObserver(controller).Wait();
}

}  // namespace ash
