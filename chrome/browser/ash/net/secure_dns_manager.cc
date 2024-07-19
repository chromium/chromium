// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/net/secure_dns_manager.h"

#include <map>
#include <string>
#include <string_view>

#include "ash/constants/ash_pref_names.h"
#include "base/check_is_test.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/browser/ash/net/dns_over_https/templates_uri_resolver_impl.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/secure_dns_config.h"
#include "chrome/browser/net/secure_dns_util.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/network/device_state.h"
#include "chromeos/ash/components/network/network_configuration_handler.h"
#include "chromeos/ash/components/network/network_event_log.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_metadata_store.h"
#include "chromeos/ash/components/network/network_state.h"
#include "components/country_codes/country_codes.h"
#include "net/dns/public/doh_provider_entry.h"
#include "net/dns/public/secure_dns_mode.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash {

SecureDnsManager::SecureDnsManager(PrefService* local_state)
    : local_state_(local_state) {
  doh_templates_uri_resolver_ =
      std::make_unique<dns_over_https::TemplatesUriResolverImpl>();

  MonitorPolicyPrefs();
  LoadProviders();
  OnPolicyPrefChanged();
  OnDoHIncludedDomainsPrefChanged();
  OnDoHExcludedDomainsPrefChanged();
}

void SecureDnsManager::MonitorPolicyPrefs() {
  local_state_registrar_.Init(local_state_);
  static constexpr std::array<const char*, 4> secure_dns_pref_names = {
      ::prefs::kDnsOverHttpsMode, ::prefs::kDnsOverHttpsTemplates,
      ::prefs::kDnsOverHttpsTemplatesWithIdentifiers,
      ::prefs::kDnsOverHttpsSalt};
  for (auto* const pref_name : secure_dns_pref_names) {
    local_state_registrar_.Add(
        pref_name, base::BindRepeating(&SecureDnsManager::OnPolicyPrefChanged,
                                       base::Unretained(this)));
  }
  local_state_registrar_.Add(
      prefs::kDnsOverHttpsIncludedDomains,
      base::BindRepeating(&SecureDnsManager::OnDoHIncludedDomainsPrefChanged,
                          base::Unretained(this)));
  local_state_registrar_.Add(
      prefs::kDnsOverHttpsExcludedDomains,
      base::BindRepeating(&SecureDnsManager::OnDoHExcludedDomainsPrefChanged,
                          base::Unretained(this)));
}

SecureDnsManager::~SecureDnsManager() {
  // `local_state_` outlives the SecureDnsManager instance. The value of
  // `::prefs::kDnsOverHttpsEffectiveTemplatesChromeOS` should not outlive the
  // current instance of SecureDnsManager.
  local_state_->ClearPref(::prefs::kDnsOverHttpsEffectiveTemplatesChromeOS);
}

void SecureDnsManager::SetDoHTemplatesUriResolverForTesting(
    std::unique_ptr<dns_over_https::TemplatesUriResolver>
        doh_templates_uri_resolver) {
  CHECK_IS_TEST();
  doh_templates_uri_resolver_ = std::move(doh_templates_uri_resolver);
}

void SecureDnsManager::LoadProviders() {
  // Note: Check whether each provider is enabled *after* filtering based on
  // country code so that if we are doing experimentation via Finch for a
  // regional provider, the experiment groups will be less likely to include
  // users from other regions unnecessarily (since a client will be included in
  // the experiment if the provider feature flag is checked).
  const net::DohProviderEntry::List local_providers =
      chrome_browser_net::secure_dns::SelectEnabledProviders(
          chrome_browser_net::secure_dns::ProvidersForCountry(
              net::DohProviderEntry::GetList(),
              country_codes::GetCurrentCountryID()));

  for (const net::DohProviderEntry* provider : local_providers) {
    std::vector<std::string> ip_addrs;
    base::ranges::transform(provider->ip_addresses,
                            std::back_inserter(ip_addrs),
                            &net::IPAddress::ToString);
    local_doh_providers_[provider->doh_server_config] =
        base::JoinString(ip_addrs, ",");
  }
}

