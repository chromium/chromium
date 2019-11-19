// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <tuple>

#include "base/bind.h"
#include "base/metrics/field_trial_param_associator.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/test/bind_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings_factory.h"
#include "chrome/browser/metrics/subprocess_metrics_provider.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config_service_client_test_utils.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_service.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_util.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_features.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_pref_names.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "components/data_reduction_proxy/core/common/uma_util.h"
#include "components/data_reduction_proxy/proto/client_config.pb.h"
#include "components/page_load_metrics/browser/page_load_metrics_test_waiter.h"
#include "components/prefs/pref_service.h"
#include "components/previews/core/previews_experiments.h"
#include "components/previews/core/previews_features.h"
#include "components/proxy_config/proxy_config_dictionary.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/network_service_util.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/host_port_pair.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_request_headers.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/embedded_test_server_connection_listener.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/network_switches.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/network_service_test.mojom.h"
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

// Given a |request| to a proxy server, returns the destination host name.
std::string GetDestinationHost(const net::test_server::HttpRequest& request) {
  const auto it = request.headers.find("Host");
  if (it == request.headers.end())
    return {};
  return it->second;
}

void SimulateNetworkChange(network::mojom::ConnectionType type) {
  if (!content::IsInProcessNetworkService()) {
    mojo::Remote<network::mojom::NetworkServiceTest> network_service_test;
    content::GetNetworkService()->BindTestInterface(
        network_service_test.BindNewPipeAndPassReceiver());
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
  return CreateConfig(kSessionKey, 1000, 0, ProxyServer_ProxyScheme_HTTP,
                      host_port_pair.host(), host_port_pair.port(),
                      ProxyServer_ProxyScheme_HTTP, "fallback.net", 80, 0.5f,
                      false);
}

ClientConfig CreateEmptyConfig() {
  return CreateEmptyProxyConfig(kSessionKey, 1000, 0, 0.5f, false);
}

// Waits for a new config to be pushed to throttles. When a throttle config
// observer is added, the current config is sent immediately. This class makes
// sure a new config has been sent that is different from the original config.
class ScopedConfigWaiter
    : public mojom::DataReductionProxyThrottleConfigObserver {
 public:
  explicit ScopedConfigWaiter(Profile* profile) {
    DataReductionProxyChromeSettingsFactory::GetForBrowserContext(profile)
        ->data_reduction_proxy_service()
        ->AddThrottleConfigObserver(receiver_.BindNewPipeAndPassRemote());
  }

  ~ScopedConfigWaiter() override { run_loop_.Run(); }

 private:
  // mojom::DataReductionProxyThrottleConfigObserver:
  void OnThrottleConfigChanged(
      mojom::DataReductionProxyThrottleConfigPtr config) override {
    if (!initial_config_) {
      initial_config_ = std::move(config);
      return;
    }

    if (!config->Equals(*initial_config_))
      run_loop_.Quit();
  }

  mojom::DataReductionProxyThrottleConfigPtr initial_config_;
  mojo::Receiver<mojom::DataReductionProxyThrottleConfigObserver> receiver_{
      this};
  base::RunLoop run_loop_;
};

}  // namespace

#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_CHROMEOS)
#define DISABLE_ON_WIN_MAC_CHROMEOS(x) DISABLED_##x
#else
#define DISABLE_ON_WIN_MAC_CHROMEOS(x) x
#endif

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

  void SetUp() override { InProcessBrowserTest::SetUp(); }

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
    WaitForConfig();
  }

  // Verifies that the |request| has the Chrome-Proxy headers, and caches the
  // URL of |request| in a local container. This can be added to any test that
  // needs to verify if the proxy server is receiving the correct set of
  // headers.
  void MonitorAndVerifyRequestsToProxyServer(
      const net::test_server::HttpRequest& request) {
    ++count_proxy_server_requests_received_;
    // All requests to proxy server should have at least these headers.
    EXPECT_NE(request.headers.end(),
              request.headers.find(data_reduction_proxy::chrome_proxy_header()))
        << " url=" << request.GetURL() << " path=" << request.GetURL().path();

    VerifyChromeProxyRequestHeader(
        request.headers.at(data_reduction_proxy::chrome_proxy_header()));

    EXPECT_NE(
        request.headers.end(),
        request.headers.find(data_reduction_proxy::chrome_proxy_ect_header()))
        << " url=" << request.GetURL() << " path=" << request.GetURL().path();

    base::AutoLock lock(lock_);
    monitored_urls_.push_back(request.GetURL());
  }

  void VerifyChromeProxyRequestHeader(
      const std::string& chrome_proxy_header_value) const {
    bool exp_found = false;
    for (const auto& attributes : base::SplitStringPiece(
             chrome_proxy_header_value, ",", base::TRIM_WHITESPACE,
             base::SPLIT_WANT_NONEMPTY)) {
      if (base::StartsWith(attributes,
                           "exp=", base::CompareCase::INSENSITIVE_ASCII)) {
        const auto attribute_split = base::SplitStringPiece(
            attributes, "=", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
        EXPECT_EQ(2u, attribute_split.size());
        EXPECT_EQ(expect_exp_value_in_request_header_, attribute_split[1]);
        exp_found = true;
      }
    }
    EXPECT_EQ(!expect_exp_value_in_request_header_.empty(), exp_found)
        << " expect_exp_value_in_request_header_="
        << expect_exp_value_in_request_header_;
  }

  // Returns true if a request for URL with path |url_path| was observed by
  // |this|. The method only compares the path instead of the full URL since the
  // hostname may be different due to the use of mock host in the browsertests
  // below.
  bool WasUrlPathMonitored(const std::string& url_path) {
    base::AutoLock lock(lock_);
    for (const auto monitored_url : monitored_urls_)
      if (monitored_url.path() == url_path)
        return true;

    return false;
  }

  void ResetMonitoredUrls() {
    base::AutoLock lock(lock_);
    monitored_urls_.clear();
  }

 protected:
  void EnableDataSaver(bool enabled) {
    data_reduction_proxy::DataReductionProxySettings::
        SetDataSaverEnabledForTesting(browser()->profile()->GetPrefs(),
                                      enabled);
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
    config_ = config;
    // Config is not fetched if the holdback group is enabled and lite page
    // redirect previews are not enabled. So, return early.
    if (data_reduction_proxy::params::IsIncludedInHoldbackFieldTrial() &&
        !previews::params::IsLitePageServerPreviewsEnabled() &&
        !params::ForceEnableClientConfigServiceForAllDataSaverUsers()) {
      return;
    }

    config_waiter_ = std::make_unique<ScopedConfigWaiter>(browser()->profile());
  }

  void WaitForConfig() {
    // Config is not fetched if the holdback group is enabled and lite page
    // redirect previews are not enabled. So, return early.
    if (data_reduction_proxy::params::IsIncludedInHoldbackFieldTrial() &&
        !previews::params::IsLitePageServerPreviewsEnabled() &&
        !params::ForceEnableClientConfigServiceForAllDataSaverUsers()) {
      return;
    }
    // Destructor of |config_waiter_| waits for the config fetch request to
    // arrive. For that check to work correctly, |config_waiter_| should be
    // non-null.
    ASSERT_TRUE(config_waiter_ != nullptr);
    config_waiter_.reset();
  }

  std::string expect_exp_value_in_request_header_;

  size_t count_proxy_server_requests_received_ = 0u;

  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  // Called when |config_server_| receives a request for config fetch.
  std::unique_ptr<net::test_server::HttpResponse> GetConfigResponse(
      const net::test_server::HttpRequest& request) {
    // Config should be fetched only when holdback is disabled or lite page
    // redirect previews are enabled.
    EXPECT_TRUE(
        !data_reduction_proxy::params::IsIncludedInHoldbackFieldTrial() ||
        previews::params::IsLitePageServerPreviewsEnabled() ||
        params::ForceEnableClientConfigServiceForAllDataSaverUsers());

    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_content(config_.SerializeAsString());
    response->set_content_type("text/plain");
    return response;
  }

  ClientConfig config_;
  std::unique_ptr<ScopedConfigWaiter> config_waiter_;
  net::EmbeddedTestServer secure_proxy_check_server_;
  net::EmbeddedTestServer config_server_;
  std::unique_ptr<net::test_server::ControllableHttpResponse> favicon_catcher_;

  std::vector<GURL> monitored_urls_;

  // |lock_| guards access to |monitored_urls_|.
  base::Lock lock_;
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

  ui_test_utils::NavigateToURL(
      browser(),
      GetURLWithMockHost(original_server, "/echoheader?Chrome-Proxy"));

  EXPECT_EQ(GetBody(), kPrimaryResponse);

  net::EmbeddedTestServer new_server;
  new_server.RegisterRequestHandler(
      base::BindRepeating(&BasicResponse, kSecondaryResponse));
  ASSERT_TRUE(new_server.Start());

  SetConfig(CreateConfigForServer(new_server));
  // A network change forces the config to be fetched.
  SimulateNetworkChange(network::mojom::ConnectionType::CONNECTION_2G);
  WaitForConfig();

  ui_test_utils::NavigateToURL(
      browser(),
      GetURLWithMockHost(original_server, "/echoheader?Chrome-Proxy"));

  EXPECT_EQ(GetBody(), kSecondaryResponse);
}

