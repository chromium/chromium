// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/chrome_aim_eligibility_service.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

#include "base/callback_list.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/signin/chrome_signin_client_test_util.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/scoped_browser_locale.h"
#include "chrome/test/base/search_test_utils.h"
#include "components/omnibox/browser/aim_eligibility_service.h"
#include "components/omnibox/browser/omnibox_prefs.h"
#include "components/prefs/pref_service.h"
#include "components/search/search.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/variations/service/variations_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "net/base/mock_network_change_notifier.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/omnibox_proto/aim_eligibility_response.pb.h"

// Helper function to provide eligibility response for intercepted requests.
// This function can also simulate network failures by using the optional
// `request_counter` and `max_failures` parameters.
bool OnRequest(content::URLLoaderInterceptor::RequestParams* params,
               std::optional<omnibox::AimEligibilityResponse> response,
               base::RepeatingCallback<void(bool)> requested_handled_callback,
               std::optional<size_t> session_index = std::nullopt,
               int* request_counter = nullptr,
               int max_failures = 0) {
  const GURL& url = params->url_request.url;

  if (!url.DomainIs("google.com") || url.GetPath() != "/async/folae" ||
      url.query().find("async=_fmt:pb") == std::string::npos ||
      (session_index &&
       url.query().find("authuser=" + base::NumberToString(*session_index)) ==
           std::string::npos)) {
    return false;
  }

  if (request_counter && *request_counter < max_failures) {
    (*request_counter)++;
    params->client->OnComplete(
        network::URLLoaderCompletionStatus(net::ERR_NAME_NOT_RESOLVED));
    return true;
  }

  CHECK(response.has_value());
  std::string response_string;
  response->SerializeToString(&response_string);
  content::URLLoaderInterceptor::WriteResponse(
      "HTTP/1.1 200 OK\nContent-Type: application/x-protobuf\n\n",
      response_string, params->client.get());

  requested_handled_callback.Run(true);
  return true;
}

// Helper function to set up the default search engine.
void SetUpDefaultSearchEngine(Profile* profile, bool is_google_dse) {
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile);
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
  template_url_service->SetUserSelectedDefaultSearchProvider(template_url_ptr);
}

// Helper class to observe IdentityManager.
class IdentityManagerObserverHelper : public signin::IdentityManager::Observer {
 public:
  explicit IdentityManagerObserverHelper(
      signin::IdentityManager* identity_manager) {
    identity_manager_observation_.Observe(identity_manager);
  }

  // signin::IdentityManager::Observer:
  void OnAccountsInCookieUpdated(
      const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
      const GoogleServiceAuthError& error) override {
    if (!accounts_updated_future_.IsReady()) {
      accounts_updated_future_.SetValue();
    }
  }

  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event) override {
    if (!primary_account_changed_future_.IsReady()) {
      primary_account_changed_future_.SetValue();
    }
  }

  bool WaitForAccountsInCookieUpdated() {
    return accounts_updated_future_.WaitAndClear();
  }

  bool WaitForPrimaryAccountChanged() {
    return primary_account_changed_future_.WaitAndClear();
  }

 private:
  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observation_{this};
  base::test::TestFuture<void> accounts_updated_future_;
  base::test::TestFuture<void> primary_account_changed_future_;
};

// Friend class to access private members of AimEligibilityService for testing.
class AimEligibilityServiceFriend {
 public:
  using EligibilityRequestStatus =
      AimEligibilityService::EligibilityRequestStatus;
  using EligibilityResponseSource =
      AimEligibilityService::EligibilityResponseSource;
  using RequestSource = AimEligibilityService::RequestSource;

  void ProcessServerEligibilityResponse(
      AimEligibilityService* service,
      RequestSource request_source,
      int response_code,
      bool was_fetched_via_cache,
      int num_retries,
      std::optional<std::string> response_string) {
    service->ProcessServerEligibilityResponse(
        request_source, response_code, was_fetched_via_cache, num_retries,
        std::move(response_string));
  }
};

