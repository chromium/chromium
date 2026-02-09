// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>

#include "base/strings/stringprintf.h"
#include "chrome/browser/local_network_access/local_network_access_browsertest_base.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/policy/policy_constants.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/cpp/features.h"

// Local Network Access browser tests pertaining to splitting the initial
// local-network-access permission into 2 permissions: local-network and
// loopback-network

namespace local_network_access {

namespace {
constexpr char kTreatAsPublicAddressPath[] =
    "/local_network_access/no-favicon-treat-as-public-address.html";

// Path to a response that passes Local Network Access checks.
constexpr char kLnaPath[] =
    "/set-header"
    "?Access-Control-Allow-Origin: *";

std::string QueryPermissionScript(std::string_view permission) {
  return content::JsReplace(
      "navigator.permissions.query({ name: $1 }).then((result) => "
      "result.state)",
      permission);
}

std::string WaitForPermissionScript(std::string_view permission) {
  return content::JsReplace(
      R"(
        (async () => {
          const status = await navigator.permissions.query({name: $1});
          if (status.state !== 'prompt') return status.state;
          await new Promise(resolve => {
            status.onchange = () => {
              resolve();
            };
          });
          return status.state;
        })()
      )",
      permission);
}

}  // namespace

class LocalNetworkAccessSplitPermissionOffBrowserTest
    : public LocalNetworkAccessBrowserTestBase {
 public:
  LocalNetworkAccessSplitPermissionOffBrowserTest() {
    feature_list_.InitAndDisableFeature(
        network::features::kLocalNetworkAccessChecksSplitPermissions);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

class LocalNetworkAccessSplitPermissionOnBrowserTest
    : public LocalNetworkAccessBrowserTestBase {
 public:
  void RunIframeNavigationTest(const GURL& initial_url,
                               const GURL& iframe_url,
                               const GURL& nav_url,
                               const std::string permission_policy,
                               bool expect_nav_failure) {
    ASSERT_TRUE(content::NavigateToURL(web_contents(), initial_url));

    // Enable auto-accept of LNA permission request.
    bubble_factory()->set_response_type(
        permissions::PermissionRequestManager::AutoResponseType::ACCEPT_ALL);

    content::TestNavigationManager iframe_url_nav_manager(web_contents(),
                                                          iframe_url);
    content::TestNavigationManager nav_url_nav_manager(web_contents(), nav_url);
    std::string_view script_template = R"(
      const child = document.createElement("iframe");
      child.src = $1;
      child.allow = $2;
      document.body.appendChild(child);
    )";
    EXPECT_THAT(content::EvalJs(web_contents(),
                                content::JsReplace(script_template, iframe_url,
                                                   permission_policy)),
                content::EvalJsResult::IsOk());
    // Check that the child iframe was successfully fetched.
    ASSERT_TRUE(iframe_url_nav_manager.WaitForNavigationFinished());
    EXPECT_TRUE(iframe_url_nav_manager.was_successful());

    ASSERT_TRUE(nav_url_nav_manager.WaitForNavigationFinished());
    if (expect_nav_failure) {
      EXPECT_FALSE(nav_url_nav_manager.was_successful());
    } else {
      EXPECT_TRUE(nav_url_nav_manager.was_successful());
    }
  }

 private:
  base::test::ScopedFeatureList feature_list_{
      network::features::kLocalNetworkAccessChecksSplitPermissions};
};

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessSplitPermissionOffBrowserTest,
                       QueryPermissions) {
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      https_server().GetURL("a.com", kTreatAsPublicAddressPath)));

  ASSERT_EQ("prompt",
            content::EvalJs(web_contents(),
                            QueryPermissionScript("local-network-access")));
  EXPECT_THAT(
      content::EvalJs(web_contents(), QueryPermissionScript("local-network")),
      content::EvalJsResult::IsError());
  EXPECT_THAT(content::EvalJs(web_contents(),
                              QueryPermissionScript("loopback-network")),
              content::EvalJsResult::IsError());
}

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessSplitPermissionOnBrowserTest,
                       QueryPermissions) {
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      https_server().GetURL("a.com", kTreatAsPublicAddressPath)));

  ASSERT_EQ("prompt",
            content::EvalJs(web_contents(),
                            QueryPermissionScript("local-network-access")));
  ASSERT_EQ("prompt", content::EvalJs(web_contents(),
                                      QueryPermissionScript("local-network")));
  ASSERT_EQ("prompt", content::EvalJs(web_contents(), QueryPermissionScript(
                                                          "loopback-network")));
}

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessSplitPermissionOnBrowserTest,
                       FetchDenyPermissionLoopback) {
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      https_server().GetURL("a.com", kTreatAsPublicAddressPath)));

  // Enable auto-deny of LNA permission request.
  bubble_factory()->set_response_type(
      permissions::PermissionRequestManager::AutoResponseType::DENY_ALL);

  // LNA fetch should fail.
  EXPECT_THAT(content::EvalJs(
                  web_contents(),
                  content::JsReplace("fetch($1).then(response => response.ok)",
                                     https_server().GetURL("b.com", kLnaPath))),
              content::EvalJsResult::IsError());

  // It should be the loopback-network permission that is denied ...
  ASSERT_EQ("denied", content::EvalJs(web_contents(), QueryPermissionScript(
                                                          "loopback-network")));
  // and not the local-network permission.
  ASSERT_EQ("prompt", content::EvalJs(web_contents(),
                                      QueryPermissionScript("local-network")));

  // Querying old permission should give the non-ask value.
  ASSERT_EQ("denied",
            content::EvalJs(web_contents(),
                            QueryPermissionScript("local-network-access")));
}

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessSplitPermissionOnBrowserTest,
                       FetchDenyPermissionLocal) {
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      https_server().GetURL("a.com", kTreatAsPublicAddressPath)));

  // Enable auto-accept of LNA permission request.
  bubble_factory()->set_response_type(
      permissions::PermissionRequestManager::AutoResponseType::DENY_ALL);

  // LNA fetch should fail.
  EXPECT_THAT(
      content::EvalJs(
          web_contents(),
          content::JsReplace("fetch($1).then(response => response.ok)",
                             https_local_server().GetURL("b.com", kLnaPath))),
      content::EvalJsResult::IsError());

  // It should not be the loopback-network permission that is denied ...
  ASSERT_EQ("prompt", content::EvalJs(web_contents(), QueryPermissionScript(
                                                          "loopback-network")));
  // but the local-network permission should be denied.
  ASSERT_EQ("denied", content::EvalJs(web_contents(),
                                      QueryPermissionScript("local-network")));

  // Querying old permission should give the non-ask value.
  ASSERT_EQ("denied",
            content::EvalJs(web_contents(),
                            QueryPermissionScript("local-network-access")));
}

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessSplitPermissionOnBrowserTest,
                       FetchAcceptPermissionLoopback) {
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      https_server().GetURL("a.com", kTreatAsPublicAddressPath)));

  // Enable auto-accept of LNA permission request.
  bubble_factory()->set_response_type(
      permissions::PermissionRequestManager::AutoResponseType::ACCEPT_ALL);

  // LNA fetch should succeed.
  ASSERT_EQ(true,
            content::EvalJs(
                web_contents(),
                content::JsReplace("fetch($1).then(response => response.ok)",
                                   https_server().GetURL("b.com", kLnaPath))));

  // It should be the loopback-network permission that is granted ...
  ASSERT_EQ("granted",
            content::EvalJs(web_contents(),
                            QueryPermissionScript("loopback-network")));
  // and not the local-network permission.
  ASSERT_EQ("prompt", content::EvalJs(web_contents(),
                                      QueryPermissionScript("local-network")));

  // Querying old permission should give the non-ask value.
  ASSERT_EQ("granted",
            content::EvalJs(web_contents(),
                            QueryPermissionScript("local-network-access")));
}

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessSplitPermissionOnBrowserTest,
                       FetchAcceptPermissionLocal) {
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      https_server().GetURL("a.com", kTreatAsPublicAddressPath)));

  // Enable auto-accept of LNA permission request.
  bubble_factory()->set_response_type(
      permissions::PermissionRequestManager::AutoResponseType::ACCEPT_ALL);

  // LNA fetch should succeed.
  ASSERT_EQ(true, content::EvalJs(
                      web_contents(),
                      content::JsReplace(
                          "fetch($1).then(response => response.ok)",
                          https_local_server().GetURL("b.com", kLnaPath))));

  // It should not be the loopback-network permission that is granted ...
  ASSERT_EQ("prompt", content::EvalJs(web_contents(), QueryPermissionScript(
                                                          "loopback-network")));
  // but the local-network permission should be.
  ASSERT_EQ("granted", content::EvalJs(web_contents(),
                                       QueryPermissionScript("local-network")));

  // Querying old permission should give the non-ask value.
  ASSERT_EQ("granted",
            content::EvalJs(web_contents(),
                            QueryPermissionScript("local-network-access")));
}

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessSplitPermissionOnBrowserTest,
                       FetchAcceptPermissionLocalDenyPermissionLoopback) {
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      https_server().GetURL("a.com", kTreatAsPublicAddressPath)));

  // Enable auto-accept of LNA permission request.
  bubble_factory()->set_response_type(
      permissions::PermissionRequestManager::AutoResponseType::ACCEPT_ALL);

  // Local LNA fetch should succeed.
  ASSERT_EQ(true, content::EvalJs(
                      web_contents(),
                      content::JsReplace(
                          "fetch($1).then(response => response.ok)",
                          https_local_server().GetURL("b.com", kLnaPath))));

  // Enable auto-deny of LNA permission request.
  bubble_factory()->set_response_type(
      permissions::PermissionRequestManager::AutoResponseType::DENY_ALL);

  // Loopback LNA fetch should fail.
  EXPECT_THAT(content::EvalJs(
                  web_contents(),
                  content::JsReplace("fetch($1).then(response => response.ok)",
                                     https_server().GetURL("b.com", kLnaPath))),
              content::EvalJsResult::IsError());

  // local-network permission should be granted.
  ASSERT_EQ("granted", content::EvalJs(web_contents(),
                                       QueryPermissionScript("local-network")));

  // loopback-network permission should be denied.
  ASSERT_EQ("denied", content::EvalJs(web_contents(), QueryPermissionScript(
                                                          "loopback-network")));

  // Querying old permission should give the denied value.
  ASSERT_EQ("denied",
            content::EvalJs(web_contents(),
                            QueryPermissionScript("local-network-access")));
}

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessSplitPermissionOnBrowserTest,
                       FetchDenyPermissionLocalAcceptPermissionLoopback) {
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      https_server().GetURL("a.com", kTreatAsPublicAddressPath)));

  // Enable auto-deny of LNA permission request.
  bubble_factory()->set_response_type(
      permissions::PermissionRequestManager::AutoResponseType::DENY_ALL);

  // Local LNA fetch should fail.
  EXPECT_THAT(
      content::EvalJs(
          web_contents(),
          content::JsReplace("fetch($1).then(response => response.ok)",
                             https_local_server().GetURL("b.com", kLnaPath))),
      content::EvalJsResult::IsError());

  // Enable auto-accept of LNA permission request.
  bubble_factory()->set_response_type(
      permissions::PermissionRequestManager::AutoResponseType::ACCEPT_ALL);

  // Loopback LNA fetch should succeed.
  ASSERT_EQ(true,
            content::EvalJs(
                web_contents(),
                content::JsReplace("fetch($1).then(response => response.ok)",
                                   https_server().GetURL("b.com", kLnaPath))));

  // local-network permission should be denied.
  ASSERT_EQ("denied", content::EvalJs(web_contents(),
                                      QueryPermissionScript("local-network")));

  // loopback-network permission should be granted.
  ASSERT_EQ("granted",
            content::EvalJs(web_contents(),
                            QueryPermissionScript("loopback-network")));

  // Querying old permission should give the denied value.
  ASSERT_EQ("denied",
            content::EvalJs(web_contents(),
                            QueryPermissionScript("local-network-access")));
}