// Verify that when a client config with no proxies is provided to Chrome,
// then usage of proxy is disabled. Later, when the client config with valid
// proxies is fetched, then the specified proxies are used.
IN_PROC_BROWSER_TEST_F(DataReductionProxyBrowsertest, EmptyConfig) {
  net::EmbeddedTestServer origin_server;
  origin_server.RegisterRequestHandler(
      base::BindRepeating(&BasicResponse, kDummyBody));
  ASSERT_TRUE(origin_server.Start());

  // Set config to empty, and verify that the response comes from the
  // |origin_server|.
  SetConfig(CreateEmptyConfig());
  // A network change forces the config to be fetched.
  SimulateNetworkChange(network::mojom::ConnectionType::CONNECTION_2G);
  WaitForConfig();
  ui_test_utils::NavigateToURL(
      browser(), GetURLWithMockHost(origin_server, "/echoheader?Chrome-Proxy"));
  ASSERT_EQ(GetBody(), kDummyBody);

  net::EmbeddedTestServer proxy_server;
  proxy_server.RegisterRequestHandler(
      base::BindRepeating(&BasicResponse, kPrimaryResponse));
  ASSERT_TRUE(proxy_server.Start());

  // Set config to |proxy_server|, and verify that the response comes from
  // |proxy_server|.
  SetConfig(CreateConfigForServer(proxy_server));
  // A network change forces the config to be fetched.
  SimulateNetworkChange(network::mojom::ConnectionType::CONNECTION_3G);
  WaitForConfig();
  ui_test_utils::NavigateToURL(
      browser(), GetURLWithMockHost(origin_server, "/echoheader?Chrome-Proxy"));
  EXPECT_EQ(GetBody(), kPrimaryResponse);

  // Set config to empty again, and verify that the response comes from the
  // |origin_server|.
  SetConfig(CreateEmptyConfig());
  // A network change forces the config to be fetched.
  SimulateNetworkChange(network::mojom::ConnectionType::CONNECTION_2G);
  WaitForConfig();
  ui_test_utils::NavigateToURL(
      browser(), GetURLWithMockHost(origin_server, "/echoheader?Chrome-Proxy"));
  ASSERT_EQ(GetBody(), kDummyBody);
}

IN_PROC_BROWSER_TEST_F(DataReductionProxyBrowsertest, ChromeProxyHeaderSet) {
  // Proxy will be used, so it shouldn't matter if the host cannot be resolved.
  ui_test_utils::NavigateToURL(
      browser(), GURL("http://does.not.resolve/echoheader?Chrome-Proxy"));

  std::string body = GetBody();
  EXPECT_THAT(body, HasSubstr(kSessionKey));
  EXPECT_THAT(body, HasSubstr("pid="));
  EXPECT_THAT(body, Not(HasSubstr("pid=0")));
  EXPECT_THAT(body, HasSubstr("s="));
  EXPECT_THAT(body, HasSubstr("c="));
  EXPECT_THAT(body, HasSubstr("b="));
  EXPECT_THAT(body, HasSubstr("p="));
}

// Gets the response body for an XHR to |url| (as seen by the renderer).
std::string ReadSubresourceFromRenderer(Browser* browser,
                                        const GURL& url,
                                        bool asynchronous_xhr = true) {
  static const char asynchronous_script[] = R"((url => {
    var xhr = new XMLHttpRequest();
    xhr.open('GET', url, true);
    xhr.onload = () => domAutomationController.send(xhr.responseText);
    xhr.send();
  }))";
  static const char synchronous_script[] = R"((url => {
    var xhr = new XMLHttpRequest();
    xhr.open('GET', url, false);
    xhr.send();
    domAutomationController.send(xhr.responseText);
  }))";
  std::string result;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      browser->tab_strip_model()->GetActiveWebContents(),
      base::StrCat({asynchronous_xhr ? asynchronous_script : synchronous_script,
                    "('", url.spec(), "')"}),
      &result));
  return result;
}

IN_PROC_BROWSER_TEST_F(DataReductionProxyBrowsertest, DisabledOnIncognito) {
  net::EmbeddedTestServer test_server;
  test_server.RegisterRequestHandler(
      base::BindRepeating(&BasicResponse, kDummyBody));
  ASSERT_TRUE(test_server.Start());

  Browser* incognito = CreateIncognitoBrowser();
  ASSERT_FALSE(DataReductionProxyChromeSettingsFactory::GetForBrowserContext(
      incognito->profile()));
  ASSERT_TRUE(DataReductionProxyChromeSettingsFactory::GetForBrowserContext(
      browser()->profile()));

  ui_test_utils::NavigateToURL(
      incognito, GetURLWithMockHost(test_server, "/echoheader?Chrome-Proxy"));
  EXPECT_EQ(GetBody(incognito), kDummyBody);

  // Make sure subresource doesn't use DRP either.
  std::string result = ReadSubresourceFromRenderer(
      incognito, GetURLWithMockHost(test_server, "/echoheader?Chrome-Proxy"));

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

  std::string result = ReadSubresourceFromRenderer(
      browser(), GetURLWithMockHost(test_server, "/echoheader?Chrome-Proxy"));

  EXPECT_THAT(result, HasSubstr(kSessionKey));
  EXPECT_THAT(result, Not(HasSubstr("pid=")));
  EXPECT_THAT(result, HasSubstr("s="));
  EXPECT_THAT(result, HasSubstr("c="));
  EXPECT_THAT(result, HasSubstr("b="));
  EXPECT_THAT(result, HasSubstr("p="));
}

