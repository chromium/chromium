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
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_service.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_util.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_features.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_pref_names.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "components/data_reduction_proxy/core/common/uma_util.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/page_load_metrics/browser/page_load_metrics_test_waiter.h"
#include "components/prefs/pref_service.h"
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

constexpr char kMockHost[] = "mock.host";
constexpr char kDummyBody[] = "dummy";

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

}  // namespace

#if defined(OS_WIN) || defined(OS_MAC) || BUILDFLAG(IS_CHROMEOS_ASH)
#define DISABLE_ON_WIN_MAC_CHROMEOS(x) DISABLED_##x
#else
#define DISABLE_ON_WIN_MAC_CHROMEOS(x) x
#endif

class DataReductionProxyBrowsertestBase : public InProcessBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(
        network::switches::kForceEffectiveConnectionType, "4G");
  }

  void SetUp() override { InProcessBrowserTest::SetUp(); }

  void SetUpOnMainThread() override {
    // Make sure the favicon doesn't mess with the tests.
    favicon_catcher_ =
        std::make_unique<net::test_server::ControllableHttpResponse>(
            embedded_test_server(), "/favicon.ico");
    ASSERT_TRUE(embedded_test_server()->Start());

    host_resolver()->AddRule(kMockHost, "127.0.0.1");

    EnableDataSaver(true);
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


  std::string expect_exp_value_in_request_header_;

  size_t count_proxy_server_requests_received_ = 0u;

  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  std::unique_ptr<net::test_server::ControllableHttpResponse> favicon_catcher_;
};

class DataReductionProxyBrowsertest : public DataReductionProxyBrowsertestBase {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    DataReductionProxyBrowsertestBase::SetUpCommandLine(command_line);
  }
};

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


// Verify that requests initiated by SimpleURLLoader use the proxy only if
// render frame ID is set.
IN_PROC_BROWSER_TEST_F(DataReductionProxyBrowsertest, SimpleURLLoader) {
  net::EmbeddedTestServer origin_server;
  origin_server.RegisterRequestHandler(
      base::BindRepeating(&BasicResponse, kDummyBody));
  ASSERT_TRUE(origin_server.Start());

  net::EmbeddedTestServer proxy_server;
  ASSERT_TRUE(proxy_server.Start());

  // A network change forces the config to be fetched.
  SimulateNetworkChange(network::mojom::ConnectionType::CONNECTION_3G);

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url =
      GetURLWithMockHost(origin_server, "/echoheader?Chrome-Proxy");

  auto simple_loader = network::SimpleURLLoader::Create(
      std::move(resource_request), TRAFFIC_ANNOTATION_FOR_TESTS);

  auto* storage_partition =
      content::BrowserContext::GetDefaultStoragePartition(browser()->profile());
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



}  // namespace data_reduction_proxy
