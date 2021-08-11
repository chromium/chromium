// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_NETWORK_SETTINGS_SERVICE_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_NETWORK_SETTINGS_SERVICE_ASH_H_

#include "chromeos/crosapi/mojom/network_settings_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace crosapi {

// This class is the Ash-Chrome implementation of the NetworkSettingsService
// interface. This class must only be used from the main thread.
// It's used to propagate network changes to Lacros-Chrome.
class NetworkSettingsServiceAsh
    : public crosapi::mojom::NetworkSettingsService {
 public:
  NetworkSettingsServiceAsh();
  NetworkSettingsServiceAsh(const NetworkSettingsServiceAsh&) = delete;
  NetworkSettingsServiceAsh& operator=(const NetworkSettingsServiceAsh&) =
      delete;
  ~NetworkSettingsServiceAsh() override;

  void BindReceiver(
      mojo::PendingReceiver<crosapi::mojom::NetworkSettingsService>
          pending_receiver);
  // crosapi::mojom::NetworSettingsService:
  void AddNetworkSettingsObserver(
      mojo::PendingRemote<mojom::NetworkSettingsObserver> observer) override;

 private:
  // Support any number of connections.
  mojo::ReceiverSet<mojom::NetworkSettingsService> receivers_;
  // Support any number of observers.
  mojo::RemoteSet<mojom::NetworkSettingsObserver> observers_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_NETWORK_SETTINGS_SERVICE_ASH_H_
