// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/enterprise/net/enterprise_proxy_error_service_factory.h"
#include "chrome/browser/enterprise/net/test/enterprise_proxy_browsertest_base.h"
#include "chrome/browser/enterprise/test/management_context_mixin.h"
#include "chrome/browser/net/profile_network_context_service.h"
#include "chrome/browser/net/profile_network_context_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/enterprise/net/content/enterprise_proxy_tab_helper.h"
#include "components/enterprise/net/core/enterprise_proxy_error_data.h"
#include "components/enterprise/net/core/enterprise_proxy_error_service.h"
#include "components/enterprise/net/core/enterprise_proxy_service.h"
#include "components/error_page/common/net_error_info.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/prefs/pref_service.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/strings/grit/components_strings.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/http/http_status_code.h"
#include "net/log/net_log_entry.h"
#include "net/log/net_log_event_type.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace enterprise::test {

namespace {

#if BUILDFLAG(IS_ANDROID)
class TestTabHelperDelegate
    : public enterprise_net::EnterpriseProxyTabHelper::Delegate {
 public:
  explicit TestTabHelperDelegate(
      base::OnceCallback<void(content::WebContents*)> on_sign_in)
      : on_sign_in_(std::move(on_sign_in)) {}
  ~TestTabHelperDelegate() override = default;

  void SignIn(content::WebContents* web_contents) override {
    if (on_sign_in_) {
      std::move(on_sign_in_).Run(web_contents);
    }
  }

 private:
  base::OnceCallback<void(content::WebContents*)> on_sign_in_;
};
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace

class EnterpriseProxyErrorBrowserTest : public EnterpriseProxyBrowserTestBase {
 public:
  EnterpriseProxyErrorBrowserTest()
      : EnterpriseProxyBrowserTestBase(ManagementContext{
            .is_cloud_user_managed = true,
            .is_cloud_machine_managed = true,
            .affiliated = true,
        }) {}

 protected:
  // Validates that the rendered error page in `web_contents` matches the
  // Authentication error state (category 0).
  void VerifyAuthenticationErrorPage(content::WebContents* web_contents) {
    ASSERT_TRUE(web_contents);

    EXPECT_EQ(
        true,
        content::EvalJs(
            web_contents,
            "document.body.classList.contains('enterprise-proxy-error')"));
    EXPECT_EQ(
        true,
        content::EvalJs(
            web_contents,
            "document.body.classList.contains('category-authentication')"));
    EXPECT_EQ(
        false,
        content::EvalJs(
            web_contents,
            "document.body.classList.contains('category-authorization')"));
    EXPECT_EQ(false, content::EvalJs(
                         web_contents,
                         "document.body.classList.contains('category-other')"));

    EXPECT_EQ("0", content::EvalJs(web_contents,
                                   "window.loadTimeDataRaw['error_category']"));
    EXPECT_EQ(
        default_proxy_server().GetURL().spec(),
        content::EvalJs(web_contents, "window.loadTimeDataRaw['proxy_url']"));

    const std::string expected_heading =
        l10n_util::GetStringUTF8(IDS_ENTERPRISE_PROXY_AUTHN_ERROR_HEADING);
    EXPECT_EQ(kDestinationHost,
              content::EvalJs(web_contents, "document.title"));
    EXPECT_EQ(expected_heading,
              content::EvalJs(
                  web_contents,
                  "document.querySelector('#main-message h1').textContent"));
    EXPECT_EQ(l10n_util::GetStringUTF8(
                  IDS_ENTERPRISE_PROXY_AUTHN_ERROR_PRIMARY_PARAGRAPH),
              content::EvalJs(
                  web_contents,
                  "document.querySelector('#main-message p').textContent"));

    EXPECT_EQ(false, content::EvalJs(
                         web_contents,
                         "document.getElementById('primary-button').hidden"));
    EXPECT_EQ(l10n_util::GetStringUTF8(IDS_CONTINUE),
              content::EvalJs(
                  web_contents,
                  "document.getElementById('primary-button').textContent"));

    // Verify clicking #primary-button invokes the C++ errorPageController
    // portalSigninButtonClick().
    base::HistogramTester histograms;
#if BUILDFLAG(IS_ANDROID)
    base::test::TestFuture<content::WebContents*> sign_in_future;
    auto* tab_helper = enterprise_net::EnterpriseProxyTabHelper::From(
        tabs::TabInterface::MaybeGetFromContents(web_contents));
    ASSERT_TRUE(tab_helper);
    tab_helper->SetDelegateForTesting(
        std::make_unique<TestTabHelperDelegate>(sign_in_future.GetCallback()));
#endif  // BUILDFLAG(IS_ANDROID)

    EXPECT_TRUE(content::ExecJs(
        web_contents, "document.getElementById('primary-button').click();"));

#if BUILDFLAG(IS_ANDROID)
    EXPECT_EQ(web_contents, sign_in_future.Get());
#endif  // BUILDFLAG(IS_ANDROID)
    content::FetchHistogramsFromChildProcesses();
    metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
    histograms.ExpectBucketCount(
        "Net.ErrorPageCounts",
        error_page::NETWORK_ERROR_PORTAL_SIGNIN_BUTTON_CLICKED, 1);
  }

