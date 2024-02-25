// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/net/network_settings_observer.h"

#include "base/functional/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/lacros/lacros_service.h"
#include "components/policy/content/policy_blocklist_service.h"

NetworkSettingsObserver::NetworkSettingsObserver(Profile* profile)
    : profile_(profile) {}

NetworkSettingsObserver::~NetworkSettingsObserver() = default;

void NetworkSettingsObserver::Start() {
  auto* lacros_service = chromeos::LacrosService::Get();
  if (!lacros_service->IsAvailable<crosapi::mojom::NetworkSettingsService>()) {
    return;
  }

  // Check if Ash is too old to support
  // NetworkSettingsObserver.
  int version =
      lacros_service
          ->GetInterfaceVersion<crosapi::mojom::NetworkSettingsService>();
  int min_required_version = static_cast<int>(
      crosapi::mojom::NetworkSettingsService::MethodMinVersions::
          kAddNetworkSettingsObserverMinVersion);
  if (version < min_required_version) {
    return;
  }

  lacros_service->GetRemote<crosapi::mojom::NetworkSettingsService>()
      ->AddNetworkSettingsObserver(receiver_.BindNewPipeAndPassRemote());

  lacros_service->GetRemote<crosapi::mojom::NetworkSettingsService>()
      ->IsAlwaysOnVpnPreConnectUrlAllowlistEnforced(base::BindOnce(
          &NetworkSettingsObserver::
              IsAlwaysOnVpnPreConnectUrlAllowlistEnforcedCallback,
          weak_ptr_factory_.GetWeakPtr()));
}

void NetworkSettingsObserver::
    OnAlwaysOnVpnPreConnectUrlAllowlistEnforcedChanged(bool enforced) {
  IsAlwaysOnVpnPreConnectUrlAllowlistEnforcedCallback(enforced);
}

void NetworkSettingsObserver::OnProxyChanged(
    crosapi::mojom::ProxyConfigPtr proxy_config) {
  // This update is handled by the `ProxyConfigServiceLacros` instance which
  // serves as a "base service" (i.e. platform specific implementation for
  // proxy configuration) in Lacros.
}
void NetworkSettingsObserver::
    IsAlwaysOnVpnPreConnectUrlAllowlistEnforcedCallback(bool enforced) {
  PolicyBlocklistService* service =
      PolicyBlocklistFactory::GetForBrowserContext(profile_);
  if (!service) {
    return;
  }
  service->SetAlwaysOnVpnPreConnectUrlAllowlistEnforced(enforced);
}