// Test of the permissionStatus.onchange handler for the LOOBPACK_NETWORK
// permission.
IN_PROC_BROWSER_TEST_F(LocalNetworkAccessSplitPermissionOnBrowserTest,
                       PermissionStatusOnchangeNewPermission) {
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      https_public_server().GetURL(
          "a.com", "/local_network_access/permission-status-onchange.html")));

  // Enable auto-accept of LNA permission request.
  bubble_factory()->set_response_type(
      permissions::PermissionRequestManager::AutoResponseType::ACCEPT_ALL);

  // permission status onchange handler should be triggered.
  ASSERT_EQ("granted",
            content::EvalJs(
                web_contents(),
                content::JsReplace("permission_onstatus($1, $2)",
                                   https_server().GetURL("b.com", kLnaPath),
                                   "loopback-network")));

  // It should be the loopback-network permission that is granted.
  ASSERT_EQ("granted",
            content::EvalJs(web_contents(),
                            QueryPermissionScript("loopback-network")));
}

// Test of the permissionStatus.onchange handler for the LOCAL_NETWORK_ACCESS
// permission alias. Regression test for crbug.com/480069043.
IN_PROC_BROWSER_TEST_F(LocalNetworkAccessSplitPermissionOnBrowserTest,
                       PermissionStatusOnchangeOldPermission) {
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      https_public_server().GetURL(
          "a.com", "/local_network_access/permission-status-onchange.html")));

  // Enable auto-accept of LNA permission request.
  bubble_factory()->set_response_type(
      permissions::PermissionRequestManager::AutoResponseType::ACCEPT_ALL);

  // permission status onchange handler should be triggered.
  ASSERT_EQ("granted",
            content::EvalJs(
                web_contents(),
                content::JsReplace("permission_onstatus($1, $2)",
                                   https_server().GetURL("b.com", kLnaPath),
                                   "local-network-access")));

  // It should be the loopback-network permission that is granted.
  ASSERT_EQ("granted",
            content::EvalJs(web_contents(),
                            QueryPermissionScript("loopback-network")));
}

