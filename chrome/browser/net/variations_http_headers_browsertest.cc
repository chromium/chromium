// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/net/variations_http_headers.h"

#include <map>

#include "base/bind.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_browser_main.h"
#include "chrome/browser/chrome_browser_main_extra_parts.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/variations/net/variations_http_headers.h"
#include "components/variations/variations_http_header_provider.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/network_connection_change_simulator.h"
#include "content/public/test/simple_url_loader_test_helper.h"
#include "net/base/escape.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/service_worker/service_worker_utils.h"
#include "url/gurl.h"

namespace {

class VariationHeaderSetter : public ChromeBrowserMainExtraParts {
 public:
  VariationHeaderSetter() = default;
  ~VariationHeaderSetter() override = default;

  // ChromeBrowserMainExtraParts:
  void PostEarlyInitialization() override {
    // Set up some fake variations.
    auto* variations_provider =
        variations::VariationsHttpHeaderProvider::GetInstance();
    variations_provider->ForceVariationIds({"12", "456", "t789"}, "");
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(VariationHeaderSetter);
};

class VariationsHttpHeadersBrowserTest : public InProcessBrowserTest {
 public:
  VariationsHttpHeadersBrowserTest()
      : https_server_(net::test_server::EmbeddedTestServer::TYPE_HTTPS) {}
  ~VariationsHttpHeadersBrowserTest() override = default;

  void CreatedBrowserMainParts(content::BrowserMainParts* parts) override {
    static_cast<ChromeBrowserMainParts*>(parts)->AddParts(
        new VariationHeaderSetter());
  }

  void SetUp() override {
    ASSERT_TRUE(server()->InitializeAndListen());
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    content::NetworkConnectionChangeSimulator().SetConnectionType(
        network::mojom::ConnectionType::CONNECTION_ETHERNET);

    host_resolver()->AddRule("*", "127.0.0.1");

    base::FilePath test_data_dir;
    ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir));
    server()->ServeFilesFromDirectory(test_data_dir);

    server()->RegisterRequestHandler(
        base::BindRepeating(&VariationsHttpHeadersBrowserTest::RequestHandler,
                            base::Unretained(this)));

