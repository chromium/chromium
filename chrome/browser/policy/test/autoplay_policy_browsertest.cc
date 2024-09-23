// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
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
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"

namespace policy {

namespace {
const char kAutoplayTestPageURL[] = "/media/autoplay_iframe.html";
const char kUnifiedAutoplayTestPageURL[] = "/media/unified_autoplay.html";
}  // namespace

class AutoplayPolicyTest : public PolicyTest {
 public:
  AutoplayPolicyTest() {
    // Start two embedded test servers on different ports. This will ensure
    // the test works correctly with cross origin iframes and site-per-process.
    embedded_test_server2()->AddDefaultHandlers(GetChromeTestDataDir());
    EXPECT_TRUE(embedded_test_server()->Start());
    EXPECT_TRUE(embedded_test_server2()->Start());
  }

  void NavigateToTestPage(const std::string& main_origin = std::string(),
                          const std::string& subframe_origin = std::string()) {
    GURL origin =
        main_origin.empty()
            ? embedded_test_server()->GetURL(kAutoplayTestPageURL)
            : embedded_test_server()->GetURL(main_origin, kAutoplayTestPageURL);
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), origin));

    // Navigate the subframe to the test page but on the second origin.
    GURL origin2 = subframe_origin.empty()
                       ? embedded_test_server2()->GetURL(kAutoplayTestPageURL)
                       : embedded_test_server()->GetURL(subframe_origin,
                                                        kAutoplayTestPageURL);
    std::string script = base::StringPrintf(
        "setTimeout(\""
        "document.getElementById('subframe').src='%s';"
        "\",0)",
        origin2.spec().c_str());
    content::TestNavigationObserver load_observer(GetWebContents());
    EXPECT_TRUE(ExecJs(GetWebContents(), script,
                       content::EXECUTE_SCRIPT_NO_USER_GESTURE));
    load_observer.Wait();
  }

  net::EmbeddedTestServer* embedded_test_server2() {
    return &embedded_test_server2_;
  }

  bool TryAutoplay(content::RenderFrameHost* rfh) {
    return content::EvalJs(rfh, "tryPlayback();",
                           content::EXECUTE_SCRIPT_NO_USER_GESTURE)
        .ExtractBool();
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  content::RenderFrameHost* GetPrimaryMainFrame() {
    return GetWebContents()->GetPrimaryMainFrame();
  }

  content::RenderFrameHost* GetChildFrame() {
    return ChildFrameAt(GetPrimaryMainFrame(), 0);
  }

 private:
  // Second instance of embedded test server to provide a second test origin.
  net::EmbeddedTestServer embedded_test_server2_;
};

IN_PROC_BROWSER_TEST_F(AutoplayPolicyTest, AutoplayAllowedByPolicy) {
  NavigateToTestPage();

  // Check that autoplay was not allowed.
  EXPECT_FALSE(TryAutoplay(GetPrimaryMainFrame()));
  EXPECT_FALSE(TryAutoplay(GetChildFrame()));

  // Update policy to allow autoplay.
  PolicyMap policies;
  SetPolicy(&policies, key::kAutoplayAllowed, base::Value(true));
  UpdateProviderPolicy(policies);

  // Check that autoplay was allowed by policy.
  NavigateToTestPage();
  EXPECT_TRUE(TryAutoplay(GetPrimaryMainFrame()));
  EXPECT_TRUE(TryAutoplay(GetChildFrame()));
}

IN_PROC_BROWSER_TEST_F(AutoplayPolicyTest, CrossOriginIframe) {
  NavigateToTestPage("foo.com", "bar.com");

  // Check that autoplay was not allowed.
  EXPECT_FALSE(TryAutoplay(GetPrimaryMainFrame()));
  EXPECT_FALSE(TryAutoplay(GetChildFrame()));

  // Update policy to allow autoplay.
  PolicyMap policies;
  SetPolicy(&policies, key::kAutoplayAllowed, base::Value(true));
  UpdateProviderPolicy(policies);

  // Check that autoplay was allowed by policy.
  NavigateToTestPage("foo.com", "bar.com");
  EXPECT_TRUE(TryAutoplay(GetPrimaryMainFrame()));
  EXPECT_TRUE(TryAutoplay(GetChildFrame()));
}

// Flaky on Linux. See: crbug.com/1189597.
#if BUILDFLAG(IS_LINUX)
#define MAYBE_AutoplayAllowlist_Allowed DISABLED_AutoplayAllowlist_Allowed
#else
#define MAYBE_AutoplayAllowlist_Allowed AutoplayAllowlist_Allowed
#endif
IN_PROC_BROWSER_TEST_F(AutoplayPolicyTest, MAYBE_AutoplayAllowlist_Allowed) {
  NavigateToTestPage();

  // Check that autoplay was not allowed.
  EXPECT_FALSE(TryAutoplay(GetPrimaryMainFrame()));
  EXPECT_FALSE(TryAutoplay(GetChildFrame()));

  // Create a test allowlist with our origin.
  base::Value::List allowlist;
  allowlist.Append(embedded_test_server()->GetURL("/").spec());

  // Update policy to allow autoplay for our test origin.
  PolicyMap policies;
  SetPolicy(&policies, key::kAutoplayAllowlist,
            base::Value(std::move(allowlist)));
  UpdateProviderPolicy(policies);

  // Check that autoplay was allowed by policy.
  NavigateToTestPage();
  EXPECT_TRUE(TryAutoplay(GetPrimaryMainFrame()));
  EXPECT_TRUE(TryAutoplay(GetChildFrame()));
}

