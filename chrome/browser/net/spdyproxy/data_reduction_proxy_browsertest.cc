// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <tuple>

#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/metrics/subprocess_metrics_provider.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_test_waiter.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config_service_client_test_utils.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_util.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_features.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "components/data_reduction_proxy/proto/client_config.pb.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/common/service_names.mojom.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/network_switches.h"
#include "services/network/public/mojom/network_service_test.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace data_reduction_proxy {
namespace {

using testing::HasSubstr;
using testing::Not;

constexpr char kSessionKey[] = "TheSessionKeyYay!";
constexpr char kMockHost[] = "mock.host";
constexpr char kDummyBody[] = "dummy";
constexpr char kPrimaryResponse[] = "primary";
constexpr char kSecondaryResponse[] = "secondary";

std::unique_ptr<net::test_server::HttpResponse> BasicResponse(
    const std::string& content,
    const net::test_server::HttpRequest& request) {
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_content(content);
  response->set_content_type("text/plain");
  return response;
}

std::unique_ptr<net::test_server::HttpResponse> IncrementRequestCount(
    const std::string& relative_url,
    int* request_count,
    const net::test_server::HttpRequest& request) {
  if (request.relative_url == relative_url)
    (*request_count)++;
  return std::make_unique<net::test_server::BasicHttpResponse>();
}

void SimulateNetworkChange(network::mojom::ConnectionType type) {
  if (base::FeatureList::IsEnabled(network::features::kNetworkService) &&
      !content::IsNetworkServiceRunningInProcess()) {
    network::mojom::NetworkServiceTestPtr network_service_test;
    content::ServiceManagerConnection::GetForProcess()
        ->GetConnector()
        ->BindInterface(content::mojom::kNetworkServiceName,
                        &network_service_test);
    base::RunLoop run_loop;
    network_service_test->SimulateNetworkChange(type, run_loop.QuitClosure());
    run_loop.Run();
    return;
  }
  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::ConnectionType(type));
}

ClientConfig CreateConfigForServer(const net::EmbeddedTestServer& server) {
  net::HostPortPair host_port_pair = server.host_port_pair();
  return CreateConfig(
      kSessionKey, 1000, 0, ProxyServer_ProxyScheme_HTTP, host_port_pair.host(),
      host_port_pair.port(), ProxyServer::CORE, ProxyServer_ProxyScheme_HTTP,
      "fallback.net", 80, ProxyServer::UNSPECIFIED_TYPE, 0.5f, false);
}

}  // namespace

