// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NET_VPN_LIST_FORWARDER_H_
#define CHROME_BROWSER_ASH_NET_VPN_LIST_FORWARDER_H_

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/app_list/arc/arc_vpn_provider_manager.h"
#include "chrome/browser/ash/crosapi/vpn_extension_observer_ash.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "components/user_manager/user_manager.h"
#include "extensions/browser/extension_registry_observer.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace extensions {
class ExtensionRegistry;
}

// Forwards a list of third party (Extension and Arc) VPN providers in the
// primary user's profile to the chromeos.network_config.mojom service for
// use in the Settings and Ash UI code.
class VpnListForwarder
    : public app_list::ArcVpnProviderManager::Observer,
      public extensions::ExtensionRegistryObserver,
      public user_manager::UserManager::UserSessionStateObserver,
      public crosapi::VpnExtensionObserverAsh::Delegate {
 public:
  VpnListForwarder();

  VpnListForwarder(const VpnListForwarder&) = delete;
  VpnListForwarder& operator=(const VpnListForwarder&) = delete;

  ~VpnListForwarder() override;

  // app_list::ArcVpnProviderManager::Observer:
  void OnArcVpnProvidersRefreshed(
      const std::vector<
          std::unique_ptr<app_list::ArcVpnProviderManager::ArcVpnProvider>>&
          arc_vpn_providers) override;
  void OnArcVpnProviderRemoved(const std::string& package_name) override;
  void OnArcVpnProviderUpdated(app_list::ArcVpnProviderManager::ArcVpnProvider*
                                   arc_vpn_provider) override;

  // extensions::ExtensionRegistryObserver:
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const extensions::Extension* extension) override;
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const extensions::Extension* extension,
                           extensions::UnloadedExtensionReason reason) override;
  void OnShutdown(extensions::ExtensionRegistry* registry) override;

  // crosapi::VpnExtensionObserverAsh::Delegate:
  void OnLacrosVpnExtensionLoaded(const std::string& extension_id,
                                  const std::string& extension_name) override;
  void OnLacrosVpnExtensionUnloaded(const std::string& extension_id) override;
  void OnLacrosVpnExtensionObserverDisconnected() override;

  // user_manager::UserManager::UserSessionStateObserver:
  void ActiveUserChanged(user_manager::User* active_user) override;

 private:
  // Calls cros_network_config_->SetVpnProviders with the current provider list.
  void SetVpnProviders();

  // Starts to observe extension registry and ArcAppListPrefs. Must only be
  // called when a user is logged in.
  void AttachToPrimaryUserProfile();

  // Starts observing the primary user's extension registry to detect changes to
  // the list of VPN providers enabled in the user's profile and caches the
  // initial list. Must only be called when a user is logged in.
  void AttachToPrimaryUserExtensionRegistry();

  // Starts observing the primary user's app_list::ArcVpnProviderManager to
  // detect changes to the list of Arc VPN providers installed in the user's
  // profile. Must only be called when a user is logged in.
  void AttachToPrimaryUserArcVpnProviderManager();

  // Starts observing the primary user's extension registry in lacros via
  // crosapi.
  void AttachToVpnExtensionObserverAsh();

  // The primary user's extension registry, if a user is logged in.
  raw_ptr<extensions::ExtensionRegistry> extension_registry_ = nullptr;

  // The primary user's app_list::ArcVpnProviderManager, if a user is logged
  // in.
  raw_ptr<app_list::ArcVpnProviderManager> arc_vpn_provider_manager_ = nullptr;

  std::unique_ptr<
      mojo::Remote<chromeos::network_config::mojom::CrosNetworkConfig>>
      cros_network_config_;

  // Map of unique provider id to VpnProvider dictionary.
  base::flat_map<std::string, chromeos::network_config::mojom::VpnProviderPtr>
      vpn_providers_;

  // Map of unique provider id to VpnProvider dictionary for lacros vpn
  // extensions.
  base::flat_map<std::string, chromeos::network_config::mojom::VpnProviderPtr>
      lacros_vpn_providers_;

  base::WeakPtrFactory<VpnListForwarder> weak_factory_{this};
};

#endif  // CHROME_BROWSER_ASH_NET_VPN_LIST_FORWARDER_H_