  // Validates that the rendered error page in `web_contents` matches the
  // Authorization error state (category 1).
  void VerifyAuthorizationErrorPage(content::WebContents* web_contents,
                                    const GURL& destination_url) {
    ASSERT_TRUE(web_contents);

    EXPECT_EQ(
        true,
        content::EvalJs(
            web_contents,
            "document.body.classList.contains('enterprise-proxy-error')"));
    EXPECT_EQ(
        false,
        content::EvalJs(
            web_contents,
            "document.body.classList.contains('category-authentication')"));
    EXPECT_EQ(
        true,
        content::EvalJs(
            web_contents,
            "document.body.classList.contains('category-authorization')"));
    EXPECT_EQ(false, content::EvalJs(
                         web_contents,
                         "document.body.classList.contains('category-other')"));

    EXPECT_EQ("1", content::EvalJs(web_contents,
                                   "window.loadTimeDataRaw['error_category']"));

    const std::string expected_heading =
        l10n_util::GetStringUTF8(IDS_ENTERPRISE_PROXY_AUTHZ_ERROR_HEADING);
    EXPECT_EQ(destination_url.host(),
              content::EvalJs(web_contents, "document.title"));
    EXPECT_EQ(expected_heading,
              content::EvalJs(
                  web_contents,
                  "document.querySelector('#main-message h1').textContent"));
    EXPECT_EQ(l10n_util::GetStringFUTF8(
                  IDS_ENTERPRISE_PROXY_AUTHZ_ERROR_PRIMARY_PARAGRAPH,
                  base::UTF8ToUTF16(destination_url.spec())),
              content::EvalJs(
                  web_contents,
                  "document.querySelector('#main-message p').textContent"));

    EXPECT_EQ(false, content::EvalJs(
                         web_contents,
                         "document.getElementById('primary-button').hidden"));
    EXPECT_EQ(l10n_util::GetStringUTF8(IDS_ENTERPRISE_BLOCK_GO_BACK),
              content::EvalJs(
                  web_contents,
                  "document.getElementById('primary-button').textContent"));
  }

  // Validates that the rendered error page in `web_contents` matches the
  // Other error state (category 2).
  void VerifyOtherErrorPage(content::WebContents* web_contents) {
    ASSERT_TRUE(web_contents);

    EXPECT_EQ(
        true,
        content::EvalJs(
            web_contents,
            "document.body.classList.contains('enterprise-proxy-error')"));
    EXPECT_EQ(
        false,
        content::EvalJs(
            web_contents,
            "document.body.classList.contains('category-authentication')"));
    EXPECT_EQ(
        false,
        content::EvalJs(
            web_contents,
            "document.body.classList.contains('category-authorization')"));
    EXPECT_EQ(true, content::EvalJs(
                        web_contents,
                        "document.body.classList.contains('category-other')"));

    EXPECT_EQ("2", content::EvalJs(web_contents,
                                   "window.loadTimeDataRaw['error_category']"));

    const std::string expected_heading =
        l10n_util::GetStringUTF8(IDS_ENTERPRISE_PROXY_OTHER_ERROR_HEADING);
    EXPECT_EQ(kDestinationHost,
              content::EvalJs(web_contents, "document.title"));
    EXPECT_EQ(expected_heading,
              content::EvalJs(
                  web_contents,
                  "document.querySelector('#main-message h1').textContent"));
    EXPECT_EQ(l10n_util::GetStringUTF8(
                  IDS_ENTERPRISE_PROXY_OTHER_ERROR_PRIMARY_PARAGRAPH),
              content::EvalJs(
                  web_contents,
                  "document.querySelector('#main-message p').textContent"));

    EXPECT_EQ(false, content::EvalJs(
                         web_contents,
                         "document.getElementById('primary-button').hidden"));
    EXPECT_EQ(l10n_util::GetStringUTF8(IDS_ENTERPRISE_BLOCK_GO_BACK),
              content::EvalJs(
                  web_contents,
                  "document.getElementById('primary-button').textContent"));
  }

  // Helper to verify PvD fetch failure transitions the domain to
  // `expected_state`.
  void VerifyPvdFetchFailure(
      net::HttpStatusCode code,
      const std::string& content,
      const std::string& content_type,
      enterprise_net::ProvisioningDomainProxyConfig::State expected_state) {
    SetPvdResponseOverride(code, content, content_type);

    base::ListValue domains;
    domains.Append(CreateDomainPolicyEntry(kTestPvdDomain));
    SetUserProxyProvisioningDomains(std::move(domains));

    auto* service = GetEnterpriseProxyService();
    ASSERT_TRUE(service);
    ASSERT_TRUE(base::test::RunUntil([service, expected_state]() {
      auto configs = service->GetProvisioningDomainConfigs();
      return !configs.empty() && configs[0].state == expected_state;
    }));

    auto pause_entries = net_log_observer().GetEntriesWithType(
        net::NetLogEventType::ENTERPRISE_PROXY_NETWORK_PAUSE);
    ASSERT_GE(pause_entries.size(), 2u);
    EXPECT_EQ(net::NetLogEventPhase::BEGIN, pause_entries[0].phase);
    EXPECT_EQ(net::NetLogEventPhase::END, pause_entries[1].phase);
  }

  // Navigates `web_contents` to `initial_url` and waits until a navigation to
  // `expected_target_url` finishes loading. Returns whether that target
  // navigation succeeded.
  bool NavigateAndWaitForTargetUrl(content::WebContents* web_contents,
                                   const GURL& initial_url,
                                   const GURL& expected_target_url) {
    content::TestNavigationObserver observer(expected_target_url);
    observer.WatchExistingWebContents();
    web_contents->GetController().LoadURL(initial_url, content::Referrer(),
                                          ui::PAGE_TRANSITION_TYPED,
                                          /*extra_headers=*/std::string());
    observer.Wait();
    return observer.last_navigation_succeeded();
  }