class DataReductionProxyBrowsertestBase : public InProcessBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(
        network::switches::kForceEffectiveConnectionType, "4G");

    secure_proxy_check_server_.RegisterRequestHandler(
        base::BindRepeating(&BasicResponse, "OK"));
    ASSERT_TRUE(secure_proxy_check_server_.Start());
    command_line->AppendSwitchASCII(
        switches::kDataReductionProxySecureProxyCheckURL,
        secure_proxy_check_server_.base_url().spec());

    config_server_.RegisterRequestHandler(base::BindRepeating(
        &DataReductionProxyBrowsertestBase::GetConfigResponse,
        base::Unretained(this)));
    ASSERT_TRUE(config_server_.Start());
    command_line->AppendSwitchASCII(switches::kDataReductionProxyConfigURL,
                                    config_server_.base_url().spec());
  }

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        features::kDataReductionProxyEnabledWithNetworkService);
    param_feature_list_.InitAndEnableFeatureWithParameters(
        features::kDataReductionProxyRobustConnection,
        {{params::GetMissingViaBypassParamName(), "true"},
         {"warmup_fetch_callback_enabled", "true"}});
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    // Make sure the favicon doesn't mess with the tests.
    favicon_catcher_ =
        std::make_unique<net::test_server::ControllableHttpResponse>(
            embedded_test_server(), "/favicon.ico");
    ASSERT_TRUE(embedded_test_server()->Start());
    // Set a default proxy config if one isn't set yet.
    if (!config_.has_proxy_config())
      SetConfig(CreateConfigForServer(*embedded_test_server()));

    host_resolver()->AddRule(kMockHost, "127.0.0.1");

    EnableDataSaver(true);
    // Make sure initial config has been loaded.
    WaitForConfig();
  }

 protected:
  void EnableDataSaver(bool enabled) {
    PrefService* prefs = browser()->profile()->GetPrefs();
    prefs->SetBoolean(::prefs::kDataSaverEnabled, enabled);
    base::RunLoop().RunUntilIdle();
  }

  std::string GetBody() { return GetBody(browser()); }

  std::string GetBody(Browser* browser) {
    std::string body;
    EXPECT_TRUE(content::ExecuteScriptAndExtractString(
        browser->tab_strip_model()->GetActiveWebContents(),
        "window.domAutomationController.send(document.body.textContent);",
        &body));
    return body;
  }

  GURL GetURLWithMockHost(const net::EmbeddedTestServer& server,
                          const std::string& relative_url) {
    GURL server_base_url = server.base_url();
    GURL base_url =
        GURL(base::StrCat({server_base_url.scheme(), "://", kMockHost, ":",
                           server_base_url.port()}));
    EXPECT_TRUE(base_url.is_valid()) << base_url.possibly_invalid_spec();
    return base_url.Resolve(relative_url);
  }

  void SetConfig(const ClientConfig& config) {
    config_run_loop_ = std::make_unique<base::RunLoop>();
    config_ = config;
  }

  void WaitForConfig() { config_run_loop_->Run(); }

 private:
  std::unique_ptr<net::test_server::HttpResponse> GetConfigResponse(
      const net::test_server::HttpRequest& request) {
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_content(config_.SerializeAsString());
    response->set_content_type("text/plain");
    if (config_run_loop_)
      config_run_loop_->Quit();
    return response;
  }

  ClientConfig config_;
  std::unique_ptr<base::RunLoop> config_run_loop_;
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::ScopedFeatureList param_feature_list_;
  net::EmbeddedTestServer secure_proxy_check_server_;
  net::EmbeddedTestServer config_server_;
  std::unique_ptr<net::test_server::ControllableHttpResponse> favicon_catcher_;
};

class DataReductionProxyBrowsertest : public DataReductionProxyBrowsertestBase {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    DataReductionProxyBrowsertestBase::SetUpCommandLine(command_line);
    command_line->AppendSwitch(
        switches::kDisableDataReductionProxyWarmupURLFetch);
  }
};

IN_PROC_BROWSER_TEST_F(DataReductionProxyBrowsertest, UpdateConfig) {
  net::EmbeddedTestServer original_server;
  original_server.RegisterRequestHandler(
      base::BindRepeating(&BasicResponse, kPrimaryResponse));
  ASSERT_TRUE(original_server.Start());

  SetConfig(CreateConfigForServer(original_server));
  // A network change forces the config to be fetched.
  SimulateNetworkChange(network::mojom::ConnectionType::CONNECTION_3G);
  WaitForConfig();

  ui_test_utils::NavigateToURL(browser(), GURL("http://does.not.resolve/foo"));

  EXPECT_EQ(GetBody(), kPrimaryResponse);

  net::EmbeddedTestServer new_server;
  new_server.RegisterRequestHandler(
      base::BindRepeating(&BasicResponse, kSecondaryResponse));
  ASSERT_TRUE(new_server.Start());

  SetConfig(CreateConfigForServer(new_server));
  // A network change forces the config to be fetched.
  SimulateNetworkChange(network::mojom::ConnectionType::CONNECTION_2G);
  WaitForConfig();

  ui_test_utils::NavigateToURL(browser(), GURL("http://does.not.resolve/foo"));

  EXPECT_EQ(GetBody(), kSecondaryResponse);
}

