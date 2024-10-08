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
#include "net/dns/public/dns_over_https_config.h"
#include "net/dns/public/dns_over_https_server_config.h"
#include "net/dns/public/secure_dns_mode.h"

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

  // Returns the OS' secure DNS configuration.
  net::DnsOverHttpsConfig GetOsDohConfig() const;
  net::SecureDnsMode GetOsDohMode() const;

  void SetPrimaryProfilePropertiesForTesting(PrefService* profile_prefs,
                                             bool is_profile_managed);

  // Whether or not DoHIncludedDomains or DoHExcludedDomains is set.
  bool IsDohDomainConfigSet() const { return cached_domain_config_set_; }

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

  // When moving between profiles (and login screen), SecureDnsManager instance
  // is destroyed. A new instance is created on the new user session. On login
  // screen, the class is not instantiated. In order to have the correct Shill
  // state on login screen, Shill's state needs to be reset whenever the class
  // is destroyed. This is done by propagating all the default values of the
  // states.
  void ResetShillState();

  // If the DoH template URIs contain network identifiers, this method will
  // instantiate `network_state_handler_observer_` to start monitoring
  // network changes. Otherwise, it will reset
  // `network_state_handler_observer_`.
  void ToggleNetworkMonitoring();
  void UpdateTemplateUri();

  // Update the internal cached DoH config. If either the template URIs or the
  // mode have been modified, inform all registered observers in the
  // 'observers_' list and also notify Lacros and the shill service about the
  // new values.
  // `new_template_uris` is a space separated DoH template URI. The value is
  // expected to be fetched from Chrome's kDnsOverHttpsTemplates prefs.
  // When `force_update` is true, always send the updates to the observer
  // regardless of any value changes.
  // The DoH config for Chrome and shill might differ in the case where
  // DoHIncludedDomains or DoHExcludedDomains is set. On such cases, DoH in
  // Chrome is always disabled in order for ChromeOS DNS proxy to be able to
  // listen for Chrome DNS requests.
  void UpdateDoHConfig(const std::string& new_mode,
                       const std::string& new_template_uris,
                       bool force_update = false);
  void UpdateChromeDoHConfig(const std::string& new_mode,
                             const std::string& new_template_uris,
                             bool force_update = false);
  void UpdateShillDoHConfig(const std::string& new_mode,
                            const std::string& new_template_uris);

  // Updates `cached_domain_config_set_`. If DoH domain config changed, also
  // trigger a DoH config update to the observers in order for the UI to be
  // updated.
  void UpdateCachedDomainConfigSet();

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

  // Cached OS-wide DoH provider URIs and mode. The value is expected to match
  // Chrome's secure DNS prefs.
  std::string cached_template_uris_;
  std::string cached_mode_;

  // Cached DoH provider URIs and mode for Chrome.
  // The values might differ with the actual values in the case where
  // DoHIncludedDomains or DoHExcludedDomains is set.
  std::string cached_chrome_template_uris_;
  std::string cached_chrome_mode_;

  // Whether or not DoHIncludedDomains or DoHExcludedDomains is set.
  bool cached_domain_config_set_ = false;

  bool cached_is_config_managed_ = false;
  bool is_profile_managed_ = false;

  base::ObserverList<Observer> observers_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_NET_SECURE_DNS_MANAGER_H_
