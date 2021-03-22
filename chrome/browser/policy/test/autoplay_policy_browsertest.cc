// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"

namespace policy {

namespace {
const char kAutoplayTestPageURL[] = "/media/autoplay_iframe.html";
}

class AutoplayPolicyTest : public PolicyTest {
 public:
  AutoplayPolicyTest() {
    // Start two embedded test servers on different ports. This will ensure
    // the test works correctly with cross origin iframes and site-per-process.
    embedded_test_server2()->AddDefaultHandlers(GetChromeTestDataDir());
    EXPECT_TRUE(embedded_test_server()->Start());
    EXPECT_TRUE(embedded_test_server2()->Start());
  }

  void NavigateToTestPage() {
    GURL origin = embedded_test_server()->GetURL(kAutoplayTestPageURL);
    ui_test_utils::NavigateToURL(browser(), origin);

    // Navigate the subframe to the test page but on the second origin.
    GURL origin2 = embedded_test_server2()->GetURL(kAutoplayTestPageURL);
    std::string script = base::StringPrintf(
        "setTimeout(\""
        "document.getElementById('subframe').src='%s';"
        "\",0)",
        origin2.spec().c_str());
    content::TestNavigationObserver load_observer(GetWebContents());
    EXPECT_TRUE(ExecuteScriptWithoutUserGesture(GetWebContents(), script));
    load_observer.Wait();
  }

  net::EmbeddedTestServer* embedded_test_server2() {
    return &embedded_test_server2_;
  }