IN_PROC_BROWSER_TEST_F(DataReductionProxyBrowsertest, ChromeProxyHeaderSet) {
  // Proxy will be used, so it shouldn't matter if the host cannot be resolved.
  ui_test_utils::NavigateToURL(
      browser(), GURL("http://does.not.resolve/echoheader?Chrome-Proxy"));

  std::string body = GetBody();
  EXPECT_THAT(body, HasSubstr(kSessionKey));
  EXPECT_THAT(body, HasSubstr("pid="));
}

IN_PROC_BROWSER_TEST_F(DataReductionProxyBrowsertest, DisabledOnIncognito) {
  net::EmbeddedTestServer test_server;
  test_server.RegisterRequestHandler(
      base::BindRepeating(&BasicResponse, kDummyBody));
  ASSERT_TRUE(test_server.Start());

  Browser* incognito = CreateIncognitoBrowser();
  ui_test_utils::NavigateToURL(
      incognito, GetURLWithMockHost(test_server, "/echoheader?Chrome-Proxy"));
  EXPECT_EQ(GetBody(incognito), kDummyBody);

  // Make sure subresource doesn't use DRP either.
  std::string script = R"((url => {
    var xhr = new XMLHttpRequest();
    xhr.open('GET', url, true);
    xhr.onload = () => domAutomationController.send(xhr.responseText);
    xhr.send();
  }))";
  std::string result;
  ASSERT_TRUE(ExecuteScriptAndExtractString(
      incognito->tab_strip_model()->GetActiveWebContents(),
      script + "('" +
          GetURLWithMockHost(test_server, "/echoheader?Chrome-Proxy").spec() +
          "')",
      &result));

  EXPECT_EQ(result, kDummyBody);
}

IN_PROC_BROWSER_TEST_F(DataReductionProxyBrowsertest,
                       ChromeProxyHeaderSetForSubresource) {
  net::EmbeddedTestServer test_server;
  test_server.RegisterRequestHandler(
      base::BindRepeating(&BasicResponse, kDummyBody));
  ASSERT_TRUE(test_server.Start());

  ui_test_utils::NavigateToURL(browser(),
                               GetURLWithMockHost(test_server, "/echo"));

  std::string script = R"((url => {
    var xhr = new XMLHttpRequest();
    xhr.open('GET', url, true);
    xhr.onload = () => domAutomationController.send(xhr.responseText);
    xhr.send();
  }))";
  std::string result;
  ASSERT_TRUE(ExecuteScriptAndExtractString(
      browser()->tab_strip_model()->GetActiveWebContents(),
      script + "('" +
          GetURLWithMockHost(test_server, "/echoheader?Chrome-Proxy").spec() +
          "')",
      &result));

  EXPECT_THAT(result, HasSubstr(kSessionKey));
  EXPECT_THAT(result, Not(HasSubstr("pid=")));
}

IN_PROC_BROWSER_TEST_F(DataReductionProxyBrowsertest, ChromeProxyEctHeaderSet) {
  // Proxy will be used, so it shouldn't matter if the host cannot be resolved.
  ui_test_utils::NavigateToURL(
      browser(), GURL("http://does.not.resolve/echoheader?Chrome-Proxy-Ect"));

  EXPECT_EQ(GetBody(), "4G");
}

IN_PROC_BROWSER_TEST_F(DataReductionProxyBrowsertest,
                       ProxyNotUsedWhenDisabled) {
  net::EmbeddedTestServer test_server;
  test_server.RegisterRequestHandler(
      base::BindRepeating(&BasicResponse, kDummyBody));
  ASSERT_TRUE(test_server.Start());

  ui_test_utils::NavigateToURL(
      browser(), GetURLWithMockHost(test_server, "/echoheader?Chrome-Proxy"));
  EXPECT_THAT(GetBody(), testing::HasSubstr(kSessionKey));

  EnableDataSaver(false);

  // |test_server| only has the BasicResponse handler, so should return the
  // dummy response no matter what the URL if it is not being proxied.
  ui_test_utils::NavigateToURL(
      browser(), GetURLWithMockHost(test_server, "/echoheader?Chrome-Proxy"));
  EXPECT_EQ(GetBody(), kDummyBody);
}

