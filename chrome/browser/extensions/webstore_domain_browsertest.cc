// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_event_histogram_value.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/api/management.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/switches.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/cpp/network_switches.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

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
    cert_config.dns_names = {GetParam().GetHost(), "foo.com", "bar.com"};
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
      command_line->AppendSwitchASCII(switches::kAppsGalleryURL,
                                      kWebstoreOverrideURL);
    }

    ExtensionApiTest::SetUpCommandLine(command_line);
  }
};

// Tests that webstorePrivate, management and runtime are exposed to the
// webstore domain, but not to a non-webstore domain.
// Note: Although we don't explicitly provide runtime to the webstore domain in
// the case of it being a "web page" context, it is granted implicitly by the
// NativeExtensionBindingsSystem due to other extension APIs (webstorePrivate
// and management) being exposed to the web page.
IN_PROC_BROWSER_TEST_P(WebstoreDomainBrowserTest, ExpectedAvailability) {
  const GURL webstore_url = GetParam().Resolve("/webstore/mock_store.html");
  const GURL not_webstore_url = GURL(kNonWebstoreURL1).Resolve("/empty.html");

  content::WebContents* web_contents = GetActiveWebContents();

  auto is_api_available = [web_contents](const std::string& api_name) {
    constexpr char kScript[] = "chrome.hasOwnProperty($1);";
    return content::EvalJs(web_contents, content::JsReplace(kScript, api_name))
        .ExtractBool();
  };

  ASSERT_TRUE(NavigateToURL(web_contents, webstore_url));
  EXPECT_EQ(web_contents->GetPrimaryMainFrame()->GetLastCommittedURL(),
            webstore_url);

  // The webstorePrivate and management APIs are only available on the new
  // webstore domain. The old site gained access to them via the hosted app,
  // which is no longer allowed to access webstorePrivate or management (since
  // the hosted app isn't used).
  // The runtime API is still available since it's always available to all
  // items, but it doesn't really have any capabilities (and the hosted app is
  // still safe; just unused).
  bool expect_fun_apis = GetParam() == GURL(kNewWebstoreURL) ||
                         GetParam() == GURL(kWebstoreOverrideURL);

  EXPECT_EQ(expect_fun_apis, is_api_available("webstorePrivate"));
  EXPECT_EQ(expect_fun_apis, is_api_available("management"));

  // Even runtime shouldn't be available for the old hosted app URL if the
  // hosted app isn't installed.
  bool expect_runtime =
      GetParam() == GURL(kNewWebstoreURL) ||
      GetParam() == GURL(kWebstoreOverrideURL) ||
      (GetParam() == GURL(kWebstoreAppBaseURL) &&
       base::FeatureList::IsEnabled(extensions_features::kWebstoreHostedApp));
  EXPECT_EQ(expect_runtime, is_api_available("runtime"));

  ASSERT_TRUE(NavigateToURL(web_contents, not_webstore_url));
  EXPECT_EQ(web_contents->GetPrimaryMainFrame()->GetLastCommittedURL(),
            not_webstore_url);
  EXPECT_FALSE(is_api_available("management"));
  EXPECT_FALSE(is_api_available("webstorePrivate"));
  EXPECT_FALSE(is_api_available("runtime"));
}

// Test that the webstore can register and receive management events. Normally
// we have a check that the receiver of an extension event can never be a
// webpage context. The old webstore got around this by appearing as a hosted
// app extension context, but the new webstore has the APIs exposed directly to
// the webpage context it uses. Regression test for crbug.com/40064270.
IN_PROC_BROWSER_TEST_P(WebstoreDomainBrowserTest, CanReceiveEvents) {
  const GURL webstore_url = GetParam().Resolve("/webstore/mock_store.html");

  content::WebContents* web_contents = GetActiveWebContents();

  ASSERT_TRUE(NavigateToURL(web_contents, webstore_url));
  EXPECT_EQ(web_contents->GetPrimaryMainFrame()->GetLastCommittedURL(),
            webstore_url);
  constexpr char kAddListener[] = R"(
    try {
      chrome.management.onInstalled.addListener(() => {
        domAutomationController.send('received event');
      });
      'listener added';
    } catch(e) {
      'Error registering';
    }
  )";

  // The webstore hosted app no longer has access to the management API.
  bool expect_management = GetParam() == GURL(kNewWebstoreURL) ||
                           GetParam() == GURL(kWebstoreOverrideURL);

  std::string js_result =
      content::EvalJs(web_contents, kAddListener).ExtractString();

  if (expect_management) {
    ASSERT_EQ("listener added", js_result);

    content::DOMMessageQueue message_queue(web_contents);
    // Directly broadcast the management.onInstalled event from the EventRouter
    // and verify it arrived to the page without causing a crash.
    EventRouter* event_router = EventRouter::Get(profile());
    api::management::ExtensionInfo info;
    info.install_type = api::management::ExtensionInstallType::kNormal;
    info.type = api::management::ExtensionType::kExtension;
    event_router->BroadcastEvent(std::make_unique<Event>(
        events::FOR_TEST, api::management::OnInstalled::kEventName,
        api::management::OnInstalled::Create(info)));

    std::string message;
    EXPECT_TRUE(message_queue.WaitForMessage(&message));
    EXPECT_EQ("\"received event\"", message);
  } else {
    ASSERT_EQ("Error registering", js_result);
  }
}

// Tests that a webstore page with misconfigured or missing X-Frame-Options
// headers that is embedded in an iframe has the headers adjusted to SAMEORIGIN
// and that the subframe navigation is subsequently blocked.
IN_PROC_BROWSER_TEST_P(WebstoreDomainBrowserTest, FrameWebstorePageBlocked) {
  GURL outer_frame_url = GURL(kNonWebstoreURL1).Resolve("/empty.html");

  content::WebContents* web_contents = GetActiveWebContents();
  ASSERT_TRUE(NavigateToURL(web_contents, outer_frame_url));
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
    const network::mojom::URLResponseHead* response_head =
        subframe->GetLastResponseHead();
    ASSERT_TRUE(response_head);
    ASSERT_TRUE(response_head->headers);
    EXPECT_TRUE(
        response_head->headers->HasHeaderValue("X-Frame-Options", "foo"));

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
    const network::mojom::URLResponseHead* response_head =
        subframe->GetLastResponseHead();
    ASSERT_TRUE(response_head);
    ASSERT_TRUE(response_head->headers);
    EXPECT_TRUE(response_head->headers->HasHeaderValue("X-Frame-Options",
                                                       "SAMEORIGIN"));

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
    const network::mojom::URLResponseHead* response_head =
        subframe->GetLastResponseHead();
    ASSERT_TRUE(response_head);
    ASSERT_TRUE(response_head->headers);
    EXPECT_TRUE(response_head->headers->HasHeaderValue("X-Frame-Options",
                                                       "SAMEORIGIN"));

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