IN_PROC_BROWSER_TEST_F(DataReductionProxyBrowsertest,
                       ChromeProxyHeaderSetForSubresourceSync) {
  net::EmbeddedTestServer test_server;
  test_server.RegisterRequestHandler(
      base::BindRepeating(&BasicResponse, kDummyBody));
  ASSERT_TRUE(test_server.Start());

  ui_test_utils::NavigateToURL(browser(),
                               GetURLWithMockHost(test_server, "/echo"));

  const bool asynchronous_xhr = false;
  std::string result = ReadSubresourceFromRenderer(
      browser(), GetURLWithMockHost(test_server, "/echoheader?Chrome-Proxy"),
      asynchronous_xhr);

  EXPECT_THAT(result, HasSubstr(kSessionKey));
  EXPECT_THAT(result, Not(HasSubstr("pid=")));
  EXPECT_THAT(result, HasSubstr("s="));
  EXPECT_THAT(result, HasSubstr("c="));
  EXPECT_THAT(result, HasSubstr("b="));
  EXPECT_THAT(result, HasSubstr("p="));
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

IN_PROC_BROWSER_TEST_F(DataReductionProxyBrowsertest,
                       ProxyNotUsedForWebSocket) {
  // Expect the WebSocket handshake to be attempted with |test_server|
  // directly.
  base::RunLoop web_socket_handshake_loop;
  net::EmbeddedTestServer test_server;
  test_server.RegisterRequestHandler(
      base::BindRepeating(&BasicResponse, kDummyBody));
  test_server.RegisterRequestMonitor(base::BindLambdaForTesting(
      [&web_socket_handshake_loop](
          const net::test_server::HttpRequest& request) {
        if (request.headers.count("upgrade") > 0u)
          web_socket_handshake_loop.Quit();
      }));
  ASSERT_TRUE(test_server.Start());

  // If the DRP client (erroneously) decides to proxy the WebSocket handshake,
  // it will attempt to establish a tunnel through |drp_server|.
  net::EmbeddedTestServer drp_server;
  drp_server.AddDefaultHandlers(GetChromeTestDataDir());
  bool tunnel_attempted = false;
  drp_server.RegisterRequestMonitor(base::BindLambdaForTesting(
      [&tunnel_attempted, &web_socket_handshake_loop](
          const net::test_server::HttpRequest& request) {
        if (request.method == net::test_server::METHOD_CONNECT) {
          tunnel_attempted = true;
          web_socket_handshake_loop.Quit();
        }
      }));
  ASSERT_TRUE(drp_server.Start());
  SetConfig(CreateConfigForServer(drp_server));
  // A network change forces the config to be fetched.
  SimulateNetworkChange(network::mojom::ConnectionType::CONNECTION_3G);
  WaitForConfig();

  ui_test_utils::NavigateToURL(browser(),
                               GetURLWithMockHost(test_server, "/echo"));

  const std::string url =
      base::StrCat({"ws://", kMockHost, ":", test_server.base_url().port()});
  const std::string script = R"((url => {
    var ws = new WebSocket(url);
  }))";
  EXPECT_TRUE(
      ExecuteScript(browser()->tab_strip_model()->GetActiveWebContents(),
                    script + "('" + url + "')"));
  web_socket_handshake_loop.Run();
  EXPECT_FALSE(tunnel_attempted);
}

IN_PROC_BROWSER_TEST_F(DataReductionProxyBrowsertest,
                       DoesNotOverrideExistingProxyConfig) {
  // When there's a proxy configuration provided to the browser already (system
  // proxy, command line, etc.), the DRP proxy must not override it.
  net::EmbeddedTestServer existing_proxy_server;
  existing_proxy_server.RegisterRequestHandler(
      base::BindRepeating(&BasicResponse, kDummyBody));
  ASSERT_TRUE(existing_proxy_server.Start());

  browser()->profile()->GetPrefs()->Set(
      proxy_config::prefs::kProxy,
      ProxyConfigDictionary::CreateFixedServers(
          existing_proxy_server.host_port_pair().ToString(), ""));

  EnableDataSaver(true);

  // Proxy will be used, so it shouldn't matter if the host cannot be resolved.
  ui_test_utils::NavigateToURL(browser(),
                               GURL("http://does.not.resolve.com/echo"));

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

// Test that enabling the holdback disables the proxy and that the client config
// is fetched when lite page redirect preview is enabled.
class DataReductionProxyWithHoldbackBrowsertest
    : public ::testing::WithParamInterface<std::tuple<bool, bool, bool>>,
      public DataReductionProxyBrowsertest {
 public:
  DataReductionProxyWithHoldbackBrowsertest()
      : DataReductionProxyBrowsertest(),
        // Consider the holdback as enabled if holdback is enabled or the
        // |force_enable_config_service_fetches_| is enabled.
        data_reduction_proxy_holdback_enabled_(std::get<2>(GetParam()) ||
                                               std::get<0>(GetParam())),
        lite_page_redirect_previews_enabled_(std::get<1>(GetParam())),
        force_enable_config_service_fetches_(std::get<2>(GetParam())) {}

  void SetUp() override {
    if (force_enable_config_service_fetches_) {
      scoped_feature_list_.InitAndEnableFeatureWithParameters(
          data_reduction_proxy::features::kDataReductionProxyHoldback,
          {{"force_enable_config_service_fetches", "true"}});
    }
    if (!force_enable_config_service_fetches_ &&
        data_reduction_proxy_holdback_enabled_) {
      scoped_feature_list_.InitAndEnableFeature(
          data_reduction_proxy::features::kDataReductionProxyHoldback);
    }
    if (lite_page_redirect_previews_enabled_) {
      previews_lite_page_redirect_feature_list_.InitAndEnableFeature(
          previews::features::kLitePageServerPreviews);
    }

    InProcessBrowserTest::SetUp();
  }

  const bool data_reduction_proxy_holdback_enabled_;
  const bool lite_page_redirect_previews_enabled_;
  const bool force_enable_config_service_fetches_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::ScopedFeatureList previews_lite_page_redirect_feature_list_;
};

IN_PROC_BROWSER_TEST_P(DataReductionProxyWithHoldbackBrowsertest,
                       UpdateConfig) {
  net::EmbeddedTestServer proxy_server;
  proxy_server.RegisterRequestMonitor(base::BindRepeating(
      &DataReductionProxyBrowsertest::MonitorAndVerifyRequestsToProxyServer,
      base::Unretained(this)));
  proxy_server.RegisterRequestHandler(
      base::BindRepeating(&BasicResponse, kPrimaryResponse));
  ASSERT_TRUE(proxy_server.Start());

  SetConfig(CreateConfigForServer(proxy_server));
  // A network change forces the config to be fetched.
  SimulateNetworkChange(network::mojom::ConnectionType::CONNECTION_3G);

  // Ensure that the client config is fetched when lite page redirect preview is
  // enabled or DRP holdback is disabled.
  WaitForConfig();

  // Load a webpage in holdback group as well. This ensures that while in
  // holdback group, Chrome does not fetch the client config. If Chrome were to
  // fetch the client config, the DHCECKs and other conditionals that check that
  // holdback is not enabled would trigger and cause the test to fail.
  ui_test_utils::NavigateToURL(browser(), GURL("http://does.not.resolve/foo"));

  if (data_reduction_proxy_holdback_enabled_ ||
      force_enable_config_service_fetches_) {
    EXPECT_NE(GetBody(), kPrimaryResponse);
  } else {
    EXPECT_EQ(GetBody(), kPrimaryResponse);
  }
}

// First parameter is true if the data reduction proxy holdback should be
// enabled. Second parameter is true if lite page redirect preview is enabled.
// Third parameter is true if data reduction proxy config should always be
// fetched.
INSTANTIATE_TEST_SUITE_P(,
                         DataReductionProxyWithHoldbackBrowsertest,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::Bool(),
                                            ::testing::Bool()));

class DataReductionProxyExpBrowsertest : public DataReductionProxyBrowsertest {
 public:
  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        data_reduction_proxy::switches::kDataReductionProxyExperiment,
        "foo_experiment");

    DataReductionProxyBrowsertest::SetUp();
  }
};

IN_PROC_BROWSER_TEST_F(DataReductionProxyExpBrowsertest,
                       ChromeProxyExpHeaderSet) {
  expect_exp_value_in_request_header_ = "foo_experiment";

  net::EmbeddedTestServer proxy_server;
  proxy_server.RegisterRequestMonitor(base::BindRepeating(
      &DataReductionProxyBrowsertest::MonitorAndVerifyRequestsToProxyServer,
      base::Unretained(this)));
  proxy_server.RegisterRequestHandler(
      base::BindRepeating(&BasicResponse, kPrimaryResponse));
  ASSERT_TRUE(proxy_server.Start());
  SetConfig(CreateConfigForServer(proxy_server));
  // A network change forces the config to be fetched.
  SimulateNetworkChange(network::mojom::ConnectionType::CONNECTION_3G);
  WaitForConfig();

  ui_test_utils::NavigateToURL(browser(), GURL("http://does.not.resolve/foo"));
  EXPECT_LE(1u, count_proxy_server_requests_received_);
}

