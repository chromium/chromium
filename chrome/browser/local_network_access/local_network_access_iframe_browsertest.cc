// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/local_network_access/local_network_access_browsertest_base.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/permissions/permission_request_manager.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"

// Local Network Access browser tests related to iframes.
//
namespace local_network_access {

// Path to a response that passes Local Network Access checks.
constexpr char kLnaPath[] =
    "/set-header"
    "?Access-Control-Allow-Origin: *";

class LocalNetworkAccessIframeBrowserTest
    : public LocalNetworkAccessBrowserTestBase {
 public:
  // Helper function for running iframe navigation tests that are intended to
  // succeed.
  //
  // TODO(crbug.com/406991278): work on covering more iframe tests cases,
  // including those that we expect to fail.
  void RunIframeNavigationTest(const GURL& initial_url,
                               const GURL& iframe_url,
                               const GURL& nav_url) {
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
      child.allow = "local-network-access";
      document.body.appendChild(child);
    )";
    EXPECT_THAT(
        content::EvalJs(web_contents(),
                        content::JsReplace(script_template, iframe_url)),
        content::EvalJsResult::IsOk());
    // Check that the child iframe was successfully fetched.
    ASSERT_TRUE(iframe_url_nav_manager.WaitForNavigationFinished());
    EXPECT_TRUE(iframe_url_nav_manager.was_successful());

    ASSERT_TRUE(nav_url_nav_manager.WaitForNavigationFinished());
    EXPECT_TRUE(nav_url_nav_manager.was_successful());
  }
};

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessIframeBrowserTest,
                       IframeDenyPermission) {
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      https_server().GetURL(
          "a.com",
          "/local_network_access/no-favicon-treat-as-public-address.html")));

  // Enable auto-denial of LNA permission request.
  bubble_factory()->set_response_type(
      permissions::PermissionRequestManager::AutoResponseType::DENY_ALL);

  GURL iframe_url = https_server().GetURL("b.com", kLnaPath);
  content::TestNavigationManager nav_manager(web_contents(), iframe_url);
  std::string_view script_template = R"(
    const child = document.createElement("iframe");
    child.src = $1;
    document.body.appendChild(child);
  )";
  EXPECT_THAT(content::EvalJs(web_contents(),
                              content::JsReplace(script_template, iframe_url)),
              content::EvalJsResult::IsOk());
  ASSERT_TRUE(nav_manager.WaitForNavigationFinished());

  // Check that the child iframe failed to fetch.
  EXPECT_FALSE(nav_manager.was_successful());
}

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessIframeBrowserTest,
                       IframeAcceptPermission) {
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      https_server().GetURL(
          "a.com",
          "/local_network_access/no-favicon-treat-as-public-address.html")));

  // Enable auto-accept of LNA permission request.
  bubble_factory()->set_response_type(
      permissions::PermissionRequestManager::AutoResponseType::ACCEPT_ALL);

  GURL iframe_url = https_server().GetURL("b.com", kLnaPath);
  content::TestNavigationManager nav_manager(web_contents(), iframe_url);
  std::string_view script_template = R"(
    const child = document.createElement("iframe");
    child.src = $1;
    document.body.appendChild(child);
  )";
  EXPECT_THAT(content::EvalJs(web_contents(),
                              content::JsReplace(script_template, iframe_url)),
              content::EvalJsResult::IsOk());
  ASSERT_TRUE(nav_manager.WaitForNavigationFinished());

  // Check that the child iframe was successfully fetched.
  EXPECT_TRUE(nav_manager.was_successful());
}

// Open a public page that iframes a public page, then navigate it to a loopback
// page.
IN_PROC_BROWSER_TEST_F(LocalNetworkAccessIframeBrowserTest,
                       IframeNavigationPublicPagePublicIframe) {
  GURL initial_url = https_server().GetURL(
      "a.com", "/local_network_access/no-favicon-treat-as-public-address.html");
  GURL final_url = https_server().GetURL("c.com", "/defaultresponse");
  GURL iframe_url = https_server().GetURL(
      "b.com",
      "/local_network_access/"
      "client-redirect-treat-as-public-address.html?url=" +
          final_url.spec());

  RunIframeNavigationTest(initial_url, iframe_url, final_url);
}

// Open a public page that iframes a loopback page, then navigate it to a
// loopback page.
IN_PROC_BROWSER_TEST_F(LocalNetworkAccessIframeBrowserTest,
                       IframeNavigationPublicPageLoopbackIframe) {
  GURL initial_url = https_server().GetURL(
      "a.com", "/local_network_access/no-favicon-treat-as-public-address.html");
  GURL final_url = https_server().GetURL("c.com", "/defaultresponse");
  GURL iframe_url = https_server().GetURL(
      "b.com",
      "/local_network_access/client-redirect.html?url=" + final_url.spec());
  RunIframeNavigationTest(initial_url, iframe_url, final_url);
}

// Open a loopback page that iframes a public page, then navigate it to a
// loopback page.
IN_PROC_BROWSER_TEST_F(LocalNetworkAccessIframeBrowserTest,
                       IframeNavLoopbackPagePublicIframe) {
  GURL initial_url =
      https_server().GetURL("a.com", "/local_network_access/no-favicon.html");
  GURL final_url = https_server().GetURL("c.com", "/defaultresponse");
  GURL iframe_url = https_server().GetURL(
      "b.com",
      "/local_network_access/"
      "client-redirect-treat-as-public-address.html?url=" +
          final_url.spec());
  RunIframeNavigationTest(initial_url, iframe_url, final_url);
}

// Open a loopback page that iframes a loopback page, then navigate it to a
// loopback page.
IN_PROC_BROWSER_TEST_F(LocalNetworkAccessIframeBrowserTest,
                       IframeNavLoopbackPageLoopbackIframe) {
  GURL initial_url =
      https_server().GetURL("a.com", "/local_network_access/no-favicon.html");
  GURL final_url = https_server().GetURL("c.com", "/defaultresponse");
  GURL iframe_url = https_server().GetURL(
      "b.com",
      "/local_network_access/client-redirect.html?url=" + final_url.spec());
  RunIframeNavigationTest(initial_url, iframe_url, final_url);
}

// Open a public page that iframes a public page, then navigate it to a loopback
// page. The page and the iframe have the same origin
IN_PROC_BROWSER_TEST_F(LocalNetworkAccessIframeBrowserTest,
                       IframeNavPublicPagePublicLoopbackSameOrigin) {
  GURL initial_url = https_server().GetURL(
      "a.com", "/local_network_access/no-favicon-treat-as-public-address.html");
  GURL final_url = https_server().GetURL("c.com", "/defaultresponse");
  GURL iframe_url = https_server().GetURL(
      "a.com",
      "/local_network_access/"
      "client-redirect-treat-as-public-address.html?url=" +
          final_url.spec());

  RunIframeNavigationTest(initial_url, iframe_url, final_url);
}

// Open a loopback page that iframes a loopback page, then navigate it to a
// loopback page. The page and the iframe have the same origin.
IN_PROC_BROWSER_TEST_F(LocalNetworkAccessIframeBrowserTest,
                       IframeNavLoopbackPageLoopbackIframeSameOrigin) {
  GURL initial_url =
      https_server().GetURL("a.com", "/local_network_access/no-favicon.html");
  GURL final_url = https_server().GetURL("c.com", "/defaultresponse");
  GURL iframe_url = https_server().GetURL(
      "a.com",
      "/local_network_access/client-redirect.html?url=" + final_url.spec());
  RunIframeNavigationTest(initial_url, iframe_url, final_url);
}

}  // namespace local_network_access
