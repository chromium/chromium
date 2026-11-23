// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base64.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/enterprise/net/test/enterprise_proxy_browsertest_base.h"
#include "chrome/browser/enterprise/test/management_context_mixin.h"
#include "chrome/browser/net/profile_network_context_service.h"
#include "chrome/browser/net/profile_network_context_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/enterprise/browser/identifiers/profile_id_service.h"
#include "components/enterprise/net/core/enterprise_proxy_service.h"
#include "components/enterprise/net/core/features.h"
#include "components/enterprise/net/core/prefs.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/proxy_config/proxy_prefs.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/http_request.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace enterprise::test {

class EnterpriseProxyBrowserTest : public EnterpriseProxyBrowserTestBase {
 public:
  EnterpriseProxyBrowserTest()
      : EnterpriseProxyBrowserTestBase(ManagementContext{
            .is_cloud_user_managed = true,
            .is_cloud_machine_managed = true,
            .affiliated = true,
        }) {}
};

// Verifies the complete E2E happy path with multiple Provisioning Domain
// configs:
// - Policy sets multiple Provisioning Domains, including two configs that match
//   the same destination (the first requires auth, the second does not).
// - PvD fetch completes and loads dynamic proxy routing rules for all domains.
// - Navigation to matching destination is routed to the first matching config's
//   proxy endpoint and requires authentication.
// - EnterpriseProxyService handles the challenge, acquires an OAuth access
//   token, formats Basic Auth credentials with ${profile_id} and bearer token.
// - Navigation succeeds with 200 OK.
IN_PROC_BROWSER_TEST_F(EnterpriseProxyBrowserTest,
                       AppliesDynamicRoutingAndAuthenticates) {
  base::ListValue domains;
  domains.Append(CreateDomainPolicyEntry(kTestPvdDomain, /*use_oauth=*/false));
  domains.Append(CreateDomainPolicyEntry(kUnauthenticatedPvdDomain,
                                         /*use_oauth=*/false));
  domains.Append(
      CreateDomainPolicyEntry(kSecondaryPvdDomain, /*use_oauth=*/false));
  SetUserProxyProvisioningDomains(std::move(domains));

  WaitForDynamicRoutesReady();

  auto* service = GetEnterpriseProxyService();
  ASSERT_TRUE(service);
  EXPECT_EQ(3u, service->GetProvisioningDomainConfigs().size());

  GURL destination_url = GetOriginURL(kDestinationHost, "/simple.html");
  EXPECT_TRUE(chrome_test_utils::NavigateToURL(
      chrome_test_utils::GetActiveWebContents(this), destination_url));

  EXPECT_TRUE(was_proxy_accessed());
  // Because the first matching config (`kTestPvdDomain`) requires auth, an auth
  // header is sent (whereas `kUnauthenticatedPvdDomain` would not send one).
  EXPECT_TRUE(was_auth_header_received());
  EXPECT_GE(GetTokenFetchRequestCount(), 1u);
  EXPECT_GE(origin_request_count(), 1u);

  // Verify the credentials format in Proxy-Authorization: Basic <base64>
  const std::string& auth_header = last_received_auth_header();
  ASSERT_TRUE(auth_header.starts_with("Basic "));
  std::string encoded_creds = auth_header.substr(6);
  std::string decoded_creds;
  ASSERT_TRUE(base::Base64Decode(encoded_creds, &decoded_creds));
  // decoded_creds is formatted as "<username>:<password>", where the username
  // contains resolved extra headers and password contains the access token.
  size_t colon_pos = decoded_creds.find(':');
  ASSERT_NE(std::string::npos, colon_pos);
  std::string username = decoded_creds.substr(0, colon_pos);
  std::string password = decoded_creds.substr(colon_pos + 1);
  EXPECT_EQ(password, kExpectedAccessToken);

  auto* profile_id_service = GetProfileIdService();
  ASSERT_TRUE(profile_id_service);
  std::optional<std::string> profile_id = profile_id_service->GetProfileId();
  ASSERT_TRUE(profile_id.has_value());
  EXPECT_EQ(username,
            base::StringPrintf("X-Profile-ID=%s",
                               base::EscapeQueryParamValue(*profile_id,
                                                           /*use_plus=*/true)
                                   .c_str()));

  // NetLog check: Network pause during PvD fetch / route loading.
  auto pause_entries = net_log_observer().GetEntriesWithType(
      net::NetLogEventType::ENTERPRISE_PROXY_NETWORK_PAUSE);
  ASSERT_GE(pause_entries.size(), 2u);
  EXPECT_EQ(net::NetLogEventPhase::BEGIN, pause_entries[0].phase);
  EXPECT_EQ(net::NetLogEventPhase::END, pause_entries[1].phase);

  // NetLog check: Auth challenge received and resolved with token_acquired.
  auto received_entries = net_log_observer().GetEntriesWithType(
      net::NetLogEventType::ENTERPRISE_PROXY_AUTH_CHALLENGE_RECEIVED);
  ASSERT_GE(received_entries.size(), 1u);
  EXPECT_EQ(destination_url.spec(),
            *received_entries[0].params.FindString("destination_url"));

  auto resolved_entries = net_log_observer().GetEntriesWithType(
      net::NetLogEventType::ENTERPRISE_PROXY_AUTH_CHALLENGE_RESOLVED);
  ASSERT_GE(resolved_entries.size(), 1u);
  EXPECT_EQ("token_acquired",
            *resolved_entries[0].params.FindString("decision"));
  EXPECT_EQ(received_entries[0].source.id, resolved_entries[0].source.id);
}