    server()->StartAcceptingConnections();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);

    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  }

  const net::EmbeddedTestServer* server() const { return &https_server_; }
  net::EmbeddedTestServer* server() { return &https_server_; }

  GURL GetGoogleUrlWithPath(const std::string& path) const {
    return server()->GetURL("www.google.com", path);
  }

  GURL GetGoogleUrl() const { return GetGoogleUrlWithPath("/landing.html"); }

  GURL GetGoogleRedirectUrl1() const {
    return GetGoogleUrlWithPath("/redirect");
  }

  GURL GetGoogleRedirectUrl2() const {
    return GetGoogleUrlWithPath("/redirect2");
  }

  GURL GetExampleUrlWithPath(const std::string& path) const {
    return server()->GetURL("www.example.com", path);
  }

  GURL GetExampleUrl() const { return GetExampleUrlWithPath("/landing.html"); }

  // Returns whether a given |header| has been received for a |url|. If
  // |url| has not been observed, fails an EXPECT and returns false.
  bool HasReceivedHeader(const GURL& url, const std::string& header) const {
    auto it = received_headers_.find(url);
    EXPECT_TRUE(it != received_headers_.end());
    if (it == received_headers_.end())
      return false;
    return it->second.find(header) != it->second.end();
  }

  void ClearReceivedHeaders() { received_headers_.clear(); }

  bool FetchResource(const GURL& url) {
    if (!url.is_valid())
      return false;
    std::string script(
        "var xhr = new XMLHttpRequest();"
        "xhr.open('GET', '");
    script += url.spec() +
              "', true);"
              "xhr.onload = function (e) {"
              "  if (xhr.readyState === 4) {"
              "    window.domAutomationController.send(xhr.status === 200);"
              "  }"
              "};"
              "xhr.onerror = function () {"
              "  window.domAutomationController.send(false);"
              "};"
              "xhr.send(null)";
    return ExecuteScript(script);
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  // Registers a service worker for google.com root scope.
  void RegisterServiceWorker(const std::string& worker_path) {
    GURL url =
        GetGoogleUrlWithPath("/service_worker/create_service_worker.html");
    EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    EXPECT_EQ("DONE", EvalJs(GetWebContents(),
                             base::StringPrintf("register('%s', '/');",
                                                worker_path.c_str())));
  }

  // Registers the given service worker for google.com then tests navigation and
  // subresource requests through the worker have X-Client-Data when
  // appropriate.
  void ServiceWorkerTest(const std::string& worker_path) {
    RegisterServiceWorker(worker_path);

    // Navigate to a Google URL.
    GURL page_url =
        GetGoogleUrlWithPath("/service_worker/fetch_from_page.html");
    ui_test_utils::NavigateToURL(browser(), page_url);
    EXPECT_TRUE(HasReceivedHeader(page_url, "X-Client-Data"));
    // Check that there is a controller to check that the test is really testing
    // service worker.
    EXPECT_EQ(true,
              EvalJs(GetWebContents(), "!!navigator.serviceWorker.controller"));

    // Verify subresource requests from the page also have X-Client-Data.
    EXPECT_EQ("hello", EvalJs(GetWebContents(),
                              base::StrCat({"fetch_from_page('",
                                            GetGoogleUrl().spec(), "');"})));
    EXPECT_TRUE(HasReceivedHeader(GetGoogleUrl(), "X-Client-Data"));

    // But not if they are to non-Google domains.
    EXPECT_EQ("hello", EvalJs(GetWebContents(),
                              base::StrCat({"fetch_from_page('",
                                            GetExampleUrl().spec(), "');"})));
    EXPECT_FALSE(HasReceivedHeader(GetExampleUrl(), "X-Client-Data"));
  }

  // Creates a worker and tests that the main script and import scripts have
  // X-Client-Data when appropriate. |page| is the page that creates the
  // specified |worker|, which should be an "import_*_worker.js" script that is
  // expected to import "empty.js" (as a relative path) and also accept an
  // "import=" parameter specifying another script to import. This allows
  // testing that the empty.js import request for google.com has the header, and
  // an import request to example.com does not have the header.
  void WorkerScriptTest(const std::string& page, const std::string& worker) {
    // Build a worker URL for a google.com worker that imports
    // an example.com script.
    GURL absolute_import = GetExampleUrlWithPath("/workers/empty.js");
    const std::string worker_path = base::StrCat(
        {worker, "?import=",
         net::EscapeQueryParamValue(absolute_import.spec(), false)});
    GURL worker_url = GetGoogleUrlWithPath(worker_path);

    // Build the page URL that tells the page to create the worker.
    const std::string page_path = base::StrCat(
        {page,
         "?worker_url=", net::EscapeQueryParamValue(worker_url.spec(), false)});
    GURL page_url = GetGoogleUrlWithPath(page_path);

    // Navigate and test.
    EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), page_url));
    EXPECT_EQ("DONE", EvalJs(GetWebContents(), "waitForMessage();"));

    // The header should be on the main script request.
    EXPECT_TRUE(HasReceivedHeader(worker_url, "X-Client-Data"));

    // And on import script requests to Google.
    EXPECT_TRUE(HasReceivedHeader(GetGoogleUrlWithPath("/workers/empty.js"),
                                  "X-Client-Data"));

    // But not on requests not to Google.
    EXPECT_FALSE(HasReceivedHeader(absolute_import, "X-Client-Data"));
  }

 private:
  bool ExecuteScript(const std::string& script) {
    bool xhr_result = false;
    // The JS call will fail if disallowed because the process will be killed.
    bool execute_result =
        ExecuteScriptAndExtractBool(GetWebContents(), script, &xhr_result);
    return xhr_result && execute_result;
  }

  // Custom request handler that record request headers and simulates a redirect
  // from google.com to example.com.
  std::unique_ptr<net::test_server::HttpResponse> RequestHandler(
      const net::test_server::HttpRequest& request);

  net::EmbeddedTestServer https_server_;

  // Stores the observed HTTP Request headers.
  std::map<GURL, net::test_server::HttpRequest::HeaderMap> received_headers_;

  DISALLOW_COPY_AND_ASSIGN(VariationsHttpHeadersBrowserTest);
};

std::unique_ptr<net::test_server::HttpResponse>
VariationsHttpHeadersBrowserTest::RequestHandler(
    const net::test_server::HttpRequest& request) {
  // Retrieve the host name (without port) from the request headers.
  std::string host = "";
  if (request.headers.find("Host") != request.headers.end())
    host = request.headers.find("Host")->second;
  if (host.find(':') != std::string::npos)
    host = host.substr(0, host.find(':'));

  // Recover the original URL of the request by replacing the host name in
  // request.GetURL() (which is 127.0.0.1) with the host name from the request
  // headers.
  url::Replacements<char> replacements;
  replacements.SetHost(host.c_str(), url::Component(0, host.length()));
  GURL original_url = request.GetURL().ReplaceComponents(replacements);

  // Memorize the request headers for this URL for later verification.
  received_headers_[original_url] = request.headers;

  // Set up a test server that redirects according to the
  // following redirect chain:
  // https://www.google.com:<port>/redirect
  // --> https://www.google.com:<port>/redirect2
  // --> https://www.example.com:<port>/
  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->AddCustomHeader("Access-Control-Allow-Origin", "*");
  if (request.relative_url == GetGoogleRedirectUrl1().path()) {
    http_response->set_code(net::HTTP_MOVED_PERMANENTLY);
    http_response->AddCustomHeader("Location", GetGoogleRedirectUrl2().spec());
  } else if (request.relative_url == GetGoogleRedirectUrl2().path()) {
    http_response->set_code(net::HTTP_MOVED_PERMANENTLY);
    http_response->AddCustomHeader("Location", GetExampleUrl().spec());
  } else if (request.relative_url == GetExampleUrl().path()) {
    http_response->set_code(net::HTTP_OK);
    http_response->set_content("hello");
    http_response->set_content_type("text/plain");
  } else {
    return nullptr;
  }
  return http_response;
}

}  // namespace