class ChromeAimEligibilityServiceBrowserTest
    : public InProcessBrowserTest,
      public ::testing::WithParamInterface<
          std::tuple<std::string, std::string, bool, bool, bool, bool, bool>> {
 public:
  ChromeAimEligibilityServiceBrowserTest() = default;
  ~ChromeAimEligibilityServiceBrowserTest() override = default;

  signin::IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_adaptor_->identity_test_env();
  }

  network::TestURLLoaderFactory* test_url_loader_factory() {
    return signin_client_with_url_loader_helper_.test_url_loader_factory();
  }

 protected:
  void SetUp() override {
    auto [locale, country, server_eligibility_enabled, allowed_by_policy,
          is_google_dse, is_server_eligible, is_pdf_upload_eligible] =
        GetParam();

    std::vector<base::test::FeatureRefAndParams> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;

    // Needed for bots with field trial testing configs explicitly disabled.
    enabled_features.push_back(
        {omnibox::kAimServerEligibilityChangedNotification, {}});
    enabled_features.push_back(
        {omnibox::kAimServerEligibilityForPrimaryAccountEnabled, {}});
    enabled_features.push_back(
        {omnibox::kAimServerRequestOnStartupEnabled, {}});
    enabled_features.push_back(
        {omnibox::kAimServerRequestOnIdentityChangeEnabled,
         {{"request_on_cookie_jar_changes", "true"},
          {"request_on_primary_account_changes", "false"}}});
    disabled_features.push_back(
        omnibox::kAimStartupRequestDelayedUntilNetworkAvailableEnabled);

    if (!server_eligibility_enabled) {
      disabled_features.push_back(omnibox::kAimServerEligibilityEnabled);
    }

    feature_list_.InitWithFeaturesAndParameters(enabled_features,
                                                disabled_features);

    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    auto [locale, country, server_eligibility_enabled, allowed_by_policy,
          is_google_dse, is_server_eligible, is_pdf_upload_eligible] =
        GetParam();

    // Set up locale and country.
    scoped_browser_locale_ = std::make_unique<ScopedBrowserLocale>(locale);
    g_browser_process->variations_service()->OverrideStoredPermanentCountry(
        country);

    // Set up the AIM policy pref; 0 = allowed, 1 = disallowed.
    browser()->profile()->GetPrefs()->SetInteger(omnibox::kAIModeSettings,
                                                 allowed_by_policy ? 0 : 1);

    SetUpDefaultSearchEngine(browser()->profile(), is_google_dse);

    // Set the adaptor that supports signin::IdentityTestEnvironment.
    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(
            browser()->profile());

    // Set the testing factory for AimEligibilityService.
    AimEligibilityServiceFactory::GetInstance()->SetTestingFactory(
        browser()->profile(),
        base::BindOnce(AimEligibilityServiceFactory::GetDefaultFactory()));

    InProcessBrowserTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    scoped_browser_locale_.reset();

    InProcessBrowserTest::TearDownOnMainThread();
  }

  void SetUpInProcessBrowserTestFixture() override {
    signin_client_with_url_loader_helper_.SetUp();
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::BindRepeating(&ChromeAimEligibilityServiceBrowserTest::
                                        OnWillCreateBrowserContextServices));
  }

  static void OnWillCreateBrowserContextServices(
      content::BrowserContext* context) {
    // Set up IdentityTestEnvironment.
    IdentityTestEnvironmentProfileAdaptor::
        SetIdentityTestEnvironmentFactoriesOnBrowserContext(context);
  }

  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<ScopedBrowserLocale> scoped_browser_locale_;
  ChromeSigninClientWithURLLoaderHelper signin_client_with_url_loader_helper_;
  base::CallbackListSubscription create_services_subscription_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
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
                             ::testing::Values(true, false),
                             // Values for Pdf server response eligibility.
                             ::testing::Values(true, false)));

