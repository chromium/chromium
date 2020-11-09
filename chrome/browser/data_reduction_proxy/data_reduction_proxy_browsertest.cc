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
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/bind_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings_factory.h"
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
#include "components/metrics/content/subprocess_metrics_provider.h"
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
#include "content/public/test/browser_test.h"
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
constexpr char kPrimaryProxyResponse[] = "primary";

std::unique_ptr<net::test_server::HttpResponse> BasicResponse(
    const std::string& content,
    const net::test_server::HttpRequest& request) {
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_content(content);
  response->set_content_type("text/plain");
  return response;
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
  return CreateClientConfig(kSessionKey, 1000, 0);
}

ClientConfig CreateEmptyConfig() {
  return CreateClientConfig(kSessionKey, 1000, 0);
}

class TestSettingsObserver : public DataReductionProxySettingsObserver {
 public:
  TestSettingsObserver() = default;
  ~TestSettingsObserver() = default;

  void OnPrefetchProxyHostsChanged(
      const std::vector<GURL>& prefetch_proxies) override {
    prefetch_proxies_ = prefetch_proxies;
  }

  const std::vector<GURL>& prefetch_proxies() const {
    return prefetch_proxies_;
  }

 private:
  std::vector<GURL> prefetch_proxies_;
};

}  // namespace

#if defined(OS_WIN) || defined(OS_MAC) || defined(OS_CHROMEOS)
#define DISABLE_ON_WIN_MAC_CHROMEOS(x) DISABLED_##x
#else
#define DISABLE_ON_WIN_MAC_CHROMEOS(x) x
#endif

class DataReductionProxyBrowsertestBase : public InProcessBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(
        network::switches::kForceEffectiveConnectionType, "4G");

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
    // Config is not fetched if the lite page
    // redirect previews are not enabled. So, return early.
    if (!params::ForceEnableClientConfigServiceForAllDataSaverUsers()) {
      return;
    }
  }

  void RetryForHistogramUntilCountReached(
      base::HistogramTester* histogram_tester,
      const std::string& histogram_name,
      size_t count) {
    while (true) {
      base::ThreadPoolInstance::Get()->FlushForTesting();
      base::RunLoop().RunUntilIdle();

      content::FetchHistogramsFromChildProcesses();
      metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

      const std::vector<base::Bucket> buckets =
          histogram_tester->GetAllSamples(histogram_name);
      size_t total_count = 0;
      for (const auto& bucket : buckets) {
        total_count += bucket.count;
      }
      if (total_count >= count) {
        break;
      }
    }
  }

  void WaitForConfig() {
    if (!params::ForceEnableClientConfigServiceForAllDataSaverUsers()) {
      return;
    }

    base::HistogramTester histogram_tester;
    RetryForHistogramUntilCountReached(
        &histogram_tester, "DataReductionProxy.Settings.ConfigReceived", 1);
  }

  std::string expect_exp_value_in_request_header_;

  size_t count_proxy_server_requests_received_ = 0u;

  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  // Called when |config_server_| receives a request for config fetch.
  std::unique_ptr<net::test_server::HttpResponse> GetConfigResponse(
      const net::test_server::HttpRequest& request) {
    // Config should be fetched only when lite page
    // redirect previews are enabled.
    EXPECT_TRUE(params::ForceEnableClientConfigServiceForAllDataSaverUsers());

    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_content(config_.SerializeAsString());
    response->set_content_type("text/plain");
    return response;
  }

  ClientConfig config_;
  net::EmbeddedTestServer config_server_;
  std::unique_ptr<net::test_server::ControllableHttpResponse> favicon_catcher_;
};

class DataReductionProxyBrowsertest : public DataReductionProxyBrowsertestBase {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    DataReductionProxyBrowsertestBase::SetUpCommandLine(command_line);
  }
};

IN_PROC_BROWSER_TEST_F(DataReductionProxyBrowsertest, UpdateConfig) {
  net::EmbeddedTestServer original_server;
  original_server.RegisterRequestHandler(
      base::BindRepeating(&BasicResponse, kDummyBody));
  ASSERT_TRUE(original_server.Start());

  SetConfig(CreateConfigForServer(original_server));
  // A network change forces the config to be fetched.
  SimulateNetworkChange(network::mojom::ConnectionType::CONNECTION_3G);
  WaitForConfig();

  ui_test_utils::NavigateToURL(
      browser(),
      GetURLWithMockHost(original_server, "/echoheader?Chrome-Proxy"));

  EXPECT_EQ(GetBody(), kDummyBody);
}

