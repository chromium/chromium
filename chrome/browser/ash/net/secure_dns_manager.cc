// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/net/secure_dns_manager.h"

#include <map>
#include <string>
#include <string_view>

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
#include "chrome/browser/net/secure_dns_config.h"
#include "chrome/browser/net/secure_dns_util.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/network/device_state.h"
#include "chromeos/ash/components/network/network_configuration_handler.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_metadata_store.h"
#include "chromeos/ash/components/network/network_state.h"
#include "components/country_codes/country_codes.h"
#include "net/dns/public/doh_provider_entry.h"
#include "net/dns/public/secure_dns_mode.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash {

SecureDnsManager::SecureDnsManager(PrefService* pref_service)
    : pref_service_(pref_service) {
  doh_templates_uri_resolver_ =
      std::make_unique<dns_over_https::TemplatesUriResolverImpl>();
  registrar_.Init(pref_service);
  registrar_.Add(prefs::kDnsOverHttpsMode,
                 base::BindRepeating(&SecureDnsManager::OnPrefChanged,
                                     base::Unretained(this)));
  registrar_.Add(prefs::kDnsOverHttpsTemplates,
                 base::BindRepeating(&SecureDnsManager::OnPrefChanged,
                                     base::Unretained(this)));
  registrar_.Add(prefs::kDnsOverHttpsTemplatesWithIdentifiers,
                 base::BindRepeating(&SecureDnsManager::OnPrefChanged,
                                     base::Unretained(this)));
  registrar_.Add(prefs::kDnsOverHttpsSalt,
                 base::BindRepeating(&SecureDnsManager::OnPrefChanged,
                                     base::Unretained(this)));
  LoadProviders();
  OnPrefChanged();
}

SecureDnsManager::~SecureDnsManager() {
  // `pref_service_` outlives the SecureDnsManager instance. The value of
  // `prefs::kDnsOverHttpsEffectiveTemplatesChromeOS` should not outlive the
  // current instance of SecureDnsManager.
  pref_service_->ClearPref(prefs::kDnsOverHttpsEffectiveTemplatesChromeOS);
  registrar_.RemoveAll();
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
  const std::string& mode = pref_service_->GetString(prefs::kDnsOverHttpsMode);
  if (mode == SecureDnsConfig::kModeOff) {
    return;
  }

  // Network updates are only relevant for determining the effective DoH
  // template URI if the admin has configured the
  // DnsOverHttpsTemplatesWithIdentifiers policy to include the IP addresses.
  std::string templates_with_identifiers =
      pref_service_->GetString(prefs::kDnsOverHttpsTemplatesWithIdentifiers);
  if (!dns_over_https::TemplatesUriResolverImpl::
          IsDeviceIpAddressIncludedInUriTemplate(templates_with_identifiers)) {
    return;
  }
  UpdateTemplateUri();
}

void SecureDnsManager::OnPrefChanged() {
  UpdateTemplateUri();

  if (!doh_templates_uri_resolver_->GetDohWithIdentifiersActive()) {
    return;
  }

  // If DoH with identifiers are active, verify if network changes need to be
  // observed for URI template placeholder replacement.
  std::string templates_with_identifiers =
      pref_service_->GetString(prefs::kDnsOverHttpsTemplatesWithIdentifiers);

  bool should_observe_default_network_changes =
      dns_over_https::TemplatesUriResolverImpl::
          IsDeviceIpAddressIncludedInUriTemplate(templates_with_identifiers);

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

void SecureDnsManager::UpdateTemplateUri() {
  doh_templates_uri_resolver_->Update(pref_service_);

  const std::string effective_uri_templates =
      doh_templates_uri_resolver_->GetEffectiveTemplates();

  // Set the DoH URI template pref which is synced with Lacros and the
  // NetworkService.
  // TODO(acostinas, b/331903009): Storing the effective DoH providers in a
  // local_state pref on Chrome OS has downsides. Replace this pref with an
  // in-memory mechanism to sync effective DoH prefs.
  pref_service_->SetString(prefs::kDnsOverHttpsEffectiveTemplatesChromeOS,
                           effective_uri_templates);

  // Set the DoH URI template shill property which is synced with platform
  // daemons (shill, dns-proxy etc).
  base::Value::Dict doh_providers =
      GetProviders(registrar_.prefs()->GetString(prefs::kDnsOverHttpsMode),
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
