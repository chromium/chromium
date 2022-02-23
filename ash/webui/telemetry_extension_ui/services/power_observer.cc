// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/telemetry_extension_ui/services/power_observer.h"

#include <utility>

#include "ash/webui/telemetry_extension_ui/mojom/system_events_service.mojom.h"
#include "base/bind.h"
#include "chromeos/services/cros_healthd/public/cpp/service_connection.h"

namespace ash {

PowerObserver::PowerObserver() : receiver_{this} {
  Connect();
}

PowerObserver::~PowerObserver() = default;

void PowerObserver::AddObserver(
    mojo::PendingRemote<health::mojom::PowerObserver> observer) {
  observers_.Add(std::move(observer));
}

void PowerObserver::OnAcInserted() {
  for (auto& observer : observers_) {
    observer->OnAcInserted();
  }
}

void PowerObserver::OnAcRemoved() {
  for (auto& observer : observers_) {
    observer->OnAcRemoved();
  }
}

void PowerObserver::OnOsSuspend() {
  for (auto& observer : observers_) {
    observer->OnOsSuspend();
  }
}

void PowerObserver::OnOsResume() {
  for (auto& observer : observers_) {
    observer->OnOsResume();
  }
}

void PowerObserver::Connect() {
  receiver_.reset();
  cros_healthd::ServiceConnection::GetInstance()->AddPowerObserver(
      receiver_.BindNewPipeAndPassRemote());

  // We try to reconnect right after disconnect because Mojo will queue the
  // request and connect to cros_healthd when it becomes available.
  receiver_.set_disconnect_handler(
      base::BindOnce(&PowerObserver::Connect, base::Unretained(this)));
}

void PowerObserver::FlushForTesting() {
  receiver_.FlushForTesting();  // IN-TEST
}

}  // namespace ash
