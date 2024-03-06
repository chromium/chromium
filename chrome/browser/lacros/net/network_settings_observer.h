// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_NET_NETWORK_SETTINGS_OBSERVER_H_
#define CHROME_BROWSER_LACROS_NET_NETWORK_SETTINGS_OBSERVER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/crosapi/mojom/network_settings_service.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"

class Profile;

// This class monitors for updates by the mojo NetworkSettingsService in Ash.
class NetworkSettingsObserver : public crosapi::mojom::NetworkSettingsObserver {
 public:
  explicit NetworkSettingsObserver(Profile* profile);

  NetworkSettingsObserver(const NetworkSettingsObserver&) = delete;
  NetworkSettingsObserver& operator=(const NetworkSettingsObserver&) = delete;
  ~NetworkSettingsObserver() override;

  // Start observing Ash-Chrome updates.
  void Start();

 private:
  // If `enforced` is false, this method configures the `PolicyBlocklistService`
  // to use the default behaviour (which filters traffic according to the
  // `URLBlocklist`  and `URLAllowlist` prefs). When `enforced` is true, this
  // method overrides the default behaviour of the `PolicyBlocklistService`
  // service by enforcing the URL filters configured in the
  // `AlwaysOnVpnPreConnectUrlAllowlist` policy.
  void IsAlwaysOnVpnPreConnectUrlAllowlistEnforcedCallback(bool enforced);

  // crosapi::mojom::NetworkSettingsObserver:
  void OnAlwaysOnVpnPreConnectUrlAllowlistEnforcedChanged(
      bool enforced) override;
  void OnProxyChanged(crosapi::mojom::ProxyConfigPtr proxy_config) override;

  mojo::Receiver<crosapi::mojom::NetworkSettingsObserver> receiver_{this};
  const raw_ptr<Profile, DanglingUntriaged> profile_;
  base::WeakPtrFactory<NetworkSettingsObserver> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_LACROS_NET_NETWORK_SETTINGS_OBSERVER_H_
