// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/enterprise/test/management_context_mixin.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
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

namespace enterprise::test {

namespace {

constexpr char kDestinationHost[] = "destination.com";
constexpr char kResolvableHost[] = "resolvable.com";
constexpr char kUnresolvableHost[] = "unresolvable.com";
constexpr int kUnreachablePort = 12345;

}  // namespace

class ProxyOverrideRulesTestBase : public MixinBasedPlatformBrowserTest {
 public:
  explicit ProxyOverrideRulesTestBase(const ManagementContext& context) {
    scoped_feature_list_.InitAndEnableFeature(kEnableProxyOverrideRules);
    management_context_mixin_ =
        ManagementContextMixin::Create(&mixin_host_, this, context);
  }

  void SetUpOnMainThread() override {
    MixinBasedPlatformBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule(kDestinationHost, "127.0.0.1");
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

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  void SetEnableForAllUsers(bool enable) {
    base::flat_map<std::string, std::optional<base::Value>> policies;
    policies.emplace(policy::key::kEnableProxyOverrideRulesForAllUsers,
                     base::Value(enable ? 1 : 0));
    management_context_mixin_->SetCloudMachinePolicies(std::move(policies));
  }
#endif

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<ManagementContextMixin> management_context_mixin_;
};

class ProxyOverrideRulesBrowserTest : public ProxyOverrideRulesTestBase {
 public:
  ProxyOverrideRulesBrowserTest()
      : ProxyOverrideRulesTestBase(ManagementContext{
            .is_cloud_user_managed = true,
// This is required to be able to set the machine policy.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
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

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
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
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

}  // namespace enterprise::test