IN_PROC_BROWSER_TEST_F(AutoplayPolicyTest, AutoplayAllowlist_PatternAllowed) {
  NavigateToTestPage();

  // Check that autoplay was not allowed.
  EXPECT_FALSE(TryAutoplay(GetPrimaryMainFrame()));
  EXPECT_FALSE(TryAutoplay(GetChildFrame()));

  // Create a test allowlist with our origin.
  base::Value::List allowlist;
  allowlist.Append("127.0.0.1:*");

  // Update policy to allow autoplay for our test origin.
  PolicyMap policies;
  SetPolicy(&policies, key::kAutoplayAllowlist,
            base::Value(std::move(allowlist)));
  UpdateProviderPolicy(policies);

  // Check that autoplay was allowed by policy.
  NavigateToTestPage();
  EXPECT_TRUE(TryAutoplay(GetPrimaryMainFrame()));
  EXPECT_TRUE(TryAutoplay(GetChildFrame()));
}

IN_PROC_BROWSER_TEST_F(AutoplayPolicyTest, AutoplayAllowlist_Missing) {
  NavigateToTestPage();

  // Check that autoplay was not allowed.
  EXPECT_FALSE(TryAutoplay(GetPrimaryMainFrame()));
  EXPECT_FALSE(TryAutoplay(GetChildFrame()));

  // Create a test allowlist with a random origin.
  base::Value::List allowlist;
  allowlist.Append("https://www.example.com");

  // Update policy to allow autoplay for a random origin.
  PolicyMap policies;
  SetPolicy(&policies, key::kAutoplayAllowlist,
            base::Value(std::move(allowlist)));
  UpdateProviderPolicy(policies);

  // Check that autoplay was not allowed.
  NavigateToTestPage();
  EXPECT_FALSE(TryAutoplay(GetPrimaryMainFrame()));
  EXPECT_FALSE(TryAutoplay(GetChildFrame()));
}

// Flaky on Linux and ChromeOS. See: crbug.com/1172978.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#define MAYBE_AutoplayDeniedByPolicy DISABLED_AutoplayDeniedByPolicy
#else
#define MAYBE_AutoplayDeniedByPolicy AutoplayDeniedByPolicy
#endif
IN_PROC_BROWSER_TEST_F(AutoplayPolicyTest, MAYBE_AutoplayDeniedByPolicy) {
  NavigateToTestPage();

  // Check that autoplay was not allowed.
  EXPECT_FALSE(TryAutoplay(GetPrimaryMainFrame()));
  EXPECT_FALSE(TryAutoplay(GetChildFrame()));

  // Update policy to forbid autoplay.
  PolicyMap policies;
  SetPolicy(&policies, key::kAutoplayAllowed, base::Value(false));
  UpdateProviderPolicy(policies);

  // Check that autoplay was not allowed by policy.
  NavigateToTestPage();
  EXPECT_FALSE(TryAutoplay(GetPrimaryMainFrame()));
  EXPECT_FALSE(TryAutoplay(GetChildFrame()));

  // Create a test allowlist with a random origin.
  base::Value::List allowlist;
  allowlist.Append("https://www.example.com");

  // Update policy to allow autoplay for a random origin.
  SetPolicy(&policies, key::kAutoplayAllowlist,
            base::Value(std::move(allowlist)));
  UpdateProviderPolicy(policies);

  // Check that autoplay was not allowed.
  NavigateToTestPage();
  EXPECT_FALSE(TryAutoplay(GetPrimaryMainFrame()));
  EXPECT_FALSE(TryAutoplay(GetChildFrame()));
}

// Flaky on Linux. See: crbug.com/1172978.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#define MAYBE_AutoplayDeniedAllowedWithURL DISABLED_AutoplayDeniedAllowedWithURL
#else
#define MAYBE_AutoplayDeniedAllowedWithURL AutoplayDeniedAllowedWithURL
#endif
IN_PROC_BROWSER_TEST_F(AutoplayPolicyTest, MAYBE_AutoplayDeniedAllowedWithURL) {
  NavigateToTestPage();

  // Check that autoplay was not allowed.
  EXPECT_FALSE(TryAutoplay(GetPrimaryMainFrame()));
  EXPECT_FALSE(TryAutoplay(GetChildFrame()));

  // Update policy to forbid autoplay.
  PolicyMap policies;
  SetPolicy(&policies, key::kAutoplayAllowed, base::Value(false));
  UpdateProviderPolicy(policies);

  // Check that autoplay was not allowed by policy.
  NavigateToTestPage();
  EXPECT_FALSE(TryAutoplay(GetPrimaryMainFrame()));
  EXPECT_FALSE(TryAutoplay(GetChildFrame()));

  // Create a test allowlist with our test origin.
  base::Value::List allowlist;
  allowlist.Append(embedded_test_server()->GetURL("/").spec());

  // Update policy to allow autoplay for our test origin.
  SetPolicy(&policies, key::kAutoplayAllowlist,
            base::Value(std::move(allowlist)));
  UpdateProviderPolicy(policies);

  // Check that autoplay was allowed by policy.
  NavigateToTestPage();
  EXPECT_TRUE(TryAutoplay(GetPrimaryMainFrame()));
  EXPECT_TRUE(TryAutoplay(GetChildFrame()));
}