  bool TryAutoplay(content::RenderFrameHost* rfh) {
    bool result = false;

    EXPECT_TRUE(content::ExecuteScriptWithoutUserGestureAndExtractBool(
        rfh, "tryPlayback();", &result));

    return result;
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  content::RenderFrameHost* GetMainFrame() {
    return GetWebContents()->GetMainFrame();
  }

  content::RenderFrameHost* GetChildFrame() {
    return GetWebContents()->GetAllFrames()[1];
  }

 private:
  // Second instance of embedded test server to provide a second test origin.
  net::EmbeddedTestServer embedded_test_server2_;
};

IN_PROC_BROWSER_TEST_F(AutoplayPolicyTest, AutoplayAllowedByPolicy) {
  NavigateToTestPage();

  // Check that autoplay was not allowed.
  EXPECT_FALSE(TryAutoplay(GetMainFrame()));
  EXPECT_FALSE(TryAutoplay(GetChildFrame()));

  // Update policy to allow autoplay.
  PolicyMap policies;
  SetPolicy(&policies, key::kAutoplayAllowed, base::Value(true));
  UpdateProviderPolicy(policies);

  // Check that autoplay was allowed by policy.
  NavigateToTestPage();
  EXPECT_TRUE(TryAutoplay(GetMainFrame()));
  EXPECT_TRUE(TryAutoplay(GetChildFrame()));
}

// Flaky on Linux. See: crbug.com/1189597.
#if defined(OS_LINUX)
#define MAYBE_AutoplayWhitelist_Allowed DISABLED_AutoplayWhitelist_Allowed
#else
#define MAYBE_AutoplayWhitelist_Allowed AutoplayWhitelist_Allowed
#endif
IN_PROC_BROWSER_TEST_F(AutoplayPolicyTest, MAYBE_AutoplayWhitelist_Allowed) {
  NavigateToTestPage();

  // Check that autoplay was not allowed.
  EXPECT_FALSE(TryAutoplay(GetMainFrame()));
  EXPECT_FALSE(TryAutoplay(GetChildFrame()));

  // Create a test whitelist with our origin.
  std::vector<base::Value> whitelist;
  whitelist.push_back(base::Value(embedded_test_server()->GetURL("/").spec()));

  // Update policy to allow autoplay for our test origin.
  PolicyMap policies;
  SetPolicy(&policies, key::kAutoplayAllowlist, base::Value(whitelist));
  UpdateProviderPolicy(policies);

  // Check that autoplay was allowed by policy.
  NavigateToTestPage();
  EXPECT_TRUE(TryAutoplay(GetMainFrame()));
  EXPECT_TRUE(TryAutoplay(GetChildFrame()));
}

IN_PROC_BROWSER_TEST_F(AutoplayPolicyTest, AutoplayWhitelist_PatternAllowed) {
  NavigateToTestPage();

  // Check that autoplay was not allowed.
  EXPECT_FALSE(TryAutoplay(GetMainFrame()));
  EXPECT_FALSE(TryAutoplay(GetChildFrame()));

  // Create a test whitelist with our origin.
  std::vector<base::Value> whitelist;
  whitelist.push_back(base::Value("127.0.0.1:*"));

  // Update policy to allow autoplay for our test origin.
  PolicyMap policies;
  SetPolicy(&policies, key::kAutoplayAllowlist, base::Value(whitelist));
  UpdateProviderPolicy(policies);

  // Check that autoplay was allowed by policy.
  NavigateToTestPage();
  EXPECT_TRUE(TryAutoplay(GetMainFrame()));
  EXPECT_TRUE(TryAutoplay(GetChildFrame()));
}

IN_PROC_BROWSER_TEST_F(AutoplayPolicyTest, AutoplayWhitelist_Missing) {
  NavigateToTestPage();

  // Check that autoplay was not allowed.
  EXPECT_FALSE(TryAutoplay(GetMainFrame()));
  EXPECT_FALSE(TryAutoplay(GetChildFrame()));

  // Create a test whitelist with a random origin.
  std::vector<base::Value> whitelist;
  whitelist.push_back(base::Value("https://www.example.com"));

  // Update policy to allow autoplay for a random origin.
  PolicyMap policies;
  SetPolicy(&policies, key::kAutoplayAllowlist, base::Value(whitelist));
  UpdateProviderPolicy(policies);

  // Check that autoplay was not allowed.
  NavigateToTestPage();
  EXPECT_FALSE(TryAutoplay(GetMainFrame()));
  EXPECT_FALSE(TryAutoplay(GetChildFrame()));
}

// Flaky on Linux. See: crbug.com/1172978.
#if defined(OS_LINUX)
#define MAYBE_AutoplayDeniedByPolicy DISABLED_AutoplayDeniedByPolicy
#else
#define MAYBE_AutoplayDeniedByPolicy AutoplayDeniedByPolicy
#endif
IN_PROC_BROWSER_TEST_F(AutoplayPolicyTest, MAYBE_AutoplayDeniedByPolicy) {
  NavigateToTestPage();

  // Check that autoplay was not allowed.
  EXPECT_FALSE(TryAutoplay(GetMainFrame()));
  EXPECT_FALSE(TryAutoplay(GetChildFrame()));

  // Update policy to forbid autoplay.
  PolicyMap policies;
  SetPolicy(&policies, key::kAutoplayAllowed, base::Value(false));
  UpdateProviderPolicy(policies);

  // Check that autoplay was not allowed by policy.
  NavigateToTestPage();
  EXPECT_FALSE(TryAutoplay(GetMainFrame()));
  EXPECT_FALSE(TryAutoplay(GetChildFrame()));

  // Create a test whitelist with a random origin.
  std::vector<base::Value> whitelist;
  whitelist.push_back(base::Value("https://www.example.com"));

  // Update policy to allow autoplay for a random origin.
  SetPolicy(&policies, key::kAutoplayAllowlist, base::Value(whitelist));
  UpdateProviderPolicy(policies);

  // Check that autoplay was not allowed.
  NavigateToTestPage();
  EXPECT_FALSE(TryAutoplay(GetMainFrame()));
  EXPECT_FALSE(TryAutoplay(GetChildFrame()));
}

IN_PROC_BROWSER_TEST_F(AutoplayPolicyTest, AutoplayDeniedAllowedWithURL) {
  NavigateToTestPage();

  // Check that autoplay was not allowed.
  EXPECT_FALSE(TryAutoplay(GetMainFrame()));
  EXPECT_FALSE(TryAutoplay(GetChildFrame()));

  // Update policy to forbid autoplay.
  PolicyMap policies;
  SetPolicy(&policies, key::kAutoplayAllowed, base::Value(false));
  UpdateProviderPolicy(policies);

  // Check that autoplay was not allowed by policy.
  NavigateToTestPage();
  EXPECT_FALSE(TryAutoplay(GetMainFrame()));
  EXPECT_FALSE(TryAutoplay(GetChildFrame()));

  // Create a test whitelist with our test origin.
  std::vector<base::Value> whitelist;
  whitelist.push_back(base::Value(embedded_test_server()->GetURL("/").spec()));

  // Update policy to allow autoplay for our test origin.
  SetPolicy(&policies, key::kAutoplayAllowlist, base::Value(whitelist));
  UpdateProviderPolicy(policies);

  // Check that autoplay was allowed by policy.
  NavigateToTestPage();
  EXPECT_TRUE(TryAutoplay(GetMainFrame()));
  EXPECT_TRUE(TryAutoplay(GetChildFrame()));
}

// TODO(crbug.com/1167239): Flaky test.
IN_PROC_BROWSER_TEST_F(AutoplayPolicyTest,
                       DISABLED_AutoplayAllowedGlobalAndURL) {
  NavigateToTestPage();

  // Check that autoplay was not allowed.
  EXPECT_FALSE(TryAutoplay(GetMainFrame()));
  EXPECT_FALSE(TryAutoplay(GetChildFrame()));

  // Update policy to forbid autoplay.
  PolicyMap policies;
  SetPolicy(&policies, key::kAutoplayAllowed, base::Value(false));
  UpdateProviderPolicy(policies);

  // Check that autoplay was not allowed by policy.
  NavigateToTestPage();
  EXPECT_FALSE(TryAutoplay(GetMainFrame()));
  EXPECT_FALSE(TryAutoplay(GetChildFrame()));

  // Create a test whitelist with our test origin.
  std::vector<base::Value> whitelist;
  whitelist.push_back(base::Value(embedded_test_server()->GetURL("/").spec()));

  // Update policy to allow autoplay for our test origin.
  SetPolicy(&policies, key::kAutoplayAllowlist, base::Value(whitelist));
  UpdateProviderPolicy(policies);

  // Check that autoplay was allowed by policy.
  NavigateToTestPage();
  EXPECT_TRUE(TryAutoplay(GetMainFrame()));
  EXPECT_TRUE(TryAutoplay(GetChildFrame()));
}

}  // namespace policy
