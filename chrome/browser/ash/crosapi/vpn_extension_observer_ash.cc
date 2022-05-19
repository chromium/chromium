// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/vpn_extension_observer_ash.h"
#include "base/bind.h"

namespace crosapi {

VpnExtensionObserverAsh::VpnExtensionObserverAsh() {
  receivers_.set_disconnect_handler(base::BindRepeating(
      &VpnExtensionObserverAsh::OnLacrosVpnExtensionObserverDisconnected,
      base::Unretained(this)));
}

VpnExtensionObserverAsh::~VpnExtensionObserverAsh() = default;

void VpnExtensionObserverAsh::AddObserver(
    VpnExtensionObserverAsh::Observer* observer) {
  observers_.AddObserver(observer);
}

void VpnExtensionObserverAsh::RemoveObserver(
    VpnExtensionObserverAsh::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void VpnExtensionObserverAsh::OnLacrosVpnExtensionLoaded(
    const std::string& extension_id,
    const std::string& extension_name) {
  for (auto& observer : observers_) {
    observer.OnLacrosVpnExtensionLoaded(extension_id, extension_name);
  }
}

void VpnExtensionObserverAsh::OnLacrosVpnExtensionUnloaded(
    const std::string& extension_id) {
  for (auto& observer : observers_) {
    observer.OnLacrosVpnExtensionUnloaded(extension_id);
  }
}

void VpnExtensionObserverAsh::BindReceiver(
    mojo::PendingReceiver<crosapi::mojom::VpnExtensionObserver> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void VpnExtensionObserverAsh::OnLacrosVpnExtensionObserverDisconnected() {
  for (auto& observer : observers_) {
    observer.OnLacrosVpnExtensionObserverDisconnected();
  }
}

}  // namespace crosapi
