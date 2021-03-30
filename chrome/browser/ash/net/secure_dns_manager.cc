// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/net/secure_dns_manager.h"

#include <algorithm>
#include <map>
#include <string>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/browser/net/secure_dns_config.h"
#include "chrome/browser/net/secure_dns_util.h"
#include "chrome/common/pref_names.h"
#include "chromeos/network/network_configuration_handler.h"
#include "chromeos/network/network_handler.h"
#include "components/country_codes/country_codes.h"
#include "net/dns/public/secure_dns_mode.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace net {

SecureDnsManager::SecureDnsManager(PrefService* pref_service) {
  registrar_.Init(pref_service);
  registrar_.Add(prefs::kDnsOverHttpsMode,
                 base::BindRepeating(&SecureDnsManager::OnPrefChanged,
                                     base::Unretained(this)));
  registrar_.Add(prefs::kDnsOverHttpsTemplates,
                 base::BindRepeating(&SecureDnsManager::OnPrefChanged,
                                     base::Unretained(this)));
  LoadProviders();
  OnPrefChanged();
}

SecureDnsManager::~SecureDnsManager() {
  registrar_.RemoveAll();
}

void SecureDnsManager::LoadProviders() {
  auto local_providers = chrome_browser_net::secure_dns::ProvidersForCountry(
      net::DohProviderEntry::GetList(), country_codes::GetCurrentCountryID());
  local_providers = chrome_browser_net::secure_dns::RemoveDisabledProviders(
      local_providers, chrome_browser_net::secure_dns::GetDisabledProviders());

  for (const auto* provider : local_providers) {
    std::vector<std::string> ip_addrs;
    std::transform(provider->ip_addresses.begin(), provider->ip_addresses.end(),
                   std::back_inserter(ip_addrs),
                   [](const IPAddress& addr) { return addr.ToString(); });
    local_doh_providers_[provider->dns_over_https_template] =
        base::JoinString(ip_addrs, ",");
  }
}

base::Value SecureDnsManager::GetProviders(const std::string& mode,
                                           const std::string& templates) {
  base::Value doh_providers(base::Value::Type::DICTIONARY);

  if (mode == SecureDnsConfig::kModeOff)
    return doh_providers.Clone();

  // If there are templates then use them. In secure mode, the values, which
  // hold the IP addresses of the name servers, are left empty. In automatic
  // mode, the corresponding name servers will be populated using the
  // applicable providers. If no templates are given for automatic mode, the
  // entire list of providers is used. This enables dns-proxy to correctly
  // switch providers whenever the tracked network or its settings change.
  for (const auto& doh_template : base::SplitString(
           templates, " ", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    doh_providers.SetKey(doh_template, base::Value(""));
  }
  if (mode == SecureDnsConfig::kModeSecure)
    return doh_providers.Clone();

  const bool want_all = doh_providers.DictEmpty();
  for (const auto& provider : local_doh_providers_) {
    if (want_all || doh_providers.FindKey(provider.first)) {
      doh_providers.SetKey(provider.first, base::Value(provider.second));
    }
  }
  return doh_providers.Clone();
}

void SecureDnsManager::OnPrefChanged() {
  const auto doh_providers = GetProviders(
      registrar_.prefs()->GetString(prefs::kDnsOverHttpsMode),
      registrar_.prefs()->GetString(prefs::kDnsOverHttpsTemplates));

  chromeos::NetworkHandler::Get()
      ->network_configuration_handler()
      ->SetManagerProperty(shill::kDNSProxyDOHProvidersProperty, doh_providers);
}

}  // namespace net
