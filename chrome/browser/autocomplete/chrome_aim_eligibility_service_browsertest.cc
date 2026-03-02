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
#include "components/contextual_tasks/public/features.h"
#include "components/omnibox/browser/aim_eligibility_service.h"
#include "components/omnibox/browser/aim_eligibility_service_features.h"
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
      GaiaId pending_request_account,
      int response_code,
      EligibilityRequestStatus request_status,
      int num_retries,
      std::optional<std::string> response_string) {
    service->ProcessServerEligibilityResponse(
        request_source, pending_request_account, response_code, request_status,
        num_retries, std::move(response_string));
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
        {omnibox::kAimServerEligibilityForPrimaryAccountEnabled, {}});
    enabled_features.push_back(
        {omnibox::kAimServerRequestOnStartupEnabled, {}});
    enabled_features.push_back(
        {omnibox::kAimServerRequestOnIdentityChangeEnabled,
         {{"request_on_cookie_jar_changes", "true"},
          {"request_on_primary_account_changes", "false"}}});
    disabled_features.push_back(
        omnibox::kAimStartupRequestDelayedUntilNetworkAvailableEnabled);
    disabled_features.push_back(contextual_tasks::kContextualTasks);

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
         omnibox::kAimServerEligibilityEnabled,
         omnibox::kAimServerRequestOnStartupEnabled,
         omnibox::kAimStartupRequestDelayedUntilNetworkAvailableEnabled},
        // Disabled features.
        {contextual_tasks::kContextualTasks});

    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    SetUpDefaultSearchEngine(browser()->profile(), /*is_google_dse=*/true);

    AimEligibilityServiceFactory::GetInstance()->SetTestingFactory(
        browser()->profile(),
        base::BindOnce(AimEligibilityServiceFactory::GetDefaultFactory()));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// TODO(crbug.com/473787329): Flaky on multiple platforms. Re-enable once
