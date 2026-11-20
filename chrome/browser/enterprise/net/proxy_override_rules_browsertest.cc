// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/enterprise/test/management_context_mixin.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "components/enterprise/net/core/features.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/policy_constants.h"
#include "components/proxy_config/pref_proxy_config_tracker_impl.h"
#include "components/proxy_config/proxy_prefs_utils.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/run_until.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/net/profile_network_context_service.h"
#include "chrome/browser/net/profile_network_context_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/dbus/shill/shill_profile_client.h"
#include "chromeos/ash/components/dbus/shill/shill_service_client.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_profile_handler.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "components/onc/onc_pref_names.h"  // nogncheck
#include "components/prefs/pref_service.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"
#endif

namespace enterprise::test {

namespace {

constexpr char kDestinationHost[] = "destination.com";
#if BUILDFLAG(IS_CHROMEOS)
constexpr char kPacDestinationHost[] = "pac-destination.com";
constexpr char kDirectDestinationHost[] = "direct-destination.com";
#endif
constexpr char kResolvableHost[] = "resolvable.com";
constexpr char kUnresolvableHost[] = "unresolvable.com";
constexpr int kUnreachablePort = 12345;

}  // namespace

class ProxyOverrideRulesTestBase : public MixinBasedPlatformBrowserTest {
 public:
  explicit ProxyOverrideRulesTestBase(const ManagementContext& context)
      : is_platform_managed_(context.is_platform_managed) {
    scoped_feature_list_.InitAndEnableFeature(kEnableProxyOverrideRules);
    management_context_mixin_ =
        ManagementContextMixin::Create(&mixin_host_, this, context);
  }

  void SetUpOnMainThread() override {
    MixinBasedPlatformBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule(kDestinationHost, "127.0.0.1");
#if BUILDFLAG(IS_CHROMEOS)
    host_resolver()->AddRule(kPacDestinationHost, "127.0.0.1");
    host_resolver()->AddRule(kDirectDestinationHost, "127.0.0.1");
    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        [](const net::test_server::EmbeddedTestServer* server,
           const net::test_server::HttpRequest& request)
            -> std::unique_ptr<net::test_server::HttpResponse> {
          if (request.relative_url != "/proxy.pac") {
            return nullptr;
          }
          auto response =
              std::make_unique<net::test_server::BasicHttpResponse>();
          response->set_code(net::HTTP_OK);
          response->set_content_type("application/x-ns-proxy-autoconfig");
          response->set_content(base::StringPrintf(
              R"(
                function FindProxyForURL(url, host) {
                  if (host === "%s") {
                    return "PROXY %s";
                  }
                  return "DIRECT";
                }
              )",
              kPacDestinationHost,
              server->host_port_pair().ToString().c_str()));
          return response;
        },
        embedded_test_server()));
#endif
    host_resolver()->AddRule(kResolvableHost, "127.0.0.1");
    host_resolver()->AddSimulatedFailure(kUnresolvableHost);

    // Only the default `embedded_test_server()` is used for all proxies and
    // destinations in this test suite because it is the only one for which
    // port forwarding is automatically set up on Android. To verify which
    // proxy configuration is used, the tests target a destination URL on an
    // unreachable port; successful navigation implies the proxy was used (as
    // the proxy receives the request and ignores the port), while failure
    // implies it was bypassed.
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void SetUserProxyOverrideRules(base::ListValue rules) {
    base::flat_map<std::string, std::optional<base::Value>> policies;
    policies.emplace(policy::key::kProxyOverrideRules,
                     base::Value(std::move(rules)));
    management_context_mixin_->SetCloudUserPolicies(std::move(policies));
  }

  base::DictValue CreateProxyRule(
      const std::string& host,
      const std::string& proxy,
      std::optional<base::ListValue> conditions = std::nullopt) {
    base::DictValue rule;
    rule.Set(proxy_config::kKeyDestinationMatchers,
             base::ListValue().Append(host));
    rule.Set(proxy_config::kKeyProxyList, base::ListValue().Append(proxy));
    if (conditions) {
      rule.Set(proxy_config::kKeyConditions, std::move(*conditions));
    }
    return rule;
  }

  base::DictValue CreateDnsCondition(const std::string& host,
                                     const std::string& result) {
    base::DictValue dns_probe;
    dns_probe.Set(proxy_config::kKeyHost, host);
    dns_probe.Set(proxy_config::kKeyResult, result);
    base::DictValue condition;
    condition.Set(proxy_config::kKeyDnsProbe, std::move(dns_probe));
    return condition;
  }

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_ANDROID)
  void SetEnableForAllUsers(bool enable) {
    base::flat_map<std::string, std::optional<base::Value>> policies;
    policies.emplace(policy::key::kEnableProxyOverrideRulesForAllUsers,
                     base::Value(enable ? 1 : 0));
    if (is_platform_managed_) {
      management_context_mixin_->SetPlatformPolicies(std::move(policies));
    } else {
      management_context_mixin_->SetCloudMachinePolicies(std::move(policies));
    }
  }
