// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/eche_app_ui/eche_connection_status_observer.h"

#include "ash/constants/ash_features.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"

namespace ash::eche_app {

EcheConnectionStatusObserver::EcheConnectionStatusObserver() = default;

EcheConnectionStatusObserver::~EcheConnectionStatusObserver() = default;

void EcheConnectionStatusObserver::OnConnectionStatusChanged(
    mojom::ConnectionStatus connectionStatus) {
  if (features::IsEcheNetworkConnectionStateEnabled()) {
    PA_LOG(INFO) << "echeapi EcheConnectionStatusObserver "
                 << " OnConnectionStatusChanged " << connectionStatus;
    NotifyConnectionStatusChanged(connectionStatus);
  }
}

void EcheConnectionStatusObserver::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void EcheConnectionStatusObserver::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void EcheConnectionStatusObserver::Bind(
    mojo::PendingReceiver<mojom::ConnectionStatusObserver> receiver) {
  connection_status_receiver_.reset();
  connection_status_receiver_.Bind(std::move(receiver));
}

void EcheConnectionStatusObserver::NotifyConnectionStatusChanged(
    mojom::ConnectionStatus connectionStatus) {
  for (auto& observer : observer_list_) {
    observer.OnConnectionStatusChanged(connectionStatus);
  }
}

}  // namespace ash::eche_app
