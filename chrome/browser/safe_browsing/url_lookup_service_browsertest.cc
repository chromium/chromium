// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/realtime/url_lookup_service.h"

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/url_lookup_service_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safe_browsing/core/common/utils.h"
#include "components/variations/pref_names.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"

namespace safe_browsing {

namespace {

constexpr std::string_view kRealtimeEndpoint = "/realtime_endpoint";

class SafeBrowsingUrlLookupServiceTest : public InProcessBrowserTest {
 public:
  SafeBrowsingUrlLookupServiceTest() {
    scoped_feature_list_.InitAndDisableFeature(
        kSafeBrowsingRemoveCookiesInAuthRequests);
  }
  SafeBrowsingUrlLookupServiceTest(const SafeBrowsingUrlLookupServiceTest&) =
      delete;
  SafeBrowsingUrlLookupServiceTest& operator=(
      const SafeBrowsingUrlLookupServiceTest&) = delete;

  void SetUp() override {
    secure_embedded_test_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::Type::TYPE_HTTPS);
    secure_embedded_test_server_->RegisterRequestHandler(base::BindRepeating(
        &SafeBrowsingUrlLookupServiceTest::UrlRequestHandler,
        base::Unretained(this)));
    ASSERT_TRUE(secure_embedded_test_server_->Start());
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(
            browser()->profile());
  }

  void SetUpInProcessBrowserTestFixture() override {
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::BindRepeating(&SafeBrowsingUrlLookupServiceTest::
                                        OnWillCreateBrowserContextServices,
                                    base::Unretained(this)));
  }

  signin::IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_adaptor_->identity_test_env();
  }

  net::EmbeddedTestServer* secure_embedded_test_server() {
    return secure_embedded_test_server_.get();
  }

  const net::test_server::HttpRequest last_realtime_request() {
    return last_realtime_request_;
  }

 private:
  std::unique_ptr<net::test_server::HttpResponse> UrlRequestHandler(
      const net::test_server::HttpRequest& request) {
    if (request.relative_url == kRealtimeEndpoint) {
      auto response = std::make_unique<net::test_server::BasicHttpResponse>();
      response->set_code(net::HTTP_OK);
      response->AddCustomHeader("Set-Cookie", "cookie=reset");
      last_realtime_request_ = request;
      return response;
    }
    return nullptr;
  }

  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    IdentityTestEnvironmentProfileAdaptor::
        SetIdentityTestEnvironmentFactoriesOnBrowserContext(context);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<net::EmbeddedTestServer> secure_embedded_test_server_;
  base::CallbackListSubscription create_services_subscription_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  net::test_server::HttpRequest last_realtime_request_;
};

IN_PROC_BROWSER_TEST_F(SafeBrowsingUrlLookupServiceTest,
                       ServiceRespectsLocationChanges) {
  safe_browsing::SetSafeBrowsingState(
      browser()->profile()->GetPrefs(),
      safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION);
  auto* url_lookup_service =
      RealTimeUrlLookupServiceFactory::GetForProfile(browser()->profile());

  // By default for ESB, full URL lookups should be enabled.
  EXPECT_TRUE(url_lookup_service->CanPerformFullURLLookup());

  // Changing to CN should disable the lookups.
  g_browser_process->local_state()->SetString(
      variations::prefs::kVariationsCountry, "cn");
  EXPECT_FALSE(url_lookup_service->CanPerformFullURLLookup());

  // Changing to US should re-enable the lookups.
  g_browser_process->local_state()->SetString(
      variations::prefs::kVariationsCountry, "us");
  EXPECT_TRUE(url_lookup_service->CanPerformFullURLLookup());
}

IN_PROC_BROWSER_TEST_F(SafeBrowsingUrlLookupServiceTest, LookupWithToken) {
  identity_test_env()->SetPrimaryAccount("test@example.com",
                                         signin::ConsentLevel::kSync);
  identity_test_env()->SetRefreshTokenForPrimaryAccount();
  identity_test_env()->SetAutomaticIssueOfAccessTokens(true);

  RealTimeUrlLookupService::OverrideUrlForTesting(
      secure_embedded_test_server()->GetURL(kRealtimeEndpoint));

  safe_browsing::SetSafeBrowsingState(
      browser()->profile()->GetPrefs(),
      safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION);
  auto* url_lookup_service =
      RealTimeUrlLookupServiceFactory::GetForProfile(browser()->profile());

  base::RunLoop run_loop;
  base::HistogramTester histogram_tester;
  url_lookup_service->StartLookup(
      secure_embedded_test_server()->GetURL("/"),
      base::IgnoreArgs<bool, bool, std::unique_ptr<RTLookupResponse>>(
          run_loop.QuitClosure()),
      base::SequencedTaskRunner::GetCurrentDefault(), SessionID::InvalidValue(),
      /*referring_app_info=*/std::nullopt);
  run_loop.Run();

  EXPECT_TRUE(base::Contains(last_realtime_request().headers,
                             net::HttpRequestHeaders::kAuthorization));

  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.AuthenticatedCookieResetEndpoint",
      SafeBrowsingAuthenticatedEndpoint::kRealtimeUrlLookup, 1);
}

IN_PROC_BROWSER_TEST_F(SafeBrowsingUrlLookupServiceTest, LookupWithoutToken) {
  RealTimeUrlLookupService::OverrideUrlForTesting(
      secure_embedded_test_server()->GetURL(kRealtimeEndpoint));

  safe_browsing::SetSafeBrowsingState(
      browser()->profile()->GetPrefs(),
      safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION);
  auto* url_lookup_service =
      RealTimeUrlLookupServiceFactory::GetForProfile(browser()->profile());

  base::RunLoop run_loop;
  base::HistogramTester histogram_tester;
  url_lookup_service->StartLookup(
      secure_embedded_test_server()->GetURL("/"),
      base::IgnoreArgs<bool, bool, std::unique_ptr<RTLookupResponse>>(
          run_loop.QuitClosure()),
      base::SequencedTaskRunner::GetCurrentDefault(), SessionID::InvalidValue(),
      /*referring_app_info=*/std::nullopt);
  run_loop.Run();

  EXPECT_FALSE(base::Contains(last_realtime_request().headers,
                              net::HttpRequestHeaders::kAuthorization));

  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.AuthenticatedCookieResetEndpoint",
      SafeBrowsingAuthenticatedEndpoint::kRealtimeUrlLookup, 0);
}

}  // namespace

}  // namespace safe_browsing
