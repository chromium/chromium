// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NET_SECURE_DNS_MANAGER_H_
#define CHROME_BROWSER_ASH_NET_SECURE_DNS_MANAGER_H_

#include <string>

#include "base/containers/flat_map.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/net/dns_over_https/templates_uri_resolver.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_state_handler_observer.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "net/dns/public/dns_over_https_server_config.h"

namespace ash {

// Responds to changes in the SecureDNS preferences from the local state and
// generates and updates the corresponding shill property which can then be used
// by downstream services.
class SecureDnsManager : public NetworkStateHandlerObserver {
 public:
  explicit SecureDnsManager(PrefService* local_state);
  SecureDnsManager(const SecureDnsManager&) = delete;
  SecureDnsManager& operator=(const SecureDnsManager&) = delete;
  ~SecureDnsManager() override;

  void SetDoHTemplatesUriResolverForTesting(
      std::unique_ptr<dns_over_https::TemplatesUriResolver>
          doh_templates_uri_resolver);

 private:
  void DefaultNetworkChanged(const NetworkState* network) override;

  // Retrieves the list of secure DNS providers, preprocesses and caches it for
  // later use. This is safe since the list is embedded in code and will not
  // change at runtime.
  void LoadProviders();

  // Computes a collection of secure DNS providers to use based on the |mode|
  // and |templates| prefs applied to |local_doh_providers_|.
  base::Value::Dict GetProviders(const std::string& mode,
                                 const std::string& templates);

  // Starts tracking secure DNS enterprise policy changes. The policy values are
  // mapped by the policy service to the local state pref service.
  void MonitorPolicyPrefs();

  // Callback for the registrar. Evaluates the current settings and publishes
  // the result to shill.
  void OnPolicyPrefChanged();

  void OnDoHIncludedDomainsPrefChanged();
  void OnDoHExcludedDomainsPrefChanged();

  // If the DoH template URIs contain network identifiers, this method will
  // instantiate `network_state_handler_observer_` to start monitoring
  // network changes. Otherwise, it will reset
  // `network_state_handler_observer_`.
  void ToggleNetworkMonitoring();
  void UpdateTemplateUri();

  base::ScopedObservation<NetworkStateHandler, NetworkStateHandlerObserver>
      network_state_handler_observer_{this};

  PrefChangeRegistrar local_state_registrar_;
  raw_ptr<PrefService> local_state_;

  // Maps secure DNS provider URL templates to their corresponding standard DNS
  // name servers. Providers that are either disabled or not applicable for the
  // country have been pre-filtered.
  base::flat_map<net::DnsOverHttpsServerConfig, std::string>
      local_doh_providers_;

  std::unique_ptr<dns_over_https::TemplatesUriResolver>
      doh_templates_uri_resolver_;

  base::Value::Dict cached_doh_providers_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_NET_SECURE_DNS_MANAGER_H_
