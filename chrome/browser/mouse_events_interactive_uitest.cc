// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_browsertest_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/javascript_dialogs/javascript_dialog_tab_helper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/base/test/ui_controls.h"

namespace {

// Integration test of browser event forwarding and web content event handling.
class MouseEventsTest : public InProcessBrowserTest {
 public:
  MouseEventsTest() {}

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  }

  content::WebContents* GetActiveWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  // Wait for the active web contents title to match |title|.
  void WaitForTitle(const std::string& title) {
    // Logging added temporarily to track down flakiness cited below.
    LOG(INFO) << "Waiting for title: " << title;
    const base::string16 expected_title(base::ASCIIToUTF16(title));
    content::TitleWatcher title_watcher(GetActiveWebContents(), expected_title);
    ASSERT_EQ(expected_title, title_watcher.WaitAndGetTitle());
  }

  // Load the test page and wait for onmouseover to be called.
  void NavigateAndWaitForMouseOver() {
    ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

    // Move the mouse 2px above the web contents; allows onmouseover after load.
    const gfx::Rect bounds = GetActiveWebContents()->GetContainerBounds();
    ui_controls::SendMouseMove(bounds.CenterPoint().x(), bounds.y() - 2);

    // Navigate to the test page and wait for onload to be called.
    const GURL url = ui_test_utils::GetTestUrl(
        base::FilePath(),
        base::FilePath(FILE_PATH_LITERAL("mouse_events_test.html")));
    ui_test_utils::NavigateToURL(browser(), url);
    WaitForTitle("onload");

    // Move the mouse over the div and wait for onmouseover to be called.
    ui_controls::SendMouseMove(bounds.CenterPoint().x(), bounds.y() + 10);
    WaitForTitle("onmouseover");
  }

  // Load the test page and wait for onmouseover then onmouseout to be called.
  void NavigateAndWaitForMouseOverThenMouseOut() {
    EXPECT_NO_FATAL_FAILURE(NavigateAndWaitForMouseOver());

    // Moving the mouse outside the div should trigger onmouseout.
    const gfx::Rect bounds = GetActiveWebContents()->GetContainerBounds();
    ui_controls::SendMouseMove(bounds.CenterPoint().x(), bounds.y() - 10);
    WaitForTitle("onmouseout");
  }

  DISALLOW_COPY_AND_ASSIGN(MouseEventsTest);
};

#if defined(OS_MACOSX)
// OS_MACOSX: Missing automation provider support: http://crbug.com/45892.
#define MAYBE_MouseOver DISABLED_MouseOver
#else
#define MAYBE_MouseOver MouseOver
#endif

IN_PROC_BROWSER_TEST_F(MouseEventsTest, MAYBE_MouseOver) {
  NavigateAndWaitForMouseOver();
}

#if defined(OS_MACOSX)
// OS_MACOSX: Missing automation provider support: http://crbug.com/45892.
#define MAYBE_ClickAndDoubleClick DISABLED_ClickAndDoubleClick
#else
#define MAYBE_ClickAndDoubleClick ClickAndDoubleClick
#endif

IN_PROC_BROWSER_TEST_F(MouseEventsTest, MAYBE_ClickAndDoubleClick) {
  NavigateAndWaitForMouseOver();

  ui_controls::SendMouseClick(ui_controls::LEFT);
  WaitForTitle("onclick");

  ui_controls::SendMouseClick(ui_controls::LEFT);
  WaitForTitle("ondblclick");
}

#if defined(OS_MACOSX) || defined(OS_LINUX) || defined(OS_WIN)
// OS_MACOSX: Missing automation provider support: http://crbug.com/45892.
// OS_LINUX, OS_WIN: http://crbug.com/133361.
#define MAYBE_TestOnMouseOut DISABLED_TestOnMouseOut
#else
#define MAYBE_TestOnMouseOut TestOnMouseOut
#endif

IN_PROC_BROWSER_TEST_F(MouseEventsTest, MAYBE_TestOnMouseOut) {
  NavigateAndWaitForMouseOverThenMouseOut();
}

