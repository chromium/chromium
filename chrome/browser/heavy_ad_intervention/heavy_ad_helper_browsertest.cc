// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/heavy_ad_intervention/heavy_ad_helper.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace {

bool IsContentInDocument(content::RenderFrameHost* rfh, std::string content) {
  std::string script =
      "document.documentElement.innerHTML.includes('" + content + "');";
  // Execute script in an isolated world to avoid causing a Trusted Types
  // violation due to eval.
  return EvalJs(rfh, script, content::EXECUTE_SCRIPT_DEFAULT_OPTIONS,
                /*world_id=*/1)
      .ExtractBool();
}

}  // namespace

class HeavyAdHelperBrowserTest : public InProcessBrowserTest {
 public:
  HeavyAdHelperBrowserTest() {}
  ~HeavyAdHelperBrowserTest() override {}

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    content::SetupCrossSiteRedirector(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());
  }
};

// Verifies that there are no JS errors or missing load time data in the error
// page for heavy ads.
IN_PROC_BROWSER_TEST_F(HeavyAdHelperBrowserTest,
                       HeavyAdErrorPage_NoConsoleMessages) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::NavigationController& controller = web_contents->GetController();

  GURL url(embedded_test_server()->GetURL("/iframe.html"));
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::RenderFrameHost* child =
      ChildFrameAt(web_contents->GetPrimaryMainFrame(), 0);

  content::WebContentsConsoleObserver console_observer(web_contents);

  content::TestNavigationObserver error_observer(web_contents);
  controller.LoadPostCommitErrorPage(
      child, url,
      heavy_ad_intervention::PrepareHeavyAdPage(
          g_browser_process->GetApplicationLocale()));
  error_observer.Wait();

  for (const auto& message : console_observer.messages()) {
    if (message.log_level == blink::mojom::ConsoleMessageLevel::kError) {
      FAIL() << message.message;
    }
  }
}

// Checks that the heavy ad strings are in the html content of the rendered
// error page.
IN_PROC_BROWSER_TEST_F(HeavyAdHelperBrowserTest,
                       HeavyAdErrorPage_HeavyAdStringsUsed) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::NavigationController& controller = web_contents->GetController();

  GURL url(embedded_test_server()->GetURL("/iframe.html"));
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::RenderFrameHost* child =
      ChildFrameAt(web_contents->GetPrimaryMainFrame(), 0);

  content::TestNavigationObserver error_observer(web_contents);
  controller.LoadPostCommitErrorPage(
      child, url,
      heavy_ad_intervention::PrepareHeavyAdPage(
          g_browser_process->GetApplicationLocale()));
  error_observer.Wait();

  child = ChildFrameAt(web_contents->GetPrimaryMainFrame(), 0);

  EXPECT_TRUE(IsContentInDocument(
      child,
      l10n_util::GetStringUTF8(IDS_HEAVY_AD_INTERVENTION_BUTTON_DETAILS)));
  EXPECT_TRUE(IsContentInDocument(
      child, l10n_util::GetStringUTF8(IDS_HEAVY_AD_INTERVENTION_HEADING)));
  EXPECT_TRUE(IsContentInDocument(
      child, l10n_util::GetStringUTF8(IDS_HEAVY_AD_INTERVENTION_SUMMARY)));
}