IN_PROC_BROWSER_TEST_P(ChromeAimEligibilityServiceBrowserTest,
                       ComprehensiveEligibilityTest) {
  auto [locale, country, server_eligibility_enabled, allowed_by_policy,
        is_google_dse, is_server_eligible, is_pdf_upload_eligible] = GetParam();

  // Handle the eligibility request on startup with a custom response.
  omnibox::AimEligibilityResponse response;
  response.set_is_eligible(is_server_eligible);
  response.set_is_pdf_upload_eligible(is_pdf_upload_eligible);
  base::test::TestFuture<bool> request_handled_future;
  auto url_loader_interceptor = std::make_unique<content::URLLoaderInterceptor>(
      base::BindLambdaForTesting(
          [&](content::URLLoaderInterceptor::RequestParams* params) {
            return OnRequest(params, std::make_optional(response),
                             request_handled_future.GetRepeatingCallback());
          }));

  // Test service startup.
  {
    base::HistogramTester histogram_tester;

    auto* service =
        AimEligibilityServiceFactory::GetForProfile(browser()->profile());
    base::test::TestFuture<void> eligibility_changed_future;
    auto eligibility_subscription = service->RegisterEligibilityChangedCallback(
        eligibility_changed_future.GetRepeatingCallback());

    // Test country and locale detection.
    EXPECT_TRUE(service->IsCountry(country));
    EXPECT_TRUE(service->IsLanguage(locale.substr(0, 2)));

    // Test IsServerEligibilityEnabled().
    EXPECT_EQ(service->IsServerEligibilityEnabled(),
              server_eligibility_enabled);

    // Wait for the eligibility change callback to be invoked, if applicable.
    if (is_google_dse) {
      EXPECT_TRUE(request_handled_future.Take());
      EXPECT_TRUE(eligibility_changed_future.Wait());
    } else {
      EXPECT_FALSE(request_handled_future.IsReady());
      EXPECT_FALSE(eligibility_changed_future.IsReady());
    }

    // Test IsAimLocallyEligible().
    bool expected_local_eligibility = is_google_dse && allowed_by_policy;
    EXPECT_EQ(service->IsAimLocallyEligible(), expected_local_eligibility);

    // Test IsAimEligible().
    bool expected_eligible =
        expected_local_eligibility &&
        (!server_eligibility_enabled || is_server_eligible);
    EXPECT_EQ(service->IsAimEligible(), expected_eligible);

    // Test IsPdfUploadEligible().
    bool expected_pdf_upload_eligible =
        expected_eligible &&
        (!server_eligibility_enabled || is_pdf_upload_eligible);
    EXPECT_EQ(service->IsPdfUploadEligible(), expected_pdf_upload_eligible);

    // Verify histograms for the request on startup.
    if (is_google_dse) {
      // Startup sliced histograms.
      histogram_tester.ExpectTotalCount(
          "Omnibox.AimEligibility.EligibilityRequestPrimaryAccountExists."
          "Startup",
          1);
      histogram_tester.ExpectUniqueSample(
          "Omnibox.AimEligibility.EligibilityRequestPrimaryAccountExists."
          "Startup",
          false, 1);
      histogram_tester.ExpectTotalCount(
          "Omnibox.AimEligibility.EligibilityRequestPrimaryAccountInCookieJar."
          "Startup",
          0);
      histogram_tester.ExpectTotalCount(
          "Omnibox.AimEligibility.EligibilityRequestPrimaryAccountIndex."
          "Startup",
          0);

      histogram_tester.ExpectTotalCount(
          "Omnibox.AimEligibility.EligibilityRequestStatus.Startup", 2);
      histogram_tester.ExpectBucketCount(
          "Omnibox.AimEligibility.EligibilityRequestStatus.Startup",
          AimEligibilityServiceFriend::EligibilityRequestStatus::kSent, 1);
      histogram_tester.ExpectBucketCount(
          "Omnibox.AimEligibility.EligibilityRequestStatus.Startup",
          AimEligibilityServiceFriend::EligibilityRequestStatus::kSuccess, 1);

      histogram_tester.ExpectTotalCount(
          "Omnibox.AimEligibility.EligibilityResponseCode.Startup", 1);
      histogram_tester.ExpectUniqueSample(
          "Omnibox.AimEligibility.EligibilityResponseCode.Startup", 200, 1);

      histogram_tester.ExpectUniqueSample(
          "Omnibox.AimEligibility.EligibilityResponse.Startup.is_eligible",
          is_server_eligible, 1);
      histogram_tester.ExpectUniqueSample(
          "Omnibox.AimEligibility.EligibilityResponse.Startup.is_pdf_upload_"
          "eligible",
          is_pdf_upload_eligible, 1);

      // Unsliced histograms.
      histogram_tester.ExpectTotalCount(
          "Omnibox.AimEligibility.EligibilityRequestPrimaryAccountExists", 1);
      histogram_tester.ExpectUniqueSample(
          "Omnibox.AimEligibility.EligibilityRequestPrimaryAccountExists",
          false, 1);
      histogram_tester.ExpectTotalCount(
          "Omnibox.AimEligibility.EligibilityRequestPrimaryAccountInCookieJar",
          0);
      histogram_tester.ExpectTotalCount(
          "Omnibox.AimEligibility.EligibilityRequestPrimaryAccountIndex", 0);

      histogram_tester.ExpectTotalCount(
          "Omnibox.AimEligibility.EligibilityRequestStatus", 2);
      histogram_tester.ExpectBucketCount(
          "Omnibox.AimEligibility.EligibilityRequestStatus",
          AimEligibilityServiceFriend::EligibilityRequestStatus::kSent, 1);
      histogram_tester.ExpectBucketCount(
          "Omnibox.AimEligibility.EligibilityRequestStatus",
          AimEligibilityServiceFriend::EligibilityRequestStatus::kSuccess, 1);

      histogram_tester.ExpectTotalCount(
          "Omnibox.AimEligibility.EligibilityResponseCode", 1);
      histogram_tester.ExpectUniqueSample(
          "Omnibox.AimEligibility.EligibilityResponseCode", 200, 1);

      histogram_tester.ExpectUniqueSample(
          "Omnibox.AimEligibility.EligibilityResponse.is_eligible",
          is_server_eligible, 1);
      histogram_tester.ExpectUniqueSample(
          "Omnibox.AimEligibility.EligibilityResponse.is_pdf_upload_eligible",
          is_pdf_upload_eligible, 1);

      // Response change histograms.
      histogram_tester.ExpectUniqueSample(
          "Omnibox.AimEligibility.EligibilityResponseChange.is_eligible",
          is_server_eligible, 1);
      histogram_tester.ExpectUniqueSample(
          "Omnibox.AimEligibility.EligibilityResponseChange.is_pdf_upload_"
          "eligible",
          is_pdf_upload_eligible, 1);

    } else {
      histogram_tester.ExpectTotalCount(
          "Omnibox.AimEligibility.EligibilityRequestStatus.Startup", 0);
      histogram_tester.ExpectTotalCount(
          "Omnibox.AimEligibility.EligibilityResponseCode.Startup", 0);
    }
  }

  url_loader_interceptor.reset();

  // Test changes to the accounts in the cookie jar.
  {
    base::HistogramTester histogram_tester;

    // Handle the eligibility request with a custom response.
    response.set_is_eligible(!is_server_eligible);
    response.set_is_pdf_upload_eligible(!is_pdf_upload_eligible);
    url_loader_interceptor = std::make_unique<content::URLLoaderInterceptor>(
        base::BindLambdaForTesting(
            [&](content::URLLoaderInterceptor::RequestParams* params) {
              return OnRequest(params, std::make_optional(response),
                               request_handled_future.GetRepeatingCallback(),
                               /*session_index=*/1);
            }));

    auto* service =
        AimEligibilityServiceFactory::GetForProfile(browser()->profile());
    base::test::TestFuture<void> eligibility_changed_future;
    auto eligibility_subscription = service->RegisterEligibilityChangedCallback(
        eligibility_changed_future.GetRepeatingCallback());

    // Simulate a change to the accounts in the cookie jar and primary account.
    auto* identity_manager = identity_test_env()->identity_manager();
    IdentityManagerObserverHelper identity_observer(identity_manager);

    AccountInfo primary_account_info = signin::MakeAccountAvailable(
        identity_manager,
        signin::AccountAvailabilityOptionsBuilder(test_url_loader_factory())
            .AsPrimary(signin::ConsentLevel::kSignin)
            .Build("primary@email.com"));
    AccountInfo secondary_account_info = signin::MakeAccountAvailable(
        identity_manager,
        signin::AccountAvailabilityOptionsBuilder(test_url_loader_factory())
            .Build("secondary@email.com"));
    signin::SetCookieAccounts(
        identity_manager, test_url_loader_factory(),
        {{secondary_account_info.email, secondary_account_info.gaia},
         {primary_account_info.email, primary_account_info.gaia}});
    EXPECT_TRUE(identity_observer.WaitForAccountsInCookieUpdated());
    EXPECT_TRUE(identity_observer.WaitForPrimaryAccountChanged());

    // Wait for the eligibility change callback to be invoked, if applicable.
    if (is_google_dse) {
      EXPECT_TRUE(request_handled_future.Take());
      EXPECT_TRUE(eligibility_changed_future.Wait());
    } else {
      EXPECT_FALSE(request_handled_future.IsReady());
      EXPECT_FALSE(eligibility_changed_future.IsReady());
    }

    // Test IsAimLocallyEligible().
    bool expected_local_eligibility = is_google_dse && allowed_by_policy;
    EXPECT_EQ(service->IsAimLocallyEligible(), expected_local_eligibility);

    // Test IsAimEligible().
    bool expected_eligible =
        expected_local_eligibility &&
        (!server_eligibility_enabled || !is_server_eligible);
    EXPECT_EQ(service->IsAimEligible(), expected_eligible);

    // Test IsPdfUploadEligible().
    bool expected_pdf_upload_eligible =
        expected_eligible &&
        (!server_eligibility_enabled || !is_pdf_upload_eligible);
    EXPECT_EQ(service->IsPdfUploadEligible(), expected_pdf_upload_eligible);
    // Verify histograms.
    if (is_google_dse) {
      // CookieChange sliced histograms.
      histogram_tester.ExpectTotalCount(
          "Omnibox.AimEligibility.EligibilityRequestPrimaryAccountExists."
          "CookieChange",
          1);
      histogram_tester.ExpectUniqueSample(
          "Omnibox.AimEligibility.EligibilityRequestPrimaryAccountExists."
          "CookieChange",
          true, 1);
      histogram_tester.ExpectTotalCount(
          "Omnibox.AimEligibility.EligibilityRequestPrimaryAccountInCookieJar."
          "CookieChange",
          1);
      histogram_tester.ExpectUniqueSample(
          "Omnibox.AimEligibility.EligibilityRequestPrimaryAccountInCookieJar."
          "CookieChange",
          true, 1);
      histogram_tester.ExpectUniqueSample(
          "Omnibox.AimEligibility.EligibilityRequestPrimaryAccountIndex."
          "CookieChange",
          1, 1);

      histogram_tester.ExpectTotalCount(
          "Omnibox.AimEligibility.EligibilityRequestStatus.CookieChange", 2);
      histogram_tester.ExpectBucketCount(
          "Omnibox.AimEligibility.EligibilityRequestStatus.CookieChange",
          AimEligibilityServiceFriend::EligibilityRequestStatus::kSent, 1);
      histogram_tester.ExpectBucketCount(
          "Omnibox.AimEligibility.EligibilityRequestStatus.CookieChange",
          AimEligibilityServiceFriend::EligibilityRequestStatus::kSuccess, 1);

      histogram_tester.ExpectTotalCount(
          "Omnibox.AimEligibility.EligibilityResponseCode.CookieChange", 1);
      histogram_tester.ExpectUniqueSample(
          "Omnibox.AimEligibility.EligibilityResponseCode.CookieChange", 200,
          1);

      histogram_tester.ExpectUniqueSample(
          "Omnibox.AimEligibility.EligibilityResponse.CookieChange.is_eligible",
          !is_server_eligible, 1);
      histogram_tester.ExpectUniqueSample(
          "Omnibox.AimEligibility.EligibilityResponse.CookieChange.is_pdf_"
          "upload_eligible",
          !is_pdf_upload_eligible, 1);

      // Unsliced histograms.
      histogram_tester.ExpectTotalCount(
          "Omnibox.AimEligibility.EligibilityRequestPrimaryAccountExists", 1);
      histogram_tester.ExpectUniqueSample(
          "Omnibox.AimEligibility.EligibilityRequestPrimaryAccountExists", true,
          1);
      histogram_tester.ExpectTotalCount(
          "Omnibox.AimEligibility.EligibilityRequestPrimaryAccountInCookieJar",
          1);
      histogram_tester.ExpectUniqueSample(
          "Omnibox.AimEligibility.EligibilityRequestPrimaryAccountInCookieJar",
          true, 1);
      histogram_tester.ExpectUniqueSample(
          "Omnibox.AimEligibility.EligibilityRequestPrimaryAccountIndex", 1, 1);

      histogram_tester.ExpectTotalCount(
          "Omnibox.AimEligibility.EligibilityRequestStatus", 2);
      histogram_tester.ExpectBucketCount(
          "Omnibox.AimEligibility.EligibilityRequestStatus",
          AimEligibilityServiceFriend::EligibilityRequestStatus::kSent, 1);
      histogram_tester.ExpectBucketCount(
          "Omnibox.AimEligibility.EligibilityRequestStatus",
          AimEligibilityServiceFriend::EligibilityRequestStatus::kSuccess, 1);

      histogram_tester.ExpectTotalCount(
          "Omnibox.AimEligibility.EligibilityResponseCode", 1);
      histogram_tester.ExpectUniqueSample(
          "Omnibox.AimEligibility.EligibilityResponseCode", 200, 1);

      histogram_tester.ExpectUniqueSample(
          "Omnibox.AimEligibility.EligibilityResponse.is_eligible",
          !is_server_eligible, 1);
      histogram_tester.ExpectUniqueSample(
          "Omnibox.AimEligibility.EligibilityResponse.is_pdf_upload_eligible",
          !is_pdf_upload_eligible, 1);

      // Response change histograms.
      histogram_tester.ExpectUniqueSample(
          "Omnibox.AimEligibility.EligibilityResponseChange.is_eligible", true,
          1);
      histogram_tester.ExpectUniqueSample(
          "Omnibox.AimEligibility.EligibilityResponseChange.is_pdf_upload_"
          "eligible",
          true, 1);

    } else {
      histogram_tester.ExpectTotalCount(
          "Omnibox.AimEligibility.EligibilityRequestStatus.CookieChange", 0);
      histogram_tester.ExpectTotalCount(
          "Omnibox.AimEligibility.EligibilityResponseCode.CookieChange", 0);
    }
  }
}