IN_PROC_BROWSER_TEST_F(DataReductionProxyBrowsertest, UMAMetricsRecorded) {
  base::HistogramTester histogram_tester;

  // Make sure we wait for timing information.
  page_load_metrics::PageLoadMetricsTestWaiter waiter(
      browser()->tab_strip_model()->GetActiveWebContents());
  waiter.AddPageExpectation(
      page_load_metrics::PageLoadMetricsTestWaiter::TimingField::kFirstPaint);

  // Proxy will be used, so it shouldn't matter if the host cannot be resolved.
  ui_test_utils::NavigateToURL(browser(), GURL("http://does.not.resolve/echo"));
  waiter.Wait();

  SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  histogram_tester.ExpectUniqueSample("DataReductionProxy.ProxySchemeUsed",
                                      ProxyScheme::PROXY_SCHEME_HTTP, 1);
  histogram_tester.ExpectTotalCount(
      "PageLoad.Clients.DataReductionProxy.PaintTiming."
      "NavigationToFirstContentfulPaint",
      1);
}

class DataReductionProxyFallbackBrowsertest
    : public DataReductionProxyBrowsertest {
 public:
  void SetUpOnMainThread() override {
    // Set up a primary server which will return the Chrome-Proxy header set by
    // SetHeader() and status set by SetStatusCode(). Secondary server will just
    // return the secondary response.
    primary_server_.RegisterRequestHandler(base::BindRepeating(
        &DataReductionProxyFallbackBrowsertest::AddChromeProxyHeader,
        base::Unretained(this)));
    ASSERT_TRUE(primary_server_.Start());

    secondary_server_.RegisterRequestHandler(
        base::BindRepeating(&BasicResponse, kSecondaryResponse));
    ASSERT_TRUE(secondary_server_.Start());

    net::HostPortPair primary_host_port_pair = primary_server_.host_port_pair();
    net::HostPortPair secondary_host_port_pair =
        secondary_server_.host_port_pair();
    SetConfig(CreateConfig(
        kSessionKey, 1000, 0, ProxyServer_ProxyScheme_HTTP,
        primary_host_port_pair.host(), primary_host_port_pair.port(),
        ProxyServer::CORE, ProxyServer_ProxyScheme_HTTP,
        secondary_host_port_pair.host(), secondary_host_port_pair.port(),
        ProxyServer::CORE, 0.5f, false));

    DataReductionProxyBrowsertest::SetUpOnMainThread();
  }

  void SetHeader(const std::string& header) { header_ = header; }

  void SetStatusCode(net::HttpStatusCode status_code) {
    status_code_ = status_code;
  }

 private:
  std::unique_ptr<net::test_server::HttpResponse> AddChromeProxyHeader(
      const net::test_server::HttpRequest& request) {
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    if (!header_.empty())
      response->AddCustomHeader(chrome_proxy_header(), header_);
    response->set_code(status_code_);
    response->set_content(kPrimaryResponse);
    response->set_content_type("text/plain");
    return response;
  }

  net::HttpStatusCode status_code_ = net::HTTP_OK;
  std::string header_;
  net::EmbeddedTestServer primary_server_;
  net::EmbeddedTestServer secondary_server_;
};

IN_PROC_BROWSER_TEST_F(DataReductionProxyFallbackBrowsertest,
                       FallbackProxyUsedOn500Status) {
  // Should fall back to the secondary proxy if a 500 error occurs.
  SetStatusCode(net::HTTP_INTERNAL_SERVER_ERROR);
  ui_test_utils::NavigateToURL(
      browser(), GURL("http://does.not.resolve/echoheader?Chrome-Proxy"));
  EXPECT_THAT(GetBody(), kSecondaryResponse);

  // Bad proxy should still be bypassed.
  SetStatusCode(net::HTTP_OK);
  ui_test_utils::NavigateToURL(
      browser(), GURL("http://does.not.resolve/echoheader?Chrome-Proxy"));
  EXPECT_THAT(GetBody(), kSecondaryResponse);
}

