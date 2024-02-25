// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros_apps/cros_apps_test_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

// Tests for content and blink mechanism that controls API exposure.
//
// This test directly controls Blink RuntimeFeatureState thus doesn't use
// CrosAppsApiTest (which enables CrosApps flags to control feature exposure
// based on feature control rules).
class CrosAppsBlinkRuntimeFeatureBrowserTest
    : public InProcessBrowserTest,
      private content::WebContentsObserver {
 public:
  CrosAppsBlinkRuntimeFeatureBrowserTest() = default;
  CrosAppsBlinkRuntimeFeatureBrowserTest(
      const CrosAppsBlinkRuntimeFeatureBrowserTest&) = delete;
  CrosAppsBlinkRuntimeFeatureBrowserTest& operator=(
      const CrosAppsBlinkRuntimeFeatureBrowserTest&) = delete;
  ~CrosAppsBlinkRuntimeFeatureBrowserTest() override = default;

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

IN_PROC_BROWSER_TEST_F(CrosAppsBlinkRuntimeFeatureBrowserTest,
                       DefaultDisabled_GlobalChromeOS) {
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  NavigateAndWait(web_contents, embedded_test_server()->GetURL("/empty.html"));

  EXPECT_EQ(false, IsIdentifierDefined(web_contents, "window.chromeos"));
}

IN_PROC_BROWSER_TEST_F(CrosAppsBlinkRuntimeFeatureBrowserTest,
                       SelectivelyEnabledByFeatureState_GlobalChromeOS) {
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
  EXPECT_EQ(true, IsIdentifierDefined(web_contents, "window.chromeos"));

  // Stop enabling the feature and navigate again. The global object isn't
  // exposed to the new page.
  SetReadyToCommitNavigationCallback(base::DoNothing());
  NavigateAndWait(web_contents, embedded_test_server()->GetURL("/empty.html"));
  EXPECT_EQ(false, IsIdentifierDefined(web_contents, "window.chromeos"));
}

IN_PROC_BROWSER_TEST_F(CrosAppsBlinkRuntimeFeatureBrowserTest,
                       DefaultDisabled_ChromeOS_Diagnostics) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  NavigateAndWait(web_contents, embedded_test_server()->GetURL("/empty.html"));
  EXPECT_EQ(false, IsIdentifierDefined(web_contents, "window.chromeos"));

  // Enable diagnostic feature without the global chromeos shouldn't expose the
  // feature.
  SetReadyToCommitNavigationCallback(
      base::BindRepeating([](content::NavigationHandle* navigation_handle) {
        navigation_handle->GetMutableRuntimeFeatureStateContext()
            .SetBlinkExtensionDiagnosticsEnabled(true);
      }));
  EXPECT_EQ(false, IsIdentifierDefined(web_contents, "window.chromeos"));
}

IN_PROC_BROWSER_TEST_F(CrosAppsBlinkRuntimeFeatureBrowserTest,
                       SelectivelyEnabledByFeatureState_ChromeOS_Diagnostics) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Start enabling the feature and the feature attribute is exposed.
  SetReadyToCommitNavigationCallback(
      base::BindRepeating([](content::NavigationHandle* navigation_handle) {
        navigation_handle->GetMutableRuntimeFeatureStateContext()
            .SetBlinkExtensionChromeOSEnabled(true);
        navigation_handle->GetMutableRuntimeFeatureStateContext()
            .SetBlinkExtensionDiagnosticsEnabled(true);
      }));
  NavigateAndWait(web_contents, embedded_test_server()->GetURL("/empty.html"));
  EXPECT_EQ(true, IsIdentifierDefined(web_contents, "window.chromeos"));
  EXPECT_EQ(true,
            IsIdentifierDefined(web_contents, "window.chromeos.diagnostics"));

  // Stop enabling the feature, but still enable the global object.
  SetReadyToCommitNavigationCallback(
      base::BindRepeating([](content::NavigationHandle* navigation_handle) {
        navigation_handle->GetMutableRuntimeFeatureStateContext()
            .SetBlinkExtensionChromeOSEnabled(true);
      }));
  NavigateAndWait(web_contents, embedded_test_server()->GetURL("/empty.html"));
  EXPECT_EQ(true, IsIdentifierDefined(web_contents, "window.chromeos"));
  EXPECT_EQ(false,
            IsIdentifierDefined(web_contents, "window.chromeos.diagnostics"));
}