#endif

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  const bool is_platform_managed_ = false;
  std::unique_ptr<ManagementContextMixin> management_context_mixin_;
};

class ProxyOverrideRulesBrowserTest : public ProxyOverrideRulesTestBase {
 public:
  ProxyOverrideRulesBrowserTest()
      : ProxyOverrideRulesTestBase(ManagementContext{
            .is_cloud_user_managed = true,
// This is required to be able to set the machine policy.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_ANDROID)
            .is_cloud_machine_managed = true,
#endif
            .affiliated = true,
        }) {
  }
};

// Verifies that a simple rule matching the destination URL correctly routes
// traffic to the configured proxy (Synchronous path).
IN_PROC_BROWSER_TEST_F(ProxyOverrideRulesBrowserTest, AppliesRule) {
  base::ListValue rules;
  rules.Append(CreateProxyRule(
      kDestinationHost,
      "PROXY " + embedded_test_server()->host_port_pair().ToString()));

  SetUserProxyOverrideRules(std::move(rules));

  // Use a port that is not open on the destination. If the proxy is used, the
  // request will go to the embedded_test_server (which ignores the port in the
  // request line) and succeed. If direct, it will fail.
  GURL destination_url = GURL(base::StringPrintf(
      "http://%s:%d/simple.html", kDestinationHost, kUnreachablePort));

  ASSERT_TRUE(chrome_test_utils::NavigateToURL(
      chrome_test_utils::GetActiveWebContents(this), destination_url));
}

// Verifies that when the ProxyOverrideRules policy is set but does not contain
// any rules matching the destination URL, the system falls back to DIRECT
// (which fails in this test setup because the targeted port is closed).
IN_PROC_BROWSER_TEST_F(ProxyOverrideRulesBrowserTest, NoRulesMatch) {
  base::ListValue rules;
  // Rule matches a different host.
  rules.Append(CreateProxyRule(
      "other.com",
      "PROXY " + embedded_test_server()->host_port_pair().ToString()));

  SetUserProxyOverrideRules(std::move(rules));

  // Navigation should fail because the rule doesn't match, so it falls back to
  // DIRECT, which fails because the destination port is closed.
  GURL destination_url = GURL(base::StringPrintf(
      "http://%s:%d/simple.html", kDestinationHost, kUnreachablePort));
  EXPECT_FALSE(chrome_test_utils::NavigateToURL(
      chrome_test_utils::GetActiveWebContents(this), destination_url));
}

// Verifies that ProxyOverrideRules take precedence over the global
// ProxySettings policy when an override rule matches the destination.
IN_PROC_BROWSER_TEST_F(ProxyOverrideRulesBrowserTest,
                       PrecedenceOverProxySettings) {
  // Configure ProxySettings to use an invalid proxy.
  base::DictValue proxy_settings;
  proxy_settings.Set(policy::key::kProxyMode, "fixed_servers");
  proxy_settings.Set(policy::key::kProxyServer, "invalid.com:12345");

  // Configure ProxyOverrideRules to use the valid proxy for kDestinationHost.
  base::ListValue rules;
  rules.Append(CreateProxyRule(
      kDestinationHost,
      "PROXY " + embedded_test_server()->host_port_pair().ToString()));

  // Apply both policies.
  base::flat_map<std::string, std::optional<base::Value>> policies;
  policies.emplace(policy::key::kProxySettings,
                   base::Value(std::move(proxy_settings)));
  policies.emplace(policy::key::kProxyOverrideRules,
                   base::Value(std::move(rules)));
  management_context_mixin_->SetCloudUserPolicies(std::move(policies));

  // Navigation should succeed because the override rule takes precedence.
  GURL destination_url = GURL(base::StringPrintf(
      "http://%s:%d/simple.html", kDestinationHost, kUnreachablePort));
  EXPECT_TRUE(chrome_test_utils::NavigateToURL(
      chrome_test_utils::GetActiveWebContents(this), destination_url));
}