IN_PROC_BROWSER_TEST_F(DataReductionProxyFallbackBrowsertest,
                       FallbackProxyUsedWhenBypassHeaderSent) {
  // Should fall back to the secondary proxy if the bypass header is set.
  SetHeader("bypass=100");
  ui_test_utils::NavigateToURL(
      browser(), GURL("http://does.not.resolve/echoheader?Chrome-Proxy"));
  EXPECT_THAT(GetBody(), kSecondaryResponse);

  // Bad proxy should still be bypassed.
  SetHeader("");
  ui_test_utils::NavigateToURL(
      browser(), GURL("http://does.not.resolve/echoheader?Chrome-Proxy"));
  EXPECT_THAT(GetBody(), kSecondaryResponse);
}

IN_PROC_BROWSER_TEST_F(DataReductionProxyFallbackBrowsertest,
                       NoProxyUsedWhenBlockOnceHeaderSent) {
  net::EmbeddedTestServer test_server;
  test_server.RegisterRequestHandler(
      base::BindRepeating(&BasicResponse, kDummyBody));
  ASSERT_TRUE(test_server.Start());

  // Request should not use a proxy.
  SetHeader("block-once");
  ui_test_utils::NavigateToURL(browser(),
                               GetURLWithMockHost(test_server, "/echo"));
  EXPECT_THAT(GetBody(), kDummyBody);

  // Proxy should no longer be blocked, and use first proxy.
  SetHeader("");
  ui_test_utils::NavigateToURL(browser(),
                               GetURLWithMockHost(test_server, "/echo"));
  EXPECT_EQ(GetBody(), kPrimaryResponse);
}

IN_PROC_BROWSER_TEST_F(DataReductionProxyFallbackBrowsertest,
                       FallbackProxyUsedWhenBlockHeaderSent) {
  net::EmbeddedTestServer test_server;
  test_server.RegisterRequestHandler(
      base::BindRepeating(&BasicResponse, kDummyBody));
  ASSERT_TRUE(test_server.Start());

  // Request should not use a proxy.
  SetHeader("block=100");
  ui_test_utils::NavigateToURL(browser(),
                               GetURLWithMockHost(test_server, "/echo"));
  EXPECT_THAT(GetBody(), kDummyBody);

  // Request should still not use proxy.
  SetHeader("");
  ui_test_utils::NavigateToURL(browser(),
                               GetURLWithMockHost(test_server, "/echo"));
  EXPECT_THAT(GetBody(), kDummyBody);
}

IN_PROC_BROWSER_TEST_F(DataReductionProxyFallbackBrowsertest,
                       FallbackProxyUsedWhenBlockZeroHeaderSent) {
  net::EmbeddedTestServer test_server;
  test_server.RegisterRequestHandler(
      base::BindRepeating(&BasicResponse, kDummyBody));
  ASSERT_TRUE(test_server.Start());

  // Request should not use a proxy. Sending 0 for the block param will block
  // requests for a random duration between 1 and 5 minutes.
  SetHeader("block=0");
  ui_test_utils::NavigateToURL(browser(),
                               GetURLWithMockHost(test_server, "/echo"));
  EXPECT_THAT(GetBody(), kDummyBody);

  // Request should still not use proxy.
  SetHeader("");
  ui_test_utils::NavigateToURL(browser(),
                               GetURLWithMockHost(test_server, "/echo"));
  EXPECT_THAT(GetBody(), kDummyBody);
}