// Verifies both extra_headers expansion contracts end-to-end:
// 1. Policy `extra_headers` (configured with "key" and "value" without "type")
//    expand `${profile_id}` and `${accept_language}` (and drop unsupported
//    placeholders) when calling the PvD endpoint (`/.well-known/pvd`).
// 2. PvD response `extra-headers` (configured with "constant" and "variable"
//    entries) expand variables and placeholders (and drop unsupported
//    placeholders) into the Basic auth username when handling HTTP 407 proxy
//    authentication challenges.
IN_PROC_BROWSER_TEST_F(EnterpriseProxyBrowserTest,
                       ExpandsExtraHeadersInPolicyAndPvdResponse) {
  constexpr char kTestAcceptLanguages[] = "en-US,fr";
  Profile* profile = chrome_test_utils::GetProfile(this);
  ASSERT_TRUE(profile);
  profile->GetPrefs()->SetString("intl.selected_languages",
                                 kTestAcceptLanguages);
  profile->GetPrefs()->SetString("intl.accept_languages", kTestAcceptLanguages);

  auto* profile_id_service = GetProfileIdService();
  ASSERT_TRUE(profile_id_service);
  std::optional<std::string> profile_id = profile_id_service->GetProfileId();
  ASSERT_TRUE(profile_id.has_value());

  // Configure PvD response with both "constant" and "variable" extra-headers.
  std::string pvd_json = base::StringPrintf(
      R"({
        "identifier": "%s",
        "expires": "Wed, 21 Oct 2026 07:28:00 GMT",
        "proxies": [
          {
            "protocol": "https-connect",
            "identity": "proxy1",
            "proxy": "%s",
            "google_chrome": {
              "auth": {
                "type": "profile_bearer_token",
                "scope": "cloud_secure_gateway"
              },
              "extra-headers": [
                {
                  "key": "x-resource-key",
                  "constant": "pvd-resource-val"
                },
                {
                  "key": "x-profile-id",
                  "variable": "profileId"
                },
                {
                  "key": "x-profile-placeholder",
                  "variable": "pid-${profile_id}"
                },
                {
                  "key": "accept-language",
                  "variable": "acceptLanguage"
                },
                {
                  "key": "x-lang-placeholder",
                  "variable": "${accept_language}"
                },
                {
                  "key": "x-pvd-unsupported",
                  "variable": "${unsupported_var}"
                }
              ]
            }
          }
        ],
        "proxy-match": [
          {
            "proxies": ["proxy1"],
            "domains": ["%s"]
          }
        ]
      })",
      kTestPvdDomain,
      default_proxy_server().host_port_pair().ToString().c_str(),
      kDestinationHost);
  SetPvdResponseOverride(net::HttpStatusCode::HTTP_OK, pvd_json);

  // Configure policy extra_headers using the policy schema format ("key" and
  // "value" without "type").
  base::ListValue policy_extra_headers;
  policy_extra_headers.Append(base::DictValue()
                                  .Set("key", "x-resource-key")
                                  .Set("value", "policy-resource-val"));
  policy_extra_headers.Append(base::DictValue()
                                  .Set("key", "x-profile-id")
                                  .Set("value", "${profile_id}"));
  policy_extra_headers.Append(base::DictValue()
                                  .Set("key", "x-accept-language")
                                  .Set("value", "${accept_language}"));
  policy_extra_headers.Append(base::DictValue()
                                  .Set("key", "x-policy-unsupported")
                                  .Set("value", "${unsupported_var}"));

  base::ListValue domains;
  domains.Append(CreateDomainPolicyEntry(kTestPvdDomain, /*use_oauth=*/true,
                                         std::move(policy_extra_headers)));
  SetUserProxyProvisioningDomains(std::move(domains));

  WaitForDynamicRoutesReady();

  // 1. Verify policy extra_headers were expanded on the /.well-known/pvd fetch.
  const net::test_server::HttpRequest::HeaderMap pvd_headers =
      last_pvd_request_headers();
  auto auth_it = pvd_headers.find("Authorization");
  ASSERT_NE(auth_it, pvd_headers.end());
  EXPECT_EQ(auth_it->second,
            base::StringPrintf("Bearer %s", kExpectedAccessToken));

  auto resource_it = pvd_headers.find("x-resource-key");
  ASSERT_NE(resource_it, pvd_headers.end());
  EXPECT_EQ(resource_it->second, "policy-resource-val");

  auto profile_it = pvd_headers.find("x-profile-id");
  ASSERT_NE(profile_it, pvd_headers.end());
  EXPECT_EQ(profile_it->second, *profile_id);

  auto lang_it = pvd_headers.find("x-accept-language");
  ASSERT_NE(lang_it, pvd_headers.end());
  EXPECT_EQ(lang_it->second, kTestAcceptLanguages);

  EXPECT_EQ(pvd_headers.find("x-policy-unsupported"), pvd_headers.end());

  // 2. Navigate through the proxy and verify PvD response extra-headers are
  //    expanded in the Proxy-Authorization Basic auth challenge response.
  GURL destination_url = GetOriginURL(kDestinationHost, "/simple.html");
  EXPECT_TRUE(chrome_test_utils::NavigateToURL(
      chrome_test_utils::GetActiveWebContents(this), destination_url));

  EXPECT_TRUE(was_proxy_accessed());
  EXPECT_TRUE(was_auth_header_received());

  const std::string& auth_header = last_received_auth_header();
  ASSERT_TRUE(auth_header.starts_with("Basic "));
  std::string decoded_creds;
  ASSERT_TRUE(base::Base64Decode(auth_header.substr(6), &decoded_creds));
  size_t colon_pos = decoded_creds.find(':');
  ASSERT_NE(std::string::npos, colon_pos);
  std::string username = decoded_creds.substr(0, colon_pos);
  std::string password = decoded_creds.substr(colon_pos + 1);
  EXPECT_EQ(password, kExpectedAccessToken);

  auto escape = [](const std::string& val) {
    return base::EscapeQueryParamValue(val, /*use_plus=*/true);
  };
  std::vector<std::string> expected_params = {
      "x-resource-key=pvd-resource-val",
      "x-profile-id=" + escape(*profile_id),
      "x-profile-placeholder=" + escape("pid-" + *profile_id),
      "accept-language=" + escape(kTestAcceptLanguages),
      "x-lang-placeholder=" + escape(kTestAcceptLanguages),
  };
  EXPECT_EQ(username, base::JoinString(expected_params, "&"));
}

// Verifies fetching `/.well-known/pvd` with `auth_config.type =
// "profile_bearer_token"` (`use_oauth=true`) while dynamic route refresh holds
// the `NetworkService` pause (`is_update_in_progress = true`):
// - On Android, `ProfileOAuth2TokenServiceDelegateAndroid` fetches the OAuth2
//   token out-of-band via JNI `AccountManagerFacade`, bypassing Chrome's paused
//   `NetworkContext`, so `/.well-known/pvd` receives `Authorization: Bearer`
//   and navigation succeeds end-to-end.
// - On Desktop, `DesktopTestAuthHelper` currently issues tokens in-memory via
//   `IdentityTestEnvironmentProfileAdaptor` (see `TODO(crbug.com/532559920)`).
IN_PROC_BROWSER_TEST_F(EnterpriseProxyBrowserTest, FetchesPvdWithOAuthToken) {
  base::ListValue domains;
  domains.Append(CreateDomainPolicyEntry(kTestPvdDomain, /*use_oauth=*/true));
  SetUserProxyProvisioningDomains(std::move(domains));

  WaitForDynamicRoutesReady();

  const net::test_server::HttpRequest::HeaderMap pvd_headers =
      last_pvd_request_headers();
  auto auth_it = pvd_headers.find("Authorization");
  ASSERT_NE(auth_it, pvd_headers.end());
  EXPECT_EQ(auth_it->second,
            base::StringPrintf("Bearer %s", kExpectedAccessToken));
  EXPECT_GE(GetTokenFetchRequestCount(), 1u);

  GURL destination_url = GetOriginURL(kDestinationHost, "/simple.html");
  EXPECT_TRUE(chrome_test_utils::NavigateToURL(
      chrome_test_utils::GetActiveWebContents(this), destination_url));
  EXPECT_TRUE(was_proxy_accessed());
  EXPECT_TRUE(was_auth_header_received());
}

