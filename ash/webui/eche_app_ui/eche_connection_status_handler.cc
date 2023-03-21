// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/eche_app_ui/eche_connection_status_handler.h"

#include "ash/constants/ash_features.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"

namespace ash::eche_app {

EcheConnectionStatusHandler::EcheConnectionStatusHandler() = default;

EcheConnectionStatusHandler::~EcheConnectionStatusHandler() = default;

void EcheConnectionStatusHandler::OnConnectionStatusChanged(
    mojom::ConnectionStatus connection_status) {
  if (features::IsEcheNetworkConnectionStateEnabled()) {
    PA_LOG(INFO) << "echeapi EcheConnectionStatusHandler "
                 << " OnConnectionStatusChanged " << connection_status;
    NotifyConnectionStatusChanged(connection_status);
  }
}

void EcheConnectionStatusHandler::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void EcheConnectionStatusHandler::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void EcheConnectionStatusHandler::Bind(
    mojo::PendingReceiver<mojom::ConnectionStatusObserver> receiver) {
  connection_status_receiver_.reset();
  connection_status_receiver_.Bind(std::move(receiver));
}

void EcheConnectionStatusHandler::NotifyConnectionStatusChanged(
    mojom::ConnectionStatus connection_status) {
  for (auto& observer : observer_list_) {
    observer.OnConnectionStatusChanged(connection_status);
  }
}

}  // namespace ash::eche_app
