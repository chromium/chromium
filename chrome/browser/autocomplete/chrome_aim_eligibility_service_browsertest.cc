// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/chrome_aim_eligibility_service.h"

#include <algorithm>
#include <tuple>
#include <vector>

#include "base/functional/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/scoped_browser_locale.h"
#include "chrome/test/base/search_test_utils.h"
#include "components/omnibox/browser/aim_eligibility_service.h"
#include "components/omnibox/browser/aim_eligibility_service_observer.h"
#include "components/omnibox/browser/omnibox_prefs.h"
#include "components/prefs/pref_service.h"
#include "components/search/search.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/variations/service/variations_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/url_loader_interceptor.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/omnibox_proto/aim_eligibility_response.pb.h"

// Helper function to provide eligibility response for intercepted requests.
bool OnRequest(omnibox::AimEligibilityResponse response,
               content::URLLoaderInterceptor::RequestParams* params) {
  const GURL& url = params->url_request.url;

  if (!url.DomainIs("google.com") || url.path() != "/async/folae" ||
      url.query() != "async=_fmt:pb") {
    return false;
  }

  std::string response_string;
  response.SerializeToString(&response_string);

  content::URLLoaderInterceptor::WriteResponse(
      "HTTP/1.1 200 OK\nContent-Type: application/x-protobuf\n\n",
      response_string, params->client.get());
  return true;
}

// Friend class to access private members of AimEligibilityService for testing.
class AimEligibilityServiceFriend {
 public:
  using ServerRequestStatus = AimEligibilityService::ServerRequestStatus;
};

class ChromeAimEligibilityServiceBrowserTest
    : public InProcessBrowserTest,
      public AimEligibilityServiceObserver,
      public ::testing::WithParamInterface<
          std::tuple<std::string, std::string, bool, bool, bool, bool>> {
 public:
  ChromeAimEligibilityServiceBrowserTest() = default;
  ~ChromeAimEligibilityServiceBrowserTest() override = default;

  // AimEligibilityServiceObserver:
  void OnAimEligibilityChanged() override {
    eligibility_changed_future_.SetValue();
  }

 protected:
  void SetUp() override {
    auto [locale, country, server_eligibility_enabled, allowed_by_policy,
          is_google_dse, is_server_eligible] = GetParam();

    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;

    if (server_eligibility_enabled) {
      enabled_features.push_back(kAimServerEligibilityEnabled);
    } else {
      disabled_features.push_back(kAimServerEligibilityEnabled);
    }

    feature_list_.InitWithFeatures(enabled_features, disabled_features);

    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    auto [locale, country, server_eligibility_enabled, allowed_by_policy,
          is_google_dse, is_server_eligible] = GetParam();

    // Set up locale and country.
    scoped_browser_locale_ = std::make_unique<ScopedBrowserLocale>(locale);
    g_browser_process->variations_service()->OverrideStoredPermanentCountry(
        country);

    // Set up the AIM policy pref; 0 = allowed, 1 = disallowed.
    browser()->profile()->GetPrefs()->SetInteger(omnibox::kAIModeSettings,
                                                 allowed_by_policy ? 0 : 1);

    // Set up the default search engine.
    TemplateURLService* template_url_service =
        TemplateURLServiceFactory::GetForProfile(browser()->profile());
    search_test_utils::WaitForTemplateURLServiceToLoad(template_url_service);
    TemplateURLData template_url_data;
    if (is_google_dse) {
      template_url_data.SetShortName(u"Google");
      template_url_data.SetKeyword(u"google.com");
      template_url_data.SetURL("https://www.google.com/search?q={searchTerms}");
    } else {
      template_url_data.SetShortName(u"Bing");
      template_url_data.SetKeyword(u"bing.com");
      template_url_data.SetURL("https://www.bing.com/search?q={searchTerms}");
    }
    auto template_url = std::make_unique<TemplateURL>(template_url_data);
    auto* template_url_ptr = template_url_service->Add(std::move(template_url));
    template_url_service->SetUserSelectedDefaultSearchProvider(
        template_url_ptr);

    AimEligibilityServiceFactory::GetInstance()->SetTestingFactory(
        browser()->profile(),
        base::BindOnce(AimEligibilityServiceFactory::GetDefaultFactory()));

    InProcessBrowserTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    scoped_browser_locale_.reset();

    InProcessBrowserTest::TearDownOnMainThread();
  }

  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<ScopedBrowserLocale> scoped_browser_locale_;
  base::test::TestFuture<void> eligibility_changed_future_;
};

