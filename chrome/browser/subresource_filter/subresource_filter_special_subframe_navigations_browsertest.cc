// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/bind.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/subresource_filter/subresource_filter_browser_test_harness.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace subresource_filter {

class SubresourceFilterSpecialSubframeNavigationsBrowserTest
    : public SubresourceFilterBrowserTest {};

// Tests that navigations to special URLs (e.g. about:blank, data URLs, etc)
// which do not trigger ReadyToCommitNavigation (and therefore our activation
// IPC), properly inherit the activation of their parent frame.
// Also tests that a child frame of a special url frame inherits the activation
// state of its parent.
IN_PROC_BROWSER_TEST_F(SubresourceFilterSpecialSubframeNavigationsBrowserTest,
                       NavigationsWithNoIPC_HaveActivation) {
  const GURL url(GetTestUrl("subresource_filter/frame_set_special_urls.html"));
  const std::vector<const char*> subframe_names{"blank", "grandChild", "js",
                                                "data", "srcdoc"};
  ConfigureAsPhishingURL(url);

  ui_test_utils::NavigateToURL(browser(), url);
  ASSERT_NO_FATAL_FAILURE(ExpectParsedScriptElementLoadedStatusInFrames(
      subframe_names, {true, true, true, true, true}));

  // Disallow included_script.js, and all frames should filter it in subsequent
  // navigations.
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));
  ui_test_utils::NavigateToURL(browser(), url);
  ASSERT_NO_FATAL_FAILURE(ExpectParsedScriptElementLoadedStatusInFrames(
      subframe_names, {false, false, false, false, false}));
}

// Navigate to a site with site hierarchy a(b(c)). Let a navigate c to a data
// URL, and expect that the resulting frame has activation.
// See crbug.com/739777.
IN_PROC_BROWSER_TEST_F(SubresourceFilterSpecialSubframeNavigationsBrowserTest,
                       NavigateCrossProcessDataUrl_MaintainsActivation) {
  const GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b(c))"));
  ConfigureAsPhishingURL(main_url);
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));
  const GURL included_url(embedded_test_server()->GetURL(
      "a.com", "/subresource_filter/included_script.js"));

  ui_test_utils::NavigateToURL(browser(), main_url);

  // The root node will initiate the navigation; its grandchild node will be the
  // target of the navigation.
  content::TestNavigationObserver navigation_observer(web_contents(), 1);
  EXPECT_TRUE(content::ExecuteScript(
      web_contents()->GetMainFrame(),
      base::StringPrintf(
          "var data_url = 'data:text/html,<script src=\"%s\"></script>';"
          "window.frames[0][0].location.href = data_url;",
          included_url.spec().c_str())));
  navigation_observer.Wait();

  content::RenderFrameHost* target = content::FrameMatchingPredicate(
      web_contents(), base::Bind([](content::RenderFrameHost* rfh) {
        return rfh->GetLastCommittedURL().scheme_piece() == url::kDataScheme;
      }));
  ASSERT_NE(target, nullptr);
  EXPECT_TRUE(target->GetLastCommittedOrigin().opaque());
  EXPECT_FALSE(WasParsedScriptElementLoaded(target));
}

}  // namespace subresource_filter