// Verifies that a rule with a satisfied DnsProbe condition correctly routes
// traffic to the proxy (Asynchronous path).
IN_PROC_BROWSER_TEST_F(ProxyOverrideRulesBrowserTest,
                       AppliesRuleWithDnsCondition) {
  base::ListValue conditions;
  conditions.Append(
      CreateDnsCondition(kResolvableHost, proxy_config::kResultResolved));

  base::ListValue rules;
  rules.Append(CreateProxyRule(
      kDestinationHost,
      "PROXY " + embedded_test_server()->host_port_pair().ToString(),
      std::move(conditions)));

  SetUserProxyOverrideRules(std::move(rules));

  GURL destination_url = GURL(base::StringPrintf(
      "http://%s:%d/simple.html", kDestinationHost, kUnreachablePort));
  EXPECT_TRUE(chrome_test_utils::NavigateToURL(
      chrome_test_utils::GetActiveWebContents(this), destination_url));
}

// Verifies that rule ordering determines priority: the first matching rule in
// the list is applied even if subsequent rules also match.
IN_PROC_BROWSER_TEST_F(ProxyOverrideRulesBrowserTest, AppliesPriority) {
  // Rule 1: Matches destination and DNS condition (Resolved). Valid Proxy.
  base::ListValue conditions;
  conditions.Append(
      CreateDnsCondition(kResolvableHost, proxy_config::kResultResolved));

  base::ListValue rules;
  rules.Append(CreateProxyRule(
      kDestinationHost,
      "PROXY " + embedded_test_server()->host_port_pair().ToString(),
      std::move(conditions)));

  // Rule 2: Matches destination. Invalid Proxy.
  rules.Append(CreateProxyRule(kDestinationHost, "PROXY invalid.com:12345"));

  SetUserProxyOverrideRules(std::move(rules));

  // Should succeed if Rule 1 is chosen.
  GURL destination_url = GURL(base::StringPrintf(
      "http://%s:%d/simple.html", kDestinationHost, kUnreachablePort));
  EXPECT_TRUE(chrome_test_utils::NavigateToURL(
      chrome_test_utils::GetActiveWebContents(this), destination_url));
}

// Verifies that if a higher-priority rule matches the destination but its DNS
// condition fails, the system correctly falls back to the next matching rule.
IN_PROC_BROWSER_TEST_F(ProxyOverrideRulesBrowserTest, AppliesPriorityFallback) {
  // Rule 1: Matches destination but DNS condition fails (Unresolvable). Invalid
  // Proxy.
  base::ListValue conditions;
  conditions.Append(
      CreateDnsCondition(kUnresolvableHost, proxy_config::kResultResolved));

  base::ListValue rules;
  rules.Append(CreateProxyRule(kDestinationHost, "PROXY invalid.com:12345",
                               std::move(conditions)));

  // Rule 2: Matches destination. Valid Proxy.
  rules.Append(CreateProxyRule(
      kDestinationHost,
      "PROXY " + embedded_test_server()->host_port_pair().ToString()));

  SetUserProxyOverrideRules(std::move(rules));

  // Should succeed if Rule 2 is chosen (Rule 1 skipped).
  GURL destination_url = GURL(base::StringPrintf(
      "http://%s:%d/simple.html", kDestinationHost, kUnreachablePort));
  ASSERT_TRUE(chrome_test_utils::NavigateToURL(
      chrome_test_utils::GetActiveWebContents(this), destination_url));
}

