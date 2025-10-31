// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/local_network_access/local_network_access_browsertest_base.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/permissions/permission_request_manager.h"
#include "components/policy/policy_constants.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "services/network/public/cpp/network_switches.h"

// Local Network Access browser tests related to enterprise policies.

namespace local_network_access {

// Path to a response that passes Local Network Access checks.
constexpr char kLnaPath[] =
    "/set-header"
    "?Access-Control-Allow-Origin: *";

constexpr char kWorkerHtmlPath[] =
    "/local_network_access/request-from-worker-as-public-address.html";

constexpr char kSharedWorkerHtmlPath[] =
    "/local_network_access/fetch-from-shared-worker-as-public-address.html";

constexpr char kServiceWorkerHtmlPath[] =
    "/local_network_access/fetch-from-service-worker-as-public-address.html";

class LocalNetworkAccessPoliciesBrowserTest
    : public LocalNetworkAccessBrowserTestBase {};

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessPoliciesBrowserTest,
                       CheckEnterprisePolicyEnableLNA) {
  policy::PolicyMap policies;
  SetPolicy(&policies, policy::key::kLocalNetworkAccessRestrictionsEnabled,
            std::optional<base::Value>(true));
  UpdateProviderPolicy(policies);

  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      https_server().GetURL(
          "a.com",
          "/local_network_access/no-favicon-treat-as-public-address.html")));

  // Enable auto-denial of LNA permission request.
  bubble_factory()->set_response_type(
      permissions::PermissionRequestManager::AutoResponseType::DENY_ALL);

  // Expect LNA fetch to fail.
  EXPECT_THAT(content::EvalJs(
                  web_contents(),
                  content::JsReplace("fetch($1).then(response => response.ok)",
                                     https_server().GetURL("b.com", kLnaPath))),
              content::EvalJsResult::IsError());
}

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessPoliciesBrowserTest,
                       CheckEnterprisePolicyOptOut) {
  policy::PolicyMap policies;
  SetPolicy(&policies,
            policy::key::kLocalNetworkAccessRestrictionsTemporaryOptOut,
            std::optional<base::Value>(true));
  UpdateProviderPolicy(policies);

  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      https_server().GetURL(
          "a.com",
          "/local_network_access/no-favicon-treat-as-public-address.html")));

  // Enable auto-denial of LNA permission request.
  bubble_factory()->set_response_type(
      permissions::PermissionRequestManager::AutoResponseType::DENY_ALL);

  // Expect LNA fetch to succeed.
  ASSERT_EQ(true,
            content::EvalJs(
                web_contents(),
                content::JsReplace("fetch($1).then(response => response.ok)",
                                   https_server().GetURL("b.com", kLnaPath))));
}

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessPoliciesBrowserTest,
                       CheckEnterprisePolicyOptOutDedicatedWorker) {
  policy::PolicyMap policies;
  SetPolicy(&policies,
            policy::key::kLocalNetworkAccessRestrictionsTemporaryOptOut,
            std::optional<base::Value>(true));
  UpdateProviderPolicy(policies);
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(), https_server().GetURL("a.com", kWorkerHtmlPath)));

  GURL fetch_url = https_server().GetURL("b.com", kLnaPath);
  std::string_view script_template = "fetch_from_worker($1);";
  // URL fetched, body is just the header that's set.
  EXPECT_EQ("Access-Control-Allow-Origin: *",
            content::EvalJs(web_contents(),
                            content::JsReplace(script_template, fetch_url)));

  CheckCounter(WebFeature::kPrivateNetworkAccessWithinWorker, 1);
  CheckCounter(WebFeature::kLocalNetworkAccessWithinDedicatedWorker, 1);
}

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessPoliciesBrowserTest,
                       CheckEnterprisePolicyOptOutServiceWorker) {
  policy::PolicyMap policies;
  SetPolicy(&policies,
            policy::key::kLocalNetworkAccessRestrictionsTemporaryOptOut,
            std::optional<base::Value>(true));
  UpdateProviderPolicy(policies);
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(), https_server().GetURL("a.com", kServiceWorkerHtmlPath)));

  EXPECT_EQ("ready", content::EvalJs(web_contents(), "setup();"));
  GURL fetch_url = https_server().GetURL("b.com", kLnaPath);
  std::string_view script_template = "fetch_from_service_worker($1);";
  // Fetched URL
  EXPECT_EQ("Access-Control-Allow-Origin: *",
            content::EvalJs(web_contents(),
                            content::JsReplace(script_template, fetch_url)));
}

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessPoliciesBrowserTest,
                       CheckEnterprisePolicyOptOutSharedWorker) {
  policy::PolicyMap policies;
  SetPolicy(&policies,
            policy::key::kLocalNetworkAccessRestrictionsTemporaryOptOut,
            std::optional<base::Value>(true));
  UpdateProviderPolicy(policies);

  ASSERT_TRUE(content::NavigateToURL(
      web_contents(), https_server().GetURL("a.com", kSharedWorkerHtmlPath)));

  // Enable auto-deny of LNA permission request.
  bubble_factory()->set_response_type(
      permissions::PermissionRequestManager::AutoResponseType::ACCEPT_ALL);

  GURL fetch_url = https_server().GetURL("b.com", kLnaPath);
  std::string_view script_template = "fetch_from_shared_worker($1);";
  EXPECT_EQ("Access-Control-Allow-Origin: *",
            content::EvalJs(web_contents(),
                            content::JsReplace(script_template, fetch_url)));
  CheckCounter(WebFeature::kPrivateNetworkAccessWithinWorker, 1);
  CheckCounter(WebFeature::kLocalNetworkAccessWithinSharedWorker, 1);
}

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessPoliciesBrowserTest,
                       CheckEnterprisePolicyOptOutIframeNav) {
  policy::PolicyMap policies;
  SetPolicy(&policies,
            policy::key::kLocalNetworkAccessRestrictionsTemporaryOptOut,
            std::optional<base::Value>(true));
  UpdateProviderPolicy(policies);

  GURL initial_url =
      https_server().GetURL("a.com", "/local_network_access/no-favicon.html");
  GURL nav_url = https_server().GetURL("c.com", "/defaultresponse");
  GURL iframe_url = https_server().GetURL(
      "b.com",
      "/local_network_access/"
      "client-redirect-treat-as-public-address.html?url=" +
          nav_url.spec());
  ASSERT_TRUE(content::NavigateToURL(web_contents(), initial_url));

  content::TestNavigationManager iframe_url_nav_manager(web_contents(),
                                                        iframe_url);
  content::TestNavigationManager nav_url_nav_manager(web_contents(), nav_url);
  std::string_view script_template = R"(
    const child = document.createElement("iframe");
    child.src = $1;
    child.allow = "local-network-access";
    document.body.appendChild(child);
  )";
  EXPECT_THAT(content::EvalJs(web_contents(),
                              content::JsReplace(script_template, iframe_url)),
              content::EvalJsResult::IsOk());
  // Check that the child iframe was successfully fetched.
  ASSERT_TRUE(iframe_url_nav_manager.WaitForNavigationFinished());
  EXPECT_TRUE(iframe_url_nav_manager.was_successful());

  ASSERT_TRUE(nav_url_nav_manager.WaitForNavigationFinished());
  EXPECT_TRUE(nav_url_nav_manager.was_successful());
}

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessPoliciesBrowserTest,
                       LocalNetworkAccessAllowedForUrlsPolicy) {
  policy::PolicyMap policies;
  base::Value::List allowlist;
  allowlist.Append(base::Value("*"));
  SetPolicy(&policies, policy::key::kLocalNetworkAccessAllowedForUrls,
            base::Value(std::move(allowlist)));
  UpdateProviderPolicy(policies);

  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      https_server().GetURL(
          "a.com",
          "/local_network_access/no-favicon-treat-as-public-address.html")));

  // LNA fetch should pass.
  ASSERT_EQ(true,
            content::EvalJs(
                web_contents(),
                content::JsReplace("fetch($1).then(response => response.ok)",
                                   https_server().GetURL("b.com", kLnaPath))));
}

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessPoliciesBrowserTest,
                       LocalNetworkAccessBlockedForUrlsPolicy) {
  // Set both policies. Block should override Allow
  policy::PolicyMap policies;
  base::Value::List allowlist;
  allowlist.Append(base::Value("*"));
  SetPolicy(&policies, policy::key::kLocalNetworkAccessAllowedForUrls,
            base::Value(std::move(allowlist)));
  base::Value::List blocklist;
  blocklist.Append(base::Value("*"));
  SetPolicy(&policies, policy::key::kLocalNetworkAccessBlockedForUrls,
            base::Value(std::move(blocklist)));
  UpdateProviderPolicy(policies);

  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      https_server().GetURL(
          "a.com",
          "/local_network_access/no-favicon-treat-as-public-address.html")));

  // Enable auto-accept of LNA permission request, although it should not be
  // checked.
  bubble_factory()->set_response_type(
      permissions::PermissionRequestManager::AutoResponseType::ACCEPT_ALL);

  // LNA fetch should fail.
  EXPECT_THAT(content::EvalJs(
                  web_contents(),
                  content::JsReplace("fetch($1).then(response => response.ok)",
                                     https_server().GetURL("b.com", kLnaPath))),
              content::EvalJsResult::IsError());
}

