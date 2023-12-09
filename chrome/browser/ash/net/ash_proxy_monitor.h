// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NET_ASH_PROXY_MONITOR_H_
#define CHROME_BROWSER_ASH_NET_ASH_PROXY_MONITOR_H_

#include <optional>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "chromeos/ash/components/network/network_state_handler_observer.h"
#include "components/proxy_config/proxy_config_dictionary.h"
#include "url/gurl.h"

class PrefService;
class PrefChangeRegistrar;
class PrefRegistrySimple;
class Profile;
class ProfileManager;

namespace ash {
class NetworkState;
class NetworkStateHandler;
}  // namespace ash

namespace ash {

// Monitors proxy changes coming from policies, extensions and the default
// network, and propagates the proxy configuration to observers. Can only be
// used inside the user session.
// TODO(b/290595436, acostinas) Allow using the AshProxyMonitor at the login
// screen.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) AshProxyMonitor
    : public NetworkStateHandlerObserver,
      public ProfileManagerObserver {
 public:
  struct ExtensionMetadata {
    ExtensionMetadata(const std::string& name,
                      const std::string& id,
                      bool can_be_disabled);
    std::string name;
    std::string id;
    bool can_be_disabled;
  };
  class COMPONENT_EXPORT(CHROMEOS_NETWORK) Observer
      : public base::CheckedObserver {
   public:
    // Called when the effective proxy config changes in Ash or when the
    // metadata of the Lacros extension controlling the proxy changes.
    virtual void OnProxyChanged() = 0;
  };

  AshProxyMonitor(PrefService* local_state, ProfileManager* profile_manager);
  AshProxyMonitor(const AshProxyMonitor&) = delete;
  AshProxyMonitor& operator=(const AshProxyMonitor&) = delete;
  ~AshProxyMonitor() override;

  // Adds an observer. An observer is removed by calling `RemoveObserver`.
  // Multiple observers can be added.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Indicates if the proxy is controlled by an extension running in the primary
  // profile in Lacros.
  bool IsLacrosExtensionControllingProxy() const;
  // Stores as a user preference the metadata about the extension which is
  // controlling the pref in the Lacros primary profile. The metadata can be
  // retrieved by calling `GetLacrosExtensionControllingTheProxy` method.
  void SetLacrosExtensionControllingProxyInfo(const std::string& name,
                                              const std::string& id,
                                              bool can_be_disabled);
  // If the `kProxy` pref is controlled by an extension running in the Lacros
  // browser associated with the primary profile, these method returns metadata
  // about the extension, otherwise it returns a null object.
  std::optional<ExtensionMetadata> GetLacrosExtensionControllingTheProxy()
      const;

  void ClearLacrosExtensionControllingProxyInfo();

  void SetProfileForTesting(Profile* profile);

  ProxyConfigDictionary* GetLatestProxyConfig() const;

  GURL GetLatestWpadUrl() const;

 private:
  void NotifyObservers();

  // NetworkStateHandlerObserver:
  void DefaultNetworkChanged(const NetworkState* network) override;
  void OnShuttingDown() override;

  // ProfileManagerObserver:
  void OnProfileAdded(Profile* profile) override;
  void OnProfileMarkedForPermanentDeletion(Profile* profile) override;
  void OnProfileManagerDestroying() override;

  // Only sends an update to observers if the effective proxy config has
  // changed.
  void OnProxyChanged(std::optional<GURL> wpad_url);

  // The PAC URL associated with `default_network_name_`, received via the DHCP
  // discovery method.
  std::optional<GURL> cached_wpad_url_ = std::nullopt;
  std::unique_ptr<ProxyConfigDictionary> cached_proxy_config_ = nullptr;

  std::unique_ptr<PrefChangeRegistrar> profile_prefs_registrar_;

  raw_ptr<PrefService> local_state_ = nullptr;
  raw_ptr<ProfileManager> profile_manager_ = nullptr;
  raw_ptr<Profile> primary_profile_ = nullptr;

  base::ObserverList<Observer, /*check_empty=*/true> observers_;

  base::ScopedObservation<NetworkStateHandler, NetworkStateHandlerObserver>
      network_state_handler_observer_{this};

  base::ScopedObservation<ProfileManager, ProfileManagerObserver>
      profile_manager_observation_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_NET_ASH_PROXY_MONITOR_H_