class ChromeAimEligibilityServiceStartupRequestBrowserTest
    : public InProcessBrowserTest {
 public:
  ChromeAimEligibilityServiceStartupRequestBrowserTest() = default;
  ~ChromeAimEligibilityServiceStartupRequestBrowserTest() override = default;

 protected:
  void SetUp() override {
    feature_list_.InitWithFeatures(
        // Enabled features.
        {omnibox::kAimEnabled,
         omnibox::kAimServerEligibilityChangedNotification,
         omnibox::kAimServerEligibilityEnabled,
         omnibox::kAimServerRequestOnStartupEnabled,
         omnibox::kAimStartupRequestDelayedUntilNetworkAvailableEnabled},
        // Disabled features.
        {});

    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    SetUpDefaultSearchEngine(browser()->profile(), /*is_google_dse=*/true);

    AimEligibilityServiceFactory::GetInstance()->SetTestingFactory(
        browser()->profile(),
        base::BindOnce(AimEligibilityServiceFactory::GetDefaultFactory()));

    InProcessBrowserTest::SetUpOnMainThread();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(ChromeAimEligibilityServiceStartupRequestBrowserTest,
                       RequestWhenOfflineAtStartup) {
  base::HistogramTester histogram_tester;

  omnibox::AimEligibilityResponse response;
  response.set_is_eligible(true);
  auto url_loader_interceptor = std::make_unique<content::URLLoaderInterceptor>(
      base::BindLambdaForTesting(
          [&](content::URLLoaderInterceptor::RequestParams* params) {
            return OnRequest(params, std::make_optional(response),
                             base::DoNothing());
          }));

  // Given the user is offline at startup.
  auto scoped_mock_network_change_notifier =
      std::make_unique<net::test::ScopedMockNetworkChangeNotifier>();
  scoped_mock_network_change_notifier->mock_network_change_notifier()
      ->SetConnectionType(net::NetworkChangeNotifier::CONNECTION_NONE);

  // When the service is initialized.
  auto* service =
      AimEligibilityServiceFactory::GetForProfile(browser()->profile());
  base::test::TestFuture<void> eligibility_changed_future;
  auto eligibility_subscription = service->RegisterEligibilityChangedCallback(
      eligibility_changed_future.GetRepeatingCallback());

  // Then no request is sent at startup when offline.
  histogram_tester.ExpectTotalCount(
      "Omnibox.AimEligibility.EligibilityRequestStatus.Startup", 0);

  // When the network status changes to online.
  scoped_mock_network_change_notifier->mock_network_change_notifier()
      ->SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);
  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::CONNECTION_WIFI);

  // Then the delayed request should be sent.
  EXPECT_TRUE(eligibility_changed_future.Wait());

  histogram_tester.ExpectTotalCount(
      "Omnibox.AimEligibility.EligibilityRequestStatus.NetworkChange", 2);
  histogram_tester.ExpectBucketCount(
      "Omnibox.AimEligibility.EligibilityRequestStatus.NetworkChange",
      AimEligibilityServiceFriend::EligibilityRequestStatus::kSent, 1);
  histogram_tester.ExpectBucketCount(
      "Omnibox.AimEligibility.EligibilityRequestStatus.NetworkChange",
      AimEligibilityServiceFriend::EligibilityRequestStatus::kSuccess, 1);
}