// Verifies that if ProxyOverrideRules are configured but none match the
// destination, the configuration correctly falls back to the ProxySettings
// policy.
IN_PROC_BROWSER_TEST_F(ProxyOverrideRulesBrowserTest, FallbackToProxySettings) {
  // Configure ProxySettings to use the valid proxy.
  base::DictValue proxy_settings;
  proxy_settings.Set(policy::key::kProxyMode, "fixed_servers");
  proxy_settings.Set(policy::key::kProxyServer,
                     embedded_test_server()->host_port_pair().ToString());

  // Configure ProxyOverrideRules with a rule that does NOT match
  // kDestinationHost.
  base::ListValue rules;
  rules.Append(CreateProxyRule("other.com", "PROXY invalid.com:12345"));

  // Apply both policies.
  base::flat_map<std::string, std::optional<base::Value>> policies;
  policies.emplace(policy::key::kProxySettings,
                   base::Value(std::move(proxy_settings)));
  policies.emplace(policy::key::kProxyOverrideRules,
                   base::Value(std::move(rules)));
  management_context_mixin_->SetCloudUserPolicies(std::move(policies));

  // Navigation should succeed because the override rule doesn't match, so it
  // falls back to ProxySettings, which points to the valid proxy.
  GURL destination_url = GURL(base::StringPrintf(
      "http://%s:%d/simple.html", kDestinationHost, kUnreachablePort));
  EXPECT_TRUE(chrome_test_utils::NavigateToURL(
      chrome_test_utils::GetActiveWebContents(this), destination_url));
}