IN_PROC_BROWSER_TEST_F(DataReductionProxyBrowsertest,
                       UpdatePrefetchProxyConfig) {
  TestSettingsObserver observer;
  DataReductionProxySettings* settings =
      DataReductionProxyChromeSettingsFactory::GetForBrowserContext(
          browser()->profile());
  settings->AddDataReductionProxySettingsObserver(&observer);

  net::EmbeddedTestServer original_server;
  original_server.RegisterRequestHandler(
      base::BindRepeating(&BasicResponse, kDummyBody));
  ASSERT_TRUE(original_server.Start());

  ClientConfig config = CreateConfigForServer(original_server);

  PrefetchProxyConfig_Proxy* valid_secure_proxy =
      config.mutable_prefetch_proxy_config()->add_proxy_list();
  valid_secure_proxy->set_type(PrefetchProxyConfig_Proxy_Type_CONNECT);
  valid_secure_proxy->set_host("prefetch-proxy.com");
  valid_secure_proxy->set_port(443);
  valid_secure_proxy->set_scheme(PrefetchProxyConfig_Proxy_Scheme_HTTPS);

  PrefetchProxyConfig_Proxy* non_connect_proxy =
      config.mutable_prefetch_proxy_config()->add_proxy_list();
  non_connect_proxy->set_type(PrefetchProxyConfig_Proxy_Type_UNSPECIFIED_TYPE);
  non_connect_proxy->set_host("prefetch-proxy.com");
  non_connect_proxy->set_port(443);
  non_connect_proxy->set_scheme(PrefetchProxyConfig_Proxy_Scheme_HTTPS);

  PrefetchProxyConfig_Proxy* unknown_scheme_proxy =
      config.mutable_prefetch_proxy_config()->add_proxy_list();
  unknown_scheme_proxy->set_type(PrefetchProxyConfig_Proxy_Type_CONNECT);
  unknown_scheme_proxy->set_host("prefetch-proxy.com");
  unknown_scheme_proxy->set_port(443);
  unknown_scheme_proxy->set_scheme(
      PrefetchProxyConfig_Proxy_Scheme_UNSPECIFIED_SCHEME);

  PrefetchProxyConfig_Proxy* invalid_host =
      config.mutable_prefetch_proxy_config()->add_proxy_list();
  invalid_host->set_type(PrefetchProxyConfig_Proxy_Type_CONNECT);
  invalid_host->set_host(std::string());
  invalid_host->set_port(443);
  invalid_host->set_scheme(PrefetchProxyConfig_Proxy_Scheme_HTTPS);

  PrefetchProxyConfig_Proxy* insecure_proxy =
      config.mutable_prefetch_proxy_config()->add_proxy_list();
  insecure_proxy->set_type(PrefetchProxyConfig_Proxy_Type_CONNECT);
  insecure_proxy->set_host("insecure-prefetch-proxy.com");
  insecure_proxy->set_port(80);
  insecure_proxy->set_scheme(PrefetchProxyConfig_Proxy_Scheme_HTTP);

  SetConfig(config);

  // A network change forces the config to be fetched.
  SimulateNetworkChange(network::mojom::ConnectionType::CONNECTION_3G);
  WaitForConfig();

  std::vector<GURL> want_hosts = {
      GURL("https://prefetch-proxy.com:443/"),
  };
  EXPECT_EQ(want_hosts, settings->GetPrefetchProxies());
  EXPECT_EQ(want_hosts, observer.prefetch_proxies());
}

IN_PROC_BROWSER_TEST_F(DataReductionProxyBrowsertest,
                       UpdatePrefetchProxyConfig_Insecure) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      "allow-insecure-prefetch-proxy-for-testing");

  TestSettingsObserver observer;
  DataReductionProxySettings* settings =
      DataReductionProxyChromeSettingsFactory::GetForBrowserContext(
          browser()->profile());
  settings->AddDataReductionProxySettingsObserver(&observer);

  net::EmbeddedTestServer original_server;
  original_server.RegisterRequestHandler(
      base::BindRepeating(&BasicResponse, kPrimaryProxyResponse));
  ASSERT_TRUE(original_server.Start());

  ClientConfig config = CreateConfigForServer(original_server);

  PrefetchProxyConfig_Proxy* valid_secure_proxy =
      config.mutable_prefetch_proxy_config()->add_proxy_list();
  valid_secure_proxy->set_type(PrefetchProxyConfig_Proxy_Type_CONNECT);
  valid_secure_proxy->set_host("prefetch-proxy.com");
  valid_secure_proxy->set_port(443);
  valid_secure_proxy->set_scheme(PrefetchProxyConfig_Proxy_Scheme_HTTPS);

  PrefetchProxyConfig_Proxy* valid_insecure_proxy =
      config.mutable_prefetch_proxy_config()->add_proxy_list();
  valid_insecure_proxy->set_type(PrefetchProxyConfig_Proxy_Type_CONNECT);
  valid_insecure_proxy->set_host("insecure-prefetch-proxy.com");
  valid_insecure_proxy->set_port(80);
  valid_insecure_proxy->set_scheme(PrefetchProxyConfig_Proxy_Scheme_HTTP);

  SetConfig(config);

  // A network change forces the config to be fetched.
  SimulateNetworkChange(network::mojom::ConnectionType::CONNECTION_3G);
  WaitForConfig();

  std::vector<GURL> want_hosts = {
      GURL("https://prefetch-proxy.com:443/"),
      GURL("http://insecure-prefetch-proxy.com/"),
  };
  EXPECT_EQ(want_hosts, settings->GetPrefetchProxies());
  EXPECT_EQ(want_hosts, observer.prefetch_proxies());
}

