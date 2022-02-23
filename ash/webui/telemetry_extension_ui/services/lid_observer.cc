// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/telemetry_extension_ui/services/lid_observer.h"

#include <utility>

#include "ash/webui/telemetry_extension_ui/mojom/system_events_service.mojom.h"
#include "base/bind.h"
#include "chromeos/services/cros_healthd/public/cpp/service_connection.h"

namespace ash {

LidObserver::LidObserver() : receiver_{this} {
  Connect();
}

LidObserver::~LidObserver() = default;

void LidObserver::AddObserver(
    mojo::PendingRemote<health::mojom::LidObserver> observer) {
  observers_.Add(std::move(observer));
}

void LidObserver::OnLidClosed() {
  for (auto& observer : observers_) {
    observer->OnLidClosed();
  }
}

void LidObserver::OnLidOpened() {
  for (auto& observer : observers_) {
    observer->OnLidOpened();
  }
}

void LidObserver::Connect() {
  receiver_.reset();
  cros_healthd::ServiceConnection::GetInstance()->AddLidObserver(
      receiver_.BindNewPipeAndPassRemote());

  // We try to reconnect right after disconnect because Mojo will queue the
  // request and connect to cros_healthd when it becomes available.
  receiver_.set_disconnect_handler(
      base::BindOnce(&LidObserver::Connect, base::Unretained(this)));
}

void LidObserver::FlushForTesting() {
  receiver_.FlushForTesting();  // IN-TEST
}

}  // namespace ash
