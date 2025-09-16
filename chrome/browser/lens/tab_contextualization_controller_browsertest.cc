// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/lens/tab_contextualization_controller.h"

#include "base/test/bind.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace lens {

class TabContextualizationControllerBrowserTest : public InProcessBrowserTest {
 public:
  TabContextualizationControllerBrowserTest() = default;
  ~TabContextualizationControllerBrowserTest() override = default;

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
    InProcessBrowserTest::SetUpOnMainThread();
  }

 protected:
  TabContextualizationController* GetTabContextualizationController() {
    tabs::TabInterface* tab = tabs::TabInterface::GetFromContents(
        browser()->tab_strip_model()->GetWebContentsAt(0));
    return TabContextualizationController::From(tab);
  }
};

IN_PROC_BROWSER_TEST_F(TabContextualizationControllerBrowserTest,
                       EligibilityIsTrueForEligiblePage) {
  auto* controller = GetTabContextualizationController();

  GURL url(embedded_test_server()->GetURL("/empty.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  base::RunLoop run_loop;
  controller->UpdatePageContextEligibility(
      base::BindOnce(&TabContextualizationController::OnEligibilityChecked,
                     base::Unretained(controller))
          .Then(run_loop.QuitClosure()));
  run_loop.Run();

  EXPECT_TRUE(controller->GetCurrentPageContextEligibility());
}

IN_PROC_BROWSER_TEST_F(TabContextualizationControllerBrowserTest,
                       EligibilityIsFalseForIneligiblePage) {
  auto* controller = GetTabContextualizationController();

  GURL url(embedded_test_server()->GetURL("/empty.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  base::RunLoop run_loop;
  controller->UpdatePageContextEligibility(
      base::BindOnce(&TabContextualizationController::OnEligibilityChecked,
                     base::Unretained(controller))
          .Then(run_loop.QuitClosure()));
  run_loop.Run();

  controller->OnEligibilityChecked(false);

  EXPECT_FALSE(controller->GetCurrentPageContextEligibility());
}

IN_PROC_BROWSER_TEST_F(TabContextualizationControllerBrowserTest,
                       GetPageContext) {
  auto* controller = GetTabContextualizationController();

  GURL url(embedded_test_server()->GetURL("/title1.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  base::RunLoop run_loop;
  controller->GetPageContext(base::BindLambdaForTesting(
      [&](std::unique_ptr<lens::ContextualInputData> data) {
        EXPECT_EQ(data->page_url, url);
        EXPECT_TRUE(data->page_title.has_value());
        EXPECT_TRUE(data->viewport_screenshot.has_value());
        run_loop.Quit();
      }));
  run_loop.Run();
}

}  // namespace lens
