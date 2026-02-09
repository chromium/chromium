// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_SECURE_DNS_UTIL_H_
#define CHROME_BROWSER_NET_SECURE_DNS_UTIL_H_

#include <memory>

#include "net/dns/public/doh_provider_entry.h"
#include "services/network/public/cpp/network_context_getter.h"

namespace chrome_browser_net {
class DnsProbeRunner;
}  // namespace chrome_browser_net

namespace country_codes {
class CountryId;
}  // namespace country_codes

namespace net {
class DnsOverHttpsConfig;
}  // namespace net

namespace chrome_browser_net::secure_dns {

// Returns the subsequence of `providers` that are marked for use in the
// specified country.
net::DohProviderEntry::List ProvidersForCountry(
    const net::DohProviderEntry::List& providers,
    country_codes::CountryId country_id);

// Returns the subsequence of `providers` that are enabled, according to their
// `net::DohProviderEntry::feature` members.
net::DohProviderEntry::List SelectEnabledProviders(
    const net::DohProviderEntry::List& providers);

void UpdateValidationHistogram(bool valid);
void UpdateProbeHistogram(bool success);

// Returns a DNS prober configured for testing DoH servers
std::unique_ptr<DnsProbeRunner> MakeProbeRunner(
    net::DnsOverHttpsConfig doh_config,
    const network::NetworkContextGetter& network_context_getter);

}  // namespace chrome_browser_net::secure_dns

#endif  // CHROME_BROWSER_NET_SECURE_DNS_UTIL_H_
