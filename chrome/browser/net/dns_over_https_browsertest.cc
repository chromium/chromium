// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/net/secure_dns_config.h"
#include "chrome/browser/net/stub_resolver_config_reader.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/public/dns_over_https_config.h"
#include "net/dns/public/dns_over_https_server_config.h"
#include "net/dns/public/doh_provider_entry.h"
#include "net/dns/public/secure_dns_mode.h"
#include "testing/gtest/include/gtest/gtest-message.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

struct DohParameter {
  DohParameter(std::string provider,
               net::DnsOverHttpsServerConfig server,
               bool valid,
               bool provider_feature_enabled)
      : doh_provider(std::move(provider)),
        server_config(std::move(server)),
        is_valid(valid),
        provider_feature_enabled(provider_feature_enabled) {}

  std::string doh_provider;
  net::DnsOverHttpsServerConfig server_config;
  bool is_valid;
  bool provider_feature_enabled;
};

std::vector<DohParameter> GetDohServerTestCases() {
  std::vector<DohParameter> doh_test_cases;
  for (const auto* entry : net::DohProviderEntry::GetList()) {
    const bool feature_enabled = base::FeatureList::IsEnabled(entry->feature);
    doh_test_cases.emplace_back(entry->provider, entry->doh_server_config,
                                /*valid=*/true, feature_enabled);
  }
  // Negative test-case
  doh_test_cases.emplace_back(
      "NegativeTestExampleCom",
      *net::DnsOverHttpsServerConfig::FromString("https://www.example.com"),
      /*valid=*/false, /*provider_feature_enabled=*/false);
  return doh_test_cases;
}

}  // namespace

class DohBrowserTest : public InProcessBrowserTest,
                       public testing::WithParamInterface<DohParameter> {
 public:
  DohBrowserTest() : test_url_("https://www.google.com") {
    // Allow test to use full host resolver code, instead of the test resolver
    SetAllowNetworkAccessToHostResolutions();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {// {features::kNetworkServiceInProcess, {}}, // Turn on for debugging
         {features::kDnsOverHttps,
          {{"Fallback", "false"},
           {"Templates",
            net::DnsOverHttpsConfig({GetParam().server_config}).ToString()}}}},
        {});
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  const GURL test_url_;
};

IN_PROC_BROWSER_TEST_P(DohBrowserTest, MANUAL_ExternalDohServers) {
  SCOPED_TRACE(testing::Message() << "Provider's base::Feature is enabled: "
                                  << GetParam().provider_feature_enabled);

  SecureDnsConfig secure_dns_config =
      SystemNetworkContextManager::GetStubResolverConfigReader()
          ->GetSecureDnsConfiguration(
              false /* force_check_parental_controls_for_automatic_mode */);
  // Ensure that DoH is enabled in secure mode
  EXPECT_EQ(net::SecureDnsMode::kSecure, secure_dns_config.mode());

  content::TestNavigationObserver nav_observer(
      browser()->tab_strip_model()->GetActiveWebContents(), 1);
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url_));
  nav_observer.WaitForNavigationFinished();
  EXPECT_EQ(GetParam().is_valid, nav_observer.last_navigation_succeeded());
}

INSTANTIATE_TEST_SUITE_P(
    DohBrowserParameterizedTest,
    DohBrowserTest,
    ::testing::ValuesIn(GetDohServerTestCases()),
    [](const testing::TestParamInfo<DohBrowserTest::ParamType>& info) {
      return info.param.doh_provider;
    });