IN_PROC_BROWSER_TEST_F(ChromeAimEligibilityServiceStartupRequestBrowserTest,
                       RequestWhenOnlineAtStartup) {
  base::HistogramTester histogram_tester;

  omnibox::AimEligibilityResponse response;
  response.set_is_eligible(true);
  auto url_loader_interceptor = std::make_unique<content::URLLoaderInterceptor>(
      base::BindLambdaForTesting(
          [&](content::URLLoaderInterceptor::RequestParams* params) {
            return OnRequest(params, std::make_optional(response),
                             base::DoNothing());
          }));

  // Given the user is online at startup.
  auto* service =
      AimEligibilityServiceFactory::GetForProfile(browser()->profile());
  base::test::TestFuture<void> eligibility_changed_future;
  auto eligibility_subscription = service->RegisterEligibilityChangedCallback(
      eligibility_changed_future.GetRepeatingCallback());

  // When the service is initialized, then an eligibility request is sent.
  EXPECT_TRUE(eligibility_changed_future.Wait());

  histogram_tester.ExpectTotalCount(
      "Omnibox.AimEligibility.EligibilityRequestStatus.Startup", 2);
  histogram_tester.ExpectBucketCount(
      "Omnibox.AimEligibility.EligibilityRequestStatus.Startup",
      AimEligibilityServiceFriend::EligibilityRequestStatus::kSent, 1);
  histogram_tester.ExpectBucketCount(
      "Omnibox.AimEligibility.EligibilityRequestStatus.Startup",
      AimEligibilityServiceFriend::EligibilityRequestStatus::kSuccess, 1);
}