// Test that using the LNA allow policy override on an HTTP url works in
// conjunction with setting the kUnsafelyTreatInsecureOriginAsSecure command
// line switch.
class LocalNetworkAccessHttpCommandLineOverrideBrowserTest
    : public LocalNetworkAccessBrowserTestBase {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) final {
    LocalNetworkAccessBrowserTestBase::SetUpCommandLine(command_line);

    command_line->AppendSwitchASCII(
        network::switches::kUnsafelyTreatInsecureOriginAsSecure,
        embedded_test_server()->GetURL("a.com", "/").spec());
  }
};

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessHttpCommandLineOverrideBrowserTest,
                       LocalNetworkAccessAllowedForHttpUrlsPolicy) {
  policy::PolicyMap policies;
  base::Value::List allowlist;
  allowlist.Append(base::Value("*"));
  SetPolicy(&policies, policy::key::kLocalNetworkAccessAllowedForUrls,
            base::Value(std::move(allowlist)));
  UpdateProviderPolicy(policies);

  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      embedded_test_server()->GetURL(
          "a.com",
          "/local_network_access/no-favicon-treat-as-public-address.html")));

  // LNA fetch should pass.
  ASSERT_EQ(true,
            content::EvalJs(
                web_contents(),
                content::JsReplace("fetch($1).then(response => response.ok)",
                                   https_server().GetURL("b.com", kLnaPath))));
}