base::Value::Dict SecureDnsManager::GetProviders(const std::string& mode,
                                                 const std::string& templates) {
  base::Value::Dict doh_providers;

  if (mode == SecureDnsConfig::kModeOff) {
    return doh_providers;
  }

  // If there are templates then use them. In secure mode, the values, which
  // hold the IP addresses of the name servers, are left empty. In secure DNS
  // mode with fallback to plain-text nameservers, the values are stored as a
  // wildcard character denoting that it matches any IP addresses. In automatic
  // upgrade mode, the corresponding name servers will be populated using the
  // applicable providers.
  const std::string_view addr =
      mode == SecureDnsConfig::kModeSecure
          ? ""
          : shill::kDNSProxyDOHProvidersMatchAnyIPAddress;
  for (const auto& doh_template : base::SplitString(
           templates, " ", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    doh_providers.Set(doh_template, addr);
  }
  if (mode == SecureDnsConfig::kModeSecure) {
    return doh_providers;
  }
  if (!doh_providers.empty()) {
    return doh_providers;
  }

  // No specified DoH providers, relay all DoH provider upgrade configuration
  // for dns-proxy to switch providers whenever the network or its settings
  // change.
  for (const auto& provider : local_doh_providers_) {
    doh_providers.Set(provider.first.server_template(), provider.second);
  }
  return doh_providers;
}

void SecureDnsManager::DefaultNetworkChanged(const NetworkState* network) {
  const std::string& mode = local_state_->GetString(::prefs::kDnsOverHttpsMode);
  if (mode == SecureDnsConfig::kModeOff) {
    return;
  }

  // Network updates are only relevant for determining the effective DoH
  // template URI if the admin has configured the
  // DnsOverHttpsTemplatesWithIdentifiers policy to include the IP addresses.
  std::string templates_with_identifiers =
      local_state_->GetString(::prefs::kDnsOverHttpsTemplatesWithIdentifiers);
  if (!dns_over_https::TemplatesUriResolverImpl::
          IsDeviceIpAddressIncludedInUriTemplate(templates_with_identifiers)) {
    return;
  }
  UpdateTemplateUri();
}

void SecureDnsManager::OnPolicyPrefChanged() {
  UpdateTemplateUri();
  ToggleNetworkMonitoring();
}

void SecureDnsManager::ToggleNetworkMonitoring() {
  // If DoH with identifiers are active, verify if network changes need to be
  // observed for URI template placeholder replacement.
  std::string templates_with_identifiers =
      local_state_->GetString(::prefs::kDnsOverHttpsTemplatesWithIdentifiers);

  bool template_uri_includes_network_identifiers =
      doh_templates_uri_resolver_->GetDohWithIdentifiersActive() &&
      dns_over_https::TemplatesUriResolverImpl::
          IsDeviceIpAddressIncludedInUriTemplate(templates_with_identifiers);

  bool should_observe_default_network_changes =
      template_uri_includes_network_identifiers &&
      (local_state_->GetString(::prefs::kDnsOverHttpsMode) !=
       SecureDnsConfig::kModeOff);

  if (!should_observe_default_network_changes) {
    network_state_handler_observer_.Reset();
    return;
  }
  // Already observing default network changes.
  if (network_state_handler_observer_.IsObserving()) {
    return;
  }
  network_state_handler_observer_.Observe(
      NetworkHandler::Get()->network_state_handler());
}

void SecureDnsManager::OnDoHIncludedDomainsPrefChanged() {
  base::Value::List included_domains =
      local_state_->GetList(prefs::kDnsOverHttpsIncludedDomains).Clone();
  NetworkHandler::Get()->network_configuration_handler()->SetManagerProperty(
      shill::kDOHIncludedDomainsProperty,
      base::Value(std::move(included_domains)));

  // TODO(b/351091814): Proxy DoH packets from the browser using plain-text DNS
  // to DNS proxy. DNS proxy should be responsible for the DoH usage when domain
  // DoH config is set.
}

void SecureDnsManager::OnDoHExcludedDomainsPrefChanged() {
  base::Value::List excluded_domains =
      local_state_->GetList(prefs::kDnsOverHttpsExcludedDomains).Clone();
  NetworkHandler::Get()->network_configuration_handler()->SetManagerProperty(
      shill::kDOHExcludedDomainsProperty,
      base::Value(std::move(excluded_domains)));

  // TODO(b/351091814): Proxy DoH packets from the browser using plain-text DNS
  // to DNS proxy. DNS proxy should be responsible for the DoH usage when domain
  // DoH config is set.
}

void SecureDnsManager::UpdateTemplateUri() {
  doh_templates_uri_resolver_->Update(local_state_);

  const std::string effective_uri_templates =
      doh_templates_uri_resolver_->GetEffectiveTemplates();

  // Set the DoH URI template pref which is synced with Lacros and the
  // NetworkService.
  // TODO(acostinas, b/331903009): Storing the effective DoH providers in a
  // local_state pref on Chrome OS has downsides. Replace this pref with an
  // in-memory mechanism to sync effective DoH prefs.
  local_state_->SetString(::prefs::kDnsOverHttpsEffectiveTemplatesChromeOS,
                          effective_uri_templates);

  // Set the DoH URI template shill property which is synced with platform
  // daemons (shill, dns-proxy etc).
  base::Value::Dict doh_providers = GetProviders(
      local_state_registrar_.prefs()->GetString(::prefs::kDnsOverHttpsMode),
      effective_uri_templates);

  if (cached_doh_providers_ == doh_providers) {
    return;
  }

  cached_doh_providers_ = doh_providers.Clone();

  NetworkHandler::Get()->network_configuration_handler()->SetManagerProperty(
      shill::kDNSProxyDOHProvidersProperty,
      base::Value(std::move(doh_providers)));

  NetworkHandler::Get()
      ->network_metadata_store()
      ->SetSecureDnsTemplatesWithIdentifiersActive(
          doh_templates_uri_resolver_->GetDohWithIdentifiersActive());
}

}  // namespace ash
