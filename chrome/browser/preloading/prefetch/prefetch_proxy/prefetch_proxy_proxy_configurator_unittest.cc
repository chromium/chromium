// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetch_proxy_proxy_configurator.h"

#include <memory>

#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetch_proxy_features.h"
#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetch_proxy_params.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/google_api_keys.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/host_port_pair.h"
#include "net/base/proxy_server.h"
#include "net/base/proxy_string_util.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_util.h"
#include "net/proxy_resolution/proxy_config.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

class TestCustomProxyConfigClient
    : public network::mojom::CustomProxyConfigClient {
 public:
  explicit TestCustomProxyConfigClient(
      mojo::PendingReceiver<network::mojom::CustomProxyConfigClient>
          pending_receiver)
      : receiver_(this, std::move(pending_receiver)) {}

  // network::mojom::CustomProxyConfigClient:
  void OnCustomProxyConfigUpdated(
      network::mojom::CustomProxyConfigPtr proxy_config,
      OnCustomProxyConfigUpdatedCallback callback) override {
    config_ = std::move(proxy_config);
    std::move(callback).Run();
  }
  void MarkProxiesAsBad(base::TimeDelta bypass_duration,
                        const net::ProxyList& bad_proxies,
                        MarkProxiesAsBadCallback callback) override {}
  void ClearBadProxiesCache() override {}

  network::mojom::CustomProxyConfigPtr config_;

 private:
  mojo::Receiver<network::mojom::CustomProxyConfigClient> receiver_;
};

class PrefetchProxyProxyConfiguratorTest : public testing::Test {
 public:
  PrefetchProxyProxyConfiguratorTest() = default;
  ~PrefetchProxyProxyConfiguratorTest() override = default;

  network::mojom::CustomProxyConfigPtr LatestProxyConfig() {
    return std::move(config_client_->config_);
  }

  void VerifyLatestProxyConfig(const GURL& proxy_url,
                               const net::HttpRequestHeaders& headers) {
    auto config = LatestProxyConfig();
    ASSERT_TRUE(config);

    EXPECT_EQ(config->rules.type,
              net::ProxyConfig::ProxyRules::Type::PROXY_LIST_PER_SCHEME);
    EXPECT_FALSE(config->should_override_existing_config);
    EXPECT_FALSE(config->allow_non_idempotent_methods);

    EXPECT_EQ(config->connect_tunnel_headers.ToString(), headers.ToString());

    EXPECT_EQ(config->rules.proxies_for_http.size(), 0U);
    EXPECT_EQ(config->rules.proxies_for_ftp.size(), 0U);

    ASSERT_EQ(config->rules.proxies_for_https.size(), 1U);
    EXPECT_EQ(
        GURL(net::ProxyServerToProxyUri(config->rules.proxies_for_https.Get())),
        proxy_url);
  }

  PrefetchProxyProxyConfigurator* configurator() {
    if (!configurator_) {
      // Lazy construct and init so that any changed field trials can be used.
      configurator_ = std::make_unique<PrefetchProxyProxyConfigurator>();
      mojo::Remote<network::mojom::CustomProxyConfigClient> client_remote;
      config_client_ = std::make_unique<TestCustomProxyConfigClient>(
          client_remote.BindNewPipeAndPassReceiver());
      base::RunLoop run_loop;
      configurator_->AddCustomProxyConfigClient(std::move(client_remote),
                                                run_loop.QuitClosure());
      configurator_->SetClockForTesting(task_environment_.GetMockClock());
      run_loop.Run();
    }
    return configurator_.get();
  }

  void FastForwardBy(base::TimeDelta delta) {
    task_environment_.FastForwardBy(delta);
  }

 private:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<PrefetchProxyProxyConfigurator> configurator_;
  std::unique_ptr<TestCustomProxyConfigClient> config_client_;
};

TEST_F(PrefetchProxyProxyConfiguratorTest, FeatureOff) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(features::kIsolatePrerenders);

  base::RunLoop loop;
  configurator()->UpdateCustomProxyConfig(loop.QuitClosure());
  loop.Run();

  EXPECT_FALSE(LatestProxyConfig());
}