class DataReductionProxyResourceTypeBrowsertest
    : public DataReductionProxyBrowsertest {
 public:
  void SetUpOnMainThread() override {
    // Two proxies are set up here, one with type CORE and one UNSPECIFIED_TYPE.
    // The CORE proxy is the secondary, and should be used for requests from the
    // <video> tag.
    unspecified_server_.RegisterRequestHandler(base::BindRepeating(
        &IncrementRequestCount, "/video", &unspecified_request_count_));
    ASSERT_TRUE(unspecified_server_.Start());

    core_server_.RegisterRequestHandler(base::BindRepeating(
        &IncrementRequestCount, "/video", &core_request_count_));
    ASSERT_TRUE(core_server_.Start());

    net::HostPortPair unspecified_host_port_pair =
        unspecified_server_.host_port_pair();
    net::HostPortPair core_host_port_pair = core_server_.host_port_pair();
    SetConfig(CreateConfig(
        kSessionKey, 1000, 0, ProxyServer_ProxyScheme_HTTP,
        unspecified_host_port_pair.host(), unspecified_host_port_pair.port(),
        ProxyServer::UNSPECIFIED_TYPE, ProxyServer_ProxyScheme_HTTP,
        core_host_port_pair.host(), core_host_port_pair.port(),
        ProxyServer::CORE, 0.5f, false));

    DataReductionProxyBrowsertest::SetUpOnMainThread();
  }

  int unspecified_request_count_ = 0;
  int core_request_count_ = 0;

 private:
  net::EmbeddedTestServer unspecified_server_;
  net::EmbeddedTestServer core_server_;
};

IN_PROC_BROWSER_TEST_F(DataReductionProxyResourceTypeBrowsertest,
                       CoreProxyUsedForMedia) {
  ui_test_utils::NavigateToURL(
      browser(), GetURLWithMockHost(*embedded_test_server(), "/echo"));

  std::string script = R"((url => {
    var video = document.createElement('video');
    // Use onerror since the response is not a valid video.
    video.onerror = () => domAutomationController.send('done');
    video.src = url;
    video.load();
    document.body.appendChild(video);
  }))";
  std::string result;
  ASSERT_TRUE(ExecuteScriptAndExtractString(
      browser()->tab_strip_model()->GetActiveWebContents(),
      script + "('" +
          GetURLWithMockHost(*embedded_test_server(), "/video").spec() + "')",
      &result));
  EXPECT_EQ(result, "done");

  EXPECT_EQ(unspecified_request_count_, 0);
  EXPECT_EQ(core_request_count_, 1);
}