  // Verifies `Enterprise.SecureGateway.ErrorPage.Event{Category}` counts: when
  // `expected_category` is set, expects 1 `kShown` sample on the aggregate and
  // matching category histograms (and 0 on the other categories); when
  // `std::nullopt`, expects 0 samples across all four.
  void VerifyErrorPageShownHistograms(
      const base::HistogramTester& histogram_tester,
      std::optional<enterprise_net::EnterpriseProxyErrorData::ErrorCategory>
          expected_category) {
    using ErrorCategory =
        enterprise_net::EnterpriseProxyErrorData::ErrorCategory;
    if (expected_category.has_value()) {
      histogram_tester.ExpectUniqueSample(
          "Enterprise.SecureGateway.ErrorPage.Event",
          enterprise_net::SecureGatewayErrorPageEvent::kShown, 1);
    } else {
      histogram_tester.ExpectTotalCount(
          "Enterprise.SecureGateway.ErrorPage.Event", 0);
    }

    constexpr std::pair<ErrorCategory, std::string_view> kCategories[] = {
        {ErrorCategory::kAuthentication,
         "Enterprise.SecureGateway.ErrorPage.Event.Authentication"},
        {ErrorCategory::kAuthorization,
         "Enterprise.SecureGateway.ErrorPage.Event.Authorization"},
        {ErrorCategory::kOther,
         "Enterprise.SecureGateway.ErrorPage.Event.Other"},
    };
    for (const auto& [category, histogram] : kCategories) {
      if (expected_category == category) {
        histogram_tester.ExpectUniqueSample(
            histogram, enterprise_net::SecureGatewayErrorPageEvent::kShown, 1);
      } else {
        histogram_tester.ExpectTotalCount(histogram, 0);
      }
    }
  }