// Open a public page that iframes a public page, then navigate it to a loopback
// page.
IN_PROC_BROWSER_TEST_F(
    LocalNetworkAccessSplitPermissionOnBrowserTest,
    IframeNavigationPublicPagePublicIframeLoopbackDestination) {
  GURL initial_url = https_server().GetURL(
      "a.com", "/local_network_access/no-favicon-treat-as-public-address.html");
  GURL final_url = https_server().GetURL("c.com", "/defaultresponse");
  GURL iframe_url = https_server().GetURL(
      "b.com",
      "/local_network_access/"
      "client-redirect-treat-as-public-address.html?url=" +
          final_url.spec());

  RunIframeNavigationTest(initial_url, iframe_url, final_url,
                          "loopback-network", /*expect_nav_failure=*/false);
  RunIframeNavigationTest(initial_url, iframe_url, final_url, "local-network",
                          /*expect_nav_failure=*/true);
}

// Open a public page that iframes a public page, then navigate it to a local
// page.
IN_PROC_BROWSER_TEST_F(LocalNetworkAccessSplitPermissionOnBrowserTest,
                       IframeNavigationPublicPagePublicIframeLocalDestination) {
  GURL initial_url = https_server().GetURL(
      "a.com", "/local_network_access/no-favicon-treat-as-public-address.html");
  GURL final_url = https_local_server().GetURL("c.com", "/defaultresponse");
  GURL iframe_url = https_server().GetURL(
      "b.com",
      "/local_network_access/"
      "client-redirect-treat-as-public-address.html?url=" +
          final_url.spec());

  RunIframeNavigationTest(initial_url, iframe_url, final_url,
                          "loopback-network", /*expect_nav_failure=*/true);
  RunIframeNavigationTest(initial_url, iframe_url, final_url, "local-network",
                          /*expect_nav_failure=*/false);
}

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessSplitPermissionOnBrowserTest,
                       LocalNetworkAccessAllowedForUrlsPolicy) {
  policy::PolicyMap policies;
  base::ListValue allowlist;
  allowlist.Append(base::Value("*"));
  SetPolicy(&policies, policy::key::kLocalNetworkAccessAllowedForUrls,
            base::Value(std::move(allowlist)));
  UpdateProviderPolicy(policies);

  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      https_server().GetURL(
          "a.com",
          "/local_network_access/no-favicon-treat-as-public-address.html")));

  // LNA fetch should pass for both loopback requests ...
  ASSERT_EQ(true,
            content::EvalJs(
                web_contents(),
                content::JsReplace("fetch($1).then(response => response.ok)",
                                   https_server().GetURL("b.com", kLnaPath))));

  // ... and for local requests.
  ASSERT_EQ(true, content::EvalJs(
                      web_contents(),
                      content::JsReplace(
                          "fetch($1).then(response => response.ok)",
                          https_local_server().GetURL("b.com", kLnaPath))));
}

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessSplitPermissionOnBrowserTest,
                       LocalNetworkAccessBlockedForUrlsPolicy) {
  // Set both policies. Block should override Allow
  policy::PolicyMap policies;
  base::ListValue allowlist;
  allowlist.Append(base::Value("*"));
  SetPolicy(&policies, policy::key::kLocalNetworkAccessAllowedForUrls,
            base::Value(std::move(allowlist)));
  base::ListValue blocklist;
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

  // LNA fetch should fail, both for loopback requests...
  EXPECT_THAT(content::EvalJs(
                  web_contents(),
                  content::JsReplace("fetch($1).then(response => response.ok)",
                                     https_server().GetURL("b.com", kLnaPath))),
              content::EvalJsResult::IsError());

  // ... and for local requests.
  EXPECT_THAT(
      content::EvalJs(
          web_contents(),
          content::JsReplace("fetch($1).then(response => response.ok)",
                             https_local_server().GetURL("b.com", kLnaPath))),
      content::EvalJsResult::IsError());
}

