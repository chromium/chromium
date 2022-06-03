// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_SECURE_DNS_UTIL_H_
#define CHROME_BROWSER_NET_SECURE_DNS_UTIL_H_

#include <vector>

#include "base/strings/string_piece.h"
#include "net/dns/public/doh_provider_entry.h"

namespace net {
struct DnsConfigOverrides;
}  // namespace net

class PrefRegistrySimple;
class PrefService;

namespace chrome_browser_net {

namespace secure_dns {

// Returns the subset of |providers| that are marked for use in the specified
// country.
net::DohProviderEntry::List ProvidersForCountry(
    const net::DohProviderEntry::List& providers,
    int country_id);

// Returns the names of providers that have been remotely disabled, for use with
// RemoveDisabledProviders().
std::vector<std::string> GetDisabledProviders();

// Returns the subset of |providers| for which |DohProviderEntry::provider| is
// not listed in |disabled_providers|.
net::DohProviderEntry::List RemoveDisabledProviders(
    const net::DohProviderEntry::List& providers,
    const std::vector<std::string>& disabled_providers);

// Implements the whitespace-delimited group syntax for DoH templates.
std::vector<base::StringPiece> SplitGroup(base::StringPiece group);

// Returns true if a group of templates are all valid per
// net::dns_util::IsValidDohTemplate().  This should be checked before updating
// stored preferences.
bool IsValidGroup(base::StringPiece group);

// When the selected template changes, call this function to update the
// Selected, Unselected, and Ignored histograms for all the included providers,
// and also for the custom provider option.  If the old or new selection is the
// custom provider option, pass an empty string as the template.
void UpdateDropdownHistograms(const net::DohProviderEntry::List& providers,
                              base::StringPiece old_template,
                              base::StringPiece new_template);
void UpdateValidationHistogram(bool valid);
void UpdateProbeHistogram(bool success);

// Modifies |overrides| to use the DoH server specified by |server_template|.
void ApplyTemplate(net::DnsConfigOverrides* overrides,
                   base::StringPiece server_template);

// Registers the backup preference required for the DNS probes setting reset.
// TODO(crbug.com/1062698): Remove this once the privacy settings redesign
// is fully launched.
void RegisterProbesSettingBackupPref(PrefRegistrySimple* registry);

// Backs up the unneeded preference controlling DNS and captive portal probes
// once the privacy settings redesign is enabled, or restores the backup
// in case the feature is rolled back.
// TODO(crbug.com/1062698): Remove this once the privacy settings redesign
// is fully launched.
void MigrateProbesSettingToOrFromBackup(PrefService* prefs);

}  // namespace secure_dns

}  // namespace chrome_browser_net

#endif  // CHROME_BROWSER_NET_SECURE_DNS_UTIL_H_
