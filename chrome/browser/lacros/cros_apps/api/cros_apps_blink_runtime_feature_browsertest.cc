// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

// Tests for content and blink mechanism that controls API exposure.
class CrosAppsBlinkRuntimeFeatureBrowsertest
    : public InProcessBrowserTest,
      private content::WebContentsObserver {
 public:
  CrosAppsBlinkRuntimeFeatureBrowsertest() = default;
  CrosAppsBlinkRuntimeFeatureBrowsertest(
      const CrosAppsBlinkRuntimeFeatureBrowsertest&) = delete;
  CrosAppsBlinkRuntimeFeatureBrowsertest& operator=(
      const CrosAppsBlinkRuntimeFeatureBrowsertest&) = delete;
  ~CrosAppsBlinkRuntimeFeatureBrowsertest() override = default;

  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  // Sets a callback to be invoked during ReadyToCommitNavigation.
  void SetReadyToCommitNavigationCallback(
      base::RepeatingCallback<void(content::NavigationHandle*)> callback) {
    on_ready_to_commit_navigation_ = std::move(callback);
  }

  void ReadyToCommitNavigation(
      content::NavigationHandle* navigation_handle) override {
    on_ready_to_commit_navigation_.Run(navigation_handle);
  }

  void NavigateAndWait(content::WebContents* web_contents, const GURL& url) {
    Observe(web_contents);
    NavigateToURLBlockUntilNavigationsComplete(web_contents, url,
                                               /*num_of_navigations*/ 1);
  }

 private:
  base::RepeatingCallback<void(content::NavigationHandle*)>
      on_ready_to_commit_navigation_ = base::DoNothing();
};

IN_PROC_BROWSER_TEST_F(CrosAppsBlinkRuntimeFeatureBrowsertest,
                       DefaultDisabled) {
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  NavigateAndWait(web_contents, embedded_test_server()->GetURL("/empty.html"));

  EXPECT_EQ(true, content::EvalJs(web_contents,
                                  "typeof window.chromeos === 'undefined';"));
}

IN_PROC_BROWSER_TEST_F(CrosAppsBlinkRuntimeFeatureBrowsertest,
                       SelectivelyEnabledByFeatureState) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Start enabling the feature and check the global object is exposed for a new
  // document.
  SetReadyToCommitNavigationCallback(
      base::BindRepeating([](content::NavigationHandle* navigation_handle) {
        navigation_handle->GetMutableRuntimeFeatureStateContext()
            .SetBlinkExtensionChromeOSEnabled(true);
      }));
  NavigateAndWait(web_contents, embedded_test_server()->GetURL("/empty.html"));
  EXPECT_EQ(true, content::EvalJs(web_contents,
                                  "typeof window.chromeos !== 'undefined';"));

  // Stop enabling the feature and navigate again. The global object isn't
  // exposed to the new page.
  SetReadyToCommitNavigationCallback(base::DoNothing());
  NavigateAndWait(web_contents, embedded_test_server()->GetURL("/empty.html"));
  EXPECT_EQ(true, content::EvalJs(web_contents,
                                  "typeof window.chromeos === 'undefined';"));
}
