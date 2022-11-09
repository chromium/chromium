// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/cpp/network_switches.h"

namespace extensions {
namespace {

// URL the new webstore is associated with in production.
constexpr char kNewWebstoreURL[] = "https://chromewebstore.google.com/";
// URL the webstore hosted app is associated with in production, minus the
// /webstore/ path which is added in the tests themselves.
constexpr char kWebstoreAppBaseURL[] = "https://chrome.google.com/";
// URL to test the command line override for the webstore.
constexpr char kWebstoreOverrideURL[] = "https://chrome.webstore.test.com/";
constexpr char kNonWebstoreURL1[] = "https://foo.com/";
constexpr char kNonWebstoreURL2[] = "https://bar.com/";

}  // namespace

class WebstoreDomainBrowserTest : public ExtensionApiTest,
                                  public testing::WithParamInterface<GURL> {
 public:
  WebstoreDomainBrowserTest() {
    UseHttpsTestServer();
    // Override the test server SSL config with the webstore domain under test
    // and two other non-webstore domains used in the tests.
    net::EmbeddedTestServer::ServerCertificateConfig cert_config;
    cert_config.dns_names = {GetParam().host(), "foo.com", "bar.com"};
    embedded_test_server()->SetSSLConfig(cert_config);
    // Add the extensions directory to the test server as it has a /webstore/
    // directory to serve files from, which the webstore hosted app requires as
    // part of the URL it is associated with.
    embedded_test_server()->ServeFilesFromSourceDirectory(
        "chrome/test/data/extensions");
    EXPECT_TRUE(embedded_test_server()->Start());
  }
  ~WebstoreDomainBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Add a host resolver rule to map all outgoing requests to the test server.
    // This allows us to use "real" hostnames and standard ports in URLs (i.e.,
    // without having to inject the port number into all URLs).
    command_line->AppendSwitchASCII(
        network::switches::kHostResolverRules,
        "MAP * " + embedded_test_server()->host_port_pair().ToString());

    // Only override the webstore URL if this test case is testing the override.
    if (GetParam().spec() == kWebstoreOverrideURL) {
      command_line->AppendSwitchASCII(::switches::kAppsGalleryURL,
                                      kWebstoreOverrideURL);
    }

    ExtensionApiTest::SetUpCommandLine(command_line);
  }
};

// Tests that webstorePrivate and management are exposed to the webstore domain,
// but not to a non-webstore domain.
IN_PROC_BROWSER_TEST_P(WebstoreDomainBrowserTest, ExpectedAvailability) {
  const GURL webstore_url = GetParam().Resolve("/webstore/mock_store.html");
  const GURL not_webstore_url = GURL(kNonWebstoreURL1).Resolve("/empty.html");

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  auto is_api_available = [web_contents](const std::string& api_name) {
    constexpr char kScript[] =
        R"({
             domAutomationController.send(chrome.hasOwnProperty($1));
           })";
    bool result = false;
    EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
        web_contents, content::JsReplace(kScript, api_name), &result));
    return result;
  };

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), webstore_url));
  EXPECT_EQ(web_contents->GetPrimaryMainFrame()->GetLastCommittedURL(),
            webstore_url);
  EXPECT_TRUE(is_api_available("webstorePrivate"));
  EXPECT_TRUE(is_api_available("management"));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), not_webstore_url));
  EXPECT_EQ(web_contents->GetPrimaryMainFrame()->GetLastCommittedURL(),
            not_webstore_url);
  EXPECT_FALSE(is_api_available("management"));
  EXPECT_FALSE(is_api_available("webstorePrivate"));
}

