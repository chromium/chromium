// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/containers/contains.h"
#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_settings_factory.h"
#include "chrome/browser/privacy_sandbox/tracking_protection_onboarding_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tpcd/experiment/eligibility_service.h"
#include "chrome/browser/tpcd/experiment/eligibility_service_factory.h"
#include "chrome/browser/tpcd/experiment/tpcd_experiment_features.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/privacy_sandbox/privacy_sandbox_settings.h"
#include "components/privacy_sandbox/privacy_sandbox_test_util.h"
#include "components/privacy_sandbox/tracking_protection_onboarding.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tpcd::experiment {

// The param indicates whether the user is in in a cohort with 3PCD enabled.
// (True indicates that third-party cookies are blocked.)
// These tests are running with "force_eligible" enabled to be deterministic
// and avoid being flaky.
class EligibilityServiceBrowserTest : public InProcessBrowserTest,
                                      public testing::WithParamInterface<bool> {
 public:
  EligibilityServiceBrowserTest() {
    feature_list_.InitAndEnableFeatureWithParameters(
        features::kCookieDeprecationFacilitatedTesting,
        {{"label", "label_test"},
         {"force_eligible", "true"},
         {"disable_3p_cookies", GetDisable3PCookies()}});
  }

  ~EligibilityServiceBrowserTest() override = default;

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server_.ServeFilesFromSourceDirectory("content/test/data");
  }

 protected:
  std::string GetDisable3PCookies() { return GetParam() ? "true" : "false"; }

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

IN_PROC_BROWSER_TEST_P(EligibilityServiceBrowserTest,
                       EligibilityChanged_NetworkContextUpdated) {
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

  auto* privacy_sandbox_settings =
      PrivacySandboxSettingsFactory::GetForProfile(browser()->profile());
  auto privacy_sandbox_delegate = std::make_unique<
      privacy_sandbox_test_util::MockPrivacySandboxSettingsDelegate>();
  EXPECT_CALL(*privacy_sandbox_delegate, IsCookieDeprecationExperimentEligible)
      .Times(4)
      .WillOnce(testing::Return(false))
      .WillOnce(testing::Return(false))
      .WillOnce(testing::Return(true))
      .WillOnce(testing::Return(true));
  privacy_sandbox_settings->SetDelegateForTesting(
      std::move(privacy_sandbox_delegate));

  ASSERT_FALSE(privacy_sandbox_settings->IsCookieDeprecationLabelAllowed());

  MarkProfileEligibility(/*is_eligible=*/false);

  // Ensures the cookie deprecation label is updated in the network context.
  FlushNetworkInterface();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL("a.test", "/title1.html")));

  AddImageToDocument(https_server_.GetURL("b.test", "/b_a"));

  // [b.test/a] - Non opted-in request should not receive a label header.
  response_b_a->WaitForRequest();
  ASSERT_FALSE(base::Contains(response_b_a->http_request()->headers,
                              "Sec-Cookie-Deprecation"));
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
}

IN_PROC_BROWSER_TEST_P(EligibilityServiceBrowserTest,
                       EligibilityChanged_OnboardingServiceNotified) {
  privacy_sandbox::TrackingProtectionOnboarding* onboarding_service =
      TrackingProtectionOnboardingFactory::GetForProfile(browser()->profile());

  const bool disable_3p_cookies = GetParam();

  EXPECT_EQ(onboarding_service->GetOnboardingStatus(),
            disable_3p_cookies ? privacy_sandbox::TrackingProtectionOnboarding::
                                     OnboardingStatus::kEligible
                               : privacy_sandbox::TrackingProtectionOnboarding::
                                     OnboardingStatus::kIneligible);

  MarkProfileEligibility(/*is_eligible=*/false);

  EXPECT_EQ(onboarding_service->GetOnboardingStatus(),
            privacy_sandbox::TrackingProtectionOnboarding::OnboardingStatus::
                kIneligible);

  MarkProfileEligibility(/*is_eligible=*/true);

  // Onboarding should be marked eligible for the cohort where 3PCD is enabled,
  // but not when 3PCD is disabled.
  EXPECT_EQ(onboarding_service->GetOnboardingStatus(),
            disable_3p_cookies ? privacy_sandbox::TrackingProtectionOnboarding::
                                     OnboardingStatus::kEligible
                               : privacy_sandbox::TrackingProtectionOnboarding::
                                     OnboardingStatus::kIneligible);
}

INSTANTIATE_TEST_SUITE_P(All, EligibilityServiceBrowserTest, testing::Bool());

}  // namespace tpcd::experiment