IN_PROC_BROWSER_TEST_F(ChromeAimEligibilityServiceStartupRequestBrowserTest,
                       NoRequestOnSubsequentNetworkChanges) {
  base::HistogramTester histogram_tester;

  omnibox::AimEligibilityResponse response;
  response.set_is_eligible(true);
  auto url_loader_interceptor = std::make_unique<content::URLLoaderInterceptor>(
      base::BindLambdaForTesting(
          [&](content::URLLoaderInterceptor::RequestParams* params) {
            return OnRequest(params, std::make_optional(response),
                             base::DoNothing());
          }));

  // Given the user is offline at startup.
  auto scoped_mock_network_change_notifier =
      std::make_unique<net::test::ScopedMockNetworkChangeNotifier>();
  scoped_mock_network_change_notifier->mock_network_change_notifier()
      ->SetConnectionType(net::NetworkChangeNotifier::CONNECTION_NONE);

  // When the service is initialized.
  auto* service =
      AimEligibilityServiceFactory::GetForProfile(browser()->profile());
  base::test::TestFuture<void> eligibility_changed_future;
  auto eligibility_subscription = service->RegisterEligibilityChangedCallback(
      eligibility_changed_future.GetRepeatingCallback());

  // Then no request is sent at startup when offline.
  histogram_tester.ExpectTotalCount(
      "Omnibox.AimEligibility.EligibilityRequestStatus.Startup", 0);

  // When the network status changes to online.
  scoped_mock_network_change_notifier->mock_network_change_notifier()
      ->SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);
  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::CONNECTION_WIFI);

  // Then the delayed request should be sent.
  EXPECT_TRUE(eligibility_changed_future.Wait());

  histogram_tester.ExpectTotalCount(
      "Omnibox.AimEligibility.EligibilityRequestStatus.NetworkChange", 2);

  // When the network status changes to offline again.
  scoped_mock_network_change_notifier->mock_network_change_notifier()
      ->SetConnectionType(net::NetworkChangeNotifier::CONNECTION_NONE);
  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::CONNECTION_NONE);

  // And then online again.
  scoped_mock_network_change_notifier->mock_network_change_notifier()
      ->SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);
  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::CONNECTION_WIFI);

  // Run the message loop to allow any potential requests to be sent.
  base::RunLoop().RunUntilIdle();

  // Then no additional requests are sent.
  histogram_tester.ExpectTotalCount(
      "Omnibox.AimEligibility.EligibilityRequestStatus.NetworkChange", 2);
}

