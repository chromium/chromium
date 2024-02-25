// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_NET_PROXY_CONFIG_SERVICE_LACROS_H_
#define CHROME_BROWSER_LACROS_NET_PROXY_CONFIG_SERVICE_LACROS_H_

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "chromeos/crosapi/mojom/network_settings_service.mojom.h"
#include "components/prefs/pref_change_registrar.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/proxy_resolution/proxy_config_service.h"
#include "net/proxy_resolution/proxy_config_with_annotation.h"

class PrefRegistrySimple;
class Profile;

namespace lacros {
namespace net {
class LacrosExtensionProxyTracker;
}
}  // namespace lacros

namespace chromeos {

// Implementation of ProxyConfigService that retrieves the system proxy
// settings from Ash-Chrome via the mojo API. Unlike other
// net::ProxyConfigService implementations used as base service, this class will
// only forward the system proxy retrieved from Ash-Chrome if allowed by pref
// kUseAshProxy.
class ProxyConfigServiceLacros
    : public net::ProxyConfigService,
      public crosapi::mojom::NetworkSettingsObserver {
 public:
  explicit ProxyConfigServiceLacros(Profile* profile);

  ProxyConfigServiceLacros(const ProxyConfigServiceLacros&) = delete;
  ProxyConfigServiceLacros& operator=(const ProxyConfigServiceLacros&) = delete;
  ~ProxyConfigServiceLacros() override;

  // net::ProxyConfigService impl
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  ProxyConfigService::ConfigAvailability GetLatestProxyConfig(
      net::ProxyConfigWithAnnotation* config) override;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

 private:
  // crosapi::mojom::NetworkSettingsObserver impl
  void OnProxyChanged(crosapi::mojom::ProxyConfigPtr proxy_config) override;
  void OnAlwaysOnVpnPreConnectUrlAllowlistEnforcedChanged(
      bool enforced) override;

  void OnUseAshProxyPrefChanged();

  void NotifyObservers();
  // The profile associated with this ProxyConfigServiceLacros instance.
  raw_ptr<Profile> profile_;

  PrefChangeRegistrar profile_pref_change_registrar_;

  // TODO(acostinas, b/200001678): Use checked observers.
  base::ObserverList<net::ProxyConfigService::Observer>::Unchecked observers_;
  // Receives mojo messages from Ash-Chrome.
  mojo::Receiver<crosapi::mojom::NetworkSettingsObserver> receiver_{this};
  // The latest proxy configuration sent by Ash-Chrome via mojo. This proxy is
  // enforced in the browser only if the pref kUseAshProxy=true and the
  // kProxy pref, which has precedence, is unset or set to mode=system.
  std::optional<net::ProxyConfigWithAnnotation> cached_config_;
  // Indicates if the proxy config comes from an extension active in the main
  // Lacros profile.
  bool proxy_controlled_by_extension_ = false;

  // Forwards proxy configs set via extensions in the Lacros primary profile
  // to Ash-Chrome.
  std::unique_ptr<lacros::net::LacrosExtensionProxyTracker>
      lacros_extension_proxy_tracker_;

  base::WeakPtrFactory<ProxyConfigServiceLacros> weak_ptr_factory_{this};
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_LACROS_NET_PROXY_CONFIG_SERVICE_LACROS_H_