TEST_F(PrefetchProxyProxyConfiguratorTest, ExperimentOverrides) {
  GURL proxy_url("https://proxy.com");
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kIsolatePrerenders,
      {{"proxy_host", proxy_url.spec()}, {"proxy_header_key", "test-header"}});

  base::RunLoop loop;
  configurator()->UpdateCustomProxyConfig(loop.QuitClosure());
  loop.Run();

  net::HttpRequestHeaders headers;
  headers.SetHeader("test-header", "key=" + google_apis::GetAPIKey());
  VerifyLatestProxyConfig(proxy_url, headers);
}

TEST_F(PrefetchProxyProxyConfiguratorTest,
       Fallback_DoesRandomBackoff_ErrFailed) {
  base::HistogramTester histogram_tester;
  GURL proxy_url("https://proxy.com");
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kIsolatePrerenders, {{"proxy_host", proxy_url.spec()}});

  net::ProxyServer proxy(
      net::GetSchemeFromUriScheme(PrefetchProxyProxyHost().scheme()),
      net::HostPortPair::FromURL(PrefetchProxyProxyHost()));

  EXPECT_TRUE(configurator()->IsPrefetchProxyAvailable());

  configurator()->OnFallback(proxy, net::ERR_FAILED);
  EXPECT_FALSE(configurator()->IsPrefetchProxyAvailable());
  histogram_tester.ExpectUniqueSample("PrefetchProxy.Proxy.Fallback.NetError",
                                      std::abs(net::ERR_FAILED), 1);

  FastForwardBy(base::Seconds(5 * 60 + 1));

  EXPECT_TRUE(configurator()->IsPrefetchProxyAvailable());
}

TEST_F(PrefetchProxyProxyConfiguratorTest, FallbackDoesRandomBackoff_ErrOK) {
  base::HistogramTester histogram_tester;
  GURL proxy_url("https://proxy.com");
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kIsolatePrerenders, {{"proxy_host", proxy_url.spec()}});

  net::ProxyServer proxy(
      net::GetSchemeFromUriScheme(PrefetchProxyProxyHost().scheme()),
      net::HostPortPair::FromURL(PrefetchProxyProxyHost()));

  EXPECT_TRUE(configurator()->IsPrefetchProxyAvailable());

  configurator()->OnFallback(proxy, net::OK);
  EXPECT_FALSE(configurator()->IsPrefetchProxyAvailable());
  histogram_tester.ExpectUniqueSample("PrefetchProxy.Proxy.Fallback.NetError",
                                      net::OK, 1);

  FastForwardBy(base::Seconds(5 * 60 + 1));

  EXPECT_TRUE(configurator()->IsPrefetchProxyAvailable());
}

TEST_F(PrefetchProxyProxyConfiguratorTest, Fallback_DifferentProxy) {
  base::HistogramTester histogram_tester;
  GURL proxy_url("https://proxy.com");
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kIsolatePrerenders, {{"proxy_host", proxy_url.spec()}});

  net::ProxyServer proxy(
      net::GetSchemeFromUriScheme(PrefetchProxyProxyHost().scheme()),
      net::HostPortPair::FromURL(GURL("http://foo.com")));

  EXPECT_TRUE(configurator()->IsPrefetchProxyAvailable());

  configurator()->OnFallback(proxy, net::OK);
  EXPECT_TRUE(configurator()->IsPrefetchProxyAvailable());
  histogram_tester.ExpectTotalCount("PrefetchProxy.Proxy.Fallback.NetError", 0);
}

TEST_F(PrefetchProxyProxyConfiguratorTest, TunnelHeaders_200OK) {
  base::HistogramTester histogram_tester;
  GURL proxy_url("https://proxy.com");
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kIsolatePrerenders, {{"proxy_host", proxy_url.spec()}});

  net::ProxyServer proxy(
      net::GetSchemeFromUriScheme(PrefetchProxyProxyHost().scheme()),
      net::HostPortPair::FromURL(PrefetchProxyProxyHost()));

  EXPECT_TRUE(configurator()->IsPrefetchProxyAvailable());

  configurator()->OnTunnelHeadersReceived(
      proxy, base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK"));
  EXPECT_TRUE(configurator()->IsPrefetchProxyAvailable());
  histogram_tester.ExpectUniqueSample("PrefetchProxy.Proxy.RespCode", 200, 1);
}