class DataReductionProxyExpFeatureBrowsertest
    : public DataReductionProxyBrowsertest {
 public:
  void SetUp() override {
    std::map<std::string, std::string> field_trial_params;
    field_trial_params[data_reduction_proxy::params::
                           GetDataSaverServerExperimentsOptionName()] =
        experiment_name;

    scoped_feature_list_.InitWithFeaturesAndParameters(
        {
            {data_reduction_proxy::features::
                 kDataReductionProxyServerExperiments,
             {field_trial_params}},
        },
        {});

    InProcessBrowserTest::SetUp();
  }

  const std::string experiment_name = "foo_feature_experiment";
};

IN_PROC_BROWSER_TEST_F(DataReductionProxyExpFeatureBrowsertest,
                       ChromeProxyExpHeaderSet) {
  expect_exp_value_in_request_header_ = experiment_name;

  net::EmbeddedTestServer proxy_server;
  proxy_server.RegisterRequestMonitor(base::BindRepeating(
      &DataReductionProxyBrowsertest::MonitorAndVerifyRequestsToProxyServer,
      base::Unretained(this)));
  proxy_server.RegisterRequestHandler(
      base::BindRepeating(&BasicResponse, kPrimaryResponse));
  ASSERT_TRUE(proxy_server.Start());
  SetConfig(CreateConfigForServer(proxy_server));
  // A network change forces the config to be fetched.
  SimulateNetworkChange(network::mojom::ConnectionType::CONNECTION_3G);
  WaitForConfig();

  ui_test_utils::NavigateToURL(browser(), GURL("http://does.not.resolve/foo"));
  EXPECT_LE(1u, count_proxy_server_requests_received_);
}

IN_PROC_BROWSER_TEST_F(DataReductionProxyBrowsertest, DataUsePrefsRecorded) {
  PrefService* prefs = browser()->profile()->GetPrefs();

  // Make sure we wait for timing information.
  page_load_metrics::PageLoadMetricsTestWaiter waiter(
      browser()->tab_strip_model()->GetActiveWebContents());
  waiter.AddPageExpectation(
      page_load_metrics::PageLoadMetricsTestWaiter::TimingField::kFirstPaint);

  // Proxy will be used, so it shouldn't matter if the host cannot be resolved.
  ui_test_utils::NavigateToURL(browser(), GURL("http://does.not.resolve/echo"));
  waiter.Wait();

  ASSERT_GE(0, prefs->GetInt64(
                   data_reduction_proxy::prefs::kHttpReceivedContentLength));
  ASSERT_GE(0, prefs->GetInt64(
                   data_reduction_proxy::prefs::kHttpOriginalContentLength));
}

class DataReductionProxyFallbackBrowsertest
    : public DataReductionProxyBrowsertest {
 public:
  using ResponseHook =
      base::RepeatingCallback<void(net::test_server::BasicHttpResponse*)>;

  void SetUpOnMainThread() override {
    // Set up a primary server which will return the Chrome-Proxy header set by
    // SetHeader() and status set by SetStatusCode(). Secondary server will just
    // return the secondary response.
    primary_server_.RegisterRequestHandler(base::BindRepeating(
        &DataReductionProxyFallbackBrowsertest::AddChromeProxyHeader,
        base::Unretained(this)));
    primary_server_.RegisterRequestMonitor(base::BindRepeating(
        &DataReductionProxyBrowsertest::MonitorAndVerifyRequestsToProxyServer,
        base::Unretained(this)));
    ASSERT_TRUE(primary_server_.Start());

    secondary_server_.RegisterRequestHandler(
        base::BindRepeating(&BasicResponse, kSecondaryResponse));
    secondary_server_.RegisterRequestMonitor(base::BindRepeating(
        &DataReductionProxyBrowsertest::MonitorAndVerifyRequestsToProxyServer,
        base::Unretained(this)));
    ASSERT_TRUE(secondary_server_.Start());

    net::HostPortPair primary_host_port_pair = primary_server_.host_port_pair();
    net::HostPortPair secondary_host_port_pair =
        secondary_server_.host_port_pair();
    SetConfig(CreateConfig(
        kSessionKey, 1000, 0, ProxyServer_ProxyScheme_HTTP,
        primary_host_port_pair.host(), primary_host_port_pair.port(),
        ProxyServer_ProxyScheme_HTTP, secondary_host_port_pair.host(),
        secondary_host_port_pair.port(), 0.5f, false));

    DataReductionProxyBrowsertest::SetUpOnMainThread();
  }

  void SetResponseHook(ResponseHook response_hook) {
    base::AutoLock auto_lock(lock_);
    response_hook_ = response_hook;
  }

  void SetHeader(const std::string& header) {
    base::AutoLock auto_lock(lock_);
    header_ = header;
  }

  void SetStatusCode(net::HttpStatusCode status_code) {
    base::AutoLock auto_lock(lock_);
    status_code_ = status_code;
  }

  // If the request is for the URL from |host_port_pair|, then response
  // status code would be set to |status_code|.
  void SetStatusCodeForURLsFromHostPortPair(
      const net::HostPortPair& host_port_pair,
      net::HttpStatusCode status_code) {
    base::AutoLock auto_lock(lock_);
    special_host_port_pair_ = host_port_pair;
    special_status_code_ = status_code;
  }

  void SetLocationHeader(const std::string& header) {
    base::AutoLock auto_lock(lock_);
    location_header_ = header;
  }

  void TearDown() override {
    EXPECT_LE(1u, count_proxy_server_requests_received_);
  }

 private:
  std::unique_ptr<net::test_server::HttpResponse> AddChromeProxyHeader(
      const net::test_server::HttpRequest& request) {
    base::AutoLock auto_lock(lock_);
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    if (!header_.empty())
      response->AddCustomHeader(chrome_proxy_header(), header_);
    if (!location_header_.empty())
      response->AddCustomHeader("Location", location_header_);
    if (response_hook_)
      response_hook_.Run(response.get());

    // Compute the requested URL from the "Host" header. It's not possible
    // to use the request URL directly since that contains the hostname of the
    // proxy server.
    bool use_special_status_code = false;
    if (request.headers.find("Host") != request.headers.end()) {
      const GURL kOriginUrl(
          base::StrCat({"http://", request.headers.find("Host")->second +
                                       request.GetURL().path()}));

      if (!special_host_port_pair_.IsEmpty() &&
          net::HostPortPair::FromURL(kOriginUrl) == special_host_port_pair_) {
        use_special_status_code = true;
      }
    }

    if (use_special_status_code) {
      response->set_code(special_status_code_);
    } else {
      response->set_code(status_code_);
    }
    response->set_content(kPrimaryResponse);
    response->set_content_type("text/plain");
    return response;
  }

  // |lock_| guards access to all the local variables except the embedded test
  // servers directly.
  base::Lock lock_;
  std::string header_;
  std::string location_header_;

  // If the request is for the URL from |special_host_port_pair_|, then response
  // status code is set to |special_status_code_|. Otherwise, it is set to
  // |status_code_|.
  net::HostPortPair special_host_port_pair_;
  net::HttpStatusCode special_status_code_ = net::HTTP_OK;
  net::HttpStatusCode status_code_ = net::HTTP_OK;

  ResponseHook response_hook_;
  net::EmbeddedTestServer primary_server_;
  net::EmbeddedTestServer secondary_server_;
};

IN_PROC_BROWSER_TEST_F(DataReductionProxyFallbackBrowsertest,
                       FallbackProxyUsedOnNetError) {
  SetResponseHook(
      base::BindRepeating([](net::test_server::BasicHttpResponse* response) {
        response->AddCustomHeader("Content-Disposition", "inline");
        response->AddCustomHeader("Content-Disposition", "form-data");
      }));
  base::HistogramTester histogram_tester;
  ui_test_utils::NavigateToURL(
      browser(), GURL("http://does.not.resolve/echoheader?Chrome-Proxy"));
  EXPECT_THAT(GetBody(), kSecondaryResponse);
  histogram_tester.ExpectUniqueSample(
      "DataReductionProxy.InvalidResponseHeadersReceived.NetError",
      -net::ERR_RESPONSE_HEADERS_MULTIPLE_CONTENT_DISPOSITION, 1);

  // Bad proxy should still be bypassed.
  SetResponseHook(ResponseHook());
  ui_test_utils::NavigateToURL(
      browser(), GURL("http://does.not.resolve/echoheader?Chrome-Proxy"));
  EXPECT_THAT(GetBody(), kSecondaryResponse);
}