// Verifies that traffic to unmatched destinations bypasses the proxy and
// completes direct navigation without triggering proxy authentication.
IN_PROC_BROWSER_TEST_F(EnterpriseProxyBrowserTest,
                       DirectBypassForUnmatchedDestinations) {
  base::ListValue domains;
  domains.Append(CreateDomainPolicyEntry(kTestPvdDomain, /*use_oauth=*/false));
  SetUserProxyProvisioningDomains(std::move(domains));

  WaitForDynamicRoutesReady();

  GURL direct_url = GetOriginURL(kUnmatchedHost, "/direct.html");
  EXPECT_TRUE(chrome_test_utils::NavigateToURL(
      chrome_test_utils::GetActiveWebContents(this), direct_url));

  EXPECT_FALSE(was_proxy_accessed());
  EXPECT_EQ(1u, origin_request_count());

  // Verify that no proxy auth challenge was received for unmatched destination.
  auto received_entries = net_log_observer().GetEntriesWithType(
      net::NetLogEventType::ENTERPRISE_PROXY_AUTH_CHALLENGE_RECEIVED);
  EXPECT_TRUE(received_entries.empty());
}

// Verifies that multiple Provisioning Domain policy entries are fetched and
// their routing rules are merged into the dynamic routing configuration.
IN_PROC_BROWSER_TEST_F(EnterpriseProxyBrowserTest, MultiDomainPolicyMerging) {
  base::ListValue domains;
  domains.Append(CreateDomainPolicyEntry(kUnauthenticatedPvdDomain));
  domains.Append(CreateDomainPolicyEntry(kSecondaryPvdDomain));
  SetUserProxyProvisioningDomains(std::move(domains));

  WaitForDynamicRoutesReady();

  auto* service = GetEnterpriseProxyService();
  ASSERT_TRUE(service);
  auto config = service->GetDynamicRoutingConfig();
  EXPECT_FALSE(config.is_update_in_progress);
  EXPECT_FALSE(config.routing_rules.empty());

  auto pause_entries = net_log_observer().GetEntriesWithType(
      net::NetLogEventType::ENTERPRISE_PROXY_NETWORK_PAUSE);
  EXPECT_FALSE(pause_entries.empty());
}

// Verifies that ProxyProvisioningDomains set at the machine (device) policy
// level correctly applies dynamic proxy routing.
IN_PROC_BROWSER_TEST_F(EnterpriseProxyBrowserTest,
                       AppliesDynamicRoutingWithMachinePolicy) {
  base::ListValue domains;
  domains.Append(CreateDomainPolicyEntry(kTestPvdDomain, /*use_oauth=*/false));
  SetMachineProxyProvisioningDomains(std::move(domains));

  WaitForDynamicRoutesReady();

  GURL destination_url = GetOriginURL(kDestinationHost, "/simple.html");
  EXPECT_TRUE(chrome_test_utils::NavigateToURL(
      chrome_test_utils::GetActiveWebContents(this), destination_url));

  EXPECT_TRUE(was_proxy_accessed());
  EXPECT_TRUE(was_auth_header_received());
}

// Verifies that clearing ProxyProvisioningDomains immediately clears active
// dynamic proxy routes from the network stack so subsequent navigations connect
// directly without hitting the proxy or triggering HTTP 407 login dialogs.
IN_PROC_BROWSER_TEST_F(EnterpriseProxyBrowserTest,
                       ClearsDynamicRoutingWhenPolicyRemoved) {
  base::ListValue domains;
  domains.Append(CreateDomainPolicyEntry(kTestPvdDomain, /*use_oauth=*/false));
  domains.Append(
      CreateDomainPolicyEntry(kSeparateSiteHost, /*use_oauth=*/false));
  SetUserProxyProvisioningDomains(std::move(domains));

  WaitForDynamicRoutesReady();

  // Confirm dynamic routes are active for kDestinationHost.
  GURL first_url = GetOriginURL(kDestinationHost, "/simple.html");
  EXPECT_TRUE(chrome_test_utils::NavigateToURL(
      chrome_test_utils::GetActiveWebContents(this), first_url));
  EXPECT_TRUE(was_proxy_accessed());
  const size_t challenges_before_clear =
      net_log_observer()
          .GetEntriesWithType(
              net::NetLogEventType::ENTERPRISE_PROXY_AUTH_CHALLENGE_RECEIVED)
          .size();
  ASSERT_GE(challenges_before_clear, 1u);

  // Clear the policy (simulating sign-out / policy removal).
  SetUserProxyProvisioningDomains(base::ListValue());
  ProfileNetworkContextServiceFactory::GetForContext(
      chrome_test_utils::GetProfile(this))
      ->FlushProxyConfigMonitorForTesting();

  auto* service = GetEnterpriseProxyService();
  ASSERT_TRUE(service);
  EXPECT_TRUE(service->GetDynamicRoutingConfig().routing_rules.empty());

  default_proxy_server().ResetStats();

  // Navigate to a fresh previously-proxied destination (`kSeparateSiteHost`)
  // under a distinct registrable domain (`NetworkAnonymizationKey`) so socket
  // pooling and cached proxy credentials for `kDestinationHost` cannot mask
  // stale proxy routing. Navigation must succeed directly without touching the
  // proxy or challenging.
  GURL second_url = GetOriginURL(kSeparateSiteHost, "/simple.html");
  EXPECT_TRUE(chrome_test_utils::NavigateToURL(
      chrome_test_utils::GetActiveWebContents(this), second_url));
  EXPECT_FALSE(was_proxy_accessed());
  EXPECT_EQ(
      challenges_before_clear,
      net_log_observer()
          .GetEntriesWithType(
              net::NetLogEventType::ENTERPRISE_PROXY_AUTH_CHALLENGE_RECEIVED)
          .size());
}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || \
    BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(
    EnterpriseProxyBrowserTest,
    AppliedForAffiliatedUserWhenEnablePORForAllUsersDisabled) {
  SetEnableForAllUsers(false);

  base::ListValue domains;
  domains.Append(CreateDomainPolicyEntry(kTestPvdDomain, /*use_oauth=*/false));
  SetUserProxyProvisioningDomains(std::move(domains));

  WaitForDynamicRoutesReady();

  Profile* profile = chrome_test_utils::GetProfile(this);
  ASSERT_TRUE(profile);
  EXPECT_FALSE(profile->GetPrefs()
                   ->GetList(enterprise_net::kProxyProvisioningDomains)
                   .empty());

  auto* service = GetEnterpriseProxyService();
  ASSERT_TRUE(service);
  EXPECT_EQ(1u, service->GetProvisioningDomainConfigs().size());

  GURL destination_url = GetOriginURL(kDestinationHost, "/simple.html");
  EXPECT_TRUE(chrome_test_utils::NavigateToURL(
      chrome_test_utils::GetActiveWebContents(this), destination_url));
  EXPECT_TRUE(was_proxy_accessed());
}