class ChromeAimEligibilityServiceRetryRequestBrowserTest
    : public InProcessBrowserTest {
 public:
  ChromeAimEligibilityServiceRetryRequestBrowserTest() = default;
  ~ChromeAimEligibilityServiceRetryRequestBrowserTest() override = default;

 protected:
  void SetUp() override {
    feature_list_.InitWithFeatures(
        // Enabled features.
        {omnibox::kAimEnabled,
         omnibox::kAimServerEligibilityChangedNotification,
         omnibox::kAimServerEligibilityEnabled,
         omnibox::kAimServerRequestOnStartupEnabled,
         omnibox::kAimServerEligibilityCustomRetryPolicyEnabled},
        // Disabled features.
        {});

    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    SetUpDefaultSearchEngine(browser()->profile(), /*is_google_dse=*/true);

    AimEligibilityServiceFactory::GetInstance()->SetTestingFactory(
        browser()->profile(),
        base::BindOnce(AimEligibilityServiceFactory::GetDefaultFactory()));

    InProcessBrowserTest::SetUpOnMainThread();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(ChromeAimEligibilityServiceRetryRequestBrowserTest,
                       RequestSucceedsOnFirstTry) {
  base::HistogramTester histogram_tester;

  // Given the eligibility request will succeed on the first try.
  int request_counter = 0;
  const int expected_retries = 0;
  omnibox::AimEligibilityResponse response;
  response.set_is_eligible(true);
  auto url_loader_interceptor = std::make_unique<content::URLLoaderInterceptor>(
      base::BindLambdaForTesting(
          [&](content::URLLoaderInterceptor::RequestParams* params) {
            return OnRequest(params, std::make_optional(response),
                             base::DoNothing(), std::nullopt, &request_counter,
                             expected_retries);
          }));

  // When the service is initialized.
  auto* service =
      AimEligibilityServiceFactory::GetForProfile(browser()->profile());
  base::test::TestFuture<void> eligibility_changed_future;
  auto eligibility_subscription = service->RegisterEligibilityChangedCallback(
      eligibility_changed_future.GetRepeatingCallback());

  // Then the eligibility request should succeed on the first try.
  EXPECT_TRUE(eligibility_changed_future.Wait());
  histogram_tester.ExpectUniqueSample(
      "Omnibox.AimEligibility.EligibilityRequestRetries.Succeeded",
      expected_retries, 1);
}

IN_PROC_BROWSER_TEST_F(ChromeAimEligibilityServiceRetryRequestBrowserTest,
                       RequestSucceedsAfterRetries) {
  base::HistogramTester histogram_tester;

  // Given the eligibility request will fail twice before succeeding.
  int request_counter = 0;
  const int expected_retries = 2;
  omnibox::AimEligibilityResponse response;
  response.set_is_eligible(true);
  auto url_loader_interceptor = std::make_unique<content::URLLoaderInterceptor>(
      base::BindLambdaForTesting(
          [&](content::URLLoaderInterceptor::RequestParams* params) {
            return OnRequest(params, std::make_optional(response),
                             base::DoNothing(), std::nullopt, &request_counter,
                             expected_retries);
          }));

  // When the service is initialized.
  auto* service =
      AimEligibilityServiceFactory::GetForProfile(browser()->profile());
  base::test::TestFuture<void> eligibility_changed_future;
  auto eligibility_subscription = service->RegisterEligibilityChangedCallback(
      eligibility_changed_future.GetRepeatingCallback());

  // Then the eligibility request should succeed after retries.
  EXPECT_TRUE(eligibility_changed_future.Wait());
  histogram_tester.ExpectUniqueSample(
      "Omnibox.AimEligibility.EligibilityRequestRetries.Succeeded",
      expected_retries, 1);
}

IN_PROC_BROWSER_TEST_F(ChromeAimEligibilityServiceRetryRequestBrowserTest,
                       RequestFailsAfterMaxRetries) {
  base::HistogramTester histogram_tester;

  // Given the eligibility request will fail on all attempts.
  int request_counter = 0;
  const int max_retries = 3;
  const int expected_failures = max_retries + 1;
  auto url_loader_interceptor = std::make_unique<content::URLLoaderInterceptor>(
      base::BindLambdaForTesting(
          [&](content::URLLoaderInterceptor::RequestParams* params) {
            return OnRequest(params, std::nullopt, base::DoNothing(),
                             std::nullopt, &request_counter, expected_failures);
          }));

  // When the service is initialized.
  auto* service =
      AimEligibilityServiceFactory::GetForProfile(browser()->profile());
  base::test::TestFuture<void> eligibility_changed_future;
  auto eligibility_subscription = service->RegisterEligibilityChangedCallback(
      eligibility_changed_future.GetRepeatingCallback());

  // Then the eligibility request should fail after all retries.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return request_counter == expected_failures;
  })) << "Timeout waiting for the request counter to reach the expected number "
         "of failures";
  EXPECT_FALSE(eligibility_changed_future.IsReady());
  histogram_tester.ExpectUniqueSample(
      "Omnibox.AimEligibility.EligibilityRequestRetries.Failed", max_retries,
      1);
}

