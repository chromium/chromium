// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/proxy_override_rules/proxy_override_rules_transformer.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/check.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/proxy_override_rules_private.h"
#include "components/policy/core/common/policy_service.h"
#include "components/proxy_config/proxy_prefs_utils.h"
#include "extensions/browser/pref_modifiability_observer.h"
#include "extensions/buildflags/buildflags.h"
#include "net/base/scheme_host_port_matcher_rule.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || \
    BUILDFLAG(IS_ANDROID)
#include "chrome/browser/extensions/api/proxy_override_rules/proxy_override_rules_modifiability_observer.h"
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) ||
        // BUILDFLAG(IS_ANDROID)

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

namespace {

namespace api_por = api::proxy_override_rules_private;

bool ValidateProxyList(const std::vector<std::string>& proxies,
                       std::string& error) {
  for (const auto& proxy : proxies) {
    net::ProxyChain chain =
        proxy_config::ProxyOverrideRuleProxyFromString(proxy);

    if (!chain.IsValid()) {
      error = base::StringPrintf("Invalid proxy: %s", proxy.c_str());
      return false;
    }
  }

  return true;
}

bool ValidateConditions(
    const std::vector<api_por::ProxyOverrideRuleCondition>& conditions,
    std::string& error) {
  for (const auto& condition : conditions) {
    if (!condition.dns_probe) {
      error = "Each condition must have exactly one probe type.";
      return false;
    }

    const api_por::DnsProbe& dns_probe = *condition.dns_probe;

    auto scheme_host_port =
        proxy_config::ProxyOverrideRuleHostFromString(dns_probe.host);

    if (!scheme_host_port.IsValid()) {
      error = base::StringPrintf("Invalid DnsProbe Host: %s",
                                 dns_probe.host.c_str());
      return false;
    }

    if (dns_probe.result == api_por::DnsProbeResult::kNone) {
      error = "Invalid DnsProbe Result.";
      return false;
    }
  }

  return true;
}

bool ValidateRule(const api_por::ProxyOverrideRule& rule, std::string& error) {
  if (!ValidateProxyList(rule.proxy_list, error)) {
    return false;
  }

  if (rule.conditions && !ValidateConditions(*rule.conditions, error)) {
    return false;
  }

  return true;
}
}  // namespace

ProxyOverrideRulesTransformer::ProxyOverrideRulesTransformer() = default;

ProxyOverrideRulesTransformer::~ProxyOverrideRulesTransformer() = default;

std::optional<base::Value>
ProxyOverrideRulesTransformer::ExtensionToBrowserPref(
    const base::Value& extension_pref,
    std::string& error,
    bool& bad_message) {
  if (!extension_pref.is_list()) {
    bad_message = true;
    return std::nullopt;
  }

  for (const auto& rule_value : extension_pref.GetList()) {
    auto rule = api_por::ProxyOverrideRule::FromValue(rule_value);
    if (!rule) {
      bad_message = true;
      return std::nullopt;
    }

    // ValidateRule will set `error` and return false if validation fails.
    if (!ValidateRule(*rule, error)) {
      return std::nullopt;
    }
  }

  return extension_pref.Clone();
}

std::optional<base::Value>
ProxyOverrideRulesTransformer::BrowserToExtensionPref(
    const base::Value& browser_pref,
    bool is_incognito_profile) {
  return browser_pref.Clone();
}

bool ProxyOverrideRulesTransformer::IsExtensionModifiable(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  CHECK(profile);
  policy::PolicyService* policy_service =
      profile->GetProfilePolicyConnector()
          ? profile->GetProfilePolicyConnector()->policy_service()
          : nullptr;
  return proxy_config::AreUserProxyOverrideRulesAllowed(profile->GetPrefs(),
                                                        policy_service);
}

std::unique_ptr<PrefModifiabilityObserver>
ProxyOverrideRulesTransformer::CreateModifiabilityObserver(
    content::BrowserContext* context) {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || \
    BUILDFLAG(IS_ANDROID)
  Profile* profile = Profile::FromBrowserContext(context);
  CHECK(profile);
  return std::make_unique<ProxyOverrideRulesModifiabilityObserver>(this,
                                                                   profile);
#else
  return nullptr;
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) ||
        // BUILDFLAG(IS_ANDROID)
}

}  // namespace extensions