// TODO(crbug.com/40742600): Flaky test.
IN_PROC_BROWSER_TEST_F(AutoplayPolicyTest,
                       DISABLED_AutoplayAllowedGlobalAndURL) {
  NavigateToTestPage();

  // Check that autoplay was not allowed.
  EXPECT_FALSE(TryAutoplay(GetPrimaryMainFrame()));
  EXPECT_FALSE(TryAutoplay(GetChildFrame()));

  // Update policy to forbid autoplay.
  PolicyMap policies;
  SetPolicy(&policies, key::kAutoplayAllowed, base::Value(false));
  UpdateProviderPolicy(policies);

  // Check that autoplay was not allowed by policy.
  NavigateToTestPage();
  EXPECT_FALSE(TryAutoplay(GetPrimaryMainFrame()));
  EXPECT_FALSE(TryAutoplay(GetChildFrame()));

  // Create a test allowlist with our test origin.
  base::Value::List allowlist;
  allowlist.Append(embedded_test_server()->GetURL("/").spec());

  // Update policy to allow autoplay for our test origin.
  SetPolicy(&policies, key::kAutoplayAllowlist,
            base::Value(std::move(allowlist)));
  UpdateProviderPolicy(policies);

  // Check that autoplay was allowed by policy.
  NavigateToTestPage();
  EXPECT_TRUE(TryAutoplay(GetPrimaryMainFrame()));
  EXPECT_TRUE(TryAutoplay(GetChildFrame()));
}

class AutoplayPolicyFencedFrameTest : public AutoplayPolicyTest {
 public:
  AutoplayPolicyFencedFrameTest() = default;
  ~AutoplayPolicyFencedFrameTest() override = default;

  // Prevent additional feature/field trial enablement.
  void SetUpCommandLine(base::CommandLine* command_line) override {}

  void NavigateAndCheckAutoplayAllowed(bool expected_result) {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(),
        embedded_test_server()->GetURL(kUnifiedAutoplayTestPageURL)));
    // Append a cross origin fenced frame into the primary main frame.
    content::RenderFrameHost* fenced_frame_host =
        fenced_frame_helper_.CreateFencedFrame(
            GetPrimaryMainFrame(),
            embedded_test_server2()->GetURL(kUnifiedAutoplayTestPageURL));
    ASSERT_NE(nullptr, fenced_frame_host);

    // Check that autoplay works as |expected_result|.
    EXPECT_EQ(TryAutoplay(GetPrimaryMainFrame()), expected_result);
    EXPECT_EQ(TryAutoplay(fenced_frame_host), expected_result);
  }

  bool TryAutoplay(content::RenderFrameHost* rfh) {
    return content::EvalJs(rfh, "attemptPlay();",
                           content::EXECUTE_SCRIPT_NO_USER_GESTURE)
        .ExtractBool();
  }

 protected:
  content::test::FencedFrameTestHelper fenced_frame_helper_;
};

IN_PROC_BROWSER_TEST_F(AutoplayPolicyFencedFrameTest, AutoplayAllowedByPolicy) {
  // Check that autoplay was not allowed.
  NavigateAndCheckAutoplayAllowed(false);

  // Update policy to allow autoplay.
  PolicyMap policies;
  SetPolicy(&policies, key::kAutoplayAllowed, base::Value(true));
  UpdateProviderPolicy(policies);

  // Check that autoplay was allowed by policy.
  NavigateAndCheckAutoplayAllowed(true);
}

IN_PROC_BROWSER_TEST_F(AutoplayPolicyFencedFrameTest,
                       AutoplayAllowlist_Allowed) {
  // Check that autoplay was not allowed.
  NavigateAndCheckAutoplayAllowed(false);

  // Create a test allowlist with our origin.
  base::Value::List allowlist;
  allowlist.Append(embedded_test_server()->GetURL("/").spec());

  // Update policy to allow autoplay for our test origin.
  PolicyMap policies;
  SetPolicy(&policies, key::kAutoplayAllowlist,
            base::Value(std::move(allowlist)));
  UpdateProviderPolicy(policies);

  // Check that autoplay was allowed by policy.
  NavigateAndCheckAutoplayAllowed(true);
}

}  // namespace policy