// Tests for forwards compatibility of the "local-network-access" permissions
// policy feature.
//
// Open a public page that iframes a public page with an
// `allow="local-network-access"` permissions policy. The subframe should have
// all three permissions policy features allowed. Navigating the subframe to a
// local page should trigger the permission prompt and succeed. Navigating the
// subframe to a loopback page should also trigger the permission prompt and
// succeed.
IN_PROC_BROWSER_TEST_F(LocalNetworkAccessSplitPermissionOnBrowserTest,
                       PermissionsPolicyForwardsCompatible) {
  GURL initial_url = https_public_server().GetURL(
      "a.com", "/local_network_access/no-favicon.html");

  GURL local_final_url =
      https_local_server().GetURL("c.com", "/defaultresponse");
  GURL local_iframe_url = https_public_server().GetURL(
      "b.com", "/local_network_access/client-redirect.html?url=" +
                   local_final_url.spec());

  GURL loopback_final_url = https_server().GetURL("e.com", "/defaultresponse");
  GURL loopback_iframe_url = https_server().GetURL(
      "d.com", "/local_network_access/client-redirect.html?url=" +
                   loopback_final_url.spec());

  // Both navigation to a local page and to a loopback page should succeed.
  RunIframeNavigationTest(initial_url, local_iframe_url, local_final_url,
                          "local-network-access", /*expect_nav_failure=*/false);
  RunIframeNavigationTest(initial_url, loopback_iframe_url, loopback_final_url,
                          "local-network-access", /*expect_nav_failure=*/false);
}

