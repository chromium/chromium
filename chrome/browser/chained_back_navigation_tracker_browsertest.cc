// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chained_back_navigation_tracker.h"

#include "base/test/bind.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/navigation_simulator.h"
#include "net/dns/mock_host_resolver.h"

class ChainedBackNavigationTrackerBrowserTest : public InProcessBrowserTest {
 public:
  ChainedBackNavigationTrackerBrowserTest() = default;

  ChainedBackNavigationTrackerBrowserTest(
      const ChainedBackNavigationTrackerBrowserTest&) = delete;
  ChainedBackNavigationTrackerBrowserTest& operator=(
      const ChainedBackNavigationTrackerBrowserTest&) = delete;

  ~ChainedBackNavigationTrackerBrowserTest() override = default;

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
  }

 protected:
  content::WebContents* web_contents() const {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  const uint32_t min_navigation_cnt_ =
      ChainedBackNavigationTracker::kMinimumChainedBackNavigationLength;
  const int64_t max_navigation_interval_ = ChainedBackNavigationTracker::
      kMaxChainedBackNavigationIntervalInMilliseconds;
};

IN_PROC_BROWSER_TEST_F(ChainedBackNavigationTrackerBrowserTest,
                       SubframeBackNavigationIsCountedAsChained) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a1 = embedded_test_server()->GetURL("a1.com", "/title1.html");
  GURL url_a2 = embedded_test_server()->GetURL("a2.com", "/title1.html");
  GURL url_b = embedded_test_server()->GetURL("b.com", "/title1.html");
  GURL url_c = embedded_test_server()->GetURL("c.com", "/title1.html");

  ASSERT_TRUE(content::NavigateToURL(web_contents(), url_a1));
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url_a2));

  ChainedBackNavigationTracker::CreateForWebContents(web_contents());
  ChainedBackNavigationTracker* tracker =
      ChainedBackNavigationTracker::FromWebContents(web_contents());
  ASSERT_TRUE(tracker);

  // The main frame back navigation should increment the count by 1.
  ASSERT_TRUE(content::HistoryGoBack(web_contents()));
  ASSERT_EQ(url_a1, web_contents()->GetLastCommittedURL());
  ASSERT_EQ(1u, tracker->chained_back_navigation_count_);

  // Create a subframe and append it to the document.
  ASSERT_TRUE(
      ExecJs(web_contents(),
             content::JsReplace("let frame = document.createElement('iframe');"
                                "frame.src = $1;"
                                "document.body.appendChild(frame);",
                                url_b)));
  ASSERT_TRUE(content::WaitForLoadStop(web_contents()));
  content::RenderFrameHost* subframe_host =
      ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0);

  // Navigate the subframe away, the chained back navigation count should be
  // reset to 0.
  ASSERT_TRUE(content::ExecJs(
      subframe_host, content::JsReplace("window.location.href = $1;", url_c)));
  ASSERT_TRUE(content::WaitForLoadStop(web_contents()));
  ASSERT_EQ(0u, tracker->chained_back_navigation_count_);

  // The sub frame back navigation should increment the count by 1.
  ASSERT_TRUE(content::HistoryGoBack(web_contents()));
  ASSERT_EQ(1u, tracker->chained_back_navigation_count_);
}

IN_PROC_BROWSER_TEST_F(ChainedBackNavigationTrackerBrowserTest,
                       RendererInitiatedBackNavigationIsNotCountedAsChained) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a = embedded_test_server()->GetURL("a.com", "/title1.html");
  GURL url_b = embedded_test_server()->GetURL("b.com", "/title1.html");
  GURL url_c = embedded_test_server()->GetURL("c.com", "/title1.html");
  GURL url_d = embedded_test_server()->GetURL("d.com", "/title1.html");

  ASSERT_TRUE(content::NavigateToURL(web_contents(), url_a));
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url_b));
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url_c));
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url_d));

  ChainedBackNavigationTracker::CreateForWebContents(web_contents());
  ChainedBackNavigationTracker* tracker =
      ChainedBackNavigationTracker::FromWebContents(web_contents());
  ASSERT_TRUE(tracker);

  // No back navigation is performed yet, the chain length should not be
  // updated.
  ASSERT_EQ(0u, tracker->chained_back_navigation_count_);

  // The back navigation is renderer initiated, it should not increment the
  // chain length.
  ASSERT_TRUE(content::ExecJs(web_contents(), "window.history.back();"));
  ASSERT_TRUE(content::WaitForLoadStop(web_contents()));
  ASSERT_EQ(url_c, web_contents()->GetLastCommittedURL());
  ASSERT_EQ(0u, tracker->chained_back_navigation_count_);

  // The back navigation is browser initiated, it should  increment the chain
  // length.
  ASSERT_TRUE(content::HistoryGoBack(web_contents()));
  ASSERT_EQ(url_b, web_contents()->GetLastCommittedURL());
  ASSERT_EQ(1u, tracker->chained_back_navigation_count_);

  // The back navigation is renderer initiated, it should reset the chain
  // length.
  ASSERT_TRUE(content::ExecJs(web_contents(), "window.history.back();"));
  ASSERT_TRUE(content::WaitForLoadStop(web_contents()));
  ASSERT_EQ(url_a, web_contents()->GetLastCommittedURL());
  ASSERT_EQ(0u, tracker->chained_back_navigation_count_);
}