IN_PROC_BROWSER_TEST_F(
    EnterpriseProxyBrowserTest,
    AppliedForAffiliatedUserWhenEnablePORForAllUsersEnabled) {
  SetEnableForAllUsers(true);

  base::ListValue domains;
  domains.Append(CreateDomainPolicyEntry(kTestPvdDomain, /*use_oauth=*/false));
  SetUserProxyProvisioningDomains(std::move(domains));

  WaitForDynamicRoutesReady();

  Profile* profile = chrome_test_utils::GetProfile(this);
  ASSERT_TRUE(profile);
  EXPECT_FALSE(profile->GetPrefs()
                   ->GetList(enterprise_net::kProxyProvisioningDomains)
                   .empty());

  auto* service = GetEnterpriseProxyService();
  ASSERT_TRUE(service);
  EXPECT_EQ(1u, service->GetProvisioningDomainConfigs().size());

  GURL destination_url = GetOriginURL(kDestinationHost, "/simple.html");
  EXPECT_TRUE(chrome_test_utils::NavigateToURL(
      chrome_test_utils::GetActiveWebContents(this), destination_url));
  EXPECT_TRUE(was_proxy_accessed());
}

class EnterpriseProxyUnaffiliatedBrowserTest
    : public EnterpriseProxyBrowserTestBase {
 public:
  EnterpriseProxyUnaffiliatedBrowserTest()
      : EnterpriseProxyBrowserTestBase(ManagementContext{
            .is_cloud_user_managed = true,
            .is_cloud_machine_managed = true,
            .affiliated = false,
        }) {}
};

IN_PROC_BROWSER_TEST_F(EnterpriseProxyUnaffiliatedBrowserTest,
                       IgnoredForUnaffiliatedUserByDefault) {
  base::ListValue domains;
  domains.Append(CreateDomainPolicyEntry(kTestPvdDomain, /*use_oauth=*/false));
  SetUserProxyProvisioningDomains(std::move(domains));

  ProfileNetworkContextServiceFactory::GetForContext(
      chrome_test_utils::GetProfile(this))
      ->FlushProxyConfigMonitorForTesting();

  Profile* profile = chrome_test_utils::GetProfile(this);
  ASSERT_TRUE(profile);
  EXPECT_TRUE(profile->GetPrefs()
                  ->GetList(enterprise_net::kProxyProvisioningDomains)
                  .empty());

  auto* service = GetEnterpriseProxyService();
  ASSERT_TRUE(service);
  EXPECT_TRUE(service->GetProvisioningDomainConfigs().empty());
  EXPECT_TRUE(service->GetDynamicRoutingConfig().routing_rules.empty());

  GURL destination_url = GetOriginURL(kDestinationHost, "/simple.html");
  EXPECT_TRUE(chrome_test_utils::NavigateToURL(
      chrome_test_utils::GetActiveWebContents(this), destination_url));
  EXPECT_FALSE(was_proxy_accessed());
}

IN_PROC_BROWSER_TEST_F(
    EnterpriseProxyUnaffiliatedBrowserTest,
    IgnoredForUnaffiliatedUserWhenEnablePORForAllUsersDisabled) {
  SetEnableForAllUsers(false);

  base::ListValue domains;
  domains.Append(CreateDomainPolicyEntry(kTestPvdDomain, /*use_oauth=*/false));
  SetUserProxyProvisioningDomains(std::move(domains));

  ProfileNetworkContextServiceFactory::GetForContext(
      chrome_test_utils::GetProfile(this))
      ->FlushProxyConfigMonitorForTesting();

  Profile* profile = chrome_test_utils::GetProfile(this);
  ASSERT_TRUE(profile);
  EXPECT_TRUE(profile->GetPrefs()
                  ->GetList(enterprise_net::kProxyProvisioningDomains)
                  .empty());

  auto* service = GetEnterpriseProxyService();
  ASSERT_TRUE(service);
  EXPECT_TRUE(service->GetProvisioningDomainConfigs().empty());
  EXPECT_TRUE(service->GetDynamicRoutingConfig().routing_rules.empty());

  GURL destination_url = GetOriginURL(kDestinationHost, "/simple.html");
  EXPECT_TRUE(chrome_test_utils::NavigateToURL(
      chrome_test_utils::GetActiveWebContents(this), destination_url));
  EXPECT_FALSE(was_proxy_accessed());
}

IN_PROC_BROWSER_TEST_F(
    EnterpriseProxyUnaffiliatedBrowserTest,
    AppliedForUnaffiliatedUserWhenEnablePORForAllUsersEnabled) {
  SetEnableForAllUsers(true);

  base::ListValue domains;
  domains.Append(CreateDomainPolicyEntry(kTestPvdDomain, /*use_oauth=*/false));
  SetUserProxyProvisioningDomains(std::move(domains));

  WaitForDynamicRoutesReady();

  Profile* profile = chrome_test_utils::GetProfile(this);
  ASSERT_TRUE(profile);
  EXPECT_FALSE(profile->GetPrefs()
                   ->GetList(enterprise_net::kProxyProvisioningDomains)
                   .empty());

  auto* service = GetEnterpriseProxyService();
  ASSERT_TRUE(service);
  EXPECT_EQ(1u, service->GetProvisioningDomainConfigs().size());

  GURL destination_url = GetOriginURL(kDestinationHost, "/simple.html");
  EXPECT_TRUE(chrome_test_utils::NavigateToURL(
      chrome_test_utils::GetActiveWebContents(this), destination_url));
  EXPECT_TRUE(was_proxy_accessed());
}

class EnterpriseProxyUnmanagedDeviceBrowserTest
    : public EnterpriseProxyBrowserTestBase {
 public:
  EnterpriseProxyUnmanagedDeviceBrowserTest()
      : EnterpriseProxyBrowserTestBase(ManagementContext{
            .is_cloud_user_managed = true,
            .is_cloud_machine_managed = false,
        }) {}
};

IN_PROC_BROWSER_TEST_F(EnterpriseProxyUnmanagedDeviceBrowserTest,
                       AppliedForUserOnUnmanagedDevice) {
  base::ListValue domains;
  domains.Append(CreateDomainPolicyEntry(kTestPvdDomain, /*use_oauth=*/false));
  SetUserProxyProvisioningDomains(std::move(domains));

  WaitForDynamicRoutesReady();

  Profile* profile = chrome_test_utils::GetProfile(this);
  ASSERT_TRUE(profile);
  EXPECT_FALSE(profile->GetPrefs()
                   ->GetList(enterprise_net::kProxyProvisioningDomains)
                   .empty());

  auto* service = GetEnterpriseProxyService();
  ASSERT_TRUE(service);
  EXPECT_EQ(1u, service->GetProvisioningDomainConfigs().size());

  GURL destination_url = GetOriginURL(kDestinationHost, "/simple.html");
  EXPECT_TRUE(chrome_test_utils::NavigateToURL(
      chrome_test_utils::GetActiveWebContents(this), destination_url));
  EXPECT_TRUE(was_proxy_accessed());
}

