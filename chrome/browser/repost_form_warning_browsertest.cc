// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/run_until.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/login/login_handler.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/strings/grit/components_strings.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/base/net_errors.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "ui/views/test/dialog_test.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"

using web_modal::WebContentsModalDialogManager;

class RepostFormWarningTest : public DialogBrowserTest {
 public:
  RepostFormWarningTest() = default;

  RepostFormWarningTest(const RepostFormWarningTest&) = delete;
  RepostFormWarningTest& operator=(const RepostFormWarningTest&) = delete;

  ~RepostFormWarningTest() override = default;

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

// If becomes flaky, disable on Windows and use http://crbug.com/40411916
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

// If becomes flaky, disable on Windows and use http://crbug.com/40411916
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
// https://crbug.com/41296226
#if !BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_F(RepostFormWarningTest, InvokeUi_TestRepostWarning) {
  ShowAndVerifyUi();
}
#endif

// Verifies that confirming form resubmission after navigating back to an
// uncacheable POST page resubmits the POST data (https://crbug.com/553614977).
IN_PROC_BROWSER_TEST_F(RepostFormWarningTest,
                       ConfirmResubmissionAfterBackNavigation) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Submit a form to an uncacheable URL (/echoall/nocache) with POST data.
  GURL echo_url = embedded_test_server()->GetURL("/echoall/nocache");
  {
    content::TestNavigationObserver observer(web_contents);
    ASSERT_TRUE(content::ExecJs(
        web_contents, content::JsReplace(
                          R"(let form = document.createElement('form');
               form.method = 'POST';
               form.action = $1;
               let input = document.createElement('input');
               input.name = 'text';
               input.value = 'val';
               form.appendChild(input);
               document.body.appendChild(form);
               form.submit();)",
                          echo_url)));
    observer.Wait();
    EXPECT_TRUE(observer.last_navigation_succeeded());
  }
  EXPECT_EQ(echo_url, web_contents->GetLastCommittedURL());

  // Verify that the initial POST was successful and body was echoed.
  EXPECT_EQ(
      "text=val\n",
      content::EvalJs(web_contents,
                      "document.getElementsByTagName('pre')[0].innerText;"));

  // Navigate forward to another page.
  GURL other_url = embedded_test_server()->GetURL("/title2.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), other_url));
  EXPECT_EQ(other_url, web_contents->GetLastCommittedURL());

  // Navigate back to the POST page. Because it is uncacheable, this results in
  // net::ERR_CACHE_MISS and displays the "Confirm Form Resubmission" error
  // page.
  {
    content::TestNavigationObserver back_observer(web_contents);
    web_contents->GetController().GoBack();
    back_observer.Wait();
    EXPECT_FALSE(back_observer.last_navigation_succeeded());
    EXPECT_EQ(net::ERR_CACHE_MISS, back_observer.last_net_error_code());
  }
  EXPECT_EQ(echo_url, web_contents->GetLastCommittedURL());

  // Reload the page, checking for repost.
  web_contents->GetController().Reload(content::ReloadType::NORMAL,
                                       /*check_for_repost=*/true);

  // Wait for the repost warning dialog to become active.
  WebContentsModalDialogManager* modal_dialog_manager =
      WebContentsModalDialogManager::FromWebContents(web_contents);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return modal_dialog_manager->IsDialogActive(); }));

  // Find the dialog widget.
  views::Widget* dialog_widget = nullptr;
  for (views::Widget* widget : views::test::WidgetTest::GetAllWidgets()) {
    if (widget->widget_delegate() &&
        widget->widget_delegate()->AsDialogDelegate() &&
        widget->widget_delegate()->GetWindowTitle() ==
            l10n_util::GetStringUTF16(IDS_HTTP_POST_WARNING_TITLE)) {
      dialog_widget = widget;
      break;
    }
  }
  ASSERT_TRUE(dialog_widget);

  // Accept the dialog ("Continue") and wait for the reload to finish.
  content::TestNavigationObserver reload_observer(web_contents);
  views::test::AcceptDialog(dialog_widget);
  reload_observer.Wait();
  EXPECT_TRUE(reload_observer.last_navigation_succeeded());
  EXPECT_EQ(echo_url, web_contents->GetLastCommittedURL());

  // Verify that the reload was a POST request.
  std::string request_headers =
      content::EvalJs(web_contents,
                      "document.getElementById('request-headers').innerText;")
          .ExtractString();
  EXPECT_THAT(request_headers, ::testing::HasSubstr("POST /echoall/nocache"));

  // Verify that the POST body was resubmitted and echoed in the page body.
  EXPECT_EQ(
      "text=val\n",
      content::EvalJs(web_contents,
                      "document.getElementsByTagName('pre')[0].innerText;"));
}
