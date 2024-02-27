// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/network_change_ash.h"

#include "chrome/browser/ash/network_change_manager_client.h"

namespace crosapi {

NetworkChangeAsh::NetworkChangeAsh() = default;
NetworkChangeAsh::~NetworkChangeAsh() = default;

void NetworkChangeAsh::BindReceiver(
    mojo::PendingReceiver<mojom::NetworkChange> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void NetworkChangeAsh::AddObserver(
    mojo::PendingRemote<crosapi::mojom::NetworkChangeObserver> observer) {
  auto* client = ash::NetworkChangeManagerClient::GetInstance();
  // NetworkChangeManagerClient might be not set for testing.
  if (client)
    client->AddLacrosNetworkChangeObserver(std::move(observer));
}

}  // namespace crosapi