class EnterpriseProxyPlatformManagedDeviceBrowserTest
    : public EnterpriseProxyBrowserTestBase {
 public:
  EnterpriseProxyPlatformManagedDeviceBrowserTest()
      : EnterpriseProxyBrowserTestBase(ManagementContext{
            .is_cloud_user_managed = true,
            .is_cloud_machine_managed = false,
            .is_platform_managed = true,
            .affiliated = false,
        }) {}

 private:
  base::test::ScopedFeatureList affiliation_feature_list_{
      enterprise_net::kEnforceProxyAffiliationOnPlatformManagedDevices};
};

// Verifies that user-scoped ProxyProvisioningDomains on a platform/EMM-managed
// device without CBCM are ignored by default and applied when
// EnableProxyOverrideRulesForAllUsers is enabled.
IN_PROC_BROWSER_TEST_F(EnterpriseProxyPlatformManagedDeviceBrowserTest,
                       PlatformManagedDeviceWithoutCbcm) {
  base::ListValue domains;
  domains.Append(CreateDomainPolicyEntry(kTestPvdDomain, /*use_oauth=*/false));
  SetUserProxyProvisioningDomains(std::move(domains));

  ProfileNetworkContextServiceFactory::GetForContext(
      chrome_test_utils::GetProfile(this))
      ->FlushProxyConfigMonitorForTesting();

  Profile* profile = chrome_test_utils::GetProfile(this);
  ASSERT_TRUE(profile);
  EXPECT_TRUE(profile->GetPrefs()
                  ->GetList(enterprise_net::kProxyProvisioningDomains)
                  .empty());

  auto* service = GetEnterpriseProxyService();
  ASSERT_TRUE(service);
  EXPECT_TRUE(service->GetProvisioningDomainConfigs().empty());
  EXPECT_TRUE(service->GetDynamicRoutingConfig().routing_rules.empty());

  GURL destination_url = GetOriginURL(kDestinationHost, "/simple.html");
  EXPECT_TRUE(chrome_test_utils::NavigateToURL(
      chrome_test_utils::GetActiveWebContents(this), destination_url));
  EXPECT_FALSE(was_proxy_accessed());

  SetEnableForAllUsers(true);
  WaitForDynamicRoutesReady();

  EXPECT_FALSE(profile->GetPrefs()
                   ->GetList(enterprise_net::kProxyProvisioningDomains)
                   .empty());
  EXPECT_EQ(1u, service->GetProvisioningDomainConfigs().size());

  EXPECT_TRUE(chrome_test_utils::NavigateToURL(
      chrome_test_utils::GetActiveWebContents(this), destination_url));
  EXPECT_TRUE(was_proxy_accessed());
}
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) ||
        // BUILDFLAG(IS_ANDROID)

// Verifies across Mojo that `NetworkService` holds in-flight navigations paused
// while `/.well-known/pvd` fetches are mid-flight:
// - During initial policy load, an in-flight navigation waits for the PvD
//   response instead of leaking directly, then routes via the proxy.
// - During a runtime policy update that adds an OAuth-authenticated PvD domain
//   (`use_oauth = true`) after cached tokens are evicted, Chrome acquires a
//   fresh OAuth token while `NetworkService` is paused without deadlocking,
//   sends `Authorization: Bearer` on `/.well-known/pvd`, and holds concurrent
//   navigations until the updated routes arrive.
IN_PROC_BROWSER_TEST_F(
    EnterpriseProxyBrowserTest,
    NetworkPauseHoldsNavigationDuringInitialAndRuntimeRefresh) {
  Profile* profile = chrome_test_utils::GetProfile(this);
  content::WebContents* web_contents =
      chrome_test_utils::GetActiveWebContents(this);
  enterprise_net::EnterpriseProxyService* service = GetEnterpriseProxyService();
  ASSERT_TRUE(service);

  // Initial policy load: defer the PvD response and verify an in-flight
  // navigation is held paused until the PvD fetch finishes.
  SetDeferPvdResponses(true);

  base::ListValue initial_domains;
  initial_domains.Append(
      CreateDomainPolicyEntry(kTestPvdDomain, /*use_oauth=*/false));
  SetUserProxyProvisioningDomains(std::move(initial_domains));
  ProfileNetworkContextServiceFactory::GetForContext(profile)
      ->FlushProxyConfigMonitorForTesting();

  WaitForDeferredPvdRequests(1);
  EXPECT_TRUE(service->IsRefreshInProgress());

  GURL initial_url = GetOriginURL(kDestinationHost, "/simple.html");
  content::TestNavigationManager initial_nav(web_contents, initial_url);
  web_contents->GetController().LoadURL(initial_url, content::Referrer(),
                                        ui::PAGE_TRANSITION_TYPED,
                                        /*extra_headers=*/std::string());

  ASSERT_TRUE(initial_nav.WaitForLoaderStart());
  profile->GetDefaultStoragePartition()->FlushNetworkInterfaceForTesting();

  // While the PvD fetch is deferred, the navigation must not reach either the
  // proxy or the origin server.
  EXPECT_NE(initial_url, web_contents->GetLastCommittedURL());
  EXPECT_FALSE(was_proxy_accessed());
  EXPECT_EQ(0u, origin_request_count());

  SetDeferPvdResponses(false);
  ReleaseDeferredPvdResponses();

  ASSERT_TRUE(initial_nav.WaitForNavigationFinished());
  EXPECT_TRUE(initial_nav.was_successful());
  EXPECT_EQ(initial_url, web_contents->GetLastCommittedURL());
  EXPECT_TRUE(was_proxy_accessed());
  EXPECT_TRUE(was_auth_header_received());
  EXPECT_EQ(1u, origin_request_count());

  // Runtime policy update: evict the cached OAuth token, defer PvD responses
  // again, and switch to `kSecondaryPvdDomain` with `use_oauth = true`.
  SetPvdJsonForDomain(
      kSecondaryPvdDomain,
      BuildValidPvdJson(kSecondaryPvdDomain,
                        default_proxy_server().host_port_pair().ToString(),
                        {kAddedDestinationHost}, /*require_auth=*/true));
  EvictCachedProxyAccessToken();
  ASSERT_TRUE(base::test::RunUntil(
      [service]() { return !service->IsRefreshInProgress(); }));
  SetDeferPvdResponses(true);
  default_proxy_server().ResetStats();
  const size_t token_fetches_before_refresh = GetTokenFetchRequestCount();

  base::ListValue updated_domains;
  updated_domains.Append(
      CreateDomainPolicyEntry(kSecondaryPvdDomain, /*use_oauth=*/true));
  SetUserProxyProvisioningDomains(std::move(updated_domains));
  ProfileNetworkContextServiceFactory::GetForContext(profile)
      ->FlushProxyConfigMonitorForTesting();

  // Reaching `pvd_server_` proves token acquisition succeeded while
  // `NetworkService` re-entered the pause (`IsRefreshInProgress() == true`).
  ASSERT_TRUE(base::test::RunUntil([this]() {
    return GetPvdRequestCountForDomain(kSecondaryPvdDomain) >= 1u &&
           deferred_pvd_request_count() >= 1u;
  }));
  EXPECT_TRUE(service->IsRefreshInProgress());
  EXPECT_GT(GetTokenFetchRequestCount(), token_fetches_before_refresh);

  const net::test_server::HttpRequest::HeaderMap pvd_headers =
      last_pvd_request_headers();
  auto auth_it = pvd_headers.find("Authorization");
  ASSERT_NE(auth_it, pvd_headers.end());
  EXPECT_EQ(auth_it->second,
            base::StringPrintf("Bearer %s", kExpectedAccessToken));

  GURL added_url = GetOriginURL(kAddedDestinationHost, "/simple.html");
  content::TestNavigationManager runtime_nav(web_contents, added_url);
  web_contents->GetController().LoadURL(added_url, content::Referrer(),
                                        ui::PAGE_TRANSITION_TYPED,
                                        /*extra_headers=*/std::string());

  ASSERT_TRUE(runtime_nav.WaitForLoaderStart());
  profile->GetDefaultStoragePartition()->FlushNetworkInterfaceForTesting();
  EXPECT_NE(added_url, web_contents->GetLastCommittedURL());
  EXPECT_FALSE(was_proxy_accessed());
  EXPECT_EQ(1u, origin_request_count());

  SetDeferPvdResponses(false);
  ReleaseDeferredPvdResponses();

  ASSERT_TRUE(runtime_nav.WaitForNavigationFinished());
  EXPECT_TRUE(runtime_nav.was_successful());
  EXPECT_EQ(added_url, web_contents->GetLastCommittedURL());
  EXPECT_TRUE(was_proxy_accessed());
  EXPECT_TRUE(was_auth_header_received());
  EXPECT_EQ(2u, origin_request_count());
}