#if BUILDFLAG(IS_CHROMEOS)
// Verifies that cloud-policy-configured ProxyOverrideRules work alongside a
// per-network PAC script configured via the OpenNetworkConfiguration policy.
IN_PROC_BROWSER_TEST_F(ProxyOverrideRulesBrowserTest, WorksWithOncPac) {
  // Set up a connected Wi-Fi network in the user's Shill profile.
  constexpr char kUserProfilePath[] = "user_profile";
  constexpr char kWifiServicePath[] = "stub_wifi";
  constexpr char kWifiGuid[] = "{wifi_guid}";
  constexpr char kWifiSsid[] = "wifi0";

  Profile* profile = chrome_test_utils::GetProfile(this);
  const std::string user_hash =
      ash::ProfileHelper::GetUserIdHashFromProfile(profile);
  ash::ShillProfileClient::TestInterface* profile_test =
      ash::ShillProfileClient::Get()->GetTestInterface();
  ash::ShillServiceClient::TestInterface* service_test =
      ash::ShillServiceClient::Get()->GetTestInterface();
  profile_test->AddProfile(kUserProfilePath, user_hash);
  service_test->ClearServices();
  service_test->AddService(kWifiServicePath, kWifiGuid, kWifiSsid,
                           shill::kTypeWifi, shill::kStateOnline,
                           /*visible=*/true);
  service_test->SetServiceProperty(kWifiServicePath, shill::kProfileProperty,
                                   base::Value(kUserProfilePath));
  profile_test->AddService(kUserProfilePath, kWifiServicePath);
  ASSERT_TRUE(base::test::RunUntil([&]() {
    const ash::NetworkState* default_network =
        ash::NetworkHandler::Get()->network_state_handler()->DefaultNetwork();
    return default_network && default_network->guid() == kWifiGuid &&
           default_network->profile_path() == kUserProfilePath &&
           ash::NetworkHandler::Get()
                   ->network_profile_handler()
                   ->GetProfileForPath(kUserProfilePath) != nullptr;
  }));

  content::WebContents* web_contents =
      chrome_test_utils::GetActiveWebContents(this);
  const GURL por_url(base::StringPrintf("http://%s:%d/simple.html",
                                        kDestinationHost, kUnreachablePort));
  const GURL pac_url(base::StringPrintf("http://%s:%d/simple.html",
                                        kPacDestinationHost, kUnreachablePort));
  const GURL direct_url(base::StringPrintf(
      "http://%s:%d/simple.html", kDirectDestinationHost, kUnreachablePort));

  // Before any proxy configuration is applied, direct connections to the
  // unreachable port fail.
  EXPECT_FALSE(chrome_test_utils::NavigateToURL(web_contents, por_url));
  EXPECT_FALSE(chrome_test_utils::NavigateToURL(web_contents, pac_url));

  // Apply OpenNetworkConfiguration (configuring a per-Wi-Fi PAC URL) first and
  // verify traffic routes according to the PAC script before ProxyOverrideRules
  // are configured.
  const std::string onc_policy = base::StringPrintf(
      R"({
        "NetworkConfigurations": [{
          "GUID": "%s",
          "Name": "%s",
          "ProxySettings": {
            "Type": "PAC",
            "PAC": "%s"
          },
          "Type": "WiFi",
          "WiFi": {
            "AutoConnect": true,
            "HiddenSSID": false,
            "SSID": "%s",
            "Security": "None"
          }
        }]
      })",
      kWifiGuid, kWifiSsid,
      embedded_test_server()->GetURL("/proxy.pac").spec().c_str(), kWifiSsid);

  base::flat_map<std::string, std::optional<base::Value>> policies;
  policies.emplace(policy::key::kOpenNetworkConfiguration,
                   base::Value(onc_policy));
  management_context_mixin_->SetCloudUserPolicies(std::move(policies));

  ProfileNetworkContextService* network_context_service =
      ProfileNetworkContextServiceFactory::GetForContext(profile);
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return !profile->GetPrefs()
                ->GetList(::onc::prefs::kOpenNetworkConfiguration)
                .empty() &&
           network_context_service->IsProxyConfigured() == true;
  }));
  network_context_service->FlushProxyConfigMonitorForTesting();

  // Once the ONC PAC policy is applied, `kPacDestinationHost` routes through
  // the proxy and succeeds, while `kDestinationHost` falls back to DIRECT and
  // fails.
  EXPECT_TRUE(chrome_test_utils::NavigateToURL(web_contents, pac_url));
  EXPECT_FALSE(chrome_test_utils::NavigateToURL(web_contents, por_url));

  // Now apply ProxyOverrideRules (routing `kDestinationHost` through the proxy)
  // via cloud user policy while keeping the ONC policy active.
  base::ListValue rules;
  rules.Append(CreateProxyRule(
      kDestinationHost,
      "PROXY " + embedded_test_server()->host_port_pair().ToString()));
  SetUserProxyOverrideRules(std::move(rules));
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return !profile->GetPrefs()
                ->GetList(proxy_config::prefs::kProxyOverrideRules)
                .empty();
  }));
  {
    // Drain the UI-thread `UpdateProxyConfig` task posted when
    // `kProxyOverrideRules` changed before flushing the Mojo pipe.
    base::RunLoop run_loop;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }
  network_context_service->FlushProxyConfigMonitorForTesting();

  // 1. Traffic matching the cloud policy ProxyOverrideRules
  //    (`kDestinationHost`) is routed through the proxy.
  EXPECT_TRUE(chrome_test_utils::NavigateToURL(web_contents, por_url));

  // 2. Traffic not matching ProxyOverrideRules falls back to the per-network
  //    ONC PAC script, which routes `kPacDestinationHost` through the proxy.
  EXPECT_TRUE(chrome_test_utils::NavigateToURL(web_contents, pac_url));

  // 3. Traffic matching neither ProxyOverrideRules nor the PAC script's proxy
  //    rule uses DIRECT and fails on the unreachable port.
  EXPECT_FALSE(chrome_test_utils::NavigateToURL(web_contents, direct_url));
}
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_ANDROID)
class ProxyOverrideRulesUnaffiliatedBrowserTest
    : public ProxyOverrideRulesTestBase {
 public:
  ProxyOverrideRulesUnaffiliatedBrowserTest()
      : ProxyOverrideRulesTestBase(ManagementContext{
            .is_cloud_user_managed = true,
            .is_cloud_machine_managed = true,
            .affiliated = false,
        }) {}
};

