// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NET_SECURE_DNS_MANAGER_H_
#define CHROME_BROWSER_ASH_NET_SECURE_DNS_MANAGER_H_

#include <string>

#include "base/containers/flat_map.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "chrome/browser/ash/net/dns_over_https/templates_uri_resolver.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_state_handler_observer.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "net/dns/public/dns_over_https_server_config.h"

namespace ash {

// Responds to changes in the SecureDNS preferences from the local state and
// generates and updates the corresponding shill property which can then be used
// by downstream services.
// The enterprise policies which control secure DNS settings in the browser are
// cross-platform policies that map to local state. This is required because the
// DNS config is global in the Network Service. On ChromeOS, local state is
// shared between all user sessions (including guest). For this reason, the
// user-set preferences map to the pref service that belongs to the primary
// profile.
class SecureDnsManager : public NetworkStateHandlerObserver {
 public:
  // Observes changes in the DNS-over-HTTPS configuration.
  class Observer : public base::CheckedObserver {
   public:
    // Called when the effective DNS-over-HTTPS template URIs change.
    virtual void OnTemplateUrisChanged(const std::string& template_uris) = 0;

    // Called when the DNS-over-HTTPS mode changes.
    virtual void OnModeChanged(const std::string& mode) = 0;

    // Called before the SecureDnsManager is destroyed.
    virtual void OnSecureDnsManagerShutdown() = 0;

    ~Observer() override = default;
  };

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  SecureDnsManager(PrefService* local_state,
                   PrefService* profile_prefs,
                   bool is_profile_managed);
  SecureDnsManager(const SecureDnsManager&) = delete;
  SecureDnsManager& operator=(const SecureDnsManager&) = delete;
  ~SecureDnsManager() override;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);
  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  void SetDoHTemplatesUriResolverForTesting(
      std::unique_ptr<dns_over_https::TemplatesUriResolver>
          doh_templates_uri_resolver);

  // If the URI templates for the DNS-over-HTTPS resolver contain user or device
  // identifiers (which are hashed before being used), this method returns the
  // plain text version of the URI templates. Otherwise returns nullopt.
  std::optional<std::string> GetDohWithIdentifiersDisplayServers() const;

  void SetPrimaryProfilePropertiesForTesting(PrefService* profile_prefs,
                                             bool is_profile_managed);

 private:
  void DefaultNetworkChanged(const NetworkState* network) override;

  // Retrieves the list of secure DNS providers, preprocesses and caches it for
  // later use. This is safe since the list is embedded in code and will not
  // change at runtime.
  void LoadProviders();

  // Computes a collection of secure DNS providers to use based on the |mode|
  // and |templates| prefs applied to |local_doh_providers_|.
  base::Value::Dict GetProviders(const std::string& mode,
                                 const std::string& templates) const;

  // Starts tracking user-configured secure DNS settings. This settings are
  // mapped to the pref service that belongs to the profile associated with the
  // primary user.
  void MonitorUserPrefs();
  void OnPrefChanged();

  // Starts tracking secure DNS enterprise policy changes. The policy values are
  // mapped by the policy service to the local state pref service.
  void MonitorLocalStatePrefs();
  void OnLocalStatePrefsChanged();

  void OnDoHIncludedDomainsPrefChanged();
  void OnDoHExcludedDomainsPrefChanged();

  // If the DoH template URIs contain network identifiers, this method will
  // instantiate `network_state_handler_observer_` to start monitoring
  // network changes. Otherwise, it will reset
  // `network_state_handler_observer_`.
  void ToggleNetworkMonitoring();
  void UpdateTemplateUri();

  // If either the template URIs or the mode have been modified,
  // inform all registered observers in the 'observers_' list and
  // also notify Lacros and the shill service about the new values.
  void BroadcastUpdates(bool template_uris_changed, bool mode_changed) const;

  base::ScopedObservation<NetworkStateHandler, NetworkStateHandlerObserver>
      network_state_handler_observer_{this};

  PrefChangeRegistrar local_state_registrar_, profile_prefs_registrar_;
  raw_ptr<PrefService> local_state_, profile_prefs_;

  // Maps secure DNS provider URL templates to their corresponding standard DNS
  // name servers. Providers that are either disabled or not applicable for the
  // country have been pre-filtered.
  base::flat_map<net::DnsOverHttpsServerConfig, std::string>
      local_doh_providers_;

  std::unique_ptr<dns_over_https::TemplatesUriResolver>
      doh_templates_uri_resolver_;

  std::string cached_template_uris_;
  std::string cached_mode_;

  bool cached_is_config_managed_ = false;
  bool is_profile_managed_ = false;

  base::ObserverList<Observer> observers_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_NET_SECURE_DNS_MANAGER_H_