// Verifies that if an in-flight navigation is paused behind a
// `/.well-known/pvd` fetch that ultimately fails (HTTP 500), `NetworkService`
// unpauses once the refresh completes and allows the navigation to fall back to
// DIRECT.
IN_PROC_BROWSER_TEST_F(EnterpriseProxyBrowserTest,
                       NetworkPauseFallsBackToDirectWhenPvdFetchFails) {
  base::HistogramTester histograms;
  SetDeferPvdResponses(true);
  SetPvdResponseOverride(net::HttpStatusCode::HTTP_INTERNAL_SERVER_ERROR,
                         "Internal Server Error", "text/plain");

  base::ListValue domains;
  domains.Append(CreateDomainPolicyEntry(kTestPvdDomain, /*use_oauth=*/false));
  SetUserProxyProvisioningDomains(std::move(domains));

  ProfileNetworkContextServiceFactory::GetForContext(
      chrome_test_utils::GetProfile(this))
      ->FlushProxyConfigMonitorForTesting();

  WaitForDeferredPvdRequests(1);

  content::WebContents* web_contents =
      chrome_test_utils::GetActiveWebContents(this);
  GURL destination_url = GetOriginURL(kDestinationHost, "/simple.html");
  content::TestNavigationManager nav_manager(web_contents, destination_url);
  web_contents->GetController().LoadURL(destination_url, content::Referrer(),
                                        ui::PAGE_TRANSITION_TYPED,
                                        /*extra_headers=*/std::string());

  ASSERT_TRUE(nav_manager.WaitForLoaderStart());
  chrome_test_utils::GetProfile(this)
      ->GetDefaultStoragePartition()
      ->FlushNetworkInterfaceForTesting();
  EXPECT_NE(destination_url, web_contents->GetLastCommittedURL());
  EXPECT_EQ(0u, origin_request_count());

  SetDeferPvdResponses(false);
  ReleaseDeferredPvdResponses();

  ASSERT_TRUE(nav_manager.WaitForNavigationFinished());
  EXPECT_TRUE(nav_manager.was_successful());
  EXPECT_FALSE(was_proxy_accessed());
  EXPECT_EQ(1u, origin_request_count());
  enterprise_net::EnterpriseProxyService* service = GetEnterpriseProxyService();
  ASSERT_TRUE(service);
  ASSERT_TRUE(base::test::RunUntil(
      [service]() { return !service->IsRefreshInProgress(); }));
  EXPECT_GE(pvd_request_count(), 3u);
  ASSERT_EQ(1u, service->GetProvisioningDomainConfigs().size());
  EXPECT_EQ(
      enterprise_net::ProvisioningDomainProxyConfig::State::kFailedTransient,
      service->GetProvisioningDomainConfigs()[0].state);

  chrome_test_utils::GetProfile(this)
      ->GetDefaultStoragePartition()
      ->FlushNetworkInterfaceForTesting();
  content::FetchHistogramsFromChildProcesses();
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  // Note: `safe_browsing::NetworkContextService` creates a second per-profile
  // `NetworkContext` that also observes `EnterpriseProxyService` and records 0
  // paused requests while idle, so assert that the main `NetworkContext`
  // recorded at least one paused request (`min >= 1`).
  EXPECT_THAT(
      histograms.GetAllSamples(
          "Net.ProxyResolution.DynamicRoutingPause.PausedRequestCount"),
      testing::Contains(testing::Field(&base::Bucket::min, testing::Ge(1))));
}