// flakiness has been resolved.
IN_PROC_BROWSER_TEST_F(ChromeAimEligibilityServiceStartupRequestBrowserTest,
                       DISABLED_RequestWhenOfflineAtStartup) {
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

// TODO(crbug.com/473787329): Flaky on multiple platforms. Re-enable once
// flakiness has been resolved.
IN_PROC_BROWSER_TEST_F(ChromeAimEligibilityServiceStartupRequestBrowserTest,
                       DISABLED_NoRequestOnSubsequentNetworkChanges) {
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

class ChromeAimEligibilityServicePecApiEnabledBrowserTest
    : public InProcessBrowserTest {
 public:
  ChromeAimEligibilityServicePecApiEnabledBrowserTest() = default;
  ~ChromeAimEligibilityServicePecApiEnabledBrowserTest() override = default;

 protected:
  void SetUp() override {
    feature_list_.InitWithFeatures(
        {omnibox::kAimEnabled, omnibox::kAimServerEligibilityEnabled,
         omnibox::kAimServerRequestOnStartupEnabled, omnibox::kAimUsePecApi},
        {contextual_tasks::kContextualTasks});
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    SetUpDefaultSearchEngine(browser()->profile(), /*is_google_dse=*/true);
    AimEligibilityServiceFactory::GetInstance()->SetTestingFactory(
        browser()->profile(),
        base::BindOnce(AimEligibilityServiceFactory::GetDefaultFactory()));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

class ChromeAimEligibilityServicePecApiDisabledBrowserTest
    : public InProcessBrowserTest {
 public:
  ChromeAimEligibilityServicePecApiDisabledBrowserTest() = default;
  ~ChromeAimEligibilityServicePecApiDisabledBrowserTest() override = default;

 protected:
  void SetUp() override {
    feature_list_.InitWithFeatures(
        {omnibox::kAimEnabled, omnibox::kAimServerEligibilityEnabled,
         omnibox::kAimServerRequestOnStartupEnabled},
        {contextual_tasks::kContextualTasks, omnibox::kAimUsePecApi});
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    SetUpDefaultSearchEngine(browser()->profile(), /*is_google_dse=*/true);
    AimEligibilityServiceFactory::GetInstance()->SetTestingFactory(
        browser()->profile(),
        base::BindOnce(AimEligibilityServiceFactory::GetDefaultFactory()));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Test that `GetSearchboxConfig` correctly retrieves and parses the config when
// provided by the server.
IN_PROC_BROWSER_TEST_F(ChromeAimEligibilityServicePecApiEnabledBrowserTest,
                       GetSearchboxConfig_ReturnsConfigWhenPresent) {
  // Prepare a response containing a `SearchboxConfig`.
  omnibox::AimEligibilityResponse response;
  response.set_is_eligible(true);

  auto* config = response.mutable_searchbox_config();
  // Set a specific value to verify that we retrieved the correct object later.
  config->set_initial_tool_mode(omnibox::ToolMode::TOOL_MODE_DEEP_SEARCH);

  base::test::TestFuture<bool> request_handled_future;
  auto url_loader_interceptor = std::make_unique<content::URLLoaderInterceptor>(
      base::BindLambdaForTesting(
          [&](content::URLLoaderInterceptor::RequestParams* params) {
            return OnRequest(params, std::make_optional(response),
                             request_handled_future.GetRepeatingCallback());
          }));

  auto* service =
      AimEligibilityServiceFactory::GetForProfile(browser()->profile());

  base::test::TestFuture<void> eligibility_changed_future;
  auto eligibility_subscription = service->RegisterEligibilityChangedCallback(
      eligibility_changed_future.GetRepeatingCallback());

  // Wait for the service to update its internal state.
  EXPECT_TRUE(eligibility_changed_future.Wait());

  // Verify the config was correctly parsed and matches the input.
  const auto* actual_config = service->GetSearchboxConfig();
  ASSERT_NE(actual_config, nullptr);
  EXPECT_EQ(actual_config->initial_tool_mode(),
            omnibox::ToolMode::TOOL_MODE_DEEP_SEARCH);
}

// Test that when the server sends legacy boolean fields and the PEC API feature
// is disabled, the service correctly backfills (generates) a `SearchboxConfig`
// locally.
IN_PROC_BROWSER_TEST_F(ChromeAimEligibilityServicePecApiDisabledBrowserTest,
                       GetSearchboxConfig_BackfillsFromLegacyFields) {
  omnibox::AimEligibilityResponse response;
  response.set_is_eligible(true);

  // Set legacy boolean fields AND `SearchboxConfig`.
  // This forces the service to generate the config locally using the backfill
  // logic because the `kAimUsePecApi` feature is disabled.
  response.set_is_deep_search_eligible(true);
  response.set_is_canvas_eligible(true);
  response.mutable_searchbox_config()->set_initial_tool_mode(
      omnibox::ToolMode::TOOL_MODE_UNSPECIFIED);

  base::test::TestFuture<bool> request_handled_future;
  auto url_loader_interceptor = std::make_unique<content::URLLoaderInterceptor>(
      base::BindLambdaForTesting(
          [&](content::URLLoaderInterceptor::RequestParams* params) {
            return OnRequest(params, std::make_optional(response),
                             request_handled_future.GetRepeatingCallback());
          }));

  auto* service =
      AimEligibilityServiceFactory::GetForProfile(browser()->profile());

  base::test::TestFuture<void> eligibility_changed_future;
  auto eligibility_subscription = service->RegisterEligibilityChangedCallback(
      eligibility_changed_future.GetRepeatingCallback());

  EXPECT_TRUE(eligibility_changed_future.Wait());

  // Verify that `GetSearchboxConfig` returns a non-null, backfilled config.
  const auto* actual_config = service->GetSearchboxConfig();

  ASSERT_NE(actual_config, nullptr);
  ASSERT_TRUE(actual_config->has_rule_set());

  // Verify Deep Search was mapped to allowed_tools.
  bool has_deep_search = false;
  for (const auto& tool : actual_config->rule_set().allowed_tools()) {
    if (tool == omnibox::ToolMode::TOOL_MODE_DEEP_SEARCH) {
      has_deep_search = true;
      break;
    }
  }
  EXPECT_TRUE(has_deep_search);

  // Verify Canvas was mapped to allowed_tools.
  bool has_canvas = false;
  for (const auto& tool : actual_config->rule_set().allowed_tools()) {
    if (tool == omnibox::ToolMode::TOOL_MODE_CANVAS) {
      has_canvas = true;
      break;
    }
  }
  EXPECT_TRUE(has_canvas);
}

IN_PROC_BROWSER_TEST_F(ChromeAimEligibilityServicePecApiEnabledBrowserTest,
                       RespectsAllowedToolsConfig) {
  omnibox::AimEligibilityResponse response;
  response.set_is_eligible(true);

  // Configure the response to explicitly allow DEEP_SEARCH but not IMAGE_GEN.
  // This helps verify that the service respects the specific allowlist.
  auto* rule_set = response.mutable_searchbox_config()->mutable_rule_set();
  rule_set->add_allowed_tools(omnibox::ToolMode::TOOL_MODE_DEEP_SEARCH);

  base::test::TestFuture<bool> request_handled_future;
  auto url_loader_interceptor = std::make_unique<content::URLLoaderInterceptor>(
      base::BindLambdaForTesting(
          [&](content::URLLoaderInterceptor::RequestParams* params) {
            return OnRequest(params, std::make_optional(response),
                             request_handled_future.GetRepeatingCallback());
          }));

  auto* service =
      AimEligibilityServiceFactory::GetForProfile(browser()->profile());
  base::test::TestFuture<void> eligibility_changed_future;
  auto eligibility_subscription = service->RegisterEligibilityChangedCallback(
      eligibility_changed_future.GetRepeatingCallback());

  // Wait for the service to process the network response.
  EXPECT_TRUE(eligibility_changed_future.Wait());

  // DEEP_SEARCH should be eligible as it is in the allowed_tools list.
  EXPECT_TRUE(service->IsDeepSearchEligible());

  // IMAGE_GEN should not be eligible as it is missing from the list.
  EXPECT_FALSE(service->IsCreateImagesEligible());
}

IN_PROC_BROWSER_TEST_F(ChromeAimEligibilityServicePecApiEnabledBrowserTest,
                       RespectsPdfUploadConfig) {
  // Prepare a response that explicitly allows PDF uploads via
  // `SearchboxConfig`. This verifies that the service checks the
  // `allowed_input_types` list.
  omnibox::AimEligibilityResponse response;
  response.set_is_eligible(true);

  auto* rule_set = response.mutable_searchbox_config()->mutable_rule_set();
  rule_set->add_allowed_input_types(omnibox::InputType::INPUT_TYPE_LENS_FILE);

  base::test::TestFuture<bool> request_handled_future;
  auto url_loader_interceptor = std::make_unique<content::URLLoaderInterceptor>(
      base::BindLambdaForTesting(
          [&](content::URLLoaderInterceptor::RequestParams* params) {
            return OnRequest(params, std::make_optional(response),
                             request_handled_future.GetRepeatingCallback());
          }));

  auto* service =
      AimEligibilityServiceFactory::GetForProfile(browser()->profile());

  base::test::TestFuture<void> eligibility_changed_future;
  auto eligibility_subscription = service->RegisterEligibilityChangedCallback(
      eligibility_changed_future.GetRepeatingCallback());

  EXPECT_TRUE(eligibility_changed_future.Wait());

  EXPECT_TRUE(service->IsPdfUploadEligible());
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
         omnibox::kAimServerEligibilityEnabled,
         omnibox::kAimServerRequestOnStartupEnabled,
         omnibox::kAimServerEligibilityCustomRetryPolicyEnabled},
        // Disabled features.
        {contextual_tasks::kContextualTasks});

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
         omnibox::kAimServerEligibilityEnabled,
         omnibox::kAimServerRequestOnStartupEnabled},
        // Disabled features.
        {contextual_tasks::kContextualTasks});

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
      service, AimEligibilityServiceFriend::RequestSource::kStartup, GaiaId(),
      200,
      AimEligibilityServiceFriend::EligibilityRequestStatus::
          kSuccessBrowserCache,
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

class ChromeAimEligibilityServiceOffTheRecordBrowserTest
    : public InProcessBrowserTest {
 public:
  ChromeAimEligibilityServiceOffTheRecordBrowserTest() = default;
  ~ChromeAimEligibilityServiceOffTheRecordBrowserTest() override = default;

 protected:
  void SetUp() override {
    feature_list_.InitWithFeatures(
        // Enabled features.
        {omnibox::kAimEnabled},
        // Disabled features.
        {contextual_tasks::kContextualTasks,
         omnibox::kAimServerEligibilityEnabled});
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

IN_PROC_BROWSER_TEST_F(ChromeAimEligibilityServiceOffTheRecordBrowserTest,
                       IsCreateImagesEligibleReturnsFalseForOffTheRecord) {
  // Check regular profile.
  auto* service =
      AimEligibilityServiceFactory::GetForProfile(browser()->profile());
  ASSERT_TRUE(service);
  EXPECT_TRUE(service->IsCreateImagesEligible());

  // Check off-the-record profile.
  Profile* otr_profile = browser()->profile()->GetPrimaryOTRProfile(
      /*create_if_needed=*/true);
  auto* otr_service = AimEligibilityServiceFactory::GetForProfile(otr_profile);
  ASSERT_TRUE(otr_service);
  EXPECT_NE(service, otr_service);
  EXPECT_FALSE(otr_service->IsCreateImagesEligible());
}

class ChromeAimEligibilityServiceOAuthBrowserTest
    : public InProcessBrowserTest {
 public:
  ChromeAimEligibilityServiceOAuthBrowserTest() = default;
  ~ChromeAimEligibilityServiceOAuthBrowserTest() override = default;

  signin::IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_adaptor_->identity_test_env();
  }

  network::TestURLLoaderFactory* test_url_loader_factory() {
    return signin_client_with_url_loader_helper_.test_url_loader_factory();
  }

 protected:
  void SetUp() override {
    feature_list_.InitWithFeaturesAndParameters(
        // Enabled features.
        {{omnibox::kAimEnabled, {}},
         {omnibox::kAimServerEligibilityEnabled, {}},
         {omnibox::kAimServerRequestOnStartupEnabled, {}},
         {omnibox::kAimEligibilityServiceIdentityImprovements, {}}},
        // Disabled features.
        {contextual_tasks::kContextualTasks,
         omnibox::kAimEligibilityServiceDebounce});
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    SetUpDefaultSearchEngine(browser()->profile(), /*is_google_dse=*/true);

    // Set the adaptor that supports signin::IdentityTestEnvironment.
    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(
            browser()->profile());

    AimEligibilityServiceFactory::GetInstance()->SetTestingFactory(
        browser()->profile(),
        base::BindOnce(AimEligibilityServiceFactory::GetDefaultFactory()));

    identity_test_env()->SetTestURLLoaderFactory(test_url_loader_factory());
    identity_test_env()->SetAutomaticIssueOfAccessTokens(true);
  }

  void TearDownOnMainThread() override {
    InProcessBrowserTest::TearDownOnMainThread();
  }

  void SetUpInProcessBrowserTestFixture() override {
    signin_client_with_url_loader_helper_.SetUp();
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
                &ChromeAimEligibilityServiceOAuthBrowserTest::
                    OnWillCreateBrowserContextServices));
  }

  static void OnWillCreateBrowserContextServices(
      content::BrowserContext* context) {
    // Set up IdentityTestEnvironment.
    IdentityTestEnvironmentProfileAdaptor::
        SetIdentityTestEnvironmentFactoriesOnBrowserContext(context);
  }

  ChromeSigninClientWithURLLoaderHelper signin_client_with_url_loader_helper_;
  base::CallbackListSubscription create_services_subscription_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(ChromeAimEligibilityServiceOAuthBrowserTest,
                       RequestIncludesOAuthToken) {
  // Setup: Make a primary account available with a refresh token.
  auto* identity_manager = identity_test_env()->identity_manager();
  AccountInfo primary_account_info = signin::MakeAccountAvailable(
      identity_manager,
      signin::AccountAvailabilityOptionsBuilder(test_url_loader_factory())
          .AsPrimary(signin::ConsentLevel::kSignin)
          .WithRefreshToken("refresh_token")
          .Build("primary@email.com"));

  // Expectation: The request should include the Authorization header.
  omnibox::AimEligibilityResponse response;
  response.set_is_eligible(true);
  base::test::TestFuture<bool> request_handled_future;
  auto url_loader_interceptor = std::make_unique<content::URLLoaderInterceptor>(
      base::BindLambdaForTesting(
          [&](content::URLLoaderInterceptor::RequestParams* params) {
            if (params->url_request.url.path() != "/async/folae") {
              return false;
            }
            std::optional<std::string> authorization =
                params->url_request.headers.GetHeader(
                    net::HttpRequestHeaders::kAuthorization);
            bool has_auth = authorization.has_value();
            EXPECT_TRUE(has_auth);
            EXPECT_TRUE(base::StartsWith(*authorization, "Bearer "));
            EXPECT_EQ(params->url_request.credentials_mode,
                      network::mojom::CredentialsMode::kOmit);
            return OnRequest(params, std::make_optional(response),
                             request_handled_future.GetRepeatingCallback());
          }));

  // Trigger the request.
  auto* service =
      AimEligibilityServiceFactory::GetForProfile(browser()->profile());
  base::test::TestFuture<void> eligibility_changed_future;
  auto eligibility_subscription = service->RegisterEligibilityChangedCallback(
      eligibility_changed_future.GetRepeatingCallback());

  EXPECT_TRUE(eligibility_changed_future.Wait());
  EXPECT_TRUE(request_handled_future.Get());
}


IN_PROC_BROWSER_TEST_F(ChromeAimEligibilityServiceOAuthBrowserTest,
                       OTRRequestIsNotDropped) {
  // Expectation: The request should include the Authorization header.
  omnibox::AimEligibilityResponse response;
  response.set_is_eligible(true);
  base::test::TestFuture<bool> request_handled_future;
  auto url_loader_interceptor = std::make_unique<content::URLLoaderInterceptor>(
      base::BindLambdaForTesting(
          [&](content::URLLoaderInterceptor::RequestParams* params) {
            if (params->url_request.url.path() != "/async/folae") {
              return false;
            }
            return OnRequest(params, std::make_optional(response),
                             request_handled_future.GetRepeatingCallback());
          }));

  // Trigger the request.
  // Check off-the-record profile.
  Profile* otr_profile = browser()->profile()->GetPrimaryOTRProfile(
      /*create_if_needed=*/true);
  auto* service = AimEligibilityServiceFactory::GetForProfile(otr_profile);
  base::test::TestFuture<void> eligibility_changed_future;
  auto eligibility_subscription = service->RegisterEligibilityChangedCallback(
      eligibility_changed_future.GetRepeatingCallback());

  EXPECT_TRUE(eligibility_changed_future.Wait());
  EXPECT_TRUE(request_handled_future.Get());
}

// This test is not supported on ChromeOS since there isn't a way to clear the
// primary account. See:
// https://crsrc.org/c/components/signin/public/identity_manager/identity_test_utils.cc;?q=signin::ClearPrimaryAccount&ss=chromium
#if !BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(ChromeAimEligibilityServiceOAuthBrowserTest,
                       PrimaryAccountTracking) {
  base::HistogramTester histogram_tester;
  int request_counter = 0;

  omnibox::AimEligibilityResponse response;
  response.set_is_eligible(true);
  base::test::TestFuture<bool> request_handled_future;
  auto url_loader_interceptor = std::make_unique<content::URLLoaderInterceptor>(
      base::BindLambdaForTesting(
          [&](content::URLLoaderInterceptor::RequestParams* params) {
            if (params->url_request.url.path() != "/async/folae") {
              return false;
            }
            request_counter++;
            return OnRequest(params, std::make_optional(response),
                             request_handled_future.GetRepeatingCallback());
          }));

  auto* service =
      AimEligibilityServiceFactory::GetForProfile(browser()->profile());
  base::test::TestFuture<void> eligibility_changed_future;
  auto eligibility_subscription = service->RegisterEligibilityChangedCallback(
      eligibility_changed_future.GetRepeatingCallback());
  // Set the cookies to be empty so they are fresh and the first request is not
  // dropped.
  identity_test_env()->SetCookieAccounts({{}});

  // Wait for initial startup request (anonymous/no primary account).
  EXPECT_TRUE(request_handled_future.Take());
  EXPECT_TRUE(eligibility_changed_future.Wait());
  request_handled_future.Clear();
  eligibility_changed_future.Clear();

  auto* identity_manager = identity_test_env()->identity_manager();
  IdentityManagerObserverHelper identity_observer(identity_manager);

  // 1. Sign In "A" (Primary). effective ID is "A". Should trigger fetch.
  // Change response to ensure observer fires.
  response.set_is_eligible(!response.is_eligible());
  AccountInfo account_a = signin::MakeAccountAvailable(
      identity_manager,
      signin::AccountAvailabilityOptionsBuilder(test_url_loader_factory())
          .AsPrimary(signin::ConsentLevel::kSignin)
          .Build("a@email.com"));
  EXPECT_TRUE(identity_observer.WaitForPrimaryAccountChanged());
  identity_test_env()->SetCookieAccounts({{account_a.email, account_a.gaia}});

  EXPECT_TRUE(request_handled_future.Take());
  EXPECT_TRUE(eligibility_changed_future.Wait());
  request_handled_future.Clear();
  eligibility_changed_future.Clear();

  // 2. Sign Out "A". effective ID is "". Should trigger fetch.
  // Change response to ensure observer fires.
  response.set_is_eligible(!response.is_eligible());
  signin::ClearPrimaryAccount(identity_manager);
  EXPECT_TRUE(identity_observer.WaitForPrimaryAccountChanged());
  identity_test_env()->SetCookieAccounts({{}});

  EXPECT_TRUE(request_handled_future.Take());
  EXPECT_TRUE(eligibility_changed_future.Wait());

  EXPECT_EQ(request_counter, 3);
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

IN_PROC_BROWSER_TEST_F(ChromeAimEligibilityServiceOAuthBrowserTest,
                       FallbackToCookieWhenNoPrimaryAccount) {
  // No primary account at startup (fallback mode).
  omnibox::AimEligibilityResponse response;
  response.set_is_eligible(true);
  base::test::TestFuture<bool> request_handled_future;
  auto url_loader_interceptor = std::make_unique<content::URLLoaderInterceptor>(
      base::BindLambdaForTesting(
          [&](content::URLLoaderInterceptor::RequestParams* params) {
            if (params->url_request.url.path() != "/async/folae") {
              return false;
            }
            EXPECT_FALSE(params->url_request.headers.HasHeader(
                net::HttpRequestHeaders::kAuthorization));
            EXPECT_EQ(params->url_request.credentials_mode,
                      network::mojom::CredentialsMode::kInclude);
            return OnRequest(params, std::make_optional(response),
                             request_handled_future.GetRepeatingCallback());
          }));

  auto* service =
      AimEligibilityServiceFactory::GetForProfile(browser()->profile());
  base::test::TestFuture<void> eligibility_changed_future;
  auto eligibility_subscription = service->RegisterEligibilityChangedCallback(
      eligibility_changed_future.GetRepeatingCallback());
  identity_test_env()->SetCookieAccounts({{}});

  EXPECT_TRUE(request_handled_future.Take());
  EXPECT_TRUE(eligibility_changed_future.Wait());
}

IN_PROC_BROWSER_TEST_F(ChromeAimEligibilityServiceOAuthBrowserTest,
                       RefreshesOnFallbackCookieChange) {
  // No primary account at startup (fallback mode).
  omnibox::AimEligibilityResponse response;
  response.set_is_eligible(true);

  base::test::TestFuture<bool> request_handled_future;
  auto url_loader_interceptor = std::make_unique<content::URLLoaderInterceptor>(
      base::BindLambdaForTesting(
          [&](content::URLLoaderInterceptor::RequestParams* params) {
            return OnRequest(params, std::make_optional(response),
                             request_handled_future.GetRepeatingCallback());
          }));

  auto* service =
      AimEligibilityServiceFactory::GetForProfile(browser()->profile());
  base::test::TestFuture<void> eligibility_changed_future;
  auto eligibility_subscription = service->RegisterEligibilityChangedCallback(
      eligibility_changed_future.GetRepeatingCallback());
  identity_test_env()->SetCookieAccounts({{}});

  // Wait for the first request (Startup) to be handled.
  EXPECT_TRUE(request_handled_future.Take());
  EXPECT_TRUE(eligibility_changed_future.Wait());
  eligibility_changed_future.Clear();

  // Update the accounts in the cookie jar to trigger a new request.
  auto* identity_manager = identity_test_env()->identity_manager();
  IdentityManagerObserverHelper identity_observer(identity_manager);

  AccountInfo account_info = signin::MakeAccountAvailable(
      identity_manager,
      signin::AccountAvailabilityOptionsBuilder(test_url_loader_factory())
          .Build("fallback@email.com"));
  signin::SetCookieAccounts(identity_manager, test_url_loader_factory(),
                            {{account_info.email, account_info.gaia}});
  EXPECT_TRUE(identity_observer.WaitForAccountsInCookieUpdated());

  EXPECT_TRUE(request_handled_future.Take());
}

// This test is not supported on ChromeOS since there isn't a way to clear the
// primary account. See:
// https://crsrc.org/c/components/signin/public/identity_manager/identity_test_utils.cc;?q=signin::ClearPrimaryAccount&ss=chromium
#if !BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(ChromeAimEligibilityServiceOAuthBrowserTest,
                       AccountTracking) {
  base::HistogramTester histogram_tester;
  int request_counter = 0;

  omnibox::AimEligibilityResponse response;
  response.set_is_eligible(true);
  base::test::TestFuture<bool> request_handled_future;
  auto url_loader_interceptor = std::make_unique<content::URLLoaderInterceptor>(
      base::BindLambdaForTesting(
          [&](content::URLLoaderInterceptor::RequestParams* params) {
            if (params->url_request.url.path() != "/async/folae") {
              return false;
            }
            request_counter++;
            return OnRequest(params, std::make_optional(response),
                             request_handled_future.GetRepeatingCallback());
          }));

  auto* service =
      AimEligibilityServiceFactory::GetForProfile(browser()->profile());
  base::test::TestFuture<void> eligibility_changed_future;
  auto eligibility_subscription = service->RegisterEligibilityChangedCallback(
      eligibility_changed_future.GetRepeatingCallback());
  identity_test_env()->SetCookieAccounts({{}});

  // Wait for initial startup request (anonymous/cookie).
  EXPECT_TRUE(request_handled_future.Take());
  EXPECT_TRUE(eligibility_changed_future.Wait());
  request_handled_future.Clear();
  eligibility_changed_future.Clear();

  auto* identity_manager = identity_test_env()->identity_manager();
  IdentityManagerObserverHelper identity_observer(identity_manager);

  // 1. Add Cookie "A". Should trigger fetch (Effective ID: "" -> "A").
  // Change response to ensure observer fires.
  response.set_is_eligible(!response.is_eligible());
  AccountInfo account_a = signin::MakeAccountAvailable(
      identity_manager,
      signin::AccountAvailabilityOptionsBuilder(test_url_loader_factory())
          .Build("a@email.com"));
  signin::SetCookieAccounts(identity_manager, test_url_loader_factory(),
                            {{account_a.email, account_a.gaia}});
  EXPECT_TRUE(identity_observer.WaitForAccountsInCookieUpdated());

  EXPECT_TRUE(request_handled_future.Take());
  EXPECT_TRUE(eligibility_changed_future.Wait());
  request_handled_future.Clear();
  eligibility_changed_future.Clear();

  // 2. Sign In "A" (Primary). effective ID is "A". Should NOT trigger fetch.
  signin::MakePrimaryAccountAvailable(identity_manager, account_a.email,
                                      signin::ConsentLevel::kSignin);
  EXPECT_TRUE(identity_observer.WaitForPrimaryAccountChanged());

  // Verify NO request sent.
  EXPECT_FALSE(request_handled_future.IsReady());

  // 3. Sign Out "A". effective ID is "A" (fallback). Should NOT trigger fetch.
  signin::ClearPrimaryAccount(identity_manager);
  EXPECT_TRUE(identity_observer.WaitForPrimaryAccountChanged());
  EXPECT_FALSE(request_handled_future.IsReady());

  // 4. Remove Cookies. Effective ID "A" -> "". Should trigger fetch.
  // Change response to ensure observer fires.
  response.set_is_eligible(!response.is_eligible());
  signin::SetCookieAccounts(identity_manager, test_url_loader_factory(), {});
  EXPECT_TRUE(identity_observer.WaitForAccountsInCookieUpdated());

  EXPECT_TRUE(request_handled_future.Take());
  EXPECT_TRUE(eligibility_changed_future.Wait());

  EXPECT_EQ(request_counter, 3);
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS)
// TODO(crbug.com/488467253): Fix and re-enable this test for CrOS.
#define MAYBE_RefreshesOnPersistentError DISABLED_RefreshesOnPersistentError
#else
#define MAYBE_RefreshesOnPersistentError RefreshesOnPersistentError
#endif
IN_PROC_BROWSER_TEST_F(ChromeAimEligibilityServiceOAuthBrowserTest,
                       MAYBE_RefreshesOnPersistentError) {
  base::HistogramTester histogram_tester;

  omnibox::AimEligibilityResponse response;
  response.set_is_eligible(true);
  base::test::TestFuture<bool> request_handled_future;
  base::test::TestFuture<bool> has_auth_future;
  base::test::TestFuture<network::mojom::CredentialsMode>
      credentials_mode_future;

  auto* identity_manager = identity_test_env()->identity_manager();
  AccountInfo account_a = signin::MakeAccountAvailable(
      identity_manager,
      signin::AccountAvailabilityOptionsBuilder(test_url_loader_factory())
          .AsPrimary(signin::ConsentLevel::kSignin)
          .Build("a@email.com"));

  AccountInfo account_b = signin::MakeAccountAvailable(
      identity_manager,
      signin::AccountAvailabilityOptionsBuilder(test_url_loader_factory())
          .Build("account_b@email.com"));

  auto url_loader_interceptor = std::make_unique<content::URLLoaderInterceptor>(
      base::BindLambdaForTesting(
          [&](content::URLLoaderInterceptor::RequestParams* params) {
            if (params->url_request.url.path() != "/async/folae") {
              return false;
            }

            has_auth_future.SetValue(params->url_request.headers.HasHeader(
                net::HttpRequestHeaders::kAuthorization));
            credentials_mode_future.SetValue(
                params->url_request.credentials_mode);

            return OnRequest(params, std::make_optional(response),
                             request_handled_future.GetRepeatingCallback());
          }));

  // Set the cookies out of order so that when the primary account is invalid,
  // the zero index cookie account ID is different from the primary account and
  // triggers a new request.
  identity_test_env()->SetCookieAccounts(
      {{account_b.email, account_b.gaia}, {account_a.email, account_a.gaia}});

  auto* service =
      AimEligibilityServiceFactory::GetForProfile(browser()->profile());
  base::test::TestFuture<void> eligibility_changed_future;
  auto eligibility_subscription = service->RegisterEligibilityChangedCallback(
      eligibility_changed_future.GetRepeatingCallback());

  // Wait for initial request.
  EXPECT_TRUE(request_handled_future.Take());
  EXPECT_TRUE(eligibility_changed_future.Wait());

  EXPECT_TRUE(has_auth_future.Take());
  EXPECT_EQ(credentials_mode_future.Take(),
            network::mojom::CredentialsMode::kOmit);

  eligibility_changed_future.Clear();

  IdentityManagerObserverHelper identity_observer(identity_manager);
  response.set_is_eligible(!response.is_eligible());

  signin::UpdatePersistentErrorOfRefreshTokenForAccount(
      identity_manager, account_a.account_id,
      GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
          GoogleServiceAuthError::InvalidGaiaCredentialsReason::
              CREDENTIALS_REJECTED_BY_SERVER));

  EXPECT_TRUE(request_handled_future.Take());
  EXPECT_TRUE(eligibility_changed_future.Wait());

  EXPECT_FALSE(has_auth_future.Take());
  EXPECT_EQ(credentials_mode_future.Take(),
            network::mojom::CredentialsMode::kInclude);
}