IN_PROC_BROWSER_TEST_F(DataReductionProxyFallbackBrowsertest,
                       FallbackProxyUsedOn500Status) {
  base::HistogramTester histogram_tester;
  // Should fall back to the secondary proxy if a 500 error occurs.
  SetStatusCode(net::HTTP_INTERNAL_SERVER_ERROR);
  ui_test_utils::NavigateToURL(
      browser(), GURL("http://does.not.resolve/echoheader?Chrome-Proxy"));
  EXPECT_THAT(GetBody(), kSecondaryResponse);
  histogram_tester.ExpectUniqueSample(
      "DataReductionProxy.BypassTypePrimary",
      BYPASS_EVENT_TYPE_STATUS_500_HTTP_INTERNAL_SERVER_ERROR, 1);

  // Bad proxy should still be bypassed.
  SetStatusCode(net::HTTP_OK);
  ui_test_utils::NavigateToURL(
      browser(), GURL("http://does.not.resolve/echoheader?Chrome-Proxy"));
  EXPECT_THAT(GetBody(), kSecondaryResponse);
}

IN_PROC_BROWSER_TEST_F(DataReductionProxyFallbackBrowsertest,
                       FallbackProxyUsedWhenBypassHeaderSent) {
  base::HistogramTester histogram_tester;
  // Should fall back to the secondary proxy if the bypass header is set.
  SetHeader("bypass=100");
  ui_test_utils::NavigateToURL(
      browser(), GURL("http://does.not.resolve/echoheader?Chrome-Proxy"));
  EXPECT_THAT(GetBody(), kSecondaryResponse);
  histogram_tester.ExpectUniqueSample("DataReductionProxy.BypassTypePrimary",
                                      BYPASS_EVENT_TYPE_MEDIUM, 1);

  // Bad proxy should still be bypassed.
  SetHeader("");
  ui_test_utils::NavigateToURL(
      browser(), GURL("http://does.not.resolve/echoheader?Chrome-Proxy"));
  EXPECT_THAT(GetBody(), kSecondaryResponse);
}

IN_PROC_BROWSER_TEST_F(
    DataReductionProxyFallbackBrowsertest,
    DISABLE_ON_WIN_MAC_CHROMEOS(BadProxiesResetWhenDisabled)) {
  base::HistogramTester histogram_tester;
  SetHeader("bypass=100");
  ui_test_utils::NavigateToURL(
      browser(), GURL("http://does.not.resolve/echoheader?Chrome-Proxy"));
  EXPECT_THAT(GetBody(), kSecondaryResponse);
  histogram_tester.ExpectUniqueSample("DataReductionProxy.BypassTypePrimary",
                                      BYPASS_EVENT_TYPE_MEDIUM, 1);

  // Disabling and enabling DRP should clear the bypass.
  EnableDataSaver(false);
  EnableDataSaver(true);

  SetHeader("");
  ui_test_utils::NavigateToURL(
      browser(), GURL("http://does.not.resolve/echoheader?Chrome-Proxy"));
  EXPECT_THAT(GetBody(), kPrimaryResponse);
}

IN_PROC_BROWSER_TEST_F(DataReductionProxyFallbackBrowsertest,
                       NoProxyUsedWhenBlockOnceHeaderSent) {
  base::HistogramTester histogram_tester;
  net::EmbeddedTestServer test_server;
  test_server.RegisterRequestHandler(
      base::BindRepeating(&BasicResponse, kDummyBody));
  ASSERT_TRUE(test_server.Start());

  // Request should not use a proxy.
  SetHeader("block-once");
  ui_test_utils::NavigateToURL(browser(),
                               GetURLWithMockHost(test_server, "/echo"));
  EXPECT_THAT(GetBody(), kDummyBody);
  EXPECT_LE(
      1, histogram_tester.GetBucketCount("DataReductionProxy.BlockTypePrimary",
                                         BYPASS_EVENT_TYPE_CURRENT));

  // Proxy should no longer be blocked, and use first proxy.
  SetHeader("");
  ui_test_utils::NavigateToURL(browser(),
                               GetURLWithMockHost(test_server, "/echo"));
  EXPECT_EQ(GetBody(), kPrimaryResponse);
}

IN_PROC_BROWSER_TEST_F(DataReductionProxyFallbackBrowsertest,
                       FallbackProxyUsedWhenBlockHeaderSent) {
  base::HistogramTester histogram_tester;
  net::EmbeddedTestServer test_server;
  test_server.RegisterRequestHandler(
      base::BindRepeating(&BasicResponse, kDummyBody));
  ASSERT_TRUE(test_server.Start());

  // Request should not use a proxy.
  SetHeader("block=100");
  ui_test_utils::NavigateToURL(browser(),
                               GetURLWithMockHost(test_server, "/echo"));
  EXPECT_THAT(GetBody(), kDummyBody);
  histogram_tester.ExpectUniqueSample("DataReductionProxy.BlockTypePrimary",
                                      BYPASS_EVENT_TYPE_MEDIUM, 1);

  // Request should still not use proxy.
  SetHeader("");
  ui_test_utils::NavigateToURL(browser(),
                               GetURLWithMockHost(test_server, "/echo"));
  EXPECT_THAT(GetBody(), kDummyBody);
}

IN_PROC_BROWSER_TEST_F(DataReductionProxyFallbackBrowsertest,
                       FallbackProxyUsedWhenBlockZeroHeaderSent) {
  base::HistogramTester histogram_tester;
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
  histogram_tester.ExpectUniqueSample("DataReductionProxy.BlockTypePrimary",
                                      BYPASS_EVENT_TYPE_MEDIUM, 1);

  // Request should still not use proxy.
  SetHeader("");
  ui_test_utils::NavigateToURL(browser(),
                               GetURLWithMockHost(test_server, "/echo"));
  EXPECT_THAT(GetBody(), kDummyBody);
}

IN_PROC_BROWSER_TEST_F(DataReductionProxyFallbackBrowsertest,
                       FallbackProxyUsedWhenBlockForLargeDurationSent) {
  base::HistogramTester histogram_tester;
  net::EmbeddedTestServer test_server;
  test_server.RegisterRequestHandler(
      base::BindRepeating(&BasicResponse, kDummyBody));
  ASSERT_TRUE(test_server.Start());

  // Sending block=86400 triggers a long bypass event that blocks requests for
  // a day.
  SetHeader("block=86400");
  ui_test_utils::NavigateToURL(browser(),
                               GetURLWithMockHost(test_server, "/echo"));
  EXPECT_THAT(GetBody(), kDummyBody);
  histogram_tester.ExpectUniqueSample("DataReductionProxy.BlockTypePrimary",
                                      BYPASS_EVENT_TYPE_LONG, 1);

  // Request should still not use proxy.
  SetHeader("");
  ui_test_utils::NavigateToURL(browser(),
                               GetURLWithMockHost(test_server, "/echo"));
  EXPECT_THAT(GetBody(), kDummyBody);
}

IN_PROC_BROWSER_TEST_F(DataReductionProxyFallbackBrowsertest,
                       DISABLE_ON_WIN_MAC_CHROMEOS(ProxyBlockedOnAuthError)) {
  base::HistogramTester histogram_tester;
  net::EmbeddedTestServer test_server;
  test_server.RegisterRequestHandler(
      base::BindRepeating(&BasicResponse, kDummyBody));
  ASSERT_TRUE(test_server.Start());

  SetStatusCode(net::HTTP_PROXY_AUTHENTICATION_REQUIRED);

  ui_test_utils::NavigateToURL(browser(),
                               GetURLWithMockHost(test_server, "/echo"));
  EXPECT_THAT(GetBody(), kDummyBody);
  histogram_tester.ExpectUniqueSample("DataReductionProxy.BlockTypePrimary",
                                      BYPASS_EVENT_TYPE_MALFORMED_407, 1);
}