IN_PROC_BROWSER_TEST_F(DataReductionProxyBrowsertest,
                       UpdatePrefetchProxyConfig_CmdLineOverride_Valid) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      "prefetch-proxy-override-proxy-hosts",
      " ,NotValid, https://override-proxy.com/  ");

  TestSettingsObserver observer;
  DataReductionProxySettings* settings =
      DataReductionProxyChromeSettingsFactory::GetForBrowserContext(
          browser()->profile());
  settings->AddDataReductionProxySettingsObserver(&observer);

  net::EmbeddedTestServer original_server;
  original_server.RegisterRequestHandler(
      base::BindRepeating(&BasicResponse, kPrimaryProxyResponse));
  ASSERT_TRUE(original_server.Start());

  ClientConfig config = CreateConfigForServer(original_server);

  PrefetchProxyConfig_Proxy* valid_secure_proxy =
      config.mutable_prefetch_proxy_config()->add_proxy_list();
  valid_secure_proxy->set_type(PrefetchProxyConfig_Proxy_Type_CONNECT);
  valid_secure_proxy->set_host("prefetch-proxy.com");
  valid_secure_proxy->set_port(443);
  valid_secure_proxy->set_scheme(PrefetchProxyConfig_Proxy_Scheme_HTTPS);

  PrefetchProxyConfig_Proxy* valid_insecure_proxy =
      config.mutable_prefetch_proxy_config()->add_proxy_list();
  valid_insecure_proxy->set_type(PrefetchProxyConfig_Proxy_Type_CONNECT);
  valid_insecure_proxy->set_host("insecure-prefetch-proxy.com");
  valid_insecure_proxy->set_port(80);
  valid_insecure_proxy->set_scheme(PrefetchProxyConfig_Proxy_Scheme_HTTP);

  SetConfig(config);

  // A network change forces the config to be fetched.
  SimulateNetworkChange(network::mojom::ConnectionType::CONNECTION_3G);
  WaitForConfig();

  std::vector<GURL> want_hosts = {
      GURL("https://override-proxy.com/"),
  };
  EXPECT_EQ(want_hosts, settings->GetPrefetchProxies());
  EXPECT_EQ(want_hosts, observer.prefetch_proxies());
}