  // Helper to verify that a disguised proxy error challenge aborts auth,
  // records NetLog events, and populates EnterpriseProxyErrorService with the
  // expected category.
  void VerifyDisguisedProxyError(
      const std::string& realm,
      int expected_error_code,
      enterprise_net::EnterpriseProxyErrorData::ErrorCategory
          expected_category) {
    SetProxyChallengeRealm(realm);

    base::ListValue domains;
    domains.Append(CreateDomainPolicyEntry(kTestPvdDomain));
    SetUserProxyProvisioningDomains(std::move(domains));

    WaitForDynamicRoutesReady();

    base::HistogramTester histogram_tester;
    GURL destination_url = GetOriginURL(kDestinationHost, "/simple.html");
    content::WebContents* web_contents =
        chrome_test_utils::GetActiveWebContents(this);
    EXPECT_FALSE(
        chrome_test_utils::NavigateToURL(web_contents, destination_url));

    VerifyErrorPageShownHistograms(histogram_tester, expected_category);

    EXPECT_TRUE(was_proxy_accessed());
    EXPECT_FALSE(was_auth_header_received());

    auto received_entries = net_log_observer().GetEntriesWithType(
        net::NetLogEventType::ENTERPRISE_PROXY_AUTH_CHALLENGE_RECEIVED);
    ASSERT_GE(received_entries.size(), 1u);
    EXPECT_EQ(destination_url.spec(),
              *received_entries[0].params.FindString("destination_url"));

    auto resolved_entries = net_log_observer().GetEntriesWithType(
        net::NetLogEventType::ENTERPRISE_PROXY_AUTH_CHALLENGE_RESOLVED);
    ASSERT_GE(resolved_entries.size(), 1u);
    EXPECT_EQ("disguised_error",
              *resolved_entries[0].params.FindString("decision"));
    EXPECT_EQ(received_entries[0].source.id, resolved_entries[0].source.id);

    auto saved_entries = net_log_observer().GetEntriesWithType(
        net::NetLogEventType::ENTERPRISE_PROXY_DISGUISED_ERROR_SAVED);
    ASSERT_GE(saved_entries.size(), 1u);
    EXPECT_EQ(destination_url.spec(),
              *saved_entries[0].params.FindString("destination_url"));
    EXPECT_EQ(expected_error_code,
              *saved_entries[0].params.FindInt("error_code"));

    auto* error_service = GetEnterpriseProxyErrorService();
    ASSERT_TRUE(error_service);
    enterprise_net::EnterpriseProxyErrorData error_data(
        destination_url, default_proxy_server().GetURL(), expected_error_code,
        expected_category);
    base::DictValue params = error_service->GetErrorPageParams(error_data);
    EXPECT_TRUE(*params.FindBool("is_enterprise_proxy_error"));
    EXPECT_EQ(default_proxy_server().GetURL().spec(),
              *params.FindString("proxy_url"));
    EXPECT_EQ(base::NumberToString(expected_error_code),
              *params.FindString("error_code"));
    EXPECT_EQ(base::NumberToString(static_cast<int>(expected_category)),
              *params.FindString("error_category"));
    EXPECT_EQ(
        default_proxy_server().GetURL().spec(),
        content::EvalJs(web_contents, "window.loadTimeDataRaw['proxy_url']"));
  }
};

// Verifies that a 500 Internal Server Error response during PvD fetch causes
// the domain to enter a transient failure state.
IN_PROC_BROWSER_TEST_F(EnterpriseProxyErrorBrowserTest,
                       PvdFetchFailure_500InternalServerError) {
  VerifyPvdFetchFailure(
      net::HttpStatusCode::HTTP_INTERNAL_SERVER_ERROR, "Internal Server Error",
      "text/plain",
      enterprise_net::ProvisioningDomainProxyConfig::State::kFailedTransient);
}

// Verifies that a 404 Not Found response during PvD fetch causes the domain
// to enter a blocked failure state.
IN_PROC_BROWSER_TEST_F(EnterpriseProxyErrorBrowserTest,
                       PvdFetchFailure_404NotFound) {
  VerifyPvdFetchFailure(
      net::HttpStatusCode::HTTP_NOT_FOUND, "Not Found", "text/plain",
      enterprise_net::ProvisioningDomainProxyConfig::State::kFailedBlocked);
}

// Verifies that a malformed JSON response during PvD fetch transitions the
// domain state to blocked without crashing the browser process.
IN_PROC_BROWSER_TEST_F(EnterpriseProxyErrorBrowserTest,
                       PvdFetchFailure_MalformedJson) {
  VerifyPvdFetchFailure(
      net::HttpStatusCode::HTTP_OK, "{ invalid: json syntax ... ",
      "application/json",
      enterprise_net::ProvisioningDomainProxyConfig::State::kFailedBlocked);
}

// Verifies that when the primary account credentials are invalid, token fetch
// fails and proxy authentication correctly rejects the request.
IN_PROC_BROWSER_TEST_F(EnterpriseProxyErrorBrowserTest,
                       ProxyAuthenticationFailure_InvalidCredentials) {
  InvalidatePrimaryAccountCredentials();

  base::ListValue domains;
  domains.Append(CreateDomainPolicyEntry(kTestPvdDomain));
  SetUserProxyProvisioningDomains(std::move(domains));

  WaitForDynamicRoutesReady();

  base::HistogramTester histogram_tester;
  GURL destination_url = GetOriginURL(kDestinationHost, "/simple.html");
  content::WebContents* web_contents =
      chrome_test_utils::GetActiveWebContents(this);
  EXPECT_FALSE(chrome_test_utils::NavigateToURL(web_contents, destination_url));
  EXPECT_GE(GetTokenFetchRequestCount(), 1u);
  EXPECT_EQ(0u, origin_request_count());

  VerifyErrorPageShownHistograms(
      histogram_tester,
      enterprise_net::EnterpriseProxyErrorData::ErrorCategory::kAuthentication);

  // Verify NetLog events:
  auto received_entries = net_log_observer().GetEntriesWithType(
      net::NetLogEventType::ENTERPRISE_PROXY_AUTH_CHALLENGE_RECEIVED);
  ASSERT_GE(received_entries.size(), 1u);
  EXPECT_EQ(destination_url.spec(),
            *received_entries[0].params.FindString("destination_url"));

  auto resolved_entries = net_log_observer().GetEntriesWithType(
      net::NetLogEventType::ENTERPRISE_PROXY_AUTH_CHALLENGE_RESOLVED);
  ASSERT_GE(resolved_entries.size(), 1u);
  EXPECT_EQ("sign_in_required",
            *resolved_entries[0].params.FindString("decision"));
  EXPECT_EQ(received_entries[0].source.id, resolved_entries[0].source.id);

  auto saved_entries = net_log_observer().GetEntriesWithType(
      net::NetLogEventType::ENTERPRISE_PROXY_DISGUISED_ERROR_SAVED);
  ASSERT_GE(saved_entries.size(), 1u);
  EXPECT_EQ(destination_url.spec(),
            *saved_entries[0].params.FindString("destination_url"));

  // Verify error page params for authentication failure (category 0).
  auto* error_service = GetEnterpriseProxyErrorService();
  ASSERT_TRUE(error_service);
  enterprise_net::EnterpriseProxyErrorData error_data(
      destination_url, default_proxy_server().GetURL(),
      /*error_code=*/407,
      enterprise_net::EnterpriseProxyErrorData::ErrorCategory::kAuthentication);
  base::DictValue params = error_service->GetErrorPageParams(error_data);
  EXPECT_TRUE(*params.FindBool("is_enterprise_proxy_error"));
  EXPECT_EQ(default_proxy_server().GetURL().spec(),
            *params.FindString("proxy_url"));
  EXPECT_EQ("0", *params.FindString("error_category"));

  VerifyAuthenticationErrorPage(web_contents);
}

// Verifies that receiving a disguised error HTTP 407 challenge (realm="403")
// records the error with ErrorCategory::kAuthorization without attempting token
// fetch.
IN_PROC_BROWSER_TEST_F(EnterpriseProxyErrorBrowserTest,
                       DisguisedProxyError_403Forbidden) {
  VerifyDisguisedProxyError(
      "403", 403,
      enterprise_net::EnterpriseProxyErrorData::ErrorCategory::kAuthorization);
  VerifyAuthorizationErrorPage(chrome_test_utils::GetActiveWebContents(this),
                               GetOriginURL(kDestinationHost, "/simple.html"));
}

// Verifies that receiving a disguised error HTTP 407 challenge with a 502
// Bad Gateway realm categorizes the error as ErrorCategory::kOther.
IN_PROC_BROWSER_TEST_F(EnterpriseProxyErrorBrowserTest,
                       DisguisedProxyError_502BadGateway) {
  VerifyDisguisedProxyError(
      "502", 502,
      enterprise_net::EnterpriseProxyErrorData::ErrorCategory::kOther);
  VerifyOtherErrorPage(chrome_test_utils::GetActiveWebContents(this));
}

// Verifies that an unrecognized realm (such as "429") is not treated as a
// disguised error, but rather as a standard proxy auth challenge for which
// credentials are acquired and navigation succeeds.
IN_PROC_BROWSER_TEST_F(EnterpriseProxyErrorBrowserTest,
                       UnrecognizedRealm_TreatedAsStandardChallenge) {
  SetProxyChallengeRealm("429");

  base::ListValue domains;
  domains.Append(CreateDomainPolicyEntry(kTestPvdDomain));
  SetUserProxyProvisioningDomains(std::move(domains));

  WaitForDynamicRoutesReady();

  base::HistogramTester histogram_tester;
  GURL destination_url = GetOriginURL(kDestinationHost, "/simple.html");
  EXPECT_TRUE(chrome_test_utils::NavigateToURL(
      chrome_test_utils::GetActiveWebContents(this), destination_url));
  VerifyErrorPageShownHistograms(histogram_tester, std::nullopt);

  EXPECT_TRUE(was_proxy_accessed());
  EXPECT_TRUE(was_auth_header_received());

  auto resolved_entries = net_log_observer().GetEntriesWithType(
      net::NetLogEventType::ENTERPRISE_PROXY_AUTH_CHALLENGE_RESOLVED);
  ASSERT_GE(resolved_entries.size(), 1u);
  EXPECT_EQ("token_acquired",
            *resolved_entries[0].params.FindString("decision"));

  // Verify no disguised error was saved.
  auto saved_entries = net_log_observer().GetEntriesWithType(
      net::NetLogEventType::ENTERPRISE_PROXY_DISGUISED_ERROR_SAVED);
  EXPECT_TRUE(saved_entries.empty());
}

// Verifies end-to-end handling of the `url=` static error page parameter on
// disguised 407 challenges:
// 1. Valid HTTPS `url=` returned after OAuth authentication redirects the tab
//    to the static error page instead of rendering the built-in interstitial.
// 2. Non-HTTPS `url=` (e.g. `http://`) is rejected and falls back to the
//    built-in Authorization (403) error interstitial.
// 3. When the `url=` target itself routes through the proxy and returns another
//    disguised-error `url=`, loop protection stops after one redirect and
//    renders the built-in Authorization (403) error interstitial.
IN_PROC_BROWSER_TEST_F(EnterpriseProxyErrorBrowserTest,
                       DisguisedProxyError_StaticErrorPageRedirectAndFallback) {
  base::ListValue domains;
  domains.Append(CreateDomainPolicyEntry(kTestPvdDomain));
  SetUserProxyProvisioningDomains(std::move(domains));
  WaitForDynamicRoutesReady();

  content::WebContents* web_contents =
      chrome_test_utils::GetActiveWebContents(this);
  const GURL destination_url = GetOriginURL(kDestinationHost, "/simple.html");

  // Case #1: 2-round proxy flow (407 auth -> OAuth token -> 407 disguised 403
  // with `url=` and `message=`) redirects to a static HTTPS error page that is
  // same-site (`example.com`) with `default_proxy_server_`
  // (`proxy1.example.com`), preserving Back history on a gestureless previous
  // page while stripping `SameSite=Strict` cookies and `Referer` and sending
  // `Sec-Fetch-Site: cross-site`.
  {
    const GURL initial_url = GetOriginURL(kUnmatchedHost, "/direct.html?init");
    ASSERT_TRUE(chrome_test_utils::NavigateToURL(web_contents, initial_url));

    const GURL static_blocked_url =
        GetOriginURL(kAddedDestinationHost, "/direct.html?txid=test-txid-1");
    ASSERT_TRUE(content::SetCookie(
        web_contents->GetBrowserContext(), static_blocked_url,
        "strict_cookie=secret; SameSite=Strict; Secure; Domain=example.com; "
        "Path=/"));
    ASSERT_TRUE(content::SetCookie(
        web_contents->GetBrowserContext(), static_blocked_url,
        "lax_cookie=allowed; SameSite=Lax; Secure; Domain=example.com; "
        "Path=/"));

    default_proxy_server().SetPostAuthChallengeHeader(base::StringPrintf(
        "Basic realm=\"403\",url=\"%s\","
        "message=\"Your company's policy restricts access to this content.\"",
        static_blocked_url.spec().c_str()));

    base::HistogramTester histogram_tester;
    EXPECT_TRUE(NavigateAndWaitForTargetUrl(web_contents, destination_url,
                                            static_blocked_url));
    EXPECT_EQ(static_blocked_url, web_contents->GetLastCommittedURL());
    EXPECT_TRUE(web_contents->GetController().CanGoBack());
    ASSERT_NE(nullptr, web_contents->GetController().GetEntryAtOffset(-1));
    EXPECT_EQ(initial_url,
              web_contents->GetController().GetEntryAtOffset(-1)->GetURL());

    const net::test_server::HttpRequest::HeaderMap headers =
        last_origin_request_headers();
    ASSERT_TRUE(headers.contains("Sec-Fetch-Site"));
    EXPECT_EQ("cross-site", headers.at("Sec-Fetch-Site"));
    EXPECT_FALSE(headers.contains("Referer"));
    ASSERT_TRUE(headers.contains("Cookie"));
    EXPECT_NE(std::string::npos, headers.at("Cookie").find("lax_cookie="));
    EXPECT_EQ(std::string::npos, headers.at("Cookie").find("strict_cookie="));

    EXPECT_TRUE(was_proxy_accessed());
    EXPECT_TRUE(was_auth_header_received());
    VerifyErrorPageShownHistograms(histogram_tester, std::nullopt);
    histogram_tester.ExpectUniqueSample(
        "Enterprise.SecureGateway.StaticErrorPageRedirectOutcome",
        enterprise_net::SecureGatewayStaticErrorPageRedirectOutcome::
            kRedirected,
        1);

    auto saved_entries = net_log_observer().GetEntriesWithType(
        net::NetLogEventType::ENTERPRISE_PROXY_DISGUISED_ERROR_SAVED);
    ASSERT_GE(saved_entries.size(), 1u);
    const std::string* logged_redirect =
        saved_entries.back().params.FindString("redirect_url");
    ASSERT_TRUE(logged_redirect);
    EXPECT_EQ(static_blocked_url.spec(), *logged_redirect);
  }

  // Case #2: Unsafe non-HTTPS `url=` (`http://...`) is ignored and falls back
  // to the built-in Authorization error interstitial.
  {
    default_proxy_server().ResetStats();
    default_proxy_server().SetPostAuthChallengeHeader("");
    default_proxy_server().SetChallengeHeader(
        "Basic realm=\"403\","
        "url=\"http://unmatched.foo.com/blocked.html?txid=unsafe\"");

    base::HistogramTester histogram_tester;
    EXPECT_FALSE(
        chrome_test_utils::NavigateToURL(web_contents, destination_url));
    VerifyErrorPageShownHistograms(histogram_tester,
                                   enterprise_net::EnterpriseProxyErrorData::
                                       ErrorCategory::kAuthorization);
    histogram_tester.ExpectUniqueSample(
        "Enterprise.SecureGateway.StaticErrorPageRedirectOutcome",
        enterprise_net::SecureGatewayStaticErrorPageRedirectOutcome::
            kRejectedInvalidUrl,
        1);
    VerifyAuthorizationErrorPage(web_contents, destination_url);
  }

  // Case #3: Static `url=` that itself matches the proxy route and triggers
  // another disguised-error `url=` redirects once and then falls back to the
  // built-in Authorization error interstitial instead of looping infinitely.
  {
    default_proxy_server().ResetStats();
    const GURL looping_error_url =
        GetOriginURL(kDestinationSubdomainHost, "/simple.html?txid=loop");
    default_proxy_server().SetChallengeHeader(base::StringPrintf(
        "Basic realm=\"403\",url=\"%s\"", looping_error_url.spec().c_str()));

    base::HistogramTester histogram_tester;
    EXPECT_FALSE(NavigateAndWaitForTargetUrl(web_contents, destination_url,
                                             looping_error_url));
    EXPECT_EQ(looping_error_url, web_contents->GetLastCommittedURL());
    VerifyErrorPageShownHistograms(histogram_tester,
                                   enterprise_net::EnterpriseProxyErrorData::
                                       ErrorCategory::kAuthorization);
    histogram_tester.ExpectBucketCount(
        "Enterprise.SecureGateway.StaticErrorPageRedirectOutcome",
        enterprise_net::SecureGatewayStaticErrorPageRedirectOutcome::
            kRedirected,
        1);
    histogram_tester.ExpectBucketCount(
        "Enterprise.SecureGateway.StaticErrorPageRedirectOutcome",
        enterprise_net::SecureGatewayStaticErrorPageRedirectOutcome::
            kRejectedLoopDetected,
        1);
    histogram_tester.ExpectTotalCount(
        "Enterprise.SecureGateway.StaticErrorPageRedirectOutcome", 2);
    VerifyAuthorizationErrorPage(web_contents, looping_error_url);
  }
}

// Verifies end-to-end that if a higher-precedence rule (such as
// ProxyOverrideRules or an extension override) routes a PvD-matched
// destination to a plain-HTTP proxy at the exact same host and port as an
// active `https-connect` PvD endpoint, its HTTP 407 challenge (including a
// disguised-error realm with a static `url=`) is rejected as
// `non_https_challenger` without fetching or sending an OAuth2 bearer token,
// showing a disguised error page, or redirecting the tab.
IN_PROC_BROWSER_TEST_F(EnterpriseProxyErrorBrowserTest,
                       PlainHttpProxyOverrideRejected) {
  GURL destination_url = GetOriginURL(kDestinationHost, "/simple.html");
  TestConnectProxy plain_http_proxy(
      kDefaultProxyHost, {net::HostPortPair::FromURL(destination_url)},
      "cloud_secure_gateway", net::EmbeddedTestServer::TYPE_HTTP);
  ASSERT_TRUE(plain_http_proxy.Start());

  const std::string shared_proxy_host_port =
      plain_http_proxy.host_port_pair().ToString();
  SetPvdResponseOverride(
      net::HTTP_OK,
      BuildValidPvdJson(kTestPvdDomain, shared_proxy_host_port,
                        {"*.example.com"}, /*require_auth=*/true));

  base::ListValue domains;
  domains.Append(CreateDomainPolicyEntry(kTestPvdDomain));
  SetUserProxyProvisioningDomains(std::move(domains));
  WaitForDynamicRoutesReady();

  base::ListValue override_rules;
  override_rules.Append(
      base::DictValue()
          .Set("DestinationMatchers",
               base::ListValue().Append(kDestinationHost))
          .Set("ProxyList",
               base::ListValue().Append("PROXY " + shared_proxy_host_port)));
  chrome_test_utils::GetProfile(this)->GetPrefs()->SetList(
      proxy_config::prefs::kProxyOverrideRules, std::move(override_rules));
  ProfileNetworkContextServiceFactory::GetForContext(
      chrome_test_utils::GetProfile(this))
      ->FlushProxyConfigMonitorForTesting();

  // Case #1: Standard 407 challenge over plain HTTP neither fetches nor sends a
  // bearer token.
  const size_t token_fetches_before = GetTokenFetchRequestCount();
  base::HistogramTester histogram_tester;
  content::WebContents* web_contents =
      chrome_test_utils::GetActiveWebContents(this);
  EXPECT_FALSE(chrome_test_utils::NavigateToURL(web_contents, destination_url));
  EXPECT_TRUE(plain_http_proxy.was_accessed());
  EXPECT_FALSE(plain_http_proxy.was_auth_header_received());
  EXPECT_EQ(token_fetches_before, GetTokenFetchRequestCount());

  // Case #2: Disguised-error realm ("403") carrying a valid HTTPS `url=` over
  // plain HTTP neither shows an Enterprise Proxy error page nor redirects the
  // tab to `url=`.
  plain_http_proxy.ResetStats();
  const GURL static_blocked_url =
      GetOriginURL(kUnmatchedHost, "/direct.html?txid=plain-http-spoof");
  plain_http_proxy.SetChallengeHeader(base::StringPrintf(
      "Basic realm=\"403\",url=\"%s\"", static_blocked_url.spec().c_str()));
  EXPECT_FALSE(chrome_test_utils::NavigateToURL(web_contents, destination_url));
  EXPECT_EQ(destination_url, web_contents->GetLastCommittedURL());
  EXPECT_TRUE(plain_http_proxy.was_accessed());
  VerifyErrorPageShownHistograms(histogram_tester, std::nullopt);
  EXPECT_TRUE(
      net_log_observer()
          .GetEntriesWithType(
              net::NetLogEventType::ENTERPRISE_PROXY_DISGUISED_ERROR_SAVED)
          .empty());

  histogram_tester.ExpectUniqueSample(
      "Enterprise.SecureGateway.ProxyAuthChallengeDecision",
      enterprise_net::EnterpriseProxyService::ProxyAuthChallengeMatch::
          Decision::kNotApplicable,
      2);
  histogram_tester.ExpectTotalCount(
      "Enterprise.SecureGateway.ProxyAuthCredentialFetchOutcome", 0);

  std::vector<net::NetLogEntry> resolved_entries =
      net_log_observer().GetEntriesWithType(
          net::NetLogEventType::ENTERPRISE_PROXY_AUTH_CHALLENGE_RESOLVED);
  ASSERT_EQ(2u, resolved_entries.size());
  for (const net::NetLogEntry& entry : resolved_entries) {
    EXPECT_EQ("not_applicable", *entry.params.FindString("decision"));
    EXPECT_EQ("non_https_challenger",
              *entry.params.FindString("failure_reason"));
  }

  // Case #3: POR's plain-HTTP route itself continues to forward traffic
  // normally without interference from the overlapping `https-connect` PvD.
  plain_http_proxy.ResetStats();
  plain_http_proxy.SetRequireAuth(false);
  EXPECT_TRUE(chrome_test_utils::NavigateToURL(web_contents, destination_url));
  EXPECT_TRUE(plain_http_proxy.was_accessed());
  EXPECT_FALSE(was_proxy_accessed());
  EXPECT_EQ(token_fetches_before, GetTokenFetchRequestCount());
}

// Verifies domain allowlist enforcement end-to-end: if a PvD endpoint
// advertises a proxy on an untrusted domain (`untrusted.evil.com`), HTTP 407
// challenges from that proxy are rejected with `inapplicable_server` before
// touching `IdentityManager`, never leaking an OAuth2 token in
// `Proxy-Authorization`.
IN_PROC_BROWSER_TEST_F(EnterpriseProxyErrorBrowserTest,
                       RejectsTokenFetchForDisallowedProxyDomain) {
  std::unique_ptr<TestConnectProxy> untrusted_proxy =
      CreateProxyServer("untrusted.evil.com");
  ASSERT_TRUE(untrusted_proxy);

  SetPvdJsonForDomain(
      kTestPvdDomain,
      BuildValidPvdJson(kTestPvdDomain,
                        untrusted_proxy->host_port_pair().ToString(),
                        {kDestinationHost}, /*require_auth=*/true));

  base::ListValue domains;
  domains.Append(CreateDomainPolicyEntry(kTestPvdDomain, /*use_oauth=*/false));
  SetUserProxyProvisioningDomains(std::move(domains));
  WaitForDynamicRoutesReady();

  content::WebContents* web_contents =
      chrome_test_utils::GetActiveWebContents(this);
  GURL destination_url = GetOriginURL(kDestinationHost, "/simple.html");
  base::HistogramTester histogram_tester;
  EXPECT_FALSE(chrome_test_utils::NavigateToURL(web_contents, destination_url));

  EXPECT_TRUE(untrusted_proxy->was_accessed());
  EXPECT_FALSE(untrusted_proxy->was_auth_header_received());
  EXPECT_EQ(0u, GetTokenFetchRequestCount());
  EXPECT_EQ(0u, origin_request_count());

  std::vector<net::NetLogEntry> resolved_entries =
      net_log_observer().GetEntriesWithType(
          net::NetLogEventType::ENTERPRISE_PROXY_AUTH_CHALLENGE_RESOLVED);
  ASSERT_GE(resolved_entries.size(), 1u);
  const std::string* decision =
      resolved_entries[0].params.FindString("decision");
  ASSERT_NE(nullptr, decision);
  EXPECT_EQ("failed", *decision);
  const std::string* failure_reason =
      resolved_entries[0].params.FindString("failure_reason");
  ASSERT_NE(nullptr, failure_reason);
  EXPECT_EQ("inapplicable_server", *failure_reason);

  VerifyOtherErrorPage(web_contents);
  VerifyErrorPageShownHistograms(
      histogram_tester,
      enterprise_net::EnterpriseProxyErrorData::ErrorCategory::kOther);
}

// Verifies that when an enterprise IdP login page (`kIdpHost`) and protected
// destination (`kDestinationHost`) both match proxy routing rules while primary
// account credentials are invalid, navigations to the IdP page cannot reach the
// origin and surface the Authentication error page (`sign_in_required`), and
// once valid credentials are restored, subsequent navigations acquire a token
// and succeed through the proxy.
IN_PROC_BROWSER_TEST_F(EnterpriseProxyErrorBrowserTest,
                       InvalidCredentialsBlocksIdpAndRecoversAfterRestore) {
  InvalidatePrimaryAccountCredentials();

  SetPvdJsonForDomain(
      kTestPvdDomain,
      BuildValidPvdJson(kTestPvdDomain,
                        default_proxy_server().host_port_pair().ToString(),
                        {kDestinationHost, kIdpHost},
                        /*require_auth=*/true));

  base::ListValue domains;
  domains.Append(CreateDomainPolicyEntry(kTestPvdDomain, /*use_oauth=*/false));
  SetUserProxyProvisioningDomains(std::move(domains));
  WaitForDynamicRoutesReady();

  content::WebContents* web_contents =
      chrome_test_utils::GetActiveWebContents(this);
  GURL idp_url = GetIdpURL("/idp/login");
  EXPECT_FALSE(chrome_test_utils::NavigateToURL(web_contents, idp_url));
  EXPECT_EQ(0u, idp_page_request_count());
  EXPECT_TRUE(was_proxy_accessed());
  EXPECT_FALSE(was_auth_header_received());
  EXPECT_EQ(true,
            content::EvalJs(
                web_contents,
                "document.body.classList.contains('category-authentication')"));

  std::vector<net::NetLogEntry> resolved_entries =
      net_log_observer().GetEntriesWithType(
          net::NetLogEventType::ENTERPRISE_PROXY_AUTH_CHALLENGE_RESOLVED);
  ASSERT_GE(resolved_entries.size(), 1u);
  EXPECT_EQ("sign_in_required",
            *resolved_entries[0].params.FindString("decision"));

  // Restoring primary account credentials allows both IdP and protected
  // destination navigations to authenticate and succeed through the proxy.
  default_proxy_server().ResetStats();
  RestorePrimaryAccountCredentials();

  EXPECT_TRUE(chrome_test_utils::NavigateToURL(web_contents, idp_url));
  EXPECT_EQ(1u, idp_page_request_count());
  EXPECT_TRUE(was_proxy_accessed());
  EXPECT_TRUE(was_auth_header_received());

  GURL destination_url = GetOriginURL(kDestinationHost, "/simple.html");
  EXPECT_TRUE(chrome_test_utils::NavigateToURL(web_contents, destination_url));
  EXPECT_EQ(1u, origin_request_count());
}

// Verifies end-to-end `#primary-button` recovery when both Gaia (`kGaiaHost`)
// and IdP (`kIdpHost`) match the 407 proxy routing rules: clicking Continue
// invokes `EnterpriseProxyTabHelperDelegate::SignIn` -> OS
// `AccountManagerFacade.updateCredentials`, restores valid tokens, completes
// `/ListAccounts` through the authenticated proxy, and redirects the tab to
// `destination_url`.
// TODO(crbug.com/532559920): Enable on Desktop once
// `EnterpriseProxyTabHelperDelegate` implements Desktop re-auth and web
// sign-in/token flows bypass proxy authentication loops.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_ReauthButtonRecoversEvenWhenGaiaRoutedThroughProxy \
  ReauthButtonRecoversEvenWhenGaiaRoutedThroughProxy
#else
#define MAYBE_ReauthButtonRecoversEvenWhenGaiaRoutedThroughProxy \
  DISABLED_ReauthButtonRecoversEvenWhenGaiaRoutedThroughProxy
#endif
IN_PROC_BROWSER_TEST_F(
    EnterpriseProxyErrorBrowserTest,
    MAYBE_ReauthButtonRecoversEvenWhenGaiaRoutedThroughProxy) {
  InvalidatePrimaryAccountCredentials();

  SetPvdJsonForDomain(
      kTestPvdDomain,
      BuildValidPvdJson(kTestPvdDomain,
                        default_proxy_server().host_port_pair().ToString(),
                        {kDestinationHost, kIdpHost, kGaiaHost},
                        /*require_auth=*/true));

  base::ListValue domains;
  domains.Append(CreateDomainPolicyEntry(kTestPvdDomain, /*use_oauth=*/false));
  SetUserProxyProvisioningDomains(std::move(domains));
  WaitForDynamicRoutesReady();

  content::WebContents* web_contents =
      chrome_test_utils::GetActiveWebContents(this);
  GURL destination_url = GetOriginURL(kDestinationHost, "/simple.html");
  EXPECT_FALSE(chrome_test_utils::NavigateToURL(web_contents, destination_url));
  EXPECT_EQ(true,
            content::EvalJs(
                web_contents,
                "document.body.classList.contains('category-authentication')"));
  EXPECT_EQ(0u, origin_request_count());

  signin::CancelAllOngoingGaiaCookieOperations(GetIdentityManager());
  signin::SetFreshnessOfAccountsInGaiaCookie(GetIdentityManager(),
                                             /*accounts_are_fresh=*/false);
  default_proxy_server().ResetStats();

  content::TestNavigationObserver redirect_observer(destination_url);
  redirect_observer.WatchExistingWebContents();
  ASSERT_TRUE(content::ExecJs(
      web_contents, "document.getElementById('primary-button').click();"));
  redirect_observer.Wait();

  EXPECT_TRUE(redirect_observer.last_navigation_succeeded());
  EXPECT_EQ(1u, GetUpdateCredentialsCount());
  EXPECT_TRUE(was_proxy_accessed());
  EXPECT_TRUE(was_auth_header_received());
  EXPECT_THAT(default_proxy_server().tunneled_targets(),
              testing::Contains(testing::HasSubstr(kGaiaHost)));
  EXPECT_THAT(default_proxy_server().tunneled_targets(),
              testing::Contains(testing::HasSubstr(kDestinationHost)));
  EXPECT_EQ(1u, origin_request_count());
}

}  // namespace enterprise::test