// Tests that if using data reduction proxy results in redirect loop, then
// the proxy is bypassed, and the request is fetched directly.
IN_PROC_BROWSER_TEST_F(DataReductionProxyFallbackBrowsertest,
                       DISABLE_ON_WIN_MAC_CHROMEOS(RedirectCycle)) {
  base::HistogramTester histogram_tester;
  net::EmbeddedTestServer test_server;
  test_server.RegisterRequestHandler(
      base::BindRepeating(&BasicResponse, kDummyBody));
  ASSERT_TRUE(test_server.Start());

  const GURL kUrl(GetURLWithMockHost(test_server, "/echo"));
  SetStatusCodeForURLsFromHostPortPair(net::HostPortPair::FromURL(kUrl),
                                       net::HTTP_TEMPORARY_REDIRECT);
  SetLocationHeader(kUrl.spec());
  ui_test_utils::NavigateToURL(browser(), kUrl);
  EXPECT_THAT(GetBody(), kDummyBody);

  // Request should still not use proxy.
  ui_test_utils::NavigateToURL(browser(), kUrl);
  EXPECT_THAT(GetBody(), kDummyBody);
}

class DataReductionProxyResourceTypeBrowsertest
    : public DataReductionProxyBrowsertest {
 public:
  void SetUpOnMainThread() override {
    unspecified_server_.RegisterRequestHandler(base::BindRepeating(
        &IncrementRequestCount, "/video", &first_proxy_request_count_));
    ASSERT_TRUE(unspecified_server_.Start());

    core_server_.RegisterRequestHandler(base::BindRepeating(
        &IncrementRequestCount, "/video", &second_proxy_request_count_));
    ASSERT_TRUE(core_server_.Start());

    net::HostPortPair unspecified_host_port_pair =
        unspecified_server_.host_port_pair();
    net::HostPortPair core_host_port_pair = core_server_.host_port_pair();
    SetConfig(CreateConfig(
        kSessionKey, 1000, 0, ProxyServer_ProxyScheme_HTTP,
        unspecified_host_port_pair.host(), unspecified_host_port_pair.port(),
        ProxyServer_ProxyScheme_HTTP, core_host_port_pair.host(),
        core_host_port_pair.port(), 0.5f, false));

    DataReductionProxyBrowsertest::SetUpOnMainThread();
  }

  int first_proxy_request_count_ = 0;
  int second_proxy_request_count_ = 0;

 private:
  net::EmbeddedTestServer unspecified_server_;
  net::EmbeddedTestServer core_server_;
};

IN_PROC_BROWSER_TEST_F(DataReductionProxyFallbackBrowsertest,
                       ProxyBypassedOn502Error) {
  base::HistogramTester histogram_tester;
  net::EmbeddedTestServer test_server;
  test_server.RegisterRequestHandler(
      base::BindRepeating(&BasicResponse, kDummyBody));
  ASSERT_TRUE(test_server.Start());

  SetStatusCode(net::HTTP_BAD_GATEWAY);

  ui_test_utils::NavigateToURL(browser(),
                               GetURLWithMockHost(test_server, "/echo"));
  EXPECT_THAT(GetBody(), kSecondaryResponse);
  histogram_tester.ExpectUniqueSample(
      "DataReductionProxy.BypassTypePrimary",
      BYPASS_EVENT_TYPE_STATUS_502_HTTP_BAD_GATEWAY, 1);
}

