// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_TELEMETRY_EXTENSION_UI_SERVICES_SYSTEM_EVENTS_SERVICE_H_
#define ASH_WEBUI_TELEMETRY_EXTENSION_UI_SERVICES_SYSTEM_EVENTS_SERVICE_H_

#include "ash/webui/telemetry_extension_ui/mojom/system_events_service.mojom.h"
#include "ash/webui/telemetry_extension_ui/services/bluetooth_observer.h"
#include "ash/webui/telemetry_extension_ui/services/lid_observer.h"
#include "ash/webui/telemetry_extension_ui/services/power_observer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace ash {

class SystemEventsService : public health::mojom::SystemEventsService {
 public:
  explicit SystemEventsService(
      mojo::PendingReceiver<health::mojom::SystemEventsService> receiver);
  SystemEventsService(const SystemEventsService&) = delete;
  SystemEventsService& operator=(const SystemEventsService&) = delete;
  ~SystemEventsService() override;

  void AddBluetoothObserver(
      mojo::PendingRemote<health::mojom::BluetoothObserver> observer) override;

  void AddLidObserver(
      mojo::PendingRemote<health::mojom::LidObserver> observer) override;

  void AddPowerObserver(
      mojo::PendingRemote<health::mojom::PowerObserver> observer) override;

  void FlushForTesting();

 private:
  mojo::Receiver<health::mojom::SystemEventsService> receiver_;

  BluetoothObserver bluetooth_observer_;
  LidObserver lid_observer_;
  PowerObserver power_observer_;
};

}  // namespace ash

// TODO(https://crbug.com/1164001): remove when ChromeOS code migration is done.
namespace chromeos {
using ::ash::SystemEventsService;
}  // namespace chromeos

#endif  // ASH_WEBUI_TELEMETRY_EXTENSION_UI_SERVICES_SYSTEM_EVENTS_SERVICE_H_
