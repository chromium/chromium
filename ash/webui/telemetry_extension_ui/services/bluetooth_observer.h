// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_TELEMETRY_EXTENSION_UI_SERVICES_BLUETOOTH_OBSERVER_H_
#define ASH_WEBUI_TELEMETRY_EXTENSION_UI_SERVICES_BLUETOOTH_OBSERVER_H_

#include "ash/webui/telemetry_extension_ui/mojom/system_events_service.mojom-forward.h"
#include "chromeos/services/cros_healthd/public/mojom/cros_healthd_events.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace ash {

// TODO(https://crbug.com/1164001): Remove if cros_healthd::mojom moved to ash.
namespace cros_healthd {
namespace mojom = ::chromeos::cros_healthd::mojom;
}  // namespace cros_healthd

class BluetoothObserver
    : public cros_healthd::mojom::CrosHealthdBluetoothObserver {
 public:
  BluetoothObserver();
  BluetoothObserver(const BluetoothObserver&) = delete;
  BluetoothObserver& operator=(const BluetoothObserver&) = delete;
  ~BluetoothObserver() override;

  void AddObserver(
      mojo::PendingRemote<health::mojom::BluetoothObserver> observer);

  void OnAdapterAdded() override;
  void OnAdapterRemoved() override;
  void OnAdapterPropertyChanged() override;
  void OnDeviceAdded() override;
  void OnDeviceRemoved() override;
  void OnDevicePropertyChanged() override;

  // Waits until disconnect handler will be triggered if fake cros_healthd was
  // shutdown.
  void FlushForTesting();

 private:
  void Connect();

  mojo::Receiver<cros_healthd::mojom::CrosHealthdBluetoothObserver> receiver_;
  mojo::RemoteSet<health::mojom::BluetoothObserver> observers_;
};

}  // namespace ash

#endif  // ASH_WEBUI_TELEMETRY_EXTENSION_UI_SERVICES_BLUETOOTH_OBSERVER_H_
