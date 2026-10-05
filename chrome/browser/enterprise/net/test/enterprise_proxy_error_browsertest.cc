// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

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
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/enterprise/net/content/enterprise_proxy_tab_helper.h"
#include "components/enterprise/net/core/enterprise_proxy_error_data.h"
#include "components/enterprise/net/core/enterprise_proxy_error_service.h"
#include "components/enterprise/net/core/enterprise_proxy_service.h"
#include "components/error_page/common/net_error_info.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/strings/grit/components_strings.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/http/http_status_code.h"
#include "net/log/net_log_event_type.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
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
    EXPECT_EQ(kDestinationHost,
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

    GURL destination_url =
        https_server_.GetURL(kDestinationHost, "/simple.html");
    EXPECT_FALSE(chrome_test_utils::NavigateToURL(
        chrome_test_utils::GetActiveWebContents(this), destination_url));

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
        destination_url, https_server_.GetURL(kTestPvdDomain, "/"),
        expected_error_code, expected_category);
    base::DictValue params = error_service->GetErrorPageParams(error_data);
    EXPECT_TRUE(*params.FindBool("is_enterprise_proxy_error"));
    EXPECT_EQ(base::NumberToString(expected_error_code),
              *params.FindString("error_code"));
    EXPECT_EQ(base::NumberToString(static_cast<int>(expected_category)),
              *params.FindString("error_category"));
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
// to enter a permanent failure state.
IN_PROC_BROWSER_TEST_F(EnterpriseProxyErrorBrowserTest,
                       PvdFetchFailure_404NotFound) {
  VerifyPvdFetchFailure(
      net::HttpStatusCode::HTTP_NOT_FOUND, "Not Found", "text/plain",
      enterprise_net::ProvisioningDomainProxyConfig::State::kFailedPermanent);
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
  ASSERT_TRUE(identity_test_env());

  // Invalidate refresh token so that token fetching fails.
  identity_test_env()->SetAutomaticIssueOfAccessTokens(false);
  identity_test_env()->SetInvalidRefreshTokenForPrimaryAccount();

  base::ListValue domains;
  domains.Append(CreateDomainPolicyEntry(kTestPvdDomain));
  SetUserProxyProvisioningDomains(std::move(domains));

  WaitForDynamicRoutesReady();

  GURL destination_url = https_server_.GetURL(kDestinationHost, "/simple.html");
  content::WebContents* web_contents =
      chrome_test_utils::GetActiveWebContents(this);
  EXPECT_FALSE(chrome_test_utils::NavigateToURL(web_contents, destination_url));

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
      destination_url, https_server_.GetURL(kTestPvdDomain, "/"),
      /*error_code=*/407,
      enterprise_net::EnterpriseProxyErrorData::ErrorCategory::kAuthentication);
  base::DictValue params = error_service->GetErrorPageParams(error_data);
  EXPECT_TRUE(*params.FindBool("is_enterprise_proxy_error"));
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
  VerifyAuthorizationErrorPage(
      chrome_test_utils::GetActiveWebContents(this),
      https_server_.GetURL(kDestinationHost, "/simple.html"));
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

  GURL destination_url = https_server_.GetURL(kDestinationHost, "/simple.html");
  EXPECT_TRUE(chrome_test_utils::NavigateToURL(
      chrome_test_utils::GetActiveWebContents(this), destination_url));

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

}  // namespace enterprise::test
