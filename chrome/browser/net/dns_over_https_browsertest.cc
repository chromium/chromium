// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
#include "net/dns/public/doh_provider_entry.h"
#include "net/dns/public/secure_dns_mode.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

struct DohParameter {
  DohParameter(std::string provider, std::string template_uri, bool valid)
      : doh_provider(std::move(provider)),
        doh_template(std::move(template_uri)),
        is_valid(valid) {}

  std::string doh_provider;
  std::string doh_template;
  bool is_valid;
};

std::vector<DohParameter> GetDohServerTestCases() {
  std::vector<DohParameter> doh_test_cases;
  for (const auto* entry : net::DohProviderEntry::GetList()) {
    doh_test_cases.emplace_back(entry->provider, entry->dns_over_https_template,
                                true);
  }
  // Negative test-case
  doh_test_cases.emplace_back("NegativeTestExampleCom",
                              "https://www.example.com", false);
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
          {{"Fallback", "false"}, {"Templates", GetParam().doh_template}}}},
        {});
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  const GURL test_url_;
};

IN_PROC_BROWSER_TEST_P(DohBrowserTest, MANUAL_ExternalDohServers) {
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
