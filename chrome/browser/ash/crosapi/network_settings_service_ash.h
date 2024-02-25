// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_NETWORK_SETTINGS_SERVICE_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_NETWORK_SETTINGS_SERVICE_ASH_H_

#include "base/scoped_observation.h"
#include "chrome/browser/ash/net/ash_proxy_monitor.h"
#include "chromeos/crosapi/mojom/network_settings_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "url/gurl.h"

namespace crosapi {

// This class is the Ash-Chrome implementation of the NetworkSettingsService
// interface. This class must only be used from the main thread.
class NetworkSettingsServiceAsh : public crosapi::mojom::NetworkSettingsService,
                                  public ash::AshProxyMonitor::Observer {
 public:
  explicit NetworkSettingsServiceAsh(ash::AshProxyMonitor* ash_proxy_monitor);
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
  void IsAlwaysOnVpnPreConnectUrlAllowlistEnforced(
      IsAlwaysOnVpnPreConnectUrlAllowlistEnforcedCallback callback) override;

  // Deprecated. Please use `SetExtensionControllingProxyMetadata` and
  // `ClearExtensionControllingProxyMetadata`.
  void SetExtensionProxy(crosapi::mojom::ProxyConfigPtr proxy_config) override;
  void ClearExtensionProxy() override;
  // Stores metadata about the extension controlling the proxy in the primary
  // profile. The actual proxy config is being set and cleared from the
  // PrefService via the mojo::Prefs service.
  // TODO(acostinas,b/268607394) Deprecate these methods when the mojo Prefs
  // service implements sending the extension metadata along with the pref
  // value.
  void SetExtensionControllingProxyMetadata(
      crosapi::mojom::ExtensionControllingProxyPtr extension) override;
  void ClearExtensionControllingProxyMetadata() override;

  // Sets a value which indicates if the AlwaysOnVpnPreConnectUrlAllowlist
  // should be used to restrict user navigation.
  void SetAlwaysOnVpnPreConnectUrlAllowlistEnforced(bool enforced);

 private:
  // ash::AshProxyMonitor::Observer:
  void OnProxyChanged() override;

  // Called when a mojo observer is disconnecting. If there's no observer for
  // this service, the service will stop listening for pref changes.
  void OnDisconnect(mojo::RemoteSetElementId mojo_id);

  crosapi::mojom::ProxyConfigPtr cached_proxy_config_;

  bool alwayson_vpn_pre_connect_url_allowlist_enforced_ = false;

  base::ScopedObservation<ash::AshProxyMonitor, ash::AshProxyMonitor::Observer>
      ash_proxy_monitor_observation_{this};
  raw_ptr<ash::AshProxyMonitor> ash_proxy_monitor_;

  // Support any number of connections.
  mojo::ReceiverSet<mojom::NetworkSettingsService> receivers_;
  // Support any number of observers.
  mojo::RemoteSet<mojom::NetworkSettingsObserver> observers_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_NETWORK_SETTINGS_SERVICE_ASH_H_
