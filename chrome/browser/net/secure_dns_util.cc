// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/secure_dns_util.h"

#include <iterator>
#include <memory>
#include <string>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/ranges/algorithm.h"
#include "build/build_config.h"
#include "chrome/browser/net/dns_probe_runner.h"
#include "chrome/common/chrome_features.h"
#include "components/country_codes/country_codes.h"
#include "components/embedder_support/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "net/dns/public/dns_config_overrides.h"
#include "net/dns/public/dns_over_https_config.h"
#include "net/dns/public/doh_provider_entry.h"
#include "net/dns/public/secure_dns_mode.h"

namespace chrome_browser_net::secure_dns {

namespace {

const char kAlternateErrorPagesBackup[] = "alternate_error_pages.backup";

bool EntryIsForCountry(const net::DohProviderEntry* entry, int country_id) {
  if (entry->display_globally) {
    return true;
  }
  const auto& countries = entry->display_countries;
  bool matches = base::ranges::any_of(
      countries, [country_id](const std::string& country_code) {
        return country_codes::CountryStringToCountryID(country_code) ==
               country_id;
      });
  if (matches) {
    DCHECK(!entry->ui_name.empty());
    DCHECK(!entry->privacy_policy.empty());
  }
  return matches;
}

}  // namespace

void RegisterProbesSettingBackupPref(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(kAlternateErrorPagesBackup, true);
}

void MigrateProbesSettingToOrFromBackup(PrefService* prefs) {
  // TODO(crbug.com/40748688): remove this code around M97 to make sure the vast
  // majority of the clients are migrated.
  if (!prefs->HasPrefPath(kAlternateErrorPagesBackup)) {
    // If the user never changed the value of the preference and still uses
    // the hardcoded default value, we'll consider it to be the user value for
    // the purposes of this migration.
    const base::Value* user_value =
        prefs->FindPreference(embedder_support::kAlternateErrorPagesEnabled)
                ->HasUserSetting()
            ? prefs->GetUserPrefValue(
                  embedder_support::kAlternateErrorPagesEnabled)
            : prefs->GetDefaultPrefValue(
                  embedder_support::kAlternateErrorPagesEnabled);

    DCHECK(user_value->is_bool());
    prefs->SetBoolean(kAlternateErrorPagesBackup, user_value->GetBool());
    prefs->ClearPref(embedder_support::kAlternateErrorPagesEnabled);
  }
}

net::DohProviderEntry::List ProvidersForCountry(
    const net::DohProviderEntry::List& providers,
    int country_id) {
  net::DohProviderEntry::List local_providers;
  base::ranges::copy_if(providers, std::back_inserter(local_providers),
                        [country_id](const net::DohProviderEntry* entry) {
                          return EntryIsForCountry(entry, country_id);
                        });
  return local_providers;
}

net::DohProviderEntry::List SelectEnabledProviders(
    const net::DohProviderEntry::List& providers) {
  net::DohProviderEntry::List enabled_providers;
  base::ranges::copy_if(
      providers, std::back_inserter(enabled_providers),
      [](const net::DohProviderEntry* entry) {
        return base::FeatureList::IsEnabled(entry->feature.get());
      });
  return enabled_providers;
}

void UpdateValidationHistogram(bool valid) {
  UMA_HISTOGRAM_BOOLEAN("Net.DNS.UI.ValidationAttemptSuccess", valid);
}

void UpdateProbeHistogram(bool success) {
  UMA_HISTOGRAM_BOOLEAN("Net.DNS.UI.ProbeAttemptSuccess", success);
}

std::unique_ptr<DnsProbeRunner> MakeProbeRunner(
    net::DnsOverHttpsConfig doh_config,
    const network::NetworkContextGetter& network_context_getter) {
  net::DnsConfigOverrides overrides;
  overrides.search = std::vector<std::string>();
  overrides.attempts = 1;
  overrides.secure_dns_mode = net::SecureDnsMode::kSecure;
  overrides.dns_over_https_config = std::move(doh_config);

  return std::make_unique<DnsProbeRunner>(std::move(overrides),
                                          network_context_getter);
}

}  // namespace chrome_browser_net::secure_dns