// Tests for forwards compatibility of the "local-network-access" permissions
// policy feature, for the Permissions and FeaturePolicy reflection APIs.
//
// Open a public page that iframes a public page with
// `allow="local-network-access"` permissions policy. Permissions.query() in
// the subframe should return "prompt" for all three features, and
// FeaturePolicy.allowsFeature() in the subframe should return true for all
// three features.
IN_PROC_BROWSER_TEST_F(LocalNetworkAccessSplitPermissionOnBrowserTest,
                       PermissionPolicyForwardsCompatibleReflectionApis) {
  GURL mainframe_url = https_public_server().GetURL(
      "a.com", "/local_network_access/no-favicon.html");
  GURL iframe_url = https_public_server().GetURL(
      "b.com", "/local_network_access/no-favicon.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), mainframe_url));
  content::TestNavigationManager nav_manager(web_contents(), iframe_url);
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
  ASSERT_TRUE(nav_manager.WaitForNavigationFinished());
  EXPECT_TRUE(nav_manager.was_successful());

  content::RenderFrameHost* main_frame = web_contents()->GetPrimaryMainFrame();
  content::RenderFrameHost* child_frame = content::ChildFrameAt(main_frame, 0);
  auto* child_contents = content::WebContents::FromRenderFrameHost(child_frame);

  // Check the Permission.query() status inside the iframe.
  EXPECT_EQ("prompt",
            content::EvalJs(child_contents,
                            QueryPermissionScript("local-network-access")));
  EXPECT_EQ("prompt", content::EvalJs(child_contents,
                                      QueryPermissionScript("local-network")));
  EXPECT_EQ("prompt", content::EvalJs(child_contents, QueryPermissionScript(
                                                          "loopback-network")));

  // Check the FeaturePolicy.allowsFeature() status inside the iframe.
  EXPECT_TRUE(
      content::EvalJs(
          child_contents,
          content::JsReplace(
              "document.featurePolicy.allowsFeature('local-network-access')"))
          .ExtractBool());
  EXPECT_TRUE(content::EvalJs(
                  child_contents,
                  content::JsReplace(
                      "document.featurePolicy.allowsFeature('local-network')"))
                  .ExtractBool());
  EXPECT_TRUE(
      content::EvalJs(
          child_contents,
          content::JsReplace(
              "document.featurePolicy.allowsFeature('loopback-network')"))
          .ExtractBool());
}

// Tests forward compatibility for disallowing "local-network-access" in
// permissions policy headers.
IN_PROC_BROWSER_TEST_F(LocalNetworkAccessSplitPermissionOnBrowserTest,
                       PermissionPolicyForwardsCompatibleHeaderDisallow) {
  // Navigate to public page that includes a permissions policy header
  // disallowing LNA:
  //   Permissions-Policy: local-network-access=()
  GURL url = https_public_server().GetURL(
      "a.com", "/local_network_access/permissions-policy-disallow-lna.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // All three LNA features should be disallowed.
  EXPECT_FALSE(
      content::EvalJs(
          web_contents(),
          content::JsReplace(
              "document.featurePolicy.allowsFeature('local-network-access')"))
          .ExtractBool());
  EXPECT_FALSE(content::EvalJs(
                   web_contents(),
                   content::JsReplace(
                       "document.featurePolicy.allowsFeature('local-network')"))
                   .ExtractBool());
  EXPECT_FALSE(
      content::EvalJs(
          web_contents(),
          content::JsReplace(
              "document.featurePolicy.allowsFeature('loopback-network')"))
          .ExtractBool());
}

struct WebRtcTestParam {
  std::string_view test_name;
  std::string_view candidate_ip;
  bool accept_permission;
  std::string_view expected_loopback_state;
  std::string_view expected_local_state;
  std::string_view expected_deprecated_state;
};

constexpr WebRtcTestParam kWebRtcTestParams[] = {
    {
        .test_name = "LoopbackGranted",
        .candidate_ip = "127.0.0.1",
        .accept_permission = true,
        .expected_loopback_state = "granted",
        .expected_local_state = "prompt",
        .expected_deprecated_state = "granted",
    },
    {
        .test_name = "LoopbackDenied",
        .candidate_ip = "127.0.0.1",
        .accept_permission = false,
        .expected_loopback_state = "denied",
        .expected_local_state = "prompt",
        .expected_deprecated_state = "denied",
    },
    {
        .test_name = "LocalGranted",
        .candidate_ip = "192.168.1.1",
        .accept_permission = true,
        .expected_loopback_state = "prompt",
        .expected_local_state = "granted",
        .expected_deprecated_state = "granted",
    },
    {
        .test_name = "LocalDenied",
        .candidate_ip = "192.168.1.1",
        .accept_permission = false,
        .expected_loopback_state = "prompt",
        .expected_local_state = "denied",
        .expected_deprecated_state = "denied",
    },
};

class LocalNetworkAccessSplitPermissionWebRtcBrowserTest
    : public LocalNetworkAccessSplitPermissionOnBrowserTest,
      public testing::WithParamInterface<WebRtcTestParam> {
 public:
  static std::string DescribeParams(
      const testing::TestParamInfo<ParamType>& info) {
    return std::string(info.param.test_name);
  }

 private:
  base::test::ScopedFeatureList feature_list_{
      network::features::kLocalNetworkAccessChecksWebRTC};
};

IN_PROC_BROWSER_TEST_P(LocalNetworkAccessSplitPermissionWebRtcBrowserTest,
                       SetRemoteDescription) {
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      https_public_server().GetURL("a.com", kTreatAsPublicAddressPath)));

  // Configure the permission request response.
  bubble_factory()->set_response_type(
      GetParam().accept_permission
          ? permissions::PermissionRequestManager::AutoResponseType::ACCEPT_ALL
          : permissions::PermissionRequestManager::AutoResponseType::DENY_ALL);

  const std::string remote_description = base::StringPrintf(
      R"(v=0
o=- 3988109818200882900 2 IN IP4 0.0.0.0
s=-
t=0 0
a=msid-semantic: WMS
m=application 9 UDP/DTLS/SCTP webrtc-datachannel
c=IN IP4 0.0.0.0
a=mid:0
a=ice-ufrag:someRemoteUfrag
a=ice-pwd:someRemotePasswordGeneratedString
a=sctpmap:5000 webrtc-datachannel 1024
a=fingerprint:sha-256 11:22:33:44:55:66:77:88:99:AA:BB:CC:DD:EE:FF:00:11:22:33:44:55:66:77:88:99:AA:BB:CC:DD:EE:FF:00
a=candidate:1 1 UDP 2130706431 %s 12345 typ host generation 0
)",
      GetParam().candidate_ip);

  std::string script = R"(
    (async () => {
      const pc = new RTCPeerConnection();
      const dataChannel = pc.createDataChannel("myWebRTCChannel");
      const offer = await pc.createOffer();
      await pc.setLocalDescription(offer);
      await pc.setRemoteDescription({
        type: 'answer',
        sdp: $1
      });
    })()
  )";

  EXPECT_TRUE(content::ExecJs(web_contents(),
                              content::JsReplace(script, remote_description)));

  // Verify permission states. WebRTC asks for permission asynchronously, so we
  // wait for the permission state to switch from "prompt" before we check it.
  if (GetParam().expected_loopback_state != "prompt") {
    EXPECT_EQ(GetParam().expected_loopback_state,
              content::EvalJs(web_contents(),
                              WaitForPermissionScript("loopback-network")));
    EXPECT_EQ("prompt", content::EvalJs(web_contents(), QueryPermissionScript(
                                                            "local-network")));
  } else {
    EXPECT_EQ(GetParam().expected_local_state,
              content::EvalJs(web_contents(),
                              WaitForPermissionScript("local-network")));
    EXPECT_EQ("prompt",
              content::EvalJs(web_contents(),
                              QueryPermissionScript("loopback-network")));
  }

  EXPECT_EQ(GetParam().expected_deprecated_state,
            content::EvalJs(web_contents(),
                            QueryPermissionScript("local-network-access")));
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    LocalNetworkAccessSplitPermissionWebRtcBrowserTest,
    testing::ValuesIn(kWebRtcTestParams),
    LocalNetworkAccessSplitPermissionWebRtcBrowserTest::DescribeParams);

}  // namespace local_network_access
