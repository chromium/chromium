// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/net/secure_dns_manager.h"

#include <map>
#include <string>

#include "base/check_is_test.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/browser/ash/net/dns_over_https/templates_uri_resolver_impl.h"
#include "chrome/browser/net/secure_dns_config.h"
#include "chrome/browser/net/secure_dns_util.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/network/network_configuration_handler.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_metadata_store.h"
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
  registrar_.RemoveAll();
}

void SecureDnsManager::SetDoHTemplatesUriResolverForTesting(
    std::unique_ptr<dns_over_https::TemplatesUriResolver>
        doh_templates_uri_resolver) {
  CHECK_IS_TEST();
  doh_templates_uri_resolver_ = std::move(doh_templates_uri_resolver);
}

void SecureDnsManager::LoadProviders() {
  const net::DohProviderEntry::List local_providers =
      chrome_browser_net::secure_dns::SelectEnabledProviders(
          chrome_browser_net::secure_dns::ProvidersForCountry(
              net::DohProviderEntry::GetList(),
              country_codes::GetCurrentCountryID()));

  for (const auto* provider : local_providers) {
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
  // hold the IP addresses of the name servers, are left empty. In automatic
  // mode, the corresponding name servers will be populated using the
  // applicable providers. If no templates are given for automatic mode, the
  // entire list of providers is used. This enables dns-proxy to correctly
  // switch providers whenever the tracked network or its settings change.
  for (const auto& doh_template : base::SplitString(
           templates, " ", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    doh_providers.Set(doh_template, "");
  }
  if (mode == SecureDnsConfig::kModeSecure) {
    return doh_providers;
  }

  const bool want_all = doh_providers.empty();
  for (const auto& provider : local_doh_providers_) {
    const std::string& server_template = provider.first.server_template();
    if (want_all || doh_providers.contains(server_template)) {
      doh_providers.Set(server_template, provider.second);
    }
  }
  return doh_providers;
}

void SecureDnsManager::OnPrefChanged() {
  doh_templates_uri_resolver_->UpdateFromPrefs(pref_service_);

  base::Value::Dict doh_providers =
      GetProviders(registrar_.prefs()->GetString(prefs::kDnsOverHttpsMode),
                   doh_templates_uri_resolver_->GetEffectiveTemplates());

  NetworkHandler::Get()->network_configuration_handler()->SetManagerProperty(
      shill::kDNSProxyDOHProvidersProperty,
      base::Value(std::move(doh_providers)));

  NetworkHandler::Get()
      ->network_metadata_store()
      ->set_secure_dns_templates_with_identifiers_active(
          doh_templates_uri_resolver_->GetDohWithIdentifiersActive());
}

}  // namespace ash