// Verify in an integration test that the variations header (X-Client-Data) is
// attached to network requests to Google but stripped on redirects.
IN_PROC_BROWSER_TEST_F(VariationsHttpHeadersBrowserTest,
                       TestStrippingHeadersFromResourceRequest) {
  ui_test_utils::NavigateToURL(browser(), GetGoogleRedirectUrl1());

  EXPECT_TRUE(HasReceivedHeader(GetGoogleRedirectUrl1(), "X-Client-Data"));
  EXPECT_TRUE(HasReceivedHeader(GetGoogleRedirectUrl2(), "X-Client-Data"));
  EXPECT_TRUE(HasReceivedHeader(GetExampleUrl(), "Host"));
  EXPECT_FALSE(HasReceivedHeader(GetExampleUrl(), "X-Client-Data"));
}

// Verify in an integration that that the variations header (X-Client-Data) is
// correctly attached and stripped from network requests.
IN_PROC_BROWSER_TEST_F(VariationsHttpHeadersBrowserTest,
                       TestStrippingHeadersFromSubresourceRequest) {
  GURL url = server()->GetURL("/simple_page.html");
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_TRUE(FetchResource(GetGoogleRedirectUrl1()));
  EXPECT_TRUE(HasReceivedHeader(GetGoogleRedirectUrl1(), "X-Client-Data"));
  EXPECT_TRUE(HasReceivedHeader(GetGoogleRedirectUrl2(), "X-Client-Data"));
  EXPECT_TRUE(HasReceivedHeader(GetExampleUrl(), "Host"));
  EXPECT_FALSE(HasReceivedHeader(GetExampleUrl(), "X-Client-Data"));
}

IN_PROC_BROWSER_TEST_F(
    VariationsHttpHeadersBrowserTest,
    TestStrippingHeadersFromRequestUsingSimpleURLLoaderWithProfileNetworkContext) {
  GURL url = GetGoogleRedirectUrl1();

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = url;

  std::unique_ptr<network::SimpleURLLoader> loader =
      variations::CreateSimpleURLLoaderWithVariationsHeaderUnknownSignedIn(
          std::move(resource_request), variations::InIncognito::kNo,
          TRAFFIC_ANNOTATION_FOR_TESTS);

  content::StoragePartition* partition =
      content::BrowserContext::GetDefaultStoragePartition(browser()->profile());
  network::SharedURLLoaderFactory* loader_factory =
      partition->GetURLLoaderFactoryForBrowserProcess().get();
  content::SimpleURLLoaderTestHelper loader_helper;
  loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      loader_factory, loader_helper.GetCallback());

  // Wait for the response to complete.
  loader_helper.WaitForCallback();
  EXPECT_EQ(net::OK, loader->NetError());
  EXPECT_TRUE(loader_helper.response_body());

  EXPECT_TRUE(HasReceivedHeader(GetGoogleRedirectUrl1(), "X-Client-Data"));
  EXPECT_TRUE(HasReceivedHeader(GetGoogleRedirectUrl2(), "X-Client-Data"));
  EXPECT_TRUE(HasReceivedHeader(GetExampleUrl(), "Host"));
  EXPECT_FALSE(HasReceivedHeader(GetExampleUrl(), "X-Client-Data"));
}

IN_PROC_BROWSER_TEST_F(
    VariationsHttpHeadersBrowserTest,
    TestStrippingHeadersFromRequestUsingSimpleURLLoaderWithGlobalSystemNetworkContext) {
  GURL url = GetGoogleRedirectUrl1();

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = url;

  std::unique_ptr<network::SimpleURLLoader> loader =
      variations::CreateSimpleURLLoaderWithVariationsHeaderUnknownSignedIn(
          std::move(resource_request), variations::InIncognito::kNo,
          TRAFFIC_ANNOTATION_FOR_TESTS);

  network::SharedURLLoaderFactory* loader_factory =
      g_browser_process->system_network_context_manager()
          ->GetSharedURLLoaderFactory()
          .get();
  content::SimpleURLLoaderTestHelper loader_helper;
  loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      loader_factory, loader_helper.GetCallback());

  // Wait for the response to complete.
  loader_helper.WaitForCallback();
  EXPECT_EQ(net::OK, loader->NetError());
  EXPECT_TRUE(loader_helper.response_body());

  EXPECT_TRUE(HasReceivedHeader(GetGoogleRedirectUrl1(), "X-Client-Data"));
  EXPECT_TRUE(HasReceivedHeader(GetGoogleRedirectUrl2(), "X-Client-Data"));
  EXPECT_TRUE(HasReceivedHeader(GetExampleUrl(), "Host"));
  EXPECT_FALSE(HasReceivedHeader(GetExampleUrl(), "X-Client-Data"));
}