// Tests that a webstore page with misconfigured or missing X-Frame-Options
// headers that is embedded in an iframe has the headers adjusted to SAMEORIGIN
// and that the subframe navigation is subsequently blocked.
IN_PROC_BROWSER_TEST_P(WebstoreDomainBrowserTest, FrameWebstorePageBlocked) {
  GURL outer_frame_url = GURL(kNonWebstoreURL1).Resolve("/empty.html");

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), outer_frame_url));
  EXPECT_EQ(outer_frame_url, web_contents->GetLastCommittedURL());

  constexpr char kScript[] =
      R"({
           var f = document.createElement('iframe');
           f.src = $1;
           !!document.body.appendChild(f);
         })";

  // Embedding a non-webstore page with a misconfigured X-Frame-Options header
  // will just have the header ignored and load fine.
  {
    GURL non_webstore_url =
        GURL(kNonWebstoreURL2)
            .Resolve("/webstore/xfo_header_misconfigured.html");
    content::TestNavigationObserver observer(web_contents);
    EXPECT_TRUE(content::EvalJs(web_contents,
                                content::JsReplace(kScript, non_webstore_url))
                    .ExtractBool());
    EXPECT_TRUE(WaitForLoadStop(web_contents));
    content::RenderFrameHost* subframe =
        content::ChildFrameAt(web_contents->GetPrimaryMainFrame(), 0);
    ASSERT_TRUE(subframe);
    const net::HttpResponseHeaders* headers =
        subframe->GetLastResponseHeaders();
    ASSERT_TRUE(headers);
    EXPECT_TRUE(headers->HasHeaderValue("X-Frame-Options", "foo"));

    // The subframe should have loaded fine.
    EXPECT_EQ(non_webstore_url, subframe->GetLastCommittedURL());
    EXPECT_TRUE(observer.last_navigation_succeeded());
  }

  // Embedding a webstore page with a misconfigured X-Frame-Options header
  // should have the header replaced and the frame load should fail.
  {
    GURL webstore_url =
        GetParam().Resolve("/webstore/xfo_header_misconfigured.html");
    content::TestNavigationObserver observer(web_contents);
    EXPECT_TRUE(
        content::EvalJs(web_contents, content::JsReplace(kScript, webstore_url))
            .ExtractBool());
    EXPECT_TRUE(WaitForLoadStop(web_contents));
    content::RenderFrameHost* subframe =
        content::ChildFrameAt(web_contents->GetPrimaryMainFrame(), 1);
    ASSERT_TRUE(subframe);
    const net::HttpResponseHeaders* headers =
        subframe->GetLastResponseHeaders();
    ASSERT_TRUE(headers);
    EXPECT_TRUE(headers->HasHeaderValue("X-Frame-Options", "SAMEORIGIN"));

    // The subframe load should fail due to XFO.
    EXPECT_EQ(webstore_url, subframe->GetLastCommittedURL());
    EXPECT_FALSE(observer.last_navigation_succeeded());
    EXPECT_EQ(net::ERR_BLOCKED_BY_RESPONSE, observer.last_net_error_code());
  }

  // Loading a webstore page that doesn't exist and results in a 404 should
  // have the X-Frame-Options SAMEORIGIN added and the load should fail.
  {
    GURL webstore_url = GetParam().Resolve("/webstore/not_an_actual_file.html");
    content::TestNavigationObserver observer(web_contents);
    EXPECT_TRUE(
        content::EvalJs(web_contents, content::JsReplace(kScript, webstore_url))
            .ExtractBool());
    EXPECT_TRUE(WaitForLoadStop(web_contents));
    content::RenderFrameHost* subframe =
        content::ChildFrameAt(web_contents->GetPrimaryMainFrame(), 2);
    ASSERT_TRUE(subframe);
    const net::HttpResponseHeaders* headers =
        subframe->GetLastResponseHeaders();
    ASSERT_TRUE(headers);
    EXPECT_TRUE(headers->HasHeaderValue("X-Frame-Options", "SAMEORIGIN"));

    // The subframe load should fail due to XFO.
    EXPECT_EQ(webstore_url, subframe->GetLastCommittedURL());
    EXPECT_FALSE(observer.last_navigation_succeeded());
    EXPECT_EQ(net::ERR_BLOCKED_BY_RESPONSE, observer.last_net_error_code());
  }
}

INSTANTIATE_TEST_SUITE_P(WebstoreNewURL,
                         WebstoreDomainBrowserTest,
                         testing::Values(GURL(kNewWebstoreURL)));
INSTANTIATE_TEST_SUITE_P(WebstoreHostedAppURL,
                         WebstoreDomainBrowserTest,
                         testing::Values(GURL(kWebstoreAppBaseURL)));
INSTANTIATE_TEST_SUITE_P(WebstoreOverrideURL,
                         WebstoreDomainBrowserTest,
                         testing::Values(GURL(kWebstoreOverrideURL)));

}  // namespace extensions