#if defined(OS_WIN)
// OS_MACOSX: Missing automation provider support: http://crbug.com/45892
// OS_LINUX: http://crbug.com/133361. interactive mouse tests are flaky.
IN_PROC_BROWSER_TEST_F(MouseEventsTest, MouseDownOnBrowserCaption) {
  gfx::Rect browser_bounds = browser()->window()->GetBounds();
  ui_controls::SendMouseMove(browser_bounds.x() + 200, browser_bounds.y() + 10);
  ui_controls::SendMouseClick(ui_controls::LEFT);

  NavigateAndWaitForMouseOverThenMouseOut();
}
#endif

#if defined(OS_MACOSX) || defined(OS_WIN)
// Test that a mouseleave is not triggered when showing the context menu.
// If the test is failed, it means that Blink gets the mouseleave event
// when showing the context menu and it could make the unexpecting
// content behavior such as clearing the hover status.
// Please refer to the below issue for understanding what happens .
// TODO: Make test pass on OS_WIN and OS_MACOSX
// OS_WIN: Flaky. See http://crbug.com/656101.
// OS_MACOSX: Missing automation provider support: http://crbug.com/45892.
#define MAYBE_ContextMenu DISABLED_ContextMenu
#else
#define MAYBE_ContextMenu ContextMenu
#endif

IN_PROC_BROWSER_TEST_F(MouseEventsTest, MAYBE_ContextMenu) {
  EXPECT_NO_FATAL_FAILURE(NavigateAndWaitForMouseOver());

  ContextMenuWaiter menu_observer;
  ui_controls::SendMouseClick(ui_controls::RIGHT);
  // Wait until the context menu is opened and closed.
  menu_observer.WaitForMenuOpenAndClose();

  content::WebContents* tab = GetActiveWebContents();
  tab->GetMainFrame()->ExecuteJavaScriptForTests(base::ASCIIToUTF16("done()"),
                                                 base::NullCallback());
  const base::string16 success_title = base::ASCIIToUTF16("without mouseleave");
  const base::string16 failure_title = base::ASCIIToUTF16("with mouseleave");
  content::TitleWatcher done_title_watcher(tab, success_title);
  done_title_watcher.AlsoWaitForTitle(failure_title);
  EXPECT_EQ(success_title, done_title_watcher.WaitAndGetTitle());
}

#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX)
// Test that a mouseleave is not triggered when showing a modal dialog.
// Sample regression: crbug.com/394672
// TODO: Make test pass on OS_WIN and OS_MACOSX
// OS_WIN: http://crbug.com/450138
// OS_MACOSX: Missing automation provider support: http://crbug.com/45892.
// OS_LINUX: Flaky http://crbug.com/838120
#define MAYBE_ModalDialog DISABLED_ModalDialog
#else
#define MAYBE_ModalDialog ModalDialog
#endif

IN_PROC_BROWSER_TEST_F(MouseEventsTest, MAYBE_ModalDialog) {
  EXPECT_NO_FATAL_FAILURE(NavigateAndWaitForMouseOver());

  content::WebContents* tab = GetActiveWebContents();
  JavaScriptDialogTabHelper* js_helper =
      JavaScriptDialogTabHelper::FromWebContents(tab);
  base::RunLoop dialog_wait;
  js_helper->SetDialogShownCallbackForTesting(dialog_wait.QuitClosure());
  tab->GetMainFrame()->ExecuteJavaScriptForTests(base::UTF8ToUTF16("alert()"),
                                                 base::NullCallback());
  dialog_wait.Run();

  // Cancel the dialog.
  js_helper->HandleJavaScriptDialog(tab, false, nullptr);

  tab->GetMainFrame()->ExecuteJavaScriptForTests(base::ASCIIToUTF16("done()"),
                                                 base::NullCallback());
  const base::string16 success_title = base::ASCIIToUTF16("without mouseleave");
  const base::string16 failure_title = base::ASCIIToUTF16("with mouseleave");
  content::TitleWatcher done_title_watcher(tab, success_title);
  done_title_watcher.AlsoWaitForTitle(failure_title);
  EXPECT_EQ(success_title, done_title_watcher.WaitAndGetTitle());
}

}  // namespace