TEST_F(PrefetchProxyProxyConfiguratorTest, TunnelHeaders_DifferentProxy) {
  base::HistogramTester histogram_tester;
  GURL proxy_url("https://proxy.com");
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kIsolatePrerenders, {{"proxy_host", proxy_url.spec()}});

  net::ProxyServer proxy(
      net::GetSchemeFromUriScheme(PrefetchProxyProxyHost().scheme()),
      net::HostPortPair::FromURL(GURL("http://foo.com")));

  EXPECT_TRUE(configurator()->IsPrefetchProxyAvailable());

  configurator()->OnTunnelHeadersReceived(
      proxy, base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK"));
  EXPECT_TRUE(configurator()->IsPrefetchProxyAvailable());
  histogram_tester.ExpectTotalCount("PrefetchProxy.Proxy.RespCode", 0);
}

TEST_F(PrefetchProxyProxyConfiguratorTest, TunnelHeaders_500NoRetryAfter) {
  base::HistogramTester histogram_tester;
  GURL proxy_url("https://proxy.com");
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kIsolatePrerenders, {{"proxy_host", proxy_url.spec()}});

  net::ProxyServer proxy(
      net::GetSchemeFromUriScheme(PrefetchProxyProxyHost().scheme()),
      net::HostPortPair::FromURL(PrefetchProxyProxyHost()));

  EXPECT_TRUE(configurator()->IsPrefetchProxyAvailable());

  configurator()->OnTunnelHeadersReceived(
      proxy, base::MakeRefCounted<net::HttpResponseHeaders>(
                 "HTTP/1.1 500 Internal Server Error"));
  EXPECT_FALSE(configurator()->IsPrefetchProxyAvailable());
  histogram_tester.ExpectUniqueSample("PrefetchProxy.Proxy.RespCode", 500, 1);

  FastForwardBy(base::Seconds(5 * 60 + 1));
  EXPECT_TRUE(configurator()->IsPrefetchProxyAvailable());
}

TEST_F(PrefetchProxyProxyConfiguratorTest, TunnelHeaders_500WithRetryAfter) {
  base::HistogramTester histogram_tester;
  GURL proxy_url("https://proxy.com");
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kIsolatePrerenders, {{"proxy_host", proxy_url.spec()}});

  net::ProxyServer proxy(
      net::GetSchemeFromUriScheme(PrefetchProxyProxyHost().scheme()),
      net::HostPortPair::FromURL(PrefetchProxyProxyHost()));

  EXPECT_TRUE(configurator()->IsPrefetchProxyAvailable());

  configurator()->OnTunnelHeadersReceived(
      proxy,
      base::MakeRefCounted<
          net::HttpResponseHeaders>(net::HttpUtil::AssembleRawHeaders(
          "HTTP/1.1 500 Internal Server Error\r\nRetry-After: 120\r\n\r\n")));
  EXPECT_FALSE(configurator()->IsPrefetchProxyAvailable());
  histogram_tester.ExpectUniqueSample("PrefetchProxy.Proxy.RespCode", 500, 1);

  FastForwardBy(base::Seconds(119));
  EXPECT_FALSE(configurator()->IsPrefetchProxyAvailable());

  FastForwardBy(base::Seconds(1));
  EXPECT_TRUE(configurator()->IsPrefetchProxyAvailable());
}

TEST_F(PrefetchProxyProxyConfiguratorTest, ServerExperimentGroup) {
  GURL proxy_url("https://proxy.com");
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kIsolatePrerenders,
      {{"proxy_host", proxy_url.spec()},
       {"proxy_header_key", "test-header"},
       {"server_experiment_group", "test_group"}});

  base::RunLoop loop;
  configurator()->UpdateCustomProxyConfig(loop.QuitClosure());
  loop.Run();

  net::HttpRequestHeaders headers;
  headers.SetHeader("test-header",
                    "key=" + google_apis::GetAPIKey() + ",exp=test_group");
  VerifyLatestProxyConfig(proxy_url, headers);
}
