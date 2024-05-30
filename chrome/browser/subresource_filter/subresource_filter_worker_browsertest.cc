// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/subresource_filter/subresource_filter_browser_test_harness.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace subresource_filter {

class SubresourceFilterWorkerFetchBrowserTest
    : public SubresourceFilterBrowserTest {
 public:
  SubresourceFilterWorkerFetchBrowserTest() = default;

  SubresourceFilterWorkerFetchBrowserTest(
      const SubresourceFilterWorkerFetchBrowserTest&) = delete;
  SubresourceFilterWorkerFetchBrowserTest& operator=(
      const SubresourceFilterWorkerFetchBrowserTest&) = delete;

  ~SubresourceFilterWorkerFetchBrowserTest() override = default;

 protected:
  void RunTest(const std::string& document_path,
               const std::string& filter_path) {
    const std::u16string fetch_succeeded_title = u"FetchSucceeded";
    const std::u16string fetch_failed_title = u"FetchFailed";
    const std::u16string fetch_partially_failed_title = u"FetchPartiallyFailed";

    GURL url(GetTestUrl(document_path));
    ConfigureAsPhishingURL(url);

    // This unrelated rule shouldn't block fetch.
    ASSERT_NO_FATAL_FAILURE(SetRulesetToDisallowURLsWithPathSuffix(
        "suffix-that-does-not-match-anything"));
    {
      content::TitleWatcher title_watcher(
          browser()->tab_strip_model()->GetActiveWebContents(),
          fetch_succeeded_title);
      title_watcher.AlsoWaitForTitle(fetch_failed_title);
      title_watcher.AlsoWaitForTitle(fetch_partially_failed_title);
      ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
      EXPECT_EQ(fetch_succeeded_title, title_watcher.WaitAndGetTitle());
    }
    ClearTitle();

    // This rule should block fetch.
    ASSERT_NO_FATAL_FAILURE(
        SetRulesetToDisallowURLsWithPathSuffix(filter_path));
    {
      content::TitleWatcher title_watcher(
          browser()->tab_strip_model()->GetActiveWebContents(),
          fetch_succeeded_title);
      title_watcher.AlsoWaitForTitle(fetch_failed_title);
      title_watcher.AlsoWaitForTitle(fetch_partially_failed_title);
      ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
      EXPECT_EQ(fetch_failed_title, title_watcher.WaitAndGetTitle());
    }
    ClearTitle();
  }

  void ClearTitle() {
    ASSERT_TRUE(content::ExecJs(web_contents()->GetPrimaryMainFrame(),
                                "document.title = \"\";"));
  }
};

// TODO(crbug.com/40101794): Add more tests for workers like top-level
// worker script fetch and module script fetch.

// Test if fetch() on dedicated workers is blocked by the subresource filter.
IN_PROC_BROWSER_TEST_F(SubresourceFilterWorkerFetchBrowserTest, WorkerFetch) {
  // This fetches "worklet_fetch_data.txt" by fetch().
  RunTest("subresource_filter/worker_fetch.html", "worker_fetch_data.txt");
}

// Test if top-level worklet script fetch is blocked by the subresource filter.
IN_PROC_BROWSER_TEST_F(SubresourceFilterWorkerFetchBrowserTest,
                       WorkletScriptFetch) {
  RunTest("subresource_filter/worklet_script_fetch.html",
          "worklet_script_fetch.js");
}

// Test if static import on worklets is blocked by the subresource filter.
IN_PROC_BROWSER_TEST_F(SubresourceFilterWorkerFetchBrowserTest,
                       WorkletStaticImport) {
  // This fetches "empty.js" by static import.
  RunTest("subresource_filter/worklet_script_fetch.html", "empty.js");
}

// Any network APIs including dynamic import are disallowed on worklets, so we
// don't have to test them.

}  // namespace subresource_filter
