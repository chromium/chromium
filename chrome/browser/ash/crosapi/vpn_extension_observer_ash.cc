// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/vpn_extension_observer_ash.h"
#include "base/functional/bind.h"

namespace crosapi {

VpnExtensionObserverAsh::VpnExtensionObserverAsh() {
  receivers_.set_disconnect_handler(base::BindRepeating(
      &VpnExtensionObserverAsh::OnLacrosVpnExtensionObserverDisconnected,
      base::Unretained(this)));
}

VpnExtensionObserverAsh::~VpnExtensionObserverAsh() = default;

void VpnExtensionObserverAsh::BindReceiver(
    mojo::PendingReceiver<crosapi::mojom::VpnExtensionObserver> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void VpnExtensionObserverAsh::SetDelegate(
    VpnExtensionObserverAsh::Delegate* delegate) {
  delegate_ = delegate;
}

void VpnExtensionObserverAsh::OnLacrosVpnExtensionLoaded(
    const std::string& extension_id,
    const std::string& extension_name) {
  if (delegate_) {
    delegate_->OnLacrosVpnExtensionLoaded(extension_id, extension_name);
  }
}

void VpnExtensionObserverAsh::OnLacrosVpnExtensionUnloaded(
    const std::string& extension_id) {
  if (delegate_) {
    delegate_->OnLacrosVpnExtensionUnloaded(extension_id);
  }
}

void VpnExtensionObserverAsh::OnLacrosVpnExtensionObserverDisconnected() {
  if (delegate_) {
    delegate_->OnLacrosVpnExtensionObserverDisconnected();
  }
}

}  // namespace crosapi
