// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/run_until.h"
#include "base/values.h"
#include "chrome/browser/enterprise/net/enterprise_proxy_error_service_factory.h"
#include "chrome/browser/enterprise/net/test/enterprise_proxy_browsertest_base.h"
#include "chrome/browser/enterprise/test/management_context_mixin.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/enterprise/net/core/enterprise_proxy_error_data.h"
#include "components/enterprise/net/core/enterprise_proxy_error_service.h"
#include "components/enterprise/net/core/enterprise_proxy_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_test.h"
#include "net/http/http_status_code.h"
#include "net/log/net_log_event_type.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace enterprise::test {

class EnterpriseProxyErrorBrowserTest : public EnterpriseProxyBrowserTestBase {
 public:
  EnterpriseProxyErrorBrowserTest()
      : EnterpriseProxyBrowserTestBase(ManagementContext{
            .is_cloud_user_managed = true,
            .is_cloud_machine_managed = true,
            .affiliated = true,
        }) {}

 protected:
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
  EXPECT_FALSE(chrome_test_utils::NavigateToURL(
      chrome_test_utils::GetActiveWebContents(this), destination_url));

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
}

// Verifies that receiving a disguised error HTTP 407 challenge (realm="403")
// records the error with ErrorCategory::kAuthorization without attempting token
// fetch.
IN_PROC_BROWSER_TEST_F(EnterpriseProxyErrorBrowserTest,
                       DisguisedProxyError_403Forbidden) {
  VerifyDisguisedProxyError(
      "403", 403,
      enterprise_net::EnterpriseProxyErrorData::ErrorCategory::kAuthorization);
}

// Verifies that receiving a disguised error HTTP 407 challenge with a 502
// Bad Gateway realm categorizes the error as ErrorCategory::kOther.
IN_PROC_BROWSER_TEST_F(EnterpriseProxyErrorBrowserTest,
                       DisguisedProxyError_502BadGateway) {
  VerifyDisguisedProxyError(
      "502", 502,
      enterprise_net::EnterpriseProxyErrorData::ErrorCategory::kOther);
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
