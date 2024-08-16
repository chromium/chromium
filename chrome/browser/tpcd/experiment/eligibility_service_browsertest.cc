// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tpcd/experiment/eligibility_service.h"

#include <memory>
#include <string>
#include <utility>

#include "base/containers/contains.h"
#include "base/metrics/histogram_base.h"
#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_settings_factory.h"
#include "chrome/browser/privacy_sandbox/tracking_protection_onboarding_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tpcd/experiment/eligibility_service_factory.h"
#include "chrome/browser/tpcd/experiment/tpcd_experiment_features.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/privacy_sandbox/privacy_sandbox_settings.h"
#include "components/privacy_sandbox/privacy_sandbox_test_util.h"
#include "components/privacy_sandbox/tracking_protection_onboarding.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tpcd::experiment {

namespace {

constexpr char kSecCookieDeprecationHeaderStatus[] =
    "Privacy.3PCD.SecCookieDeprecationHeaderStatus";

}  // namespace

// These tests are running with "force_eligible" enabled to be deterministic
// and avoid being flaky.
class EligibilityServiceBrowserTestBase : public InProcessBrowserTest {
 public:
  EligibilityServiceBrowserTestBase(bool disable_3p_cookies,
                                    bool enable_silent_onboarding) {
    feature_list_.InitAndEnableFeatureWithParameters(
        features::kCookieDeprecationFacilitatedTesting,
        {{"label", "label_test"},
         {"force_eligible", "true"},
         {kDisable3PCookiesName, disable_3p_cookies ? "true" : "false"},
         {kEnableSilentOnboardingName,
          enable_silent_onboarding ? "true" : "false"}});
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server_.ServeFilesFromSourceDirectory("content/test/data");
  }

 protected:
  void AddImageToDocument(const GURL& src_url) {
    ASSERT_EQ(true,
              EvalJs(GetActiveWebContents(),
                     base::StrCat({"((() => { const img = "
                                   "document.createElement('img'); img.src = '",
                                   src_url.spec(), "'; return true; })())"})));
  }

  void FlushNetworkInterface() {
    browser()
        ->profile()
        ->GetDefaultStoragePartition()
        ->FlushNetworkInterfaceForTesting();
  }

  void MarkProfileEligibility(bool is_eligible) {
    auto* eligibility_service =
        tpcd::experiment::EligibilityServiceFactory::GetForProfile(
            browser()->profile());
    eligibility_service->MarkProfileEligibility(is_eligible);
  }

  net::EmbeddedTestServer https_server_{
      net::test_server::EmbeddedTestServer::TYPE_HTTPS};

 private:
  content::WebContents* GetActiveWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  base::test::ScopedFeatureList feature_list_;
};

// The param indicates whether the user is in in a cohort with 3PCD enabled.
// (True indicates that third-party cookies are blocked.)
class EligibilityServiceBrowserTest : public EligibilityServiceBrowserTestBase,
                                      public testing::WithParamInterface<bool> {
 public:
  EligibilityServiceBrowserTest()
      : EligibilityServiceBrowserTestBase(/*disable_3p_cookies=*/GetParam(),
                                          /*enable_silent_onboarding=*/false) {}
};

IN_PROC_BROWSER_TEST_P(EligibilityServiceBrowserTest,
                       EligibilityChanged_NetworkContextUpdated) {
  base::HistogramTester histograms;

  auto response_b_a =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          &https_server_, "/b_a");
  auto response_b_b =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          &https_server_, "/b_b");
  auto response_b_c =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          &https_server_, "/b_c");
  ASSERT_TRUE(https_server_.Start());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL("a.test", "/title1.html")));

  auto* privacy_sandbox_settings =
      PrivacySandboxSettingsFactory::GetForProfile(browser()->profile());
  auto privacy_sandbox_delegate = std::make_unique<
      privacy_sandbox_test_util::MockPrivacySandboxSettingsDelegate>();
  EXPECT_CALL(*privacy_sandbox_delegate, IsCookieDeprecationLabelAllowed)
      .WillOnce(testing::Return(false))
      .WillOnce(testing::Return(false))
      .WillOnce(testing::Return(true))
      .WillOnce(testing::Return(true));
  privacy_sandbox_settings->SetDelegateForTesting(
      std::move(privacy_sandbox_delegate));

  ASSERT_FALSE(privacy_sandbox_settings->IsCookieDeprecationLabelAllowed());

  // `is_eligible` only affects onboarding and is irrelevant to this test.
  MarkProfileEligibility(/*is_eligible=*/true);

  // Ensures the cookie deprecation label is updated in the network context.
  FlushNetworkInterface();

  AddImageToDocument(https_server_.GetURL("b.test", "/b_a"));

  // [b.test/a] - Non opted-in request should not receive a label header.
  response_b_a->WaitForRequest();
  ASSERT_FALSE(base::Contains(response_b_a->http_request()->headers,
                              "Sec-Cookie-Deprecation"));

  // kSet = 0, kNoLabel = 1
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  const auto no_label_count =
      histograms.GetBucketCount(kSecCookieDeprecationHeaderStatus, 1);
  EXPECT_GT(no_label_count, 0);
  histograms.ExpectBucketCount(kSecCookieDeprecationHeaderStatus, 0, 0);

  auto http_response_b_a =
      std::make_unique<net::test_server::BasicHttpResponse>();
  http_response_b_a->set_code(net::HTTP_MOVED_PERMANENTLY);
  http_response_b_a->AddCustomHeader(
      "Location", https_server_.GetURL("b.test", "/b_b").spec());
  // b.test opts in to receiving the label.
  http_response_b_a->AddCustomHeader(
      "Set-Cookie",
      "receive-cookie-deprecation=any-value; Secure; HttpOnly; "
      "Path=/; SameSite=None; Partitioned");
  response_b_a->Send(http_response_b_a->ToResponseString());
  response_b_a->Done();

  // [b.test/b] - Opted-in request should not receive a label header if
  // disallowed.
  response_b_b->WaitForRequest();
  ASSERT_FALSE(base::Contains(response_b_b->http_request()->headers,
                              "Sec-Cookie-Deprecation"));

  ASSERT_TRUE(privacy_sandbox_settings->IsCookieDeprecationLabelAllowed());

  // kSet = 0, kNoLabel = 1
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  histograms.ExpectBucketCount(kSecCookieDeprecationHeaderStatus, 0, 0);
  EXPECT_GT(histograms.GetBucketCount(kSecCookieDeprecationHeaderStatus, 1),
            no_label_count);

  MarkProfileEligibility(/*is_eligible=*/true);

  // Ensures the cookie deprecation label is updated in the network context.
  FlushNetworkInterface();

  AddImageToDocument(https_server_.GetURL("b.test", "/b_c"));

  // [b.test/c] - Opted-in request should receive a label header if allowed.
  response_b_c->WaitForRequest();
  ASSERT_TRUE(base::Contains(response_b_c->http_request()->headers,
                             "Sec-Cookie-Deprecation"));
  EXPECT_EQ(response_b_c->http_request()->headers.at("Sec-Cookie-Deprecation"),
            "label_test");

  // kSet = 0
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  histograms.ExpectBucketCount(kSecCookieDeprecationHeaderStatus, 0, 1);
}

INSTANTIATE_TEST_SUITE_P(All, EligibilityServiceBrowserTest, testing::Bool());

}  // namespace tpcd::experiment