class DataReductionProxyWarmupURLBrowsertest
    : public ::testing::WithParamInterface<
          std::tuple<ProxyServer_ProxyScheme, bool>>,
      public DataReductionProxyBrowsertestBase {
 public:
  DataReductionProxyWarmupURLBrowsertest()
      : via_header_(std::get<1>(GetParam()) ? "1.1 Chrome-Compression-Proxy"
                                            : "bad"),
        primary_server_(GetTestServerType()),
        secondary_server_(GetTestServerType()) {}

  void SetUpOnMainThread() override {
    primary_server_loop_ = std::make_unique<base::RunLoop>();
    primary_server_.RegisterRequestHandler(base::BindRepeating(
        &DataReductionProxyWarmupURLBrowsertest::WaitForWarmupRequest,
        base::Unretained(this), primary_server_loop_.get()));
    ASSERT_TRUE(primary_server_.Start());

    secondary_server_loop_ = std::make_unique<base::RunLoop>();
    secondary_server_.RegisterRequestHandler(base::BindRepeating(
        &DataReductionProxyWarmupURLBrowsertest::WaitForWarmupRequest,
        base::Unretained(this), secondary_server_loop_.get()));
    ASSERT_TRUE(secondary_server_.Start());

    net::HostPortPair primary_host_port_pair = primary_server_.host_port_pair();
    net::HostPortPair secondary_host_port_pair =
        secondary_server_.host_port_pair();
    SetConfig(CreateConfig(
        kSessionKey, 1000, 0, std::get<0>(GetParam()),
        primary_host_port_pair.host(), primary_host_port_pair.port(),
        ProxyServer::UNSPECIFIED_TYPE, std::get<0>(GetParam()),
        secondary_host_port_pair.host(), secondary_host_port_pair.port(),
        ProxyServer::CORE, 0.5f, false));

    DataReductionProxyBrowsertestBase::SetUpOnMainThread();
  }

  // Retries fetching |histogram_name| until it contains at least |count|
  // samples.
  void RetryForHistogramUntilCountReached(
      base::HistogramTester* histogram_tester,
      const std::string& histogram_name,
      size_t count) {
    base::RunLoop().RunUntilIdle();
    for (size_t attempt = 0; attempt < 3; ++attempt) {
      const std::vector<base::Bucket> buckets =
          histogram_tester->GetAllSamples(histogram_name);
      size_t total_count = 0;
      for (const auto& bucket : buckets)
        total_count += bucket.count;
      if (total_count >= count)
        return;
      content::FetchHistogramsFromChildProcesses();
      SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
      base::RunLoop().RunUntilIdle();
    }
  }

  std::string GetHistogramName(ProxyServer::ProxyType type) {
    return base::StrCat(
        {"DataReductionProxy.WarmupURLFetcherCallback.SuccessfulFetch.",
         std::get<0>(GetParam()) == ProxyServer_ProxyScheme_HTTP ? "Insecure"
                                                                 : "Secure",
         "Proxy.", type == ProxyServer::CORE ? "Core" : "NonCore"});
  }

  std::unique_ptr<base::RunLoop> primary_server_loop_;
  std::unique_ptr<base::RunLoop> secondary_server_loop_;
  base::HistogramTester histogram_tester_;

 private:
  net::EmbeddedTestServer::Type GetTestServerType() {
    if (std::get<0>(GetParam()) == ProxyServer_ProxyScheme_HTTP)
      return net::EmbeddedTestServer::TYPE_HTTP;
    return net::EmbeddedTestServer::TYPE_HTTPS;
  }

  std::unique_ptr<net::test_server::HttpResponse> WaitForWarmupRequest(
      base::RunLoop* run_loop,
      const net::test_server::HttpRequest& request) {
    if (base::StartsWith(request.relative_url, "/e2e_probe",
                         base::CompareCase::SENSITIVE)) {
      run_loop->Quit();
    }
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_content("content");
    response->AddCustomHeader("via", via_header_);
    return response;
  }

  const std::string via_header_;
  net::EmbeddedTestServer primary_server_;
  net::EmbeddedTestServer secondary_server_;
};

IN_PROC_BROWSER_TEST_P(DataReductionProxyWarmupURLBrowsertest,
                       WarmupURLsFetchedForEachProxy) {
  primary_server_loop_->Run();
  secondary_server_loop_->Run();

  SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  RetryForHistogramUntilCountReached(
      &histogram_tester_, GetHistogramName(ProxyServer::UNSPECIFIED_TYPE), 1);
  RetryForHistogramUntilCountReached(&histogram_tester_,
                                     GetHistogramName(ProxyServer::CORE), 1);

  histogram_tester_.ExpectUniqueSample(
      GetHistogramName(ProxyServer::UNSPECIFIED_TYPE), std::get<1>(GetParam()),
      1);
  histogram_tester_.ExpectUniqueSample(GetHistogramName(ProxyServer::CORE),
                                       std::get<1>(GetParam()), 1);
}

// First parameter indicate proxy scheme for proxies that are being tested.
// Second parameter is true if the test proxy server should set via header
// correctly on the response headers.
INSTANTIATE_TEST_CASE_P(
    ,
    DataReductionProxyWarmupURLBrowsertest,
    ::testing::Combine(testing::Values(ProxyServer_ProxyScheme_HTTP,
                                       ProxyServer_ProxyScheme_HTTPS),
                       ::testing::Bool()));

}  // namespace data_reduction_proxy
