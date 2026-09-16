// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base64.h"
#include "base/run_loop.h"
#include "base/strings/escape.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/enterprise/net/test/enterprise_proxy_browsertest_base.h"
#include "chrome/browser/enterprise/test/management_context_mixin.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/enterprise/browser/identifiers/profile_id_service.h"
#include "components/enterprise/net/core/enterprise_proxy_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
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
  domains.Append(
      CreateDomainPolicyEntry("domain1.example.com", /*use_oauth=*/false));
  domains.Append(
      CreateDomainPolicyEntry("domain2.example.com", /*use_oauth=*/false));
  SetUserProxyProvisioningDomains(std::move(domains));

  WaitForDynamicRoutesReady();

  auto* service = GetEnterpriseProxyService();
  ASSERT_TRUE(service);
  EXPECT_EQ(3u, service->GetProvisioningDomainConfigs().size());

  GURL destination_url = https_server_.GetURL(kDestinationHost, "/simple.html");
  EXPECT_TRUE(chrome_test_utils::NavigateToURL(
      chrome_test_utils::GetActiveWebContents(this), destination_url));

  EXPECT_TRUE(was_proxy_accessed());
  // Because the first matching config requires auth, an auth header is sent.
  // (If it had matched domain1 instead, no auth header would have been sent).
  EXPECT_TRUE(was_auth_header_received());

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

// Verifies that traffic to unmatched destinations bypasses the proxy and
// completes direct navigation without triggering proxy authentication.
IN_PROC_BROWSER_TEST_F(EnterpriseProxyBrowserTest,
                       DirectBypassForUnmatchedDestinations) {
  base::ListValue domains;
  domains.Append(CreateDomainPolicyEntry(kTestPvdDomain, /*use_oauth=*/false));
  SetUserProxyProvisioningDomains(std::move(domains));

  WaitForDynamicRoutesReady();

  GURL direct_url = https_server_.GetURL(kUnmatchedHost, "/direct.html");
  EXPECT_TRUE(chrome_test_utils::NavigateToURL(
      chrome_test_utils::GetActiveWebContents(this), direct_url));

  // Verify that no proxy auth challenge was received for unmatched destination.
  auto received_entries = net_log_observer().GetEntriesWithType(
      net::NetLogEventType::ENTERPRISE_PROXY_AUTH_CHALLENGE_RECEIVED);
  EXPECT_TRUE(received_entries.empty());
}

// Verifies that multiple Provisioning Domain policy entries are fetched and
// their routing rules are merged into the dynamic routing configuration.
IN_PROC_BROWSER_TEST_F(EnterpriseProxyBrowserTest, MultiDomainPolicyMerging) {
  base::ListValue domains;
  domains.Append(CreateDomainPolicyEntry("domain1.example.com"));
  domains.Append(CreateDomainPolicyEntry("domain2.example.com"));
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

  GURL destination_url = https_server_.GetURL(kDestinationHost, "/simple.html");
  EXPECT_TRUE(chrome_test_utils::NavigateToURL(
      chrome_test_utils::GetActiveWebContents(this), destination_url));

  EXPECT_TRUE(was_proxy_accessed());
  EXPECT_TRUE(was_auth_header_received());
}

}  // namespace enterprise::test