class DataReductionProxyFallbackBrowsertestWithBlockOnBadGatewayFeature
    : public DataReductionProxyFallbackBrowsertest {
 public:
  DataReductionProxyFallbackBrowsertestWithBlockOnBadGatewayFeature() {
    feature_list_.InitAndEnableFeatureWithParameters(
        features::kDataReductionProxyBlockOnBadGatewayResponse,
        {{"block_duration_seconds", "10"}});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    DataReductionProxyFallbackBrowsertestWithBlockOnBadGatewayFeature,
    ProxyShortBypassedOn502ErrorWithFeature) {
  base::HistogramTester histogram_tester;
  net::EmbeddedTestServer test_server;
  test_server.RegisterRequestHandler(
      base::BindRepeating(&BasicResponse, kDummyBody));
  ASSERT_TRUE(test_server.Start());

  SetStatusCode(net::HTTP_BAD_GATEWAY);

  ui_test_utils::NavigateToURL(browser(),
                               GetURLWithMockHost(test_server, "/echo"));
  // Both the proxies should be blocked.
  EXPECT_THAT(GetBody(), kDummyBody);
  histogram_tester.ExpectUniqueSample("DataReductionProxy.BlockTypePrimary",
                                      BYPASS_EVENT_TYPE_SHORT, 1);
}

IN_PROC_BROWSER_TEST_F(DataReductionProxyResourceTypeBrowsertest,
                       FirstProxyUsedForMedia) {
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

  EXPECT_EQ(first_proxy_request_count_, 1);
  EXPECT_EQ(second_proxy_request_count_, 0);
}

class DataReductionProxyWarmupURLBrowsertest
    : public ::testing::WithParamInterface<
          std::tuple<ProxyServer_ProxyScheme, bool, bool>>,
      public DataReductionProxyBrowsertestBase {
 public:
  DataReductionProxyWarmupURLBrowsertest()
      : via_header_(std::get<1>(GetParam()) ? "1.1 Chrome-Compression-Proxy"
                                            : "bad"),
        primary_server_(GetTestServerType()),
        secondary_server_(GetTestServerType()) {
    if (!std::get<2>(GetParam())) {
      scoped_feature_list_.InitAndDisableFeature(
          features::kDataReductionProxyDisableProxyFailedWarmup);
    }
  }

  void SetUpOnMainThread() override {
    primary_server_loop_ = std::make_unique<base::RunLoop>();
    primary_server_.RegisterRequestHandler(base::BindRepeating(
        &DataReductionProxyWarmupURLBrowsertest::WaitForWarmupRequest,
        base::Unretained(this), primary_server_loop_.get()));
    primary_server_.RegisterRequestMonitor(base::BindRepeating(
        &DataReductionProxyBrowsertest::MonitorAndVerifyRequestsToProxyServer,
        base::Unretained(this)));
    ASSERT_TRUE(primary_server_.Start());

    secondary_server_loop_ = std::make_unique<base::RunLoop>();
    secondary_server_.RegisterRequestHandler(base::BindRepeating(
        &DataReductionProxyWarmupURLBrowsertest::WaitForWarmupRequest,
        base::Unretained(this), secondary_server_loop_.get()));
    secondary_server_.RegisterRequestMonitor(base::BindRepeating(
        &DataReductionProxyBrowsertest::MonitorAndVerifyRequestsToProxyServer,
        base::Unretained(this)));
    ASSERT_TRUE(secondary_server_.Start());

    net::HostPortPair primary_host_port_pair = primary_server_.host_port_pair();
    net::HostPortPair secondary_host_port_pair =
        secondary_server_.host_port_pair();
    SetConfig(CreateConfig(
        kSessionKey, 1000, 0, std::get<0>(GetParam()),
        primary_host_port_pair.host(), primary_host_port_pair.port(),
        std::get<0>(GetParam()), secondary_host_port_pair.host(),
        secondary_host_port_pair.port(), 0.5f, false));

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

  std::string GetHistogramName() {
    return base::StrCat(
        {"DataReductionProxy.WarmupURLFetcherCallback.SuccessfulFetch.",
         std::get<0>(GetParam()) == ProxyServer_ProxyScheme_HTTP ? "Insecure"
                                                                 : "Secure",
         "Proxy.Core"});
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
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    if (base::StartsWith(request.relative_url, "/e2e_probe",
                         base::CompareCase::SENSITIVE)) {
      run_loop->Quit();
      response->set_content("content");
      response->AddCustomHeader("via", via_header_);
      const auto user_agent =
          request.headers.find(net::HttpRequestHeaders::kUserAgent);
      EXPECT_TRUE(user_agent != request.headers.end());
      EXPECT_THAT(user_agent->second, HasSubstr("Chrome/"));
    } else if (base::StartsWith(request.relative_url, "/echoheader",
                                base::CompareCase::SENSITIVE)) {
      const auto chrome_proxy_header = request.headers.find("chrome-proxy");
      if (chrome_proxy_header != request.headers.end()) {
        response->set_content(chrome_proxy_header->second);
        response->AddCustomHeader("chrome-proxy", "ofcl=1000");
      }
    }
    return response;
  }

  const std::string via_header_;
  net::EmbeddedTestServer primary_server_;
  net::EmbeddedTestServer secondary_server_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(
    DataReductionProxyWarmupURLBrowsertest,
    DISABLE_ON_WIN_MAC_CHROMEOS(WarmupURLsFetchedForEachProxy)) {
  net::EmbeddedTestServer test_server;
  test_server.RegisterRequestHandler(
      base::BindRepeating(&BasicResponse, kDummyBody));
  ASSERT_TRUE(test_server.Start());

  bool is_warmup_fetch_successful = std::get<1>(GetParam());
  bool disallow_proxy_failed_warmup_feature_enabled = std::get<2>(GetParam());
  primary_server_loop_->Run();

  SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  RetryForHistogramUntilCountReached(&histogram_tester_, GetHistogramName(), 1);

  histogram_tester_.ExpectUniqueSample(GetHistogramName(),
                                       is_warmup_fetch_successful, 1);

  base::RunLoop().RunUntilIdle();

  // Navigate to some URL to see if the proxy is only used when warmup URL fetch
  // was successful.
  ui_test_utils::NavigateToURL(
      browser(), GetURLWithMockHost(test_server, "/echoheader?Chrome-Proxy"));
  std::string body = GetBody();
  if (is_warmup_fetch_successful) {
    EXPECT_THAT(body, HasSubstr(kSessionKey));
  } else {
    if (disallow_proxy_failed_warmup_feature_enabled) {
      EXPECT_THAT(body, kDummyBody);
    } else {
      // When the feature is disabled, the proxy is still being used
      EXPECT_THAT(body, HasSubstr(kSessionKey));
    }
  }
  EXPECT_TRUE(WasUrlPathMonitored("/e2e_probe"));
}

// First parameter indicate proxy scheme for proxies that are being tested.
// Second parameter is true if the test proxy server should set via header
// correctly on the response headers.
INSTANTIATE_TEST_SUITE_P(
    ,
    DataReductionProxyWarmupURLBrowsertest,
    ::testing::Combine(
        testing::Values(ProxyServer_ProxyScheme_HTTP,
                        ProxyServer_ProxyScheme_HTTPS),
        ::testing::Bool(),  // is_warmup_fetch_successful
        ::testing::Bool()   // kDataReductionProxyDisallowProxyFailedWarmup
                            // active
        ));

// Threadsafe log for recording a sequence of events as newline separated text.
class EventLog {
 public:
  void Add(const std::string& event) {
    base::AutoLock lock(lock_);
    log_ += event + "\n";
  }

  std::string GetAndReset() {
    base::AutoLock lock(lock_);
    return std::move(log_);
  }

 private:
  base::Lock lock_;
  std::string log_;
};

// Responds to requests for |path| with a 502 and "Chrome-Proxy: block-once",
// and logs the request into |event_log|.
std::unique_ptr<net::test_server::HttpResponse> DrpBlockOnceHandler(
    const std::string& server_name,
    EventLog* event_log,
    const net::test_server::HttpRequest& request) {
  if (request.relative_url == "/favicon.ico")
    return nullptr;

  event_log->Add(server_name + " responded 502 for " + request.relative_url);
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_content_type("text/plain");
  response->set_code(net::HTTP_BAD_GATEWAY);
  response->AddCustomHeader(chrome_proxy_header(), "block-once");
  return response;
}

// Responds to requests with the path as response body, and logs the request
// into |event_log|.
std::unique_ptr<net::test_server::HttpResponse> RespondWithRequestPathHandler(
    const std::string& server_name,
    EventLog* event_log,
    const net::test_server::HttpRequest& request) {
  if (request.relative_url == "/favicon.ico")
    return nullptr;

  event_log->Add(server_name + " responded 200 for " + request.relative_url);
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_content_type("text/plain");
  response->set_code(net::HTTP_OK);
  response->set_content(request.relative_url);
  return response;
}

// Verify that requests initiated by SimpleURLLoader use the proxy only if
// render frame ID is set.
IN_PROC_BROWSER_TEST_F(DataReductionProxyBrowsertest, SimpleURLLoader) {
  net::EmbeddedTestServer origin_server;
  origin_server.RegisterRequestHandler(
      base::BindRepeating(&BasicResponse, kDummyBody));
  ASSERT_TRUE(origin_server.Start());

  net::EmbeddedTestServer proxy_server;
  proxy_server.RegisterRequestMonitor(base::BindRepeating(
      &DataReductionProxyBrowsertest::MonitorAndVerifyRequestsToProxyServer,
      base::Unretained(this)));
  proxy_server.RegisterRequestHandler(
      base::BindRepeating(&BasicResponse, kPrimaryResponse));
  ASSERT_TRUE(proxy_server.Start());

  // Set config to |proxy_server|.
  SetConfig(CreateConfigForServer(proxy_server));
  // A network change forces the config to be fetched.
  SimulateNetworkChange(network::mojom::ConnectionType::CONNECTION_3G);
  WaitForConfig();

  for (const bool set_render_frame_id : {false, true}) {
    auto resource_request = std::make_unique<network::ResourceRequest>();
    if (set_render_frame_id)
      resource_request->render_frame_id = MSG_ROUTING_CONTROL;
    // Change the URL for different test cases because a bypassed URL may be
    // added to the local cache by network service proxy delegate.
    if (set_render_frame_id) {
      resource_request->url = GetURLWithMockHost(
          origin_server, "/echoheader?Chrome-Proxy&random=1");
    } else {
      resource_request->url = GetURLWithMockHost(
          origin_server, "/echoheader?Chrome-Proxy&random=2");
    }

    auto simple_loader = network::SimpleURLLoader::Create(
        std::move(resource_request), TRAFFIC_ANNOTATION_FOR_TESTS);

    auto* storage_partition =
        content::BrowserContext::GetDefaultStoragePartition(
            browser()->profile());
    auto url_loader_factory =
        storage_partition->GetURLLoaderFactoryForBrowserProcess();

    base::RunLoop loop;
    simple_loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
        url_loader_factory.get(),
        base::BindLambdaForTesting(
            [&](std::unique_ptr<std::string> response_body) {
              loop.Quit();
              ASSERT_TRUE(response_body);
              EXPECT_EQ(set_render_frame_id ? kPrimaryResponse : kDummyBody,
                        *response_body);
            }));
    loop.Run();
  }
}

// Ensure that renderer initiated same-site navigations work.
IN_PROC_BROWSER_TEST_F(DataReductionProxyBrowsertest,
                       RendererInitiatedSameSiteNavigation) {
  // Perform a browser-initiated navigation.
  net::EmbeddedTestServer origin_server;
  origin_server.ServeFilesFromSourceDirectory(
      base::FilePath(FILE_PATH_LITERAL("content/test/data")));
  ASSERT_TRUE(origin_server.Start());

  net::EmbeddedTestServer proxy_server;
  proxy_server.ServeFilesFromSourceDirectory(
      base::FilePath(FILE_PATH_LITERAL("content/test/data")));
  proxy_server.RegisterRequestMonitor(base::BindRepeating(
      &DataReductionProxyBrowsertest::MonitorAndVerifyRequestsToProxyServer,
      base::Unretained(this)));
  ASSERT_TRUE(proxy_server.Start());
  // Set config to |proxy_server|.
  SetConfig(CreateConfigForServer(proxy_server));
  // A network change forces the config to be fetched.
  SimulateNetworkChange(network::mojom::ConnectionType::CONNECTION_3G);
  WaitForConfig();

  {
    content::TestNavigationObserver observer(
        browser()->tab_strip_model()->GetActiveWebContents());
    GURL url(GetURLWithMockHost(origin_server, "/simple_links.html"));
    ui_test_utils::NavigateToURL(browser(), url);
    EXPECT_EQ(url, observer.last_navigation_url());
    EXPECT_TRUE(observer.last_navigation_succeeded());
    EXPECT_FALSE(observer.last_initiator_origin().has_value());
    EXPECT_TRUE(WasUrlPathMonitored(url.path()));
  }

  ResetMonitoredUrls();

  // Simulate clicking on a same-site link.
  {
    content::TestNavigationObserver observer(
        browser()->tab_strip_model()->GetActiveWebContents());
    GURL url(GetURLWithMockHost(origin_server, "/title2.html"));
    bool success = false;
    EXPECT_TRUE(ExecuteScriptAndExtractBool(
        browser()->tab_strip_model()->GetActiveWebContents(),
        "window.domAutomationController.send(clickSameSiteLink());", &success));
    EXPECT_TRUE(success);
    EXPECT_TRUE(
        WaitForLoadStop(browser()->tab_strip_model()->GetActiveWebContents()));
    EXPECT_EQ(url, observer.last_navigation_url());
    EXPECT_TRUE(observer.last_navigation_succeeded());
    EXPECT_EQ(browser()
                  ->tab_strip_model()
                  ->GetActiveWebContents()
                  ->GetMainFrame()
                  ->GetLastCommittedOrigin(),
              observer.last_initiator_origin());
    EXPECT_TRUE(WasUrlPathMonitored(url.path()));
  }
}

// Tests that Chrome-Proxy response headers are respected after the
// configuration is updated.
//
// When run under NetworkService, the DataReductionProxyURLLoaderThrottle
// decides whether to block-once based on the configured DRP servers. This
// config is in turn synchronized through the DataReductionProxyThrottleManager.
//
// The goal of this test is to ensure that this throttle sees the correct
// configuration when processing response headers (the UpdateConfig() test
// already checks that the network service sees the updated config).
IN_PROC_BROWSER_TEST_F(DataReductionProxyBrowsertest,
                       BlockOnceWorksAfterUpdateConfig) {
  EventLog event_log;

  // Setup a DRP server that will reply with "Chrome-Proxy: block-once".
  net::EmbeddedTestServer drp_server1;
  drp_server1.RegisterRequestHandler(base::BindRepeating(
      &DrpBlockOnceHandler, "drp_server1", base::Unretained(&event_log)));
  ASSERT_TRUE(drp_server1.Start());

  // Setup a DRP server that will reply with "Chrome-Proxy: block-once".
  net::EmbeddedTestServer drp_server2;
  drp_server2.RegisterRequestHandler(base::BindRepeating(
      &DrpBlockOnceHandler, "drp_server2", base::Unretained(&event_log)));
  ASSERT_TRUE(drp_server2.Start());

  // Regular server that will respond with the request path as body.
  net::EmbeddedTestServer direct_server;
  direct_server.RegisterRequestHandler(base::BindRepeating(
      &RespondWithRequestPathHandler, "direct_server", &event_log));
  ASSERT_TRUE(direct_server.Start());

  // Change the DRP configuration so that |drp_server1| is the current DRP.
  SetConfig(CreateConfigForServer(drp_server1));
  SimulateNetworkChange(network::mojom::ConnectionType::CONNECTION_3G);
  WaitForConfig();

  // When issuing request /x1, it should first go to |drp_server1|, and then get
  // restarted on |direct_server|.
  const char kExpectedLog1[] =
      "drp_server1 responded 502 for /x1\n"
      "direct_server responded 200 for /x1\n";

  GURL x1_url = GetURLWithMockHost(direct_server, "/x1");

  // Test a browser-initiated request.
  ui_test_utils::NavigateToURL(browser(), x1_url);
  EXPECT_EQ("/x1", GetBody());
  EXPECT_EQ(kExpectedLog1, event_log.GetAndReset());

  // Test a renderer initiated request.
  EXPECT_EQ("/x1", ReadSubresourceFromRenderer(browser(), x1_url));
  EXPECT_EQ(kExpectedLog1, event_log.GetAndReset());

  // Change the DRP configuration so that |drp_server2| is the current DRP.
  SetConfig(CreateConfigForServer(drp_server2));
  SimulateNetworkChange(network::mojom::ConnectionType::CONNECTION_3G);
  WaitForConfig();

  // When issuing request /x2, it should first go to |drp_server2|, and then get
  // restarted on |direct_server|.
  const char kExpectedLog2[] =
      "drp_server2 responded 502 for /x2\n"
      "direct_server responded 200 for /x2\n";

  // Test a browser-initiated request.
  GURL x2_url = GetURLWithMockHost(direct_server, "/x2");
  ui_test_utils::NavigateToURL(browser(), x2_url);
  EXPECT_EQ("/x2", GetBody());
  EXPECT_EQ(kExpectedLog2, event_log.GetAndReset());

  // Test a renderer initiated request.
  EXPECT_EQ("/x2", ReadSubresourceFromRenderer(browser(), x2_url));
  EXPECT_EQ(kExpectedLog2, event_log.GetAndReset());
}

IN_PROC_BROWSER_TEST_F(DataReductionProxyBrowsertest, NestedWebWorker) {
  // Nested Web Workers exercise URLLoaderThrottles in interesting ways. Each
  // worker runs on a separate thread and each one has an associated
  // URLLoaderThrottleProvider. When spawning a nested worker, the outer
  // worker's throttle provider is cloned on the outer worker's thread and then
  // used on the inner worker's thread. This test verifies that the mojo
  // connections between the throttles and DRP are set up correctly given the
  // non-trivial threading scenario.
  constexpr char kHtml[] = R"(
    <html><body><script language="javascript">
      function workerImpl() {
        function nestedWorkerImpl() {
          postMessage('done');
        }
        var blob = new Blob(['(' + nestedWorkerImpl.toString() + ')()'],
                            { type: 'application/javascript' });
        var nestedWorker = new Worker(URL.createObjectURL(blob));
        nestedWorker.onmessage = (event) => postMessage(event.data);
      }
      var blob = new Blob(['(' + workerImpl.toString() + ')()'],
                          { type: 'application/javascript' });
      var worker = new Worker(URL.createObjectURL(blob));
      worker.onmessage = (event) => document.title = event.data;
    </script></body></html>
  )";
  constexpr char kDestinationHost[] = "some.host";

  net::EmbeddedTestServer drp_server;
  drp_server.RegisterRequestHandler(base::BindLambdaForTesting(
      [&kHtml, &kDestinationHost](const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        if (GetDestinationHost(request) != kDestinationHost)
          return nullptr;

        auto response = std::make_unique<net::test_server::BasicHttpResponse>();
        response->set_content(kHtml);
        return response;
      }));
  ASSERT_TRUE(drp_server.Start());

  // Change the DRP configuration so that |drp_server| is the current DRP.
  SetConfig(CreateConfigForServer(drp_server));
  SimulateNetworkChange(network::mojom::ConnectionType::CONNECTION_3G);
  WaitForConfig();

  ui_test_utils::NavigateToURL(browser(),
                               GURL(std::string("http://") + kDestinationHost));

  const auto kExpectedTitle = base::ASCIIToUTF16("done");
  content::TitleWatcher title_watcher(
      browser()->tab_strip_model()->GetActiveWebContents(), kExpectedTitle);
  EXPECT_EQ(title_watcher.WaitAndGetTitle(), kExpectedTitle);
}

}  // namespace data_reduction_proxy
