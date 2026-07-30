// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/platform_browser_test.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

class ChromeContentBrowserClientPlatformBrowserTest
    : public PlatformBrowserTest {
 public:
  void SetUpOnMainThread() override {
    PlatformBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  content::WebContents* web_contents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }
};

// window.open() with the 'background' window feature from ordinary web content
// must not be allowed to open a new window, even with transient user
// activation.
IN_PROC_BROWSER_TEST_F(ChromeContentBrowserClientPlatformBrowserTest,
                       BackgroundFeatureFromOrdinaryWebContentIsBlocked) {
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(), embedded_test_server()->GetURL("/title1.html")));

  const size_t initial_web_contents_count = content::GetAllWebContents().size();

  // 1. Attempt to open a window with the 'background' feature.
  // Using default EvalJs options executes with user_gesture = true, which is
  // critical: without a user gesture, the standard popup blocker would reject
  // the call before reaching ChromeContentBrowserClient::CanCreateWindow().
  // Even with user activation, ordinary web content lacks extension background
  // permissions and must be refused when requesting 'background' window
  // features.
  EXPECT_EQ(true,
            content::EvalJs(
                web_contents(),
                "window.open('/title2.html', '', 'background') === null"));

  // Verify browser-side state: no new WebContents or tab was created.
  EXPECT_EQ(initial_web_contents_count, content::GetAllWebContents().size());

  // 2. Control assertion: verify that an ordinary popup without 'background'
  // succeeds with transient user activation, proving that the failure above
  // was specifically due to the 'background' window feature and not because
  // popups are globally disabled in the test environment.
  EXPECT_EQ(true,
            content::EvalJs(web_contents(),
                            "window.open('/title2.html', '', '') !== null"));

  // Verify that exactly one new WebContents was created by the control popup.
  EXPECT_EQ(initial_web_contents_count + 1,
            content::GetAllWebContents().size());
}

}  // namespace