// Verifies full 3-tier coexistence and precedence when `ProxyOverrideRules`
// (POR), `ProxyProvisioningDomains` (PvD), and `ProxySettings` (PAC script) are
// all configured simultaneously (`POR > PvD > PAC`):
// - `/.well-known/pvd` fetches bypass the PAC proxy (`LOAD_BYPASS_PROXY`).
// - Case #1 (`kPorOnlyDestinationHost`, matched by POR + PAC): routes via
//   `por_proxy`.
// - Case #2 (`kDestinationHost`, matched by POR + PvD + PAC): POR takes
//   precedence over both PvD and PAC (`por_proxy`).
// - Case #3 (`kSecondaryDestinationHost`, matched by PvD + PAC): PvD takes
//   precedence over PAC (`default_proxy_server` with 407 auth).
// - Case #4 (`kUnmatchedHost`, matched only by PAC): falls through POR and PvD
//   to `pac_proxy`.
IN_PROC_BROWSER_TEST_F(EnterpriseProxyBrowserTest,
                       ProxyPrecedencePorOverPvdOverPac) {
  std::unique_ptr<TestConnectProxy> por_proxy =
      CreateProxyServer(kSecondaryProxyHost);
  ASSERT_TRUE(por_proxy);
  por_proxy->SetRequireAuth(false);

  std::unique_ptr<TestConnectProxy> pac_proxy =
      CreateProxyServer(kTertiaryProxyHost);
  ASSERT_TRUE(pac_proxy);
  pac_proxy->SetRequireAuth(false);

  // Scope the PAC script to the test and PvD hostnames so unrelated background
  // requests (e.g. Gaia) do not touch `pac_proxy`.
  const std::string pac_data_url = base::StringPrintf(
      "data:application/x-ns-proxy-autoconfig,"
      "function FindProxyForURL(url, host) {"
      "  if (host == '%s' || host == '%s' || host == '%s' || host == '%s' ||"
      "      host == '%s' || host == '%s') {"
      "    return 'HTTPS %s';"
      "  }"
      "  return 'DIRECT';"
      "}",
      kPorOnlyDestinationHost, kDestinationHost, kSecondaryDestinationHost,
      kUnmatchedHost, kTestPvdDomain, kSecondaryPvdDomain,
      pac_proxy->host_port_pair().ToString().c_str());

  base::DictValue proxy_settings;
  proxy_settings.Set(policy::key::kProxyMode,
                     ProxyPrefs::kPacScriptProxyModeName);
  proxy_settings.Set(policy::key::kProxyPacUrl, pac_data_url);
  SetUserProxySettings(std::move(proxy_settings));

  const std::string por_proxy_spec =
      base::StrCat({"HTTPS ", por_proxy->host_port_pair().ToString()});
  base::ListValue por_rules;
  por_rules.Append(
      base::DictValue()
          .Set("DestinationMatchers",
               base::ListValue()
                   .Append(base::StrCat({"https://", kPorOnlyDestinationHost}))
                   .Append(base::StrCat({"https://", kDestinationHost})))
          .Set("ProxyList", base::ListValue().Append(por_proxy_spec)));
  SetUserProxyOverrideRules(std::move(por_rules));

  base::ListValue pvd_domains;
  pvd_domains.Append(
      CreateDomainPolicyEntry(kTestPvdDomain, /*use_oauth=*/false));
  pvd_domains.Append(
      CreateDomainPolicyEntry(kSecondaryPvdDomain, /*use_oauth=*/false));
  SetUserProxyProvisioningDomains(std::move(pvd_domains));

  WaitForDynamicRoutesReady();
  ProfileNetworkContextServiceFactory::GetForContext(
      chrome_test_utils::GetProfile(this))
      ->FlushProxyConfigMonitorForTesting();

  // `/.well-known/pvd` uses `LOAD_BYPASS_PROXY` and must not touch `pac_proxy`.
  EXPECT_THAT(pac_proxy->tunneled_targets(), testing::IsEmpty());

  auto reset_all_proxy_stats = [&]() {
    por_proxy->ResetStats();
    default_proxy_server().ResetStats();
    pac_proxy->ResetStats();
  };

  content::WebContents* web_contents =
      chrome_test_utils::GetActiveWebContents(this);

  // Case #1: Matched by POR + PAC (`kPorOnlyDestinationHost`) -> `por_proxy`.
  reset_all_proxy_stats();
  EXPECT_TRUE(chrome_test_utils::NavigateToURL(
      web_contents, GetOriginURL(kPorOnlyDestinationHost, "/simple.html")));
  EXPECT_TRUE(por_proxy->was_accessed());
  EXPECT_FALSE(was_proxy_accessed());
  EXPECT_FALSE(pac_proxy->was_accessed());

  // Case #2: Matched by POR + PvD + PAC (`kDestinationHost`) -> POR wins.
  reset_all_proxy_stats();
  EXPECT_TRUE(chrome_test_utils::NavigateToURL(
      web_contents, GetOriginURL(kDestinationHost, "/simple.html")));
  EXPECT_TRUE(por_proxy->was_accessed());
  EXPECT_THAT(por_proxy->tunneled_targets(),
              testing::Contains(testing::HasSubstr(kDestinationHost)));
  EXPECT_FALSE(was_proxy_accessed());
  EXPECT_FALSE(pac_proxy->was_accessed());

  // Case #3: Matched by PvD + PAC (`kSecondaryDestinationHost`) -> PvD wins.
  reset_all_proxy_stats();
  EXPECT_TRUE(chrome_test_utils::NavigateToURL(
      web_contents, GetOriginURL(kSecondaryDestinationHost, "/simple.html")));
  EXPECT_FALSE(por_proxy->was_accessed());
  EXPECT_TRUE(was_proxy_accessed());
  EXPECT_TRUE(was_auth_header_received());
  EXPECT_THAT(default_proxy_server().tunneled_targets(),
              testing::Contains(testing::HasSubstr(kSecondaryDestinationHost)));
  EXPECT_FALSE(pac_proxy->was_accessed());

  // Case #4: Matched only by PAC (`kUnmatchedHost`) -> falls back to PAC.
  reset_all_proxy_stats();
  EXPECT_TRUE(chrome_test_utils::NavigateToURL(
      web_contents, GetOriginURL(kUnmatchedHost, "/direct.html")));
  EXPECT_FALSE(por_proxy->was_accessed());
  EXPECT_FALSE(was_proxy_accessed());
  EXPECT_TRUE(pac_proxy->was_accessed());
  EXPECT_THAT(pac_proxy->tunneled_targets(),
              testing::Contains(testing::HasSubstr(kUnmatchedHost)));
}

