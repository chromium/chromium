// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"

namespace crosapi {
namespace {

// Waits for a widget to become active.
void WaitForActivation(views::Widget* widget) {
  views::test::WidgetActivationWaiter waiter(widget, true);
  waiter.Wait();
}

}  // namespace

class WindowLacrosBrowserTest : public InProcessBrowserTest {
 public:
  WindowLacrosBrowserTest() = default;
  WindowLacrosBrowserTest(const WindowLacrosBrowserTest&) = delete;
  WindowLacrosBrowserTest& operator=(const WindowLacrosBrowserTest&) = delete;
  ~WindowLacrosBrowserTest() override = default;
};

IN_PROC_BROWSER_TEST_F(WindowLacrosBrowserTest, Activation) {
  if (!ui::OzonePlatform::GetInstance()
           ->GetPlatformRuntimeProperties()
           .supports_activation)
    return;

  // Showing the first window should implicitly activate.
  Browser* browser1 =
      Browser::Create(Browser::CreateParams(browser()->profile(), false));
  views::Widget* widget1 =
      BrowserView::GetBrowserViewForBrowser(browser1)->GetWidget();
  browser1->window()->Show();
  WaitForActivation(widget1);

  // Showing the second window should implicitly activate.
  Browser* browser2 =
      Browser::Create(Browser::CreateParams(browser()->profile(), false));
  views::Widget* widget2 =
      BrowserView::GetBrowserViewForBrowser(browser2)->GetWidget();
  browser2->window()->Show();
  WaitForActivation(widget2);

  // Check that activating the first browser makes it active.
  widget1->Activate();
  WaitForActivation(widget1);

  // Check that deactivating the first browser makes the second one active.
  widget1->Deactivate();
  WaitForActivation(widget2);
}

}  // namespace crosapi
