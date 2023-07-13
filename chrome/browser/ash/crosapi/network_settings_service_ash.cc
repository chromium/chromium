// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/network_settings_service_ash.h"

#include <stdint.h>

#include "ash/constants/ash_pref_names.h"
#include "base/check_is_test.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/crosapi/network_settings_translation.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "components/user_manager/user_manager.h"

namespace {

// Returns the extension metadata (as a crosapi object) for the extension which
// is controlling the proxy in the Lacros primary profile. If the proxy is not
// controlled by an extension, returns the null pointer.
crosapi::mojom::ExtensionControllingProxyPtr GetExtensionPtr(
    const ash::AshProxyMonitor* ash_proxy_monitor) {
  if (!ash_proxy_monitor ||
      !ash_proxy_monitor->GetLacrosExtensionControllingTheProxy()) {
    return nullptr;
  }
  absl::optional<ash::AshProxyMonitor::ExtensionMetadata> extension_metadata =
      ash_proxy_monitor->GetLacrosExtensionControllingTheProxy();
  crosapi::mojom::ExtensionControllingProxyPtr extension =
      crosapi::mojom::ExtensionControllingProxy::New();
  extension->name = extension_metadata->name;
  extension->id = extension_metadata->id;
  extension->can_be_disabled = extension_metadata->can_be_disabled;
  return extension;
}

}  // namespace
namespace crosapi {

NetworkSettingsServiceAsh::NetworkSettingsServiceAsh(
    ash::AshProxyMonitor* ash_proxy_monitor)
    : ash_proxy_monitor_(ash_proxy_monitor) {
  // Missing in unit tests.
  if (ash_proxy_monitor_) {
    ash_proxy_monitor_observation_.Observe(ash_proxy_monitor_);
  } else {
    CHECK_IS_TEST();
  }
  observers_.set_disconnect_handler(base::BindRepeating(
      &NetworkSettingsServiceAsh::OnDisconnect, base::Unretained(this)));
}

NetworkSettingsServiceAsh::~NetworkSettingsServiceAsh() = default;

void NetworkSettingsServiceAsh::BindReceiver(
    mojo::PendingReceiver<mojom::NetworkSettingsService> pending_receiver) {
  receivers_.Add(this, std::move(pending_receiver));
}

void NetworkSettingsServiceAsh::SetExtensionProxy(
    crosapi::mojom::ProxyConfigPtr proxy_config) {
  if (!proxy_config->extension) {
    LOG(ERROR)
        << "Received extension proxy configuration without extension data";
    return;
  }
  // Required to display the extension which is controlling the proxy in the OS
  // Settings > Network > Proxy window.
  ash_proxy_monitor_->SetLacrosExtensionControllingProxyInfo(
      proxy_config->extension->name, proxy_config->extension->id,
      proxy_config->extension->can_be_disabled);
}

void NetworkSettingsServiceAsh::ClearExtensionProxy() {
  ash_proxy_monitor_->ClearLacrosExtensionControllingProxyInfo();
}

void NetworkSettingsServiceAsh::AddNetworkSettingsObserver(
    mojo::PendingRemote<mojom::NetworkSettingsObserver> observer) {
  mojo::Remote<mojom::NetworkSettingsObserver> remote(std::move(observer));
  if (cached_proxy_config_) {
    remote->OnProxyChanged(cached_proxy_config_.Clone());
  }
  observers_.Add(std::move(remote));
}

void NetworkSettingsServiceAsh::OnProxyChanged() {
  crosapi::mojom::ProxyConfigPtr new_proxy_config =
      ProxyConfigToCrosapiProxy(ash_proxy_monitor_->GetLatestProxyConfig(),
                                ash_proxy_monitor_->GetLatestWpadUrl());
  new_proxy_config->extension = GetExtensionPtr(ash_proxy_monitor_);

  // Proxy config has not changed.
  if (cached_proxy_config_ && new_proxy_config->Equals(*cached_proxy_config_)) {
    return;
  }

  cached_proxy_config_ = std::move(new_proxy_config);
  for (const auto& obs : observers_) {
    obs->OnProxyChanged(cached_proxy_config_.Clone());
  }
}

void NetworkSettingsServiceAsh::OnDisconnect(mojo::RemoteSetElementId mojo_id) {
  observers_.Remove(mojo_id);
}

}  // namespace crosapi