// Verifies "Venn diagram" dynamic route updates across policy refreshes:
// - Policy v1 routes `[removed.example.com, retained.example.com]` via `proxy1`
// - Policy v2 routes `[retained.example.com, added.example.com]` via `proxy2`
// After the update, `removed` goes direct while `retained` and `added` route
// through `proxy2`.
IN_PROC_BROWSER_TEST_F(EnterpriseProxyBrowserTest,
                       DynamicRouteUpdateRemovesRetainsAndAddsRoutes) {
  std::unique_ptr<TestConnectProxy> proxy2 =
      CreateProxyServer(kSecondaryProxyHost);
  ASSERT_TRUE(proxy2);

  SetPvdJsonForDomain(
      kTestPvdDomain,
      BuildValidPvdJson(kTestPvdDomain,
                        default_proxy_server().host_port_pair().ToString(),
                        {kRemovedDestinationHost, kRetainedDestinationHost},
                        /*require_auth=*/true));

  base::ListValue v1_domains;
  v1_domains.Append(
      CreateDomainPolicyEntry(kTestPvdDomain, /*use_oauth=*/false));
  SetUserProxyProvisioningDomains(std::move(v1_domains));
  WaitForDynamicRoutesReady();

  content::WebContents* web_contents =
      chrome_test_utils::GetActiveWebContents(this);

  // Under v1: `kRemovedDestinationHost` and `kRetainedDestinationHost` use
  // `proxy1`; `kAddedDestinationHost` is direct.
  EXPECT_TRUE(chrome_test_utils::NavigateToURL(
      web_contents, GetOriginURL(kRemovedDestinationHost, "/simple.html")));
  EXPECT_TRUE(was_proxy_accessed());
  EXPECT_FALSE(proxy2->was_accessed());

  default_proxy_server().ResetStats();
  EXPECT_TRUE(chrome_test_utils::NavigateToURL(
      web_contents,
      GetOriginURL(kRetainedDestinationHost, "/simple.html?v=1")));
  EXPECT_TRUE(was_proxy_accessed());
  EXPECT_FALSE(proxy2->was_accessed());

  default_proxy_server().ResetStats();
  EXPECT_TRUE(chrome_test_utils::NavigateToURL(
      web_contents, GetOriginURL(kAddedDestinationHost, "/direct.html")));
  EXPECT_FALSE(was_proxy_accessed());
  EXPECT_FALSE(proxy2->was_accessed());

  // Update policy to v2 (`kSecondaryPvdDomain` -> `proxy2` for `retained` +
  // `added`).
  SetPvdJsonForDomain(
      kSecondaryPvdDomain,
      BuildValidPvdJson(kSecondaryPvdDomain,
                        proxy2->host_port_pair().ToString(),
                        {kRetainedDestinationHost, kAddedDestinationHost},
                        /*require_auth=*/true));
  base::ListValue v2_domains;
  v2_domains.Append(
      CreateDomainPolicyEntry(kSecondaryPvdDomain, /*use_oauth=*/false));
  SetUserProxyProvisioningDomains(std::move(v2_domains));

  enterprise_net::EnterpriseProxyService* service = GetEnterpriseProxyService();
  ASSERT_TRUE(service);
  ASSERT_TRUE(base::test::RunUntil([this, service]() {
    return GetPvdRequestCountForDomain(kSecondaryPvdDomain) >= 1u &&
           !service->IsRefreshInProgress() &&
           service->GetProvisioningDomainConfigs().size() == 1u &&
           service->GetProvisioningDomainConfigs()[0].state ==
               enterprise_net::ProvisioningDomainProxyConfig::State::kValid;
  }));
  ProfileNetworkContextServiceFactory::GetForContext(
      chrome_test_utils::GetProfile(this))
      ->FlushProxyConfigMonitorForTesting();

  // Navigate to a fresh path on `kRemovedDestinationHost` and verify it now
  // connects directly without touching either proxy.
  default_proxy_server().ResetStats();
  proxy2->ResetStats();
  EXPECT_TRUE(chrome_test_utils::NavigateToURL(
      web_contents, GetOriginURL(kRemovedDestinationHost, "/direct.html")));
  EXPECT_FALSE(was_proxy_accessed());
  EXPECT_FALSE(proxy2->was_accessed());

  // Both `kRetainedDestinationHost` and `kAddedDestinationHost` now route via
  // `proxy2`.
  EXPECT_TRUE(chrome_test_utils::NavigateToURL(
      web_contents,
      GetOriginURL(kRetainedDestinationHost, "/simple.html?v=2")));
  EXPECT_FALSE(was_proxy_accessed());
  EXPECT_TRUE(proxy2->was_accessed());
  EXPECT_TRUE(proxy2->was_auth_header_received());

  proxy2->ResetStats();
  EXPECT_TRUE(chrome_test_utils::NavigateToURL(
      web_contents, GetOriginURL(kAddedDestinationHost, "/simple.html")));
  EXPECT_FALSE(was_proxy_accessed());
  EXPECT_TRUE(proxy2->was_accessed());
}

// Verifies multi-proxy failover: when a PvD routing rule specifies an ordered
// list `["proxy1", "proxy2"]` and `proxy1` is unreachable, `//net` fails over
// to `proxy2` and `EnterpriseProxyService` authenticates against `proxy2`.
IN_PROC_BROWSER_TEST_F(EnterpriseProxyBrowserTest,
                       FailsOverToSecondaryProxyWhenPrimaryUnreachable) {
  std::unique_ptr<TestConnectProxy> proxy2 =
      CreateProxyServer(kSecondaryProxyHost);
  ASSERT_TRUE(proxy2);

  SetPvdJsonForDomain(
      kTestPvdDomain,
      BuildMultiProxyPvdJson(
          kTestPvdDomain,
          {{"proxy1", default_proxy_server().host_port_pair().ToString()},
           {"proxy2", proxy2->host_port_pair().ToString()}},
          {"proxy1", "proxy2"}, {kDestinationHost}, /*require_auth=*/true));

  base::ListValue domains;
  domains.Append(CreateDomainPolicyEntry(kTestPvdDomain, /*use_oauth=*/false));
  SetUserProxyProvisioningDomains(std::move(domains));
  WaitForDynamicRoutesReady();

  // Shut down primary `proxy1` before navigation so connection attempts fail.
  ASSERT_TRUE(default_proxy_server().Shutdown());

  GURL destination_url = GetOriginURL(kDestinationHost, "/simple.html");
  EXPECT_TRUE(chrome_test_utils::NavigateToURL(
      chrome_test_utils::GetActiveWebContents(this), destination_url));

  EXPECT_FALSE(was_proxy_accessed());
  EXPECT_TRUE(proxy2->was_accessed());
  EXPECT_TRUE(proxy2->was_auth_header_received());
  EXPECT_EQ(1u, origin_request_count());
}

// Verifies that when dynamic proxy routing rules match the Gaia OAuth2 host
// (`kGaiaHost`) in addition to `kDestinationHost` with no cached access token,
// Android acquires the OAuth2 token out-of-band via OS `AccountManagerFacade`
// without looping on a 407 proxy challenge.
// TODO(crbug.com/532559920): Enable on Desktop once OAuth2 token fetches bypass
// proxy authentication loops when Gaia matches proxy routing rules.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_AuthenticatesWhenGaiaTokenEndpointMatchesProxyRoute \
  AuthenticatesWhenGaiaTokenEndpointMatchesProxyRoute
#else
#define MAYBE_AuthenticatesWhenGaiaTokenEndpointMatchesProxyRoute \
  DISABLED_AuthenticatesWhenGaiaTokenEndpointMatchesProxyRoute
#endif
IN_PROC_BROWSER_TEST_F(
    EnterpriseProxyBrowserTest,
    MAYBE_AuthenticatesWhenGaiaTokenEndpointMatchesProxyRoute) {
  signin::DisableAccessTokenFetchRetries(GetIdentityManager());
  SetPvdJsonForDomain(
      kTestPvdDomain,
      BuildValidPvdJson(kTestPvdDomain,
                        default_proxy_server().host_port_pair().ToString(),
                        {kDestinationHost, kGaiaHost},
                        /*require_auth=*/true));

  base::ListValue domains;
  domains.Append(CreateDomainPolicyEntry(kTestPvdDomain, /*use_oauth=*/false));
  SetUserProxyProvisioningDomains(std::move(domains));
  WaitForDynamicRoutesReady();

  EvictCachedProxyAccessToken();

  GURL destination_url = GetOriginURL(kDestinationHost, "/simple.html");
  EXPECT_TRUE(chrome_test_utils::NavigateToURL(
      chrome_test_utils::GetActiveWebContents(this), destination_url));
  EXPECT_TRUE(was_proxy_accessed());
  EXPECT_TRUE(was_auth_header_received());
  EXPECT_GE(GetTokenFetchRequestCount(), 1u);
}

}  // namespace enterprise::test