// Verifies that ProxyOverrideRules are ignored for unaffiliated users when
// EnableProxyOverrideRulesForAllUsers is not set (default 0).
IN_PROC_BROWSER_TEST_F(ProxyOverrideRulesUnaffiliatedBrowserTest,
                       IgnoredByDefault) {
  base::ListValue rules;
  rules.Append(CreateProxyRule(
      kDestinationHost,
      "PROXY " + embedded_test_server()->host_port_pair().ToString()));

  SetUserProxyOverrideRules(std::move(rules));

  GURL destination_url = GURL(base::StringPrintf(
      "http://%s:%d/simple.html", kDestinationHost, kUnreachablePort));
  EXPECT_FALSE(chrome_test_utils::NavigateToURL(
      chrome_test_utils::GetActiveWebContents(this), destination_url));
}

// Verifies that ProxyOverrideRules are ignored for unaffiliated users when
// EnableProxyOverrideRulesForAllUsers is explicitly disabled.
IN_PROC_BROWSER_TEST_F(ProxyOverrideRulesUnaffiliatedBrowserTest,
                       IgnoredWhenDisabled) {
  SetEnableForAllUsers(false);

  base::ListValue rules;
  rules.Append(CreateProxyRule(
      kDestinationHost,
      "PROXY " + embedded_test_server()->host_port_pair().ToString()));

  SetUserProxyOverrideRules(std::move(rules));

  GURL destination_url = GURL(base::StringPrintf(
      "http://%s:%d/simple.html", kDestinationHost, kUnreachablePort));
  EXPECT_FALSE(chrome_test_utils::NavigateToURL(
      chrome_test_utils::GetActiveWebContents(this), destination_url));
}

