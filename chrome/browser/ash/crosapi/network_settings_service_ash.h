// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_NETWORK_SETTINGS_SERVICE_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_NETWORK_SETTINGS_SERVICE_ASH_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "chromeos/ash/components/network/network_state_handler_observer.h"
#include "chromeos/crosapi/mojom/network_settings_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"
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

namespace crosapi {

// This class is the Ash-Chrome implementation of the NetworkSettingsService
// interface. This class must only be used from the main thread.
// It observes proxy changes coming from policies and the default network, and
// propagates the proxy configuration to Lacros-Chrome observers.
// The class also observes the `ProfileAdded` event sent by ProfileManager to
// verify if Lacros is still enabled in Ash via flag or user policy; if not,
// then it will clear the proxy settings set by an extension in the primary
// profile.
class NetworkSettingsServiceAsh : public crosapi::mojom::NetworkSettingsService,
                                  public ash::NetworkStateHandlerObserver,
                                  public ProfileManagerObserver {
 public:
  explicit NetworkSettingsServiceAsh(PrefService* local_state);
  NetworkSettingsServiceAsh(const NetworkSettingsServiceAsh&) = delete;
  NetworkSettingsServiceAsh& operator=(const NetworkSettingsServiceAsh&) =
      delete;
  ~NetworkSettingsServiceAsh() override;

  void BindReceiver(
      mojo::PendingReceiver<crosapi::mojom::NetworkSettingsService>
          pending_receiver);
  // crosapi::mojom::NetworkSettingsServiceAsh:
  void AddNetworkSettingsObserver(
      mojo::PendingRemote<mojom::NetworkSettingsObserver> observer) override;
  // Sets the kProxy preference in the user store.
  void SetExtensionProxy(crosapi::mojom::ProxyConfigPtr proxy_config) override;
  void ClearExtensionProxy() override;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

 private:
  // NetworkStateHandlerObserver:
  void DefaultNetworkChanged(const ash::NetworkState* network) override;

  void SendProxyConfigToObservers();

  // Starts tracking the kProxy pref on the primary profile. Only invoked when
  // there is at least one entry in `observers_`.
  void StartTrackingPrefChanges();
  void OnPrefChanged();

  // Clears the kProxy preference from the user store. If the preference is also
  // set via policy, then the policy value stored in the managed store will
  // still be active.
  void ClearProxyPrefFromUserStore();

  void DetermineEffectiveProxy();

  // Called when a mojo observer is disconnecting. If there's no observer for
  // this service, the service will stop listening for pref changes.
  void OnDisconnect(mojo::RemoteSetElementId mojo_id);

  // ProfileManagerObserver:
  void OnProfileAdded(Profile* profile) override;
  void OnProfileManagerDestroying() override;

  crosapi::mojom::ProxyConfigPtr cached_proxy_config_;
  crosapi::mojom::ExtensionControllingProxyPtr extension_controlling_proxy_;

  // The PAC URL associated with `default_network_name_`, received via the DHCP
  // discovery method.
  GURL cached_wpad_url_;

  // Has a non-null value only when there are entries in `observers_`, i.e. the
  // class is only listening to pref changes when there is at least a Lacros
  // instance running.
  std::unique_ptr<PrefChangeRegistrar> profile_prefs_registrar_;

  raw_ptr<PrefService, ExperimentalAsh> local_state_;
  raw_ptr<ProfileManager, ExperimentalAsh> profile_manager_ = nullptr;

  base::ScopedObservation<ash::NetworkStateHandler,
                          ash::NetworkStateHandlerObserver>
      network_state_handler_observer_{this};

  // Support any number of connections.
  mojo::ReceiverSet<mojom::NetworkSettingsService> receivers_;
  // Support any number of observers.
  mojo::RemoteSet<mojom::NetworkSettingsObserver> observers_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_NETWORK_SETTINGS_SERVICE_ASH_H_
