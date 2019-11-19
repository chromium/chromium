// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings_factory.h"
#include "chrome/browser/data_use_measurement/chrome_data_use_measurement.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config_service_client_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_features.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "components/data_reduction_proxy/proto/client_config.pb.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test_base.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/features.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace data_use_measurement {

namespace {
std::unique_ptr<net::test_server::HttpResponse>
HandleDataReductionProxyWarmupRequest(
    const net::test_server::HttpRequest& request) {
  if (!base::StartsWith(request.relative_url,
                        data_reduction_proxy::params::GetWarmupURL().path(),
                        base::CompareCase::INSENSITIVE_ASCII))
    return std::unique_ptr<net::test_server::HttpResponse>();

  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_content(std::string(1024, ' '));
  http_response->set_code(net::HTTP_OK);
  return std::move(http_response);
}
}  // namespace

// Enables the data saver and checks data use is recorded for the
// services.
class DataUseMeasurementBrowserTestWithDataSaverEnabled
    : public InProcessBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    net::HostPortPair host_port_pair = embedded_test_server()->host_port_pair();
    std::string config = data_reduction_proxy::EncodeConfig(
        CreateConfig("TheSessionKeyYay!", 1000, 0,
                     data_reduction_proxy::ProxyServer_ProxyScheme_HTTP,
                     host_port_pair.host(), host_port_pair.port(),
                     data_reduction_proxy::ProxyServer_ProxyScheme_HTTP,
                     "fallback.net", 80, 0.5f, false));
    command_line->AppendSwitchASCII(
        data_reduction_proxy::switches::kDataReductionProxyServerClientConfig,
        config);
  }

  void SetUp() override {
    embedded_test_server()->RegisterRequestHandler(
        base::BindRepeating(&HandleDataReductionProxyWarmupRequest));
    ASSERT_TRUE(embedded_test_server()->Start());
    scoped_feature_list_.InitAndEnableFeature(
        data_reduction_proxy::features::
            kDataReductionProxyEnabledWithNetworkService);
    InProcessBrowserTest::SetUp();
  }

 protected:
  void EnableDataSaver(bool enabled) {
    data_reduction_proxy::DataReductionProxySettings*
        data_reduction_proxy_settings =
            DataReductionProxyChromeSettingsFactory::GetForBrowserContext(
                browser()->profile());
    data_reduction_proxy_settings->SetDataReductionProxyEnabled(enabled);
  }

  const base::HistogramTester& histogram_tester() { return histogram_tester_; }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::ScopedFeatureList param_feature_list_;
  base::HistogramTester histogram_tester_;
};

// TODO(rajendrant): Fix the test. The test does not work on ChromeOS. The test is flaky in linux.
IN_PROC_BROWSER_TEST_F(DataUseMeasurementBrowserTestWithDataSaverEnabled,
                       DISABLED_CheckServicesDataUseRecorded) {
  EnableDataSaver(true);
  ui_test_utils::NavigateToURL(browser(), GURL("about:blank"));
  size_t data_saver_warmup_usage_kb = 0;
  for (const auto& bucket : histogram_tester().GetAllSamples(
           "DataUse.AllServicesKB.Downstream.Foreground")) {
    if (bucket.min == COMPUTE_NETWORK_TRAFFIC_ANNOTATION_ID_HASH(
                          "data_reduction_proxy_warmup")) {
      data_saver_warmup_usage_kb += bucket.count;
    }
  }
  // Data use is probabilistically rounded as 1-2 KB.
  EXPECT_THAT(data_saver_warmup_usage_kb,
              testing::AllOf(testing::Ge(1U), testing::Le(2U)));
}

}  // namespace data_use_measurement
