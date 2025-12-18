// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/local_network_access/local_network_access_browsertest_base.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
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

}  // namespace
class LocalNetworkAccessSplitPermissionOffBrowserTest
    : public LocalNetworkAccessBrowserTestBase {};

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

}  // namespace local_network_access