IN_PROC_BROWSER_TEST_F(DataReductionProxyBrowsertest,
                       UpdatePrefetchProxyConfig_CmdLineOverride_Invalid) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      "allow-insecure-prefetch-proxy-for-testing");
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      "prefetch-proxy-override-proxy-hosts", " ,NotValid, also-not-valid  ");

  TestSettingsObserver observer;
  DataReductionProxySettings* settings =
      DataReductionProxyChromeSettingsFactory::GetForBrowserContext(
          browser()->profile());
  settings->AddDataReductionProxySettingsObserver(&observer);

  net::EmbeddedTestServer original_server;
  original_server.RegisterRequestHandler(
      base::BindRepeating(&BasicResponse, kPrimaryProxyResponse));
  ASSERT_TRUE(original_server.Start());

  ClientConfig config = CreateConfigForServer(original_server);

  PrefetchProxyConfig_Proxy* valid_secure_proxy =
      config.mutable_prefetch_proxy_config()->add_proxy_list();
  valid_secure_proxy->set_type(PrefetchProxyConfig_Proxy_Type_CONNECT);
  valid_secure_proxy->set_host("prefetch-proxy.com");
  valid_secure_proxy->set_port(443);
  valid_secure_proxy->set_scheme(PrefetchProxyConfig_Proxy_Scheme_HTTPS);

  PrefetchProxyConfig_Proxy* valid_insecure_proxy =
      config.mutable_prefetch_proxy_config()->add_proxy_list();
  valid_insecure_proxy->set_type(PrefetchProxyConfig_Proxy_Type_CONNECT);
  valid_insecure_proxy->set_host("insecure-prefetch-proxy.com");
  valid_insecure_proxy->set_port(80);
  valid_insecure_proxy->set_scheme(PrefetchProxyConfig_Proxy_Scheme_HTTP);

  SetConfig(config);

  // A network change forces the config to be fetched.
  SimulateNetworkChange(network::mojom::ConnectionType::CONNECTION_3G);
  WaitForConfig();

  std::vector<GURL> want_hosts = {
      GURL("https://prefetch-proxy.com:443/"),
      GURL("http://insecure-prefetch-proxy.com/"),
  };
  EXPECT_EQ(want_hosts, settings->GetPrefetchProxies());
  EXPECT_EQ(want_hosts, observer.prefetch_proxies());
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
      base::BindRepeating(&BasicResponse, kPrimaryProxyResponse));
  ASSERT_TRUE(proxy_server.Start());

  // Set config to |proxy_server|, and verify that the response comes from
  // |proxy_server|.
  SetConfig(CreateConfigForServer(proxy_server));
  // A network change forces the config to be fetched.
  SimulateNetworkChange(network::mojom::ConnectionType::CONNECTION_3G);
  WaitForConfig();
  ui_test_utils::NavigateToURL(
      browser(), GetURLWithMockHost(origin_server, "/echoheader?Chrome-Proxy"));
  EXPECT_EQ(GetBody(), kDummyBody);

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

  EXPECT_EQ(result, kDummyBody);
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

  EXPECT_EQ(result, kDummyBody);
}

IN_PROC_BROWSER_TEST_F(DataReductionProxyBrowsertest,
                       ProxyNotUsedWhenDisabled) {
  net::EmbeddedTestServer test_server;
  test_server.RegisterRequestHandler(
      base::BindRepeating(&BasicResponse, kDummyBody));
  ASSERT_TRUE(test_server.Start());

  ui_test_utils::NavigateToURL(
      browser(), GetURLWithMockHost(test_server, "/echoheader?Chrome-Proxy"));
  EXPECT_EQ(GetBody(), kDummyBody);

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

// Test that enabling the holdback disables the proxy and that the client config
// is fetched when it is force enabled.
class DataReductionProxyWithHoldbackBrowsertest
    : public ::testing::WithParamInterface<bool>,
      public DataReductionProxyBrowsertest {
 public:
  DataReductionProxyWithHoldbackBrowsertest()
      : enable_config_service_fetches_(GetParam()) {}

  void SetUp() override {
    fetch_client_config_feature_list_.InitWithFeatureState(
        data_reduction_proxy::features::kFetchClientConfig,
        enable_config_service_fetches_);

    InProcessBrowserTest::SetUp();
  }

  const bool enable_config_service_fetches_;

 private:
  base::test::ScopedFeatureList fetch_client_config_feature_list_;
};

IN_PROC_BROWSER_TEST_P(DataReductionProxyWithHoldbackBrowsertest,
                       UpdateConfig) {
  net::EmbeddedTestServer proxy_server;
  proxy_server.RegisterRequestHandler(
      base::BindRepeating(&BasicResponse, kPrimaryProxyResponse));
  ASSERT_TRUE(proxy_server.Start());

  SetConfig(CreateConfigForServer(proxy_server));
  // A network change forces the config to be fetched.
  SimulateNetworkChange(network::mojom::ConnectionType::CONNECTION_3G);

  // Ensure that the client config is fetched when lite page redirect preview is
  // enabled.
  WaitForConfig();

  // Load a webpage in holdback group as well. This ensures that while in
  // holdback group, Chrome does not fetch the client config. If Chrome were to
  // fetch the client config, the DCHECKs and other conditionals that check that
  // holdback is not enabled would trigger and cause the test to fail.
  ui_test_utils::NavigateToURL(browser(), GURL("http://does.not.resolve/foo"));

  EXPECT_NE(GetBody(), kPrimaryProxyResponse);
}

// Parameter is true if data reduction proxy config should always be fetched.
INSTANTIATE_TEST_SUITE_P(All,
                         DataReductionProxyWithHoldbackBrowsertest,
                         ::testing::Bool());

class DataReductionProxyExpBrowsertest : public DataReductionProxyBrowsertest {
 public:
  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        data_reduction_proxy::switches::kDataReductionProxyExperiment,
        "foo_experiment");

    DataReductionProxyBrowsertest::SetUp();
  }
};

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
              EXPECT_EQ(kDummyBody, *response_body);
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
  }

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
  }
}

}  // namespace data_reduction_proxy