class ChromeAimEligibilityServiceCacheBrowserTest
    : public InProcessBrowserTest {
 public:
  ChromeAimEligibilityServiceCacheBrowserTest() = default;
  ~ChromeAimEligibilityServiceCacheBrowserTest() override = default;

 protected:
  void SetUp() override {
    feature_list_.InitWithFeatures(
        // Enabled features.
        {omnibox::kAimEnabled,
         omnibox::kAimServerEligibilityChangedNotification,
         omnibox::kAimServerEligibilityEnabled,
         omnibox::kAimServerRequestOnStartupEnabled},
        // Disabled features.
        {});

    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    SetUpDefaultSearchEngine(browser()->profile(), /*is_google_dse=*/true);

    AimEligibilityServiceFactory::GetInstance()->SetTestingFactory(
        browser()->profile(),
        base::BindOnce(AimEligibilityServiceFactory::GetDefaultFactory()));

    InProcessBrowserTest::SetUpOnMainThread();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(ChromeAimEligibilityServiceCacheBrowserTest,
                       RequestFromCache) {
  auto* service =
      AimEligibilityServiceFactory::GetForProfile(browser()->profile());

  omnibox::AimEligibilityResponse response;
  response.set_is_eligible(true);
  std::string response_string;
  response.SerializeToString(&response_string);

  base::HistogramTester histogram_tester;

  AimEligibilityServiceFriend aim_eligibility_service_friend;
  aim_eligibility_service_friend.ProcessServerEligibilityResponse(
      service, AimEligibilityServiceFriend::RequestSource::kStartup, 200,
      /*was_fetched_via_cache=*/true,
      /*num_retries=*/0, std::move(response_string));
  service->IsAimEligible();

  histogram_tester.ExpectUniqueSample(
      "Omnibox.AimEligibility.EligibilityRequestStatus.Startup",
      AimEligibilityServiceFriend::EligibilityRequestStatus::
          kSuccessBrowserCache,
      1);
  histogram_tester.ExpectUniqueSample(
      "Omnibox.AimEligibility.EligibilityResponseSource",
      AimEligibilityServiceFriend::EligibilityResponseSource::kBrowserCache, 1);
}
