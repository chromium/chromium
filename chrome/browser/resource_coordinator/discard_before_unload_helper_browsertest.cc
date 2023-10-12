// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/discard_before_unload_helper.h"

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/javascript_dialogs/app_modal_dialog_controller.h"
#include "components/javascript_dialogs/app_modal_dialog_view.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "url/gurl.h"

namespace resource_coordinator {

namespace {

class HasBeforeUnloadHandlerTest : public InProcessBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    embedded_test_server()->StartAcceptingConnections();
  }

  void TestDiscardBeforeUnloadHelper(const char* url,
                                     bool has_beforeunload_helper) {
    GURL gurl(embedded_test_server()->GetURL("a.com", url));
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), gurl));
    auto* wc = browser()->tab_strip_model()->GetActiveWebContents();
    content::PrepContentsForBeforeUnloadTest(wc);

    base::RunLoop run_loop;
    bool callback_invoked = false;
    bool response = false;
    HasBeforeUnloadHandlerCallback callback = base::BindLambdaForTesting(
        [&run_loop, &callback_invoked, &response](bool response_arg) {
          run_loop.Quit();
          callback_invoked = true;
          response = response_arg;
        });

    HasBeforeUnloadHandler(wc, std::move(callback));

    // The callback should not be invoked synchronously. In a world where
    // NeedToFireBeforeUnloadOrUnloadEvents works properly this expectation
    // changes.
    ASSERT_FALSE(callback_invoked);

    // Run the loop until we process the callback.
    run_loop.Run();

    EXPECT_TRUE(callback_invoked);
    EXPECT_EQ(has_beforeunload_helper, response);

    // If we didn't expect to proceed, then that means there's an unload
    // handler. Which means that when we try to exit the browser a dialog will
    // be shown. Wait for it, and accept it to allow the browser to close and
    // the test to complete.
    browser()->tab_strip_model()->CloseAllTabs();
    if (has_beforeunload_helper) {
      javascript_dialogs::AppModalDialogController* alert =
          ui_test_utils::WaitForAppModalDialog();
      ASSERT_TRUE(alert);
      EXPECT_TRUE(alert->is_before_unload_dialog());
      alert->view()->AcceptAppModalDialog();
    }
  }
};

}  // namespace

IN_PROC_BROWSER_TEST_F(HasBeforeUnloadHandlerTest,
                       NonEmptyBeforeUnloadDetected) {
  TestDiscardBeforeUnloadHelper("/beforeunload.html",
                                true /* has_beforeunload_helper */);
}

IN_PROC_BROWSER_TEST_F(HasBeforeUnloadHandlerTest, EmptyBeforeUnloadDetected) {
  TestDiscardBeforeUnloadHelper("/emptybeforeunload.html",
                                false /* has_beforeunload_helper */);
}

IN_PROC_BROWSER_TEST_F(HasBeforeUnloadHandlerTest, NoBeforeUnloadDetected) {
  TestDiscardBeforeUnloadHelper("/empty.html",
                                false /* has_beforeunload_helper */);
}

}  // namespace resource_coordinator
