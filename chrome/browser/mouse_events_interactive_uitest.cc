// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_browsertest_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/javascript_dialogs/tab_modal_dialog_manager.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/isolated_world_ids.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/base/test/ui_controls.h"

namespace {

// Integration test of browser event forwarding and web content event handling.
class MouseEventsTest : public InProcessBrowserTest {
 public:
  MouseEventsTest() = default;

  MouseEventsTest(const MouseEventsTest&) = delete;
  MouseEventsTest& operator=(const MouseEventsTest&) = delete;

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
    const std::u16string expected_title(base::ASCIIToUTF16(title));
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
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
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
};

#if BUILDFLAG(IS_MAC)
// Flaky; http://crbug.com/133361.
#define MAYBE_MouseOver DISABLED_MouseOver
#else
#define MAYBE_MouseOver MouseOver
#endif

IN_PROC_BROWSER_TEST_F(MouseEventsTest, MAYBE_MouseOver) {
  NavigateAndWaitForMouseOver();
}

#if BUILDFLAG(IS_MAC)
// Flaky; http://crbug.com/133361.
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

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || \
    BUILDFLAG(IS_WIN)
// Flaky; http://crbug.com/133361.
#define MAYBE_TestOnMouseOut DISABLED_TestOnMouseOut
#else
#define MAYBE_TestOnMouseOut TestOnMouseOut
#endif

IN_PROC_BROWSER_TEST_F(MouseEventsTest, MAYBE_TestOnMouseOut) {
  NavigateAndWaitForMouseOverThenMouseOut();
}

#if BUILDFLAG(IS_WIN)
// Mac/Linux are flaky; http://crbug.com/133361.
IN_PROC_BROWSER_TEST_F(MouseEventsTest, MouseDownOnBrowserCaption) {
  gfx::Rect browser_bounds = browser()->window()->GetBounds();
  ui_controls::SendMouseMove(browser_bounds.x() + 200, browser_bounds.y() + 10);
  ui_controls::SendMouseClick(ui_controls::LEFT);

  NavigateAndWaitForMouseOverThenMouseOut();
}
#endif

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_OZONE)
// Test that a mouseleave is not triggered when showing the context menu.
// If the test is failed, it means that Blink gets the mouseleave event
// when showing the context menu and it could make the unexpecting
// content behavior such as clearing the hover status.
// Please refer to the below issue for understanding what happens .
// Flaky; See http://crbug.com/656101.
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
  tab->GetPrimaryMainFrame()->ExecuteJavaScriptForTests(
      u"done()", base::NullCallback(), content::ISOLATED_WORLD_ID_GLOBAL);
  const std::u16string success_title = u"without mouseleave";
  const std::u16string failure_title = u"with mouseleave";
  content::TitleWatcher done_title_watcher(tab, success_title);
  done_title_watcher.AlsoWaitForTitle(failure_title);
  EXPECT_EQ(success_title, done_title_watcher.WaitAndGetTitle());
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
// Test that a mouseleave is not triggered when showing a modal dialog.
// Sample regression: crbug.com/394672
// Flaky; http://crbug.com/838120
#define MAYBE_ModalDialog DISABLED_ModalDialog
#else
#define MAYBE_ModalDialog ModalDialog
#endif

IN_PROC_BROWSER_TEST_F(MouseEventsTest, MAYBE_ModalDialog) {
  EXPECT_NO_FATAL_FAILURE(NavigateAndWaitForMouseOver());

  content::WebContents* tab = GetActiveWebContents();
  auto* js_dialog_manager =
      javascript_dialogs::TabModalDialogManager::FromWebContents(tab);
  base::RunLoop dialog_wait;
  js_dialog_manager->SetDialogShownCallbackForTesting(
      dialog_wait.QuitClosure());
  tab->GetPrimaryMainFrame()->ExecuteJavaScriptForTests(
      u"alert()", base::NullCallback(), content::ISOLATED_WORLD_ID_GLOBAL);
  dialog_wait.Run();

  // Cancel the dialog.
  js_dialog_manager->HandleJavaScriptDialog(tab, false, nullptr);

  tab->GetPrimaryMainFrame()->ExecuteJavaScriptForTests(
      u"done()", base::NullCallback(), content::ISOLATED_WORLD_ID_GLOBAL);
  const std::u16string success_title = u"without mouseleave";
  const std::u16string failure_title = u"with mouseleave";
  content::TitleWatcher done_title_watcher(tab, success_title);
  done_title_watcher.AlsoWaitForTitle(failure_title);
  EXPECT_EQ(success_title, done_title_watcher.WaitAndGetTitle());
}

}  // namespace