// Verify in an integration test that the variations header (X-Client-Data) is
// attached to service worker navigation preload requests. Regression test
// for https://crbug.com/873061.
IN_PROC_BROWSER_TEST_F(VariationsHttpHeadersBrowserTest,
                       ServiceWorkerNavigationPreload) {
  // Register a service worker that uses navigation preload.
  RegisterServiceWorker("/service_worker/navigation_preload_worker.js");

  // Verify "X-Client-Data" is present on the navigation to Google.
  // Also test that "Service-Worker-Navigation-Preload" is present to verify
  // we are really testing the navigation preload request.
  ui_test_utils::NavigateToURL(browser(), GetGoogleUrl());
  EXPECT_TRUE(HasReceivedHeader(GetGoogleUrl(), "X-Client-Data"));
  EXPECT_TRUE(
      HasReceivedHeader(GetGoogleUrl(), "Service-Worker-Navigation-Preload"));
}

// Verify in an integration test that the variations header (X-Client-Data) is
// attached to requests after the service worker falls back to network.
IN_PROC_BROWSER_TEST_F(VariationsHttpHeadersBrowserTest,
                       ServiceWorkerNetworkFallback) {
  ServiceWorkerTest("/service_worker/network_fallback_worker.js");
}

// Verify in an integration test that the variations header (X-Client-Data) is
// attached to requests after the service worker does
// respondWith(fetch(request)).
IN_PROC_BROWSER_TEST_F(VariationsHttpHeadersBrowserTest,
                       ServiceWorkerRespondWithFetch) {
  ServiceWorkerTest("/service_worker/respond_with_fetch_worker.js");
}

// Verify in an integration test that the variations header (X-Client-Data) is
// attached to requests for service worker scripts when installing and updating.
IN_PROC_BROWSER_TEST_F(VariationsHttpHeadersBrowserTest, ServiceWorkerScript) {
  // Register a service worker that imports scripts.
  GURL absolute_import = GetExampleUrlWithPath("/service_worker/empty.js");
  const std::string worker_path =
      "/service_worker/import_scripts_worker.js?import=" +
      net::EscapeQueryParamValue(absolute_import.spec(), false);
  RegisterServiceWorker(worker_path);

  // Test that the header is present on the main script request.
  EXPECT_TRUE(
      HasReceivedHeader(GetGoogleUrlWithPath(worker_path), "X-Client-Data"));

  // And on import script requests to Google.
  EXPECT_TRUE(HasReceivedHeader(
      GetGoogleUrlWithPath("/service_worker/empty.js"), "X-Client-Data"));

  // But not on requests not to Google.
  EXPECT_FALSE(HasReceivedHeader(absolute_import, "X-Client-Data"));

  // Prepare for the update case.
  ClearReceivedHeaders();

  // Tries to update the service worker.
  EXPECT_EQ("DONE", EvalJs(GetWebContents(), "update();"));

  // Test that the header is present on the main script request.
  EXPECT_TRUE(
      HasReceivedHeader(GetGoogleUrlWithPath(worker_path), "X-Client-Data"));

  if (blink::ServiceWorkerUtils::IsImportedScriptUpdateCheckEnabled()) {
    // And on import script requests to Google.
    EXPECT_TRUE(HasReceivedHeader(
        GetGoogleUrlWithPath("/service_worker/empty.js"), "X-Client-Data"));
    // But not on requests not to Google.
    EXPECT_FALSE(HasReceivedHeader(absolute_import, "X-Client-Data"));
  }
}

// Verify in an integration test that the variations header (X-Client-Data) is
// attached to requests for shared worker scripts.
IN_PROC_BROWSER_TEST_F(VariationsHttpHeadersBrowserTest, SharedWorkerScript) {
  WorkerScriptTest("/workers/create_shared_worker.html",
                   "/workers/import_scripts_shared_worker.js");
}

// Verify in an integration test that the variations header (X-Client-Data) is
// attached to requests for dedicated worker scripts.
IN_PROC_BROWSER_TEST_F(VariationsHttpHeadersBrowserTest,
                       DedicatedWorkerScript) {
  WorkerScriptTest("/workers/create_dedicated_worker.html",
                   "/workers/import_scripts_dedicated_worker.js");
}