INSTANTIATE_TEST_SUITE_P(,
                         ChromeAimEligibilityServiceBrowserTest,
                         ::testing::Combine(
                             // Values for the locale.
                             ::testing::Values("en-US", "es-MX"),
                             // Values for the country.
                             ::testing::Values("us", "ca"),
                             // Values for server eligibility enabled.
                             ::testing::Values(true, false),
                             // Values for allowed by policy.
                             ::testing::Values(true, false),
                             // Values for Google DSE.
                             ::testing::Values(true, false),
                             // Values for server response eligibility.
                             ::testing::Values(true, false)));

IN_PROC_BROWSER_TEST_P(ChromeAimEligibilityServiceBrowserTest,
                       ComprehensiveEligibilityTest) {
  auto [locale, country, server_eligibility_enabled, allowed_by_policy,
        is_google_dse, is_server_eligible] = GetParam();

  omnibox::AimEligibilityResponse response;
  response.set_is_eligible(is_server_eligible);
  content::URLLoaderInterceptor url_loader_interceptor(
      base::BindRepeating(&OnRequest, response));

  base::HistogramTester histogram_tester;

  auto* service =
      AimEligibilityServiceFactory::GetForProfile(browser()->profile());
  service->AddObserver(this);

  if (is_google_dse) {
    // Wait for the observer to be notified of potential eligibility changes.
    EXPECT_TRUE(eligibility_changed_future_.Wait());
    // Verify histograms were recorded.
    histogram_tester.ExpectTotalCount(
        "Omnibox.AimEligibility.ServerRequestStatus", 2);
    histogram_tester.ExpectBucketCount(
        "Omnibox.AimEligibility.ServerRequestStatus",
        AimEligibilityServiceFriend::ServerRequestStatus::kSent, 1);
    histogram_tester.ExpectBucketCount(
        "Omnibox.AimEligibility.ServerRequestStatus",
        AimEligibilityServiceFriend::ServerRequestStatus::kSuccess, 1);
    histogram_tester.ExpectTotalCount(
        "Omnibox.AimEligibility.ServerEligibility.is_eligible", 1);
    histogram_tester.ExpectUniqueSample(
        "Omnibox.AimEligibility.ServerEligibility.is_eligible",
        is_server_eligible, 1);
  } else {
    // Verify no histogram were recorded.
    histogram_tester.ExpectTotalCount(
        "Omnibox.AimEligibility.ServerRequestStatus", 0);
    histogram_tester.ExpectTotalCount(
        "Omnibox.AimEligibility.ServerEligibility.is_eligible", 0);
  }

  // Test country and locale detection.
  EXPECT_TRUE(service->IsCountry(country));
  EXPECT_TRUE(service->IsLanguage(locale.substr(0, 2)));

  // Test IsServerEligibilityEnabled.
  EXPECT_EQ(service->IsServerEligibilityEnabled(), server_eligibility_enabled);

  // Test IsAimLocallyEligible.
  bool expected_local_eligibility = is_google_dse && allowed_by_policy;
  EXPECT_EQ(service->IsAimLocallyEligible(), expected_local_eligibility);

  // Test IsAimEligible.
  bool expected_enabled = expected_local_eligibility &&
                          (!server_eligibility_enabled || is_server_eligible);
  EXPECT_EQ(service->IsAimEligible(), expected_enabled);
}