// Test that using the LNA allow policy override on an HTTP url works in
// conjunction with setting the kOverrideSecurityRestrictionsOnInsecureOrigin
// enterprise policy.
class LocalNetworkAccessHttpPolicyOverrideBrowserTest
    : public LocalNetworkAccessBrowserTestBase {
 protected:
  void SetUpInProcessBrowserTestFixture() override {
    LocalNetworkAccessBrowserTestBase::SetUpInProcessBrowserTestFixture();

    policy::PolicyMap policies;
    base::Value::List secureList;
    secureList.Append(
        base::Value(embedded_test_server()->GetURL("a.com", "/").spec()));
    SetPolicy(&policies,
              policy::key::kOverrideSecurityRestrictionsOnInsecureOrigin,
              base::Value(std::move(secureList)));
    base::Value::List allowlist;
    allowlist.Append(base::Value("*"));
    SetPolicy(&policies, policy::key::kLocalNetworkAccessAllowedForUrls,
              base::Value(std::move(allowlist)));
    UpdateProviderPolicy(policies);
  }
};

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessHttpPolicyOverrideBrowserTest,
                       LocalNetworkAccessAllowedForHttpUrlsPolicy) {
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      embedded_test_server()->GetURL(
          "a.com",
          "/local_network_access/no-favicon-treat-as-public-address.html")));

  // LNA fetch should pass.
  ASSERT_EQ(true,
            content::EvalJs(
                web_contents(),
                content::JsReplace("fetch($1).then(response => response.ok)",
                                   https_server().GetURL("b.com", kLnaPath))));
}

}  // namespace local_network_access
