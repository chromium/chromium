// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/run_until.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/login/login_handler.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

using web_modal::WebContentsModalDialogManager;

class RepostFormWarningTest : public DialogBrowserTest {
 public:
  RepostFormWarningTest() {}

  RepostFormWarningTest(const RepostFormWarningTest&) = delete;
  RepostFormWarningTest& operator=(const RepostFormWarningTest&) = delete;

  ~RepostFormWarningTest() override {}

  // BrowserTestBase:
  void SetUpOnMainThread() override;

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override;

 protected:
  content::WebContents* TryReload();
};

void RepostFormWarningTest::SetUpOnMainThread() {
  DialogBrowserTest::SetUpOnMainThread();
  ASSERT_TRUE(embedded_test_server()->Start());

  // Load a form.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/form.html")));
  // Submit it.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL("javascript:document.getElementById('form').submit()")));
}

void RepostFormWarningTest::ShowUi(const std::string& name) {
  TryReload();
}

content::WebContents* RepostFormWarningTest::TryReload() {
  // Try to reload it, checking for repost.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  web_contents->GetController().Reload(content::ReloadType::NORMAL, true);
  return web_contents;
}

// If becomes flaky, disable on Windows and use http://crbug.com/47228
IN_PROC_BROWSER_TEST_F(RepostFormWarningTest, TestDoubleReload) {
  // Try to reload it twice, checking for repost.
  content::WebContents* web_contents = TryReload();
  TryReload();

  // There should only be one dialog open.
  WebContentsModalDialogManager* web_contents_modal_dialog_manager =
      WebContentsModalDialogManager::FromWebContents(web_contents);
  EXPECT_TRUE(web_contents_modal_dialog_manager->IsDialogActive());

  // Navigate away from the page (this is when the test usually crashes).
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/bar")));

  // The dialog should've been closed.
  EXPECT_FALSE(web_contents_modal_dialog_manager->IsDialogActive());
}

// If becomes flaky, disable on Windows and use http://crbug.com/47228
IN_PROC_BROWSER_TEST_F(RepostFormWarningTest, TestLoginAfterRepost) {
  // Try to reload it, checking for repost.
  content::WebContents* web_contents = TryReload();

  // Navigate to a page that requires authentication, bringing up another
  // tab-modal sheet.
  browser()->OpenURL(
      content::OpenURLParams(
          embedded_test_server()->GetURL("/auth-basic"), content::Referrer(),
          WindowOpenDisposition::CURRENT_TAB, ui::PAGE_TRANSITION_TYPED, false),
      /*navigation_handle_callback=*/{});
  ASSERT_TRUE(base::test::RunUntil(
      []() { return LoginHandler::GetAllLoginHandlersForTest().size() == 1; }));

  // Try to reload it again.
  web_contents->GetController().Reload(content::ReloadType::NORMAL, true);

  // Navigate away from the page. We can't use ui_test_utils:NavigateToURL
  // because that waits for the current page to stop loading first, which won't
  // happen while the auth dialog is up.
  content::TestNavigationObserver navigation_observer(web_contents);
  browser()->OpenURL(
      content::OpenURLParams(
          embedded_test_server()->GetURL("/bar"), content::Referrer(),
          WindowOpenDisposition::CURRENT_TAB, ui::PAGE_TRANSITION_TYPED, false),
      /*navigation_handle_callback=*/{});
  navigation_observer.Wait();
}

// Disable on Mac OS until dialogs are using toolkit-views for MacViews project.
// https://crbug.com/683356
#if !BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_F(RepostFormWarningTest, InvokeUi_TestRepostWarning) {
  ShowAndVerifyUi();
}
#endif
