// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/secure_dns_util.h"

#include <algorithm>
#include <string>

#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_split.h"
#include "build/build_config.h"
#include "chrome/common/chrome_features.h"
#include "components/country_codes/country_codes.h"
#include "components/embedder_support/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "net/dns/public/dns_config_overrides.h"
#include "net/dns/public/doh_provider_entry.h"
#include "net/dns/public/util.h"
#include "net/third_party/uri_template/uri_template.h"
#include "url/gurl.h"

namespace chrome_browser_net {

namespace secure_dns {

namespace {

const char kAlternateErrorPagesBackup[] = "alternate_error_pages.backup";

void IncrementDropdownHistogram(net::DohProviderIdForHistogram id,
                                const std::string& doh_template,
                                base::StringPiece old_template,
                                base::StringPiece new_template) {
  if (doh_template == old_template) {
    UMA_HISTOGRAM_ENUMERATION("Net.DNS.UI.DropdownSelectionEvent.Unselected",
                              id);
  } else if (doh_template == new_template) {
    UMA_HISTOGRAM_ENUMERATION("Net.DNS.UI.DropdownSelectionEvent.Selected", id);
  } else {
    UMA_HISTOGRAM_ENUMERATION("Net.DNS.UI.DropdownSelectionEvent.Ignored", id);
  }
}

bool EntryIsForCountry(const net::DohProviderEntry* entry, int country_id) {
  if (entry->display_globally) {
    return true;
  }
  const auto& countries = entry->display_countries;
  bool matches = std::any_of(countries.begin(), countries.end(),
                             [country_id](const std::string& country_code) {
                               return country_codes::CountryStringToCountryID(
                                          country_code) == country_id;
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
// TODO(crbug.com/1177778): Allow this to run on Android when there is a flag
// controlling a settings ui redesign for Android as was done on Desktop. A
// model for this new flag has been left commented in the code, in the interim
// this code will continue to migrate any Desktop users who have not yet been
// migrated.
#if !defined(OS_ANDROID)
  // If the "android settings redesign" is enabled and the user value of the
  // preference hasn't been backed up yet, back it up, and clear it. That way,
  // the preference will revert to using the hardcoded default value (unless
  // it's managed by a policy or an extension). This is necessary, as the
  // "android settings redesign" removed the user-facing toggle, and so the
  // user value of the preference is no longer modifiable.
  if (!prefs->HasPrefPath(kAlternateErrorPagesBackup)) {
#if defined(OS_ANDROID)
// if(base::FeatureList::IsEnabled("kAndroidSettingsRedesign")) {
#endif  // defined(OS_ANDROID)

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
#if defined(OS_ANDROID)
// }
#endif  // defined(OS_ANDROID)
  }
// The reverse migration should only occur on Android, so this guard should
// remain after the "android settings redesign" begins.
#if defined(OS_ANDROID)
  // If the "android settings redesign" is rolled back and there is a backed up
  // value of the preference, restore it to the original preference, and clear
  // the backup.
  if (prefs->HasPrefPath(kAlternateErrorPagesBackup)
      /* && !base::FeatureList::IsEnabled("kAndroidSettingsRedesign") */) {
    prefs->SetBoolean(embedder_support::kAlternateErrorPagesEnabled,
                      prefs->GetBoolean(kAlternateErrorPagesBackup));
    prefs->ClearPref(kAlternateErrorPagesBackup);
  }
#endif  // defined(OS_ANDROID)
#endif  // !defined(OS_ANDROID)
}

net::DohProviderEntry::List ProvidersForCountry(
    const net::DohProviderEntry::List& providers,
    int country_id) {
  net::DohProviderEntry::List local_providers;
  std::copy_if(providers.begin(), providers.end(),
               std::back_inserter(local_providers),
               [country_id](const auto* entry) {
                 return EntryIsForCountry(entry, country_id);
               });
  return local_providers;
}

std::vector<std::string> GetDisabledProviders() {
  return SplitString(features::kDnsOverHttpsDisabledProvidersParam.Get(), ",",
                     base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
}

net::DohProviderEntry::List RemoveDisabledProviders(
    const net::DohProviderEntry::List& providers,
    const std::vector<string>& disabled_providers) {
  net::DohProviderEntry::List filtered_providers;
  std::copy_if(providers.begin(), providers.end(),
               std::back_inserter(filtered_providers),
               [&disabled_providers](const auto* entry) {
                 return !base::Contains(disabled_providers, entry->provider);
               });
  return filtered_providers;
}

std::vector<base::StringPiece> SplitGroup(base::StringPiece group) {
  // Templates in a group are whitespace-separated.
  return SplitStringPiece(group, " ", base::TRIM_WHITESPACE,
                          base::SPLIT_WANT_NONEMPTY);
}

bool IsValidGroup(base::StringPiece group) {
  // All templates must be valid for the group to be considered valid.
  std::vector<base::StringPiece> templates = SplitGroup(group);
  return std::all_of(templates.begin(), templates.end(), [](auto t) {
    std::string method;
    return net::dns_util::IsValidDohTemplate(t, &method);
  });
}

void UpdateDropdownHistograms(
    const std::vector<const net::DohProviderEntry*>& providers,
    base::StringPiece old_template,
    base::StringPiece new_template) {
  for (const auto* entry : providers) {
    IncrementDropdownHistogram(entry->provider_id_for_histogram.value(),
                               entry->dns_over_https_template, old_template,
                               new_template);
  }
  // An empty template indicates a custom provider.
  IncrementDropdownHistogram(net::DohProviderIdForHistogram::kCustom,
                             std::string(), old_template, new_template);
}

void UpdateValidationHistogram(bool valid) {
  UMA_HISTOGRAM_BOOLEAN("Net.DNS.UI.ValidationAttemptSuccess", valid);
}

void UpdateProbeHistogram(bool success) {
  UMA_HISTOGRAM_BOOLEAN("Net.DNS.UI.ProbeAttemptSuccess", success);
}

void ApplyTemplate(net::DnsConfigOverrides* overrides,
                   base::StringPiece server_template) {
  std::string server_method;
  // We only allow use of templates that have already passed a format
  // validation check.
  CHECK(net::dns_util::IsValidDohTemplate(server_template, &server_method));
  overrides->dns_over_https_servers.emplace({net::DnsOverHttpsServerConfig(
      std::string(server_template), server_method == "POST")});
}

}  // namespace secure_dns

}  // namespace chrome_browser_net
