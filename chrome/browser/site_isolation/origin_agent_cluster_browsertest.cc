// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/page_load_metrics/browser/page_load_metrics_test_waiter.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "url/gurl.h"

// General browsertests for the Origin-Agent-Cluster header can be found in
// content/browser/isolated_origin_browsertest.cc. However testing use counters
// is best done from chrome/; thus, this file exists.

class OriginAgentClusterBrowserTest : public InProcessBrowserTest {
 public:
  OriginAgentClusterBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}
  ~OriginAgentClusterBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());

    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);

    feature_list_.InitAndEnableFeature(features::kOriginIsolationHeader);
  }

  void SetUpOnMainThread() override {
    https_server()->AddDefaultHandlers(GetChromeTestDataDir());
    https_server()->RegisterRequestHandler(
        base::BindRepeating(&OriginAgentClusterBrowserTest::HandleResponse,
                            base::Unretained(this)));

    ASSERT_TRUE(https_server()->Start());

    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->StartAcceptingConnections();
  }

  void TearDownOnMainThread() override {
    ASSERT_TRUE(https_server()->ShutdownAndWaitUntilComplete());
  }

  net::EmbeddedTestServer* https_server() { return &https_server_; }

 private:
  std::unique_ptr<net::test_server::HttpResponse> HandleResponse(
      const net::test_server::HttpRequest& request) {
    if (request.relative_url == "/origin_key_me") {
      auto response = std::make_unique<net::test_server::BasicHttpResponse>();
      response->set_code(net::HTTP_OK);
      response->set_content_type("text/html");
      response->AddCustomHeader("Origin-Agent-Cluster", "?1");
      response->set_content("I like origin keys!");
      return std::move(response);
    }

    return nullptr;
  }

  net::EmbeddedTestServer https_server_;
  base::test::ScopedFeatureList feature_list_;

  DISALLOW_COPY_AND_ASSIGN(OriginAgentClusterBrowserTest);
};

IN_PROC_BROWSER_TEST_F(OriginAgentClusterBrowserTest, Navigations) {
  GURL start_url(https_server()->GetURL("foo.com", "/iframe.html"));
  GURL origin_keyed_url(
      https_server()->GetURL("origin-keyed.foo.com", "/origin_key_me"));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  auto web_feature_waiter =
      std::make_unique<page_load_metrics::PageLoadMetricsTestWaiter>(
          web_contents);
  web_feature_waiter->AddWebFeatureExpectation(
      blink::mojom::WebFeature::kOriginAgentClusterHeader);

  ui_test_utils::NavigateToURL(browser(), start_url);

  EXPECT_FALSE(web_feature_waiter->DidObserveWebFeature(
      blink::mojom::WebFeature::kOriginAgentClusterHeader));

  EXPECT_TRUE(NavigateIframeToURL(web_contents, "test", origin_keyed_url));

  web_feature_waiter->Wait();
}
