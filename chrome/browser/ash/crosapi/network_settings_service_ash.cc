// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/network_settings_service_ash.h"

namespace crosapi {

NetworkSettingsServiceAsh::NetworkSettingsServiceAsh() = default;

NetworkSettingsServiceAsh::~NetworkSettingsServiceAsh() = default;

void NetworkSettingsServiceAsh::BindReceiver(
    mojo::PendingReceiver<mojom::NetworkSettingsService> pending_receiver) {
  receivers_.Add(this, std::move(pending_receiver));
}

void NetworkSettingsServiceAsh::AddNetworkSettingsObserver(
    mojo::PendingRemote<mojom::NetworkSettingsObserver> observer) {
  mojo::Remote<mojom::NetworkSettingsObserver> remote(std::move(observer));
  observers_.Add(std::move(remote));
  // TODO(acostinas, b/192914690): Fire the observer with the initial value.
}

}  // namespace crosapi