// Verifies that ProxyOverrideRules are applied for unaffiliated users when
// EnableProxyOverrideRulesForAllUsers is enabled.
IN_PROC_BROWSER_TEST_F(ProxyOverrideRulesUnaffiliatedBrowserTest,
                       AppliedWhenEnabled) {
  SetEnableForAllUsers(true);

  base::ListValue rules;
  rules.Append(CreateProxyRule(
      kDestinationHost,
      "PROXY " + embedded_test_server()->host_port_pair().ToString()));

  SetUserProxyOverrideRules(std::move(rules));

  GURL destination_url = GURL(base::StringPrintf(
      "http://%s:%d/simple.html", kDestinationHost, kUnreachablePort));
  EXPECT_TRUE(chrome_test_utils::NavigateToURL(
      chrome_test_utils::GetActiveWebContents(this), destination_url));
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
// Verifies that dynamically toggling EnableProxyOverrideRulesForAllUsers
// after ProxyOverrideRules are already set immediately updates the active
// proxy configuration for an unaffiliated user.
IN_PROC_BROWSER_TEST_F(ProxyOverrideRulesUnaffiliatedBrowserTest,
                       DynamicallyTogglesEnableForAllUsers) {
  base::ListValue rules;
  rules.Append(CreateProxyRule(
      kDestinationHost,
      "PROXY " + embedded_test_server()->host_port_pair().ToString()));

  SetUserProxyOverrideRules(std::move(rules));

  GURL destination_url = GURL(base::StringPrintf(
      "http://%s:%d/simple.html", kDestinationHost, kUnreachablePort));
  EXPECT_FALSE(chrome_test_utils::NavigateToURL(
      chrome_test_utils::GetActiveWebContents(this), destination_url));

  SetEnableForAllUsers(true);
  EXPECT_TRUE(chrome_test_utils::NavigateToURL(
      chrome_test_utils::GetActiveWebContents(this), destination_url));

  SetEnableForAllUsers(false);
  EXPECT_FALSE(chrome_test_utils::NavigateToURL(
      chrome_test_utils::GetActiveWebContents(this), destination_url));
}

// Verifies that dynamically toggling the user's affiliation status after
// ProxyOverrideRules are already persisted in prefs immediately updates the
// active proxy configuration.
// TODO(crbug.com/481444351): Once extension-level affiliation checks land,
// unaffiliated extensions will also be prevented from writing the pref and will
// have their rules cleared when becoming unaffiliated.
IN_PROC_BROWSER_TEST_F(ProxyOverrideRulesBrowserTest,
                       DynamicallyTogglesAffiliation) {
  base::ListValue rules;
  rules.Append(CreateProxyRule(
      kDestinationHost,
      "PROXY " + embedded_test_server()->host_port_pair().ToString()));

  // Set the policy while affiliated so `ProxyOverrideRulesPolicyHandler`
  // persists the preference value.
  SetUserProxyOverrideRules(std::move(rules));

  GURL destination_url = GURL(base::StringPrintf(
      "http://%s:%d/simple.html", kDestinationHost, kUnreachablePort));
  EXPECT_TRUE(chrome_test_utils::NavigateToURL(
      chrome_test_utils::GetActiveWebContents(this), destination_url));

  management_context_mixin_->SetAffiliation(false);
  EXPECT_FALSE(chrome_test_utils::NavigateToURL(
      chrome_test_utils::GetActiveWebContents(this), destination_url));

  management_context_mixin_->SetAffiliation(true);
  EXPECT_TRUE(chrome_test_utils::NavigateToURL(
      chrome_test_utils::GetActiveWebContents(this), destination_url));
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

// Verifies that ProxyOverrideRules are applied for affiliated users even when
// EnableProxyOverrideRulesForAllUsers is disabled.
IN_PROC_BROWSER_TEST_F(ProxyOverrideRulesBrowserTest, AppliedWhenDisabled) {
  SetEnableForAllUsers(false);

  base::ListValue rules;
  rules.Append(CreateProxyRule(
      kDestinationHost,
      "PROXY " + embedded_test_server()->host_port_pair().ToString()));

  SetUserProxyOverrideRules(std::move(rules));

  GURL destination_url = GURL(base::StringPrintf(
      "http://%s:%d/simple.html", kDestinationHost, kUnreachablePort));
  EXPECT_TRUE(chrome_test_utils::NavigateToURL(
      chrome_test_utils::GetActiveWebContents(this), destination_url));
}

// Verifies that ProxyOverrideRules are applied for affiliated users when
// EnableProxyOverrideRulesForAllUsers is enabled.
IN_PROC_BROWSER_TEST_F(ProxyOverrideRulesBrowserTest, AppliedWhenEnabled) {
  SetEnableForAllUsers(true);

  base::ListValue rules;
  rules.Append(CreateProxyRule(
      kDestinationHost,
      "PROXY " + embedded_test_server()->host_port_pair().ToString()));

  SetUserProxyOverrideRules(std::move(rules));

  GURL destination_url = GURL(base::StringPrintf(
      "http://%s:%d/simple.html", kDestinationHost, kUnreachablePort));
  EXPECT_TRUE(chrome_test_utils::NavigateToURL(
      chrome_test_utils::GetActiveWebContents(this), destination_url));
}

class ProxyOverrideRulesUnmanagedDeviceBrowserTest
    : public ProxyOverrideRulesTestBase {
 public:
  ProxyOverrideRulesUnmanagedDeviceBrowserTest()
      : ProxyOverrideRulesTestBase(ManagementContext{
            .is_cloud_user_managed = true,
            .is_cloud_machine_managed = false,
        }) {}
};

// Verifies that ProxyOverrideRules are applied when the device is unmanaged,
// even if EnableProxyOverrideRulesForAllUsers is not set.
IN_PROC_BROWSER_TEST_F(ProxyOverrideRulesUnmanagedDeviceBrowserTest,
                       WorksOnUnmanagedDevice) {
  base::ListValue rules;
  rules.Append(CreateProxyRule(
      kDestinationHost,
      "PROXY " + embedded_test_server()->host_port_pair().ToString()));

  SetUserProxyOverrideRules(std::move(rules));

  GURL destination_url = GURL(base::StringPrintf(
      "http://%s:%d/simple.html", kDestinationHost, kUnreachablePort));
  EXPECT_TRUE(chrome_test_utils::NavigateToURL(
      chrome_test_utils::GetActiveWebContents(this), destination_url));
}

class ProxyOverrideRulesPlatformManagedDeviceBrowserTest
    : public ProxyOverrideRulesTestBase {
 public:
  ProxyOverrideRulesPlatformManagedDeviceBrowserTest()
      : ProxyOverrideRulesTestBase(ManagementContext{
            .is_cloud_user_managed = true,
            .is_cloud_machine_managed = false,
            .is_platform_managed = true,
            .affiliated = false,
        }) {}

 private:
  base::test::ScopedFeatureList affiliation_feature_list_{
      enterprise_net::kEnforceProxyAffiliationOnPlatformManagedDevices};
};

// Verifies that user-scoped ProxyOverrideRules on a platform/EMM-managed
// device without CBCM are ignored by default and applied when
// EnableProxyOverrideRulesForAllUsers is enabled.
IN_PROC_BROWSER_TEST_F(ProxyOverrideRulesPlatformManagedDeviceBrowserTest,
                       PlatformManagedDeviceWithoutCbcm) {
  base::ListValue rules;
  rules.Append(CreateProxyRule(
      kDestinationHost,
      "PROXY " + embedded_test_server()->host_port_pair().ToString()));

  SetUserProxyOverrideRules(std::move(rules));

  GURL destination_url = GURL(base::StringPrintf(
      "http://%s:%d/simple.html", kDestinationHost, kUnreachablePort));
  EXPECT_FALSE(chrome_test_utils::NavigateToURL(
      chrome_test_utils::GetActiveWebContents(this), destination_url));

  SetEnableForAllUsers(true);
  EXPECT_TRUE(chrome_test_utils::NavigateToURL(
      chrome_test_utils::GetActiveWebContents(this), destination_url));
}

class ProxyOverrideRulesPlatformManagedDeviceNotEnforcedBrowserTest
    : public ProxyOverrideRulesTestBase {
 public:
  ProxyOverrideRulesPlatformManagedDeviceNotEnforcedBrowserTest()
      : ProxyOverrideRulesTestBase(ManagementContext{
            .is_cloud_user_managed = true,
            .is_cloud_machine_managed = false,
            .is_platform_managed = true,
            .affiliated = false,
        }) {
    affiliation_feature_list_.InitAndDisableFeature(
        enterprise_net::kEnforceProxyAffiliationOnPlatformManagedDevices);
  }

 private:
  base::test::ScopedFeatureList affiliation_feature_list_;
};

// Verifies that user-scoped ProxyOverrideRules on a platform/EMM-managed
// device without CBCM are applied when
// `kEnforceProxyAffiliationOnPlatformManagedDevices` is disabled, even if
// EnableProxyOverrideRulesForAllUsers is not set.
IN_PROC_BROWSER_TEST_F(
    ProxyOverrideRulesPlatformManagedDeviceNotEnforcedBrowserTest,
    AppliedWithoutEnableForAllUsers) {
  constexpr char kPolicyImpactHistogram[] =
      "Enterprise.ProxyOverrideRules.PlatformAffiliationImpact.Policy";
  base::HistogramTester histogram_tester;

  base::ListValue rules;
  rules.Append(CreateProxyRule(
      kDestinationHost,
      "PROXY " + embedded_test_server()->host_port_pair().ToString()));

  SetUserProxyOverrideRules(std::move(rules));

  GURL destination_url = GURL(base::StringPrintf(
      "http://%s:%d/simple.html", kDestinationHost, kUnreachablePort));
  EXPECT_TRUE(chrome_test_utils::NavigateToURL(
      chrome_test_utils::GetActiveWebContents(this), destination_url));

#if !BUILDFLAG(IS_ANDROID)
  // Opening an incognito window creates an OTR ProxyConfigMonitor; verify it
  // does not emit a duplicate sample.
  CreateIncognitoBrowser();
#endif

  // Emitted once for the regular profile (not duplicated by Safe Browsing or
  // Incognito ProxyConfigMonitors).
  histogram_tester.ExpectUniqueSample(
      kPolicyImpactHistogram,
      proxy_config::ProxyPlatformAffiliationImpact::kWouldBeBlocked, 1);

  SetEnableForAllUsers(true);
  histogram_tester.ExpectBucketCount(
      kPolicyImpactHistogram,
      proxy_config::ProxyPlatformAffiliationImpact::kAllowedByForAllUsers, 1);
  histogram_tester.ExpectTotalCount(kPolicyImpactHistogram, 2);
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_ANDROID)

}  // namespace enterprise::test
